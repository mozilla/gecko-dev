/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentHelper.h"
#include "LayerManagerD3D10.h"
#include "MetroWidget.h"
#include "MetroApp.h"
#include "mozilla/Preferences.h"
#include "nsToolkit.h"
#include "KeyboardLayout.h"
#include "MetroUtils.h"
#include "WinUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsIAppStartup.h"
#include "../resource.h"
#include "nsIWidgetListener.h"
#include "nsIPresShell.h"
#include "nsPrintfCString.h"
#include "nsWindowDefs.h"
#include "FrameworkView.h"
#include "nsTextStore.h"
#include "Layers.h"
#include "ClientLayerManager.h"
#include "BasicLayers.h"
#include "FrameMetrics.h"
#include <windows.devices.input.h>
#include "Windows.Graphics.Display.h"
#include "DisplayInfo_sdk81.h"
#include "nsNativeDragTarget.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "UIABridgePrivate.h"
#include "WinMouseScrollHandler.h"
#include "InputData.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/MiscEvents.h"
#include "gfxPrefs.h"

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

using namespace mozilla;
using namespace mozilla::widget;
using namespace mozilla::layers;
using namespace mozilla::widget::winrt;

using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::ApplicationModel::Activation;
using namespace ABI::Windows::UI::Input;
using namespace ABI::Windows::Devices::Input;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::System;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Graphics::Display;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gWindowsLog;
#endif

#if !defined(SM_CONVERTIBLESLATEMODE)
#define SM_CONVERTIBLESLATEMODE 0x2003
#endif

static uint32_t gInstanceCount = 0;
const char16_t* kMetroSubclassThisProp = L"MetroSubclassThisProp";
HWND MetroWidget::sICoreHwnd = nullptr;

namespace mozilla {
namespace widget {
UINT sDefaultBrowserMsgId = RegisterWindowMessageW(L"DefaultBrowserClosing");
} }

// WM_GETOBJECT id pulled from uia headers
#define MOZOBJID_UIAROOT -25

namespace mozilla {
namespace widget {
namespace winrt {
extern ComPtr<MetroApp> sMetroApp;
extern ComPtr<IUIABridge> gProviderRoot;
} } }

namespace {

