/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdleRequest.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/dom/IdleDeadline.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/WebTaskScheduler.h"

namespace mozilla::dom {

IdleRequest::IdleRequest(IdleRequestCallback* aCallback, uint32_t aHandle)
    : mCallback(aCallback), mHandle(aHandle), mTimeoutHandle(Nothing()) {
  MOZ_DIAGNOSTIC_ASSERT(mCallback);
}

IdleRequest::~IdleRequest() = default;

NS_IMPL_CYCLE_COLLECTION_CLASS(IdleRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IdleRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  if (tmp->isInList()) {
    tmp->remove();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IdleRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void IdleRequest::SetTimeoutHandle(int32_t aHandle) {
  mTimeoutHandle = Some(aHandle);
}

int32_t IdleRequest::GetTimeoutHandle() const {
  MOZ_DIAGNOSTIC_ASSERT(mTimeoutHandle.isSome());
  return mTimeoutHandle.value();
}

void IdleRequest::IdleRun(nsPIDOMWindowInner* aWindow,
                          DOMHighResTimeStamp aDeadline, bool aDidTimeout) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mCallback);

  RefPtr<IdleDeadline> deadline =
      new IdleDeadline(aWindow, aDidTimeout, aDeadline);
  RefPtr<IdleRequestCallback> callback(std::move(mCallback));
  MOZ_ASSERT(!mCallback);

  RefPtr<nsGlobalWindowInner> innerWindow = nsGlobalWindowInner::Cast(aWindow);
  // https://wicg.github.io/scheduling-apis/#sec-patches-invoke-idle-callbacks
  // Let state be a new scheduling state.
  RefPtr<WebTaskSchedulingState> newState = new WebTaskSchedulingState();
  // Set state’s priority source to the result of creating a fixed priority
  // unabortable task signal given "background" and realm.
  newState->SetPrioritySource(
      new TaskSignal(aWindow->AsGlobal(), TaskPriority::Background));
  // Set event loop’s current scheduling state to state.
  innerWindow->SetWebTaskSchedulingState(newState);
  callback->Call(*deadline, "requestIdleCallback handler");
  // Set event loop’s current scheduling state to null.
  innerWindow->SetWebTaskSchedulingState(nullptr);
}

}  // namespace mozilla::dom
