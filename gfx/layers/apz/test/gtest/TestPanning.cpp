/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "FrameMetrics.h"
#include "InputUtils.h"
#include "gtest/gtest.h"
#include "mozilla/ScrollSnapInfo.h"
#include "mozilla/ServoComputedData.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/ScrollableLayerGuid.h"

class APZCPanningTester : public APZCBasicTester {
 protected:
  void DoPanTest(bool aShouldTriggerScroll, bool aShouldBeConsumed,
                 uint32_t aBehavior) {
    if (aShouldTriggerScroll) {
      // Three repaint request for each pan.
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(6);
    } else {
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);
    }

    int touchStart = 50;
    int touchEnd = 10;
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;

    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(aBehavior);

    // Pan down
    PanAndCheckStatus(apzc, touchStart, touchEnd, aShouldBeConsumed,
                      &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);

    if (aShouldTriggerScroll) {
      EXPECT_EQ(ParentLayerPoint(0, -(touchEnd - touchStart)), pointOut);
      EXPECT_NE(AsyncTransform(), viewTransformOut);
    } else {
      EXPECT_EQ(ParentLayerPoint(), pointOut);
      EXPECT_EQ(AsyncTransform(), viewTransformOut);
    }

    // Clear the fling from the previous pan, or stopping it will
    // consume the next touchstart
    apzc->CancelAnimation();

    // Pan back
    PanAndCheckStatus(apzc, touchEnd, touchStart, aShouldBeConsumed,
                      &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);

    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(AsyncTransform(), viewTransformOut);
  }

  void DoPanWithPreventDefaultTest() {
    MakeApzcWaitForMainThread();

    int touchStart = 50;
    int touchEnd = 10;
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    uint64_t blockId = 0;

    // Pan down
    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(
        mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
    PanAndCheckStatus(apzc, touchStart, touchEnd, true, &allowedTouchBehaviors,
                      &blockId);

    // Send the signal that content has handled and preventDefaulted the touch
    // events. This flushes the event queue.
    apzc->ContentReceivedInputBlock(blockId, true);

    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(AsyncTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }

  void PanWithFling() {
    // Send a pan gesture that triggers a fling animation at the end.
    // Note that we need at least two _PAN events to have enough samples
    // in the velocity tracker to compute a fling velocity.
    PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
               ScreenPoint(0, 2), mcc->Time());
    mcc->AdvanceByMillis(5);
    apzc->AdvanceAnimations(mcc->GetSampleTime());
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
               ScreenPoint(0, 10), mcc->Time());
    mcc->AdvanceByMillis(5);
    apzc->AdvanceAnimations(mcc->GetSampleTime());
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
               ScreenPoint(0, 10), mcc->Time());
    mcc->AdvanceByMillis(5);
    apzc->AdvanceAnimations(mcc->GetSampleTime());
    PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
               ScreenPoint(0, 0), mcc->Time(), MODIFIER_NONE,
               /*aSimulateMomentum=*/true);
  }
};

