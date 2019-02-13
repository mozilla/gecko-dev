/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsCSSProps.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "nsRuleProcessorData.h"
#include "nsRuleWalker.h"
#include "nsCSSPropertySet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "Layers.h"
#include "FrameLayerBuilder.h"
#include "nsCSSProps.h"
#include "nsDisplayList.h"
#include "nsStyleChangeList.h"
#include "nsStyleSet.h"
#include "RestyleManager.h"
#include "nsDOMMutationObserver.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;
using mozilla::dom::Animation;
using mozilla::dom::KeyframeEffectReadOnly;

using namespace mozilla;
using namespace mozilla::css;

const nsString&
ElementPropertyTransition::Name() const
{
   if (!mName.Length()) {
     const_cast<ElementPropertyTransition*>(this)->mName =
       NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(TransitionProperty()));
   }
   return dom::KeyframeEffectReadOnly::Name();
}

double
ElementPropertyTransition::CurrentValuePortion() const
{
  // It would be easy enough to handle finished transitions by using a
  // progress of 1 but currently we should not be called for finished
  // transitions.
  MOZ_ASSERT(!IsFinishedTransition(),
             "Getting the value portion of a finished transition");
  MOZ_ASSERT(!GetLocalTime().IsNull(),
             "Getting the value portion of an animation that's not being "
             "sampled");

  // Transitions use a fill mode of 'backwards' so GetComputedTiming will
  // never return a null time progress due to being *before* the animation
  // interval. However, it might be possible that we're behind on flushing
  // causing us to get called *after* the animation interval. So, just in
  // case, we override the fill mode to 'both' to ensure the progress
  // is never null.
  AnimationTiming timingToUse = mTiming;
  timingToUse.mFillMode = NS_STYLE_ANIMATION_FILL_MODE_BOTH;
  ComputedTiming computedTiming = GetComputedTiming(&timingToUse);

  MOZ_ASSERT(computedTiming.mProgress != ComputedTiming::kNullProgress,
             "Got a null progress for a fill mode of 'both'");
  MOZ_ASSERT(mProperties.Length() == 1,
             "Should have one animation property for a transition");
  MOZ_ASSERT(mProperties[0].mSegments.Length() == 1,
             "Animation property should have one segment for a transition");
  return mProperties[0].mSegments[0].mTimingFunction
         .GetValue(computedTiming.mProgress);
}

/*****************************************************************************
 * CSSTransition                                                             *
 *****************************************************************************/

mozilla::dom::AnimationPlayState
CSSTransition::PlayStateFromJS() const
{
  FlushStyle();
  return Animation::PlayStateFromJS();
}

void
CSSTransition::PlayFromJS(ErrorResult& aRv)
{
  FlushStyle();
  Animation::PlayFromJS(aRv);
}

CommonAnimationManager*
CSSTransition::GetAnimationManager() const
{
  nsPresContext* context = GetPresContext();
  if (!context) {
    return nullptr;
  }

  return context->TransitionManager();
}

/*****************************************************************************
 * nsTransitionManager                                                       *
 *****************************************************************************/

