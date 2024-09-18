/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "mozilla/ScrollPositionUpdate.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"

#include "InputUtils.h"

class APZCOverscrollTester : public APZCBasicTester {
 public:
  explicit APZCOverscrollTester(
      AsyncPanZoomController::GestureBehavior aGestureBehavior =
          AsyncPanZoomController::DEFAULT_GESTURES)
      : APZCBasicTester(aGestureBehavior) {}

 protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;

  void TestOverscroll() {
    // Pan sufficiently to hit overscroll behavior
    PanIntoOverscroll();

    // Check that we recover from overscroll via an animation.
    ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
    SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
  }

  void PanIntoOverscroll() {
    int touchStart = 500;
    int touchEnd = 10;
    Pan(apzc, touchStart, touchEnd);
    EXPECT_TRUE(apzc->IsOverscrolled());
  }

  /**
   * Sample animations until we recover from overscroll.
   * @param aExpectedScrollOffset the expected reported scroll offset
   *                              throughout the animation
   */
  void SampleAnimationUntilRecoveredFromOverscroll(
      const ParentLayerPoint& aExpectedScrollOffset) {
    const TimeDuration increment = TimeDuration::FromMilliseconds(1);
    bool recoveredFromOverscroll = false;
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    while (apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut)) {
      // The reported scroll offset should be the same throughout.
      EXPECT_EQ(aExpectedScrollOffset, pointOut);

      // Trigger computation of the overscroll tranform, to make sure
      // no assetions fire during the calculation.
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);

      if (!apzc->IsOverscrolled()) {
        recoveredFromOverscroll = true;
      }

      mcc->AdvanceBy(increment);
    }
    EXPECT_TRUE(recoveredFromOverscroll);
    apzc->AssertStateIsReset();
  }

  ScrollableLayerGuid CreateSimpleRootScrollableForWebRender() {
    ScrollableLayerGuid guid;
    guid.mScrollId = ScrollableLayerGuid::START_SCROLL_ID;
    guid.mLayersId = LayersId{0};

    ScrollMetadata metadata;
    FrameMetrics& metrics = metadata.GetMetrics();
    metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
    metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
    metrics.SetScrollId(guid.mScrollId);
    metadata.SetIsLayersIdRoot(true);

    WebRenderLayerScrollData rootLayerScrollData;
    rootLayerScrollData.InitializeRoot(0);
    WebRenderScrollData scrollData;
    rootLayerScrollData.AppendScrollMetadata(scrollData, metadata);
    scrollData.AddLayerData(std::move(rootLayerScrollData));

    registration = MakeUnique<ScopedLayerTreeRegistration>(guid.mLayersId, mcc);
    tm->UpdateHitTestingTree(WebRenderScrollDataWrapper(*updater, &scrollData),
                             guid.mLayersId, 0);
    return guid;
  }
};

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, FlingIntoOverscroll) {
  // Enable overscrolling.
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  // Scroll down by 25 px. Don't fling for simplicity.
  Pan(apzc, 50, 25, PanOptions::NoFling);

  // Now scroll back up by 20px, this time flinging after.
  // The fling should cover the remaining 5 px of room to scroll, then
  // go into overscroll, and finally snap-back to recover from overscroll.
  Pan(apzc, 25, 45);
  const TimeDuration increment = TimeDuration::FromMilliseconds(1);
  bool reachedOverscroll = false;
  bool recoveredFromOverscroll = false;
  while (apzc->AdvanceAnimations(mcc->GetSampleTime())) {
    if (!reachedOverscroll && apzc->IsOverscrolled()) {
      reachedOverscroll = true;
    }
    if (reachedOverscroll && !apzc->IsOverscrolled()) {
      recoveredFromOverscroll = true;
    }
    mcc->AdvanceBy(increment);
  }
  EXPECT_TRUE(reachedOverscroll);
  EXPECT_TRUE(recoveredFromOverscroll);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverScrollPanning) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  TestOverscroll();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Tests that an overscroll animation doesn't trigger an assertion failure
// in the case where a sample has a velocity of zero.
TEST_F(APZCOverscrollTester, OverScroll_Bug1152051a) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Doctor the prefs to make the velocity zero at the end of the first sample.

  // This ensures our incoming velocity to the overscroll animation is
  // a round(ish) number, 4.9 (that being the distance of the pan before
  // overscroll, which is 500 - 10 = 490 pixels, divided by the duration of
  // the pan, which is 100 ms).
  SCOPED_GFX_PREF_FLOAT("apz.fling_friction", 0);

  TestOverscroll();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Tests that ending an overscroll animation doesn't leave around state that
// confuses the next overscroll animation.
TEST_F(APZCOverscrollTester, OverScroll_Bug1152051b) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_FLOAT("apz.overscroll.stop_distance_threshold", 0.1f);

  // Pan sufficiently to hit overscroll behavior
  PanIntoOverscroll();

  // Sample animations once, to give the fling animation started on touch-up
  // a chance to realize it's overscrolled, and schedule a call to
  // HandleFlingOverscroll().
  SampleAnimationOnce();

  // This advances the time and runs the HandleFlingOverscroll task scheduled in
  // the previous call, which starts an overscroll animation. It then samples
  // the overscroll animation once, to get it to initialize the first overscroll
  // sample.
  SampleAnimationOnce();

  // Do a touch-down to cancel the overscroll animation, and then a touch-up
  // to schedule a new one since we're still overscrolled. We don't pan because
  // panning can trigger functions that clear the overscroll animation state
  // in other ways.
  APZEventResult result = TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time());
  if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(apzc, result.mInputBlockId);
  }
  TouchUp(apzc, ScreenIntPoint(10, 10), mcc->Time());

  // Sample the second overscroll animation to its end.
  // If the ending of the first overscroll animation fails to clear state
  // properly, this will assert.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Tests that the page doesn't get stuck in an
