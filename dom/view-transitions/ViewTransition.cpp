/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ViewTransition.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/ViewTransitionBinding.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/SVGIntegrationUtils.h"
#include "mozilla/WritingModes.h"
#include "nsDisplayList.h"
#include "nsITimer.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "Units.h"

namespace mozilla::dom {

// Set capture's old transform to a <transform-function> that would map
// element's border box from the snapshot containing block origin to its
// current visual position.
//
// Since we're using viewport as the snapshot origin, we can use
// GetBoundingClientRect() effectively...
//
// TODO(emilio): This might need revision.
static CSSToCSSMatrix4x4Flagged EffectiveTransform(nsIFrame* aFrame) {
  CSSToCSSMatrix4x4Flagged matrix;
  if (aFrame->GetSize().IsEmpty() || aFrame->Style()->IsRootElementStyle()) {
    return matrix;
  }

  CSSSize untransformedSize = CSSSize::FromAppUnits(aFrame->GetSize());
  CSSRect boundingRect = CSSRect::FromAppUnits(aFrame->GetBoundingClientRect());
  if (boundingRect.Size() != untransformedSize) {
    float sx = boundingRect.width / untransformedSize.width;
    float sy = boundingRect.height / untransformedSize.height;
    matrix = CSSToCSSMatrix4x4Flagged::Scaling(sx, sy, 0.0f);
  }
  if (boundingRect.TopLeft() != CSSPoint()) {
    matrix.PostTranslate(boundingRect.x, boundingRect.y, 0.0f);
  }
  return matrix;
}

static RefPtr<gfx::DataSourceSurface> CaptureFallbackSnapshot(
    nsIFrame* aFrame) {
  const nsRect rect = aFrame->InkOverflowRectRelativeToSelf();
  const auto surfaceRect = LayoutDeviceIntRect::FromAppUnitsToOutside(
      rect, aFrame->PresContext()->AppUnitsPerDevPixel());

  // TODO: Should we use the DrawTargetRecorder infra or what not?
  const auto format = gfx::SurfaceFormat::B8G8R8A8;
  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTarget(
      gfxPlatform::GetPlatform()->GetSoftwareBackend(),
      surfaceRect.Size().ToUnknownSize(), format);
  if (NS_WARN_IF(!dt) || NS_WARN_IF(!dt->IsValid())) {
    return nullptr;
  }

  {
    using PaintFrameFlags = nsLayoutUtils::PaintFrameFlags;
    gfxContext thebes(dt);
    // TODO: This matches the drawable code we use for -moz-element(), but is
    // this right?
    const PaintFrameFlags flags = PaintFrameFlags::InTransform;
    nsLayoutUtils::PaintFrame(&thebes, aFrame, rect, NS_RGBA(0, 0, 0, 0),
                              nsDisplayListBuilderMode::Painting, flags);
  }

  RefPtr<gfx::SourceSurface> surf = dt->GetBackingSurface();
  if (NS_WARN_IF(!surf)) {
    return nullptr;
  }
  return surf->GetDataSurface();
}

struct CapturedElementOldState {
  RefPtr<gfx::DataSourceSurface> mImage;

  // Encompasses width and height.
  nsSize mSize;
  CSSToCSSMatrix4x4Flagged mTransform;
  // Encompasses writing-mode / direction / text-orientation.
  WritingMode mWritingMode;
  StyleBlend mMixBlendMode = StyleBlend::Normal;
  StyleOwnedSlice<StyleFilter> mBackdropFilters;
  StyleColorSchemeFlags mColorScheme{0};

  CapturedElementOldState(nsIFrame* aFrame,
                          const nsSize& aSnapshotContainingBlockSize)
      : mImage(CaptureFallbackSnapshot(aFrame)),
        mSize(aFrame->Style()->IsRootElementStyle()
                  ? aSnapshotContainingBlockSize
                  : aFrame->GetRect().Size()),
        mTransform(EffectiveTransform(aFrame)),
        mWritingMode(aFrame->GetWritingMode()),
        mMixBlendMode(aFrame->StyleEffects()->mMixBlendMode),
        mBackdropFilters(aFrame->StyleEffects()->mBackdropFilters),
        mColorScheme(aFrame->StyleUI()->mColorScheme.bits) {}