void
nsTransitionManager::StyleContextChanged(dom::Element *aElement,
                                         nsStyleContext *aOldStyleContext,
                                         nsRefPtr<nsStyleContext>* aNewStyleContext /* inout */)
{
  nsStyleContext* newStyleContext = *aNewStyleContext;

  NS_PRECONDITION(aOldStyleContext->GetPseudo() == newStyleContext->GetPseudo(),
                  "pseudo type mismatch");

  if (mInAnimationOnlyStyleUpdate) {
    // If we're doing an animation-only style update, return, since the
    // purpose of an animation-only style update is to update only the
    // animation styles so that we don't consider style changes
    // resulting from changes in the animation time for starting a
    // transition.
    return;
  }

  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore transitions.
    return;
  }

  if (aOldStyleContext->HasPseudoElementData() !=
      newStyleContext->HasPseudoElementData()) {
    // If the old style context and new style context differ in terms of
    // whether they're inside ::first-letter, ::first-line, or similar,
    // bail.  We can't hit this codepath for normal style changes
    // involving moving frames around the boundaries of these
    // pseudo-elements since we don't call StyleContextChanged from
    // ReparentStyleContext.  However, we can hit this codepath during
    // the handling of transitions that start across reframes.
    //
    // While there isn't an easy *perfect* way to handle this case, err
    // on the side of missing some transitions that we ought to have
    // rather than having bogus transitions that we shouldn't.
    //
    // We could consider changing this handling, although it's worth
    // thinking about whether the code below could do anything weird in
    // this case.
    return;
  }

  // NOTE: Things in this function (and ConsiderStartingTransition)
  // should never call PeekStyleData because we don't preserve gotten
  // structs across reframes.

  // Return sooner (before the startedAny check below) for the most
  // common case: no transitions specified or running.
  const nsStyleDisplay *disp = newStyleContext->StyleDisplay();
  nsCSSPseudoElements::Type pseudoType = newStyleContext->GetPseudoType();
  if (pseudoType != nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    if (pseudoType != nsCSSPseudoElements::ePseudo_before &&
        pseudoType != nsCSSPseudoElements::ePseudo_after) {
      return;
    }

    NS_ASSERTION((pseudoType == nsCSSPseudoElements::ePseudo_before &&
                  aElement->NodeInfo()->NameAtom() == nsGkAtoms::mozgeneratedcontentbefore) ||
                 (pseudoType == nsCSSPseudoElements::ePseudo_after &&
                  aElement->NodeInfo()->NameAtom() == nsGkAtoms::mozgeneratedcontentafter),
                 "Unexpected aElement coming through");

    // Else the element we want to use from now on is the element the
    // :before or :after is attached to.
    aElement = aElement->GetParent()->AsElement();
  }

  AnimationCollection* collection = GetAnimations(aElement, pseudoType, false);
  if (!collection &&
      disp->mTransitionPropertyCount == 1 &&
      disp->mTransitions[0].GetCombinedDuration() <= 0.0f) {
    return;
  }

  if (collection &&
      collection->mCheckGeneration ==
        mPresContext->RestyleManager()->GetAnimationGeneration()) {
    // When we start a new transition, we immediately post a restyle.
    // If the animation generation on the collection is current, that
    // means *this* is that restyle, since we bump the animation
    // generation on the restyle manager whenever there's a real style
    // change (i.e., one where mInAnimationOnlyStyleUpdate isn't true,
    // which causes us to return above).  Thus we shouldn't do anything.
    return;
  }
  if (newStyleContext->GetParent() &&
      newStyleContext->GetParent()->HasPseudoElementData()) {
    // Ignore transitions on things that inherit properties from
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return;
  }

  NS_WARN_IF_FALSE(!nsLayoutUtils::AreAsyncAnimationsEnabled() ||
                     mPresContext->RestyleManager()->
                       ThrottledAnimationStyleIsUpToDate(),
                   "throttled animations not up to date");

  // Compute what the css-transitions spec calls the "after-change
  // style", which is the new style without any data from transitions,
  // but still inheriting from data that contains transitions that are
  // not stopping or starting right now.
  nsRefPtr<nsStyleContext> afterChangeStyle;
  if (collection) {
    nsStyleSet* styleSet = mPresContext->StyleSet();
    afterChangeStyle =
      styleSet->ResolveStyleWithoutAnimation(aElement, newStyleContext,
                                             eRestyle_CSSTransitions);
  } else {
    afterChangeStyle = newStyleContext;
  }

  nsAutoAnimationMutationBatch mb(aElement);

  // Per http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html
  // I'll consider only the transitions from the number of items in
  // 'transition-property' on down, and later ones will override earlier
  // ones (tracked using |whichStarted|).
  bool startedAny = false;
  nsCSSPropertySet whichStarted;
  for (uint32_t i = disp->mTransitionPropertyCount; i-- != 0; ) {
    const StyleTransition& t = disp->mTransitions[i];
    // Check the combined duration (combination of delay and duration)
    // first, since it defaults to zero, which means we can ignore the
    // transition.
    if (t.GetCombinedDuration() > 0.0f) {
      // We might have something to transition.  See if any of the
      // properties in question changed and are animatable.
      // FIXME: Would be good to find a way to share code between this
      // interpretation of transition-property and the one below.
      nsCSSProperty property = t.GetProperty();
      if (property == eCSSPropertyExtra_no_properties ||
          property == eCSSPropertyExtra_variable ||
          property == eCSSProperty_UNKNOWN) {
        // Nothing to do, but need to exclude this from cases below.
      } else if (property == eCSSPropertyExtra_all_properties) {
        for (nsCSSProperty p = nsCSSProperty(0);
             p < eCSSProperty_COUNT_no_shorthands;
             p = nsCSSProperty(p + 1)) {
          ConsiderStartingTransition(p, t, aElement, collection,
                                     aOldStyleContext, afterChangeStyle,
                                     &startedAny, &whichStarted);
        }
      } else if (nsCSSProps::IsShorthand(property)) {
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
            subprop, property, nsCSSProps::eEnabledForAllContent) {
          ConsiderStartingTransition(*subprop, t, aElement, collection,
                                     aOldStyleContext, afterChangeStyle,
                                     &startedAny, &whichStarted);
        }
      } else {
        ConsiderStartingTransition(property, t, aElement, collection,
                                   aOldStyleContext, afterChangeStyle,
                                   &startedAny, &whichStarted);
      }
    }
  }

  // Stop any transitions for properties that are no longer in
  // 'transition-property', including finished transitions.
  // Also stop any transitions (and remove any finished transitions)
  // for properties that just changed (and are still in the set of
  // properties to transition), but for which we didn't just start the
  // transition.  This can happen delay and duration are both zero, or
  // because the new value is not interpolable.
  // Note that we also do the latter set of work in
  // nsTransitionManager::PruneCompletedTransitions.
  if (collection) {
    bool checkProperties =
      disp->mTransitions[0].GetProperty() != eCSSPropertyExtra_all_properties;
    nsCSSPropertySet allTransitionProperties;
    if (checkProperties) {
      for (uint32_t i = disp->mTransitionPropertyCount; i-- != 0; ) {
        const StyleTransition& t = disp->mTransitions[i];
        // FIXME: Would be good to find a way to share code between this
        // interpretation of transition-property and the one above.
        nsCSSProperty property = t.GetProperty();
        if (property == eCSSPropertyExtra_no_properties ||
            property == eCSSPropertyExtra_variable ||
            property == eCSSProperty_UNKNOWN) {
          // Nothing to do, but need to exclude this from cases below.
        } else if (property == eCSSPropertyExtra_all_properties) {
          for (nsCSSProperty p = nsCSSProperty(0);
               p < eCSSProperty_COUNT_no_shorthands;
               p = nsCSSProperty(p + 1)) {
            allTransitionProperties.AddProperty(p);
          }
        } else if (nsCSSProps::IsShorthand(property)) {
          CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
              subprop, property, nsCSSProps::eEnabledForAllContent) {
            allTransitionProperties.AddProperty(*subprop);
          }
        } else {
          allTransitionProperties.AddProperty(property);
        }
      }
    }

    AnimationPtrArray& animations = collection->mAnimations;
    size_t i = animations.Length();
    MOZ_ASSERT(i != 0, "empty transitions list?");
    StyleAnimationValue currentValue;
    do {
      --i;
      Animation* anim = animations[i];
      dom::KeyframeEffectReadOnly* effect = anim->GetEffect();
      MOZ_ASSERT(effect && effect->Properties().Length() == 1,
                 "Should have one animation property for a transition");
      MOZ_ASSERT(effect && effect->Properties()[0].mSegments.Length() == 1,
                 "Animation property should have one segment for a transition");
      const AnimationProperty& prop = effect->Properties()[0];
      const AnimationPropertySegment& segment = prop.mSegments[0];
          // properties no longer in 'transition-property'
      if ((checkProperties &&
           !allTransitionProperties.HasProperty(prop.mProperty)) ||
          // properties whose computed values changed but for which we
          // did not start a new transition (because delay and
          // duration are both zero, or because the new value is not
          // interpolable); a new transition would have segment.mToValue
          // matching currentValue
          !ExtractComputedValueForTransition(prop.mProperty, afterChangeStyle,
                                             currentValue) ||
          currentValue != segment.mToValue) {
        // stop the transition
        if (!anim->GetEffect()->IsFinishedTransition()) {
          anim->CancelFromStyle();
          collection->UpdateAnimationGeneration(mPresContext);
        }
        animations.RemoveElementAt(i);
      }
    } while (i != 0);

    if (animations.IsEmpty()) {
      collection->Destroy();
      collection = nullptr;
    }
  }

  MOZ_ASSERT(!startedAny || collection,
             "must have element transitions if we started any transitions");

  if (collection) {
    UpdateCascadeResultsWithTransitions(collection);

    // Set the style rule refresh time to null so that EnsureStyleRuleFor
    // creates a new style rule if we started *or* stopped transitions.
    collection->mStyleRuleRefreshTime = TimeStamp();
    collection->UpdateCheckGeneration(mPresContext);
    collection->mNeedsRefreshes = true;
    TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
    collection->EnsureStyleRuleFor(now, EnsureStyleRule_IsNotThrottled);
  }

  // We want to replace the new style context with the after-change style.
  *aNewStyleContext = afterChangeStyle;
  if (collection) {
    // Since we have transition styles, we have to undo this replacement.
    // The check of collection->mCheckGeneration against the restyle
    // manager's GetAnimationGeneration() will ensure that we don't go
    // through the rest of this function again when we do.
    collection->PostRestyleForAnimation(mPresContext);
  }
}

