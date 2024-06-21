/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"
#include "mozilla/layers/ScrollableLayerGuid.h"

// Test of scenario described in bug 1269067 - that a continuing mouse drag
// doesn't interrupt a wheel scrolling animation
TEST_F(APZCTreeManagerTester, WheelInterruptedByMouseDrag) {
  // Needed because the test uses SmoothWheel()
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);

  // Set up a scrollable layer
  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();
  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // First start the mouse drag
  uint64_t dragBlockId =
      MouseDown(apzc, ScreenIntPoint(5, 5), mcc->Time()).mInputBlockId;
  uint64_t tmpBlockId =
      MouseMove(apzc, ScreenIntPoint(6, 6), mcc->Time()).mInputBlockId;
  EXPECT_EQ(dragBlockId, tmpBlockId);

  // Insert the wheel event, check that it has a new block id
  uint64_t wheelBlockId =
      SmoothWheel(apzc, ScreenIntPoint(6, 6), ScreenPoint(0, 1), mcc->Time())
          .mInputBlockId;
  EXPECT_NE(dragBlockId, wheelBlockId);

  // Continue the drag, check that the block id is the same as before
  tmpBlockId = MouseMove(apzc, ScreenIntPoint(7, 5), mcc->Time()).mInputBlockId;
  EXPECT_EQ(dragBlockId, tmpBlockId);

  // Finish the wheel animation
  apzc->AdvanceAnimationsUntilEnd();

  // Check that it scrolled
  ParentLayerPoint scroll = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);
  EXPECT_EQ(scroll.x, 0);
  EXPECT_EQ(scroll.y, 10);  // We scrolled 1 "line" or 10 pixels
}

// Test of the scenario in bug 1894228, where the touchpad generates a
// mixture of wheel events with horizontal and vertical deltas, and if the
// content is only scrollable in the vertical direction, then an input
// block starting with a wheel event with a horizontal can prevent the
// entire input block from causing any scrolling.
TEST_F(APZCTreeManagerTester, HorizontalDeltaInterferesWithVerticalScrolling) {
  using ViewID = ScrollableLayerGuid::ViewID;
  ViewID rootScrollId = ScrollableLayerGuid::START_SCROLL_ID;
  const char* treeShape = "x";
  LayerIntRect layerVisibleRect[] = {
      LayerIntRect(0, 0, 100, 100),
  };
  CreateScrollData(treeShape, layerVisibleRect);
  // Only vertically scrollable
  SetScrollableFrameMetrics(layers[0], rootScrollId, CSSRect(0, 0, 100, 1000));

  ScopedLayerTreeRegistration registration(LayersId{0}, mcc);
  UpdateHitTestingTree();
  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Configure the APZC to wait for main-thread confirmations before
  // processing events. (This is needed to trigger the buggy codepath.)
  apzc->SetWaitForMainThread();

  // Send a wheel event with a horizontal delta.
  ScreenIntPoint cursorLocation(50, 50);
  uint64_t wheelBlockId1 =
      Wheel(apzc, cursorLocation, ScreenIntPoint(-10, 0), mcc->Time())
          .mInputBlockId;

  // Send a wheel event with a vertical delta.
  uint64_t wheelBlockId2 =
      Wheel(apzc, cursorLocation, ScreenIntPoint(0, 10), mcc->Time())
          .mInputBlockId;

  // Since the wheel block's target APZC has not been confirmed yet, the second
  // event will go into the same block as the first.
  EXPECT_EQ(wheelBlockId1, wheelBlockId2);

  // Confirm the input block.
  manager->ContentReceivedInputBlock(wheelBlockId1, false);
  manager->SetTargetAPZC(wheelBlockId1, {apzc->GetGuid()});

  // We should have scrolled vertically.
  EXPECT_EQ(ParentLayerPoint(0, 10),
            apzc->GetCurrentAsyncScrollOffset(
                AsyncPanZoomController::eForEventHandling));
}