  CapturedElementOldState() = default;
};

// https://drafts.csswg.org/css-view-transitions/#captured-element
struct ViewTransition::CapturedElement {
  CapturedElementOldState mOldState;
  RefPtr<Element> mNewElement;

  CapturedElement() = default;

  CapturedElement(nsIFrame* aFrame, const nsSize& aSnapshotContainingBlockSize)
      : mOldState(aFrame, aSnapshotContainingBlockSize) {}

  // TODO: Style definitions as per:
  // https://drafts.csswg.org/css-view-transitions/#captured-element-style-definitions
};

static inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCb,
    const ViewTransition::CapturedElement& aField, const char* aName,
    uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCb, aField.mNewElement, aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ViewTransition, mDocument,
                                      mUpdateCallback,
                                      mUpdateCallbackDonePromise, mReadyPromise,
                                      mFinishedPromise, mNamedElements,
                                      mViewTransitionRoot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ViewTransition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ViewTransition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ViewTransition)

ViewTransition::ViewTransition(Document& aDoc,
                               ViewTransitionUpdateCallback* aCb)
    : mDocument(&aDoc), mUpdateCallback(aCb) {}

ViewTransition::~ViewTransition() { ClearTimeoutTimer(); }

gfx::DataSourceSurface* ViewTransition::GetOldSurface(nsAtom* aName) const {
  auto el = mNamedElements.Get(aName);
  if (NS_WARN_IF(!el)) {
    return nullptr;
  }
  return el->mOldState.mImage;
}

nsIGlobalObject* ViewTransition::GetParentObject() const {
  return mDocument ? mDocument->GetParentObject() : nullptr;
}

Promise* ViewTransition::GetUpdateCallbackDone(ErrorResult& aRv) {
  if (!mUpdateCallbackDonePromise) {
    mUpdateCallbackDonePromise = Promise::Create(GetParentObject(), aRv);
  }
  return mUpdateCallbackDonePromise;
}

Promise* ViewTransition::GetReady(ErrorResult& aRv) {
  if (!mReadyPromise) {
    mReadyPromise = Promise::Create(GetParentObject(), aRv);
  }
  return mReadyPromise;
}

Promise* ViewTransition::GetFinished(ErrorResult& aRv) {
  if (!mFinishedPromise) {
    mFinishedPromise = Promise::Create(GetParentObject(), aRv);
  }
  return mFinishedPromise;
}

void ViewTransition::CallUpdateCallbackIgnoringErrors(CallIfDone aCallIfDone) {
  if (aCallIfDone == CallIfDone::No && mPhase == Phase::Done) {
    return;
  }
  CallUpdateCallback(IgnoreErrors());
}

