/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/Logging.h"

#include "WinMouseScrollHandler.h"
#include "nsWindow.h"
#include "nsWindowDefs.h"
#include "KeyboardLayout.h"
#include "WinUtils.h"
#include "nsGkAtoms.h"
#include "nsIDOMWindowUtils.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/WheelEventBinding.h"
#include "mozilla/StaticPrefs_mousewheel.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/widget/WinRegistry.h"

#include <psapi.h>

namespace mozilla {
namespace widget {

LazyLogModule gMouseScrollLog("MouseScrollHandlerWidgets");

MouseScrollHandler* MouseScrollHandler::sInstance = nullptr;

bool MouseScrollHandler::Device::sFakeScrollableWindowNeeded = false;

bool MouseScrollHandler::Device::SynTP::sInitialized = false;
int32_t MouseScrollHandler::Device::SynTP::sMajorVersion = 0;
int32_t MouseScrollHandler::Device::SynTP::sMinorVersion = -1;

bool MouseScrollHandler::Device::Elantech::sUseSwipeHack = false;
bool MouseScrollHandler::Device::Elantech::sUsePinchHack = false;
DWORD MouseScrollHandler::Device::Elantech::sZoomUntil = 0;

bool MouseScrollHandler::Device::Apoint::sInitialized = false;
int32_t MouseScrollHandler::Device::Apoint::sMajorVersion = 0;
int32_t MouseScrollHandler::Device::Apoint::sMinorVersion = -1;

bool MouseScrollHandler::Device::SetPoint::sMightBeUsing = false;

// The duration until timeout of events transaction.  The value is 1.5 sec,
// it's just a magic number, it was suggested by Logitech's engineer, see
// bug 605648 comment 90.
#define DEFAULT_TIMEOUT_DURATION 1500

/******************************************************************************
 *
 * SynthesizingEvent: declarations
 *
 ******************************************************************************/

// Maintains state displaced by test-initiated synthesized events. Not
// instantiated under ordinary release-mode operation.
class MouseScrollHandler::SynthesizingEvent {
 public:
  SynthesizingEvent()
      : mWnd(nullptr),
        mMessage(0),
        mWParam(0),
        mLParam(0),
        mStatus(NOT_SYNTHESIZING) {}

  ~SynthesizingEvent() {}

  static SynthesizingEvent* GetActiveInstance();

  nsresult Synthesize(const POINTS& aCursorPoint, HWND aWnd, UINT aMessage,
                      WPARAM aWParam, LPARAM aLParam,
                      const BYTE (&aKeyStates)[256]);

  void NotifyMessageReceived(nsWindow* aExpectedWindow, UINT msg, WPARAM wParam,
                             LPARAM lParam);

  void NotifyMessageHandlingFinished();

  const POINTS& GetCursorPoint() const { return mCursorPoint; }

 private:
  POINTS mCursorPoint;
  HWND mWnd;
  UINT mMessage;
  WPARAM mWParam;
  LPARAM mLParam;
  BYTE mKeyState[256];
  BYTE mOriginalKeyState[256];

  enum Status {
    NOT_SYNTHESIZING,
    SENDING_MESSAGE,
  };
  Status mStatus;

  const char* GetStatusName() {
    switch (mStatus) {
      case NOT_SYNTHESIZING:
        return "NOT_SYNTHESIZING";
      case SENDING_MESSAGE:
        return "SENDING_MESSAGE";
      default:
        return "Unknown";
    }
  }

  void Finish();
};  // SynthesizingEvent

/******************************************************************************
 *
 * MouseScrollHandler
 *
 ******************************************************************************/

/// Convenience alias.
/* static */
auto MouseScrollHandler::GetActiveSynthEvent() -> SynthesizingEvent* {
  return SynthesizingEvent::GetActiveInstance();
}

/* static */
POINTS
MouseScrollHandler::GetCurrentMessagePos() {
  if (auto* synth = GetActiveSynthEvent()) {
    return synth->GetCursorPoint();
  }
  DWORD pos = ::GetMessagePos();
  return MAKEPOINTS(pos);
}

// Get rid of the GetMessagePos() API.
#define GetMessagePos()

/* static */
void MouseScrollHandler::Initialize() { Device::Init(); }

/* static */
void MouseScrollHandler::Shutdown() {
  delete sInstance;
  sInstance = nullptr;
}

/* static */
MouseScrollHandler* MouseScrollHandler::GetInstance() {
  if (!sInstance) {
    sInstance = new MouseScrollHandler();
  }
  return sInstance;
}

MouseScrollHandler::MouseScrollHandler() {
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll: Creating an instance, this=%p, sInstance=%p", this,
           sInstance));
}

MouseScrollHandler::~MouseScrollHandler() {
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll: Destroying an instance, this=%p, sInstance=%p", this,
           sInstance));
}

/* static */
void MouseScrollHandler::MaybeLogKeyState() {
  if (!MOZ_LOG_TEST(gMouseScrollLog, LogLevel::Debug)) {
    return;
  }
  BYTE keyboardState[256];
  if (::GetKeyboardState(keyboardState)) {
    for (size_t i = 0; i < std::size(keyboardState); i++) {
      if (keyboardState[i]) {
        MOZ_LOG(gMouseScrollLog, LogLevel::Debug,
                ("    Current key state: keyboardState[0x%02zX]=0x%02X (%s)", i,
                 keyboardState[i],
                 ((keyboardState[i] & 0x81) == 0x81) ? "Pressed and Toggled"
                 : (keyboardState[i] & 0x80)         ? "Pressed"
                 : (keyboardState[i] & 0x01)         ? "Toggled"
                                                     : "Unknown"));
      }
    }
  } else {
    MOZ_LOG(
        gMouseScrollLog, LogLevel::Debug,
        ("MouseScroll::MaybeLogKeyState(): Failed to print current keyboard "
         "state"));
  }
}

bool MouseScrollHandler::ProcessMouseMessage(UINT msg, WPARAM wParam,
                                             LPARAM lParam,
                                             MSGResult& aResult) {
  // Select the appropriate message handler.
  using HandlerT =
      bool (MouseScrollHandler::*)(nsWindow*, UINT, WPARAM, LPARAM);
  HandlerT const handler = [&]() -> HandlerT {
    switch (msg) {
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
        return &MouseScrollHandler::HandleMouseWheelMessage;
      case WM_VSCROLL:
      case WM_HSCROLL:
        if (lParam || mUserPrefs.IsScrollMessageHandledAsWheelMessage()) {
          return &MouseScrollHandler::HandleScrollMessageAsMouseWheelMessage;
        }
        return &MouseScrollHandler::HandleScrollMessageAsItself;
      default:
        MOZ_ASSERT(false, "wrong message type in ProcessMouseMessage");
        return nullptr;
    }
  }();
  if (!handler) {
    return false;
  }

  // Find the appropriate nsWindow to handle this message. (This is not
  // necessarily the window to which the message was sent!)
  nsWindow* const destWindow = FindTargetWindow(msg, wParam, lParam);

  // Emit a warning if the received message is unexpected, given the
  // synthesis-state.
  if (auto* synth = GetActiveSynthEvent()) {
    synth->NotifyMessageReceived(destWindow, msg, wParam, lParam);
  }

  if (!destWindow) {
    // Not over our window; return without consuming. (This will not recurse.)
    aResult.mConsumed = false;
    return true;
  }

  // Actually handle the message.
  aResult.mConsumed =
      (GetInstance()->*handler)(destWindow, msg, wParam, lParam);
  aResult.mResult = 0;

  // Reset the synthesis-state, if necessary.
  if (auto* synth = GetActiveSynthEvent()) {
    synth->NotifyMessageHandlingFinished();
  }

  return true;
}