  void SendInputs(uint32_t aModifiers, INPUT* aExtraInputs, uint32_t aExtraInputsLen)
  {
    // keySequence holds the virtual key values of each of the keys we intend
    // to press
    nsAutoTArray<KeyPair,32> keySequence;
    for (uint32_t i = 0; i < ArrayLength(sModifierKeyMap); ++i) {
      const uint32_t* map = sModifierKeyMap[i];
      if (aModifiers & map[0]) {
        keySequence.AppendElement(KeyPair(map[1], map[2]));
      }
    }

    uint32_t const len = keySequence.Length() * 2 + aExtraInputsLen;

    // The `inputs` array is a sequence of input events that will happen
    // serially.  We set the array up so that each modifier key is pressed
    // down, then the additional input events happen,
    // then each modifier key is released in reverse order of when
    // it was pressed down.  We pass this array to `SendInput`.
    //
    // inputs[0]: modifier key (e.g. shift, ctrl, etc) down
    // ...        ...
    // inputs[keySequence.Length()-1]: modifier key (e.g. shift, ctrl, etc) down
    // inputs[keySequence.Length()]: aExtraInputs[0]
    // inputs[keySequence.Length()+1]: aExtraInputs[1]
    // ...        ...
    // inputs[keySequence.Length() + aExtraInputsLen - 1]: aExtraInputs[aExtraInputsLen - 1]
    // inputs[keySequence.Length() + aExtraInputsLen]: modifier key (e.g. shift, ctrl, etc) up
    // ...        ...
    // inputs[len-1]: modifier key (e.g. shift, ctrl, etc) up
    INPUT* inputs = new INPUT[len];
    memset(inputs, 0, len * sizeof(INPUT));
    for (uint32_t i = 0; i < keySequence.Length(); ++i) {
      inputs[i].type = inputs[len-i-1].type = INPUT_KEYBOARD;
      inputs[i].ki.wVk = inputs[len-i-1].ki.wVk = keySequence[i].mSpecific
                                                ? keySequence[i].mSpecific
                                                : keySequence[i].mGeneral;
      inputs[len-i-1].ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    for (uint32_t i = 0; i < aExtraInputsLen; i++) {
      inputs[keySequence.Length()+i] = aExtraInputs[i];
    }
    WinUtils::Log("  Sending inputs");
    for (uint32_t i = 0; i < len; i++) {
      if (inputs[i].type == INPUT_KEYBOARD) {
        WinUtils::Log("    Key press: 0x%x %s",
            inputs[i].ki.wVk,
            inputs[i].ki.dwFlags & KEYEVENTF_KEYUP
            ? "UP"
            : "DOWN");
      } else if(inputs[i].type == INPUT_MOUSE) {
        WinUtils::Log("    Mouse input: 0x%x 0x%x",
            inputs[i].mi.dwFlags,
            inputs[i].mi.mouseData);
      } else {
        WinUtils::Log("    Unknown input type!");
      }
    }
    ::SendInput(len, inputs, sizeof(INPUT));
    delete[] inputs;

    // The inputs have been sent, and the WM_* messages they generate are
    // waiting to be processed by our event loop.  Now we manually pump
    // those messages so that, upon our return, all the inputs have been
    // processed.
    WinUtils::Log("  Inputs sent. Waiting for input messages to clear");
    MSG msg;
    while (WinUtils::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (nsTextStore::ProcessRawKeyMessage(msg)) {
        continue;  // the message is consumed by TSF
      }
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      WinUtils::Log("    Dispatched 0x%x 0x%x 0x%x", msg.message, msg.wParam, msg.lParam);
    }
    WinUtils::Log("  No more input messages");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(MetroWidget, nsBaseWidget)

MetroWidget::MetroWidget() :
  mTransparencyMode(eTransparencyOpaque),
  mWnd(nullptr),
  mMetroWndProc(nullptr),
  mTempBasicLayerInUse(false),
  mRootLayerTreeId(),
  nsWindowBase()
{
  // Global initialization
  if (!gInstanceCount) {
    UserActivity();
    nsTextStore::Initialize();
    MouseScrollHandler::Initialize();
    KeyboardLayout::GetInstance()->OnLayoutChange(::GetKeyboardLayout(0));
  } // !gInstanceCount
  gInstanceCount++;
}

MetroWidget::~MetroWidget()
{
  LogThis();

  gInstanceCount--;

  // Global shutdown
  if (!gInstanceCount) {
    APZController::sAPZC = nullptr;
    nsTextStore::Terminate();
  } // !gInstanceCount
}

static bool gTopLevelAssigned = false;
NS_IMETHODIMP
MetroWidget::Create(nsIWidget *aParent,
                    nsNativeWidget aNativeParent,
                    const nsIntRect &aRect,
                    nsDeviceContext *aContext,
                    nsWidgetInitData *aInitData)
{
  LogFunction();

  nsWidgetInitData defaultInitData;
  if (!aInitData)
    aInitData = &defaultInitData;

  mWindowType = aInitData->mWindowType;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  BaseCreate(aParent, aRect, aContext, aInitData);

  if (mWindowType != eWindowType_toplevel) {
    switch(mWindowType) {
      case eWindowType_dialog:
      WinUtils::Log("eWindowType_dialog window requested, returning failure.");
      break;
      case eWindowType_child:
      WinUtils::Log("eWindowType_child window requested, returning failure.");
      break;
      case eWindowType_popup:
      WinUtils::Log("eWindowType_popup window requested, returning failure.");
      break;
      case eWindowType_plugin:
      WinUtils::Log("eWindowType_plugin window requested, returning failure.");
      break;
      // we should support toolkit's eWindowType_invisible at some point.
      case eWindowType_invisible:
      WinUtils::Log("eWindowType_invisible window requested, this doesn't actually exist!");
      return NS_OK;
    }
    NS_WARNING("Invalid window type requested.");
    return NS_ERROR_FAILURE;
  }

  if (gTopLevelAssigned) {
    // Need to accept so that the mochitest-chrome test harness window
    // can be created.
    NS_WARNING("New eWindowType_toplevel window requested after FrameworkView widget created.");
    NS_WARNING("Widget created but the physical window does not exist! Fix me!");
    return NS_OK;
  }

  // the main widget gets created first
  gTopLevelAssigned = true;
  sMetroApp->SetWidget(this);
  WinUtils::SetNSWindowBasePtr(mWnd, this);

  if (mWidgetListener) {
    mWidgetListener->WindowActivated();
  }

  return NS_OK;
}

void
MetroWidget::SetView(FrameworkView* aView)
{
  mView = aView;
  // If we've already set this up, it points to a useless
  // layer manager, so reset it.
  mLayerManager = nullptr;
}

NS_IMETHODIMP
MetroWidget::Destroy()
{
  if (mOnDestroyCalled)
    return NS_OK;
  WinUtils::Log("[%X] %s mWnd=%X type=%d", this, __FUNCTION__, mWnd, mWindowType);
  mOnDestroyCalled = true;

  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  if (ShouldUseAPZC()) {
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      observerService->RemoveObserver(this, "apzc-scroll-offset-changed");
      observerService->RemoveObserver(this, "apzc-zoom-to-rect");
      observerService->RemoveObserver(this, "apzc-disable-zoom");
    }
  }

  RemoveSubclass();
  NotifyWindowDestroyed();

  // Prevent the widget from sending additional events.
  mWidgetListener = nullptr;
  mAttachedWidgetListener = nullptr;

  // Release references to children, device context, toolkit, and app shell.
  nsBaseWidget::Destroy();
  nsBaseWidget::OnDestroy();
  WinUtils::SetNSWindowBasePtr(mWnd, nullptr);

  if (mLayerManager) {
    mLayerManager->Destroy();
  }

  mLayerManager = nullptr;
  mView = nullptr;
  mIdleService = nullptr;
  mWnd = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::SetParent(nsIWidget *aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::Show(bool bState)
{
  return NS_OK;
}

uint32_t
MetroWidget::GetMaxTouchPoints() const
{
  ComPtr<IPointerDeviceStatics> deviceStatics;

  HRESULT hr = GetActivationFactory(
    HStringReference(RuntimeClass_Windows_Devices_Input_PointerDevice).Get(),
    deviceStatics.GetAddressOf());

  if (FAILED(hr)) {
    return 0;
  }

  ComPtr< IVectorView<PointerDevice*> > deviceList;
  hr = deviceStatics->GetPointerDevices(&deviceList);

  if (FAILED(hr)) {
    return 0;
  }

  uint32_t deviceNum = 0;
  deviceList->get_Size(&deviceNum);

  uint32_t maxTouchPoints = 0;
  for (uint32_t index = 0; index < deviceNum; ++index) {
    ComPtr<IPointerDevice> device;
    PointerDeviceType deviceType;

    if (FAILED(deviceList->GetAt(index, device.GetAddressOf()))) {
      continue;
    }

    if (FAILED(device->get_PointerDeviceType(&deviceType)))  {
      continue;
    }

    if (deviceType == PointerDeviceType_Touch) {
      uint32_t deviceMaxTouchPoints = 0;
      device->get_MaxContacts(&deviceMaxTouchPoints);
      maxTouchPoints = std::max(maxTouchPoints, deviceMaxTouchPoints);
    }
  }

  return maxTouchPoints;
}

NS_IMETHODIMP
MetroWidget::IsVisible(bool & aState)
{
  aState = mView->IsVisible();
  return NS_OK;
}

bool
MetroWidget::IsVisible() const
{
  if (!mView)
    return false;
  return mView->IsVisible();
}

NS_IMETHODIMP
MetroWidget::EnableDragDrop(bool aEnable) {
  if (aEnable) {
    if (nullptr == mNativeDragTarget) {
      mNativeDragTarget = new nsNativeDragTarget(this);
      if (!mNativeDragTarget) {
        return NS_ERROR_FAILURE;
      }
    }

    HRESULT hr = ::RegisterDragDrop(mWnd, static_cast<LPDROPTARGET>(mNativeDragTarget));
    return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
  } else {
    if (nullptr == mNativeDragTarget) {
      return NS_OK;
    }

    HRESULT hr = ::RevokeDragDrop(mWnd);
    return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
MetroWidget::IsEnabled(bool *aState)
{
  *aState = mView->IsEnabled();
  return NS_OK;
}

bool
MetroWidget::IsEnabled() const
{
  if (!mView)
    return false;
  return mView->IsEnabled();
}

NS_IMETHODIMP
MetroWidget::Enable(bool bState)
{
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::GetBounds(nsIntRect &aRect)
{
  if (mView) {
    mView->GetBounds(aRect);
  } else {
    nsIntRect rect(0,0,0,0);
    aRect = rect;
  }
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::GetScreenBounds(nsIntRect &aRect)
{
  if (mView) {
    mView->GetBounds(aRect);
  } else {
    nsIntRect rect(0,0,0,0);
    aRect = rect;
  }
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::GetClientBounds(nsIntRect &aRect)
{
  if (mView) {
    mView->GetBounds(aRect);
  } else {
    nsIntRect rect(0,0,0,0);
    aRect = rect;
  }
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::SetCursor(nsCursor aCursor)
{
  if (!mView)
    return NS_ERROR_FAILURE;

  switch (aCursor) {
    case eCursor_select:
      mView->SetCursor(CoreCursorType::CoreCursorType_IBeam);
      break;
    case eCursor_wait:
      mView->SetCursor(CoreCursorType::CoreCursorType_Wait);
      break;
    case eCursor_hyperlink:
      mView->SetCursor(CoreCursorType::CoreCursorType_Hand);
      break;
    case eCursor_standard:
      mView->SetCursor(CoreCursorType::CoreCursorType_Arrow);
      break;
    case eCursor_n_resize:
    case eCursor_s_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeNorthSouth);
      break;
    case eCursor_w_resize:
    case eCursor_e_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeWestEast);
      break;
    case eCursor_nw_resize:
    case eCursor_se_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeNorthwestSoutheast);
      break;
    case eCursor_ne_resize:
    case eCursor_sw_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeNortheastSouthwest);
      break;
    case eCursor_crosshair:
      mView->SetCursor(CoreCursorType::CoreCursorType_Cross);
      break;
    case eCursor_move:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeAll);
      break;
    case eCursor_help:
      mView->SetCursor(CoreCursorType::CoreCursorType_Help);
      break;
    // CSS3 custom cursors
    case eCursor_copy:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_COPY);
      break;
    case eCursor_alias:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_ALIAS);
      break;
    case eCursor_cell:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_CELL);
      break;
    case eCursor_grab:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_GRAB);
      break;
    case eCursor_grabbing:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_GRABBING);
      break;
    case eCursor_spinning:
      mView->SetCursor(CoreCursorType::CoreCursorType_Wait);
      break;
    case eCursor_context_menu:
      mView->SetCursor(CoreCursorType::CoreCursorType_Arrow);
      break;
    case eCursor_zoom_in:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_ZOOMIN);
      break;
    case eCursor_zoom_out:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_ZOOMOUT);
      break;
    case eCursor_not_allowed:
    case eCursor_no_drop:
      mView->SetCursor(CoreCursorType::CoreCursorType_UniversalNo);
      break;
    case eCursor_col_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_COLRESIZE);
      break;
    case eCursor_row_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_ROWRESIZE);
      break;
    case eCursor_vertical_text:
      mView->SetCursor(CoreCursorType::CoreCursorType_Custom, IDC_VERTICALTEXT);
      break;
    case eCursor_all_scroll:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeAll);
      break;
    case eCursor_nesw_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeNortheastSouthwest);
      break;
    case eCursor_nwse_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeNorthwestSoutheast);
      break;
    case eCursor_ns_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeNorthSouth);
      break;
    case eCursor_ew_resize:
      mView->SetCursor(CoreCursorType::CoreCursorType_SizeWestEast);
      break;
    case eCursor_none:
      mView->ClearCursor();
      break;
    default:
      NS_WARNING("Invalid cursor type");
      break;
  }
  return NS_OK;
}

