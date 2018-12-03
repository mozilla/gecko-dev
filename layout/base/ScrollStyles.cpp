/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScrollStyles.h"
#include "nsStyleStruct.h"  // for nsStyleDisplay and nsStyleBackground::Position

namespace mozilla {

ScrollStyles::ScrollStyles(uint8_t aH, uint8_t aV,
                           const nsStyleDisplay* aDisplay)
    : mHorizontal(aH),
      mVertical(aV),
      mScrollBehavior(aDisplay->mScrollBehavior),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY),
      mScrollSnapTypeX(aDisplay->mScrollSnapTypeX),
      mScrollSnapTypeY(aDisplay->mScrollSnapTypeY),
      mScrollSnapPointsX(aDisplay->mScrollSnapPointsX),
      mScrollSnapPointsY(aDisplay->mScrollSnapPointsY),
      mScrollSnapDestinationX(aDisplay->mScrollSnapDestination.mXPosition),
      mScrollSnapDestinationY(aDisplay->mScrollSnapDestination.mYPosition) {}

ScrollStyles::ScrollStyles(const nsStyleDisplay* aDisplay)
    : mHorizontal(aDisplay->mOverflowX),
      mVertical(aDisplay->mOverflowY),
      mScrollBehavior(aDisplay->mScrollBehavior),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY),
      mScrollSnapTypeX(aDisplay->mScrollSnapTypeX),
      mScrollSnapTypeY(aDisplay->mScrollSnapTypeY),
      mScrollSnapPointsX(aDisplay->mScrollSnapPointsX),
      mScrollSnapPointsY(aDisplay->mScrollSnapPointsY),
      mScrollSnapDestinationX(aDisplay->mScrollSnapDestination.mXPosition),
      mScrollSnapDestinationY(aDisplay->mScrollSnapDestination.mYPosition) {}

}  // namespace mozilla
