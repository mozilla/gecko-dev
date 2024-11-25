/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ElementAnimationData_h
#define mozilla_ElementAnimationData_h

#include "mozilla/UniquePtr.h"
#include "mozilla/PseudoStyleType.h"

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

    void DoClearEffectSet();
    void DoClearTransitions();
    void DoClearAnimations();
    void DoClearScrollTimelines();
    void DoClearViewTimelines();
    void DoClearProgressTimelineScheduler();

    void Traverse(nsCycleCollectionTraversalCallback&);
  };

  PerElementOrPseudoData mElementData;

  // TODO(emilio): Maybe this should be a hash map eventually, once we allow
  // animating all pseudo-elements.
  // TODO: Bug 1921116. Add view transition data.
  PerElementOrPseudoData mBeforeData;
  PerElementOrPseudoData mAfterData;
  PerElementOrPseudoData mMarkerData;

  const PerElementOrPseudoData& DataFor(
      const PseudoStyleRequest& aRequest) const {
    switch (aRequest.mType) {
      case PseudoStyleType::NotPseudo:
        break;
      case PseudoStyleType::before:
        return mBeforeData;
      case PseudoStyleType::after:
        return mAfterData;
      case PseudoStyleType::marker:
        return mMarkerData;
      case PseudoStyleType::viewTransition:
      case PseudoStyleType::viewTransitionGroup:
      case PseudoStyleType::viewTransitionImagePair:
      case PseudoStyleType::viewTransitionOld:
      case PseudoStyleType::viewTransitionNew:
        // TODO: Bug 1921116. Add view transition data.
        // Some existing view-transtion tests may come here, so we just return
        // mElementData for now to avoid hitting the assertion.
        return mElementData;
      default:
        MOZ_ASSERT_UNREACHABLE(
            "Should not try to get animation effects for "
            "a pseudo other that :before, :after or ::marker");
        break;
    }
    return mElementData;
  }

  PerElementOrPseudoData& DataFor(const PseudoStyleRequest& aRequest) {
    const auto& data =
        const_cast<const ElementAnimationData*>(this)->DataFor(aRequest);
    return const_cast<PerElementOrPseudoData&>(data);
  }

 public:
  void Traverse(nsCycleCollectionTraversalCallback&);

  void ClearAllAnimationCollections();

  EffectSet* GetEffectSetFor(const PseudoStyleRequest& aRequest) const {
    return DataFor(aRequest).mEffectSet.get();
  }

  void ClearEffectSetFor(const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (data.mEffectSet) {
      data.DoClearEffectSet();
    }
  }

  EffectSet& EnsureEffectSetFor(const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (auto* set = data.mEffectSet.get()) {
      return *set;
    }
    return data.DoEnsureEffectSet();
  }

  CSSTransitionCollection* GetTransitionCollection(
      const PseudoStyleRequest& aRequest) {
    return DataFor(aRequest).mTransitions.get();
  }

  void ClearTransitionCollectionFor(const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (data.mTransitions) {
      data.DoClearTransitions();
    }
  }

  CSSTransitionCollection& EnsureTransitionCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (auto* collection = data.mTransitions.get()) {
      return *collection;
    }
    return data.DoEnsureTransitions(aOwner, aRequest);
  }

  CSSAnimationCollection* GetAnimationCollection(
      const PseudoStyleRequest& aRequest) {
    return DataFor(aRequest).mAnimations.get();
  }

  void ClearAnimationCollectionFor(const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (data.mAnimations) {
      data.DoClearAnimations();
    }
  }

  CSSAnimationCollection& EnsureAnimationCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (auto* collection = data.mAnimations.get()) {
      return *collection;
    }
    return data.DoEnsureAnimations(aOwner, aRequest);
  }

  ScrollTimelineCollection* GetScrollTimelineCollection(
      const PseudoStyleRequest& aRequest) {
    return DataFor(aRequest).mScrollTimelines.get();
  }

  void ClearScrollTimelineCollectionFor(const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (data.mScrollTimelines) {
      data.DoClearScrollTimelines();
    }
  }

  ScrollTimelineCollection& EnsureScrollTimelineCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (auto* collection = data.mScrollTimelines.get()) {
      return *collection;
    }
    return data.DoEnsureScrollTimelines(aOwner, aRequest);
  }

  ViewTimelineCollection* GetViewTimelineCollection(
      const PseudoStyleRequest& aRequest) {
    return DataFor(aRequest).mViewTimelines.get();
  }

  void ClearViewTimelineCollectionFor(const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (data.mViewTimelines) {
      data.DoClearViewTimelines();
    }
  }

  ViewTimelineCollection& EnsureViewTimelineCollection(
      dom::Element& aOwner, const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (auto* collection = data.mViewTimelines.get()) {
      return *collection;
    }
    return data.DoEnsureViewTimelines(aOwner, aRequest);
  }

  dom::ProgressTimelineScheduler* GetProgressTimelineScheduler(
      const PseudoStyleRequest& aRequest) {
    return DataFor(aRequest).mProgressTimelineScheduler.get();
  }

  void ClearProgressTimelineScheduler(const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (data.mProgressTimelineScheduler) {
      data.DoClearProgressTimelineScheduler();
    }
  }

  dom::ProgressTimelineScheduler& EnsureProgressTimelineScheduler(
      const PseudoStyleRequest& aRequest) {
    auto& data = DataFor(aRequest);
    if (auto* collection = data.mProgressTimelineScheduler.get()) {
      return *collection;
    }
    return data.DoEnsureProgressTimelineScheduler();
  }

  ElementAnimationData() = default;
};

}  // namespace mozilla

#endif