void
nsTransitionManager::ConsiderStartingTransition(
  nsCSSProperty aProperty,
  const StyleTransition& aTransition,
  dom::Element* aElement,
  AnimationCollection*& aElementTransitions,
  nsStyleContext* aOldStyleContext,
  nsStyleContext* aNewStyleContext,
  bool* aStartedAny,
  nsCSSPropertySet* aWhichStarted)
{
  // IsShorthand itself will assert if aProperty is not a property.
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty),
             "property out of range");
  NS_ASSERTION(!aElementTransitions ||
               aElementTransitions->mElement == aElement, "Element mismatch");

  if (aWhichStarted->HasProperty(aProperty)) {
    // A later item in transition-property already started a
    // transition for this property, so we ignore this one.
    // See comment above and
    // http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html .
    return;
  }

  if (nsCSSProps::kAnimTypeTable[aProperty] == eStyleAnimType_None) {
    return;
  }

  dom::DocumentTimeline* timeline = aElement->OwnerDoc()->Timeline();

  StyleAnimationValue startValue, endValue, dummyValue;
  bool haveValues =
    ExtractComputedValueForTransition(aProperty, aOldStyleContext,
                                      startValue) &&
    ExtractComputedValueForTransition(aProperty, aNewStyleContext,
                                      endValue);

  bool haveChange = startValue != endValue;

  bool shouldAnimate =
    haveValues &&
    haveChange &&
    // Check that we can interpolate between these values
    // (If this is ever a performance problem, we could add a
    // CanInterpolate method, but it seems fine for now.)
    StyleAnimationValue::Interpolate(aProperty, startValue, endValue,
                                     0.5, dummyValue);

  bool haveCurrentTransition = false;
  size_t currentIndex = nsTArray<ElementPropertyTransition>::NoIndex;
  const ElementPropertyTransition *oldPT = nullptr;
  if (aElementTransitions) {
    AnimationPtrArray& animations = aElementTransitions->mAnimations;
    for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
      const ElementPropertyTransition *iPt =
        animations[i]->GetEffect()->AsTransition();
      if (iPt->TransitionProperty() == aProperty) {
        haveCurrentTransition = true;
        currentIndex = i;
        oldPT = iPt;
        break;
      }
    }
  }

  // If we got a style change that changed the value to the endpoint
  // of the currently running transition, we don't want to interrupt
  // its timing function.
  // This needs to be before the !shouldAnimate && haveCurrentTransition
  // case below because we might be close enough to the end of the
  // transition that the current value rounds to the final value.  In
  // this case, we'll end up with shouldAnimate as false (because
  // there's no value change), but we need to return early here rather
  // than cancel the running transition because shouldAnimate is false!
  //
  // Likewise, if we got a style change that changed the value to the
  // endpoint of our finished transition, we also don't want to start
  // a new transition for the reasons described in
  // https://lists.w3.org/Archives/Public/www-style/2015Jan/0444.html .
  MOZ_ASSERT(!oldPT || oldPT->Properties()[0].mSegments.Length() == 1,
             "Should have one animation property segment for a transition");
  if (haveCurrentTransition && haveValues &&
      oldPT->Properties()[0].mSegments[0].mToValue == endValue) {
    // GetAnimationRule already called RestyleForAnimation.
    return;
  }

  if (!shouldAnimate) {
    if (haveCurrentTransition && !oldPT->IsFinishedTransition()) {
      // We're in the middle of a transition, and just got a non-transition
      // style change to something that we can't animate.  This might happen
      // because we got a non-transition style change changing to the current
      // in-progress value (which is particularly easy to cause when we're
      // currently in the 'transition-delay').  It also might happen because we
      // just got a style change to a value that can't be interpolated.
      AnimationPtrArray& animations = aElementTransitions->mAnimations;
      animations[currentIndex]->CancelFromStyle();
      oldPT = nullptr; // Clear pointer so it doesn't dangle
      animations.RemoveElementAt(currentIndex);
      aElementTransitions->UpdateAnimationGeneration(mPresContext);

      if (animations.IsEmpty()) {
        aElementTransitions->Destroy();
        // |aElementTransitions| is now a dangling pointer!
        aElementTransitions = nullptr;
      }
      // GetAnimationRule already called RestyleForAnimation.
    }
    return;
  }

  const nsTimingFunction &tf = aTransition.GetTimingFunction();
  float delay = aTransition.GetDelay();
  float duration = aTransition.GetDuration();
  if (duration < 0.0) {
    // The spec says a negative duration is treated as zero.
    duration = 0.0;
  }

  StyleAnimationValue startForReversingTest = startValue;
  double reversePortion = 1.0;

  // If the new transition reverses an existing one, we'll need to
  // handle the timing differently.
  if (haveCurrentTransition &&
      !oldPT->IsFinishedTransition() &&
      oldPT->mStartForReversingTest == endValue) {
    // Compute the appropriate negative transition-delay such that right
    // now we'd end up at the current position.
    double valuePortion =
      oldPT->CurrentValuePortion() * oldPT->mReversePortion +
      (1.0 - oldPT->mReversePortion);
    // A timing function with negative y1 (or y2!) might make
    // valuePortion negative.  In this case, we still want to apply our
    // reversing logic based on relative distances, not make duration
    // negative.
    if (valuePortion < 0.0) {
      valuePortion = -valuePortion;
    }
    // A timing function with y2 (or y1!) greater than one might
    // advance past its terminal value.  It's probably a good idea to
    // clamp valuePortion to be at most one to preserve the invariant
    // that a transition will complete within at most its specified
    // time.
    if (valuePortion > 1.0) {
      valuePortion = 1.0;
    }

    // Negative delays are essentially part of the transition
    // function, so reduce them along with the duration, but don't
    // reduce positive delays.
    if (delay < 0.0f) {
      delay *= valuePortion;
    }

    duration *= valuePortion;

    startForReversingTest = oldPT->Properties()[0].mSegments[0].mToValue;
    reversePortion = valuePortion;
  }

  AnimationTiming timing;
  timing.mIterationDuration = TimeDuration::FromMilliseconds(duration);
  timing.mDelay = TimeDuration::FromMilliseconds(delay);
  timing.mIterationCount = 1;
  timing.mDirection = NS_STYLE_ANIMATION_DIRECTION_NORMAL;
  timing.mFillMode = NS_STYLE_ANIMATION_FILL_MODE_BACKWARDS;

  nsRefPtr<ElementPropertyTransition> pt =
    new ElementPropertyTransition(aElement->OwnerDoc(), aElement,
                                  aNewStyleContext->GetPseudoType(), timing);
  pt->mStartForReversingTest = startForReversingTest;
  pt->mReversePortion = reversePortion;

  AnimationProperty& prop = *pt->Properties().AppendElement();
  prop.mProperty = aProperty;
  prop.mWinsInCascade = true;

  AnimationPropertySegment& segment = *prop.mSegments.AppendElement();
  segment.mFromValue = startValue;
  segment.mToValue = endValue;
  segment.mFromKey = 0;
  segment.mToKey = 1;
  segment.mTimingFunction.Init(tf);

  nsRefPtr<CSSTransition> animation = new CSSTransition(timeline);
  // The order of the following two calls is important since PlayFromStyle
  // will add the animation to the PendingAnimationTracker of its effect's
  // document. When we come to make effect writeable (bug 1049975) we should
  // remove this dependency.
  animation->SetEffect(pt);
  animation->PlayFromStyle();

  if (!aElementTransitions) {
    aElementTransitions =
      GetAnimations(aElement, aNewStyleContext->GetPseudoType(), true);
    if (!aElementTransitions) {
      NS_WARNING("allocating CommonAnimationManager failed");
      return;
    }
  }

  AnimationPtrArray& animations = aElementTransitions->mAnimations;