/* static */
bool MouseScrollHandler::ProcessMessage(nsWindow* aWidget, UINT msg,
                                        WPARAM wParam, LPARAM lParam,
                                        MSGResult& aResult) {
  Device::Elantech::UpdateZoomUntil();

  switch (msg) {
    case WM_SETTINGCHANGE:
      if (!sInstance) {
        return false;
      }
      if (wParam == SPI_SETWHEELSCROLLLINES ||
          wParam == SPI_SETWHEELSCROLLCHARS) {
        sInstance->mSystemSettings.MarkDirty();
      }
      return false;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_HSCROLL:
    case WM_VSCROLL:
      return GetInstance()->ProcessMouseMessage(msg, wParam, lParam, aResult);

    case WM_KEYDOWN:
    case WM_KEYUP:
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
              ("MouseScroll::ProcessMessage(): aWidget=%p, "
               "msg=%s(0x%04X), wParam=0x%02zX, ::GetMessageTime()=%ld",
               aWidget,
               msg == WM_KEYDOWN ? "WM_KEYDOWN"
               : msg == WM_KEYUP ? "WM_KEYUP"
                                 : "Unknown",
               msg, wParam, ::GetMessageTime()));
      MaybeLogKeyState();
      if (Device::Elantech::HandleKeyMessage(aWidget, msg, wParam, lParam)) {
        aResult.mResult = 0;
        aResult.mConsumed = true;
        return true;
      }
      return false;

    default:
      return false;
  }
}

/* static */
nsresult MouseScrollHandler::SynthesizeNativeMouseScrollEvent(
    nsWindow* aWidget, const LayoutDeviceIntPoint& aPoint,
    uint32_t aNativeMessage, int32_t aDelta, uint32_t aModifierFlags,
    uint32_t aAdditionalFlags) {
  bool useFocusedWindow = !(
      aAdditionalFlags & nsIDOMWindowUtils::MOUSESCROLL_PREFER_WIDGET_AT_POINT);

  POINT pt;
  pt.x = aPoint.x;
  pt.y = aPoint.y;

  HWND target = useFocusedWindow ? ::WindowFromPoint(pt) : ::GetFocus();
  NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

  WPARAM wParam = 0;
  LPARAM lParam = 0;
  switch (aNativeMessage) {
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
      lParam = MAKELPARAM(pt.x, pt.y);
      WORD mod = 0;
      if (aModifierFlags & (nsIWidget::CTRL_L | nsIWidget::CTRL_R)) {
        mod |= MK_CONTROL;
      }
      if (aModifierFlags & (nsIWidget::SHIFT_L | nsIWidget::SHIFT_R)) {
        mod |= MK_SHIFT;
      }
      wParam = MAKEWPARAM(mod, aDelta);
      break;
    }
    case WM_VSCROLL:
    case WM_HSCROLL:
      lParam = (aAdditionalFlags &
                nsIDOMWindowUtils::MOUSESCROLL_WIN_SCROLL_LPARAM_NOT_NULL)
                   ? reinterpret_cast<LPARAM>(target)
                   : 0;
      wParam = aDelta;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  // Ensure to make the instance.
  GetInstance();

  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  AutoTArray<KeyPair, 10> keySequence;
  WinUtils::SetupKeyModifiersSequence(&keySequence, aModifierFlags,
                                      aNativeMessage);

  for (uint32_t i = 0; i < keySequence.Length(); ++i) {
    uint8_t key = keySequence[i].mGeneral;
    uint8_t keySpecific = keySequence[i].mSpecific;
    kbdState[key] = 0x81;  // key is down and toggled on if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0x81;
    }
  }

  if (!sInstance->mSynthesizingEvent) {
    sInstance->mSynthesizingEvent = MakeUnique<SynthesizingEvent>();
  }

  POINTS pts;
  pts.x = static_cast<SHORT>(pt.x);
  pts.y = static_cast<SHORT>(pt.y);
  return sInstance->mSynthesizingEvent->Synthesize(pts, target, aNativeMessage,
                                                   wParam, lParam, kbdState);
}

/* static */
void MouseScrollHandler::InitEvent(nsWindow* aWidget, WidgetGUIEvent& aEvent,
                                   LPARAM* aPoint) {
  NS_ENSURE_TRUE_VOID(aWidget);

  // If a point is provided, use it; otherwise, get current message point or
  // synthetic point
  POINTS pointOnScreen;
  if (aPoint != nullptr) {
    pointOnScreen = MAKEPOINTS(*aPoint);
  } else {
    pointOnScreen = GetCurrentMessagePos();
  }

  // InitEvent expects the point to be in window coordinates, so translate the
  // point from screen coordinates.
  POINT pointOnWindow;
  POINTSTOPOINT(pointOnWindow, pointOnScreen);
  ::ScreenToClient(aWidget->GetWindowHandle(), &pointOnWindow);

  LayoutDeviceIntPoint point;
  point.x = pointOnWindow.x;
  point.y = pointOnWindow.y;

  aWidget->InitEvent(aEvent, &point);
}

/* static */
ModifierKeyState MouseScrollHandler::GetModifierKeyState(UINT aMessage) {
  ModifierKeyState result;
  // Assume the Control key is down if the Elantech touchpad has sent the
  // mis-ordered WM_KEYDOWN/WM_MOUSEWHEEL messages.  (See the comment in
  // MouseScrollHandler::Device::Elantech::HandleKeyMessage().)
  if (aMessage == WM_MOUSEWHEEL && !result.IsControl() &&
      Device::Elantech::IsZooming()) {
    // XXX Do we need to unset MODIFIER_SHIFT, MODIFIER_ALT, MODIFIER_META too?
    //     If one of them are true, the default action becomes not zooming.
    result.Unset(MODIFIER_ALTGRAPH);
    result.Set(MODIFIER_CONTROL);
  }
  return result;
}

POINT
MouseScrollHandler::ComputeMessagePos(UINT aMessage, WPARAM aWParam,
                                      LPARAM aLParam) {
  POINT point;
  if (Device::SetPoint::IsGetMessagePosResponseValid(aMessage, aWParam,
                                                     aLParam)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::ComputeMessagePos: Using ::GetCursorPos()"));
    ::GetCursorPos(&point);
  } else {
    POINTS pts = GetCurrentMessagePos();
    point.x = pts.x;
    point.y = pts.y;
  }
  return point;
}

