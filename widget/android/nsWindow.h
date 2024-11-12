/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include "AndroidGraphics.h"
#include "nsBaseWidget.h"
#include "gfxPoint.h"
#include "nsIUserIdleServiceInternal.h"
#include "nsTArray.h"
#include "EventDispatcher.h"
#include "mozilla/EventForwards.h"
#include "mozilla/java/GeckoSessionNatives.h"
#include "mozilla/java/WebResponseWrappers.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextRange.h"
#include "mozilla/UniquePtr.h"

struct ANPEvent;

namespace mozilla {
class WidgetTouchEvent;

namespace layers {
class CompositorBridgeChild;
class LayerManager;
class APZCTreeManager;
class UiCompositorControllerChild;
}  // namespace layers

namespace widget {
class AndroidView;
class GeckoEditableSupport;
class GeckoViewSupport;
class LayerViewSupport;
class NPZCSupport;
class PlatformCompositorWidgetDelegate;
}  // namespace widget

namespace ipc {
class Shmem;
}  // namespace ipc

namespace a11y {
class SessionAccessibility;
}  // namespace a11y
}  // namespace mozilla

class nsWindow final : public nsBaseWidget {
 private:
  virtual ~nsWindow();

 public:
  using nsBaseWidget::GetWindowRenderer;