#ifdef DEBUG
  for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
    MOZ_ASSERT(
      i == currentIndex ||
      (animations[i]->GetEffect() &&
       animations[i]->GetEffect()->AsTransition()->TransitionProperty()
         != aProperty),
      "duplicate transitions for property");
  }
#endif
  if (haveCurrentTransition) {
    animations[currentIndex]->CancelFromStyle();
    oldPT = nullptr; // Clear pointer so it doesn't dangle
    animations[currentIndex] = animation;
  } else {
    if (!animations.AppendElement(animation)) {
      NS_WARNING("out of memory");
      return;
    }
  }
  aElementTransitions->UpdateAnimationGeneration(mPresContext);

  *aStartedAny = true;
  aWhichStarted->AddProperty(aProperty);
}

void
nsTransitionManager::PruneCompletedTransitions(mozilla::dom::Element* aElement,
                                               nsCSSPseudoElements::Type
                                                 aPseudoType,
                                               nsStyleContext* aNewStyleContext)
{
  AnimationCollection* collection = GetAnimations(aElement, aPseudoType, false);
  if (!collection) {
    return;
  }

  // Remove any finished transitions whose style doesn't match the new
  // style.
  // This is similar to some of the work that happens near the end of
  // nsTransitionManager::StyleContextChanged.
  // FIXME (bug 1158431): Really, we should also cancel running
  // transitions whose destination doesn't match as well.
  AnimationPtrArray& animations = collection->mAnimations;
  size_t i = animations.Length();
  MOZ_ASSERT(i != 0, "empty transitions list?");
  do {
    --i;
    Animation* anim = animations[i];
    dom::KeyframeEffectReadOnly* effect = anim->GetEffect();

    if (!effect->IsFinishedTransition()) {
      continue;
    }

    MOZ_ASSERT(effect && effect->Properties().Length() == 1,
               "Should have one animation property for a transition");
    MOZ_ASSERT(effect && effect->Properties()[0].mSegments.Length() == 1,
               "Animation property should have one segment for a transition");
    const AnimationProperty& prop = effect->Properties()[0];
    const AnimationPropertySegment& segment = prop.mSegments[0];

    // Since effect is a finished transition, we know it didn't
    // influence style.
    StyleAnimationValue currentValue;
    if (!ExtractComputedValueForTransition(prop.mProperty, aNewStyleContext,
                                           currentValue) ||
        currentValue != segment.mToValue) {
      animations.RemoveElementAt(i);
    }
  } while (i != 0);

  if (collection->mAnimations.IsEmpty()) {
    collection->Destroy();
    // |collection| is now a dangling pointer!
    collection = nullptr;
  }
}

