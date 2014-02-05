/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nscore.h"
#include "nsdefs.h"
#include "prlog.h"
#include "nsAutoPtr.h"
#include "nsBaseWidget.h"
#include "nsWindowBase.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWindowDbg.h"
#include "WindowHook.h"
#include "TaskbarWindowPreview.h"
#include "nsIdleService.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/Accessible.h"
#endif
#include "mozilla/EventForwards.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "nsDeque.h"
#include "APZController.h"

#include "mozwrlbase.h"

#include <windows.system.h>
#include <windows.ui.core.h>
#include <Windows.ApplicationModel.core.h>
#include <Windows.ApplicationModel.h>
#include <Windows.Applicationmodel.Activation.h>

class nsNativeDragTarget;

namespace mozilla {
namespace widget {
namespace winrt {

class FrameworkView;

} } }

class DispatchMsg;

class MetroWidget : public nsWindowBase,
                    public nsIObserver
{
  typedef uint32_t TouchBehaviorFlags;

  typedef mozilla::widget::WindowHook WindowHook;
  typedef mozilla::widget::TaskbarWindowPreview TaskbarWindowPreview;
  typedef ABI::Windows::UI::Input::IPointerPoint IPointerPoint;
  typedef ABI::Windows::UI::Core::IPointerEventArgs IPointerEventArgs;
  typedef ABI::Windows::UI::Core::IKeyEventArgs IKeyEventArgs;
  typedef ABI::Windows::UI::Core::ICharacterReceivedEventArgs ICharacterReceivedEventArgs;
  typedef mozilla::widget::winrt::FrameworkView FrameworkView;
  typedef mozilla::widget::winrt::APZController APZController;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

  static LRESULT CALLBACK
  StaticWindowProcedure(HWND aWnd, UINT aMsg, WPARAM aWParan, LPARAM aLParam);
  LRESULT WindowProcedure(HWND aWnd, UINT aMsg, WPARAM aWParan, LPARAM aLParam);

public:
  MetroWidget();
  virtual ~MetroWidget();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  static HWND GetICoreWindowHWND() { return sICoreHwnd; }

  // nsWindowBase
  virtual bool DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent) MOZ_OVERRIDE;
  virtual bool DispatchKeyboardEvent(mozilla::WidgetGUIEvent* aEvent) MOZ_OVERRIDE;
  virtual bool DispatchScrollEvent(mozilla::WidgetGUIEvent* aEvent) MOZ_OVERRIDE;
  virtual bool DispatchPluginEvent(const MSG &aMsg) MOZ_OVERRIDE { return false; }
  virtual bool IsTopLevelWidget() MOZ_OVERRIDE { return true; }
  virtual nsWindowBase* GetParentWindowBase(bool aIncludeOwner) MOZ_OVERRIDE { return nullptr; }
  // InitEvent assumes physical coordinates and is used by shared win32 code. Do
  // not hand winrt event coordinates to this routine.
  virtual void InitEvent(mozilla::WidgetGUIEvent& aEvent,
                         nsIntPoint* aPoint = nullptr) MOZ_OVERRIDE;

  // nsBaseWidget
  virtual CompositorParent* NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight);
  virtual void SetWidgetListener(nsIWidgetListener* aWidgetListener);

  // nsIWidget interface
  NS_IMETHOD    Create(nsIWidget *aParent,
                       nsNativeWidget aNativeParent,
                       const nsIntRect &aRect,
                       nsDeviceContext *aContext,
                       nsWidgetInitData *aInitData = nullptr);
  NS_IMETHOD    Destroy();
  NS_IMETHOD    EnableDragDrop(bool aEnable);
  NS_IMETHOD    SetParent(nsIWidget *aNewParent);
  NS_IMETHOD    Show(bool bState);
  NS_IMETHOD    IsVisible(bool & aState);
  NS_IMETHOD    IsEnabled(bool *aState);
  NS_IMETHOD    GetBounds(nsIntRect &aRect);
  NS_IMETHOD    GetScreenBounds(nsIntRect &aRect);
  NS_IMETHOD    GetClientBounds(nsIntRect &aRect);
  NS_IMETHOD    Invalidate(bool aEraseBackground = false,
                bool aUpdateNCArea = false,
                bool aIncludeChildren = false);
  NS_IMETHOD    Invalidate(const nsIntRect & aRect);
  NS_IMETHOD    DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                              nsEventStatus& aStatus);
  NS_IMETHOD    ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY);
  NS_IMETHOD    Move(double aX, double aY);
  NS_IMETHOD    Resize(double aWidth, double aHeight, bool aRepaint);
  NS_IMETHOD    Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint);
  NS_IMETHOD    SetFocus(bool aRaise);
  NS_IMETHOD    Enable(bool aState);
  NS_IMETHOD    SetCursor(nsCursor aCursor);
  NS_IMETHOD    SetTitle(const nsAString& aTitle);
  NS_IMETHOD    CaptureRollupEvents(nsIRollupListener * aListener,
                                    bool aDoCapture);
  NS_IMETHOD    ReparentNativeWidget(nsIWidget* aNewParent);
  virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                            int32_t aNativeKeyCode,
                                            uint32_t aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters);
  virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags);
  virtual nsresult SynthesizeNativeMouseScrollEvent(nsIntPoint aPoint,
                                                    uint32_t aNativeMessage,
                                                    double aDeltaX,
                                                    double aDeltaY,
                                                    double aDeltaZ,
                                                    uint32_t aModifierFlags,
                                                    uint32_t aAdditionalFlags);
  virtual bool  HasPendingInputEvent();
  virtual double GetDefaultScaleInternal();
  float         GetDPI();
  mozilla::LayoutDeviceIntPoint CSSIntPointToLayoutDeviceIntPoint(const mozilla::CSSIntPoint &aCSSPoint);
  void          ChangedDPI();
  virtual uint32_t GetMaxTouchPoints() const MOZ_OVERRIDE;
  virtual bool  IsVisible() const;
  virtual bool  IsEnabled() const;
  // ShouldUseOffMainThreadCompositing is defined in base widget
  virtual bool  ShouldUseOffMainThreadCompositing();
  bool          ShouldUseMainThreadD3D10Manager();
  bool          ShouldUseBasicManager();
  bool          ShouldUseAPZC();
  virtual LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                        bool* aAllowRetaining = nullptr);
  virtual void GetPreferredCompositorBackends(nsTArray<mozilla::layers::LayersBackend>& aHints) { aHints.AppendElement(mozilla::layers::LayersBackend::LAYERS_D3D11); }

  // IME related interfaces
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction);
  NS_IMETHOD_(nsIWidget::InputContext) GetInputContext();
  NS_IMETHOD    NotifyIME(NotificationToIME aNotification) MOZ_OVERRIDE;
  NS_IMETHOD    GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState);
  NS_IMETHOD    NotifyIMEOfTextChange(uint32_t aStart,
                                      uint32_t aOldEnd,
                                      uint32_t aNewEnd) MOZ_OVERRIDE;
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() MOZ_OVERRIDE;

  // FrameworkView helpers
  void SizeModeChanged();
  void Activated(bool aActiveated);
  void Paint(const nsIntRegion& aInvalidRegion);

  MetroWidget* MetroWidget::GetTopLevelWindow(bool aStopOnDialogOrPopup) { return this; }
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  virtual void* GetNativeData(uint32_t aDataType);
  virtual void  FreeNativeData(void * data, uint32_t aDataType);
  virtual nsIntPoint WidgetToScreenOffset();

  already_AddRefed<nsIPresShell> GetPresShell();

  void UserActivity();

