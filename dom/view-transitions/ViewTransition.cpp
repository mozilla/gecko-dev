/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ViewTransition.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/ViewTransitionBinding.h"
#include "nsITimer.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ViewTransition, mDocument,
                                      mUpdateCallback,
                                      mUpdateCallbackDonePromise, mReadyPromise,
                                      mFinishedPromise)

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

// https://drafts.csswg.org/css-view-transitions-1/#activate-view-transition
void ViewTransition::Activate() {
  // Step 1: If transition's phase is "done", then return.
  if (mPhase == Phase::Done) {
    return;
  }

  // TODO(emilio): Steps 2-7.

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

// https://drafts.csswg.org/css-view-transitions/#setup-view-transition
void ViewTransition::Setup() {
  // TODO(emilio): Steps 1-3: Capture old state.
  //
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

// https://drafts.csswg.org/css-view-transitions-1/#clear-view-transition
void ViewTransition::ClearActiveTransition() {
  // Steps 1-2
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(mDocument->GetActiveViewTransition() == this);

  // TODO(emilio): Step 3 (clear named elements)
  // TODO(emilio): Step 4 (clear show transition tree flag)
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
