/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRewind.h"

#include "ipc/ChildInternal.h"
#include "ipc/ParentInternal.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/StaticMutex.h"
#include "InfallibleVector.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"

namespace mozilla {
namespace recordreplay {

// The most recent checkpoint which was encountered.
static size_t gLastCheckpoint = InvalidCheckpointId;

// Lock for managing pending main thread callbacks.
static Monitor* gMainThreadCallbackMonitor;

// Callbacks to execute on the main thread, in FIFO order. Protected by
// gMainThreadCallbackMonitor.
static StaticInfallibleVector<std::function<void()>> gMainThreadCallbacks;

void InitializeRewindState() {
  gMainThreadCallbackMonitor = new Monitor();
}

// Time when the first checkpoint was taken.
static TimeStamp gFirstCheckpointTime;

// Time when the last checkpoint was taken.
static TimeStamp gLastCheckpointTime;

// Total time when the recording process was paused.
static TimeDuration gPausedTime;

TimeDuration CurrentRecordingTime() {
  MOZ_RELEASE_ASSERT(gFirstCheckpointTime);

  // gPausedTime is not updated when replaying.
  RecordReplayBytes(&gPausedTime, sizeof(gPausedTime));
  return TimeStamp::Now() - gFirstCheckpointTime - gPausedTime;
}

void CreateCheckpoint() {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  if (!HasDivergedFromRecording() &&
      !child::PaintingInProgress() &&
      js::CanCreateCheckpoint()) {
    gLastCheckpoint++;
    gLastCheckpointTime = TimeStamp::Now();

    if (gLastCheckpoint == FirstCheckpointId) {
      gFirstCheckpointTime = gLastCheckpointTime;
    }

    RecordReplayBytes(&gPausedTime, sizeof(gPausedTime));
    js::HitCheckpoint(gLastCheckpoint, CurrentRecordingTime());
  }
}

// Normally we only create checkpoints when painting or instructed to
// by the middleman. If this much time has elapsed then we will create
// checkpoints at the top of the main thread's message loop.
static const size_t CheckpointThresholdMs = 200;

void MaybeCreateCheckpoint() {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  if (gLastCheckpointTime &&
      (TimeStamp::Now() - gLastCheckpointTime).ToMilliseconds() > CheckpointThresholdMs) {
    CreateCheckpoint();
  }
}

static bool gUnhandledDivergeAllowed;

void DivergeFromRecording() {
  MOZ_RELEASE_ASSERT(IsReplaying());

  Thread* thread = Thread::Current();
  MOZ_RELEASE_ASSERT(thread->IsMainThread());

  gUnhandledDivergeAllowed = true;

  if (!thread->HasDivergedFromRecording()) {
    thread->DivergeFromRecording();

    // Direct all other threads to diverge from the recording as well.
    Thread::WaitForIdleThreads();
    for (size_t i = MainThreadId + 1; i <= MaxThreadId; i++) {
      Thread::GetById(i)->SetShouldDivergeFromRecording();
    }
    Thread::ResumeIdleThreads();
  }
}

extern "C" {

MOZ_EXPORT bool RecordReplayInterface_InternalHasDivergedFromRecording() {
  Thread* thread = Thread::Current();
  return thread && thread->HasDivergedFromRecording();
}

}  // extern "C"

void DisallowUnhandledDivergeFromRecording() {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  gUnhandledDivergeAllowed = false;
}

void EnsureNotDivergedFromRecording(const Maybe<int>& aCallId) {
  AssertEventsAreNotPassedThrough();
  if (HasDivergedFromRecording()) {
    MOZ_RELEASE_ASSERT(gUnhandledDivergeAllowed);

    PrintSpew("Unhandled recording divergence: %s\n",
              aCallId.isSome() ? GetRedirection(aCallId.ref()).mName : "");

    child::ReportUnhandledDivergence();
    Unreachable();
  }
}

size_t GetLastCheckpoint() { return gLastCheckpoint; }

static bool gMainThreadShouldPause = false;

bool MainThreadShouldPause() { return gMainThreadShouldPause; }

void PauseMainThreadAndServiceCallbacks() {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(gFirstCheckpointTime);
  AssertEventsAreNotPassedThrough();

  // Whether there is a PauseMainThreadAndServiceCallbacks frame on the stack.
  static bool gMainThreadIsPaused = false;

  if (gMainThreadIsPaused) {
    return;
  }
  gMainThreadIsPaused = true;

  TimeStamp startTime;
  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    startTime = TimeStamp::Now();
  }

