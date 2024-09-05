/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"

#include "mozilla/dom/AbortSignalBinding.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"

namespace mozilla::dom {

// AbortSignalImpl
// ----------------------------------------------------------------------------

AbortSignalImpl::AbortSignalImpl(bool aAborted, JS::Handle<JS::Value> aReason)
    : mReason(aReason), mAborted(aAborted) {
  MOZ_ASSERT_IF(!mReason.isUndefined(), mAborted);
}

bool AbortSignalImpl::Aborted() const { return mAborted; }

void AbortSignalImpl::GetReason(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aReason) {
  if (!mAborted) {
    return;
  }
  MaybeAssignAbortError(aCx);
  aReason.set(mReason);
}

JS::Value AbortSignalImpl::RawReason() const { return mReason.get(); }

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
void AbortSignalImpl::SignalAbort(JS::Handle<JS::Value> aReason) {
  // Step 1: If signal is aborted, then return.
  if (mAborted) {
    return;
  }

  // Step 2: Set signal’s abort reason to reason if it is given; otherwise to a
  // new "AbortError" DOMException.
  //
  // (But given AbortSignalImpl is supposed to run without JS context, the
  // DOMException creation is deferred to the getter.)
  SetAborted(aReason);

  // Step 3 - 6
  SignalAbortWithDependents();
}

void AbortSignalImpl::SignalAbortWithDependents() {
  // AbortSignalImpl cannot have dependents, so just run abort steps for itself.
  RunAbortSteps();
}

// https://dom.spec.whatwg.org/#run-the-abort-steps
// This skips event firing as AbortSignalImpl is not supposed to be exposed to
// JS. It's done instead in AbortSignal::RunAbortSteps.
void AbortSignalImpl::RunAbortSteps() {
  // Step 1: For each algorithm of signal’s abort algorithms: run algorithm.
  //
  // When there are multiple followers, the follower removal algorithm
  // https://dom.spec.whatwg.org/#abortsignal-remove could be invoked in an
  // earlier algorithm to remove a later algorithm, so |mFollowers| must be a
  // |nsTObserverArray| to defend against mutation.
  for (RefPtr<AbortFollower>& follower : mFollowers.ForwardRange()) {
    MOZ_ASSERT(follower->mFollowingSignal == this);
    follower->RunAbortAlgorithm();
  }

  // Step 2: Empty signal’s abort algorithms.
  UnlinkFollowers();
}

void AbortSignalImpl::SetAborted(JS::Handle<JS::Value> aReason) {
  mAborted = true;
  mReason = aReason;
}

void AbortSignalImpl::Traverse(AbortSignalImpl* aSignal,
                               nsCycleCollectionTraversalCallback& cb) {
  ImplCycleCollectionTraverse(cb, aSignal->mFollowers, "mFollowers", 0);
}

void AbortSignalImpl::Unlink(AbortSignalImpl* aSignal) {
  aSignal->mReason.setUndefined();
  aSignal->UnlinkFollowers();
}

void AbortSignalImpl::MaybeAssignAbortError(JSContext* aCx) {
  MOZ_ASSERT(mAborted);
  if (!mReason.isUndefined()) {
    return;
  }

  JS::Rooted<JS::Value> exception(aCx);
  RefPtr<DOMException> dom = DOMException::Create(NS_ERROR_DOM_ABORT_ERR);

  if (NS_WARN_IF(!ToJSValue(aCx, dom, &exception))) {
    return;
  }

  mReason.set(exception);
}

void AbortSignalImpl::UnlinkFollowers() {
  // Manually unlink all followers before destructing the array, or otherwise
  // the array will be accessed by Unfollow() while being destructed.
  for (RefPtr<AbortFollower>& follower : mFollowers.ForwardRange()) {
    follower->mFollowingSignal = nullptr;
  }
  mFollowers.Clear();
}

// AbortSignal
// ----------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AbortSignal,
                                                  DOMEventTargetHelper)
  AbortSignalImpl::Traverse(static_cast<AbortSignalImpl*>(tmp), cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDependentSignals)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AbortSignal,
                                                DOMEventTargetHelper)
  AbortSignalImpl::Unlink(static_cast<AbortSignalImpl*>(tmp));
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDependentSignals)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(AbortSignal,
                                               DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReason)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(AbortSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AbortSignal, DOMEventTargetHelper)

AbortSignal::AbortSignal(nsIGlobalObject* aGlobalObject, bool aAborted,
                         JS::Handle<JS::Value> aReason)
    : DOMEventTargetHelper(aGlobalObject),
      AbortSignalImpl(aAborted, aReason),
      mDependent(false) {
  mozilla::HoldJSObjects(this);
}

JSObject* AbortSignal::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return AbortSignal_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<AbortSignal> AbortSignal::Abort(
    GlobalObject& aGlobal, JS::Handle<JS::Value> aReason) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<AbortSignal> abortSignal = new AbortSignal(global, true, aReason);
  return abortSignal.forget();
}