  nsWindow();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, nsBaseWidget)

  static void InitNatives();
  void OnGeckoViewReady();
  RefPtr<mozilla::MozPromise<bool, bool, false>> OnLoadRequest(
      nsIURI* aUri, int32_t aWindowType, int32_t aFlags,
      nsIPrincipal* aTriggeringPrincipal, bool aHasUserGesture,
      bool aIsTopLevel);

 private:
  nsCOMPtr<nsIUserIdleServiceInternal> mIdleService;
  mozilla::ScreenIntCoord mDynamicToolbarMaxHeight{0};
  mozilla::LayoutDeviceIntMargin mSafeAreaInsets;
  mozilla::widget::PlatformCompositorWidgetDelegate* mCompositorWidgetDelegate =
      nullptr;
  mozilla::Mutex mDestroyMutex{"nsWindow::mDestroyMutex"};

  // Unique ID given to each widget, used to map Surfaces to widgets
  // in the CompositorSurfaceManager.
  int32_t mWidgetId;
  nsSizeMode mSizeMode = nsSizeMode_Normal;
  bool mIsFullScreen = false;
  bool mIsVisible = false;

  RefPtr<mozilla::widget::AndroidView> mAndroidView;

  // Object that implements native LayerView calls.
  // Owned by the Java Compositor instance.
  mozilla::jni::NativeWeakPtr<mozilla::widget::LayerViewSupport>
      mLayerViewSupport;

  // Object that implements native NativePanZoomController calls.
  // Owned by the Java NativePanZoomController instance.
  mozilla::jni::NativeWeakPtr<mozilla::widget::NPZCSupport> mNPZCSupport;

  // Object that implements native GeckoEditable calls.
  // Strong referenced by the Java instance.
  mozilla::jni::NativeWeakPtr<mozilla::widget::GeckoEditableSupport>
      mEditableSupport;
  mozilla::jni::Object::GlobalRef mEditableParent;

  // Object that implements native SessionAccessibility calls.
  // Strong referenced by the Java instance.
  mozilla::jni::NativeWeakPtr<mozilla::a11y::SessionAccessibility>
      mSessionAccessibility;

  // Object that implements native GeckoView calls and associated states.
  // nullptr for nsWindows that were not opened from GeckoView.
  mozilla::jni::NativeWeakPtr<mozilla::widget::GeckoViewSupport>
      mGeckoViewSupport;

  mozilla::Atomic<bool, mozilla::ReleaseAcquire> mContentDocumentDisplayed;

 public:
  static already_AddRefed<nsWindow> From(nsPIDOMWindowOuter* aDOMWindow);
  static already_AddRefed<nsWindow> From(nsIWidget* aWidget);

  static nsWindow* TopWindow();

  static mozilla::Modifiers GetModifiers(int32_t aMetaState);
  static mozilla::TimeStamp GetEventTimeStamp(int64_t aEventTime);

  void InitEvent(mozilla::WidgetGUIEvent& event,
                 LayoutDeviceIntPoint* aPoint = 0);

  void UpdateOverscrollVelocity(const float aX, const float aY);
  void UpdateOverscrollOffset(const float aX, const float aY);

  void HideDynamicToolbar();

  mozilla::widget::EventDispatcher* GetEventDispatcher() const;

  void PassExternalResponse(mozilla::java::WebResponse::Param aResponse);

  void ShowDynamicToolbar();

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void OnDragEvent(
      int32_t aAction, float aX, float aY,
      mozilla::jni::Object::Param aDropData,
      const mozilla::layers::APZEventResult& aApzResult,
      const mozilla::MouseInput& aInput);
  void StartDragAndDrop(mozilla::java::sdk::Bitmap::LocalRef aBitmap);
  void UpdateDragImage(mozilla::java::sdk::Bitmap::LocalRef aBitmap);

  void DetachNatives();

  mozilla::Mutex& GetDestroyMutex() { return mDestroyMutex; }

  //
  // nsIWidget
  //

  using nsBaseWidget::Create;  // for Create signature not overridden here
  [[nodiscard]] nsresult Create(nsIWidget* aParent,
                                const LayoutDeviceIntRect& aRect,
                                InitData* aInitData) override;
  void Destroy() override;
  void DidChangeParent(nsIWidget* aNewParent) override;
  float GetDPI() override;
  double GetDefaultScaleInternal() override;
  void Show(bool aState) override;
  bool IsVisible() const override;
  void ConstrainPosition(DesktopIntPoint&) override;
  void Move(double aX, double aY) override;
  void Resize(double aWidth, double aHeight, bool aRepaint) override;
  void Resize(double aX, double aY, double aWidth, double aHeight,
              bool aRepaint) override;
  nsSizeMode SizeMode() override { return mSizeMode; }
  void SetSizeMode(nsSizeMode aMode) override;
  void Enable(bool aState) override;
  bool IsEnabled() const override;
  void Invalidate(const LayoutDeviceIntRect& aRect) override;
  void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  LayoutDeviceIntRect GetScreenBounds() override;
  LayoutDeviceIntPoint WidgetToScreenOffset() override;
  nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus& aStatus) override;
  nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
  nsresult MakeFullScreen(bool aFullScreen) override;
  void SetCursor(const Cursor& aDefaultCursor) override;
  void* GetNativeData(uint32_t aDataType) override;
  nsresult SetTitle(const nsAString& aTitle) override { return NS_OK; }
  [[nodiscard]] nsresult GetAttention(int32_t aCycleCount) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
  void SetInputContext(const InputContext& aContext,
                       const InputContextAction& aAction) override;
  InputContext GetInputContext() override;

  WindowRenderer* GetWindowRenderer() override;

  void NotifyCompositorSessionLost(
      mozilla::layers::CompositorSession* aSession) override;

  bool NeedsPaint() override;

  bool WidgetPaintsBackground() override;

  uint32_t GetMaxTouchPoints() const override;

  void UpdateZoomConstraints(
      const uint32_t& aPresShellId, const ScrollableLayerGuid::ViewID& aViewId,
      const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

  nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                      TouchPointerState aPointerState,
                                      LayoutDeviceIntPoint aPoint,
                                      double aPointerPressure,
                                      uint32_t aPointerOrientation,
                                      nsIObserver* aObserver) override;
  nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                      NativeMouseMessage aNativeMessage,
                                      mozilla::MouseButton aButton,
                                      nsIWidget::Modifiers aModifierFlags,
                                      nsIObserver* aObserver) override;
  nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                     nsIObserver* aObserver) override;

  void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;

  void GetCompositorWidgetInitData(
      mozilla::widget::CompositorWidgetInitData* aInitData) override;

  mozilla::layers::CompositorBridgeChild* GetCompositorBridgeChild() const;

  void SetContentDocumentDisplayed(bool aDisplayed);
  bool IsContentDocumentDisplayed();

  // Call this function when the users activity is the direct cause of an
  // event (like a keypress or mouse click).
  void UserActivity();

  mozilla::jni::Object::Ref& GetEditableParent() { return mEditableParent; }

  RefPtr<mozilla::a11y::SessionAccessibility> GetSessionAccessibility();

  void RecvToolbarAnimatorMessageFromCompositor(int32_t aMessage) override;
  void UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset,
                              const CSSToScreenScale& aZoom) override;
  void RecvScreenPixels(mozilla::ipc::Shmem&& aMem, const ScreenIntSize& aSize,
                        bool aNeedsYFlip) override;
  void UpdateDynamicToolbarMaxHeight(mozilla::ScreenIntCoord aHeight) override;
  mozilla::ScreenIntCoord GetDynamicToolbarMaxHeight() const override {
    return mDynamicToolbarMaxHeight;
  }

  void UpdateDynamicToolbarOffset(mozilla::ScreenIntCoord aOffset);

  mozilla::LayoutDeviceIntMargin GetSafeAreaInsets() const override;
  void UpdateSafeAreaInsets(const mozilla::LayoutDeviceIntMargin&);

  void KeyboardHeightChanged(mozilla::ScreenIntCoord aHeight);

  mozilla::jni::NativeWeakPtr<mozilla::widget::NPZCSupport>
  GetNPZCSupportWeakPtr();

 protected:
  void BringToFront();
  nsWindow* FindTopLevel();
  bool IsTopLevel();

  void ConfigureAPZControllerThread() override;
  void DispatchHitTest(const mozilla::WidgetTouchEvent& aEvent);

  already_AddRefed<GeckoContentController> CreateRootContentController()
      override;

  bool UseExternalCompositingSurface() const override { return true; }

  static void DumpWindows();
  static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
  static void LogWindow(nsWindow* win, int index, int indent);

  void CreateLayerManager();
  void RedrawAll();

  void OnSizeChanged(const mozilla::gfx::IntSize& aSize);

  mozilla::layers::LayersId GetRootLayerId() const;
  RefPtr<mozilla::layers::UiCompositorControllerChild>
  GetUiCompositorControllerChild();

  friend class mozilla::widget::GeckoViewSupport;
  friend class mozilla::widget::LayerViewSupport;
  friend class mozilla::widget::NPZCSupport;
};

#endif /* NSWINDOW_H_ */