// overscroll animation after a low-velocity pan.
TEST_F(APZCOverscrollTester, OverScrollAfterLowVelocityPan_Bug1343775) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Pan into overscroll with a velocity less than the
  // apz.fling_min_velocity_threshold preference.
  Pan(apzc, 10, 30);

  EXPECT_TRUE(apzc->IsOverscrolled());

  apzc->AdvanceAnimationsUntilEnd();

  // Check that we recovered from overscroll.
  EXPECT_FALSE(apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverScrollAbort) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Pan sufficiently to hit overscroll behavior
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());

  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;

  // This sample call will run to the end of the fling animation
  // and will schedule the overscroll animation.
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut,
                                       TimeDuration::FromMilliseconds(10000));
  EXPECT_TRUE(apzc->IsOverscrolled());

  // At this point, we have an active overscroll animation.
  // Check that cancelling the animation clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverScrollPanningAbort) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Pan sufficiently to hit overscroll behaviour. Keep the finger down so
  // the pan does not end.
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, touchStart, touchEnd, PanOptions::KeepFingerDown);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that calling CancelAnimation() while the user is still panning
  // (and thus no fling or snap-back animation has had a chance to start)
  // clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
TEST_F(APZCOverscrollTester, OverscrollByVerticalPanGestures) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, StuckInOverscroll_Bug1767337) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Send two PANGESTURE_END in a row, to see if the second one gets us
  // stuck in overscroll.
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time(), MODIFIER_NONE, true);
  SampleAnimationOnce();
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time(), MODIFIER_NONE, true);

  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverscrollByVerticalAndHorizontalPanGestures) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverscrollByPanMomentumGestures) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we are not yet in overscrolled region.
  EXPECT_TRUE(!apzc->IsOverscrolled());

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());

  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, IgnoreMomemtumDuringOverscroll) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  float yMost = GetScrollRange().YMost();
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, yMost / 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, yMost), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, yMost / 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  // And check the overscrolled transform value before/after calling PanGesture
  // to make sure the overscroll amount isn't affected by momentum events.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  AsyncTransformComponentMatrix overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_EQ(
      overscrolledTransform,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  EXPECT_EQ(
      overscrolledTransform,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  EXPECT_EQ(
      overscrolledTransform,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 2), mcc->Time());
  EXPECT_EQ(
      overscrolledTransform,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_EQ(
      overscrolledTransform,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling));

  // Check that we've recovered from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, VerticalOnlyOverscroll) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Make the content scrollable only vertically.
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  apzc->SetFrameMetrics(metrics);

  // Scroll up into overscroll a bit.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());
  // Now it's overscrolled.
  EXPECT_TRUE(apzc->IsOverscrolled());
  AsyncTransformComponentMatrix overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  // The overscroll shouldn't happen horizontally.
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  // Happens only vertically.
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  // Send pan momentum events including horizontal bits.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-10, -100), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  // The overscroll shouldn't happen horizontally.
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-5, -50), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -2), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, VerticalOnlyOverscrollByPanMomentum) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Make the content scrollable only vertically.
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  // Scrolls the content down a bit.
  metrics.SetVisualScrollOffset(CSSPoint(0, 50));
  apzc->SetFrameMetrics(metrics);

  // Scroll up a bit where overscroll will not happen.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure it's not yet overscrolled.
  EXPECT_TRUE(!apzc->IsOverscrolled());

  // Send pan momentum events including horizontal bits.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-10, -100), mcc->Time());
  // Now it's overscrolled.
  EXPECT_TRUE(apzc->IsOverscrolled());

  AsyncTransformComponentMatrix overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  // But the overscroll shouldn't happen horizontally.
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  // Happens only vertically.
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-5, -50), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -2), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, DisallowOverscrollInSingleLineTextControl) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Create a horizontal scrollable frame with `vertical disregarded direction`.
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 10));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 10));
  apzc->SetFrameMetrics(metrics);
  metadata.SetDisregardedDirection(Some(ScrollDirection::eVertical));
  apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                            /*aThisLayerTreeUpdated=*/true);

  // Try to overscroll up and left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(-10, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(0, 0), mcc->Time());

  // No overscrolling should happen.
  EXPECT_TRUE(!apzc->IsOverscrolled());

  // Send pan momentum events too.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(-100, -100), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(-50, -50), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(0, 0), mcc->Time());
  // No overscrolling should happen either.
  EXPECT_TRUE(!apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
// Tests that horizontal overscroll animation keeps running with vertical
// pan momentum scrolling.
TEST_F(APZCOverscrollTester,
       HorizontalOverscrollAnimationWithVerticalPanMomentumScrolling) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 5000));
  apzc->SetFrameMetrics(metrics);

  // Try to overscroll left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  AsyncTransformComponentMatrix initialOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);

  // Send lengthy downward momentums to make sure the overscroll animation
  // doesn't clobber the momentums scrolling.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount on X axis has started being managed by the overscroll
  // animation.
  AsyncTransformComponentMatrix currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_NE(initialOverscrolledTransform._41, currentOverscrolledTransform._41);
  // There is no overscroll on Y axis.
  EXPECT_EQ(currentOverscrolledTransform._42, 0);
  ParentLayerPoint scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  // The scroll offset shouldn't be changed by the overscroll animation.
  EXPECT_EQ(scrollOffset.y, 0);

  // Simple gesture on the Y axis to ensure that we can send a vertical
  // momentum scroll
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  ParentLayerPoint offsetAfterPan = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);

  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by this pan
  // momentum start event since the displacement is zero.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount should be managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  // Not yet started scrolling.
  EXPECT_EQ(scrollOffset.y, offsetAfterPan.y);
  EXPECT_EQ(scrollOffset.x, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);

  // Send a long pan momentum.
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // Now it started scrolling vertically.
  scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  EXPECT_GT(scrollOffset.y, 0);
  EXPECT_EQ(scrollOffset.x, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll on X axis keeps being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // The scroll offset on Y axis shouldn't be changed by the overscroll
  // animation.
  EXPECT_EQ(scrollOffset.y, apzc->GetCurrentAsyncScrollOffset(
                                    AsyncPanZoomController::eForEventHandling)
                                .y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // Scrolling keeps going by momentum.
  EXPECT_GT(apzc->GetCurrentAsyncScrollOffset(
                    AsyncPanZoomController::eForEventHandling)
                .y,
            scrollOffset.y);

  scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 10), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // Scrolling keeps going by momentum.
  EXPECT_GT(apzc->GetCurrentAsyncScrollOffset(
                    AsyncPanZoomController::eForEventHandling)
                .y,
            scrollOffset.y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // This momentum event doesn't change the scroll offset since its
  // displacement is zero.
  EXPECT_EQ(apzc->GetCurrentAsyncScrollOffset(
                    AsyncPanZoomController::eForEventHandling)
                .y,
            scrollOffset.y);

  // Check that we recover from the horizontal overscroll via the animation.
  ParentLayerPoint expectedScrollOffset(0, scrollOffset.y);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
// Similar to above
// HorizontalOverscrollAnimationWithVerticalPanMomentumScrolling,
// but having OverscrollAnimation on both axes initially.
TEST_F(APZCOverscrollTester,
       BothAxesOverscrollAnimationWithPanMomentumScrolling) {
  // TODO: This test currently requires gestures that cause movement on both
  // axis, which excludes DOMINANT_AXIS locking mode. The gestures should be
  // broken up into multiple gestures to cause the overscroll.
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 2);
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 5000));
  apzc->SetFrameMetrics(metrics);

  // Try to overscroll up and left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  AsyncTransformComponentMatrix initialOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);

  // Send lengthy downward momentums to make sure the overscroll animation
  // doesn't clobber the momentums scrolling.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount has started being managed by the overscroll
  // animation.
  AsyncTransformComponentMatrix currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_NE(initialOverscrolledTransform._41, currentOverscrolledTransform._41);
  EXPECT_NE(initialOverscrolledTransform._42, currentOverscrolledTransform._42);

  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by this pan
  // momentum start event since the displacement is zero.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // Still being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  EXPECT_NE(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  // Send a long pan momentum.
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // But now the overscroll amount on Y axis should be changed by this momentum
  // pan.
  EXPECT_NE(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);
  // Actually it's no longer overscrolled.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42,
      0);

  ParentLayerPoint currentScrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  // Now it started scrolling.
  EXPECT_GT(currentScrollOffset.y, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll on X axis keeps being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // But the overscroll on Y axis is no longer affected by the overscroll
  // animation.
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);
  // The scroll offset on Y axis shouldn't be changed by the overscroll
  // animation.
  EXPECT_EQ(currentScrollOffset.y,
            apzc->GetCurrentAsyncScrollOffset(
                    AsyncPanZoomController::eForEventHandling)
                .y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  currentScrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // Keeping no overscrolling on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42,
      0);
  // Scrolling keeps going by momentum.
  EXPECT_GT(apzc->GetCurrentAsyncScrollOffset(
                    AsyncPanZoomController::eForEventHandling)
                .y,
            currentScrollOffset.y);

  currentScrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 10), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // Keeping no overscrolling on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42,
      0);
  // Scrolling keeps going by momentum.
  EXPECT_GT(apzc->GetCurrentAsyncScrollOffset(
                    AsyncPanZoomController::eForEventHandling)
                .y,
            currentScrollOffset.y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  currentScrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // Keeping no overscrolling on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42,
      0);
  // This momentum event doesn't change the scroll offset since its
  // displacement is zero.
  EXPECT_EQ(apzc->GetCurrentAsyncScrollOffset(
                    AsyncPanZoomController::eForEventHandling)
                .y,
            currentScrollOffset.y);

  // Check that we recover from the horizontal overscroll via the animation.
  ParentLayerPoint expectedScrollOffset(0, currentScrollOffset.y);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
