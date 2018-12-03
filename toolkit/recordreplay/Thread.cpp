/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Thread.h"

#include "ipc/ChildIPC.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/ThreadLocal.h"
#include "ChunkAllocator.h"
#include "MemorySnapshot.h"
#include "ProcessRewind.h"
#include "SpinLock.h"
#include "ThreadSnapshot.h"

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Thread Organization
///////////////////////////////////////////////////////////////////////////////

static MOZ_THREAD_LOCAL(Thread*) gTlsThreadKey;

/* static */ Monitor* Thread::gMonitor;

/* static */ Thread* Thread::Current() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  Thread* thread = gTlsThreadKey.get();
  if (!thread && IsReplaying()) {
    // Disable system threads when replaying.
    WaitForeverNoIdle();
  }
  return thread;
}

/* static */ bool Thread::CurrentIsMainThread() {
  Thread* thread = Current();
  return thread && thread->IsMainThread();
}

void Thread::BindToCurrent() {
  MOZ_ASSERT(!mStackBase);
  gTlsThreadKey.set(this);

  mNativeId = pthread_self();
  size_t size = pthread_get_stacksize_np(mNativeId);
  uint8_t* base = (uint8_t*)pthread_get_stackaddr_np(mNativeId) - size;

  // Lock if we will be notifying later on. We don't do this for the main
  // thread because we haven't initialized enough state yet that we can use
  // a monitor.
  Maybe<MonitorAutoLock> lock;
  if (mId != MainThreadId) {
    lock.emplace(*gMonitor);
  }

  mStackBase = base;
  mStackSize = size;

  // Notify WaitUntilInitialized if it is waiting for this thread to start.
  if (mId != MainThreadId) {
    gMonitor->NotifyAll();
  }
}

// All threads, indexed by the thread ID.
static Thread* gThreads;

/* static */ Thread* Thread::GetById(size_t aId) {
  MOZ_ASSERT(aId);
  MOZ_ASSERT(aId <= MaxThreadId);
  return &gThreads[aId];
}

/* static */ Thread* Thread::GetByNativeId(NativeThreadId aNativeId) {
  for (size_t id = MainThreadId; id <= MaxRecordedThreadId; id++) {
    Thread* thread = GetById(id);
    if (thread->mNativeId == aNativeId) {
      return thread;
    }
  }
  return nullptr;
}

/* static */ Thread* Thread::GetByStackPointer(void* aSp) {
  if (!gThreads) {
    return nullptr;
  }
  for (size_t i = MainThreadId; i <= MaxThreadId; i++) {
    Thread* thread = &gThreads[i];
    if (MemoryContains(thread->mStackBase, thread->mStackSize, aSp)) {
      return thread;
    }
  }
  return nullptr;
}

/* static */ void Thread::InitializeThreads() {
  gThreads = new Thread[MaxThreadId + 1];
  for (size_t i = MainThreadId; i <= MaxThreadId; i++) {
    Thread* thread = &gThreads[i];
    PodZero(thread);
    new (thread) Thread();

    thread->mId = i;

    if (i <= MaxRecordedThreadId) {
      thread->mEvents = gRecordingFile->OpenStream(StreamName::Event, i);
    }

    DirectCreatePipe(&thread->mNotifyfd, &thread->mIdlefd);
  }

  if (!gTlsThreadKey.init()) {
    MOZ_CRASH();
  }
}

/* static */ void Thread::WaitUntilInitialized(Thread* aThread) {
  MonitorAutoLock lock(*gMonitor);
  while (!aThread->mStackBase) {
    gMonitor->Wait();
  }
}

/* static */ void Thread::ThreadMain(void* aArgument) {
  MOZ_ASSERT(IsRecordingOrReplaying());

  Thread* thread = (Thread*)aArgument;
  MOZ_ASSERT(thread->mId > MainThreadId);

  thread->BindToCurrent();

  while (true) {
    // Wait until this thread has been given a start routine.
    while (true) {
      {
        MonitorAutoLock lock(*gMonitor);
        if (thread->mStart) {
          break;
        }
      }
      Wait();
    }

    {
      Maybe<AutoPassThroughThreadEvents> pt;
      if (!thread->IsRecordedThread()) pt.emplace();
      thread->mStart(thread->mStartArg);
    }

    MonitorAutoLock lock(*gMonitor);

    // Clear the start routine to indicate to other threads that this one has
    // finished executing.
    thread->mStart = nullptr;
    thread->mStartArg = nullptr;

    // Notify any other thread waiting for this to finish in JoinThread.
    gMonitor->NotifyAll();
  }
}

/* static */ void Thread::SpawnAllThreads() {
  MOZ_ASSERT(AreThreadEventsPassedThrough());

  InitializeThreadSnapshots(MaxRecordedThreadId + 1);

  gMonitor = new Monitor();

  // All Threads are spawned up front. This allows threads to be scanned
  // (e.g. in ReplayUnlock) without worrying about racing with other threads
  // being spawned.
  for (size_t i = MainThreadId + 1; i <= MaxRecordedThreadId; i++) {
    SpawnThread(GetById(i));
  }
}