nsresult
MetroWidget::SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                      int32_t aNativeKeyCode,
                                      uint32_t aModifierFlags,
                                      const nsAString& aCharacters,
                                      const nsAString& aUnmodifiedCharacters)
{
  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
  return keyboardLayout->SynthesizeNativeKeyEvent(
           this, aNativeKeyboardLayout, aNativeKeyCode, aModifierFlags,
           aCharacters, aUnmodifiedCharacters);
}

nsresult
MetroWidget::SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                        uint32_t aNativeMessage,
                                        uint32_t aModifierFlags)
{
  WinUtils::Log("ENTERED SynthesizeNativeMouseEvent");

  INPUT inputs[2];
  memset(inputs, 0, 2*sizeof(INPUT));
  inputs[0].type = inputs[1].type = INPUT_MOUSE;
  inputs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
  // Inexplicably, the x and y coordinates that we want to move the mouse to
  // are specified as values in the range (0, 65535). (0,0) represents the
  // top left of the primary monitor and (65535, 65535) represents the
  // bottom right of the primary monitor.
  inputs[0].mi.dx = (aPoint.x * 65535) / ::GetSystemMetrics(SM_CXSCREEN);
  inputs[0].mi.dy = (aPoint.y * 65535) / ::GetSystemMetrics(SM_CYSCREEN);
  inputs[1].mi.dwFlags = aNativeMessage;
  SendInputs(aModifierFlags, inputs, 2);

  WinUtils::Log("Exiting SynthesizeNativeMouseEvent");
  return NS_OK;
}

nsresult
MetroWidget::SynthesizeNativeMouseScrollEvent(nsIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              double aDeltaX,
                                              double aDeltaY,
                                              double aDeltaZ,
                                              uint32_t aModifierFlags,
                                              uint32_t aAdditionalFlags)
{
  return MouseScrollHandler::SynthesizeNativeMouseScrollEvent(
           this, aPoint, aNativeMessage,
           (aNativeMessage == WM_MOUSEWHEEL || aNativeMessage == WM_VSCROLL) ?
             static_cast<int32_t>(aDeltaY) : static_cast<int32_t>(aDeltaX),
           aModifierFlags, aAdditionalFlags);
}