// In the each of the following 4 pan tests we are performing two pan gestures:
// vertical pan from top to bottom and back - from bottom to top. According to
// the pointer-events/touch-action spec AUTO and PAN_Y touch-action values allow
// vertical scrolling while NONE and PAN_X forbid it. The first parameter of
// DoPanTest method specifies this behavior. However, the events will be marked
// as consumed even if the behavior in PAN_X, because the user could move their
// finger horizontally too - APZ has no way of knowing beforehand and so must
// consume the events.
TEST_F(APZCPanningTester, PanWithTouchActionAuto) {
  // Velocity bias can cause extra repaint requests.
  SCOPED_GFX_PREF_FLOAT("apz.velocity_bias", 0.0);
  DoPanTest(true, true,
            mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN |
                mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(APZCPanningTester, PanWithTouchActionNone) {
  // Velocity bias can cause extra repaint requests.
  SCOPED_GFX_PREF_FLOAT("apz.velocity_bias", 0.0);
  DoPanTest(false, false, 0);
}

TEST_F(APZCPanningTester, PanWithTouchActionPanX) {
  // Velocity bias can cause extra repaint requests.
  SCOPED_GFX_PREF_FLOAT("apz.velocity_bias", 0.0);
  DoPanTest(false, false,
            mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN);
}

TEST_F(APZCPanningTester, PanWithTouchActionPanY) {
  // Velocity bias can cause extra repaint requests.
  SCOPED_GFX_PREF_FLOAT("apz.velocity_bias", 0.0);
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(APZCPanningTester, PanWithPreventDefault) {
  DoPanWithPreventDefaultTest();
}

TEST_F(APZCPanningTester, PanWithHistoricalTouchData) {
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0);

  // Simulate the same pan gesture, in three different ways.
  // We start at y=50, with a 50ms resting period at the start of the pan.
  // Then we accelerate the finger upwards towards y=10, reaching a 10px/10ms
  // velocity towards the end of the panning motion.
  //
  // The first simulation fires touch move events with 10ms gaps.
  // The second simulation skips two of the touch move events, simulating
  // "jank". The third simulation also skips those two events, but reports the
  // missed positions in the following event's historical coordinates.
  //
  // Consequently, the first and third simulation should estimate the same
  // velocities, whereas the second simulation should estimate a different
  // velocity because it is missing data.

  // First simulation: full data

  APZEventResult result = TouchDown(apzc, ScreenIntPoint(0, 50), mcc->Time());
  if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(apzc, result.mInputBlockId);
  }

  mcc->AdvanceByMillis(50);
  result = TouchMove(apzc, ScreenIntPoint(0, 45), mcc->Time());
  mcc->AdvanceByMillis(10);
  result = TouchMove(apzc, ScreenIntPoint(0, 40), mcc->Time());
  mcc->AdvanceByMillis(10);
  result = TouchMove(apzc, ScreenIntPoint(0, 30), mcc->Time());
  mcc->AdvanceByMillis(10);
  result = TouchMove(apzc, ScreenIntPoint(0, 20), mcc->Time());
  result = TouchUp(apzc, ScreenIntPoint(0, 20), mcc->Time());
  auto velocityFromFullDataAsSeparateEvents = apzc->GetVelocityVector();
  apzc->CancelAnimation();

  mcc->AdvanceByMillis(100);

  // Second simulation: partial data

  result = TouchDown(apzc, ScreenIntPoint(0, 50), mcc->Time());
  if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(apzc, result.mInputBlockId);
  }

  mcc->AdvanceByMillis(50);
  result = TouchMove(apzc, ScreenIntPoint(0, 45), mcc->Time());
  mcc->AdvanceByMillis(30);
  result = TouchMove(apzc, ScreenIntPoint(0, 20), mcc->Time());
  result = TouchUp(apzc, ScreenIntPoint(0, 20), mcc->Time());
  auto velocityFromPartialData = apzc->GetVelocityVector();
  apzc->CancelAnimation();

  mcc->AdvanceByMillis(100);

  // Third simulation: full data via historical data

  result = TouchDown(apzc, ScreenIntPoint(0, 50), mcc->Time());
  if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(apzc, result.mInputBlockId);
  }

  mcc->AdvanceByMillis(50);
  result = TouchMove(apzc, ScreenIntPoint(0, 45), mcc->Time());
  mcc->AdvanceByMillis(30);

  MultiTouchInput mti =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  auto singleTouchData = CreateSingleTouchData(0, ScreenIntPoint(0, 20));
  singleTouchData.mHistoricalData.AppendElement(
      SingleTouchData::HistoricalTouchData{
          mcc->Time() - TimeDuration::FromMilliseconds(20),
          ScreenIntPoint(0, 40),
          {},
          {},
          0.0f,
          0.0f});
  singleTouchData.mHistoricalData.AppendElement(
      SingleTouchData::HistoricalTouchData{
          mcc->Time() - TimeDuration::FromMilliseconds(10),
          ScreenIntPoint(0, 30),
          {},
          {},
          0.0f,
          0.0f});
  mti.mTouches.AppendElement(singleTouchData);
  result = apzc->ReceiveInputEvent(mti);

  result = TouchUp(apzc, ScreenIntPoint(0, 20), mcc->Time());
  auto velocityFromFullDataViaHistory = apzc->GetVelocityVector();
  apzc->CancelAnimation();

  EXPECT_EQ(velocityFromFullDataAsSeparateEvents,
            velocityFromFullDataViaHistory);
  EXPECT_NE(velocityFromPartialData, velocityFromFullDataViaHistory);
}

TEST_F(APZCPanningTester, DuplicatePanEndEvents_Bug1833950) {
  // Send a pan gesture that triggers a fling animation at the end.
  PanWithFling();

  // Give the fling animation a chance to start.
  SampleAnimationOnce();
  apzc->AssertStateIsFling();

  // Send a duplicate pan-end event.
  // This test is just intended to check that doing this doesn't
  // trigger an assertion failure in debug mode.
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time(), MODIFIER_NONE,
             /*aSimulateMomentum=*/true);
}