// The number of non-recorded threads that have been spawned.
static Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve>
    gNumNonRecordedThreads;

/* static */ Thread* Thread::SpawnNonRecordedThread(Callback aStart,
                                                    void* aArgument) {
  if (IsMiddleman()) {
    DirectSpawnThread(aStart, aArgument);
    return nullptr;
  }

  size_t id = MaxRecordedThreadId + ++gNumNonRecordedThreads;
  MOZ_RELEASE_ASSERT(id <= MaxThreadId);

  Thread* thread = GetById(id);
  thread->mStart = aStart;
  thread->mStartArg = aArgument;

  SpawnThread(thread);
  return thread;
}

/* static */ void Thread::SpawnThread(Thread* aThread) {
  DirectSpawnThread(ThreadMain, aThread);
  WaitUntilInitialized(aThread);
}

/* static */ NativeThreadId Thread::StartThread(Callback aStart,
                                                void* aArgument,
                                                bool aNeedsJoin) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return 0;
  }

  MonitorAutoLock lock(*gMonitor);

  size_t id = 0;
  if (IsRecording()) {
    // Look for an idle thread.
    for (id = MainThreadId + 1; id <= MaxRecordedThreadId; id++) {
      Thread* targetThread = Thread::GetById(id);
      if (!targetThread->mStart && !targetThread->mNeedsJoin) {
        break;
      }
    }
    if (id >= MaxRecordedThreadId) {
      child::ReportFatalError(Nothing(), "Too many threads");
    }
    MOZ_RELEASE_ASSERT(id <= MaxRecordedThreadId);
  }
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::CreateThread);
  thread->Events().RecordOrReplayScalar(&id);

  Thread* targetThread = GetById(id);

  // Block until the thread is ready for a new start routine.
  while (targetThread->mStart) {
    MOZ_RELEASE_ASSERT(IsReplaying());
    gMonitor->Wait();
  }

  targetThread->mStart = aStart;
  targetThread->mStartArg = aArgument;
  targetThread->mNeedsJoin = aNeedsJoin;

  // Notify the thread in case it is waiting for a start routine under
  // ThreadMain.
  Notify(id);

  return targetThread->mNativeId;
}

void Thread::Join() {
  MOZ_ASSERT(!AreThreadEventsPassedThrough());

  EnsureNotDivergedFromRecording();

  while (true) {
    MonitorAutoLock lock(*gMonitor);
    if (!mStart) {
      MOZ_RELEASE_ASSERT(mNeedsJoin);
      mNeedsJoin = false;
      break;
    }
    gMonitor->Wait();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Thread Public API Accessors
///////////////////////////////////////////////////////////////////////////////

extern "C" {

MOZ_EXPORT void RecordReplayInterface_InternalBeginPassThroughThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  if (!gInitializationFailureMessage) {
    Thread::Current()->SetPassThrough(true);
  }
}

MOZ_EXPORT void RecordReplayInterface_InternalEndPassThroughThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  if (!gInitializationFailureMessage) {
    Thread::Current()->SetPassThrough(false);
  }
}

MOZ_EXPORT bool RecordReplayInterface_InternalAreThreadEventsPassedThrough() {
  MOZ_ASSERT(IsRecordingOrReplaying());

  // If initialization fails, pass through all thread events until we're able
  // to report the problem to the middleman and die.
  if (gInitializationFailureMessage) {
    return true;
  }

  Thread* thread = Thread::Current();
  return !thread || thread->PassThroughEvents();
}

MOZ_EXPORT void RecordReplayInterface_InternalBeginDisallowThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  Thread::Current()->BeginDisallowEvents();
}

MOZ_EXPORT void RecordReplayInterface_InternalEndDisallowThreadEvents() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  Thread::Current()->EndDisallowEvents();
}

MOZ_EXPORT bool RecordReplayInterface_InternalAreThreadEventsDisallowed() {
  MOZ_ASSERT(IsRecordingOrReplaying());
  Thread* thread = Thread::Current();
  return thread && thread->AreEventsDisallowed();
}

}  // extern "C"

///////////////////////////////////////////////////////////////////////////////
// Thread Coordination
///////////////////////////////////////////////////////////////////////////////