// This is another variant of
// HorizontalOverscrollAnimationWithVerticalPanMomentumScrolling. In this test,
// after a horizontal overscroll animation started, upwards pan moments happen,
// thus there should be a new vertical overscroll animation in addition to
// the horizontal one.
TEST_F(
    APZCOverscrollTester,
    VerticalOverscrollAnimationInAdditionToExistingHorizontalOverscrollAnimation) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 5000));
  // Scrolls the content 50px down.
  metrics.SetVisualScrollOffset(CSSPoint(0, 50));
  apzc->SetFrameMetrics(metrics);

  // Try to overscroll left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  AsyncTransformComponentMatrix initialOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);

  // Send lengthy __upward__ momentums to make sure the overscroll animation
  // doesn't clobber the momentums scrolling.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount on X axis has started being managed by the overscroll
  // animation.
  AsyncTransformComponentMatrix currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  EXPECT_NE(initialOverscrolledTransform._41, currentOverscrolledTransform._41);
  // There is no overscroll on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42,
      0);
  ParentLayerPoint scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  // The scroll offset shouldn't be changed by the overscroll animation.
  EXPECT_EQ(scrollOffset.y, 50);

  // Simple gesture on the Y axis to ensure that we can send a vertical
  // momentum scroll
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  ParentLayerPoint offsetAfterPan = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);

  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by this pan
  // momentum start event since the displacement is zero.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount should be managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  // Not yet started scrolling.
  EXPECT_EQ(scrollOffset.y, offsetAfterPan.y);
  EXPECT_EQ(scrollOffset.x, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);

  // Send a long pan momentum.
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -200), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // Now it started scrolling vertically.
  scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  EXPECT_EQ(scrollOffset.y, 0);
  EXPECT_EQ(scrollOffset.x, 0);
  // Actually it's also vertically overscrolled.
  EXPECT_GT(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42,
      0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll on X axis keeps being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // The overscroll on Y Axis hasn't been changed by the overscroll animation at
  // this moment, sine the last displacement was consumed in the last pan
  // momentum.
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -100), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // Now the overscroll amount on Y axis shouldn't be changed by this momentum
  // pan either.
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  // And now the overscroll on Y Axis should be also managed by the overscroll
  // animation.
  EXPECT_NE(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -10), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by momentum event.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling)
          ._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForEventHandling);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  // Check that we recover from the horizontal overscroll via the animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverscrollByPanGesturesInterruptedByReflowZoom) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_INT("mousewheel.with_control.action", 3);  // reflow zoom.

  // A sanity check that pan gestures with ctrl modifier will not be handled by
  // APZ.
  PanGestureInput panInput(PanGestureInput::PANGESTURE_START, mcc->Time(),
                           ScreenIntPoint(5, 5), ScreenPoint(0, -2),
                           MODIFIER_CONTROL);
  WidgetWheelEvent wheelEvent = panInput.ToWidgetEvent(nullptr);
  EXPECT_FALSE(APZInputBridge::ActionForWheelEvent(&wheelEvent).isSome());

  ScrollableLayerGuid rootGuid = CreateSimpleRootScrollableForWebRender();
  RefPtr<AsyncPanZoomController> apzc =
      tm->GetTargetAPZC(rootGuid.mLayersId, rootGuid.mScrollId);

  PanGesture(PanGestureInput::PANGESTURE_START, tm, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, tm, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Make sure overscrolling has started.
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Press ctrl until PANGESTURE_END.
  PanGestureWithModifiers(PanGestureInput::PANGESTURE_PAN, MODIFIER_CONTROL, tm,
                          ScreenIntPoint(50, 80), ScreenPoint(0, -2),
                          mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // At this moment (i.e. PANGESTURE_PAN), still in overscrolling state.
  EXPECT_TRUE(apzc->IsOverscrolled());

  PanGestureWithModifiers(PanGestureInput::PANGESTURE_END, MODIFIER_CONTROL, tm,
                          ScreenIntPoint(50, 80), ScreenPoint(0, 0),
                          mcc->Time());
  // The overscrolling state should have been restored.
  EXPECT_TRUE(!apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, SmoothTransitionFromPanToAnimation) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  // Start scrolled down to y=500px.
  metrics.SetVisualScrollOffset(CSSPoint(0, 500));
  apzc->SetFrameMetrics(metrics);

  int frameLength = 10;    // milliseconds; 10 to keep the math simple
  float panVelocity = 10;  // pixels per millisecond
  int panPixelsPerFrame = frameLength * panVelocity;  // 100 pixels per frame

  ScreenIntPoint panPoint(50, 50);
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, -1), mcc->Time());
  // Pan up for 6 frames at 100 pixels per frame. This should reduce
  // the vertical scroll offset from 500 to 0, and get us into overscroll.
  for (int i = 0; i < 6; ++i) {
    mcc->AdvanceByMillis(frameLength);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
               ScreenPoint(0, -panPixelsPerFrame), mcc->Time());
  }
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Pan further into overscroll at the same input velocity, enough
  // for the frames while we are in overscroll to dominate the computation
  // in the velocity tracker.
  // Importantly, while the input velocity is still 100 pixels per frame,
  // in the overscrolled state the page only visual moves by at most 8 pixels
  // per frame.
  int frames = StaticPrefs::apz_velocity_relevance_time_ms() / frameLength;
  for (int i = 0; i < frames; ++i) {
    mcc->AdvanceByMillis(frameLength);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
               ScreenPoint(0, -panPixelsPerFrame), mcc->Time());
  }
  EXPECT_TRUE(apzc->IsOverscrolled());

  // End the pan, allowing an overscroll animation to start.
  mcc->AdvanceByMillis(frameLength);
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, panPoint, ScreenPoint(0, 0),
             mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  // Check that the velocity reflects the actual movement (no more than 8
  // pixels/frame ==> 0.8 pixels per millisecond), not the input velocity
  // (100 pixels/frame ==> 10 pixels per millisecond). This ensures that
  // the transition from the pan to the animation appears smooth.
  // (Note: velocities are negative since they are upwards.)
  EXPECT_LT(apzc->GetVelocityVector().y, 0);
  EXPECT_GT(apzc->GetVelocityVector().y, -0.8);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, NoOverscrollForMousewheel) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  // Start scrolled down just a few pixels from the top.
  metrics.SetVisualScrollOffset(CSSPoint(0, 3));
  // Set line and page scroll amounts. Otherwise, even though Wheel() uses
  // SCROLLDELTA_PIXEL, the wheel handling code will get confused by things
  // like the "don't scroll more than one page" check.
  metadata.SetPageScrollAmount(LayoutDeviceIntSize(50, 100));
  metadata.SetLineScrollAmount(LayoutDeviceIntSize(5, 10));
  apzc->SetScrollMetadata(metadata);

  // Send a wheel with enough delta to scrollto y=0 *and* overscroll.
  Wheel(apzc, ScreenIntPoint(10, 10), ScreenPoint(0, -10), mcc->Time());

  // Check that we did not actually go into overscroll.
  EXPECT_FALSE(apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, ClickWhileOverscrolled) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  metrics.SetVisualScrollOffset(CSSPoint(0, 0));
  apzc->SetFrameMetrics(metrics);

  // Pan into overscroll at the top.
  ScreenIntPoint panPoint(50, 50);
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, -1), mcc->Time());
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
             ScreenPoint(0, -100), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->GetOverscrollAmount().y < 0);  // overscrolled at top

  // End the pan. This should start an overscroll animation.
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, panPoint, ScreenPoint(0, 0),
             mcc->Time());
  EXPECT_TRUE(apzc->GetOverscrollAmount().y < 0);  // overscrolled at top
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  // Send a mouse-down. This should interrupt the animation but not relieve
  // overscroll yet.
  ParentLayerPoint overscrollBefore = apzc->GetOverscrollAmount();
  MouseDown(apzc, panPoint, mcc->Time());
  EXPECT_FALSE(apzc->IsOverscrollAnimationRunning());
  EXPECT_EQ(overscrollBefore, apzc->GetOverscrollAmount());

  // Send a mouse-up. This should start an overscroll animation again.
  MouseUp(apzc, panPoint, mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  SampleAnimationUntilRecoveredFromOverscroll(ParentLayerPoint(0, 0));
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, DynamicallyLoadingContent) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  metrics.SetVisualScrollOffset(CSSPoint(0, 0));
  apzc->SetFrameMetrics(metrics);

  // Pan to the bottom of the page, and further, into overscroll.
  ScreenIntPoint panPoint(50, 50);
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, 1), mcc->Time());
  for (int i = 0; i < 12; ++i) {
    mcc->AdvanceByMillis(10);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
               ScreenPoint(0, 100), mcc->Time());
  }
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->GetOverscrollAmount().y > 0);  // overscrolled at bottom

  // Grow the scrollable rect at the bottom, simulating the page loading content
  // dynamically.
  CSSRect scrollableRect = metrics.GetScrollableRect();
  scrollableRect.height += 500;
  metrics.SetScrollableRect(scrollableRect);
  apzc->NotifyLayersUpdated(metadata, false, true);

  // Check that the modified scrollable rect cleared the overscroll.
  EXPECT_FALSE(apzc->IsOverscrolled());

  // Pan back up to the top, and further, into overscroll.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, -1), mcc->Time());
  for (int i = 0; i < 12; ++i) {
    mcc->AdvanceByMillis(10);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
               ScreenPoint(0, -100), mcc->Time());
  }
  EXPECT_TRUE(apzc->IsOverscrolled());
  ParentLayerPoint overscrollAmount = apzc->GetOverscrollAmount();
  EXPECT_TRUE(overscrollAmount.y < 0);  // overscrolled at top

  // Grow the scrollable rect at the bottom again.
  scrollableRect = metrics.GetScrollableRect();
  scrollableRect.height += 500;
  metrics.SetScrollableRect(scrollableRect);
  apzc->NotifyLayersUpdated(metadata, false, true);

  // Check that the modified scrollable rect did NOT clear overscroll at the
  // top.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_EQ(overscrollAmount,
            apzc->GetOverscrollAmount());  // overscroll did not change at all
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, SmallAmountOfOverscroll) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));

  // Do vertical overscroll first.
  ScreenIntPoint panPoint(50, 50);
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, panPoint, ScreenPoint(0, 0),
             mcc->Time());
  mcc->AdvanceByMillis(10);

  // Then do small horizontal overscroll which will be considered as "finished"
  // by our overscroll animation physics model.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(-0.1, 0), mcc->Time());
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
             ScreenPoint(-0.2, 0), mcc->Time());
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, panPoint, ScreenPoint(0, 0),
             mcc->Time());
  mcc->AdvanceByMillis(10);

  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->GetOverscrollAmount().y < 0);  // overscrolled at top
  EXPECT_TRUE(apzc->GetOverscrollAmount().x < 0);  // and overscrolled at left

  // Then do vertical scroll.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
             ScreenPoint(0, 100), mcc->Time());
  mcc->AdvanceByMillis(10);
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, panPoint, ScreenPoint(0, 0),
             mcc->Time());

  ParentLayerPoint scrollOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  EXPECT_GT(scrollOffset.y, 0);  // Make sure the vertical scroll offset is
                                 // greater than zero.

  // The small horizontal overscroll amount should be restored to zero.
  ParentLayerPoint expectedScrollOffset(0, scrollOffset.y);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifdef MOZ_WIDGET_ANDROID  // Only applies to WidgetOverscrollEffect