class APZCPanningTesterMock : public APZCTreeManagerTester {
 public:
  APZCPanningTesterMock() { CreateMockHitTester(); }
};

TEST_F(APZCPanningTester, HoldGesture_HoldAndRelease) {
  // Send a pan gesture that triggers a fling animation at the end.
  PanWithFling();

  // Give the fling animation a chance to start.
  SampleAnimationOnce();
  apzc->AssertStateIsFling();

  // Send a PANGESTURE_MAYSTART event, signifying that the fingers went back
  // down on the touchpad.
  PanGesture(PanGestureInput::PANGESTURE_MAYSTART, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // This should have had the effect of cancelling the fling animation.
  apzc->AssertStateIsReset();

  // Send a PANGESTURE_CANCELLED event, signifying that the fingers have been
  // lifted without any scrolling. This should have no effect on the gesture
  // state.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_CANCELLED, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  apzc->AssertStateIsReset();
}

TEST_F(APZCPanningTester, HoldGesture_HoldAndScroll) {
  // Send a pan gesture that triggers a fling animation at the end.
  PanWithFling();

  // Give the fling animation a chance to start.
  SampleAnimationOnce();
  apzc->AssertStateIsFling();

  // Record the scroll offset before the fingers go back.
  float scrollYBefore = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
  EXPECT_GT(scrollYBefore, 0);

  // Send a PANGESTURE_MAYSTART event, signifying that the fingers went back
  // down on the touchpad.
  PanGesture(PanGestureInput::PANGESTURE_MAYSTART, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // This should have had the effect of cancelling the fling animation.
  apzc->AssertStateIsReset();

  // Do actual panning as part of the same gesture.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time(), MODIFIER_NONE,
             /*aSimulateMomentum=*/true);

  // Check that we've done additional scrolling.
  float scrollYAfter = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
  EXPECT_GT(scrollYAfter, scrollYBefore);
}

TEST_F(APZCPanningTesterMock, HoldGesture_ActiveWheelListener) {
  // Explicitly set the content response timeout.
  // The value should be greater than the sum of the AdvanceByMillis()
  // intervals from the MAYSTART event until the content response.
  SCOPED_GFX_PREF_INT("apz.content_response_timeout", 100);

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  ScrollableLayerGuid::ViewID scrollId = ScrollableLayerGuid::START_SCROLL_ID;
  ScreenIntPoint panPoint(50, 80);

  // Simulate an active wheel listener by having the MockHitTester
  // return eApzAwareListeners as the hit-test result for every event.
  gfx::CompositorHitTestInfo dispatchToContent{
      CompositorHitTestFlags::eVisibleToHitTest,
      CompositorHitTestFlags::eApzAwareListeners};

  // Send a MAYSTART. Note that this has zero delta and does not
  // result in an event sent to web content (so it will not itself
  // result in a content response).
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_MAYSTART, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());

#ifdef MOZ_WIDGET_GTK
  // On Linux, send a CANCELLED. This signifies the end of the hold gesture.
  // (On Mac the widget code goes directly to sending START.)
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_CANCELLED, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());
#endif

  // Send a START. This does result in an event sent to web
  // content if there is a nonzero delta.
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(scrollId, dispatchToContent);
  auto startResult = PanGesture(PanGestureInput::PANGESTURE_START, manager,
                                panPoint, ScreenPoint(0, 10), mcc->Time());

  // Send a couple of PAN events.
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 10), mcc->Time());

  // Simulate a content response which allows the scroll.
  manager->SetTargetAPZC(startResult.mInputBlockId, {startResult.mTargetGuid});
  manager->ContentReceivedInputBlock(startResult.mInputBlockId,
                                     /*aPreventDefault=*/false);

  // Check that we did scroll.
  // In the buggy scenario for which this test case is written,
  // the input block for the hold gesture was never confirmed,
  // stalling the input queue so that we don't scroll until that input
  // block is timed out.
  EXPECT_GT(apzc->GetFrameMetrics().GetVisualScrollOffset().y, 0);

  // Clean up by sending an END event.
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());
  apzc->AssertStateIsReset();
}