#ifdef ACCESSIBILITY
  mozilla::a11y::Accessible* DispatchAccessibleEvent(uint32_t aEventType);
  mozilla::a11y::Accessible* GetAccessible();
#endif // ACCESSIBILITY

  // needed for current nsIFilePicker
  void PickerOpen();
  void PickerClosed();
  bool DestroyCalled() { return false; }
  void SuppressBlurEvents(bool aSuppress);
  bool BlurEventsSuppressed();

  // needed for nsITaskbarWindowPreview
  bool HasTaskbarIconBeenCreated() { return false; }
  void SetHasTaskbarIconBeenCreated(bool created = true) { }
  already_AddRefed<nsITaskbarWindowPreview> GetTaskbarPreview() { return nullptr; }
  void SetTaskbarPreview(nsITaskbarWindowPreview *preview) { }
  WindowHook& GetWindowHook() { return mWindowHook; }

  void SetView(FrameworkView* aView);
  void FindMetroWindow();
  virtual void SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();

  TouchBehaviorFlags ContentGetAllowedTouchBehavior(const nsIntPoint& aPoint);

  // apzc controller related api
  void ApzcGetAllowedTouchBehavior(mozilla::WidgetInputEvent* aTransformedEvent, nsTArray<TouchBehaviorFlags>& aOutBehaviors);
  void ApzcSetAllowedTouchBehavior(const ScrollableLayerGuid& aGuid, nsTArray<TouchBehaviorFlags>& aBehaviors);

  // Hit test a point to see if an apzc would consume input there
  bool ApzHitTest(mozilla::ScreenIntPoint& pt);
  // Transforms a coord so that it properly targets gecko content based
  // on apzc transforms currently applied.
  void ApzTransformGeckoCoordinate(const mozilla::ScreenIntPoint& pt,
                                   mozilla::LayoutDeviceIntPoint* aRefPointOut);
  // send ContentRecievedTouch calls to the apz with appropriate preventDefault params
  void ApzContentConsumingTouch(const ScrollableLayerGuid& aGuid);
  void ApzContentIgnoringTouch(const ScrollableLayerGuid& aGuid);
  // Input handling
  nsEventStatus ApzReceiveInputEvent(mozilla::WidgetInputEvent* aEvent,
                                     ScrollableLayerGuid* aOutTargetGuid);
  nsEventStatus ApzReceiveInputEvent(mozilla::WidgetInputEvent* aInEvent,
                                     ScrollableLayerGuid* aOutTargetGuid,
                                     mozilla::WidgetInputEvent* aOutEvent);

