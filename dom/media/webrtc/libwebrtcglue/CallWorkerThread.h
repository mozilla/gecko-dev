/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_CALLWORKERTHREAD_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_CALLWORKERTHREAD_H_

#include "mozilla/AbstractThread.h"
#include "nsIDirectTaskDispatcher.h"
#include "TaskQueueWrapper.h"

namespace mozilla {

// Implements AbstractThread for running things on the webrtc TaskQueue.
// Webrtc TaskQueues are not refcounted so cannot implement AbstractThread
// directly.
class CallWorkerThread final : public AbstractThread,
                               public nsIDirectTaskDispatcher {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDIRECTTASKDISPATCHER

  explicit CallWorkerThread(
      UniquePtr<TaskQueueWrapper<DeletionPolicy::NonBlocking>> aWebrtcTaskQueue)
      : AbstractThread(aWebrtcTaskQueue->mTaskQueue->SupportsTailDispatch()),
        mWebrtcTaskQueue(std::move(aWebrtcTaskQueue)) {}

  // AbstractThread overrides
  nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                    DispatchReason aReason = NormalDispatch) override;
  bool IsCurrentThreadIn() const override;
  TaskDispatcher& TailDispatcher() override;
  nsIEventTarget* AsEventTarget() override;
  NS_IMETHOD
  DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                  uint32_t aDelayMs) override;

  NS_IMETHOD RegisterShutdownTask(nsITargetShutdownTask* aTask) override;
  NS_IMETHOD UnregisterShutdownTask(nsITargetShutdownTask* aTask) override;

  const UniquePtr<TaskQueueWrapper<DeletionPolicy::NonBlocking>>
      mWebrtcTaskQueue;

 protected:
  ~CallWorkerThread() = default;
};

}  // namespace mozilla

#endif