TEST_F(APZCOverscrollTester, StuckInOverscroll_Bug1786452) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));

  // Over the course of the test, expect one or more calls to
  // UpdateOverscrollOffset(), followed by a call to UpdateOverscrollVelocity().
  // The latter ensures the widget has a chance to end its overscroll effect.
  InSequence s;
  EXPECT_CALL(*mcc, UpdateOverscrollOffset(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, UpdateOverscrollVelocity(_, _, _, _)).Times(1);

  // Pan into overscroll, keeping the finger down
  ScreenIntPoint startPoint(10, 500);
  ScreenIntPoint endPoint(10, 10);
  Pan(apzc, startPoint, endPoint, PanOptions::KeepFingerDown);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Linger a while to cause the velocity to drop to very low or zero
  mcc->AdvanceByMillis(100);
  TouchMove(apzc, endPoint, mcc->Time());
  EXPECT_LT(apzc->GetVelocityVector().Length(),
            StaticPrefs::apz_fling_min_velocity_threshold());
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Lift the finger
  mcc->AdvanceByMillis(20);
  TouchUp(apzc, endPoint, mcc->Time());
  EXPECT_FALSE(apzc->IsOverscrolled());
}
#endif

class APZCOverscrollTesterMock : public APZCTreeManagerTester {
 public:
  APZCOverscrollTesterMock() { CreateMockHitTester(); }

  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;
};

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTesterMock, OverscrollHandoff) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                     LayerIntRect(0, 0, 100, 50)};
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 200, 200));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            // same size as the visible region so that
                            // the container is not scrollable in any directions
                            // actually. This is simulating overflow: hidden
                            // iframe document in Fission, though we don't set
                            // a different layers id.
                            CSSRect(0, 0, 100, 50));

  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  // A pan gesture on the child scroller (which is not scrollable though).
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 20),
             ScreenPoint(0, -2), mcc->Time());
  EXPECT_TRUE(rootApzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTesterMock, VerticalOverscrollHandoffToScrollableRoot) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Create a layer tree having two vertical scrollable layers.
  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                     LayerIntRect(0, 0, 100, 50)};
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 200));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 100, 200));

  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  // A vertical pan gesture on the child scroller which will be handed off to
  // the root APZC.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 20),
             ScreenPoint(0, -2), mcc->Time());
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_FALSE(ApzcOf(layers[1])->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTesterMock, NoOverscrollHandoffToNonScrollableRoot) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Create a layer tree having non-scrollable root and a vertical scrollable
  // child.
  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                     LayerIntRect(0, 0, 100, 50)};
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 100));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 100, 200));

  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  // A vertical pan gesture on the child scroller which should not be handed
  // off the root APZC.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 20),
             ScreenPoint(0, -2), mcc->Time());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
  EXPECT_TRUE(ApzcOf(layers[1])->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTesterMock, NoOverscrollHandoffOrthogonalPanGesture) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Create a layer tree having horizontal scrollable root and a vertical
  // scrollable child.
  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                     LayerIntRect(0, 0, 100, 50)};
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 200, 100));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 100, 200));

  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  // A vertical pan gesture on the child scroller which should not be handed
  // off the root APZC because the root APZC is not scrollable vertically.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 20),
             ScreenPoint(0, -2), mcc->Time());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
  EXPECT_TRUE(ApzcOf(layers[1])->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTesterMock,
       RetriggerCancelledOverscrollAnimationByNewPanGesture) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Create a layer tree having vertical scrollable root and a horizontal
  // scrollable child.
  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                     LayerIntRect(0, 0, 100, 50)};
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 200));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 200, 50));

  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  ScreenIntPoint panPoint(50, 20);
  // A vertical pan gesture on the child scroller which should be handed off the
  // root APZC.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, panPoint,
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());

  // The root APZC should be overscrolled and the child APZC should not be.
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_FALSE(ApzcOf(layers[1])->IsOverscrolled());

  mcc->AdvanceByMillis(10);

  // Make sure the root APZC is still overscrolled.
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Start a new horizontal pan gesture on the child scroller which should be
  // handled by the child APZC now.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  APZEventResult result = PanGesture(PanGestureInput::PANGESTURE_START, manager,
                                     panPoint, ScreenPoint(-2, 0), mcc->Time());
  // The above horizontal pan start event was flagged as "this event may trigger
  // swipe" and either the root scrollable frame or the horizontal child
  // scrollable frame is not scrollable in the pan start direction, thus the pan
  // start event run into the short circuit path for swipe-to-navigation in
  // InputQueue::ReceivePanGestureInput, which means it's waiting for the
  // content response, so we need to respond explicitly here.
  manager->ContentReceivedInputBlock(result.mInputBlockId, false);
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(-10, 0), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());

  // Now both APZCs should be overscrolled.
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_TRUE(ApzcOf(layers[1])->IsOverscrolled());

  // Sample all animations until all of them have been finished.
  while (SampleAnimationsOnce());

  // After the animations finished, all overscrolled states should have been
  // restored.
  EXPECT_FALSE(rootApzc->IsOverscrolled());
  EXPECT_FALSE(ApzcOf(layers[1])->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTesterMock, RetriggeredOverscrollAnimationVelocity) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Setup two nested vertical scrollable frames.
  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                     LayerIntRect(0, 0, 100, 50)};
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 200));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 100, 200));

  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  ScreenIntPoint panPoint(50, 20);
  // A vertical upward pan gesture on the child scroller which should be handed
  // off the root APZC.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, panPoint,
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());

  // The root APZC should be overscrolled and the child APZC should not be.
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_FALSE(ApzcOf(layers[1])->IsOverscrolled());

  mcc->AdvanceByMillis(10);

  // Make sure the root APZC is still overscrolled and there's an overscroll
  // animation.
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrollAnimationRunning());

  // And make sure the overscroll animation's velocity is a certain amount in
  // the upward direction.
  EXPECT_LT(rootApzc->GetVelocityVector().y, 0);

  // Start a new downward pan gesture on the child scroller which
  // should be handled by the child APZC now.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, panPoint,
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(10);
  // The new pan-start gesture stops the overscroll animation at this moment.
  EXPECT_TRUE(!rootApzc->IsOverscrollAnimationRunning());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(10);
  // There's no overscroll animation yet even if the root APZC is still
  // overscrolled.
  EXPECT_TRUE(!rootApzc->IsOverscrollAnimationRunning());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 10), mcc->Time());

  // Now an overscroll animation should have been triggered by the pan-end
  // gesture.
  EXPECT_TRUE(rootApzc->IsOverscrollAnimationRunning());
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  // And the newly created overscroll animation's positions should never exceed
  // 0.
  while (SampleAnimationsOnce()) {
    EXPECT_LE(rootApzc->GetOverscrollAmount().y, 0);
  }
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTesterMock, OverscrollIntoPreventDefault) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  const char* treeShape = "x";
  LayerIntRect layerVisibleRects[] = {LayerIntRect(0, 0, 100, 100)};
  CreateScrollData(treeShape, layerVisibleRects);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 200));

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);

  // Start a pan gesture a few pixels below the 20px DTC region.
  ScreenIntPoint cursorLocation(10, 25);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  APZEventResult result =
      PanGesture(PanGestureInput::PANGESTURE_START, manager, cursorLocation,
                 ScreenPoint(0, -2), mcc->Time());

  // At this point, we should be overscrolled.
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Pan further, until the DTC region is under the cursor.
  // Note that, due to ApplyResistance(), we need a large input delta to cause a
  // visual transform enough to bridge the 5px to the DTC region.
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, cursorLocation,
             ScreenPoint(0, -100), mcc->Time());

  // At this point, we are still overscrolled. Record the overscroll amount.
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  float overscrollY = rootApzc->GetOverscrollAmount().y;

  // Send a content response with preventDefault = true.
  manager->SetAllowedTouchBehavior(result.mInputBlockId,
                                   {AllowedTouchBehavior::VERTICAL_PAN});
  manager->SetTargetAPZC(result.mInputBlockId, {result.mTargetGuid});
  manager->ContentReceivedInputBlock(result.mInputBlockId,
                                     /*aPreventDefault=*/true);

  // The content response has the effect of interrupting the input block
  // but no processing happens yet (as there are no events in the block).
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_EQ(overscrollY, rootApzc->GetOverscrollAmount().y);

  // Send one more pan event. This starts a new, *unconfirmed* input block
  // (via the "transmogrify" codepath).
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID,
                     {CompositorHitTestFlags::eVisibleToHitTest,
                      CompositorHitTestFlags::eIrregularArea});
  result = PanGesture(PanGestureInput::PANGESTURE_PAN, manager, cursorLocation,
                      ScreenPoint(0, -10), mcc->Time());

  // No overscroll occurs (the event is waiting in the queue for confirmation).
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_EQ(overscrollY, rootApzc->GetOverscrollAmount().y);

  // preventDefault the new event as well
  manager->SetAllowedTouchBehavior(result.mInputBlockId,
                                   {AllowedTouchBehavior::VERTICAL_PAN});
  manager->SetTargetAPZC(result.mInputBlockId, {result.mTargetGuid});
  manager->ContentReceivedInputBlock(result.mInputBlockId,
                                     /*aPreventDefault=*/true);

  // This should trigger clearing the overscrolling and resetting the state.
  EXPECT_FALSE(rootApzc->IsOverscrolled());
  rootApzc->AssertStateIsReset();

  // If there are momentum events after this point, they should not cause
  // further scrolling or overscorll.
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  result = PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager,
                      cursorLocation, ScreenPoint(0, -100), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  result = PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
                      cursorLocation, ScreenPoint(0, -100), mcc->Time());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
  EXPECT_EQ(rootApzc->GetFrameMetrics().GetVisualScrollOffset(),
            CSSPoint(0, 0));
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTesterMock, StuckInOverscroll_Bug1810935) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  using ViewID = ScrollableLayerGuid::ViewID;
  ViewID rootScrollId = ScrollableLayerGuid::START_SCROLL_ID;
  ViewID subframeScrollId = ScrollableLayerGuid::START_SCROLL_ID + 1;

  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRects[] = {LayerIntRect(0, 0, 100, 100),
                                      LayerIntRect(50, 0, 50, 100)};
  CreateScrollData(treeShape, layerVisibleRects);
  SetScrollableFrameMetrics(root, rootScrollId, CSSRect(0, 0, 100, 200));
  SetScrollableFrameMetrics(layers[1], subframeScrollId,
                            CSSRect(0, 0, 50, 200));
  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  auto* subframeApzc = ApzcOf(layers[1]);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  // Try to scroll upwards over the subframe.
  ScreenIntPoint panPoint(75, 50);
  QueueMockHitResult(subframeScrollId);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, panPoint,
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(subframeScrollId);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, -50), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(subframeScrollId);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());

  // The root APZC should be overscrolled. (The subframe APZC should be be.)
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_FALSE(subframeApzc->IsOverscrolled());

  // Give the overscroll animation on the root a chance to start.
  mcc->AdvanceByMillis(10);
  EXPECT_TRUE(rootApzc->IsOverscrollAnimationRunning());

  // Scroll the subframe downwards, with a large delta.
  QueueMockHitResult(subframeScrollId);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, panPoint,
             ScreenPoint(0, 50), mcc->Time());

  // Already after the first event, the overscroll animation should be
  // interrupted.
  EXPECT_FALSE(rootApzc->IsOverscrollAnimationRunning());

  // Cotninue the downward scroll gesture.
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(subframeScrollId);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 100), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(subframeScrollId);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 100), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(subframeScrollId);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 100), mcc->Time());
  mcc->AdvanceByMillis(10);
  QueueMockHitResult(subframeScrollId);
  // Important: pass aSimulateMomentum=true for the pan-end to exercise the bug.
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time(), MODIFIER_NONE,
             /*aSimulateMomentum=*/true);

  // The root and the subframe should both be overscrolled.
  EXPECT_TRUE(rootApzc->IsOverscrolled());
  EXPECT_TRUE(subframeApzc->IsOverscrolled());

  // Sample animations until all of them have been finished.
  while (SampleAnimationsOnce());

  // All overscrolled APZCs should have snapped back.
  EXPECT_FALSE(rootApzc->IsOverscrolled());
  EXPECT_FALSE(subframeApzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Not valid on Android
// Tests that the scroll offset is shifted with the overscroll amount when the
// content scroll range got expaned.
TEST_F(APZCOverscrollTester, FillOutGutterWhilePanning) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Scroll to the bottom edge.
  ScrollMetadata metadata = apzc->GetScrollMetadata();
  metadata.GetMetrics().SetLayoutScrollOffset(
      CSSPoint(0, GetScrollRange().YMost()));
  nsTArray<ScrollPositionUpdate> scrollUpdates;
  scrollUpdates.AppendElement(ScrollPositionUpdate::NewScroll(
      ScrollOrigin::Other,
      CSSPoint::ToAppUnits(CSSPoint(0, GetScrollRange().YMost()))));
  metadata.SetScrollUpdates(scrollUpdates);
  metadata.GetMetrics().SetScrollGeneration(
      scrollUpdates.LastElement().GetGeneration());
  apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                            /*aThisLayerTreeUpdated=*/true);

  CSSPoint scrollOffset = metadata.GetMetrics().GetLayoutScrollOffset();

  // Start panning to overscroll the content.
  Pan(apzc, 20, 10, PanOptions::KeepFingerDown);
  EXPECT_TRUE(apzc->IsOverscrolled());
  float overscrollY = apzc->GetOverscrollAmount().y;
  EXPECT_GT(overscrollY, 0);

  // Expand the content scroll range.
  metadata = apzc->GetScrollMetadata();
  FrameMetrics& metrics = metadata.GetMetrics();
  const CSSRect& scrollableRect = metrics.GetScrollableRect();
  metrics.SetScrollableRect(scrollableRect +
                            CSSSize(0, scrollableRect.height + 10));
  apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                            /*aThisLayerTreeUpdated=*/true);

  // Now that the scroll position was shifted with the overscroll amount.
  EXPECT_EQ(apzc->GetScrollMetadata().GetMetrics().GetVisualScrollOffset().y,
            scrollOffset.y + overscrollY);
  EXPECT_FALSE(apzc->IsOverscrolled());
}