void
nsTransitionManager::UpdateCascadeResultsWithTransitions(
                       AnimationCollection* aTransitions)
{
  AnimationCollection* animations = mPresContext->AnimationManager()->
      GetAnimations(aTransitions->mElement,
                    aTransitions->PseudoElementType(), false);
  UpdateCascadeResults(aTransitions, animations);
}

void
nsTransitionManager::UpdateCascadeResultsWithAnimations(
                       AnimationCollection* aAnimations)
{
  AnimationCollection* transitions = mPresContext->TransitionManager()->
      GetAnimations(aAnimations->mElement,
                    aAnimations->PseudoElementType(), false);
  UpdateCascadeResults(transitions, aAnimations);
}

void
nsTransitionManager::UpdateCascadeResultsWithAnimationsToBeDestroyed(
                       const AnimationCollection* aAnimations)
{
  // aAnimations is about to be destroyed.  So get transitions from it,
  // but then don't pass it to UpdateCascadeResults, since it has
  // information that may now be incorrect.
  AnimationCollection* transitions =
    mPresContext->TransitionManager()->
      GetAnimations(aAnimations->mElement,
                    aAnimations->PseudoElementType(), false);
  UpdateCascadeResults(transitions, nullptr);
}

void
nsTransitionManager::UpdateCascadeResults(AnimationCollection* aTransitions,
                                          AnimationCollection* aAnimations)
{
  if (!aTransitions) {
    // Nothing to do.
    return;
  }

  nsCSSPropertySet propertiesUsed;
#ifdef DEBUG
  nsCSSPropertySet propertiesWithTransitions;
#endif

  // http://dev.w3.org/csswg/css-transitions/#application says that
  // transitions do not apply when the same property has a CSS Animation
  // on that element (even though animations are lower in the cascade).
  if (aAnimations) {
    TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
    // Passing EnsureStyleRule_IsThrottled is OK since we will
    // unthrottle when animations are finishing.
    aAnimations->EnsureStyleRuleFor(now, EnsureStyleRule_IsThrottled);

    if (aAnimations->mStyleRule) {
      aAnimations->mStyleRule->AddPropertiesToSet(propertiesUsed);
    }
  }

  // Since we should never have more than one transition for the same
  // property, it doesn't matter what order we iterate the transitions.
  // But let's go the same way as animations.
  bool changed = false;
  AnimationPtrArray& animations = aTransitions->mAnimations;
  for (size_t animIdx = animations.Length(); animIdx-- != 0; ) {
    MOZ_ASSERT(animations[animIdx]->GetEffect() &&
               animations[animIdx]->GetEffect()->Properties().Length() == 1,
               "Should have one animation property for a transition");
    AnimationProperty& prop = animations[animIdx]->GetEffect()->Properties()[0];
    bool newWinsInCascade = !propertiesUsed.HasProperty(prop.mProperty);
    if (prop.mWinsInCascade != newWinsInCascade) {
      changed = true;
    }
    prop.mWinsInCascade = newWinsInCascade;
    // assert that we don't need to bother adding the transitioned
    // properties into propertiesUsed
#ifdef DEBUG
    MOZ_ASSERT(!propertiesWithTransitions.HasProperty(prop.mProperty),
               "we're assuming we have only one transition per property");
    propertiesWithTransitions.AddProperty(prop.mProperty);
#endif
  }

  if (changed) {
    mPresContext->RestyleManager()->IncrementAnimationGeneration();
    aTransitions->UpdateAnimationGeneration(mPresContext);
    aTransitions->PostUpdateLayerAnimations();

    // Invalidate our style rule.
    aTransitions->mStyleRuleRefreshTime = TimeStamp();
    aTransitions->mNeedsRefreshes = true;
  }
}

