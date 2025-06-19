/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallWorkerThread.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(CallWorkerThread, nsIDirectTaskDispatcher,
                  nsISerialEventTarget, nsIEventTarget);

//-----------------------------------------------------------------------------
// AbstractThread
//-----------------------------------------------------------------------------

nsresult CallWorkerThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                    DispatchReason aReason) {
  return mWebrtcTaskQueue->mTaskQueue->Dispatch(std::move(aRunnable), aReason);
}

bool CallWorkerThread::IsCurrentThreadIn() const {
  return mWebrtcTaskQueue->mTaskQueue->IsOnCurrentThreadInfallible() &&
         mWebrtcTaskQueue->IsCurrent();
}

TaskDispatcher& CallWorkerThread::TailDispatcher() {
  return mWebrtcTaskQueue->mTaskQueue->TailDispatcher();
}

nsIEventTarget* CallWorkerThread::AsEventTarget() {
  return mWebrtcTaskQueue->mTaskQueue->AsEventTarget();
}

NS_IMETHODIMP
CallWorkerThread::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                  uint32_t aDelayMs) {
  return mWebrtcTaskQueue->mTaskQueue->DelayedDispatch(std::move(aEvent),
                                                       aDelayMs);
}

NS_IMETHODIMP CallWorkerThread::RegisterShutdownTask(
    nsITargetShutdownTask* aTask) {
  return mWebrtcTaskQueue->mTaskQueue->RegisterShutdownTask(aTask);
}

NS_IMETHODIMP CallWorkerThread::UnregisterShutdownTask(
    nsITargetShutdownTask* aTask) {
  return mWebrtcTaskQueue->mTaskQueue->UnregisterShutdownTask(aTask);
}

//-----------------------------------------------------------------------------
// nsIDirectTaskDispatcher
//-----------------------------------------------------------------------------

NS_IMETHODIMP
CallWorkerThread::DispatchDirectTask(already_AddRefed<nsIRunnable> aEvent) {
  return mWebrtcTaskQueue->mTaskQueue->DispatchDirectTask(std::move(aEvent));
}

NS_IMETHODIMP CallWorkerThread::DrainDirectTasks() {
  return mWebrtcTaskQueue->mTaskQueue->DrainDirectTasks();
}

NS_IMETHODIMP CallWorkerThread::HaveDirectTasks(bool* aValue) {
  return mWebrtcTaskQueue->mTaskQueue->HaveDirectTasks(aValue);
}

}  // namespace mozilla
