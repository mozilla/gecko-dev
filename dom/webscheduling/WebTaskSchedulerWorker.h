/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebTaskSchedulerWorker_h
#define mozilla_dom_WebTaskSchedulerWorker_h

#include "WebTaskScheduler.h"

#include "mozilla/LinkedList.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WebTaskSchedulingBinding.h"

namespace mozilla::dom {

class WebTaskWorkerRunnable final : public WorkerSameThreadRunnable {
 public:
  explicit WebTaskWorkerRunnable(WebTaskSchedulerWorker* aSchedulerWorker);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

 private:
  ~WebTaskWorkerRunnable() = default;
  WeakPtr<WebTaskSchedulerWorker> mSchedulerWorker;
};

class WebTaskSchedulerWorker final : public WebTaskScheduler {
 public:
  static RefPtr<WebTaskSchedulerWorker> Create(WorkerPrivate* aWorkerPrivate);

  explicit WebTaskSchedulerWorker(WorkerPrivate* aWorkerPrivate);

  void Disconnect() override;

  void IncreaseNumNormalOrHighPriorityQueuesHaveTaskScheduled() override;
  void DecreaseNumNormalOrHighPriorityQueuesHaveTaskScheduled() override;

  bool HasScheduledNormalOrHighPriorityWebTasks() const {
    return mNumHighPriorityQueuesHaveTaskScheduled;
  }

 private:
  ~WebTaskSchedulerWorker() = default;

  nsresult SetTimeoutForDelayedTask(WebTask* aTask, uint64_t aDelay,
                                    EventQueuePriority aPriority) override;
  bool DispatchEventLoopRunnable(EventQueuePriority aPriority) override;

  RefPtr<StrongWorkerRef> mWorkerRef;
  bool mWorkerIsShuttingDown{false};

  // Unlike window global where multiple globals can share the
  // same event loop, worker globals don't share event loops,
  // so it's okay to have this counter lives inside the
  // scheduler for workers.
  uint32_t mNumHighPriorityQueuesHaveTaskScheduled = 0;
};
}  // namespace mozilla::dom
#endif