/* static */ void Thread::WaitForIdleThreads() {
  MOZ_RELEASE_ASSERT(CurrentIsMainThread());

  MonitorAutoLock lock(*gMonitor);
  for (size_t i = MainThreadId + 1; i <= MaxRecordedThreadId; i++) {
    Thread* thread = GetById(i);
    thread->mShouldIdle = true;
    thread->mUnrecordedWaitNotified = false;
  }
  while (true) {
    bool done = true;
    for (size_t i = MainThreadId + 1; i <= MaxRecordedThreadId; i++) {
      Thread* thread = GetById(i);
      if (!thread->mIdle) {
        done = false;

        // Check if there is a callback we can invoke to get this thread to
        // make progress. The mUnrecordedWaitOnlyWhenDiverged flag is used to
        // avoid perturbing the behavior of threads that may or may not be
        // waiting on an unrecorded resource, depending on whether they have
        // diverged from the recording yet.
        if (thread->mUnrecordedWaitCallback &&
            !thread->mUnrecordedWaitNotified) {
          // Set this flag before releasing the idle lock. Otherwise it's
          // possible the thread could call NotifyUnrecordedWait while we
          // aren't holding the lock, and we would set the flag afterwards
          // without first invoking the callback.
          thread->mUnrecordedWaitNotified = true;

          // Release the idle lock here to avoid any risk of deadlock.
          std::function<void()> callback = thread->mUnrecordedWaitCallback;
          {
            MonitorAutoUnlock unlock(*gMonitor);
            AutoPassThroughThreadEvents pt;
            callback();
          }

          // Releasing the global lock means that we need to start over
          // checking whether there are any idle threads. By marking this
          // thread as having been notified we have made progress, however.
          done = true;
          i = MainThreadId;
        }
      }
    }
    if (done) {
      break;
    }
    MonitorAutoUnlock unlock(*gMonitor);
    WaitNoIdle();
  }
}

/* static */ void Thread::ResumeSingleIdleThread(size_t aId) {
  GetById(aId)->mShouldIdle = false;
  Notify(aId);
}

/* static */ void Thread::ResumeIdleThreads() {
  MOZ_RELEASE_ASSERT(CurrentIsMainThread());
  for (size_t i = MainThreadId + 1; i <= MaxRecordedThreadId; i++) {
    ResumeSingleIdleThread(i);
  }
}

void Thread::NotifyUnrecordedWait(
    const std::function<void()>& aNotifyCallback) {
  if (IsMainThread()) {
    return;
  }

  MonitorAutoLock lock(*gMonitor);
  if (mUnrecordedWaitCallback) {
    // Per the documentation for NotifyUnrecordedWait, we need to call the
    // routine after a notify, even if the routine has been called already
    // since the main thread started to wait for idle replay threads.
    mUnrecordedWaitNotified = false;
  } else {
    MOZ_RELEASE_ASSERT(!mUnrecordedWaitNotified);
  }

  mUnrecordedWaitCallback = aNotifyCallback;

  // The main thread might be able to make progress now by calling the routine
  // if it is waiting for idle replay threads.
  if (mShouldIdle) {
    Notify(MainThreadId);
  }
}

bool Thread::MaybeWaitForCheckpointSave(
    const std::function<void()>& aReleaseCallback) {
  MOZ_RELEASE_ASSERT(!PassThroughEvents());
  if (IsMainThread()) {
    return false;
  }
  MonitorAutoLock lock(*gMonitor);
  if (!mShouldIdle) {
    return false;
  }
  aReleaseCallback();
  while (mShouldIdle) {
    MonitorAutoUnlock unlock(*gMonitor);
    Wait();
  }
  return true;
}

/* static */ void Thread::WaitNoIdle() {
  Thread* thread = Current();
  uint8_t data = 0;
  size_t read = DirectRead(thread->mIdlefd, &data, 1);
  MOZ_RELEASE_ASSERT(read == 1);
}

/* static */ void Thread::Wait() {
  Thread* thread = Current();
  MOZ_ASSERT(!thread->mIdle);
  MOZ_ASSERT(thread->IsRecordedThread() && !thread->PassThroughEvents());

  if (thread->IsMainThread()) {
    WaitNoIdle();
    return;
  }

  // The state saved for a thread needs to match up with the most recent
  // point at which it became idle, so that when the main thread saves the
  // stacks from all threads it saves those stacks at the right point.
  // SaveThreadState might trigger thread events, so make sure they are
  // passed through.
  thread->SetPassThrough(true);
  int stackSeparator = 0;
  if (!SaveThreadState(thread->Id(), &stackSeparator)) {
    // We just restored a checkpoint, notify the main thread since it is waiting
    // for all threads to restore their stacks.
    Notify(MainThreadId);
  }

  thread->mIdle = true;
  if (thread->mShouldIdle) {
    // Notify the main thread that we just became idle.
    Notify(MainThreadId);
  }

  do {
    // Do the actual waiting for another thread to notify this one.
    WaitNoIdle();

    // Rewind this thread if the main thread told us to do so. The main
    // thread is responsible for rewinding its own stack.
    if (ShouldRestoreThreadStack(thread->Id())) {
      RestoreThreadStack(thread->Id());
      Unreachable();
    }
  } while (thread->mShouldIdle);

  thread->mIdle = false;
  thread->SetPassThrough(false);
}

/* static */ void Thread::WaitForever() {
  while (true) {
    Wait();
  }
  Unreachable();
}

/* static */ void Thread::WaitForeverNoIdle() {
  FileHandle writeFd, readFd;
  DirectCreatePipe(&writeFd, &readFd);
  while (true) {
    uint8_t data;
    DirectRead(readFd, &data, 1);
  }
}

/* static */ void Thread::Notify(size_t aId) {
  uint8_t data = 0;
  DirectWrite(GetById(aId)->mNotifyfd, &data, 1);
}

}  // namespace recordreplay
}  // namespace mozilla
