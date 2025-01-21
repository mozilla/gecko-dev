/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ElementAnimationData_h
#define mozilla_ElementAnimationData_h

#include "mozilla/UniquePtr.h"
#include "mozilla/PseudoStyleType.h"
#include "nsTHashMap.h"

class nsCycleCollectionTraversalCallback;

namespace mozilla {
class EffectSet;
template <typename Animation>
class AnimationCollection;
template <typename TimelineType>
class TimelineCollection;
namespace dom {
class Element;
class CSSAnimation;
class CSSTransition;
class ProgressTimelineScheduler;
class ScrollTimeline;
class ViewTimeline;
}  // namespace dom
using CSSAnimationCollection = AnimationCollection<dom::CSSAnimation>;
using CSSTransitionCollection = AnimationCollection<dom::CSSTransition>;
using ScrollTimelineCollection = TimelineCollection<dom::ScrollTimeline>;
using ViewTimelineCollection = TimelineCollection<dom::ViewTimeline>;

// The animation data for a given element (and its pseudo-elements).
class ElementAnimationData {
  struct PerElementOrPseudoData {
    UniquePtr<EffectSet> mEffectSet;
    UniquePtr<CSSAnimationCollection> mAnimations;
    UniquePtr<CSSTransitionCollection> mTransitions;

    // Note: scroll-timeline-name is applied to elements which could be
    // scroll containers, or replaced elements. view-timeline-name is applied to
    // all elements. However, the named timeline is referenceable in
    // animation-timeline by the tree order scope.
    // Spec: https://drafts.csswg.org/scroll-animations-1/#timeline-scope.
    //
    // So it should be fine to create timeline objects only on the elements and
    // pseudo elements which support animations.
    UniquePtr<ScrollTimelineCollection> mScrollTimelines;
    UniquePtr<ViewTimelineCollection> mViewTimelines;

    // This is different from |mScrollTimelines|. We use this to schedule all
    // scroll-driven animations (which use anonymous/named scroll timelines or
    // anonymous/name view timelines) for a specific scroll source (which is the
    // element with ScrollContainerFrame).
    //
    // TimelineCollection owns and manages the named progress timeline generated
    // by specifying scroll-timeline-name property and view-timeline-name
    // property on this element. However, the anonymous progress timelines (e.g.
    // animation-timeline:scroll()) are owned by Animation objects only.
    //
    // Note:
    // 1. For named scroll timelines. The element which specifies
    //    scroll-timeline-name is the scroll source. However, for named view
    //    timelines, the element which specifies view-timeline-name may not be
    //    the scroll source because we use its nearest scroll container as the
    //    scroll source.
    // 2. For anonymous progress timelines, we don't keep their timeline obejcts
    //    in TimelineCollection.
    // So, per 1) and 2), we use |mProgressTimelineScheduler| for the scroll
    // source element to schedule scroll-driven animations.
    UniquePtr<dom::ProgressTimelineScheduler> mProgressTimelineScheduler;

    PerElementOrPseudoData();
    ~PerElementOrPseudoData();

    EffectSet& DoEnsureEffectSet();
    CSSTransitionCollection& DoEnsureTransitions(dom::Element&,
                                                 const PseudoStyleRequest&);
    CSSAnimationCollection& DoEnsureAnimations(dom::Element&,
                                               const PseudoStyleRequest&);
    ScrollTimelineCollection& DoEnsureScrollTimelines(
        dom::Element&, const PseudoStyleRequest&);
    ViewTimelineCollection& DoEnsureViewTimelines(dom::Element&,
                                                  const PseudoStyleRequest&);
    dom::ProgressTimelineScheduler& DoEnsureProgressTimelineScheduler();

    bool IsEmpty() const {
      return !mEffectSet && !mAnimations && !mTransitions &&
             !mScrollTimelines && !mViewTimelines &&
             !mProgressTimelineScheduler;
    }

    void Traverse(nsCycleCollectionTraversalCallback&);
  };

  PerElementOrPseudoData mElementData;

  using PseudoData =
      nsTHashMap<PseudoStyleRequestHashKey, UniquePtr<PerElementOrPseudoData>>;
  PseudoData mPseudoData;
  // Avoid remove hash entry while other people are still using it.
  bool mIsClearingPseudoData = false;

  const PerElementOrPseudoData* GetData(
      const PseudoStyleRequest& aRequest) const {
    switch (aRequest.mType) {
      case PseudoStyleType::NotPseudo:
        return &mElementData;
      case PseudoStyleType::before:
      case PseudoStyleType::after:
      case PseudoStyleType::marker:
      case PseudoStyleType::viewTransition:
      case PseudoStyleType::viewTransitionGroup:
      case PseudoStyleType::viewTransitionImagePair:
      case PseudoStyleType::viewTransitionOld:
      case PseudoStyleType::viewTransitionNew:
        return GetPseudoData(aRequest);
      default:
        MOZ_ASSERT_UNREACHABLE(
            "Should not try to get animation effects for "
            "a pseudo other that :before, :after, ::marker, or view transition "
            "pseudo-elements");
        break;
    }
    return nullptr;
  }

