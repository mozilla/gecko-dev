/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTHashMap.h"
#include "WebTaskScheduler.h"
#include "WebTaskSchedulerWorker.h"
#include "WebTaskSchedulerMainThread.h"
#include "nsGlobalWindowInner.h"

#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/TimeoutManager.h"

namespace mozilla::dom {

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, WebTaskQueue& aQueue,
    const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aQueue.Tasks(), aName, aFlags);
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    const WebTaskQueueHashKey& aField, const char* aName, uint32_t aFlags = 0) {
  const WebTaskQueueHashKey::WebTaskQueueTypeKey& typeKey = aField.GetTypeKey();
  if (typeKey.is<RefPtr<TaskSignal>>()) {
    ImplCycleCollectionTraverse(aCallback, typeKey.as<RefPtr<TaskSignal>>(),
                                aName, aFlags);
  }
}

inline void ImplCycleCollectionUnlink(WebTaskQueueHashKey& aField) {
  WebTaskQueueHashKey::WebTaskQueueTypeKey& typeKey = aField.GetTypeKey();
  if (typeKey.is<RefPtr<TaskSignal>>()) {
    ImplCycleCollectionUnlink(typeKey.as<RefPtr<TaskSignal>>());
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WebTaskSchedulingState)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebTaskSchedulingState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAbortSource, mPrioritySource);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebTaskSchedulingState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAbortSource, mPrioritySource);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CLASS(WebTask)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebTask)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebTaskQueueHashKey)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSchedulingState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebTask)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebTaskQueueHashKey)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSchedulingState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebTask)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebTask)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTask)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(DelayedWebTaskHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DelayedWebTaskHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DelayedWebTaskHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DelayedWebTaskHandler)

WebTask::WebTask(uint32_t aEnqueueOrder,
                 const Maybe<SchedulerPostTaskCallback&>& aCallback,
                 WebTaskSchedulingState* aSchedlingState, Promise* aPromise,
                 WebTaskScheduler* aWebTaskScheduler,
                 const WebTaskQueueHashKey& aHashKey)
    : mEnqueueOrder(aEnqueueOrder),
      mPromise(aPromise),
      mHasScheduled(false),
      mSchedulingState(aSchedlingState),
      mScheduler(aWebTaskScheduler),
      mWebTaskQueueHashKey(aHashKey) {
  if (aCallback.isSome()) {
    mCallback = &aCallback.ref();
  }
}

void WebTask::RunAbortAlgorithm() {
  // no-op if WebTask::Run has been called already
  if (mPromise->State() == Promise::PromiseState::Pending) {
    // There are two things that can keep a WebTask alive, either the abort
    // signal or WebTaskQueue.
    // It's possible that this task get cleared out from the WebTaskQueue first,
    // and then the abort signal get aborted. For example, the callback function
    // was async and there's a signal.abort() call in the callback.
    if (isInList()) {
      remove();
    }

    AutoJSAPI jsapi;
    if (!jsapi.Init(mPromise->GetGlobalObject())) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    } else {
      JSContext* cx = jsapi.cx();
      JS::Rooted<JS::Value> reason(cx);
      Signal()->GetReason(cx, &reason);
      mPromise->MaybeReject(reason);
    }
  }

  MOZ_ASSERT(!isInList());
}