nsWindow* MouseScrollHandler::FindTargetWindow(UINT aMessage, WPARAM aWParam,
                                               LPARAM aLParam) {
  POINT point = ComputeMessagePos(aMessage, aWParam, aLParam);

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::FindTargetWindow: "
           "aMessage=%s, wParam=0x%08zX, lParam=0x%08" PRIXLPTR
           ", point: { x=%ld, y=%ld }",
           aMessage == WM_MOUSEWHEEL    ? "WM_MOUSEWHEEL"
           : aMessage == WM_MOUSEHWHEEL ? "WM_MOUSEHWHEEL"
           : aMessage == WM_VSCROLL     ? "WM_VSCROLL"
                                        : "WM_HSCROLL",
           aWParam, aLParam, point.x, point.y));
  MaybeLogKeyState();

  HWND underCursorWnd = ::WindowFromPoint(point);
  if (!underCursorWnd) {
    // This is unsurprising: Windows ordinarily sends wheel messages to the
    // focused window, regardless of cursor position. (Nowadays, this is
    // configurable in Windows' settings, but we've always deliberately
    // overridden this behavior in Gecko; see bug 168354.)
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::FindTargetWindow: "
             "No window was found under the cursor"));
    return nullptr;
  }

  if (Device::Elantech::IsPinchHackNeeded() &&
      Device::Elantech::IsHelperWindow(underCursorWnd)) {
    // The Elantech driver places a window right underneath the cursor
    // when sending a WM_MOUSEWHEEL event to us as part of a pinch-to-zoom
    // gesture.  We detect that here, and search for our window that would
    // be beneath the cursor if that window wasn't there.
    underCursorWnd = WinUtils::FindOurWindowAtPoint(point);
    if (!underCursorWnd) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
              ("MouseScroll::FindTargetWindow: "
               "Our window is not found under the Elantech helper window"));
      return nullptr;
    }
  }

  // If the window under the mouse cursor is not in our process, we assume it's
  // another application's window, and discard the message.
  if (!WinUtils::IsOurProcessWindow(underCursorWnd)) {
    return nullptr;
  }

  // Otherwise, (try to) handle this message on the nsWindow it's associated
  // with.
  if (nsWindow* destWindow = WinUtils::GetNSWindowPtr(underCursorWnd)) {
    return destWindow;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::FindTargetWindow: "
           "Window found under the cursor isn't an nsWindow..."));
  HWND wnd = ::GetParent(underCursorWnd);
  for (; wnd; wnd = ::GetParent(wnd)) {
    if (nsWindow* destWindow = WinUtils::GetNSWindowPtr(wnd)) {
      return destWindow;
    }
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::FindTargetWindow: "
           "    ...and doesn't have any nsWindow ancestors"));
  return nullptr;
}

bool MouseScrollHandler::HandleScrollMessageAsItself(nsWindow* aWidget,
                                                     UINT aMessage,
                                                     WPARAM aWParam,
                                                     LPARAM aLParam) {
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::HandleScrollMessageAsItself: aWidget=%p, "
           "aMessage=%s, wParam=0x%08zX, lParam=0x%08" PRIXLPTR,
           aWidget, aMessage == WM_VSCROLL ? "WM_VSCROLL" : "WM_HSCROLL",
           aWParam, aLParam));

  // Scroll message generated by external application
  WidgetContentCommandEvent commandEvent(true, eContentCommandScroll, aWidget);
  commandEvent.mScroll.mIsHorizontal = (aMessage == WM_HSCROLL);

  switch (LOWORD(aWParam)) {
    case SB_LINEUP:  // SB_LINELEFT
      commandEvent.mScroll.mUnit =
          WidgetContentCommandEvent::eCmdScrollUnit_Line;
      commandEvent.mScroll.mAmount = -1;
      break;
    case SB_LINEDOWN:  // SB_LINERIGHT
      commandEvent.mScroll.mUnit =
          WidgetContentCommandEvent::eCmdScrollUnit_Line;
      commandEvent.mScroll.mAmount = 1;
      break;
    case SB_PAGEUP:  // SB_PAGELEFT
      commandEvent.mScroll.mUnit =
          WidgetContentCommandEvent::eCmdScrollUnit_Page;
      commandEvent.mScroll.mAmount = -1;
      break;
    case SB_PAGEDOWN:  // SB_PAGERIGHT
      commandEvent.mScroll.mUnit =
          WidgetContentCommandEvent::eCmdScrollUnit_Page;
      commandEvent.mScroll.mAmount = 1;
      break;
    case SB_TOP:  // SB_LEFT
      commandEvent.mScroll.mUnit =
          WidgetContentCommandEvent::eCmdScrollUnit_Whole;
      commandEvent.mScroll.mAmount = -1;
      break;
    case SB_BOTTOM:  // SB_RIGHT
      commandEvent.mScroll.mUnit =
          WidgetContentCommandEvent::eCmdScrollUnit_Whole;
      commandEvent.mScroll.mAmount = 1;
      break;
    default:
      return false;
  }

  aWidget->DispatchContentCommandEvent(&commandEvent);
  return true;
}

bool MouseScrollHandler::HandleMouseWheelMessage(nsWindow* aWidget,
                                                 UINT aMessage, WPARAM aWParam,
                                                 LPARAM aLParam) {
  // for logging only
  const char* const msgName [[maybe_unused]] = [&]() {
    switch (aMessage) {
      case WM_MOUSEWHEEL:
        return "WM_MOUSEWHEEL";
      case WM_MOUSEHWHEEL:
        return "WM_MOUSEHWHEEL";
      default:
        return "err";
    }
  }();

  MOZ_ASSERT((aMessage == WM_MOUSEWHEEL || aMessage == WM_MOUSEHWHEEL),
             "HandleMouseWheelMessage must be called with "
             "WM_MOUSEWHEEL or WM_MOUSEHWHEEL");

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::HandleMouseWheelMessage: aWidget=%p, "
           "aMessage=%s, aWParam=0x%08zX, aLParam=0x%08" PRIXLPTR,
           aWidget, msgName, aWParam, aLParam));

  // If it's not allowed to cache system settings, we need to reset the cache
  // before handling the mouse wheel message.
  mSystemSettings.TrustedScrollSettingsDriver();

  EventInfo eventInfo(aWidget, aMessage, aWParam, aLParam);
  if (!eventInfo.CanDispatchWheelEvent()) {
    MOZ_LOG(
        gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::HandleMouseWheelMessage: Cannot dispatch the events"));
    mLastEventInfo.ResetTransaction();
    return false;
  }

  // Discard the remaining delta if current wheel message and last one are
  // received by different window or to scroll different direction or
  // different unit scroll.  Furthermore, if the last event was too old.
  if (!mLastEventInfo.CanContinueTransaction(eventInfo)) {
    mLastEventInfo.ResetTransaction();
  }

  mLastEventInfo.RecordEvent(eventInfo);

  ModifierKeyState modKeyState = GetModifierKeyState(aMessage);

  WidgetWheelEvent wheelEvent(true, eWheel, aWidget);
  if (mLastEventInfo.InitWheelEvent(aWidget, wheelEvent, modKeyState,
                                    aLParam)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::HandleMouseWheelMessage: dispatching "
             "eWheel event"));
    aWidget->DispatchWheelEvent(&wheelEvent);
    if (aWidget->Destroyed()) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
              ("MouseScroll::HandleMouseWheelMessage: The window was destroyed "
               "by eWheel event"));
      mLastEventInfo.ResetTransaction();
    }
    return true;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::HandleMouseWheelMessage: eWheel event was not "
           "dispatched"));
  return false;
}