static void
CloseGesture()
{
  LogFunction();
  nsCOMPtr<nsIAppStartup> appStartup =
    do_GetService(NS_APPSTARTUP_CONTRACTID);
  if (appStartup) {
    appStartup->Quit(nsIAppStartup::eForceQuit);
  }
}

// Async event sending for mouse and keyboard input.

// defined in nsWindowBase, called from shared module WinMouseScrollHandler.
bool
MetroWidget::DispatchScrollEvent(mozilla::WidgetGUIEvent* aEvent)
{
  WidgetGUIEvent* newEvent = nullptr;
  switch(aEvent->mClass) {
    case eWheelEventClass:
    {
      WidgetWheelEvent* oldEvent = aEvent->AsWheelEvent();
      WidgetWheelEvent* wheelEvent =
        new WidgetWheelEvent(oldEvent->mFlags.mIsTrusted, oldEvent->message, oldEvent->widget);
      wheelEvent->AssignWheelEventData(*oldEvent, true);
      newEvent = static_cast<WidgetGUIEvent*>(wheelEvent);
    }
    break;
    case eContentCommandEventClass:
    {
      WidgetContentCommandEvent* oldEvent = aEvent->AsContentCommandEvent();
      WidgetContentCommandEvent* cmdEvent =
        new WidgetContentCommandEvent(oldEvent->mFlags.mIsTrusted, oldEvent->message, oldEvent->widget);
      cmdEvent->AssignContentCommandEventData(*oldEvent, true);
      newEvent = static_cast<WidgetGUIEvent*>(cmdEvent);
    }
    break;
    default:
      MOZ_CRASH("unknown event in DispatchScrollEvent");
    break;
  }
  mEventQueue.Push(newEvent);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &MetroWidget::DeliverNextScrollEvent);
  NS_DispatchToCurrentThread(runnable);
  return false;
}

void
MetroWidget::DeliverNextScrollEvent()
{
  WidgetGUIEvent* event =
    static_cast<WidgetInputEvent*>(mEventQueue.PopFront());
  DispatchWindowEvent(event);
  delete event;
}

// defined in nsWindowBase, called from shared module KeyboardLayout.
bool
MetroWidget::DispatchKeyboardEvent(WidgetGUIEvent* aEvent)
{
  MOZ_ASSERT(aEvent);
  WidgetKeyboardEvent* oldKeyEvent = aEvent->AsKeyboardEvent();
  WidgetKeyboardEvent* keyEvent =
    new WidgetKeyboardEvent(oldKeyEvent->mFlags.mIsTrusted,
                            oldKeyEvent->message, oldKeyEvent->widget);
  // XXX note this leaves pluginEvent null, which is fine for now.
  keyEvent->AssignKeyEventData(*oldKeyEvent, true);
  mKeyEventQueue.Push(keyEvent);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &MetroWidget::DeliverNextKeyboardEvent);
  NS_DispatchToCurrentThread(runnable);
  return false;
}

// Used in conjunction with mKeyEventQueue to find a keypress event
// that should not be delivered due to the return result of the
// preceeding keydown.
class KeyQueryIdAndCancel : public nsDequeFunctor {
public:
  KeyQueryIdAndCancel(uint32_t aIdToCancel) :
    mId(aIdToCancel) {
  }
  virtual void* operator() (void* aObject) {
    WidgetKeyboardEvent* event = static_cast<WidgetKeyboardEvent*>(aObject);
    if (event->mUniqueId == mId) {
      event->mFlags.mPropagationStopped = true;
    }
    return nullptr;
  }
protected:
  uint32_t mId;
};

void
MetroWidget::DeliverNextKeyboardEvent()
{
  WidgetKeyboardEvent* event =
    static_cast<WidgetKeyboardEvent*>(mKeyEventQueue.PopFront());
  if (event->mFlags.mPropagationStopped) {
    // This can happen if a keypress was previously cancelled.
    delete event;
    return;
  }

  if (DispatchWindowEvent(event) && event->message == NS_KEY_DOWN) {
    // keydown events may be followed by multiple keypress events which
    // shouldn't be sent if preventDefault is called on keydown.
    KeyQueryIdAndCancel query(event->mUniqueId);
    mKeyEventQueue.ForEach(query);
  }
  delete event;
}

// static
LRESULT CALLBACK
MetroWidget::StaticWindowProcedure(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam)
{
  MetroWidget* self = reinterpret_cast<MetroWidget*>(
    GetProp(aWnd, kMetroSubclassThisProp));
  if (!self) {
    NS_NOTREACHED("Missing 'this' prop on subclassed metro window, this is bad.");
    return 0;
  }
  return self->WindowProcedure(aWnd, aMsg, aWParam, aLParam);
}