  MOZ_RELEASE_ASSERT(!HasDivergedFromRecording());

  MonitorAutoLock lock(*gMainThreadCallbackMonitor);

  // Loop and invoke callbacks until one of them unpauses this thread.
  while (gMainThreadShouldPause) {
    if (!gMainThreadCallbacks.empty()) {
      std::function<void()> callback = gMainThreadCallbacks[0];
      gMainThreadCallbacks.erase(&gMainThreadCallbacks[0]);
      {
        MonitorAutoUnlock unlock(*gMainThreadCallbackMonitor);
        AutoDisallowThreadEvents disallow;
        callback();
      }
    } else {
      gMainThreadCallbackMonitor->Wait();
    }
  }

  // We shouldn't resume the main thread while it still has callbacks.
  MOZ_RELEASE_ASSERT(gMainThreadCallbacks.empty());

  // If we diverge from the recording we can't resume normal execution.
  MOZ_RELEASE_ASSERT(!HasDivergedFromRecording());

  if (IsRecording()) {
    AutoPassThroughThreadEvents pt;
    TimeStamp endTime = TimeStamp::Now();
    gPausedTime += endTime - startTime;
  }

  gMainThreadIsPaused = false;
}

void PauseMainThreadAndInvokeCallback(const std::function<void()>& aCallback) {
  {
    MonitorAutoLock lock(*gMainThreadCallbackMonitor);
    gMainThreadShouldPause = true;
    gMainThreadCallbacks.append(aCallback);
    gMainThreadCallbackMonitor->Notify();
  }

  if (Thread::CurrentIsMainThread()) {
    PauseMainThreadAndServiceCallbacks();
  }
}

// After forking, the child process does not respawn its threads until
// needed. Child processes will generally either sit idle and only fork more
// processes, or run forward a brief distance, do some operation and then
// terminate. Avoiding respawning the non-main threads in the former case
// greatly reduces pressure on the kernel from having many forks around.
static bool gNeedRespawnThreads;

void EnsureNonMainThreadsAreSpawned() {
  if (gNeedRespawnThreads) {
    AutoPassThroughThreadEvents pt;
    Thread::RespawnAllThreadsAfterFork();
    Thread::OperateOnIdleThreadLocks(Thread::OwnedLockState::NeedAcquire);
    Thread::ResumeIdleThreads();
    gNeedRespawnThreads = false;
  }
}

void ResumeExecution() {
  EnsureNonMainThreadsAreSpawned();

  MonitorAutoLock lock(*gMainThreadCallbackMonitor);
  gMainThreadShouldPause = false;
  gMainThreadCallbackMonitor->Notify();
}

bool ForkProcess() {
  MOZ_RELEASE_ASSERT(IsReplaying());

  if (!gNeedRespawnThreads) {
    Thread::WaitForIdleThreads();

    // Before forking all other threads need to release any locks they are
    // holding. After the fork the new process will only have a main thread and
    // will not be able to acquire any lock held at the time of the fork.
    Thread::OperateOnIdleThreadLocks(Thread::OwnedLockState::NeedRelease);
  }

  AutoEnsurePassThroughThreadEvents pt;
  pid_t pid = fork();

  if (pid > 0) {
    if (!gNeedRespawnThreads) {
      Thread::OperateOnIdleThreadLocks(Thread::OwnedLockState::NeedAcquire);
      Thread::ResumeIdleThreads();
    }
    return true;
  }

  Print("FORKED %d\n", getpid());

  if (TestEnv("MOZ_REPLAYING_WAIT_AT_FORK")) {
    BusyWait();
  }

  ResetPid();

  gNeedRespawnThreads = true;
  return false;
}

}  // namespace recordreplay
}  // namespace mozilla