bool MouseScrollHandler::HandleScrollMessageAsMouseWheelMessage(
    nsWindow* aWidget, UINT aMessage, WPARAM aWParam, LPARAM aLParam) {
  // for logging only
  const char* const msgName [[maybe_unused]] = [&]() {
    switch (aMessage) {
      case WM_VSCROLL:
        return "WM_VSCROLL";
      case WM_HSCROLL:
        return "WM_HSCROLL";
      default:
        return "err";
    }
  }();

  MOZ_ASSERT((aMessage == WM_VSCROLL || aMessage == WM_HSCROLL),
             "HandleScrollMessageAsMouseWheelMessage must be called with "
             "WM_VSCROLL or WM_HSCROLL");

  ModifierKeyState modKeyState = GetModifierKeyState(aMessage);

  WidgetWheelEvent wheelEvent(true, eWheel, aWidget);
  double& delta =
      (aMessage == WM_VSCROLL) ? wheelEvent.mDeltaY : wheelEvent.mDeltaX;
  int32_t& lineOrPageDelta = (aMessage == WM_VSCROLL)
                                 ? wheelEvent.mLineOrPageDeltaY
                                 : wheelEvent.mLineOrPageDeltaX;

  delta = 1.0;
  lineOrPageDelta = 1;

  switch (LOWORD(aWParam)) {
    case SB_PAGEUP:
      delta = -1.0;
      lineOrPageDelta = -1;
      [[fallthrough]];
    case SB_PAGEDOWN:
      wheelEvent.mDeltaMode = dom::WheelEvent_Binding::DOM_DELTA_PAGE;
      break;

    case SB_LINEUP:
      delta = -1.0;
      lineOrPageDelta = -1;
      [[fallthrough]];
    case SB_LINEDOWN:
      wheelEvent.mDeltaMode = dom::WheelEvent_Binding::DOM_DELTA_LINE;
      break;

    default:
      return false;
  }
  modKeyState.InitInputEvent(wheelEvent);

  // Current mouse position may not be same as when the original message
  // is received. However, this data is not available with the original
  // message, which is why nullptr is passed in. We need to know the actual
  // mouse cursor position when the original message was received.
  InitEvent(aWidget, wheelEvent, nullptr);

  MOZ_LOG(
      gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::HandleScrollMessageAsMouseWheelMessage: aWidget=%p, "
       "aMessage=%s, aWParam=0x%08zX, aLParam=0x%08" PRIXLPTR ", "
       "wheelEvent { mRefPoint: { x: %d, y: %d }, mDeltaX: %f, mDeltaY: %f, "
       "mLineOrPageDeltaX: %d, mLineOrPageDeltaY: %d, "
       "isShift: %s, isControl: %s, isAlt: %s, isMeta: %s }",
       aWidget, msgName, aWParam, aLParam, wheelEvent.mRefPoint.x.value,
       wheelEvent.mRefPoint.y.value, wheelEvent.mDeltaX, wheelEvent.mDeltaY,
       wheelEvent.mLineOrPageDeltaX, wheelEvent.mLineOrPageDeltaY,
       GetBoolName(wheelEvent.IsShift()), GetBoolName(wheelEvent.IsControl()),
       GetBoolName(wheelEvent.IsAlt()), GetBoolName(wheelEvent.IsMeta())));

  aWidget->DispatchWheelEvent(&wheelEvent);
  return true;
}

/******************************************************************************
 *
 * EventInfo
 *
 ******************************************************************************/

MouseScrollHandler::EventInfo::EventInfo(nsWindow* aWidget, UINT aMessage,
                                         WPARAM aWParam, LPARAM aLParam) {
  MOZ_ASSERT(
      aMessage == WM_MOUSEWHEEL || aMessage == WM_MOUSEHWHEEL,
      "EventInfo must be initialized with WM_MOUSEWHEEL or WM_MOUSEHWHEEL");

  MouseScrollHandler::GetInstance()->mSystemSettings.Init();

  mIsVertical = (aMessage == WM_MOUSEWHEEL);
  mIsPage =
      MouseScrollHandler::sInstance->mSystemSettings.IsPageScroll(mIsVertical);
  mDelta = (short)HIWORD(aWParam);
  mWnd = aWidget->GetWindowHandle();
  mTimeStamp = TimeStamp::Now();
}

bool MouseScrollHandler::EventInfo::CanDispatchWheelEvent() const {
  if (!GetScrollAmount()) {
    // XXX I think that we should dispatch mouse wheel events even if the
    // operation will not scroll because the wheel operation really happened
    // and web application may want to handle the event for non-scroll action.
    return false;
  }

  return (mDelta != 0);
}

int32_t MouseScrollHandler::EventInfo::GetScrollAmount() const {
  if (mIsPage) {
    return 1;
  }
  return MouseScrollHandler::sInstance->mSystemSettings.GetScrollAmount(
      mIsVertical);
}

/******************************************************************************
 *
 * LastEventInfo
 *
 ******************************************************************************/

bool MouseScrollHandler::LastEventInfo::CanContinueTransaction(
    const EventInfo& aNewEvent) {
  int32_t timeout = MouseScrollHandler::sInstance->mUserPrefs
                        .GetMouseScrollTransactionTimeout();
  return !mWnd ||
         (mWnd == aNewEvent.GetWindowHandle() &&
          IsPositive() == aNewEvent.IsPositive() &&
          mIsVertical == aNewEvent.IsVertical() &&
          mIsPage == aNewEvent.IsPage() &&
          (timeout < 0 || TimeStamp::Now() - mTimeStamp <=
                              TimeDuration::FromMilliseconds(timeout)));
}

void MouseScrollHandler::LastEventInfo::ResetTransaction() {
  if (!mWnd) {
    return;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::LastEventInfo::ResetTransaction()"));

  mWnd = nullptr;
  mAccumulatedDelta = 0;
}

void MouseScrollHandler::LastEventInfo::RecordEvent(const EventInfo& aEvent) {
  mWnd = aEvent.GetWindowHandle();
  mDelta = aEvent.GetNativeDelta();
  mIsVertical = aEvent.IsVertical();
  mIsPage = aEvent.IsPage();
  mTimeStamp = TimeStamp::Now();
}

/* static */
int32_t MouseScrollHandler::LastEventInfo::RoundDelta(double aDelta) {
  return (aDelta >= 0) ? (int32_t)floor(aDelta) : (int32_t)ceil(aDelta);
}

bool MouseScrollHandler::LastEventInfo::InitWheelEvent(
    nsWindow* aWidget, WidgetWheelEvent& aWheelEvent,
    const ModifierKeyState& aModKeyState, LPARAM aLParam) {
  MOZ_ASSERT(aWheelEvent.mMessage == eWheel);

  if (StaticPrefs::mousewheel_ignore_cursor_position_in_lparam()) {
    InitEvent(aWidget, aWheelEvent, nullptr);
  } else {
    InitEvent(aWidget, aWheelEvent, &aLParam);
  }

  aModKeyState.InitInputEvent(aWheelEvent);

  // Our positive delta value means to bottom or right.
  // But positive native delta value means to top or right.
  // Use orienter for computing our delta value with native delta value.
  int32_t orienter = mIsVertical ? -1 : 1;

  aWheelEvent.mDeltaMode = mIsPage ? dom::WheelEvent_Binding::DOM_DELTA_PAGE
                                   : dom::WheelEvent_Binding::DOM_DELTA_LINE;

  double ticks = double(mDelta) * orienter / double(WHEEL_DELTA);
  if (mIsVertical) {
    aWheelEvent.mWheelTicksY = ticks;
  } else {
    aWheelEvent.mWheelTicksX = ticks;
  }

  double& delta = mIsVertical ? aWheelEvent.mDeltaY : aWheelEvent.mDeltaX;
  int32_t& lineOrPageDelta = mIsVertical ? aWheelEvent.mLineOrPageDeltaY
                                         : aWheelEvent.mLineOrPageDeltaX;

  double nativeDeltaPerUnit =
      mIsPage ? double(WHEEL_DELTA) : double(WHEEL_DELTA) / GetScrollAmount();

  delta = double(mDelta) * orienter / nativeDeltaPerUnit;
  mAccumulatedDelta += mDelta;
  lineOrPageDelta =
      mAccumulatedDelta * orienter / RoundDelta(nativeDeltaPerUnit);
  mAccumulatedDelta -=
      lineOrPageDelta * orienter * RoundDelta(nativeDeltaPerUnit);

  if (aWheelEvent.mDeltaMode != dom::WheelEvent_Binding::DOM_DELTA_LINE) {
    // If the scroll delta mode isn't per line scroll, we shouldn't allow to
    // override the system scroll speed setting.
    aWheelEvent.mAllowToOverrideSystemScrollSpeed = false;
  }

  MOZ_LOG(
      gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::LastEventInfo::InitWheelEvent: aWidget=%p, "
       "aWheelEvent { mRefPoint: { x: %d, y: %d }, mDeltaX: %f, mDeltaY: %f, "
       "mLineOrPageDeltaX: %d, mLineOrPageDeltaY: %d, "
       "isShift: %s, isControl: %s, isAlt: %s, isMeta: %s, "
       "mAllowToOverrideSystemScrollSpeed: %s }, "
       "mAccumulatedDelta: %d",
       aWidget, aWheelEvent.mRefPoint.x.value, aWheelEvent.mRefPoint.y.value,
       aWheelEvent.mDeltaX, aWheelEvent.mDeltaY, aWheelEvent.mLineOrPageDeltaX,
       aWheelEvent.mLineOrPageDeltaY, GetBoolName(aWheelEvent.IsShift()),
       GetBoolName(aWheelEvent.IsControl()), GetBoolName(aWheelEvent.IsAlt()),
       GetBoolName(aWheelEvent.IsMeta()),
       GetBoolName(aWheelEvent.mAllowToOverrideSystemScrollSpeed),
       mAccumulatedDelta));

  return (delta != 0);
}