TEST_F(APZCPanningTesterMock, HoldGesture_PreventDefaultAfterLongHold) {
  // Explicitly set a content response timeout.
  SCOPED_GFX_PREF_INT("apz.content_response_timeout", 20);

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);
  ScrollableLayerGuid::ViewID scrollId = ScrollableLayerGuid::START_SCROLL_ID;
  ScreenIntPoint panPoint(50, 80);

  // Simulate an active wheel listener by having the MockHitTester
  // return eApzAwareListeners as the hit-test result for every event.
  gfx::CompositorHitTestInfo dispatchToContent{
      CompositorHitTestFlags::eVisibleToHitTest,
      CompositorHitTestFlags::eApzAwareListeners};

  // Send a MAYSTART. Note that this has zero delta and does not
  // result in an event sent to web content (so it will not itself
  // result in a content response).
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_MAYSTART, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());

  // Allow enough time to pass for the content response timeout to be reached.
  mcc->AdvanceByMillis(30);

#ifdef MOZ_WIDGET_GTK
  // On Linux, send a CANCELLED. This signifies the end of the hold gesture.
  // (On Mac the widget code goes directly to sending START.)
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_CANCELLED, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());
#endif

  // Send a START. This does result in an event sent to web
  // content if there is a nonzero delta.
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(scrollId, dispatchToContent);
  auto startResult = PanGesture(PanGestureInput::PANGESTURE_START, manager,
                                panPoint, ScreenPoint(0, 10), mcc->Time());

  // Send a couple of PAN events.
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, panPoint,
             ScreenPoint(0, 10), mcc->Time());

  // Simulate a content response which does NOT allow the scroll.
  manager->SetTargetAPZC(startResult.mInputBlockId, {startResult.mTargetGuid});
  manager->ContentReceivedInputBlock(startResult.mInputBlockId,
                                     /*aPreventDefault=*/true);

  // Check that we did NOT scroll.
  // In the buggy scenario for which this test case is written,
  // the hold gesture and the scroll go into the same input block,
  // for which the content response times out during the hold gesture,
  // and we don't wait for the content response for the scroll.
  EXPECT_EQ(apzc->GetFrameMetrics().GetVisualScrollOffset().y, 0);

  // Clean up by sending an END event.
  QueueMockHitResult(scrollId, dispatchToContent);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());
  apzc->AssertStateIsReset();
}

TEST_F(APZCPanningTesterMock, HoldGesture_SubframeTargeting) {
  // Set up a layer tree with a scrollable subframe handing off to a root frame.
  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {
      LayerIntRect(0, 0, 100, 100),
      LayerIntRect(0, 0, 100, 100),
  };
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 100));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 100, 200));
  SetScrollHandoff(layers[1], root);
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();

  auto* rootApzc = ApzcOf(root);
  auto* subframeApzc = ApzcOf(layers[1]);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  // Mark the subframe as overscroll-behavior:none. This is important to
  // trigger the codepath in FindFirstScrollable() that exposes the bug.
  subframeApzc->GetScrollMetadata().SetOverscrollBehavior(
      OverscrollBehaviorInfo::FromStyleConstants(
          StyleOverscrollBehavior::None, StyleOverscrollBehavior::None));

  ScrollableLayerGuid::ViewID subframeScrollId =
      ScrollableLayerGuid::START_SCROLL_ID + 1;
  ScreenIntPoint panPoint(50, 50);

  // Send a MAYSTART. Note that this has zero delta, and causes its input
  // block to be marked as having empty mAllowedScrollDirections because the
  // subframe fails the "can this APZC be scrolled by this event" check
  // and is overscroll-behavior:none.
  QueueMockHitResult(subframeScrollId,
                     CompositorHitTestFlags::eVisibleToHitTest);
  PanGesture(PanGestureInput::PANGESTURE_MAYSTART, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());

#ifdef MOZ_WIDGET_GTK
  // On Linux, send a CANCELLED. This signifies the end of the hold gesture.
  // (On Mac the widget code goes directly to sending START.)
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(subframeScrollId,
                     CompositorHitTestFlags::eVisibleToHitTest);
  PanGesture(PanGestureInput::PANGESTURE_CANCELLED, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());
#endif

  // Send a START. In the buggy scenario, this gets added to the same input
  // block as the MAYSTART, which has been marked as having empty
  // mAllowedScrollDirections, and thus fails to scroll anything.
  mcc->AdvanceByMillis(5);
  QueueMockHitResult(subframeScrollId,
                     CompositorHitTestFlags::eVisibleToHitTest);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, panPoint,
             ScreenPoint(0, 10), mcc->Time());

  // Check that the subframe scrolled.
  EXPECT_GT(subframeApzc->GetFrameMetrics().GetVisualScrollOffset().y, 0);

  // Clean up by sending an END event.
  QueueMockHitResult(subframeScrollId,
                     CompositorHitTestFlags::eVisibleToHitTest);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, panPoint,
             ScreenPoint(0, 0), mcc->Time());
}