// https://drafts.csswg.org/css-view-transitions-1/#call-the-update-callback
void ViewTransition::CallUpdateCallback(ErrorResult& aRv) {
  MOZ_ASSERT(mDocument);
  // Step 1:  Assert: transition's phase is "done", or before
  // "update-callback-called".
  MOZ_ASSERT(mPhase == Phase::Done ||
             UnderlyingValue(mPhase) <
                 UnderlyingValue(Phase::UpdateCallbackCalled));

  // Step 5: If transition's phase is not "done", then set transition's phase
  // to "update-callback-called".
  //
  // NOTE(emilio): This is swapped with the spec because the spec is broken,
  // see https://github.com/w3c/csswg-drafts/issues/10822
  if (mPhase != Phase::Done) {
    mPhase = Phase::UpdateCallbackCalled;
  }

  // Step 2: Let callbackPromise be null.
  RefPtr<Promise> callbackPromise;
  if (!mUpdateCallback) {
    // Step 3: If transition's update callback is null, then set callbackPromise
    // to a promise resolved with undefined, in transition’s relevant Realm.
    callbackPromise =
        Promise::CreateResolvedWithUndefined(GetParentObject(), aRv);
  } else {
    // Step 4: Otherwise set callbackPromise to the result of invoking
    // transition’s update callback. MOZ_KnownLive because the callback can only
    // go away when we get CCd.
    callbackPromise = MOZ_KnownLive(mUpdateCallback)->Call(aRv);
  }
  if (aRv.Failed()) {
    // TODO(emilio): Do we need extra error handling here?
    return;
  }
  MOZ_ASSERT(callbackPromise);
  // Step 8: React to callbackPromise with fulfillSteps and rejectSteps.
  callbackPromise->AddCallbacksWithCycleCollectedArgs(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult& aRv,
         ViewTransition* aVt) {
        // Step 6: Let fulfillSteps be to following steps:
        if (Promise* ucd = aVt->GetUpdateCallbackDone(aRv)) {
          // 6.1: Resolve transition's update callback done promise with
          // undefined.
          ucd->MaybeResolveWithUndefined();
        }
        if (aVt->mPhase == Phase::Done) {
          // "Skip a transition" step 8. We need to resolve "finished" after
          // update-callback-done.
          if (Promise* finished = aVt->GetFinished(aRv)) {
            finished->MaybeResolveWithUndefined();
          }
        }
        aVt->Activate();
      },
      [](JSContext*, JS::Handle<JS::Value> aReason, ErrorResult& aRv,
         ViewTransition* aVt) {
        // Step 7: Let rejectSteps be to following steps:
        if (Promise* ucd = aVt->GetUpdateCallbackDone(aRv)) {
          // 7.1: Reject transition's update callback done promise with reason.
          ucd->MaybeReject(aReason);
        }

        // 7.2: If transition's phase is "done", then return.
        if (aVt->mPhase == Phase::Done) {
          // "Skip a transition" step 8. We need to resolve "finished" after
          // update-callback-done.
          if (Promise* finished = aVt->GetFinished(aRv)) {
            finished->MaybeReject(aReason);
          }
          return;
        }

        // 7.3: Mark as handled transition's ready promise.
        if (Promise* ready = aVt->GetReady(aRv)) {
          MOZ_ALWAYS_TRUE(ready->SetAnyPromiseIsHandled());
        }
        aVt->SkipTransition(SkipTransitionReason::UpdateCallbackRejected,
                            aReason);
      },
      RefPtr(this));

  // Step 9: To skip a transition after a timeout, the user agent may perform
  // the following steps in parallel:
  MOZ_ASSERT(!mTimeoutTimer);
  ClearTimeoutTimer();  // Be safe just in case.
  mTimeoutTimer = NS_NewTimer();
  mTimeoutTimer->InitWithNamedFuncCallback(
      TimeoutCallback, this, StaticPrefs::dom_viewTransitions_timeout_ms(),
      nsITimer::TYPE_ONE_SHOT, "ViewTransition::TimeoutCallback");
}

void ViewTransition::ClearTimeoutTimer() {
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }
}