bool WebTask::Run() {
  MOZ_ASSERT(HasScheduled());
  MOZ_ASSERT(mScheduler);
  remove();

  mScheduler->RemoveEntryFromTaskQueueMapIfNeeded(mWebTaskQueueHashKey);
  ClearWebTaskScheduler();

  if (!mCallback) {
    // Scheduler.yield
    mPromise->MaybeResolveWithUndefined();
    MOZ_ASSERT(!isInList());
    return true;
  }

  MOZ_ASSERT(mSchedulingState);

  ErrorResult error;

  nsIGlobalObject* global = mPromise->GetGlobalObject();
  if (!global || global->IsDying()) {
    return false;
  }

  // 11.2.2 Set event loop’s current scheduling state to state.
  global->SetWebTaskSchedulingState(mSchedulingState);

  AutoJSAPI jsapi;
  if (!jsapi.Init(global)) {
    return false;
  }

  JS::Rooted<JS::Value> returnVal(jsapi.cx());

  MOZ_ASSERT(mPromise->State() == Promise::PromiseState::Pending);

  MOZ_KnownLive(mCallback)->Call(&returnVal, error, "WebTask",
                                 CallbackFunction::eRethrowExceptions);

  // 11.2.4 Set event loop’s current scheduling state to null.
  global->SetWebTaskSchedulingState(nullptr);

  error.WouldReportJSException();

#ifdef DEBUG
  Promise::PromiseState promiseState = mPromise->State();

  // If the state is Rejected, it means the above Call triggers the
  // RunAbortAlgorithm method and rejected the promise
  MOZ_ASSERT_IF(promiseState != Promise::PromiseState::Pending,
                promiseState == Promise::PromiseState::Rejected);
#endif

  if (error.Failed()) {
    if (!error.IsUncatchableException()) {
      mPromise->MaybeReject(std::move(error));
    } else {
      error.SuppressException();
    }
  } else {
    mPromise->MaybeResolve(returnVal);
  }

  MOZ_ASSERT(!isInList());
  return true;
}

