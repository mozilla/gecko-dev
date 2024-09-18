/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SmoothMsdScrollAnimation.h"
#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {

SmoothMsdScrollAnimation::SmoothMsdScrollAnimation(
    AsyncPanZoomController& aApzc, const CSSPoint& aInitialPosition,
    const CSSPoint& aInitialVelocity, const CSSPoint& aDestination,
    double aSpringConstant, double aDampingRatio,
    ScrollSnapTargetIds&& aSnapTargetIds,
    ScrollTriggeredByScript aTriggeredByScript)
    : mApzc(aApzc),
      mXAxisModel(aInitialPosition.x, aDestination.x, aInitialVelocity.x,
                  aSpringConstant, aDampingRatio),
      mYAxisModel(aInitialPosition.y, aDestination.y, aInitialVelocity.y,
                  aSpringConstant, aDampingRatio),
      mSnapTargetIds(std::move(aSnapTargetIds)),
      mTriggeredByScript(aTriggeredByScript) {}

bool SmoothMsdScrollAnimation::DoSample(FrameMetrics& aFrameMetrics,
                                        const TimeDuration& aDelta) {
  CSSToParentLayerScale zoom(aFrameMetrics.GetZoom());
  if (zoom == CSSToParentLayerScale(0)) {
    return false;
  }
  CSSPoint oneParentLayerPixel =
      ParentLayerPoint(1, 1) / aFrameMetrics.GetZoom();
  if (mXAxisModel.IsFinished(oneParentLayerPixel.x) &&
      mYAxisModel.IsFinished(oneParentLayerPixel.y)) {
    // Set the scroll offset to the exact destination. If we allow the scroll
    // offset to end up being a bit off from the destination, we can get
    // artefacts like "scroll to the next snap point in this direction"
    // scrolling to the snap point we're already supposed to be at.
    mApzc.ClampAndSetVisualScrollOffset(
        CSSPoint(mXAxisModel.GetDestination(), mYAxisModel.GetDestination()));
    return false;
  }

  mXAxisModel.Simulate(aDelta);
  mYAxisModel.Simulate(aDelta);

  CSSPoint position =
      CSSPoint(mXAxisModel.GetPosition(), mYAxisModel.GetPosition());
  CSSPoint css_velocity =
      CSSPoint(mXAxisModel.GetVelocity(), mYAxisModel.GetVelocity());

  // Convert from pixels/second to pixels/ms
  ParentLayerPoint velocity =
      ParentLayerPoint(css_velocity.x, css_velocity.y) / 1000.0f;

  // Keep the velocity updated for the Axis class so that any animations
  // chained off of the smooth scroll will inherit it.
  if (mXAxisModel.IsFinished(oneParentLayerPixel.x)) {
    mApzc.mX.SetVelocity(0);
  } else {
    mApzc.mX.SetVelocity(velocity.x);
  }
  if (mYAxisModel.IsFinished(oneParentLayerPixel.y)) {
    mApzc.mY.SetVelocity(0);
  } else {
    mApzc.mY.SetVelocity(velocity.y);
  }
  // If we overscroll, hand off to a fling animation that will complete the
  // spring back.
  ParentLayerPoint displacement =
      (position - aFrameMetrics.GetVisualScrollOffset()) * zoom;

  ParentLayerPoint overscroll;
  ParentLayerPoint adjustedOffset;
  mApzc.mX.AdjustDisplacement(displacement.x, adjustedOffset.x, overscroll.x);
  mApzc.mY.AdjustDisplacement(displacement.y, adjustedOffset.y, overscroll.y);
  mApzc.ScrollBy(adjustedOffset / zoom);

  bool reachedEndOfScrollRange = !IsZero(overscroll / zoom);
  // Do not hand off a smooth scroll animation to an ancestor APZC,
  // nor allow it to cause overscroll.
  return !reachedEndOfScrollRange;
}

void SmoothMsdScrollAnimation::SetDestination(
    const CSSPoint& aNewDestination, ScrollSnapTargetIds&& aSnapTargetIds,
    ScrollTriggeredByScript aTriggeredByScript) {
  mXAxisModel.SetDestination(aNewDestination.x);
  mYAxisModel.SetDestination(aNewDestination.y);
  mSnapTargetIds = std::move(aSnapTargetIds);
  mTriggeredByScript = aTriggeredByScript;
}

CSSPoint SmoothMsdScrollAnimation::GetDestination() const {
  return CSSPoint(mXAxisModel.GetDestination(), mYAxisModel.GetDestination());
}

SmoothMsdScrollAnimation*
SmoothMsdScrollAnimation::AsSmoothMsdScrollAnimation() {
  return this;
}

}  // namespace layers
}  // namespace mozilla
