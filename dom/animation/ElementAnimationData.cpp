/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ElementAnimationData.h"
#include "mozilla/AnimationCollection.h"
#include "mozilla/TimelineCollection.h"
#include "mozilla/EffectSet.h"
#include "mozilla/dom/CSSTransition.h"
#include "mozilla/dom/CSSAnimation.h"
#include "mozilla/dom/ScrollTimeline.h"
#include "mozilla/dom/ViewTimeline.h"

namespace mozilla {

const ElementAnimationData::PerElementOrPseudoData*
ElementAnimationData::GetPseudoData(const PseudoStyleRequest& aRequest) const {
  MOZ_ASSERT(!aRequest.IsNotPseudo(), "Only for pseudo-elements");

  auto entry = mPseudoData.Lookup(aRequest);
  if (!entry) {
    return nullptr;
  }
  MOZ_ASSERT(*entry, "Should always have a valid UniquePtr");
  return entry->get();
}

ElementAnimationData::PerElementOrPseudoData&
ElementAnimationData::GetOrCreatePseudoData(
    const PseudoStyleRequest& aRequest) {
  MOZ_ASSERT(!aRequest.IsNotPseudo(), "Only for pseudo-elements");

  UniquePtr<PerElementOrPseudoData>& data = mPseudoData.LookupOrInsertWith(
      aRequest, [&] { return MakeUnique<PerElementOrPseudoData>(); });
  MOZ_ASSERT(data);
  return *data;
}

void ElementAnimationData::Traverse(nsCycleCollectionTraversalCallback& cb) {
  mElementData.Traverse(cb);
  for (auto& data : mPseudoData.Values()) {
    data->Traverse(cb);
  }
}

void ElementAnimationData::ClearAllAnimationCollections() {
  mElementData.mAnimations = nullptr;
  mElementData.mTransitions = nullptr;
  mElementData.mScrollTimelines = nullptr;
  mElementData.mViewTimelines = nullptr;
  mElementData.mProgressTimelineScheduler = nullptr;
  ClearAllPseudos(false);
}

void ElementAnimationData::ClearAllPseudos(bool aOnlyViewTransitions) {
  if (mPseudoData.IsEmpty()) {
    return;
  }

  mIsClearingPseudoData = true;
  for (auto iter = mPseudoData.Iter(); !iter.Done(); iter.Next()) {
    const auto& data = iter.Data();
    MOZ_ASSERT(data);

    if (aOnlyViewTransitions && !iter.Key().IsViewTransition()) {
      continue;
    }

    // Note: We cannot remove EffectSet because we expect there is a valid
    // EffectSet when unregistering the target.
    // (See KeyframeEffect::UnregisterTarget() for more deatils).
    // So we rely on EffectSet::Destroy() to clear it.
    data->mAnimations = nullptr;
    data->mTransitions = nullptr;
    data->mScrollTimelines = nullptr;
    data->mViewTimelines = nullptr;
    data->mProgressTimelineScheduler = nullptr;

    if (data->IsEmpty()) {
      iter.Remove();
    }
  }
  mIsClearingPseudoData = false;
}

void ElementAnimationData::MaybeClearEntry(
    PseudoData::LookupResult<PseudoData&>&& aEntry) {
  if (mIsClearingPseudoData || !aEntry || !aEntry.Data()->IsEmpty()) {
    return;
  }
  UniquePtr<PerElementOrPseudoData> temp(std::move(*aEntry));
  aEntry.Remove();
}

template <typename Fn>
void ElementAnimationData::WithDataForRemoval(
    const PseudoStyleRequest& aRequest, Fn&& aFn) {
  if (aRequest.IsNotPseudo()) {
    aFn(mElementData);
    return;
  }
  auto entry = mPseudoData.Lookup(aRequest);
  if (!entry) {
    return;
  }
  aFn(**entry);
  MaybeClearEntry(std::move(entry));
}

void ElementAnimationData::ClearEffectSetFor(
    const PseudoStyleRequest& aRequest) {
  WithDataForRemoval(aRequest, [](PerElementOrPseudoData& aData) {
    aData.mEffectSet = nullptr;
  });
}

void ElementAnimationData::ClearTransitionCollectionFor(
    const PseudoStyleRequest& aRequest) {
  if (aRequest.IsNotPseudo()) {
    mElementData.mTransitions = nullptr;
    return;
  }

  auto entry = mPseudoData.Lookup(aRequest);
  if (!entry || !entry.Data()->mTransitions) {
    return;
  }
  // If a KeyframeEffect associated with only the animation in the collection,
  // nullifying the collection may call ClearEffectSetFor(), which may clear the
  // entry if all empty. Therefore, we move the collection out of the data
  // first, and destroy the collection when leaving the function, to make sure
  // |entry| is still valid when calling MaybeClearEntry().
  // Note: It seems MaybeClearEntry() here may be redundant because we always
  // rely on ClearEffectSetFor() to clear the entry. However, we still call it
  // just in case.
  auto autoRemoved(std::move(entry.Data()->mTransitions));
  MaybeClearEntry(std::move(entry));
}

void ElementAnimationData::ClearAnimationCollectionFor(
    const PseudoStyleRequest& aRequest) {
  if (aRequest.IsNotPseudo()) {
    mElementData.mAnimations = nullptr;
    return;
  }

  auto entry = mPseudoData.Lookup(aRequest);
  if (!entry || !entry.Data()->mAnimations) {
    return;
  }

  // If a KeyframeEffect associated with only the animation in the collection,
  // nullifying the collection may call ClearEffectSetFor(), which may clear the
  // entry if all empty. Therefore, we move the collection out of the data
  // first, and destroy the collection when leaving the function, to make sure
  // |entry| is still valid when calling MaybeClearEntry().
  // Note: It seems MaybeClearEntry() here may be redundant because we always
  // rely on ClearEffectSetFor() to clear the entry. However, we still call it
  // just in case.
  auto autoRemoved(std::move(entry.Data()->mAnimations));
  MaybeClearEntry(std::move(entry));
}

void ElementAnimationData::ClearScrollTimelineCollectionFor(
    const PseudoStyleRequest& aRequest) {
  WithDataForRemoval(aRequest, [](PerElementOrPseudoData& aData) {
    aData.mScrollTimelines = nullptr;
  });
}

void ElementAnimationData::ClearViewTimelineCollectionFor(
    const PseudoStyleRequest& aRequest) {
  WithDataForRemoval(aRequest, [](PerElementOrPseudoData& aData) {
    aData.mViewTimelines = nullptr;
  });
}

void ElementAnimationData::ClearProgressTimelineScheduler(
    const PseudoStyleRequest& aRequest) {
  WithDataForRemoval(aRequest, [](PerElementOrPseudoData& aData) {
    aData.mProgressTimelineScheduler = nullptr;
  });
}

ElementAnimationData::PerElementOrPseudoData::PerElementOrPseudoData() =
    default;
ElementAnimationData::PerElementOrPseudoData::~PerElementOrPseudoData() =
    default;

void ElementAnimationData::PerElementOrPseudoData::Traverse(
    nsCycleCollectionTraversalCallback& cb) {
  // We only care about mEffectSet. The animation collections are managed by the
  // pres context and go away when presentation of the document goes away.
  if (mEffectSet) {
    mEffectSet->Traverse(cb);
  }
}

EffectSet& ElementAnimationData::PerElementOrPseudoData::DoEnsureEffectSet() {
  MOZ_ASSERT(!mEffectSet);
  mEffectSet = MakeUnique<EffectSet>();
  return *mEffectSet;
}

CSSTransitionCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureTransitions(
    dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
  MOZ_ASSERT(!mTransitions);
  mTransitions = MakeUnique<CSSTransitionCollection>(aOwner, aRequest);
  return *mTransitions;
}

CSSAnimationCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureAnimations(
    dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
  MOZ_ASSERT(!mAnimations);
  mAnimations = MakeUnique<CSSAnimationCollection>(aOwner, aRequest);
  return *mAnimations;
}

ScrollTimelineCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureScrollTimelines(
    dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
  MOZ_ASSERT(!mScrollTimelines);
  mScrollTimelines = MakeUnique<ScrollTimelineCollection>(aOwner, aRequest);
  return *mScrollTimelines;
}

ViewTimelineCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureViewTimelines(
    dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
  MOZ_ASSERT(!mViewTimelines);
  mViewTimelines = MakeUnique<ViewTimelineCollection>(aOwner, aRequest);
  return *mViewTimelines;
}

dom::ProgressTimelineScheduler& ElementAnimationData::PerElementOrPseudoData::
    DoEnsureProgressTimelineScheduler() {
  MOZ_ASSERT(!mProgressTimelineScheduler);
  mProgressTimelineScheduler = MakeUnique<dom::ProgressTimelineScheduler>();
  return *mProgressTimelineScheduler;
}

}  // namespace mozilla
