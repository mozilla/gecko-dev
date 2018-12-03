/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersTypes.h"

namespace mozilla {
namespace layers {

EventRegions::EventRegions(const nsIntRegion& aHitRegion,
                           const nsIntRegion& aMaybeHitRegion,
                           const nsIntRegion& aDispatchToContentRegion,
                           const nsIntRegion& aNoActionRegion,
                           const nsIntRegion& aHorizontalPanRegion,
                           const nsIntRegion& aVerticalPanRegion,
                           bool aDTCRequiresTargetConfirmation) {
  mHitRegion = aHitRegion;
  mNoActionRegion = aNoActionRegion;
  mHorizontalPanRegion = aHorizontalPanRegion;
  mVerticalPanRegion = aVerticalPanRegion;
  // Points whose hit-region status we're not sure about need to be dispatched
  // to the content thread. If a point is in both maybeHitRegion and hitRegion
  // then it's not a "maybe" any more, and doesn't go into the dispatch-to-
  // content region.
  mDispatchToContentHitRegion.Sub(aMaybeHitRegion, mHitRegion);
  mDispatchToContentHitRegion.OrWith(aDispatchToContentRegion);
  mHitRegion.OrWith(aMaybeHitRegion);
  mDTCRequiresTargetConfirmation = aDTCRequiresTargetConfirmation;
}

}  // namespace layers
}  // namespace mozilla