LRESULT
MetroWidget::WindowProcedure(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam)
{
  if(sDefaultBrowserMsgId == aMsg) {
    CloseGesture();
  } else if (WM_SETTINGCHANGE == aMsg) {
    if (aLParam && !wcsicmp(L"ConvertibleSlateMode", (wchar_t*)aLParam)) {
      // If we're switching away from slate mode, switch to Desktop for
      // hardware that supports this feature if the pref is set.
      if (GetSystemMetrics(SM_CONVERTIBLESLATEMODE) != 0 &&
          Preferences::GetBool("browser.shell.metro-auto-switch-enabled",
                               false)) {
        nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
        if (appStartup) {
          appStartup->Quit(nsIAppStartup::eForceQuit | nsIAppStartup::eRestart);
        }
      }
    }
  }

  // Indicates if we should hand messages to the default windows
  // procedure for processing.
  bool processDefault = true;

  // The result returned if we do not do default processing.
  LRESULT processResult = 0;

  MSGResult msgResult(&processResult);
  MouseScrollHandler::ProcessMessage(this, aMsg, aWParam, aLParam, msgResult);
  if (msgResult.mConsumed) {
    return processResult;
  }

  nsTextStore::ProcessMessage(this, aMsg, aWParam, aLParam, msgResult);
  if (msgResult.mConsumed) {
    return processResult;
  }

  switch (aMsg) {
    case WM_POWERBROADCAST:
    {
      switch (aWParam)
      {
        case PBT_APMSUSPEND:
          MetroApp::PostSleepWakeNotification(true);
          break;
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMECRITICAL:
        case PBT_APMRESUMESUSPEND:
          MetroApp::PostSleepWakeNotification(false);
          break;
      }
      break;
    }

    // Keyboard handling is passed to KeyboardLayout, which delivers gecko events
    // via DispatchKeyboardEvent.

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
      MSG msg = WinUtils::InitMSG(aMsg, aWParam, aLParam, aWnd);
      // If this method doesn't call NativeKey::HandleKeyDownMessage(), this
      // method must clean up the redirected message information itself.  For
      // more information, see above comment of
      // RedirectedKeyDownMessageManager::AutoFlusher class definition in
      // KeyboardLayout.h.
      RedirectedKeyDownMessageManager::AutoFlusher
        redirectedMsgFlusher(this, msg);

      if (nsTextStore::IsComposingOn(this)) {
        break;
      }

      ModifierKeyState modKeyState;
      NativeKey nativeKey(this, msg, modKeyState);
      processDefault = !nativeKey.HandleKeyDownMessage();
      // HandleKeyDownMessage cleaned up the redirected message information
      // itself, so, we should do nothing.
      redirectedMsgFlusher.Cancel();
      break;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
      if (nsTextStore::IsComposingOn(this)) {
        break;
      }

      MSG msg = WinUtils::InitMSG(aMsg, aWParam, aLParam, aWnd);
      ModifierKeyState modKeyState;
      NativeKey nativeKey(this, msg, modKeyState);
      processDefault = !nativeKey.HandleKeyUpMessage();
      break;
    }

    case WM_CHAR:
    case WM_SYSCHAR:
    {
      if (nsTextStore::IsComposingOn(this)) {
        nsTextStore::CommitComposition(false);
      }

      MSG msg = WinUtils::InitMSG(aMsg, aWParam, aLParam, aWnd);
      ModifierKeyState modKeyState;
      NativeKey nativeKey(this, msg, modKeyState);
      processDefault = !nativeKey.HandleCharMessage(msg);
      break;
    }

    case WM_INPUTLANGCHANGE:
    {
      KeyboardLayout::GetInstance()->
        OnLayoutChange(reinterpret_cast<HKL>(aLParam));
      processResult = 1;
      break;
    }

    case WM_APPCOMMAND:
      processDefault = HandleAppCommandMsg(aWParam, aLParam, &processResult);
      break;

    case WM_GETOBJECT:
    {
      DWORD dwObjId = (LPARAM)(DWORD) aLParam;
      // Passing this to CallWindowProc can result in a failure due to a timing issue
      // in winrt core window server code, so we call it directly here. Also, it's not
      // clear Windows::UI::Core::WindowServer::OnAutomationProviderRequestedEvent is
      // compatible with metro enabled desktop browsers, it makes an initial call to
      // UiaReturnRawElementProvider passing the return result from FrameworkView
      // OnAutomationProviderRequested as the hwnd (me scratches head) which results in
      // GetLastError always being set to invalid handle (6) after CallWindowProc returns.
      if (dwObjId == MOZOBJID_UIAROOT && gProviderRoot) {
        ComPtr<IRawElementProviderSimple> simple;
        gProviderRoot.As(&simple);
        if (simple) {
          LRESULT res = UiaReturnRawElementProvider(aWnd, aWParam, aLParam, simple.Get());
          if (res) {
            return res;
          }
          NS_ASSERTION(res, "UiaReturnRawElementProvider failed!");
          WinUtils::Log("UiaReturnRawElementProvider failed! GetLastError=%X", GetLastError());
        }
      }
      break;
    }

    default:
    {
      break;
    }
  }

  if (processDefault) {
    return CallWindowProc(mMetroWndProc, aWnd, aMsg, aWParam,
                          aLParam);
  }
  return processResult;
}

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
  WCHAR className[56];
  if (GetClassNameW(hwnd, className, sizeof(className)/sizeof(WCHAR)) &&
      !wcscmp(L"Windows.UI.Core.CoreWindow", className)) {
    DWORD processID = 0;
    GetWindowThreadProcessId(hwnd, &processID);
    if (processID && processID == GetCurrentProcessId()) {
      *((HWND*)lParam) = hwnd;
      return FALSE;
    }
  }
  return TRUE;
}

void
MetroWidget::FindMetroWindow()
{
  LogFunction();
  if (mWnd)
    return;
  EnumWindows(EnumWindowsProc, (LPARAM)&mWnd);
  NS_ASSERTION(mWnd, "Couldn't find our metro CoreWindow, this is bad.");

  // subclass it
  SetSubclass();
  sICoreHwnd = mWnd;
  return;
}

void
MetroWidget::SetSubclass()
{
  if (!mWnd) {
    NS_NOTREACHED("SetSubclass called without a valid hwnd.");
    return;
  }

  WNDPROC wndProc = reinterpret_cast<WNDPROC>(
    GetWindowLongPtr(mWnd, GWLP_WNDPROC));
  if (wndProc != StaticWindowProcedure) {
      if (!SetPropW(mWnd, kMetroSubclassThisProp, this)) {
        NS_NOTREACHED("SetProp failed, can't continue.");
        return;
      }
      mMetroWndProc =
        reinterpret_cast<WNDPROC>(
          SetWindowLongPtr(mWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(StaticWindowProcedure)));
      NS_ASSERTION(mMetroWndProc != StaticWindowProcedure, "WTF?");
  }
}

void
MetroWidget::RemoveSubclass()
{
  if (!mWnd)
    return;
  WNDPROC wndProc = reinterpret_cast<WNDPROC>(
    GetWindowLongPtr(mWnd, GWLP_WNDPROC));
  if (wndProc == StaticWindowProcedure) {
      NS_ASSERTION(mMetroWndProc, "Should have old proc here.");
      SetWindowLongPtr(mWnd, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(mMetroWndProc));
      mMetroWndProc = nullptr;
  }
  RemovePropW(mWnd, kMetroSubclassThisProp);
}

bool
MetroWidget::ShouldUseOffMainThreadCompositing()
{
  // Either we're not initialized yet, or this is the toolkit widget
  if (!mView) {
    return false;
  }
  // toolkit or test widgets can't use omtc, they don't have ICoreWindow.
  return gfxPlatform::UsesOffMainThreadCompositing() &&
         mWindowType == eWindowType_toplevel;
}

