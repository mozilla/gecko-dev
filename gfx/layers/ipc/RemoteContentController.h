/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_RemoteContentController_h
#define mozilla_layers_RemoteContentController_h

#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/PAPZParent.h"

namespace mozilla {

namespace dom {
class TabParent;
}

namespace layers {

/**
 * RemoteContentController implements PAPZChild and is used to access a
 * GeckoContentController that lives in a different process.
 *
 * RemoteContentController lives on the compositor thread. All methods can
 * be called off the compositor thread and will get dispatched to the right
 * thread, with the exception of RequestContentRepaint and NotifyFlushComplete,
 * which must be called on the repaint thread, which in this case is the
 * compositor thread.
 */
class RemoteContentController : public GeckoContentController,
                                public PAPZParent {
  using GeckoContentController::APZStateChange;
  using GeckoContentController::TapType;

 public:
  RemoteContentController();

  virtual ~RemoteContentController();

  virtual void RequestContentRepaint(const RepaintRequest& aRequest) override;

  virtual void HandleTap(TapType aTapType, const LayoutDevicePoint& aPoint,
                         Modifiers aModifiers, const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId) override;

  virtual void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                                  const ScrollableLayerGuid& aGuid,
                                  LayoutDeviceCoord aSpanChange,
                                  Modifiers aModifiers) override;

  virtual void PostDelayedTask(already_AddRefed<Runnable> aTask,
                               int aDelayMs) override;

  virtual bool IsRepaintThread() override;

  virtual void DispatchToRepaintThread(
      already_AddRefed<Runnable> aTask) override;

  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange, int aArg) override;

  virtual void UpdateOverscrollVelocity(float aX, float aY,
                                        bool aIsRootContent) override;

  virtual void UpdateOverscrollOffset(float aX, float aY,
                                      bool aIsRootContent) override;

  virtual void NotifyMozMouseScrollEvent(
      const ScrollableLayerGuid::ViewID& aScrollId,
      const nsString& aEvent) override;

  virtual void NotifyFlushComplete() override;

  virtual void NotifyAsyncScrollbarDragInitiated(
      uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
      ScrollDirection aDirection) override;
  virtual void NotifyAsyncScrollbarDragRejected(
      const ScrollableLayerGuid::ViewID& aScrollId) override;

  virtual void NotifyAsyncAutoscrollRejected(
      const ScrollableLayerGuid::ViewID& aScrollId) override;

  virtual void CancelAutoscroll(const ScrollableLayerGuid& aScrollId) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual void Destroy() override;

 private:
  MessageLoop* mCompositorThread;
  bool mCanSend;

  void HandleTapOnMainThread(TapType aType, LayoutDevicePoint aPoint,
                             Modifiers aModifiers, ScrollableLayerGuid aGuid,
                             uint64_t aInputBlockId);
  void HandleTapOnCompositorThread(TapType aType, LayoutDevicePoint aPoint,
                                   Modifiers aModifiers,
                                   ScrollableLayerGuid aGuid,
                                   uint64_t aInputBlockId);
  void NotifyPinchGestureOnCompositorThread(
      PinchGestureInput::PinchGestureType aType,
      const ScrollableLayerGuid& aGuid, LayoutDeviceCoord aSpanChange,
      Modifiers aModifiers);

  void CancelAutoscrollInProcess(const ScrollableLayerGuid& aScrollId);
  void CancelAutoscrollCrossProcess(const ScrollableLayerGuid& aScrollId);
};

}  // namespace layers

}  // namespace mozilla

#endif  // mozilla_layers_RemoteContentController_h
