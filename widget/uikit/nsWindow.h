/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include <CoreFoundation/CoreFoundation.h>

#include "mozilla/widget/IOSView.h"
#include "nsBaseWidget.h"
#include "gfxPoint.h"

#include "nsTArray.h"

#ifdef __OBJC__
@class ChildView;
#else
typedef struct objc_object ChildView;
#endif

namespace mozilla::widget {
class EventDispatcher;
class TextInputHandler;
}  // namespace mozilla::widget

#define NS_WINDOW_IID                                \
  {                                                  \
    0x5e6fd559, 0xb3f9, 0x40c9, {                    \
      0x92, 0xd1, 0xef, 0x80, 0xb4, 0xf9, 0x69, 0xe9 \
    }                                                \
  }

class nsWindow final : public nsBaseWidget {
 public:
  nsWindow();

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_WINDOW_IID)

  NS_DECL_ISUPPORTS_INHERITED

  //
  // nsIWidget
  //

  [[nodiscard]] nsresult Create(
      nsIWidget* aParent, const LayoutDeviceIntRect& aRect,
      mozilla::widget::InitData* aInitData = nullptr) override;
  void Destroy() override;
  void Show(bool aState) override;
  void Enable(bool aState) override {}
  bool IsEnabled() const override { return true; }
  bool IsVisible() const override { return mVisible; }
  void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  LayoutDeviceIntPoint WidgetToScreenOffset() override;

  void SetBackgroundColor(const nscolor& aColor) override;
  void* GetNativeData(uint32_t aDataType) override;

  void Move(double aX, double aY) override;
  nsSizeMode SizeMode() override { return mSizeMode; }
  void SetSizeMode(nsSizeMode aMode) override;
  void EnteredFullScreen(bool aFullScreen);
  void Resize(double aWidth, double aHeight, bool aRepaint) override;
  void Resize(double aX, double aY, double aWidth, double aHeight,
              bool aRepaint) override;
  LayoutDeviceIntRect GetScreenBounds() override;
  void ReportMoveEvent();
  void ReportSizeEvent();
  void ReportSizeModeEvent(nsSizeMode aMode);

  CGFloat BackingScaleFactor();
  void BackingScaleFactorChanged();
  float GetDPI() override {
    // XXX: terrible
    return 326.0f;
  }
  double GetDefaultScaleInternal() override { return BackingScaleFactor(); }
  int32_t RoundsWidgetCoordinatesTo() override;

  nsresult SetTitle(const nsAString& aTitle) override { return NS_OK; }

  void Invalidate(const LayoutDeviceIntRect& aRect) override;
  nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus& aStatus) override;

  void WillPaintWindow();
  bool PaintWindow(LayoutDeviceIntRegion aRegion);

  bool HasModalDescendents() { return false; }

  // virtual nsresult
  // NotifyIME(const IMENotification& aIMENotification) override;
  void SetInputContext(const InputContext& aContext,
                       const InputContextAction& aAction) override;
  InputContext GetInputContext() override;
  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;

  mozilla::widget::TextInputHandler* GetTextInputHandler() const {
    return mTextInputHandler;
  }
  bool IsVirtualKeyboardDisabled() const;

  /*
  virtual bool ExecuteNativeKeyBinding(
                      NativeKeyBindingsType aType,
                      const mozilla::WidgetKeyboardEvent& aEvent,
                      DoCommandCallback aCallback,
                      void* aCallbackData) override;
  */

  mozilla::widget::EventDispatcher* GetEventDispatcher() const;

  static already_AddRefed<nsWindow> From(nsPIDOMWindowOuter* aDOMWindow);
  static already_AddRefed<nsWindow> From(nsIWidget* aWidget);

  void SetIOSView(already_AddRefed<mozilla::widget::IOSView>&& aView) {
    mIOSView = aView;
  }
  mozilla::widget::IOSView* GetIOSView() const { return mIOSView; }

 protected:
  virtual ~nsWindow();
  void BringToFront();
  nsWindow* FindTopLevel();
  bool IsTopLevel();
  nsresult GetCurrentOffset(uint32_t& aOffset, uint32_t& aLength);
  nsresult DeleteRange(int aOffset, int aLen);

  void TearDownView();

  ChildView* mNativeView;
  bool mVisible;
  nsSizeMode mSizeMode;
  nsTArray<nsWindow*> mChildren;
  nsWindow* mParent;

  mozilla::widget::InputContext mInputContext;
  RefPtr<mozilla::widget::TextInputHandler> mTextInputHandler;
  RefPtr<mozilla::widget::IOSView> mIOSView;

  void OnSizeChanged(const mozilla::gfx::IntSize& aSize);

  static void DumpWindows();
  static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
  static void LogWindow(nsWindow* win, int index, int indent);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsWindow, NS_WINDOW_IID)

#endif /* NSWINDOW_H_ */