bool
MetroWidget::ShouldUseMainThreadD3D10Manager()
{
  // Either we're not initialized yet, or this is the toolkit widget
  if (!mView) {
    return false;
  }
  return !gfxPlatform::UsesOffMainThreadCompositing() &&
         mWindowType == eWindowType_toplevel;
}

bool
MetroWidget::ShouldUseBasicManager()
{
  // toolkit or test widgets fall back on empty shadow layers
  return (mWindowType != eWindowType_toplevel);
}

bool
MetroWidget::ShouldUseAPZC()
{
  return gfxPrefs::AsyncPanZoomEnabled();
}

void
MetroWidget::SetWidgetListener(nsIWidgetListener* aWidgetListener)
{
  mWidgetListener = aWidgetListener;
}

CompositorParent* MetroWidget::NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight)
{
  CompositorParent *compositor = nsBaseWidget::NewCompositorParent(aSurfaceWidth, aSurfaceHeight);

  if (ShouldUseAPZC()) {
    mRootLayerTreeId = compositor->RootLayerTreeId();

    mController = new APZController();

    CompositorParent::SetControllerForLayerTree(mRootLayerTreeId, mController);

    APZController::sAPZC = CompositorParent::GetAPZCTreeManager(compositor->RootLayerTreeId());
    APZController::sAPZC->SetDPI(GetDPI());

    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      observerService->AddObserver(this, "apzc-scroll-offset-changed", false);
      observerService->AddObserver(this, "apzc-zoom-to-rect", false);
      observerService->AddObserver(this, "apzc-disable-zoom", false);
    }
  }

  return compositor;
}

MetroWidget::TouchBehaviorFlags
MetroWidget::ContentGetAllowedTouchBehavior(const nsIntPoint& aPoint)
{
  return ContentHelper::GetAllowedTouchBehavior(this, aPoint);
}

void
MetroWidget::ApzcGetAllowedTouchBehavior(WidgetInputEvent* aTransformedEvent,
                                         nsTArray<TouchBehaviorFlags>& aOutBehaviors)
{
  LogFunction();
  return APZController::sAPZC->GetAllowedTouchBehavior(aTransformedEvent, aOutBehaviors);
}

void
MetroWidget::ApzcSetAllowedTouchBehavior(const ScrollableLayerGuid& aGuid,
                                         nsTArray<TouchBehaviorFlags>& aBehaviors)
{
  LogFunction();
  if (!APZController::sAPZC) {
    return;
  }
  APZController::sAPZC->SetAllowedTouchBehavior(aGuid, aBehaviors);
}

void
MetroWidget::ApzContentConsumingTouch(const ScrollableLayerGuid& aGuid)
{
  LogFunction();
  if (!mController) {
    return;
  }
  mController->ContentReceivedTouch(aGuid, true);
}

void
MetroWidget::ApzContentIgnoringTouch(const ScrollableLayerGuid& aGuid)
{
  LogFunction();
  if (!mController) {
    return;
  }
  mController->ContentReceivedTouch(aGuid, false);
}

bool
MetroWidget::ApzHitTest(ScreenIntPoint& pt)
{
  if (!mController) {
    return false;
  }
  return mController->HitTestAPZC(pt);
}

void
MetroWidget::ApzTransformGeckoCoordinate(const ScreenIntPoint& aPoint,
                                         LayoutDeviceIntPoint* aRefPointOut)
{
  if (!mController) {
    return;
  }
  mController->TransformCoordinateToGecko(aPoint, aRefPointOut);
}

nsEventStatus
MetroWidget::ApzReceiveInputEvent(WidgetInputEvent* aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid)
{
  MOZ_ASSERT(aEvent);

  if (!mController) {
    return nsEventStatus_eIgnore;
  }
  return mController->ReceiveInputEvent(aEvent, aOutTargetGuid);
}

void
MetroWidget::SetApzPendingResponseFlusher(APZPendingResponseFlusher* aFlusher)
{
  mController->SetPendingResponseFlusher(aFlusher);
}

LayerManager*
MetroWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                             LayersBackend aBackendHint,
                             LayerManagerPersistence aPersistence,
                             bool* aAllowRetaining)
{
  bool retaining = true;

  // If we initialized earlier than the view, recreate the layer manager now
  if (mLayerManager &&
      mTempBasicLayerInUse &&
      ShouldUseOffMainThreadCompositing()) {
    mLayerManager = nullptr;
    mTempBasicLayerInUse = false;
    retaining = false;
  }

  // If the backend device has changed, create a new manager (pulled from nswindow)
  if (mLayerManager) {
    if (mLayerManager->GetBackendType() == LayersBackend::LAYERS_D3D10) {
      LayerManagerD3D10 *layerManagerD3D10 =
        static_cast<LayerManagerD3D10*>(mLayerManager.get());
      if (layerManagerD3D10->device() !=
          gfxWindowsPlatform::GetPlatform()->GetD3D10Device()) {
        MOZ_ASSERT(!mLayerManager->IsInTransaction());

        mLayerManager->Destroy();
        mLayerManager = nullptr;
        retaining = false;
      }
    }
  }

  HRESULT hr = S_OK;

  // Create a layer manager: try to use an async compositor first, if enabled.
  // Otherwise fall back on the main thread d3d manager.
  if (!mLayerManager) {
    if (ShouldUseOffMainThreadCompositing()) {
      NS_ASSERTION(aShadowManager == nullptr, "Async Compositor not supported with e10s");
      CreateCompositor();
    } else if (ShouldUseMainThreadD3D10Manager()) {
      nsRefPtr<mozilla::layers::LayerManagerD3D10> layerManager =
        new mozilla::layers::LayerManagerD3D10(this);
      if (layerManager->Initialize(true, &hr)) {
        mLayerManager = layerManager;
      }
    } else if (ShouldUseBasicManager()) {
      mLayerManager = CreateBasicLayerManager();
    }
    // Either we're not ready to initialize yet due to a missing view pointer,
    // or something has gone wrong.
    if (!mLayerManager) {
      if (!mView) {
        NS_WARNING("Using temporary basic layer manager.");
        mLayerManager = new BasicLayerManager(this);
        mTempBasicLayerInUse = true;
      } else {
#ifdef MOZ_CRASHREPORTER
        if (FAILED(hr)) {
          char errorBuf[10];
          errorBuf[0] = '\0';
          _snprintf_s(errorBuf, sizeof(errorBuf), _TRUNCATE, "%X", hr);
          CrashReporter::
            AnnotateCrashReport(NS_LITERAL_CSTRING("HRESULT"),
                                nsDependentCString(errorBuf));
        }
#endif
        NS_RUNTIMEABORT("Couldn't create layer manager");
      }
    }
  }

  if (aAllowRetaining) {
    *aAllowRetaining = retaining;
  }

  return mLayerManager;
}