/******************************************************************************
 *
 * SystemSettings
 *
 ******************************************************************************/

void MouseScrollHandler::SystemSettings::Init() {
  if (mInitialized) {
    return;
  }

  InitScrollLines();
  InitScrollChars();

  mInitialized = true;

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::SystemSettings::Init(): initialized, "
           "mScrollLines=%d, mScrollChars=%d",
           mScrollLines, mScrollChars));
}

bool MouseScrollHandler::SystemSettings::InitScrollLines() {
  int32_t oldValue = mInitialized ? mScrollLines : 0;
  mIsReliableScrollLines = false;
  mScrollLines = MouseScrollHandler::sInstance->mUserPrefs
                     .GetOverriddenVerticalScrollAmout();
  if (mScrollLines >= 0) {
    // overridden by the pref.
    mIsReliableScrollLines = true;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::SystemSettings::InitScrollLines(): mScrollLines is "
             "overridden by the pref: %d",
             mScrollLines));
  } else if (!::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &mScrollLines,
                                     0)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::SystemSettings::InitScrollLines(): "
             "::SystemParametersInfo("
             "SPI_GETWHEELSCROLLLINES) failed"));
    mScrollLines = DefaultScrollLines();
  }

  if (mScrollLines > WHEEL_DELTA) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::SystemSettings::InitScrollLines(): the result of "
             "::SystemParametersInfo(SPI_GETWHEELSCROLLLINES) is too large: %d",
             mScrollLines));
    // sScrollLines usually equals 3 or 0 (for no scrolling)
    // However, if sScrollLines > WHEEL_DELTA, we assume that
    // the mouse driver wants a page scroll.  The docs state that
    // sScrollLines should explicitly equal WHEEL_PAGESCROLL, but
    // since some mouse drivers use an arbitrary large number instead,
    // we have to handle that as well.
    mScrollLines = WHEEL_PAGESCROLL;
  }

  return oldValue != mScrollLines;
}

bool MouseScrollHandler::SystemSettings::InitScrollChars() {
  int32_t oldValue = mInitialized ? mScrollChars : 0;
  mIsReliableScrollChars = false;
  mScrollChars = MouseScrollHandler::sInstance->mUserPrefs
                     .GetOverriddenHorizontalScrollAmout();
  if (mScrollChars >= 0) {
    // overridden by the pref.
    mIsReliableScrollChars = true;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::SystemSettings::InitScrollChars(): mScrollChars is "
             "overridden by the pref: %d",
             mScrollChars));
  } else if (!::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &mScrollChars,
                                     0)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::SystemSettings::InitScrollChars(): "
             "::SystemParametersInfo("
             "SPI_GETWHEELSCROLLCHARS) failed, this is unexpected on Vista or "
             "later"));
    // XXX Should we use DefaultScrollChars()?
    mScrollChars = 1;
  }

  if (mScrollChars > WHEEL_DELTA) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::SystemSettings::InitScrollChars(): the result of "
             "::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS) is too large: %d",
             mScrollChars));
    // See the comments for the case mScrollLines > WHEEL_DELTA.
    mScrollChars = WHEEL_PAGESCROLL;
  }

  return oldValue != mScrollChars;
}

void MouseScrollHandler::SystemSettings::MarkDirty() {
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScrollHandler::SystemSettings::MarkDirty(): "
           "Marking SystemSettings dirty"));
  mInitialized = false;
  // When system settings are changed, we should reset current transaction.
  MOZ_ASSERT(sInstance,
             "Must not be called at initializing MouseScrollHandler");
  MouseScrollHandler::sInstance->mLastEventInfo.ResetTransaction();
}

void MouseScrollHandler::SystemSettings::RefreshCache() {
  bool isChanged = InitScrollLines();
  isChanged = InitScrollChars() || isChanged;
  if (!isChanged) {
    return;
  }
  // If the scroll amount is changed, we should reset current transaction.
  MOZ_ASSERT(sInstance,
             "Must not be called at initializing MouseScrollHandler");
  MouseScrollHandler::sInstance->mLastEventInfo.ResetTransaction();
}

void MouseScrollHandler::SystemSettings::TrustedScrollSettingsDriver() {
  if (!mInitialized) {
    return;
  }

  // if the cache is initialized with prefs, we don't need to refresh it.
  if (mIsReliableScrollLines && mIsReliableScrollChars) {
    return;
  }

  MouseScrollHandler::UserPrefs& userPrefs =
      MouseScrollHandler::sInstance->mUserPrefs;

  // If system settings cache is disabled, we should always refresh them.
  if (!userPrefs.IsSystemSettingCacheEnabled()) {
    RefreshCache();
    return;
  }

  // If pref is set to as "always trust the cache", we shouldn't refresh them
  // in any environments.
  if (userPrefs.IsSystemSettingCacheForciblyEnabled()) {
    return;
  }

  // If SynTP of Synaptics or Apoint of Alps is installed, it may hook
  // ::SystemParametersInfo() and returns different value from system settings.
  if (Device::SynTP::IsDriverInstalled() ||
      Device::Apoint::IsDriverInstalled()) {
    RefreshCache();
    return;
  }

  // XXX We're not sure about other touchpad drivers...
}

/******************************************************************************
 *
 * UserPrefs
 *
 ******************************************************************************/

MouseScrollHandler::UserPrefs::UserPrefs() : mInitialized(false) {
  // We need to reset mouse wheel transaction when all of mousewheel related
  // prefs are changed.
  DebugOnly<nsresult> rv =
      Preferences::RegisterPrefixCallback(OnChange, "mousewheel.", this);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to register callback for mousewheel.");
}

MouseScrollHandler::UserPrefs::~UserPrefs() {
  DebugOnly<nsresult> rv =
      Preferences::UnregisterPrefixCallback(OnChange, "mousewheel.", this);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to unregister callback for mousewheel.");
}