class AbortSignalTimeoutHandler final : public TimeoutHandler {
 public:
  AbortSignalTimeoutHandler(JSContext* aCx, AbortSignal* aSignal)
      : TimeoutHandler(aCx), mSignal(aSignal) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AbortSignalTimeoutHandler)

  // https://dom.spec.whatwg.org/#dom-abortsignal-timeout
  // Step 3
  MOZ_CAN_RUN_SCRIPT bool Call(const char* /* unused */) override {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mSignal->GetParentObject()))) {
      // (false is only for setInterval, see
      // nsGlobalWindowInner::RunTimeoutHandler)
      return true;
    }

    // Step 1. Queue a global task on the timer task source given global to
    // signal abort given signal and a new "TimeoutError" DOMException.
    JS::Rooted<JS::Value> exception(jsapi.cx());
    RefPtr<DOMException> dom = DOMException::Create(NS_ERROR_DOM_TIMEOUT_ERR);
    if (NS_WARN_IF(!ToJSValue(jsapi.cx(), dom, &exception))) {
      return true;
    }

    mSignal->SignalAbort(exception);
    return true;
  }

 private:
  ~AbortSignalTimeoutHandler() override = default;

  RefPtr<AbortSignal> mSignal;
};

NS_IMPL_CYCLE_COLLECTION(AbortSignalTimeoutHandler, mSignal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AbortSignalTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbortSignalTimeoutHandler)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortSignalTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static void SetTimeoutForGlobal(GlobalObject& aGlobal, TimeoutHandler& aHandler,
                                int32_t timeout, ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> innerWindow =
        do_QueryInterface(aGlobal.GetAsSupports());
    if (!innerWindow) {
      aRv.ThrowInvalidStateError("Could not find window.");
      return;
    }

    int32_t handle;
    nsresult rv =
        nsGlobalWindowInner::Cast(innerWindow)
            ->GetTimeoutManager()
            ->SetTimeout(&aHandler, timeout, /* aIsInterval */ false,
                         Timeout::Reason::eAbortSignalTimeout, &handle);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  } else {
    WorkerPrivate* workerPrivate =
        GetWorkerPrivateFromContext(aGlobal.Context());
    workerPrivate->SetTimeout(aGlobal.Context(), &aHandler, timeout,
                              /* aIsInterval */ false,
                              Timeout::Reason::eAbortSignalTimeout, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

// https://dom.spec.whatwg.org/#dom-abortsignal-timeout
already_AddRefed<AbortSignal> AbortSignal::Timeout(GlobalObject& aGlobal,
                                                   uint64_t aMilliseconds,
                                                   ErrorResult& aRv) {
  // Step 2. Let global be signal’s relevant global object.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  // Step 1. Let signal be a new AbortSignal object.
  RefPtr<AbortSignal> signal =
      new AbortSignal(global, false, JS::UndefinedHandleValue);

  // Step 3. Run steps after a timeout given global, "AbortSignal-timeout",
  // milliseconds, and the following step: ...
  RefPtr<TimeoutHandler> handler =
      new AbortSignalTimeoutHandler(aGlobal.Context(), signal);

  // Note: We only supports int32_t range intervals
  int32_t timeout =
      aMilliseconds > uint64_t(std::numeric_limits<int32_t>::max())
          ? std::numeric_limits<int32_t>::max()
          : static_cast<int32_t>(aMilliseconds);

  SetTimeoutForGlobal(aGlobal, *handler, timeout, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 4. Return signal.
  return signal.forget();
}

// https://dom.spec.whatwg.org/#create-a-dependent-abort-signal
already_AddRefed<AbortSignal> AbortSignal::Any(
    GlobalObject& aGlobal,
    const Sequence<OwningNonNull<AbortSignal>>& aSignals) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  mozilla::Span span{aSignals.Elements(), aSignals.Length()};
  return Any(global, span);
}

already_AddRefed<AbortSignal> AbortSignal::Any(
    nsIGlobalObject* aGlobal,
    const Span<const OwningNonNull<AbortSignal>>& aSignals) {
  // Step 1. Let resultSignal be a new object implementing AbortSignal using
  // realm
  RefPtr<AbortSignal> resultSignal =
      new AbortSignal(aGlobal, false, JS::UndefinedHandleValue);

  if (!aSignals.IsEmpty()) {
    // (Prepare for step 2 which uses the reason of this. Cannot use
    // RawReason because that can cause constructing new DOMException for each
    // dependent signal instead of sharing the single one.)
    AutoJSAPI jsapi;
    if (!jsapi.Init(aGlobal)) {
      return nullptr;
    }
    JSContext* cx = jsapi.cx();

    // Step 2. For each signal of signals: if signal is aborted, then set
    // resultSignal's abort reason to signal's abort reason and return
    // resultSignal.
    for (const auto& signal : aSignals) {
      if (signal->Aborted()) {
        JS::Rooted<JS::Value> reason(cx);
        signal->GetReason(cx, &reason);
        resultSignal->SetAborted(reason);
        return resultSignal.forget();
      }
    }
  }

  // Step 3. Set resultSignal's dependent to true
  resultSignal->mDependent = true;

  // Step 4. For each signal of signals
  for (const auto& signal : aSignals) {
    if (!signal->Dependent()) {
      // Step 4.1. If signal is not dependent, make resultSignal dependent on it
      resultSignal->MakeDependentOn(signal);
    } else {
      // Step 4.2. Otherwise, make resultSignal dependent on its source signals
      for (const auto& sourceSignal : signal->mSourceSignals) {
        if (!sourceSignal) {
          // Bug 1908466, sourceSignal might have been garbage collected.
          // As signal is not aborted, sourceSignal also wasn't.
          // Thus do not depend on it, as it cannot be aborted anymore.
          continue;
        }
        MOZ_ASSERT(!sourceSignal->Aborted() && !sourceSignal->Dependent());
        resultSignal->MakeDependentOn(sourceSignal);
      }
    }
  }

  // Step 5. Return resultSignal.
  return resultSignal.forget();
}

void AbortSignal::MakeDependentOn(AbortSignal* aSignal) {
  MOZ_ASSERT(mDependent);
  MOZ_ASSERT(aSignal);
  // append only if not already contained in list
  // https://infra.spec.whatwg.org/#set-append
  if (!mSourceSignals.Contains(aSignal)) {
    mSourceSignals.AppendElement(aSignal);
  }
  if (!aSignal->mDependentSignals.Contains(this)) {
    aSignal->mDependentSignals.AppendElement(this);
  }
}

// https://dom.spec.whatwg.org/#dom-abortsignal-throwifaborted
void AbortSignal::ThrowIfAborted(JSContext* aCx, ErrorResult& aRv) {
  aRv.MightThrowJSException();

  if (Aborted()) {
    JS::Rooted<JS::Value> reason(aCx);
    GetReason(aCx, &reason);
    aRv.ThrowJSException(aCx, reason);
  }
}

// Step 3 - 6 of https://dom.spec.whatwg.org/#abortsignal-signal-abort
void AbortSignal::SignalAbortWithDependents() {
  // Step 3: Let dependentSignalsToAbort be a new list.
  nsTArray<RefPtr<AbortSignal>> dependentSignalsToAbort;

  // mDependentSignals can go away after this function.
  nsTArray<RefPtr<AbortSignal>> dependentSignals = std::move(mDependentSignals);

  if (!dependentSignals.IsEmpty()) {
    // (Prepare for step 4.1.1 which uses the reason of this. Cannot use
    // RawReason because that can cause constructing new DOMException for each
    // dependent signal instead of sharing the single one.)
    AutoJSAPI jsapi;
    if (!jsapi.Init(GetParentObject())) {
      return;
    }
    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> reason(cx);
    GetReason(cx, &reason);

    // Step 4. For each dependentSignal of signal’s dependent signals:
    for (const auto& dependentSignal : dependentSignals) {
      MOZ_ASSERT(dependentSignal->mSourceSignals.Contains(this));
      // Step 4.1: If dependentSignal is not aborted, then:
      if (!dependentSignal->Aborted()) {
        // Step 4.1.1: Set dependentSignal’s abort reason to signal’s abort
        // reason.
        dependentSignal->SetAborted(reason);
        // Step 4.1.2: Append dependentSignal to dependentSignalsToAbort.
        dependentSignalsToAbort.AppendElement(dependentSignal);
      }
    }
  }

  // Step 5: Run the abort steps for signal.
  RunAbortSteps();

  // Step 6: For each dependentSignal of dependentSignalsToAbort, run the abort
  // steps for dependentSignal.
  for (const auto& dependentSignal : dependentSignalsToAbort) {
    dependentSignal->RunAbortSteps();
  }
}

// https://dom.spec.whatwg.org/#run-the-abort-steps
void AbortSignal::RunAbortSteps() {
  // Step 1 - 2:
  AbortSignalImpl::RunAbortSteps();

  // Step 3. Fire an event named abort at this signal.
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, u"abort"_ns, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

bool AbortSignal::Dependent() const { return mDependent; }

AbortSignal::~AbortSignal() { mozilla::DropJSObjects(this); }

// AbortFollower
// ----------------------------------------------------------------------------

AbortFollower::~AbortFollower() { Unfollow(); }

// https://dom.spec.whatwg.org/#abortsignal-add
void AbortFollower::Follow(AbortSignalImpl* aSignal) {
  // Step 1.
  if (aSignal->mAborted) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(aSignal);

  Unfollow();

  // Step 2.
  mFollowingSignal = aSignal;
  MOZ_ASSERT(!aSignal->mFollowers.Contains(this));
  aSignal->mFollowers.AppendElement(this);
}

// https://dom.spec.whatwg.org/#abortsignal-remove
void AbortFollower::Unfollow() {
  if (mFollowingSignal) {
    // |Unfollow| is called by cycle-collection unlink code that runs in no
    // guaranteed order.  So we can't, symmetric with |Follow| above, assert
    // that |this| will be found in |mFollowingSignal->mFollowers|.
    mFollowingSignal->mFollowers.RemoveElement(this);
    mFollowingSignal = nullptr;
  }
}

bool AbortFollower::IsFollowing() const { return !!mFollowingSignal; }

}  // namespace mozilla::dom