inline void ImplCycleCollectionUnlink(
    nsTHashMap<WebTaskQueueHashKey, WebTaskQueue>& aField) {
  aField.Clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsTHashMap<WebTaskQueueHashKey, WebTaskQueue>& aField, const char* aName,
    uint32_t aFlags = 0) {
  for (auto& entry : aField) {
    ImplCycleCollectionTraverse(
        aCallback, entry.GetKey(),
        "nsTHashMap<WebTaskQueueHashKey, WebTaskQueue>::WebTaskQueueHashKey",
        aFlags);
    ImplCycleCollectionTraverse(
        aCallback, *entry.GetModifiableData(),
        "nsTHashMap<WebTaskQueueHashKey, WebTaskQueue>::WebTaskQueue", aFlags);
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebTaskScheduler, mParent, mWebTaskQueues)

/* static */
already_AddRefed<WebTaskSchedulerMainThread>
WebTaskScheduler::CreateForMainThread(nsGlobalWindowInner* aWindow) {
  RefPtr<WebTaskSchedulerMainThread> scheduler =
      new WebTaskSchedulerMainThread(aWindow->AsGlobal());
  return scheduler.forget();
}

already_AddRefed<WebTaskSchedulerWorker> WebTaskScheduler::CreateForWorker(
    WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  RefPtr<WebTaskSchedulerWorker> scheduler =
      WebTaskSchedulerWorker::Create(aWorkerPrivate);
  return scheduler.forget();
}

WebTaskScheduler::WebTaskScheduler(nsIGlobalObject* aParent)
    : mParent(aParent), mNextEnqueueOrder(1) {
  MOZ_ASSERT(aParent);
}

JSObject* WebTaskScheduler::WrapObject(JSContext* cx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return Scheduler_Binding::Wrap(cx, this, aGivenProto);
}

static bool ShouldRejectPromiseWithReasonCausedByAbortSignal(
    AbortSignal& aAbortSignal, nsIGlobalObject* aGlobal, Promise& aPromise) {
  MOZ_ASSERT(aGlobal);
  if (!aAbortSignal.Aborted()) {
    return false;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(aGlobal)) {
    aPromise.MaybeRejectWithNotSupportedError(
        "Failed to initialize the JS context");
    return true;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> reason(cx);
  aAbortSignal.GetReason(cx, &reason);
  aPromise.MaybeReject(reason);
  return true;
}

// https://wicg.github.io/scheduling-apis/#sec-scheduler-alg-scheduling-tasks-and-continuations
already_AddRefed<Promise> WebTaskScheduler::PostTask(
    SchedulerPostTaskCallback& aCallback,
    const SchedulerPostTaskOptions& aOptions) {
  const Optional<OwningNonNull<AbortSignal>>& taskSignal = aOptions.mSignal;
  const Optional<TaskPriority>& taskPriority = aOptions.mPriority;

  ErrorResult rv;
  // Instead of making WebTaskScheduler::PostTask throws, we always
  // create the promise and return it. This is because we need to
  // create the promise explicitly to be able to reject it with
  // signal's reason.
  RefPtr<Promise> promise = Promise::Create(mParent, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  nsIGlobalObject* global = GetParentObject();
  if (!global || global->IsDying()) {
    promise->MaybeRejectWithNotSupportedError("Current window is detached");
    return promise.forget();
  }

  // 4. Let state be a new scheduling state.
  RefPtr<WebTaskSchedulingState> newState = new WebTaskSchedulingState();
  AbortSignal* signalValue = nullptr;
  if (taskSignal.WasPassed()) {
    signalValue = &taskSignal.Value();
    // 3. If signal is not null and it is aborted, then reject result with
    // signal’s abort reason and return result.
    if (ShouldRejectPromiseWithReasonCausedByAbortSignal(*signalValue, global,
                                                         *promise)) {
      return promise.forget();
    }

    // 5. Set state’s abort source to signal.
    newState->SetAbortSource(signalValue);
  }

  if (taskPriority.WasPassed()) {
    // 6. If options["priority"] exists, then set state’s priority source to the
    // result of creating a fixed priority unabortable task signal given
    // options["priority"]
    newState->SetPrioritySource(
        new TaskSignal(GetParentObject(), taskPriority.Value()));
  } else if (signalValue && signalValue->IsTaskSignal()) {
    // 7. Otherwise if signal is not null and implements the TaskSignal
    // interface, then set state’s priority source to signal.
    newState->SetPrioritySource(signalValue);
  }

  if (!newState->GetPrioritySource()) {
    // 8. If state’s priority source is null, then set state’s priority
    // source to the result of creating a fixed priority unabortable task
    // signal given "user-visible".
    newState->SetPrioritySource(
        new TaskSignal(GetParentObject(), TaskPriority::User_visible));
  }

  const uint64_t delay = aOptions.mDelay;

  // Let queue be the result of selecting the scheduler task queue for
  // scheduler given signal and priority.
  RefPtr<WebTask> task =
      CreateTask(taskSignal, taskPriority, false /* aIsContinuation */,
                 SomeRef(aCallback), newState, promise);

  MOZ_ASSERT(newState->GetPrioritySource() &&
             newState->GetPrioritySource()->IsTaskSignal());

  const TaskSignal* finalPrioritySource =
      static_cast<TaskSignal*>(newState->GetPrioritySource());

  if (delay > 0) {
    nsresult rv = SetTimeoutForDelayedTask(
        task, delay, GetEventQueuePriority(finalPrioritySource->Priority()));
    if (NS_FAILED(rv)) {
      promise->MaybeRejectWithUnknownError(
          "Failed to setup timeout for delayed task");
    }
    return promise.forget();
  }

  if (!QueueTask(task,
                 GetEventQueuePriority(finalPrioritySource->Priority()))) {
    MOZ_ASSERT(task->isInList());
    task->remove();

    promise->MaybeRejectWithNotSupportedError("Unable to queue the task");
    return promise.forget();
  }

  return promise.forget();
}

// https://wicg.github.io/scheduling-apis/#schedule-a-yield-continuation
already_AddRefed<Promise> WebTaskScheduler::YieldImpl() {
  ErrorResult rv;
  // 1. Let result be a new promise.
  RefPtr<Promise> promise = Promise::Create(mParent, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  nsIGlobalObject* global = GetParentObject();
  if (!global || global->IsDying()) {
    promise->MaybeRejectWithNotSupportedError("Current window is detached");
    return promise.forget();
  }

  RefPtr<AbortSignal> abortSource;
  RefPtr<TaskSignal> prioritySource;
  // 2. Let inheritedState be the scheduler’s relevant agent's event loop's
  // current scheduling state.
  if (auto* schedulingState = global->GetWebTaskSchedulingState()) {
    // 3. Let abortSource be inheritedState’s abort source if inheritedState is
    // not null, or otherwise null.
    abortSource = schedulingState->GetAbortSource();
    // 5. Let prioritySource be inheritedState’s priority source if
    // inheritedState is not null, or otherwise null.
    if (AbortSignal* inheritedPrioritySource =
            schedulingState->GetPrioritySource()) {
      MOZ_ASSERT(inheritedPrioritySource->IsTaskSignal());
      prioritySource = static_cast<TaskSignal*>(inheritedPrioritySource);
    }
  }

  if (abortSource) {
    // 4. If abortSource is not null and abortSource is aborted, then reject
    // result with abortSource’s abort reason and return result.
    if (ShouldRejectPromiseWithReasonCausedByAbortSignal(*abortSource, global,
                                                         *promise)) {
      return promise.forget();
    }
  }

  if (!prioritySource) {
    // 6. If prioritySource is null, then set prioritySource to the result of
    // creating a fixed priority unabortable task signal given "user-visible".
    prioritySource =
        new TaskSignal(GetParentObject(), TaskPriority::User_visible);
  }

  const OwningNonNull<AbortSignal> owningSignal(*prioritySource);

  Optional<OwningNonNull<AbortSignal>> optionalSignal;
  optionalSignal.Construct(*prioritySource);

  // 9. Set handle’s queue to the result of selecting the scheduler task queue
  // for scheduler given prioritySource and true.
  // 10. Schedule a task to invoke an algorithm for scheduler given handle and
  // the following steps:
  RefPtr<WebTask> task =
      CreateTask(optionalSignal, {}, true /* aIsContinuation */, Nothing(),
                 nullptr, promise);

  EventQueuePriority eventQueuePriority =
      GetEventQueuePriority(prioritySource->Priority());
  if (!QueueTask(task, eventQueuePriority)) {
    MOZ_ASSERT(task->isInList());
    // CreateTask adds the task to WebTaskScheduler's queue, so we
    // need to remove from it when we failed to dispatch the runnable.
    task->remove();

    promise->MaybeRejectWithNotSupportedError("Unable to queue the task");
    return promise.forget();
  }

  return promise.forget();
}

already_AddRefed<WebTask> WebTaskScheduler::CreateTask(
    const Optional<OwningNonNull<AbortSignal>>& aSignal,
    const Optional<TaskPriority>& aPriority, bool aIsContinuation,
    const Maybe<SchedulerPostTaskCallback&>& aCallback,
    WebTaskSchedulingState* aSchedulingState, Promise* aPromise) {
  WebTaskScheduler::SelectedTaskQueueData selectedTaskQueueData =
      SelectTaskQueue(aSignal, aPriority, aIsContinuation);

  uint32_t nextEnqueueOrder = mNextEnqueueOrder;
  ++mNextEnqueueOrder;

  RefPtr<WebTask> task =
      new WebTask(nextEnqueueOrder, aCallback, aSchedulingState, aPromise, this,
                  selectedTaskQueueData.mSelectedQueueHashKey);

  selectedTaskQueueData.mSelectedTaskQueue.AddTask(task);

  if (aSignal.WasPassed()) {
    AbortSignal& signalValue = aSignal.Value();
    task->Follow(&signalValue);
  }

  return task.forget();
}

bool WebTaskScheduler::QueueTask(WebTask* aTask, EventQueuePriority aPriority) {
  if (!DispatchEventLoopRunnable(aPriority)) {
    return false;
  }
  MOZ_ASSERT(!aTask->HasScheduled());
  aTask->SetHasScheduled(true);
  return true;
}

WebTask* WebTaskScheduler::GetNextTask() {
// https://wicg.github.io/scheduling-apis/#select-the-next-scheduler-task-queue-from-all-schedulers
  // 1. Let queues be an empty set.
  AutoTArray<nsTArray<WebTaskQueue*>, WebTaskQueue::EffectivePriorityCount>
      allQueues;
  allQueues.SetLength(WebTaskQueue::EffectivePriorityCount);

  // 2. Let schedulers be the set of all Scheduler objects whose relevant
  // agent’s event loop is event loop and that have a runnable task.
  // 3. For each scheduler in schedulers, extend queues with the result of
  // getting the runnable task queues for scheduler.
  for (auto iter = mWebTaskQueues.Iter(); !iter.Done(); iter.Next()) {
    auto& queue = iter.Data();
    if (queue.HasScheduledTasks()) {
      const WebTaskQueueHashKey& key = iter.Key();
      nsTArray<WebTaskQueue*>& queuesForThisPriority =
          allQueues[key.EffectivePriority()];
      queuesForThisPriority.AppendElement(&queue);
    }
  }

  if (allQueues.IsEmpty()) {
    return nullptr;
  }

  // Reverse checking the queues, so it starts with the highest priority
  for (auto& queues : Reversed(allQueues)) {
    if (queues.IsEmpty()) {
      continue;
    }
    WebTaskQueue* oldestQueue = nullptr;
    for (auto& webTaskQueue : queues) {
      MOZ_ASSERT(webTaskQueue->HasScheduledTasks());
      if (!oldestQueue) {
        oldestQueue = webTaskQueue;
      } else {
        WebTask* firstScheduledRunnableForCurrentQueue =
            webTaskQueue->GetFirstScheduledTask();
        WebTask* firstScheduledRunnableForOldQueue =
            oldestQueue->GetFirstScheduledTask();
        if (firstScheduledRunnableForOldQueue->EnqueueOrder() >
            firstScheduledRunnableForCurrentQueue->EnqueueOrder()) {
          oldestQueue = webTaskQueue;
        }
      }
    }
    MOZ_ASSERT(oldestQueue);
    return oldestQueue->GetFirstScheduledTask();
  }
  return nullptr;
}

void WebTaskScheduler::Disconnect() { mWebTaskQueues.Clear(); }

void WebTaskScheduler::RunTaskSignalPriorityChange(TaskSignal* aTaskSignal) {
  if (auto entry = mWebTaskQueues.Lookup({aTaskSignal, false})) {
    entry.Data().SetPriority(aTaskSignal->Priority());
  }
}

WebTaskScheduler::SelectedTaskQueueData WebTaskScheduler::SelectTaskQueue(
    const Optional<OwningNonNull<AbortSignal>>& aSignal,
    const Optional<TaskPriority>& aPriority, const bool aIsContinuation) {
  bool useSignal = !aPriority.WasPassed() && aSignal.WasPassed() &&
                   aSignal.Value().IsTaskSignal();

  if (useSignal) {
    TaskSignal* taskSignal = static_cast<TaskSignal*>(&(aSignal.Value()));
    WebTaskQueueHashKey signalHashKey(taskSignal, aIsContinuation);
    WebTaskQueue& taskQueue =
        mWebTaskQueues.LookupOrInsert(signalHashKey, this);

    taskQueue.SetPriority(taskSignal->Priority());
    taskSignal->SetWebTaskScheduler(this);

    return SelectedTaskQueueData{WebTaskQueueHashKey(signalHashKey), taskQueue};
  }

  TaskPriority taskPriority =
      aPriority.WasPassed() ? aPriority.Value() : TaskPriority::User_visible;

  uint32_t staticTaskQueueMapKey = static_cast<uint32_t>(taskPriority);
  WebTaskQueueHashKey staticHashKey(staticTaskQueueMapKey, aIsContinuation);
  WebTaskQueue& taskQueue = mWebTaskQueues.LookupOrInsert(staticHashKey, this);
  taskQueue.SetPriority(taskPriority);

  return SelectedTaskQueueData{WebTaskQueueHashKey(staticHashKey), taskQueue};
}

EventQueuePriority WebTaskScheduler::GetEventQueuePriority(
    const TaskPriority& aPriority) const {
  switch (aPriority) {
    case TaskPriority::User_blocking:
    case TaskPriority::User_visible:
    case TaskPriority::Background:
      // Bug 1941888 intends to tweak the runnable priorities
      // for better results.
      return EventQueuePriority::Normal;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid TaskPriority");
      return EventQueuePriority::Normal;
  }
}

void WebTaskScheduler::RemoveEntryFromTaskQueueMapIfNeeded(
    const WebTaskQueueHashKey& aHashKey) {
  MOZ_ASSERT(mWebTaskQueues.Contains(aHashKey));
  if (auto entry = mWebTaskQueues.Lookup(aHashKey)) {
    WebTaskQueue& taskQueue = *entry;
    if (taskQueue.IsEmpty()) {
      DeleteEntryFromWebTaskQueueMap(aHashKey);
    }
  }
}
}  // namespace mozilla::dom
