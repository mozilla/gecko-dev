/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/StackingContextHelper.h"

#include "UnitTransforms.h"
#include "nsDisplayList.h"

namespace mozilla {
namespace layers {

StackingContextHelper::StackingContextHelper()
    : mBuilder(nullptr),
      mScale(1.0f, 1.0f),
      mAffectsClipPositioning(false),
      mIsPreserve3D(false),
      mRasterizeLocally(false) {
  // mOrigin remains at 0,0
}

StackingContextHelper::StackingContextHelper(
    const StackingContextHelper& aParentSC, const ActiveScrolledRoot* aAsr,
    wr::DisplayListBuilder& aBuilder, const nsTArray<wr::WrFilterOp>& aFilters,
    const LayoutDeviceRect& aBounds, const gfx::Matrix4x4* aBoundTransform,
    const wr::WrAnimationProperty* aAnimation, const float* aOpacityPtr,
    const gfx::Matrix4x4* aTransformPtr, const gfx::Matrix4x4* aPerspectivePtr,
    const gfx::CompositionOp& aMixBlendMode, bool aBackfaceVisible,
    bool aIsPreserve3D,
    const Maybe<nsDisplayTransform*>& aDeferredTransformItem,
    const wr::WrClipId* aClipNodeId, bool aAnimated)
    : mBuilder(&aBuilder),
      mScale(1.0f, 1.0f),
      mDeferredTransformItem(aDeferredTransformItem),
      mIsPreserve3D(aIsPreserve3D),
      mRasterizeLocally(aAnimated || aParentSC.mRasterizeLocally) {
  // Compute scale for fallback rendering. We don't try to guess a scale for 3d
  // transformed items
  gfx::Matrix transform2d;
  if (aBoundTransform && aBoundTransform->CanDraw2D(&transform2d) &&
      !aPerspectivePtr && !aParentSC.mIsPreserve3D) {
    mInheritedTransform = transform2d * aParentSC.mInheritedTransform;
    mScale = mInheritedTransform.ScaleFactors(true);
    if (aAnimated) {
      mSnappingSurfaceTransform =
          gfx::Matrix::Scaling(mScale.width, mScale.height);
    } else {
      mSnappingSurfaceTransform =
          transform2d * aParentSC.mSnappingSurfaceTransform;
    }
  } else {
    mInheritedTransform = aParentSC.mInheritedTransform;
    mScale = aParentSC.mScale;
  }

  auto rasterSpace =
      mRasterizeLocally
          ? wr::RasterSpace::Local(std::max(mScale.width, mScale.height))
          : wr::RasterSpace::Screen();

  mReferenceFrameId = mBuilder->PushStackingContext(
      wr::ToLayoutRect(aBounds), aClipNodeId, aAnimation, aOpacityPtr,
      aTransformPtr,
      aIsPreserve3D ? wr::TransformStyle::Preserve3D : wr::TransformStyle::Flat,
      aPerspectivePtr, wr::ToMixBlendMode(aMixBlendMode), aFilters,
      aBackfaceVisible, rasterSpace);

  mAffectsClipPositioning =
      mReferenceFrameId.isSome() || (aBounds.TopLeft() != LayoutDevicePoint());

  // If the parent stacking context has a deferred transform item, inherit it
  // into this stacking context, as long as the ASR hasn't changed. Refer to
  // the comments on StackingContextHelper::mDeferredTransformItem for an
  // explanation of what goes in these fields.
  if (aParentSC.mDeferredTransformItem &&
      aAsr == (*aParentSC.mDeferredTransformItem)->GetActiveScrolledRoot()) {
    if (mDeferredTransformItem) {
      // If we are deferring another transform, put the combined transform from
      // all the ancestor deferred items into mDeferredAncestorTransform
      mDeferredAncestorTransform = aParentSC.GetDeferredTransformMatrix();
    } else {
      // We are not deferring another transform, so we can just inherit the
      // parent stacking context's deferred data without any modification.
      mDeferredTransformItem = aParentSC.mDeferredTransformItem;
      mDeferredAncestorTransform = aParentSC.mDeferredAncestorTransform;
    }
  }
}

StackingContextHelper::~StackingContextHelper() {
  if (mBuilder) {
    mBuilder->PopStackingContext(mReferenceFrameId.isSome());
  }
}

const Maybe<nsDisplayTransform*>&
StackingContextHelper::GetDeferredTransformItem() const {
  return mDeferredTransformItem;
}

Maybe<gfx::Matrix4x4> StackingContextHelper::GetDeferredTransformMatrix()
    const {
  if (mDeferredTransformItem) {
    // See the comments on StackingContextHelper::mDeferredTransformItem for
    // an explanation of what's stored in mDeferredTransformItem and
    // mDeferredAncestorTransform. Here we need to return the combined transform
    // transform from all the deferred ancestors, including
    // mDeferredTransformItem.
    gfx::Matrix4x4 result =
        (*mDeferredTransformItem)->GetTransform().GetMatrix();
    if (mDeferredAncestorTransform) {
      result = *mDeferredAncestorTransform * result;
    }
    return Some(result);
  } else {
    return Nothing();
  }
}

}  // namespace layers
}  // namespace mozilla
