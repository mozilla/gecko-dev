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

#include <unistd.h>

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

// Total idle time at the last checkpoint, in microseconds. Zero when replaying.
static double gLastCheckpointIdleTime;

// Last time when the recording was flushed.
static TimeStamp gLastFlushTime;

TimeDuration CurrentRecordingTime() {
  MOZ_RELEASE_ASSERT(gFirstCheckpointTime);

  return TimeStamp::Now() - gFirstCheckpointTime;
}

TimeDuration RecordingDuration() {
  MOZ_RELEASE_ASSERT(gFirstCheckpointTime);

  return gLastCheckpointTime - gFirstCheckpointTime;
}

// Note: Result will not be accurate when replaying.
static size_t NonIdleTimeSinceLastCheckpointMs() {
  double absoluteMs = (TimeStamp::Now() - gLastCheckpointTime).ToMilliseconds();
  double idleMs = (js::TotalIdleTime() - gLastCheckpointIdleTime) / 1000.0;
  return absoluteMs - idleMs;
}

static const uint32_t FlushIntervalMs = 500;

void CreateCheckpoint() {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  if (!HasDivergedFromRecording() && js::CanCreateCheckpoint()) {
    gLastCheckpoint++;

    child::MaybeSetCheckpointForLastPaint(gLastCheckpoint);

    size_t elapsed = 0;
    if (gLastCheckpoint != FirstCheckpointId) {
      elapsed = NonIdleTimeSinceLastCheckpointMs();
    }
    gLastCheckpointTime = TimeStamp::Now();
    gLastCheckpointIdleTime = js::TotalIdleTime();

    recordreplay::RecordReplayAssert("CreateCheckpoint %lu", gLastCheckpoint);

    if (gLastCheckpoint == FirstCheckpointId) {
      gFirstCheckpointTime = gLastCheckpointTime;
    }

    js::HitCheckpoint(gLastCheckpoint, CurrentRecordingTime());

    if (IsRecording()) {
      size_t time = (gLastCheckpointTime - gFirstCheckpointTime).ToMilliseconds();
      AddCheckpointSummary(*ExecutionProgressCounter(), elapsed, time);
    }

    if (gLastCheckpoint == FirstCheckpointId ||
        (gLastCheckpointTime - gLastFlushTime).ToMilliseconds() >= FlushIntervalMs) {
      FlushRecording(/* aFinishRecording */ false);
      gLastFlushTime = gLastCheckpointTime;
    }
  }
}

// Normally we only create checkpoints when painting or instructed to by the
// middleman. If this much time has elapsed (excluding idle time) then we will
// create checkpoints at the top of the main thread's message loop.
static const size_t CheckpointThresholdMs = 200;

void MaybeCreateCheckpoint() {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  if (gLastCheckpointTime) {
    double absoluteMs = (TimeStamp::Now() - gLastCheckpointTime).ToMilliseconds();
    double idleMs = (js::TotalIdleTime() - gLastCheckpointIdleTime) / 1000.0;
    if (RecordReplayValue(absoluteMs - idleMs > CheckpointThresholdMs)) {
      CreateCheckpoint();
    }
  }
}

// Ensure that non-main threads have been respawned after a fork.
static void EnsureNonMainThreadsAreSpawned();

static bool gUnhandledDivergeAllowed;

void DivergeFromRecording() {
  MOZ_RELEASE_ASSERT(IsReplaying());

  EnsureNonMainThreadsAreSpawned();

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

    Print("Unhandled recording divergence: %s\n",
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

bool NeedRespawnThreads() {
  return gNeedRespawnThreads;
}

static void EnsureNonMainThreadsAreSpawned() {
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

  if (IsReplaying()) {
    Print("ResumeExecution\n");
  }

  MonitorAutoLock lock(*gMainThreadCallbackMonitor);
  gMainThreadShouldPause = false;
  gMainThreadCallbackMonitor->Notify();
}

bool ForkProcess(size_t aForkId) {
  MOZ_RELEASE_ASSERT(IsReplaying());

  if (!gNeedRespawnThreads) {
    child::PrintLog("ForkProcess WaitForIdleThreads");
    Thread::WaitForIdleThreads();

    // Before forking all other threads need to release any locks they are
    // holding. After the fork the new process will only have a main thread and
    // will not be able to acquire any lock held at the time of the fork.
    child::PrintLog("ForkProcess ReleaseLocks");
    Thread::OperateOnIdleThreadLocks(Thread::OwnedLockState::NeedRelease);
  }

  AutoEnsurePassThroughThreadEvents pt;

  if (child::RawFork()) {
    if (!gNeedRespawnThreads) {
      Thread::OperateOnIdleThreadLocks(Thread::OwnedLockState::NeedAcquire);
      Thread::ResumeIdleThreads();
    }
    return true;
  }

  Print("FORKED %d #%lu\n", getpid(), aForkId);

  if (TestEnv("MOZ_REPLAYING_WAIT_AT_FORK")) {
    long which = strtol(getenv("MOZ_REPLAYING_WAIT_AT_FORK"), nullptr, 10);
    if ((size_t)which <= aForkId) {
      BusyWait();
    }
  }

  ResetPid();

  gNeedRespawnThreads = true;
  return false;
}

}  // namespace recordreplay
}  // namespace mozilla