NS_IMETHODIMP
MetroWidget::Invalidate(bool aEraseBackground,
                        bool aUpdateNCArea,
                        bool aIncludeChildren)
{
  nsIntRect rect;
  if (mView) {
    mView->GetBounds(rect);
  }
  Invalidate(rect);
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::Invalidate(const nsIntRect & aRect)
{
  if (mWnd) {
    RECT rect;
    rect.left   = aRect.x;
    rect.top    = aRect.y;
    rect.right  = aRect.x + aRect.width;
    rect.bottom = aRect.y + aRect.height;
    InvalidateRect(mWnd, &rect, FALSE);
  }

  return NS_OK;
}

nsTransparencyMode
MetroWidget::GetTransparencyMode()
{
  return mTransparencyMode;
}

void
MetroWidget::SetTransparencyMode(nsTransparencyMode aMode)
{
  mTransparencyMode = aMode;
}

nsIWidgetListener*
MetroWidget::GetPaintListener()
{
  if (mOnDestroyCalled)
    return nullptr;
  return mAttachedWidgetListener ? mAttachedWidgetListener :
    mWidgetListener;
}

void MetroWidget::Paint(const nsIntRegion& aInvalidRegion)
{
  gfxWindowsPlatform::GetPlatform()->UpdateRenderMode();

  nsIWidgetListener* listener = GetPaintListener();
  if (!listener)
    return;

  listener->WillPaintWindow(this);

  // Refresh since calls like WillPaintWindow can destroy the widget
  listener = GetPaintListener();
  if (!listener)
    return;

  listener->PaintWindow(this, aInvalidRegion);

  listener = GetPaintListener();
  if (!listener)
    return;

  listener->DidPaintWindow();
}

void MetroWidget::UserActivity()
{
  // Check if we have the idle service, if not we try to get it.
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
  }

  // Check that we now have the idle service.
  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }
}

// InitEvent assumes physical coordinates and is used by shared win32 code. Do
// not hand winrt event coordinates to this routine.
void
MetroWidget::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (!aPoint) {
    event.refPoint.x = event.refPoint.y = 0;
  } else {
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }
  event.time = ::GetMessageTime();
}

bool
MetroWidget::DispatchWindowEvent(WidgetGUIEvent* aEvent)
{
  MOZ_ASSERT(aEvent);
  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(aEvent, status);
  return (status == nsEventStatus_eConsumeNoDefault);
}

NS_IMETHODIMP
MetroWidget::DispatchEvent(WidgetGUIEvent* event, nsEventStatus & aStatus)
{
  if (event->AsInputEvent()) {
    UserActivity();
  }

  aStatus = nsEventStatus_eIgnore;

  // Top level windows can have a view attached which requires events be sent
  // to the underlying base window and the view. Added when we combined the
  // base chrome window with the main content child for nc client area (title
  // bar) rendering.
  if (mAttachedWidgetListener) {
    aStatus = mAttachedWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }
  else if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }

  // the window can be destroyed during processing of seemingly innocuous events like, say,
  // mousedowns due to the magic of scripting. mousedowns will return nsEventStatus_eIgnore,
  // which causes problems with the deleted window. therefore:
  if (mOnDestroyCalled)
    aStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

#ifdef ACCESSIBILITY
mozilla::a11y::Accessible*
MetroWidget::GetAccessible()
{
  // We want the ability to forcibly disable a11y on windows, because
  // some non-a11y-related components attempt to bring it up.  See bug
  // 538530 for details; we have a pref here that allows it to be disabled
  // for performance and testing resons.
  //
  // This pref is checked only once, and the browser needs a restart to
  // pick up any changes.
  static int accForceDisable = -1;

  if (accForceDisable == -1) {
    const char* kPrefName = "accessibility.win32.force_disabled";
    if (Preferences::GetBool(kPrefName, false)) {
      accForceDisable = 1;
    } else {
      accForceDisable = 0;
    }
  }

  // If the pref was true, return null here, disabling a11y.
  if (accForceDisable)
      return nullptr;

  return GetRootAccessible();
}
#endif

double
MetroWidget::GetDefaultScaleInternal()
{
  return MetroUtils::ScaleFactor();
}

LayoutDeviceIntPoint
MetroWidget::CSSIntPointToLayoutDeviceIntPoint(const CSSIntPoint &aCSSPoint)
{
  CSSToLayoutDeviceScale scale = GetDefaultScale();
  LayoutDeviceIntPoint devPx(int32_t(NS_round(scale.scale * aCSSPoint.x)),
                             int32_t(NS_round(scale.scale * aCSSPoint.y)));
  return devPx;
}

float
MetroWidget::GetDPI()
{
  if (!mView) {
    return 96.0;
  }
  return mView->GetDPI();
}

void
MetroWidget::ChangedDPI()
{
  if (mWidgetListener) {
    nsIPresShell* presShell = mWidgetListener->GetPresShell();
    if (presShell) {
      presShell->BackingScaleFactorChanged();
    }
  }
}

already_AddRefed<nsIPresShell>
MetroWidget::GetPresShell()
{
  if (mWidgetListener) {
    nsCOMPtr<nsIPresShell> ps = mWidgetListener->GetPresShell();
    return ps.forget();
  }
  return nullptr;
}

NS_IMETHODIMP
MetroWidget::ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY)
{
  return NS_OK;
}

void
MetroWidget::SizeModeChanged()
{
  if (mWidgetListener) {
    mWidgetListener->SizeModeChanged(nsSizeMode_Normal);
  }
}

void
MetroWidget::Activated(bool aActiveated)
{
  if (mWidgetListener) {
    aActiveated ?
      mWidgetListener->WindowActivated() :
      mWidgetListener->WindowDeactivated();
  }
}

