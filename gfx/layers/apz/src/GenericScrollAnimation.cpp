/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GenericScrollAnimation.h"

#include "AsyncPanZoomController.h"
#include "FrameMetrics.h"
#include "nsLayoutUtils.h"
#include "mozilla/layers/APZPublicUtils.h"
#include "nsPoint.h"
#include "ScrollAnimationPhysics.h"
#include "ScrollAnimationBezierPhysics.h"
#include "ScrollAnimationMSDPhysics.h"
#include "mozilla/StaticPrefs_general.h"

static mozilla::LazyLogModule sApzScrollAnimLog("apz.scrollanimation");
#define GSA_LOG(...) MOZ_LOG(sApzScrollAnimLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

GenericScrollAnimation::GenericScrollAnimation(AsyncPanZoomController& aApzc,
                                               const nsPoint& aInitialPosition,
                                               ScrollOrigin aOrigin)
    : mApzc(aApzc), mFinalDestination(aInitialPosition) {
  // ScrollAnimationBezierPhysics (despite its name) handles the case of
  // general.smoothScroll being disabled whereas ScrollAnimationMSDPhysics does
  // not (ie it scrolls smoothly).
  if (nsLayoutUtils::IsSmoothScrollingEnabled() &&
      StaticPrefs::general_smoothScroll_msdPhysics_enabled()) {
    mAnimationPhysics = MakeUnique<ScrollAnimationMSDPhysics>(aInitialPosition);
  } else {
    mAnimationPhysics = MakeUnique<ScrollAnimationBezierPhysics>(
        aInitialPosition,
        apz::ComputeBezierAnimationSettingsForOrigin(aOrigin));
  }
}

void GenericScrollAnimation::UpdateDelta(TimeStamp aTime, const nsPoint& aDelta,
                                         const nsSize& aCurrentVelocity) {
  mFinalDestination += aDelta;

  Update(aTime, aCurrentVelocity);
}

void GenericScrollAnimation::UpdateDestination(TimeStamp aTime,
                                               const nsPoint& aDestination,
                                               const nsSize& aCurrentVelocity) {
  mFinalDestination = aDestination;

  Update(aTime, aCurrentVelocity);
}

void GenericScrollAnimation::Update(TimeStamp aTime,
                                    const nsSize& aCurrentVelocity) {
  // Clamp the final destination to the scrollable area.
  CSSPoint clamped = CSSPoint::FromAppUnits(mFinalDestination);
  clamped.x = mApzc.mX.ClampOriginToScrollableRect(clamped.x);
  clamped.y = mApzc.mY.ClampOriginToScrollableRect(clamped.y);
  mFinalDestination = CSSPoint::ToAppUnits(clamped);

  mAnimationPhysics->Update(aTime, mFinalDestination, aCurrentVelocity);
}

bool GenericScrollAnimation::DoSample(FrameMetrics& aFrameMetrics,
                                      const TimeDuration& aDelta) {
  TimeStamp now = mApzc.GetFrameTime().Time();
  CSSToParentLayerScale zoom(aFrameMetrics.GetZoom());
  if (zoom == CSSToParentLayerScale(0)) {
    return false;
  }

  // If the animation is finished, make sure the final position is correct by
  // using one last displacement. Otherwise, compute the delta via the timing
  // function as normal.
  bool finished = mAnimationPhysics->IsFinished(now);
  nsPoint sampledDest = mAnimationPhysics->PositionAt(now);
  ParentLayerPoint displacement = (CSSPoint::FromAppUnits(sampledDest) -
                                   aFrameMetrics.GetVisualScrollOffset()) *
                                  zoom;

  if (finished) {
    mApzc.mX.SetVelocity(0);
    mApzc.mY.SetVelocity(0);
  } else if (!IsZero(displacement / zoom)) {
    // Convert velocity from AppUnits/Seconds to ParentLayerCoords/Milliseconds
    nsSize velocity = mAnimationPhysics->VelocityAt(now);
    ParentLayerPoint velocityPL =
        CSSPoint::FromAppUnits(nsPoint(velocity.width, velocity.height)) * zoom;
    mApzc.mX.SetVelocity(velocityPL.x / 1000.0);
    mApzc.mY.SetVelocity(velocityPL.y / 1000.0);
  }
  // Note: we ignore overscroll for generic animations.
  ParentLayerPoint adjustedOffset, overscroll;
  mApzc.mX.AdjustDisplacement(
      displacement.x, adjustedOffset.x, overscroll.x,
      mDirectionForcedToOverscroll == Some(ScrollDirection::eHorizontal));
  mApzc.mY.AdjustDisplacement(
      displacement.y, adjustedOffset.y, overscroll.y,
      mDirectionForcedToOverscroll == Some(ScrollDirection::eVertical));
  // If we expected to scroll, but there's no more scroll range on either axis,
  // then end the animation early. Note that the initial displacement could be 0
  // if the compositor ran very quickly (<1ms) after the animation was created.
  // When that happens we want to make sure the animation continues.
  GSA_LOG(
      "Sampling GenericScrollAnimation: time %f finished %d sampledDest %s "
      "adjustedOffset %s overscroll %s\n",
      (now - TimeStamp::ProcessCreation()).ToMilliseconds(), finished,
      ToString(CSSPoint::FromAppUnits(sampledDest)).c_str(),
      ToString(adjustedOffset).c_str(), ToString(overscroll).c_str());
  if (!IsZero(displacement / zoom) && IsZero(adjustedOffset / zoom)) {
    // Nothing more to do - end the animation.
    return false;
  }
  mApzc.ScrollBy(adjustedOffset / zoom);
  return !finished;
}

bool GenericScrollAnimation::HandleScrollOffsetUpdate(
    const Maybe<CSSPoint>& aRelativeDelta) {
  if (aRelativeDelta) {
    mAnimationPhysics->ApplyContentShift(*aRelativeDelta);
    mFinalDestination += CSSPoint::ToAppUnits(*aRelativeDelta);
    return true;
  }
  return false;
}

}  // namespace layers
}  // namespace mozilla
