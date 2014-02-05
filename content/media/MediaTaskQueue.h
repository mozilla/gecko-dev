/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaTaskQueue_h_
#define MediaTaskQueue_h_

#include <queue>
#include "mozilla/RefPtr.h"
#include "mozilla/Monitor.h"
#include "nsThreadUtils.h"

class nsIRunnable;

namespace mozilla {

class SharedThreadPool;

// Abstracts executing runnables in order in a thread pool. The runnables
// dispatched to the MediaTaskQueue will be executed in the order in which
// they're received, and are guaranteed to not be executed concurrently.
// They may be executed on different threads, and a memory barrier is used
// to make this threadsafe for objects that aren't already threadsafe.
class MediaTaskQueue : public AtomicRefCounted<MediaTaskQueue> {
public:
  MediaTaskQueue(TemporaryRef<SharedThreadPool> aPool);
  ~MediaTaskQueue();

  nsresult Dispatch(nsIRunnable* aRunnable);

  // Removes all pending tasks from the task queue, and blocks until
  // the currently running task (if any) finishes.
  void Flush();

  // Blocks until all tasks finish executing, then shuts down the task queue
  // and exits.
  void Shutdown();

  // Blocks until all task finish executing.
  void AwaitIdle();

  bool IsEmpty();

private:

  // Blocks until all task finish executing. Called internally by methods
  // that need to wait until the task queue is idle.
  // mQueueMonitor must be held.
  void AwaitIdleLocked();

  RefPtr<SharedThreadPool> mPool;

  // Monitor that protects the queue and mIsRunning;
  Monitor mQueueMonitor;

  // Queue of tasks to run.
  std::queue<RefPtr<nsIRunnable>> mTasks;

  // True if we've dispatched an event to the pool to execute events from
  // the queue.
  bool mIsRunning;

  // True if we've started our shutdown process.
  bool mIsShutdown;

  class Runner : public nsRunnable {
  public:
    Runner(MediaTaskQueue* aQueue)
      : mQueue(aQueue)
    {
    }
    NS_METHOD Run() MOZ_OVERRIDE;
  private:
    RefPtr<MediaTaskQueue> mQueue;
  };
};

} // namespace mozilla

#endif // MediaTaskQueue_h_