  PerElementOrPseudoData& GetOrCreateData(const PseudoStyleRequest& aRequest) {
    switch (aRequest.mType) {
      case PseudoStyleType::NotPseudo:
        break;
      case PseudoStyleType::before:
      case PseudoStyleType::after:
      case PseudoStyleType::marker:
      case PseudoStyleType::viewTransition:
      case PseudoStyleType::viewTransitionGroup:
      case PseudoStyleType::viewTransitionImagePair:
      case PseudoStyleType::viewTransitionOld:
      case PseudoStyleType::viewTransitionNew:
        return GetOrCreatePseudoData(aRequest);
      default:
        MOZ_ASSERT_UNREACHABLE(
            "Should not try to get animation effects for "
            "a pseudo other that :before, :after, ::marker, or view transition "
            "pseudo-elements");
        break;
    }
    return mElementData;
  }

  const PerElementOrPseudoData* GetPseudoData(
      const PseudoStyleRequest& aRequest) const;
  PerElementOrPseudoData& GetOrCreatePseudoData(
      const PseudoStyleRequest& aRequest);
  void MaybeClearEntry(PseudoData::LookupResult<PseudoData&>&& aEntry);

  // |aFn| is the removal function which accepts only |PerElementOrPseudoData&|
  // as the parameter.
  template <typename Fn>
  void WithDataForRemoval(const PseudoStyleRequest& aRequest, Fn&& aFn);

 public:
  void Traverse(nsCycleCollectionTraversalCallback&);

  void ClearAllAnimationCollections();
  void ClearAllPseudos(bool aOnlyViewTransitions);
  void ClearViewTransitionPseudos() { ClearAllPseudos(true); }

  EffectSet* GetEffectSetFor(const PseudoStyleRequest& aRequest) const {
    if (auto* data = GetData(aRequest)) {
      return data->mEffectSet.get();
    }
    return nullptr;
  }

  void ClearEffectSetFor(const PseudoStyleRequest& aRequest);

  EffectSet& EnsureEffectSetFor(const PseudoStyleRequest& aRequest) {
    auto& data = GetOrCreateData(aRequest);
    if (auto* set = data.mEffectSet.get()) {
      return *set;
    }
    return data.DoEnsureEffectSet();
  }

  CSSTransitionCollection* GetTransitionCollection(
      const PseudoStyleRequest& aRequest) const {
    if (auto* data = GetData(aRequest)) {
      return data->mTransitions.get();
    }
    return nullptr;
  }

  void ClearTransitionCollectionFor(const PseudoStyleRequest& aRequest);

  CSSTransitionCollection& EnsureTransitionCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = GetOrCreateData(aRequest);
    if (auto* collection = data.mTransitions.get()) {
      return *collection;
    }
    return data.DoEnsureTransitions(aOwner, aRequest);
  }

  CSSAnimationCollection* GetAnimationCollection(
      const PseudoStyleRequest& aRequest) const {
    if (auto* data = GetData(aRequest)) {
      return data->mAnimations.get();
    }
    return nullptr;
  }

  void ClearAnimationCollectionFor(const PseudoStyleRequest& aRequest);

  CSSAnimationCollection& EnsureAnimationCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = GetOrCreateData(aRequest);
    if (auto* collection = data.mAnimations.get()) {
      return *collection;
    }
    return data.DoEnsureAnimations(aOwner, aRequest);
  }

  ScrollTimelineCollection* GetScrollTimelineCollection(
      const PseudoStyleRequest& aRequest) const {
    if (auto* data = GetData(aRequest)) {
      return data->mScrollTimelines.get();
    }
    return nullptr;
  }

  void ClearScrollTimelineCollectionFor(const PseudoStyleRequest& aRequest);

  ScrollTimelineCollection& EnsureScrollTimelineCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = GetOrCreateData(aRequest);
    if (auto* collection = data.mScrollTimelines.get()) {
      return *collection;
    }
    return data.DoEnsureScrollTimelines(aOwner, aRequest);
  }

  ViewTimelineCollection* GetViewTimelineCollection(
      const PseudoStyleRequest& aRequest) const {
    if (auto* data = GetData(aRequest)) {
      return data->mViewTimelines.get();
    }
    return nullptr;
  }

  void ClearViewTimelineCollectionFor(const PseudoStyleRequest& aRequest);

  ViewTimelineCollection& EnsureViewTimelineCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = GetOrCreateData(aRequest);
    if (auto* collection = data.mViewTimelines.get()) {
      return *collection;
    }
    return data.DoEnsureViewTimelines(aOwner, aRequest);
  }

  dom::ProgressTimelineScheduler* GetProgressTimelineScheduler(
      const PseudoStyleRequest& aRequest) const {
    if (auto* data = GetData(aRequest)) {
      return data->mProgressTimelineScheduler.get();
    }
    return nullptr;
  }

  void ClearProgressTimelineScheduler(const PseudoStyleRequest& aRequest);

  dom::ProgressTimelineScheduler& EnsureProgressTimelineScheduler(
      const PseudoStyleRequest& aRequest) {
    auto& data = GetOrCreateData(aRequest);
    if (auto* collection = data.mProgressTimelineScheduler.get()) {
      return *collection;
    }
    return data.DoEnsureProgressTimelineScheduler();
  }

  ElementAnimationData() = default;
};

}  // namespace mozilla

#endif