NS_IMETHODIMP
MetroWidget::Move(double aX, double aY)
{
  NotifyWindowMoved(aX, aY);
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::Resize(double aWidth, double aHeight, bool aRepaint)
{
  return Resize(0, 0, aWidth, aHeight, aRepaint);
}

NS_IMETHODIMP
MetroWidget::Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint)
{
  WinUtils::Log("Resize: %f %f %f %f", aX, aY, aWidth, aHeight);
  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, aWidth, aHeight);
  }
  if (mWidgetListener) {
    mWidgetListener->WindowResized(this, aWidth, aHeight);
  }
  Invalidate();
  return NS_OK;
}

NS_IMETHODIMP
MetroWidget::SetFocus(bool aRaise)
{
  return NS_OK;
}

nsresult
MetroWidget::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  return NS_OK;
}

void*
MetroWidget::GetNativeData(uint32_t aDataType)
{
  switch(aDataType) {
    case NS_NATIVE_WINDOW:
      return mWnd;
    case NS_NATIVE_ICOREWINDOW:
      if (mView) {
        return reinterpret_cast<IUnknown*>(mView->GetCoreWindow());
      }
      break;
   case NS_NATIVE_TSF_THREAD_MGR:
   case NS_NATIVE_TSF_CATEGORY_MGR:
   case NS_NATIVE_TSF_DISPLAY_ATTR_MGR:
     return nsTextStore::GetNativeData(aDataType);
  }
  return nullptr;
}

void
MetroWidget::FreeNativeData(void * data, uint32_t aDataType)
{
}

NS_IMETHODIMP
MetroWidget::SetTitle(const nsAString& aTitle)
{
  return NS_OK;
}

nsIntPoint
MetroWidget::WidgetToScreenOffset()
{
  return nsIntPoint(0,0);
}

NS_IMETHODIMP
MetroWidget::CaptureRollupEvents(nsIRollupListener * aListener,
                                 bool aDoCapture)
{
  return NS_OK;
}

NS_IMETHODIMP_(void)
MetroWidget::SetInputContext(const InputContext& aContext,
                             const InputContextAction& aAction)
{
  mInputContext = aContext;
  nsTextStore::SetInputContext(this, mInputContext, aAction);
  bool enable = (mInputContext.mIMEState.mEnabled == IMEState::ENABLED ||
                 mInputContext.mIMEState.mEnabled == IMEState::PLUGIN);
  if (enable &&
      mInputContext.mIMEState.mOpen != IMEState::DONT_CHANGE_OPEN_STATE) {
    bool open = (mInputContext.mIMEState.mOpen == IMEState::OPEN);
    nsTextStore::SetIMEOpenState(open);
  }
}

NS_IMETHODIMP_(nsIWidget::InputContext)
MetroWidget::GetInputContext()
{
  return mInputContext;
}

NS_IMETHODIMP
MetroWidget::NotifyIME(const IMENotification& aIMENotification)
{
  switch (aIMENotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      nsTextStore::CommitComposition(false);
      return NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      nsTextStore::CommitComposition(true);
      return NS_OK;
    case NOTIFY_IME_OF_FOCUS:
      return nsTextStore::OnFocusChange(true, this,
                                        mInputContext.mIMEState.mEnabled);
    case NOTIFY_IME_OF_BLUR:
      return nsTextStore::OnFocusChange(false, this,
                                        mInputContext.mIMEState.mEnabled);
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      return nsTextStore::OnSelectionChange();
    case NOTIFY_IME_OF_TEXT_CHANGE:
      return nsTextStore::OnTextChange(aIMENotification);
    case NOTIFY_IME_OF_POSITION_CHANGE:
      return nsTextStore::OnLayoutChange();
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP
MetroWidget::GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState)
{
  NS_ENSURE_ARG_POINTER(aLEDState);
  *aLEDState = (::GetKeyState(aKeyCode) & 1) != 0;
  return NS_OK;
}

nsIMEUpdatePreference
MetroWidget::GetIMEUpdatePreference()
{
  return nsTextStore::GetIMEUpdatePreference();
}

NS_IMETHODIMP
MetroWidget::ReparentNativeWidget(nsIWidget* aNewParent)
{
  return NS_OK;
}

void
MetroWidget::SuppressBlurEvents(bool aSuppress)
{
}

bool
MetroWidget::BlurEventsSuppressed()
{
  return false;
}

void
MetroWidget::PickerOpen()
{
}

void
MetroWidget::PickerClosed()
{
}

bool
MetroWidget::HasPendingInputEvent()
{
  if (HIWORD(GetQueueStatus(QS_INPUT)))
    return true;
  return false;
}

NS_IMETHODIMP
MetroWidget::Observe(nsISupports *subject, const char *topic, const char16_t *data)
{
  NS_ENSURE_ARG_POINTER(topic);
  if (!strcmp(topic, "apzc-zoom-to-rect")) {
    CSSRect rect = CSSRect();
    uint64_t viewId = 0;
    int32_t presShellId = 0;

    int reScan = swscanf(data, L"%f,%f,%f,%f,%d,%llu",
                 &rect.x, &rect.y, &rect.width, &rect.height,
                 &presShellId, &viewId);
    if(reScan != 6) {
      NS_WARNING("Malformed apzc-zoom-to-rect message");
    }

    ScrollableLayerGuid guid = ScrollableLayerGuid(mRootLayerTreeId, presShellId, viewId);
    APZController::sAPZC->ZoomToRect(guid, rect);
  }
  else if (!strcmp(topic, "apzc-disable-zoom")) {
    uint64_t viewId = 0;
    int32_t presShellId = 0;

    int reScan = swscanf(data, L"%d,%llu",
      &presShellId, &viewId);
    if (reScan != 2) {
      NS_WARNING("Malformed apzc-disable-zoom message");
    }

    ScrollableLayerGuid guid = ScrollableLayerGuid(mRootLayerTreeId, presShellId, viewId);
    APZController::sAPZC->UpdateZoomConstraints(guid,
      ZoomConstraints(false, false, CSSToScreenScale(1.0f), CSSToScreenScale(1.0f)));
  }
  return NS_OK;
}