/*
 * nsIStyleRuleProcessor implementation
 */

/* virtual */ size_t
nsTransitionManager::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return CommonAnimationManager::SizeOfExcludingThis(aMallocSizeOf);
}

/* virtual */ size_t
nsTransitionManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

struct TransitionEventInfo {
  nsCOMPtr<nsIContent> mElement;
  InternalTransitionEvent mEvent;

  TransitionEventInfo(nsIContent *aElement, nsCSSProperty aProperty,
                      TimeDuration aDuration, const nsAString& aPseudoElement)
    : mElement(aElement)
    , mEvent(true, NS_TRANSITION_END)
  {
    // XXX Looks like nobody initialize WidgetEvent::time
    mEvent.propertyName =
      NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aProperty));
    mEvent.elapsedTime = aDuration.ToSeconds();
    mEvent.pseudoElement = aPseudoElement;
  }

  // InternalTransitionEvent doesn't support copy-construction, so we need
  // to ourselves in order to work with nsTArray
  TransitionEventInfo(const TransitionEventInfo &aOther)
    : mElement(aOther.mElement)
    , mEvent(true, NS_TRANSITION_END)
  {
    mEvent.AssignTransitionEventData(aOther.mEvent, false);
  }
};

/* virtual */ void
nsTransitionManager::WillRefresh(mozilla::TimeStamp aTime)
{
  MOZ_ASSERT(mPresContext,
             "refresh driver should not notify additional observers "
             "after pres context has been destroyed");
  if (!mPresContext->GetPresShell()) {
    // Someone might be keeping mPresContext alive past the point
    // where it has been torn down; don't bother doing anything in
    // this case.  But do get rid of all our transitions so we stop
    // triggering refreshes.
    RemoveAllElementCollections();
    return;
  }

  FlushTransitions(Can_Throttle);
}

