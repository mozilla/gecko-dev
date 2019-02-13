/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#ifndef nsTransitionManager_h_
#define nsTransitionManager_h_

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "AnimationCommon.h"
#include "nsCSSPseudoElements.h"

class nsStyleContext;
class nsPresContext;
class nsCSSPropertySet;

namespace mozilla {
struct StyleTransition;
}

/*****************************************************************************
 * Per-Element data                                                          *
 *****************************************************************************/

namespace mozilla {

struct ElementPropertyTransition : public dom::KeyframeEffectReadOnly
{
  ElementPropertyTransition(nsIDocument* aDocument,
                            dom::Element* aTarget,
                            nsCSSPseudoElements::Type aPseudoType,
                            const AnimationTiming &aTiming)
    : dom::KeyframeEffectReadOnly(aDocument, aTarget, aPseudoType,
                                  aTiming, EmptyString())
  { }

  virtual ElementPropertyTransition* AsTransition() override { return this; }
  virtual const ElementPropertyTransition* AsTransition() const override { return this; }

  virtual const nsString& Name() const override;

  nsCSSProperty TransitionProperty() const {
    MOZ_ASSERT(Properties().Length() == 1,
               "Transitions should have exactly one animation property. "
               "Perhaps we are using an un-initialized transition?");
    return Properties()[0].mProperty;
  }

  // This is the start value to be used for a check for whether a
  // transition is being reversed.  Normally the same as
  // mProperties[0].mSegments[0].mFromValue, except when this transition
  // started as the reversal of another in-progress transition.
  // Needed so we can handle two reverses in a row.
  mozilla::StyleAnimationValue mStartForReversingTest;
  // Likewise, the portion (in value space) of the "full" reversed
  // transition that we're actually covering.  For example, if a :hover
  // effect has a transition that moves the element 10px to the right
  // (by changing 'left' from 0px to 10px), and the mouse moves in to
  // the element (starting the transition) but then moves out after the
  // transition has advanced 4px, the second transition (from 10px/4px
  // to 0px) will have mReversePortion of 0.4.  (If the mouse then moves
  // in again when the transition is back to 2px, the mReversePortion
  // for the third transition (from 0px/2px to 10px) will be 0.8.
  double mReversePortion;

  // Compute the portion of the *value* space that we should be through
  // at the current time.  (The input to the transition timing function
  // has time units, the output has value units.)
  double CurrentValuePortion() const;
};

class CSSTransition final : public dom::Animation
{
public:
 explicit CSSTransition(dom::DocumentTimeline* aTimeline)
    : dom::Animation(aTimeline)
  {
  }

  virtual CSSTransition* AsCSSTransition() override { return this; }

  virtual dom::AnimationPlayState PlayStateFromJS() const override;
  virtual void PlayFromJS(ErrorResult& aRv) override;

  // A variant of Play() that avoids posting style updates since this method
  // is expected to be called whilst already updating style.
  void PlayFromStyle()
  {
    ErrorResult rv;
    DoPlay(rv, Animation::LimitBehavior::Continue);
    // play() should not throw when LimitBehavior is Continue
    MOZ_ASSERT(!rv.Failed(), "Unexpected exception playing transition");
  }

protected:
  virtual ~CSSTransition() { }

  virtual css::CommonAnimationManager* GetAnimationManager() const override;
};

} // namespace mozilla

class nsTransitionManager final
  : public mozilla::css::CommonAnimationManager
{
public:
  explicit nsTransitionManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
    , mInAnimationOnlyStyleUpdate(false)
  {
  }

  typedef mozilla::AnimationCollection AnimationCollection;

  static AnimationCollection*
  GetAnimationsForCompositor(nsIContent* aContent, nsCSSProperty aProperty)
  {
    return mozilla::css::CommonAnimationManager::GetAnimationsForCompositor(
      aContent, nsGkAtoms::transitionsProperty, aProperty);
  }

  /**
   * StyleContextChanged
   *
   * To be called from nsFrameManager::ReResolveStyleContext when the
   * style of an element has changed, to initiate transitions from
   * that style change.  For style contexts with :before and :after
   * pseudos, aElement is expected to be the generated before/after
   * element.
   *
   * It may modify the new style context (by replacing
   * *aNewStyleContext) to cover up some of the changes for the duration
   * of the restyling of descendants.  If it does, this function will
   * take care of causing the necessary restyle afterwards.
   */
  void StyleContextChanged(mozilla::dom::Element *aElement,
                           nsStyleContext *aOldStyleContext,
                           nsRefPtr<nsStyleContext>* aNewStyleContext /* inout */);

  /**
   * When we're resolving style for an element that previously didn't have
   * style, we might have some old finished transitions for it, if,
   * say, it was display:none for a while, but previously displayed.
   *
   * This method removes any finished transitions that don't match the
   * new style.
   */
  void PruneCompletedTransitions(mozilla::dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType,
                                 nsStyleContext* aNewStyleContext);

  void UpdateCascadeResultsWithTransitions(AnimationCollection* aTransitions);
  void UpdateCascadeResultsWithAnimations(AnimationCollection* aAnimations);
  void UpdateCascadeResultsWithAnimationsToBeDestroyed(
         const AnimationCollection* aAnimations);
  void UpdateCascadeResults(AnimationCollection* aTransitions,
                            AnimationCollection* aAnimations);

  void SetInAnimationOnlyStyleUpdate(bool aInAnimationOnlyUpdate) {
    mInAnimationOnlyStyleUpdate = aInAnimationOnlyUpdate;
  }

  bool InAnimationOnlyStyleUpdate() const {
    return mInAnimationOnlyStyleUpdate;
  }

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
    MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
    MOZ_MUST_OVERRIDE override;

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime) override;

  void FlushTransitions(FlushFlags aFlags);

protected:
  virtual nsIAtom* GetAnimationsAtom() override {
    return nsGkAtoms::transitionsProperty;
  }
  virtual nsIAtom* GetAnimationsBeforeAtom() override {
    return nsGkAtoms::transitionsOfBeforeProperty;
  }
  virtual nsIAtom* GetAnimationsAfterAtom() override {
    return nsGkAtoms::transitionsOfAfterProperty;
  }

private:
  void
  ConsiderStartingTransition(nsCSSProperty aProperty,
                             const mozilla::StyleTransition& aTransition,
                             mozilla::dom::Element* aElement,
                             AnimationCollection*& aElementTransitions,
                             nsStyleContext* aOldStyleContext,
                             nsStyleContext* aNewStyleContext,
                             bool* aStartedAny,
                             nsCSSPropertySet* aWhichStarted);

  bool mInAnimationOnlyStyleUpdate;
};

#endif /* !defined(nsTransitionManager_h_) */