void MouseScrollHandler::UserPrefs::Init() {
  if (mInitialized) {
    return;
  }

  mInitialized = true;

  mScrollMessageHandledAsWheelMessage =
      Preferences::GetBool("mousewheel.emulate_at_wm_scroll", false);
  mEnableSystemSettingCache =
      Preferences::GetBool("mousewheel.system_settings_cache.enabled", true);
  mForceEnableSystemSettingCache = Preferences::GetBool(
      "mousewheel.system_settings_cache.force_enabled", false);
  mEmulateToMakeWindowUnderCursorForeground = Preferences::GetBool(
      "mousewheel.debug.make_window_under_cursor_foreground", false);
  mOverriddenVerticalScrollAmount =
      Preferences::GetInt("mousewheel.windows.vertical_amount_override", -1);
  mOverriddenHorizontalScrollAmount =
      Preferences::GetInt("mousewheel.windows.horizontal_amount_override", -1);
  mMouseScrollTransactionTimeout = Preferences::GetInt(
      "mousewheel.windows.transaction.timeout", DEFAULT_TIMEOUT_DURATION);

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::UserPrefs::Init(): initialized, "
           "mScrollMessageHandledAsWheelMessage=%s, "
           "mEnableSystemSettingCache=%s, "
           "mForceEnableSystemSettingCache=%s, "
           "mEmulateToMakeWindowUnderCursorForeground=%s, "
           "mOverriddenVerticalScrollAmount=%d, "
           "mOverriddenHorizontalScrollAmount=%d, "
           "mMouseScrollTransactionTimeout=%d",
           GetBoolName(mScrollMessageHandledAsWheelMessage),
           GetBoolName(mEnableSystemSettingCache),
           GetBoolName(mForceEnableSystemSettingCache),
           GetBoolName(mEmulateToMakeWindowUnderCursorForeground),
           mOverriddenVerticalScrollAmount, mOverriddenHorizontalScrollAmount,
           mMouseScrollTransactionTimeout));
}

void MouseScrollHandler::UserPrefs::MarkDirty() {
  MOZ_LOG(
      gMouseScrollLog, LogLevel::Info,
      ("MouseScrollHandler::UserPrefs::MarkDirty(): Marking UserPrefs dirty"));
  mInitialized = false;
  // Some prefs might override system settings, so, we should mark them dirty.
  MouseScrollHandler::sInstance->mSystemSettings.MarkDirty();
  // When user prefs for mousewheel are changed, we should reset current
  // transaction.
  MOZ_ASSERT(sInstance,
             "Must not be called at initializing MouseScrollHandler");
  MouseScrollHandler::sInstance->mLastEventInfo.ResetTransaction();
}

/******************************************************************************
 *
 * Device
 *
 ******************************************************************************/

/* static */
bool MouseScrollHandler::Device::GetWorkaroundPref(const char* aPrefName,
                                                   bool aValueIfAutomatic) {
  if (!aPrefName) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::GetWorkaroundPref(): Failed, aPrefName is "
             "NULL"));
    return aValueIfAutomatic;
  }

  int32_t lHackValue = 0;
  if (NS_FAILED(Preferences::GetInt(aPrefName, &lHackValue))) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::GetWorkaroundPref(): Preferences::GetInt() "
             "failed,"
             " aPrefName=\"%s\", aValueIfAutomatic=%s",
             aPrefName, GetBoolName(aValueIfAutomatic)));
    return aValueIfAutomatic;
  }

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::Device::GetWorkaroundPref(): Succeeded, "
           "aPrefName=\"%s\", aValueIfAutomatic=%s, lHackValue=%d",
           aPrefName, GetBoolName(aValueIfAutomatic), lHackValue));

  switch (lHackValue) {
    case 0:  // disabled
      return false;
    case 1:  // enabled
      return true;
    default:  // -1: autodetect
      return aValueIfAutomatic;
  }
}

/* static */
void MouseScrollHandler::Device::Init() {
  // FYI: Thinkpad's TrackPoint is Apoint of Alps and UltraNav is SynTP of
  //      Synaptics.  So, those drivers' information should be initialized
  //      before calling methods of TrackPoint and UltraNav.
  SynTP::Init();
  Elantech::Init();
  Apoint::Init();

  sFakeScrollableWindowNeeded = GetWorkaroundPref(
      "ui.trackpoint_hack.enabled", (TrackPoint::IsDriverInstalled() ||
                                     UltraNav::IsObsoleteDriverInstalled()));

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::Device::Init(): sFakeScrollableWindowNeeded=%s",
           GetBoolName(sFakeScrollableWindowNeeded)));
}

/******************************************************************************
 *
 * Device::SynTP
 *
 ******************************************************************************/

/* static */
void MouseScrollHandler::Device::SynTP::Init() {
  if (sInitialized) {
    return;
  }

  sInitialized = true;
  sMajorVersion = 0;
  sMinorVersion = -1;

  wchar_t buf[40];
  if (!WinRegistry::GetString(
          HKEY_LOCAL_MACHINE, u"Software\\Synaptics\\SynTP\\Install"_ns,
          u"DriverVersion"_ns, buf, WinRegistry::kLegacyWinUtilsStringFlags)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::SynTP::Init(): "
             "SynTP driver is not found"));
    return;
  }

  sMajorVersion = wcstol(buf, nullptr, 10);
  sMinorVersion = 0;
  wchar_t* p = wcschr(buf, L'.');
  if (p) {
    sMinorVersion = wcstol(p + 1, nullptr, 10);
  }
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::Device::SynTP::Init(): "
           "found driver version = %d.%d",
           sMajorVersion, sMinorVersion));
}

/******************************************************************************
 *
 * Device::Elantech
 *
 ******************************************************************************/

/* static */
void MouseScrollHandler::Device::Elantech::Init() {
  int32_t version = GetDriverMajorVersion();
  bool needsHack = Device::GetWorkaroundPref(
      "ui.elantech_gesture_hacks.enabled", version != 0);
  sUseSwipeHack = needsHack && version <= 7;
  sUsePinchHack = needsHack && version <= 8;

  MOZ_LOG(
      gMouseScrollLog, LogLevel::Info,
      ("MouseScroll::Device::Elantech::Init(): version=%d, sUseSwipeHack=%s, "
       "sUsePinchHack=%s",
       version, GetBoolName(sUseSwipeHack), GetBoolName(sUsePinchHack)));
}

/* static */
int32_t MouseScrollHandler::Device::Elantech::GetDriverMajorVersion() {
  wchar_t buf[40];
  // The driver version is found in one of these two registry keys.
  if (!WinRegistry::GetString(
          HKEY_CURRENT_USER, u"Software\\Elantech\\MainOption"_ns,
          u"DriverVersion"_ns, buf, WinRegistry::kLegacyWinUtilsStringFlags) &&
      !WinRegistry::GetString(HKEY_CURRENT_USER, u"Software\\Elantech"_ns,
                              u"DriverVersion"_ns, buf,
                              WinRegistry::kLegacyWinUtilsStringFlags)) {
    return 0;
  }

  // Assume that the major version number can be found just after a space
  // or at the start of the string.
  for (wchar_t* p = buf; *p; p++) {
    if (*p >= L'0' && *p <= L'9' && (p == buf || *(p - 1) == L' ')) {
      return wcstol(p, nullptr, 10);
    }
  }

  return 0;
}

/* static */
bool MouseScrollHandler::Device::Elantech::IsHelperWindow(HWND aWnd) {
  // The helper window cannot be distinguished based on its window class, so we
  // need to check if it is owned by the helper process, ETDCtrl.exe.

  const wchar_t* filenameSuffix = L"\\etdctrl.exe";
  const int filenameSuffixLength = 12;

  DWORD pid;
  ::GetWindowThreadProcessId(aWnd, &pid);

  HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!hProcess) {
    return false;
  }

  bool result = false;
  wchar_t path[256] = {L'\0'};
  if (::GetProcessImageFileNameW(hProcess, path, std::size(path))) {
    int pathLength = lstrlenW(path);
    if (pathLength >= filenameSuffixLength) {
      if (lstrcmpiW(path + pathLength - filenameSuffixLength, filenameSuffix) ==
          0) {
        result = true;
      }
    }
  }
  ::CloseHandle(hProcess);

  return result;
}