protected:
  friend class FrameworkView;

  struct OleInitializeWrapper {
    HRESULT const hr;

    OleInitializeWrapper()
      : hr(::OleInitialize(nullptr))
    {
    }

    ~OleInitializeWrapper() {
      if (SUCCEEDED(hr)) {
        ::OleFlushClipboard();
        ::OleUninitialize();
      }
    }
  };

  void SetSubclass();
  void RemoveSubclass();
  nsIWidgetListener* GetPaintListener();

  // Async event dispatching
  void DispatchAsyncScrollEvent(DispatchMsg* aEvent);
  void DeliverNextScrollEvent();
  void DeliverNextKeyboardEvent();

protected:
  OleInitializeWrapper mOleInitializeWrapper;
  WindowHook mWindowHook;
  Microsoft::WRL::ComPtr<FrameworkView> mView;
  nsTransparencyMode mTransparencyMode;
  nsIntRegion mInvalidatedRegion;
  nsCOMPtr<nsIdleService> mIdleService;
  HWND mWnd;
  static HWND sICoreHwnd;
  WNDPROC mMetroWndProc;
  bool mTempBasicLayerInUse;
  uint64_t mRootLayerTreeId;
  nsDeque mEventQueue;
  nsDeque mKeyEventQueue;
  nsRefPtr<APZController> mController;
  nsRefPtr<nsNativeDragTarget> mNativeDragTarget;
};