void
nsTransitionManager::FlushTransitions(FlushFlags aFlags)
{
  if (PR_CLIST_IS_EMPTY(&mElementCollections)) {
    // no transitions, leave early
    return;
  }

  nsTArray<TransitionEventInfo> events;
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  bool didThrottle = false;
  // Trim transitions that have completed, post restyle events for frames that
  // are still transitioning, and start transitions with delays.
  {
    PRCList *next = PR_LIST_HEAD(&mElementCollections);
    while (next != &mElementCollections) {
      AnimationCollection* collection = static_cast<AnimationCollection*>(next);
      next = PR_NEXT_LINK(next);

      nsAutoAnimationMutationBatch mb(collection->mElement);

      collection->Tick();
      bool canThrottleTick = aFlags == Can_Throttle &&
        collection->CanPerformOnCompositorThread(
          AnimationCollection::CanAnimateFlags(0)) &&
        collection->CanThrottleAnimation(now);

      MOZ_ASSERT(collection->mElement->GetCrossShadowCurrentDoc() ==
                   mPresContext->Document(),
                 "Element::UnbindFromTree should have "
                 "destroyed the element transitions object");

      size_t i = collection->mAnimations.Length();
      MOZ_ASSERT(i != 0, "empty transitions list?");
      bool transitionStartedOrEnded = false;
      do {
        --i;
        Animation* anim = collection->mAnimations[i];
        if (!anim->GetEffect()->IsFinishedTransition()) {
          MOZ_ASSERT(anim->GetEffect(), "Transitions should have an effect");
          ComputedTiming computedTiming =
            anim->GetEffect()->GetComputedTiming();
          if (computedTiming.mPhase == ComputedTiming::AnimationPhase_After) {
            nsCSSProperty prop =
              anim->GetEffect()->AsTransition()->TransitionProperty();
            TimeDuration duration =
              anim->GetEffect()->Timing().mIterationDuration;
            events.AppendElement(
              TransitionEventInfo(collection->mElement, prop,
                                  duration,
                                  collection->PseudoElement()));

            // Leave this transition in the list for one more refresh
            // cycle, since we haven't yet processed its style change, and
            // if we also have (already, or will have from processing
            // transitionend events or other refresh driver notifications)
            // a non-animation style change that would affect it, we need
            // to know not to start a new transition for the transition
            // from the almost-completed value to the final value.
            anim->GetEffect()->SetIsFinishedTransition(true);
            collection->UpdateAnimationGeneration(mPresContext);
            transitionStartedOrEnded = true;
          } else if ((computedTiming.mPhase ==
                      ComputedTiming::AnimationPhase_Active) &&
                     canThrottleTick &&
                     !anim->IsRunningOnCompositor()) {
            // Start a transition with a delay where we should start the
            // transition proper.
            collection->UpdateAnimationGeneration(mPresContext);
            transitionStartedOrEnded = true;
          }
        }
      } while (i != 0);

      // We need to restyle even if the transition rule no longer
      // applies (in which case we just made it not apply).
      MOZ_ASSERT(collection->mElementProperty ==
                   nsGkAtoms::transitionsProperty ||
                 collection->mElementProperty ==
                   nsGkAtoms::transitionsOfBeforeProperty ||
                 collection->mElementProperty ==
                   nsGkAtoms::transitionsOfAfterProperty,
                 "Unexpected element property; might restyle too much");
      if (!canThrottleTick || transitionStartedOrEnded) {
        collection->PostRestyleForAnimation(mPresContext);
      } else {
        didThrottle = true;
      }

      if (collection->mAnimations.IsEmpty()) {
        collection->Destroy();
        // |collection| is now a dangling pointer!
        collection = nullptr;
      }
    }
  }

  if (didThrottle) {
    mPresContext->Document()->SetNeedStyleFlush();
  }

  MaybeStartOrStopObservingRefreshDriver();

  for (uint32_t i = 0, i_end = events.Length(); i < i_end; ++i) {
    TransitionEventInfo &info = events[i];
    EventDispatcher::Dispatch(info.mElement, mPresContext, &info.mEvent);

    if (!mPresContext) {
      break;
    }
  }
}