/* static */
bool MouseScrollHandler::Device::Elantech::HandleKeyMessage(nsWindow* aWidget,
                                                            UINT aMsg,
                                                            WPARAM aWParam,
                                                            LPARAM aLParam) {
  // The Elantech touchpad driver understands three-finger swipe left and
  // right gestures, and translates them into Page Up and Page Down key
  // events for most applications.  For Firefox 3.6, it instead sends
  // Alt+Left and Alt+Right to trigger browser back/forward actions.  As
  // with the Thinkpad Driver hack in nsWindow::Create, the change in
  // HWND structure makes Firefox not trigger the driver's heuristics
  // any longer.
  //
  // The Elantech driver actually sends these messages for a three-finger
  // swipe right:
  //
  //   WM_KEYDOWN virtual_key = 0xCC or 0xFF ScanCode = 00
  //   WM_KEYDOWN virtual_key = VK_NEXT      ScanCode = 00
  //   WM_KEYUP   virtual_key = VK_NEXT      ScanCode = 00
  //   WM_KEYUP   virtual_key = 0xCC or 0xFF ScanCode = 00
  //
  // Whether 0xCC or 0xFF is sent is suspected to depend on the driver
  // version.  7.0.4.12_14Jul09_WHQL, 7.0.5.10, and 7.0.6.0 generate 0xCC.
  // 7.0.4.3 from Asus on EeePC generates 0xFF.
  //
  // On some hardware, IS_VK_DOWN(0xFF) returns true even when Elantech
  // messages are not involved, meaning that alone is not enough to
  // distinguish the gesture from a regular Page Up or Page Down key press.
  // The ScanCode is therefore also tested to detect the gesture.
  // We then pretend that we should dispatch "Go Forward" command.  Similarly
  // for VK_PRIOR and "Go Back" command.
  if (sUseSwipeHack && (aWParam == VK_NEXT || aWParam == VK_PRIOR) &&
      WinUtils::GetScanCode(aLParam) == 0 &&
      (IS_VK_DOWN(0xFF) || IS_VK_DOWN(0xCC))) {
    if (aMsg == WM_KEYDOWN) {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
              ("MouseScroll::Device::Elantech::HandleKeyMessage(): Dispatching "
               "%s command event",
               aWParam == VK_NEXT ? "Forward" : "Back"));

      WidgetCommandEvent appCommandEvent(
          true, (aWParam == VK_NEXT) ? nsGkAtoms::Forward : nsGkAtoms::Back,
          aWidget);

      // In this scenario, the coordinate of the event isn't supplied, so pass
      // nullptr as an argument to indicate using the coordinate from the last
      // available window message.
      InitEvent(aWidget, appCommandEvent, nullptr);
      aWidget->DispatchWindowEvent(appCommandEvent);
    } else {
      MOZ_LOG(gMouseScrollLog, LogLevel::Info,
              ("MouseScroll::Device::Elantech::HandleKeyMessage(): Consumed"));
    }
    return true;  // consume the message (doesn't need to dispatch key events)
  }

  // Version 8 of the Elantech touchpad driver sends these messages for
  // zoom gestures:
  //
  //   WM_KEYDOWN    virtual_key = 0xCC        time = 10
  //   WM_KEYDOWN    virtual_key = VK_CONTROL  time = 10
  //   WM_MOUSEWHEEL                           time = ::GetTickCount()
  //   WM_KEYUP      virtual_key = VK_CONTROL  time = 10
  //   WM_KEYUP      virtual_key = 0xCC        time = 10
  //
  // The result of this is that we process all of the WM_KEYDOWN/WM_KEYUP
  // messages first because their timestamps make them appear to have
  // been sent before the WM_MOUSEWHEEL message.  To work around this,
  // we store the current time when we process the WM_KEYUP message and
  // assume that any WM_MOUSEWHEEL message with a timestamp before that
  // time is one that should be processed as if the Control key was down.
  if (sUsePinchHack && aMsg == WM_KEYUP && aWParam == VK_CONTROL &&
      ::GetMessageTime() == 10) {
    // We look only at the bottom 31 bits of the system tick count since
    // GetMessageTime returns a LONG, which is signed, so we want values
    // that are more easily comparable.
    sZoomUntil = ::GetTickCount() & 0x7FFFFFFF;

    MOZ_LOG(
        gMouseScrollLog, LogLevel::Info,
        ("MouseScroll::Device::Elantech::HandleKeyMessage(): sZoomUntil=%lu",
         sZoomUntil));
  }

  return false;
}

/* static */
void MouseScrollHandler::Device::Elantech::UpdateZoomUntil() {
  if (!sZoomUntil) {
    return;
  }

  // For the Elantech Touchpad Zoom Gesture Hack, we should check that the
  // system time (32-bit milliseconds) hasn't wrapped around.  Otherwise we
  // might get into the situation where wheel events for the next 50 days of
  // system uptime are assumed to be Ctrl+Wheel events.  (It is unlikely that
  // we would get into that state, because the system would already need to be
  // up for 50 days and the Control key message would need to be processed just
  // before the system time overflow and the wheel message just after.)
  //
  // We also take the chance to reset sZoomUntil if we simply have passed that
  // time.
  LONG msgTime = ::GetMessageTime();
  if ((sZoomUntil >= 0x3fffffffu && DWORD(msgTime) < 0x40000000u) ||
      (sZoomUntil < DWORD(msgTime))) {
    sZoomUntil = 0;

    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::Elantech::UpdateZoomUntil(): "
             "sZoomUntil was reset"));
  }
}

/* static */
bool MouseScrollHandler::Device::Elantech::IsZooming() {
  // Assume the Control key is down if the Elantech touchpad has sent the
  // mis-ordered WM_KEYDOWN/WM_MOUSEWHEEL messages.  (See the comment in
  // OnKeyUp.)
  return (sZoomUntil && static_cast<DWORD>(::GetMessageTime()) < sZoomUntil);
}

/******************************************************************************
 *
 * Device::Apoint
 *
 ******************************************************************************/

/* static */
void MouseScrollHandler::Device::Apoint::Init() {
  if (sInitialized) {
    return;
  }

  sInitialized = true;
  sMajorVersion = 0;
  sMinorVersion = -1;

  wchar_t buf[40];
  if (!WinRegistry::GetString(HKEY_LOCAL_MACHINE, u"Software\\Alps\\Apoint"_ns,
                              u"ProductVer"_ns, buf,
                              WinRegistry::kLegacyWinUtilsStringFlags)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::Apoint::Init(): "
             "Apoint driver is not found"));
    return;
  }

  sMajorVersion = wcstol(buf, nullptr, 10);
  sMinorVersion = 0;
  wchar_t* p = wcschr(buf, L'.');
  if (p) {
    sMinorVersion = wcstol(p + 1, nullptr, 10);
  }
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScroll::Device::Apoint::Init(): "
           "found driver version = %d.%d",
           sMajorVersion, sMinorVersion));
}

/******************************************************************************
 *
 * Device::TrackPoint
 *
 ******************************************************************************/

/* static */
bool MouseScrollHandler::Device::TrackPoint::IsDriverInstalled() {
  if (WinRegistry::HasKey(HKEY_CURRENT_USER,
                          u"Software\\Lenovo\\TrackPoint"_ns)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::TrackPoint::IsDriverInstalled(): "
             "Lenovo's TrackPoint driver is found"));
    return true;
  }

  if (WinRegistry::HasKey(HKEY_CURRENT_USER,
                          u"Software\\Alps\\Apoint\\TrackPoint"_ns)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::TrackPoint::IsDriverInstalled(): "
             "Alps's TrackPoint driver is found"));
    return true;
  }

  return false;
}

/******************************************************************************
 *
 * Device::UltraNav
 *
 ******************************************************************************/

