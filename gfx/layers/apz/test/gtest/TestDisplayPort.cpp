/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "apz/src/AsyncPanZoomController.h"
#include "mozilla/StaticPrefs_apz.h"
#include "gtest/gtest.h"

using namespace mozilla;

// Tests that display port size is monotonically increased depending on the
// composition size.
TEST(DisplayPortTest, MonotonicIncrease)
{
  CSSSize compositionSize(1000, 1000);
  CSSToScreenScale2D dPPerCSS = CSSToScreenScale2D(1.0, 1.0);

  CSSSize previousDisplayport;
  for (int32_t height = 100; height <= 3000; height++) {
    compositionSize.height = CSSCoord(height);
    CSSSize displayport =
        layers::AsyncPanZoomController::CalculateDisplayPortSize(
            compositionSize, CSSPoint(),
            layers::AsyncPanZoomController::ZoomInProgress::No, dPPerCSS);
    if (!previousDisplayport.IsEmpty()) {
      EXPECT_GT(displayport.height, previousDisplayport.height);
    }
    previousDisplayport = displayport;
  }
}