// Similar to FillOutGutterWhilePanning but expanding the content while an
// overscroll animation is running.
TEST_F(APZCOverscrollTester, FillOutGutterWhileAnimating) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Scroll to the bottom edge.
  ScrollMetadata metadata = apzc->GetScrollMetadata();
  metadata.GetMetrics().SetLayoutScrollOffset(
      CSSPoint(0, GetScrollRange().YMost()));
  nsTArray<ScrollPositionUpdate> scrollUpdates;
  scrollUpdates.AppendElement(ScrollPositionUpdate::NewScroll(
      ScrollOrigin::Other,
      CSSPoint::ToAppUnits(CSSPoint(0, GetScrollRange().YMost()))));
  metadata.SetScrollUpdates(scrollUpdates);
  metadata.GetMetrics().SetScrollGeneration(
      scrollUpdates.LastElement().GetGeneration());
  apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                            /*aThisLayerTreeUpdated=*/true);

  CSSPoint scrollOffset = metadata.GetMetrics().GetLayoutScrollOffset();

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 20), mcc->Time());
  mcc->AdvanceByMillis(5);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 60),
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);

  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  float overscrollY = apzc->GetOverscrollAmount().y;
  EXPECT_GT(overscrollY, 0);

  // Expand the content scroll range.
  metadata = apzc->GetScrollMetadata();
  FrameMetrics& metrics = metadata.GetMetrics();
  const CSSRect& scrollableRect = metrics.GetScrollableRect();
  metrics.SetScrollableRect(scrollableRect +
                            CSSSize(0, scrollableRect.height + 10));
  apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                            /*aThisLayerTreeUpdated=*/true);

  // Now that the scroll position was shifted with the overscroll amount.
  EXPECT_EQ(apzc->GetScrollMetadata().GetMetrics().GetVisualScrollOffset().y,
            scrollOffset.y + overscrollY);
  EXPECT_FALSE(apzc->IsOverscrolled());
}
#endif

// Test that a programmatic scroll animation does NOT trigger overscroll.
TEST_F(APZCOverscrollTester, ProgrammaticScroll) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Send a SmoothMsd scroll update to a destination far outside of the
  // scroll range (here, y=100000). This probably shouldn't happen in the
  // first place, but even if it does for whatever reason, the smooth scroll
  // should not trigger overscroll.
  ScrollMetadata metadata = apzc->GetScrollMetadata();
  nsTArray<ScrollPositionUpdate> scrollUpdates;
  scrollUpdates.AppendElement(ScrollPositionUpdate::NewSmoothScroll(
      ScrollMode::SmoothMsd, ScrollOrigin::Other,
      CSSPoint::ToAppUnits(CSSPoint(0, 100000)), ScrollTriggeredByScript::Yes,
      nullptr));
  metadata.SetScrollUpdates(scrollUpdates);
  metadata.GetMetrics().SetScrollGeneration(
      scrollUpdates.LastElement().GetGeneration());
  apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                            /*aThisLayerTreeUpdated=*/true);

  apzc->AssertStateIsSmoothMsdScroll();

  while (SampleAnimationOneFrame()) {
    EXPECT_FALSE(apzc->IsOverscrolled());
  }
}