/* static */
bool MouseScrollHandler::Device::UltraNav::IsObsoleteDriverInstalled() {
  if (WinRegistry::HasKey(HKEY_CURRENT_USER,
                          u"Software\\Lenovo\\UltraNav"_ns)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
             "Lenovo's UltraNav driver is found"));
    return true;
  }

  bool installed = false;
  if (WinRegistry::HasKey(HKEY_CURRENT_USER,
                          u"Software\\Synaptics\\SynTPEnh\\UltraNavUSB"_ns)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
             "Synaptics's UltraNav (USB) driver is found"));
    installed = true;
  } else if (WinRegistry::HasKey(
                 HKEY_CURRENT_USER,
                 u"Software\\Synaptics\\SynTPEnh\\UltraNavPS2"_ns)) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
             "Synaptics's UltraNav (PS/2) driver is found"));
    installed = true;
  }

  if (!installed) {
    return false;
  }

  int32_t majorVersion = Device::SynTP::GetDriverMajorVersion();
  if (!majorVersion) {
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::UltraNav::IsObsoleteDriverInstalled(): "
             "Failed to get UltraNav driver version"));
    return false;
  }
  int32_t minorVersion = Device::SynTP::GetDriverMinorVersion();
  return majorVersion < 15 || (majorVersion == 15 && minorVersion == 0);
}

/******************************************************************************
 *
 * Device::SetPoint
 *
 ******************************************************************************/

/* static */
bool MouseScrollHandler::Device::SetPoint::IsGetMessagePosResponseValid(
    UINT aMessage, WPARAM aWParam, LPARAM aLParam) {
  if (aMessage != WM_MOUSEHWHEEL) {
    return false;
  }

  POINTS pts = MouseScrollHandler::GetCurrentMessagePos();
  LPARAM messagePos = MAKELPARAM(pts.x, pts.y);

  // XXX We should check whether SetPoint is installed or not by registry.

  // SetPoint, Logitech (Logicool) mouse driver, (confirmed with 4.82.11 and
  // MX-1100) always sets 0 to the lParam of WM_MOUSEHWHEEL.  The driver SENDs
  // one message at first time, this time, ::GetMessagePos() works fine.
  // Then, we will return 0 (0 means we process it) to the message. Then, the
  // driver will POST the same messages continuously during the wheel tilted.
  // But ::GetMessagePos() API always returns (0, 0) for them, even if the
  // actual mouse cursor isn't 0,0.  Therefore, we cannot trust the result of
  // ::GetMessagePos API if the sender is SetPoint.
  if (!sMightBeUsing && !aLParam && aLParam != messagePos &&
      ::InSendMessage()) {
    sMightBeUsing = true;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::SetPoint::IsGetMessagePosResponseValid(): "
             "Might using SetPoint"));
  } else if (sMightBeUsing && aLParam != 0 && ::InSendMessage()) {
    // The user has changed the mouse from Logitech's to another one (e.g.,
    // the user has changed to the touchpad of the notebook.
    sMightBeUsing = false;
    MOZ_LOG(gMouseScrollLog, LogLevel::Info,
            ("MouseScroll::Device::SetPoint::IsGetMessagePosResponseValid(): "
             "Might stop using SetPoint"));
  }
  return (sMightBeUsing && !aLParam && !messagePos);
}

/******************************************************************************
 *
 * SynthesizingEvent: implementation
 *
 ******************************************************************************/

/* static */
MouseScrollHandler::SynthesizingEvent*
MouseScrollHandler::SynthesizingEvent::GetActiveInstance() {
  if (auto* outer = MouseScrollHandler::sInstance) {
    if (auto* self = outer->mSynthesizingEvent.get()) {
      if (self->mStatus != NOT_SYNTHESIZING) {
        return self;
      }
    }
  }
  return nullptr;
}

nsresult MouseScrollHandler::SynthesizingEvent::Synthesize(
    const POINTS& aCursorPoint, HWND aWnd, UINT aMessage, WPARAM aWParam,
    LPARAM aLParam, const BYTE (&aKeyStates)[256]) {
  MOZ_LOG(
      gMouseScrollLog, LogLevel::Info,
      ("MouseScrollHandler::SynthesizingEvent::Synthesize(): aCursorPoint: { "
       "x: %d, y: %d }, aWnd=0x%p, aMessage=0x%04X, aWParam=0x%08zX, "
       "aLParam=0x%08" PRIXLPTR ", synthesizing=%s, mStatus=%s",
       aCursorPoint.x, aCursorPoint.y, aWnd, aMessage, aWParam, aLParam,
       GetBoolName(!!GetActiveInstance()), GetStatusName()));

  if (mStatus != NOT_SYNTHESIZING) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ::GetKeyboardState(mOriginalKeyState);

  // Note that we cannot use ::SetCursorPos() because it works asynchronously.
  // We should SEND the message for reducing the possibility of receiving
  // unexpected message which were not sent from here.
  mCursorPoint = aCursorPoint;

  memcpy(mKeyState, aKeyStates, sizeof(mKeyState));
  ::SetKeyboardState(mKeyState);

  mStatus = SENDING_MESSAGE;
  mWnd = aWnd;
  mMessage = aMessage;
  mWParam = aWParam;
  mLParam = aLParam;

  // Don't assume that aWnd is always managed by nsWindow.  It might be
  // a plugin window.
  ::SendMessage(aWnd, aMessage, aWParam, aLParam);

  return NS_OK;
}

void MouseScrollHandler::SynthesizingEvent::NotifyMessageReceived(
    nsWindow* aWindow, UINT msg, WPARAM wParam, LPARAM lParam) {
  MOZ_ASSERT(mStatus != NOT_SYNTHESIZING);

  // check that the received message is as expected
  HWND handle = aWindow ? aWindow->GetWindowHandle() : nullptr;
  nsWindow* widget [[maybe_unused]] = WinUtils::GetNSWindowPtr(mWnd);

  if (mStatus == SENDING_MESSAGE && aWindow == widget && mWnd == handle &&
      mMessage == msg && mWParam == wParam && mLParam == lParam) {
    // all is well; do nothing
    MOZ_LOG(
        gMouseScrollLog, LogLevel::Debug,
        ("MouseScrollHandler::SynthesizingEvent::NotifyMessageReceived(): OK"));
    return;
  }

  // log values: [{received} vs. {expected}]
  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScrollHandler::SynthesizingEvent::NotifyMessageReceived(): "
           "handle=[0x%08zu vs. 0x%08zu], widget=[%p vs. %p], "
           "msg=[0x%04X vs. 0x%04X], wParam=[0x%08zX vs. 0x%08zX], "
           "lParam=[0x%08" PRIXLPTR "vs. 0x%08" PRIXLPTR "], mStatus=%s",
           size_t(handle), size_t(mWnd), aWindow, widget, msg, mMessage, wParam,
           mWParam, lParam, mLParam, GetStatusName()));

  // We probably shouldn't get here in normal operation, but we do during
  // testing. (See failures on bug 1945257.) Fall through without further
  // action.
}

void MouseScrollHandler::SynthesizingEvent::NotifyMessageHandlingFinished() {
  MOZ_ASSERT(mStatus != NOT_SYNTHESIZING);

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScrollHandler::SynthesizingEvent::"
           "NotifyInternalMessageHandlingFinished()"));

  Finish();
}

void MouseScrollHandler::SynthesizingEvent::Finish() {
  MOZ_ASSERT(mStatus != NOT_SYNTHESIZING);

  MOZ_LOG(gMouseScrollLog, LogLevel::Info,
          ("MouseScrollHandler::SynthesizingEvent::Finish()"));

  // Restore the original key state.
  ::SetKeyboardState(mOriginalKeyState);

  mStatus = NOT_SYNTHESIZING;
  mWnd = nullptr;
  mMessage = 0;
  mWParam = 0;
  mLParam = 0;
}

}  // namespace widget
}  // namespace mozilla

// Restore access to GetMessagePos for unified builds.
#undef GetMessagePos
