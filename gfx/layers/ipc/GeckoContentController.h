/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GeckoContentController_h
#define mozilla_layers_GeckoContentController_h

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "Units.h"                      // for CSSIntPoint, CSSRect, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "nsISupportsImpl.h"

class Task;

namespace mozilla {
namespace layers {

class GeckoContentController
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GeckoContentController)

  /**
   * Requests a paint of the given FrameMetrics |aFrameMetrics| from Gecko.
   * Implementations per-platform are responsible for actually handling this.
   */
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) = 0;

  /**
   * Requests handling of a double tap. |aPoint| is in CSS pixels, relative to
   * the current scroll offset. This should eventually round-trip back to
   * AsyncPanZoomController::ZoomToRect with the dimensions that we want to zoom
   * to.
   */
  virtual void HandleDoubleTap(const CSSIntPoint& aPoint, int32_t aModifiers) = 0;

  /**
   * Requests handling a single tap. |aPoint| is in CSS pixels, relative to the
   * current scroll offset. This should simulate and send to content a mouse
   * button down, then mouse button up at |aPoint|.
   */
  virtual void HandleSingleTap(const CSSIntPoint& aPoint, int32_t aModifiers) = 0;

  /**
   * Requests handling a long tap. |aPoint| is in CSS pixels, relative to the
   * current scroll offset.
   */
  virtual void HandleLongTap(const CSSIntPoint& aPoint, int32_t aModifiers) = 0;

  /**
   * Requests handling of releasing a long tap. |aPoint| is in CSS pixels,
   * relative to the current scroll offset. HandleLongTapUp will always be
   * preceeded by HandleLongTap
   */
  virtual void HandleLongTapUp(const CSSIntPoint& aPoint, int32_t aModifiers) = 0;

  /**
   * Requests sending a mozbrowserasyncscroll domevent to embedder.
   * |aContentRect| is in CSS pixels, relative to the current cssPage.
   * |aScrollableSize| is the current content width/height in CSS pixels.
   */
  virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect &aContentRect,
                                       const CSSSize &aScrollableSize) = 0;

  /**
   * Schedules a runnable to run on the controller/UI thread at some time
   * in the future.
   */
  virtual void PostDelayedTask(Task* aTask, int aDelayMs) = 0;

  /**
   * Retrieves the last known zoom constraints for the root scrollable layer
   * for this layers tree. This function should return false if there are no
   * last known zoom constraints.
   */
  virtual bool GetRootZoomConstraints(ZoomConstraints* aOutConstraints)
  {
    return false;
  }

  /**
   * APZ uses |FrameMetrics::mCompositionBounds| for hit testing. Sometimes,
   * widget code has knowledge of a touch-sensitive region that should
   * additionally constrain hit testing for all frames associated with the
   * controller. This method allows APZ to query the controller for such a
   * region. A return value of true indicates that the controller has such a
   * region, and it is returned in |aOutRegion|.
   * TODO: once bug 928833 is implemented, this should be removed, as
   * APZ can then get the correct touch-sensitive region for each frame
   * directly from the layer.
   */
  virtual bool GetTouchSensitiveRegion(CSSRect* aOutRegion)
  {
    return false;
  }

  /**
   * General tranformation notices for consumers. These fire any time
   * the apzc is modifying the view, including panning, zooming, and
   * fling.
   */
  virtual void NotifyTransformBegin(const ScrollableLayerGuid& aGuid) {}
  virtual void NotifyTransformEnd(const ScrollableLayerGuid& aGuid) {}

  GeckoContentController() {}
  virtual ~GeckoContentController() {}
};

}
}

#endif // mozilla_layers_GeckoContentController_h