void ViewTransition::TimeoutCallback(nsITimer* aTimer, void* aClosure) {
  RefPtr vt = static_cast<ViewTransition*>(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(aTimer == vt->mTimeoutTimer);
  vt->Timeout();
}

void ViewTransition::Timeout() {
  ClearTimeoutTimer();
  if (mPhase != Phase::Done && mDocument) {
    SkipTransition(SkipTransitionReason::Timeout);
  }
}

static already_AddRefed<Element> MakePseudo(Document& aDoc,
                                            PseudoStyleType aType,
                                            nsAtom* aName) {
  RefPtr<Element> el = aDoc.CreateHTMLElement(nsGkAtoms::div);
  if (!aName) {
    MOZ_ASSERT(aType == PseudoStyleType::viewTransition);
    el->SetIsNativeAnonymousRoot();
  }
  el->SetPseudoElementType(aType);
  if (aName) {
    el->SetAttr(nsGkAtoms::name, nsDependentAtomString(aName), IgnoreErrors());
  }
  // This is not needed, but useful for debugging.
  el->SetAttr(nsGkAtoms::type,
              nsDependentAtomString(nsCSSPseudoElements::GetPseudoAtom(aType)),
              IgnoreErrors());
  return el.forget();
}

// https://drafts.csswg.org/css-view-transitions-1/#setup-transition-pseudo-elements
void ViewTransition::SetupTransitionPseudoElements() {
  MOZ_ASSERT(!mViewTransitionRoot);

  nsAutoScriptBlocker scriptBlocker;

  RefPtr docElement = mDocument->GetRootElement();
  if (!docElement) {
    return;
  }

  // Step 1 is a declaration.

  // Step 2: Set document's show view transition tree to true.
  // (we lazily create this pseudo-element so we don't need the flag for now at
  // least).
  mViewTransitionRoot =
      MakePseudo(*mDocument, PseudoStyleType::viewTransition, nullptr);
#ifdef DEBUG
  // View transition pseudos don't care about frame tree ordering, so can be
  // restyled just fine.
  mViewTransitionRoot->SetProperty(nsGkAtoms::restylableAnonymousNode,
                                   reinterpret_cast<void*>(true));
#endif
  // Step 3: For each transitionName -> capturedElement of transition’s named
  // elements:
  for (auto& entry : mNamedElements) {
    // We don't need to notify while constructing the tree.
    constexpr bool kNotify = false;

    nsAtom* transitionName = entry.GetKey();
    const CapturedElement& capturedElement = *entry.GetData();
    // Let group be a new ::view-transition-group(), with its view transition
    // name set to transitionName.
    RefPtr<Element> group = MakePseudo(
        *mDocument, PseudoStyleType::viewTransitionGroup, transitionName);
    // Append group to transition’s transition root pseudo-element.
    mViewTransitionRoot->AppendChildTo(group, kNotify, IgnoreErrors());
    // Let imagePair be a new ::view-transition-image-pair(), with its view
    // transition name set to transitionName.
    RefPtr<Element> imagePair = MakePseudo(
        *mDocument, PseudoStyleType::viewTransitionImagePair, transitionName);
    // Append imagePair to group.
    group->AppendChildTo(imagePair, kNotify, IgnoreErrors());
    // If capturedElement's old image is not null, then:
    if (capturedElement.mOldState.mImage) {
      // Let old be a new ::view-transition-old(), with its view transition
      // name set to transitionName, displaying capturedElement's old image as
      // its replaced content.
      RefPtr<Element> old = MakePseudo(
          *mDocument, PseudoStyleType::viewTransitionOld, transitionName);
      // Append old to imagePair.
      imagePair->AppendChildTo(old, kNotify, IgnoreErrors());
    }
    // If capturedElement's new element is not null, then:
    if (capturedElement.mNewElement) {
      // Let new be a new ::view-transition-new(), with its view transition
      // name set to transitionName.
      RefPtr<Element> new_ = MakePseudo(
          *mDocument, PseudoStyleType::viewTransitionNew, transitionName);
      // Append new to imagePair.
      imagePair->AppendChildTo(new_, kNotify, IgnoreErrors());
    }
    // TODO(emilio): Dynamic UA sheet shenanigans. Seems we could have a custom
    // element class with a transition-specific
  }
  BindContext context(*docElement, BindContext::ForNativeAnonymous);
  if (NS_FAILED(mViewTransitionRoot->BindToTree(context, *docElement))) {
    mViewTransitionRoot->UnbindFromTree();
    mViewTransitionRoot = nullptr;
    return;
  }
  if (PresShell* ps = mDocument->GetPresShell()) {
    ps->ContentAppended(mViewTransitionRoot);
  }
}

// https://drafts.csswg.org/css-view-transitions-1/#activate-view-transition
void ViewTransition::Activate() {
  // Step 1: If transition's phase is "done", then return.
  if (mPhase == Phase::Done) {
    return;
  }

  // TODO(emilio): Step 2: Set rendering suppression for view transitions to
  // false.

  // Step 3: If transition's initial snapshot containing block size is not
  // equal to the snapshot containing block size, then skip the view transition
  // for transition, and return.
  if (mInitialSnapshotContainingBlockSize !=
      SnapshotContainingBlockRect().Size()) {
    return SkipTransition(SkipTransitionReason::Resize);
  }

  // Step 4: Capture the new state for transition.
  if (auto skipReason = CaptureNewState()) {
    // If failure is returned, then skip the view transition for transition...
    return SkipTransition(*skipReason);
  }

  // TODO(emilio): Step 5.

  // Step 6: Setup transition pseudo-elements for transition.
  SetupTransitionPseudoElements();

  // TODO(emilio): Step 7.

  // Step 8: Set transition's phase to "animating".
  mPhase = Phase::Animating;
  // Step 9: Resolve transition's ready promise.
  if (Promise* ready = GetReady(IgnoreErrors())) {
    ready->MaybeResolveWithUndefined();
  }
}

// https://drafts.csswg.org/css-view-transitions/#perform-pending-transition-operations
void ViewTransition::PerformPendingOperations() {
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(mDocument->GetActiveViewTransition() == this);

  switch (mPhase) {
    case Phase::PendingCapture:
      return Setup();
    case Phase::Animating:
      return HandleFrame();
    default:
      break;
  }
}

// https://drafts.csswg.org/css-view-transitions/#snapshot-containing-block
nsRect ViewTransition::SnapshotContainingBlockRect() const {
  nsPresContext* pc = mDocument->GetPresContext();
  // TODO(emilio): Ensure this is correct with Android's dynamic toolbar and
  // scrollbars.
  return pc ? pc->GetVisibleArea() : nsRect();
}

// FIXME(emilio): This should actually iterate in paint order.
template <typename Callback>
static bool ForEachChildFrame(nsIFrame* aFrame, const Callback& aCb) {
  if (!aCb(aFrame)) {
    return false;
  }
  for (auto& [list, id] : aFrame->ChildLists()) {
    for (nsIFrame* f : list) {
      if (!ForEachChildFrame(f, aCb)) {
        return false;
      }
    }
  }
  return true;
}

template <typename Callback>
static void ForEachFrame(Document* aDoc, const Callback& aCb) {
  PresShell* ps = aDoc->GetPresShell();
  if (!ps) {
    return;
  }
  nsIFrame* root = ps->GetRootFrame();
  if (!root) {
    return;
  }
  ForEachChildFrame(root, aCb);
}

// https://drafts.csswg.org/css-view-transitions-1/#document-scoped-view-transition-name
static nsAtom* DocumentScopedTransitionNameFor(nsIFrame* aFrame) {
  auto* name = aFrame->StyleUIReset()->mViewTransitionName._0.AsAtom();
  if (name->IsEmpty()) {
    return nullptr;
  }
  // TODO(emilio): This isn't quite correct, per spec we're supposed to only
  // honor names coming from the document, but that's quite some magic,
  // and it's getting actively discussed, see:
  // https://github.com/w3c/csswg-drafts/issues/10808 and related
  return name;
}

// https://drafts.csswg.org/css-view-transitions/#capture-the-old-state
Maybe<SkipTransitionReason> ViewTransition::CaptureOldState() {
  MOZ_ASSERT(mNamedElements.IsEmpty());
  // Steps 1/2 are variable declarations.
  // Step 3: Let usedTransitionNames be a new set of strings.
  nsTHashSet<nsAtom*> usedTransitionNames;
  // Step 4: Let captureElements be a new list of elements.
  AutoTArray<std::pair<nsIFrame*, nsAtom*>, 32> captureElements;

  // Step 5: If the snapshot containing block size exceeds an
  // implementation-defined maximum, then return failure.
  // TODO(emilio): Implement a maximum if we deem it needed.
  //
  // Step 6: Set transition's initial snapshot containing block size to the
  // snapshot containing block size.
  mInitialSnapshotContainingBlockSize = SnapshotContainingBlockRect().Size();

  // Step 7: For each element of every element that is connected, and has a node
  // document equal to document, in paint order:
  Maybe<SkipTransitionReason> result;
  ForEachFrame(mDocument, [&](nsIFrame* aFrame) {
    auto* name = DocumentScopedTransitionNameFor(aFrame);
    if (!name) {
      // As a fast path we check for v-t-n first.
      // If transitionName is none, or element is not rendered, then continue.
      return true;
    }
    if (aFrame->IsHiddenByContentVisibilityOnAnyAncestor()) {
      // If any flat tree ancestor of this element skips its contents, then
      // continue.
      return true;
    }
    if (aFrame->GetPrevContinuation() || aFrame->GetNextContinuation()) {
      // If element has more than one box fragment, then continue.
      return true;
    }
    if (!usedTransitionNames.EnsureInserted(name)) {
      // If usedTransitionNames contains transitionName, then return failure.
      result.emplace(
          SkipTransitionReason::DuplicateTransitionNameCapturingOldState);
      return false;
    }
    // TODO: Set element's captured in a view transition to true.
    // (but note https://github.com/w3c/csswg-drafts/issues/11058).
    captureElements.AppendElement(std::make_pair(aFrame, name));
    return true;
  });

  if (result) {
    return result;
  }

  // Step 8: For each element in captureElements:
  for (auto& [f, name] : captureElements) {
    MOZ_ASSERT(f);
    MOZ_ASSERT(f->GetContent()->IsElement());
    auto capture =
        MakeUnique<CapturedElement>(f, mInitialSnapshotContainingBlockSize);
    mNamedElements.InsertOrUpdate(name, std::move(capture));
  }

  // TODO step 9: For each element in captureElements, set element's captured
  // in a view transition to false.

  return result;
}

// https://drafts.csswg.org/css-view-transitions-1/#capture-the-new-state
Maybe<SkipTransitionReason> ViewTransition::CaptureNewState() {
  nsTHashSet<nsAtom*> usedTransitionNames;
  Maybe<SkipTransitionReason> result;
  ForEachFrame(mDocument, [&](nsIFrame* aFrame) {
    // As a fast path we check for v-t-n first.
    auto* name = DocumentScopedTransitionNameFor(aFrame);
    if (!name) {
      return true;
    }
    if (aFrame->IsHiddenByContentVisibilityOnAnyAncestor()) {
      // If any flat tree ancestor of this element skips its contents, then
      // continue.
      return true;
    }
    if (aFrame->GetPrevContinuation() || aFrame->GetNextContinuation()) {
      // If element has more than one box fragment, then continue.
      return true;
    }
    if (!usedTransitionNames.EnsureInserted(name)) {
      result.emplace(
          SkipTransitionReason::DuplicateTransitionNameCapturingNewState);
      return false;
    }
    auto& capturedElement = mNamedElements.LookupOrInsertWith(
        name, [&] { return MakeUnique<CapturedElement>(); });
    capturedElement->mNewElement = aFrame->GetContent()->AsElement();
    return true;
  });
  return result;
}

// https://drafts.csswg.org/css-view-transitions/#setup-view-transition
void ViewTransition::Setup() {
  // Step 2: Capture the old state for transition.
  if (auto skipReason = CaptureOldState()) {
    // If failure is returned, then skip the view transition for transition
    // with an "InvalidStateError" DOMException in transition’s relevant Realm,
    // and return.
    return SkipTransition(*skipReason);
  }

  // TODO Step 3: Set document's rendering suppression for view transitions to
  // true.

  // Step 4: Queue a global task on the DOM manipulation task source, given
  // transition's relevant global object, to perform the following steps:
  //   4.1: If transition's phase is "done", then abort these steps. That is
  //        achieved via CallIfDone::No.
  //   4.2: call the update callback.
  mDocument->Dispatch(NewRunnableMethod<CallIfDone>(
      "ViewTransition::CallUpdateCallbackFromSetup", this,
      &ViewTransition::CallUpdateCallbackIgnoringErrors, CallIfDone::No));
}

// https://drafts.csswg.org/css-view-transitions-1/#handle-transition-frame
void ViewTransition::HandleFrame() {
  // TODO(emilio): Steps 1-3: Compute active animations.
  bool hasActiveAnimations = false;
  // Step 4: If hasActiveAnimations is false:
  if (!hasActiveAnimations) {
    // 4.1: Set transition's phase to "done".
    mPhase = Phase::Done;
    // 4.2: Clear view transition transition.
    ClearActiveTransition();
    // 4.3: Resolve transition's finished promise.
    if (Promise* finished = GetFinished(IgnoreErrors())) {
      finished->MaybeResolveWithUndefined();
    }
    return;
  }
  // TODO(emilio): Steps 5-6 (check CB size, update pseudo styles).
}

void ViewTransition::ClearNamedElements() {
  // TODO(emilio): TODO: Set element's captured in a view transition to false.
  mNamedElements.Clear();
}

// https://drafts.csswg.org/css-view-transitions-1/#clear-view-transition
void ViewTransition::ClearActiveTransition() {
  // Steps 1-2
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(mDocument->GetActiveViewTransition() == this);

  // Step 3
  ClearNamedElements();

  // Step 4: Clear show transition tree flag (we just destroy the pseudo tree,
  // see SetupTransitionPseudoElements).
  if (mViewTransitionRoot) {
    nsAutoScriptBlocker scriptBlocker;
    if (PresShell* ps = mDocument->GetPresShell()) {
      ps->ContentWillBeRemoved(mViewTransitionRoot);
    }
    mViewTransitionRoot->UnbindFromTree();
    mViewTransitionRoot = nullptr;
  }
  mDocument->ClearActiveViewTransition();
}

void ViewTransition::SkipTransition(SkipTransitionReason aReason) {
  SkipTransition(aReason, JS::UndefinedHandleValue);
}

// https://drafts.csswg.org/css-view-transitions-1/#skip-the-view-transition
// https://drafts.csswg.org/css-view-transitions-1/#dom-viewtransition-skiptransition
void ViewTransition::SkipTransition(
    SkipTransitionReason aReason,
    JS::Handle<JS::Value> aUpdateCallbackRejectReason) {
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT_IF(aReason != SkipTransitionReason::JS, mPhase != Phase::Done);
  MOZ_ASSERT_IF(aReason != SkipTransitionReason::UpdateCallbackRejected,
                aUpdateCallbackRejectReason == JS::UndefinedHandleValue);
  if (mPhase == Phase::Done) {
    return;
  }
  // Step 3: If transition's phase is before "update-callback-called", then
  // queue a global task on the DOM manipulation task source, given
  // transition’s relevant global object, to call the update callback of
  // transition.
  if (UnderlyingValue(mPhase) < UnderlyingValue(Phase::UpdateCallbackCalled)) {
    mDocument->Dispatch(NewRunnableMethod<CallIfDone>(
        "ViewTransition::CallUpdateCallbackFromSkip", this,
        &ViewTransition::CallUpdateCallbackIgnoringErrors, CallIfDone::Yes));
  }

  // Step 4: Set rendering suppression for view transitions to false.
  // TODO(emilio): We don't have that flag yet.

  // Step 5: If document's active view transition is transition, Clear view
  // transition transition.
  if (mDocument->GetActiveViewTransition() == this) {
    ClearActiveTransition();
  }

  // Step 6: Set transition's phase to "done".
  mPhase = Phase::Done;

  // Step 7: Reject transition's ready promise with reason.
  if (Promise* readyPromise = GetReady(IgnoreErrors())) {
    switch (aReason) {
      case SkipTransitionReason::JS:
        readyPromise->MaybeRejectWithAbortError(
            "Skipped ViewTransition due to skipTransition() call");
        break;
      case SkipTransitionReason::ClobberedActiveTransition:
        readyPromise->MaybeRejectWithAbortError(
            "Skipped ViewTransition due to another transition starting");
        break;
      case SkipTransitionReason::DocumentHidden:
        readyPromise->MaybeRejectWithAbortError(
            "Skipped ViewTransition due to document being hidden");
        break;
      case SkipTransitionReason::Timeout:
        readyPromise->MaybeRejectWithAbortError(
            "Skipped ViewTransition due to timeout");
        break;
      case SkipTransitionReason::DuplicateTransitionNameCapturingOldState:
        readyPromise->MaybeRejectWithInvalidStateError(
            "Duplicate view-transition-name value while capturing old state");
        break;
      case SkipTransitionReason::DuplicateTransitionNameCapturingNewState:
        readyPromise->MaybeRejectWithInvalidStateError(
            "Duplicate view-transition-name value while capturing new state");
        break;
      case SkipTransitionReason::Resize:
        readyPromise->MaybeRejectWithInvalidStateError(
            "Skipped view transition due to viewport resize");
        break;
      case SkipTransitionReason::UpdateCallbackRejected:
        readyPromise->MaybeReject(aUpdateCallbackRejectReason);
        break;
    }
  }

  // Step 8: Resolve transition's finished promise with the result of reacting
  // to transition's update callback done promise.
  //
  // This is done in CallUpdateCallback()
}

JSObject* ViewTransition::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return ViewTransition_Binding::Wrap(aCx, this, aGivenProto);
}

};  // namespace mozilla::dom
