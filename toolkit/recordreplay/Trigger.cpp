/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Trigger.h"

#include "ipc/ChildIPC.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/RecordReplay.h"
#include "InfallibleVector.h"
#include "ProcessRewind.h"
#include "Thread.h"
#include "ValueIndex.h"

namespace mozilla {
namespace recordreplay {

// Information about each trigger.
struct TriggerInfo {
  // ID of the thread which registered this trigger.
  size_t mThreadId;

  // Callback to execute when the trigger is activated.
  std::function<void()> mCallback;

  // Number of times this trigger has been activated.
  size_t mRegisterCount;

  TriggerInfo(size_t aThreadId, const std::function<void()>& aCallback)
      : mThreadId(aThreadId), mCallback(aCallback), mRegisterCount(1) {}
};

// All registered triggers.
static ValueIndex* gTriggers;

typedef std::unordered_map<void*, TriggerInfo> TriggerInfoMap;
static TriggerInfoMap* gTriggerInfoMap;

// Triggers which have been activated. This is protected by the global lock.
static StaticInfallibleVector<size_t> gActivatedTriggers;

static StaticMutexNotRecorded gTriggersMutex;

void InitializeTriggers() {
  gTriggers = new ValueIndex();
  gTriggerInfoMap = new TriggerInfoMap();
}

extern "C" {

MOZ_EXPORT void RecordReplayInterface_RegisterTrigger(
    void* aObj, const std::function<void()>& aCallback) {
  MOZ_RELEASE_ASSERT(aObj);
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  Thread* thread = Thread::Current();
  if (thread->HasDivergedFromRecording()) {
    return;
  }
  MOZ_RELEASE_ASSERT(thread->CanAccessRecording());

  size_t id;
  {
    AutoOrderedAtomicAccess order(gTriggers);
    StaticMutexAutoLock lock(gTriggersMutex);

    TriggerInfoMap::iterator iter = gTriggerInfoMap->find(aObj);
    if (iter != gTriggerInfoMap->end()) {
      id = gTriggers->GetIndex(aObj);
      MOZ_RELEASE_ASSERT(iter->second.mThreadId == thread->Id());
      iter->second.mCallback = aCallback;
      iter->second.mRegisterCount++;
    } else {
      id = gTriggers->Insert(aObj);
      TriggerInfo info(thread->Id(), aCallback);
      gTriggerInfoMap->insert(TriggerInfoMap::value_type(aObj, info));
    }
  }

  RecordingEventSection res(thread);
  MOZ_RELEASE_ASSERT(res.CanAccessEvents());

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::RegisterTrigger);
  thread->Events().CheckInput(id);
}

MOZ_EXPORT void RecordReplayInterface_UnregisterTrigger(void* aObj) {
  MOZ_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  StaticMutexAutoLock lock(gTriggersMutex);

  TriggerInfoMap::iterator iter = gTriggerInfoMap->find(aObj);
  MOZ_RELEASE_ASSERT(iter != gTriggerInfoMap->end());
  if (--iter->second.mRegisterCount == 0) {
    gTriggerInfoMap->erase(iter);
    gTriggers->Remove(aObj);
  }
}

MOZ_EXPORT void RecordReplayInterface_ActivateTrigger(void* aObj) {
  if (!IsRecording()) {
    return;
  }

  StaticMutexAutoLock lock(gTriggersMutex);

  size_t id = gTriggers->GetIndex(aObj);
  gActivatedTriggers.emplaceBack(id);
}

static void InvokeTriggerCallback(size_t aId) {
  void* obj;
  std::function<void()> callback;
  {
    StaticMutexAutoLock lock(gTriggersMutex);
    obj = const_cast<void*>(gTriggers->GetValue(aId));
    TriggerInfoMap::iterator iter = gTriggerInfoMap->find(obj);
    MOZ_RELEASE_ASSERT(iter != gTriggerInfoMap->end());
    MOZ_RELEASE_ASSERT(iter->second.mThreadId == Thread::Current()->Id());
    MOZ_RELEASE_ASSERT(iter->second.mRegisterCount);
    MOZ_RELEASE_ASSERT(iter->second.mCallback);
    callback = iter->second.mCallback;
  }

  callback();
}

static Maybe<size_t> RemoveTriggerCallbackForThreadId(size_t aThreadId) {
  StaticMutexAutoLock lock(gTriggersMutex);
  for (size_t i = 0; i < gActivatedTriggers.length(); i++) {
    size_t id = gActivatedTriggers[i];
    void* obj = const_cast<void*>(gTriggers->GetValue(id));
    TriggerInfoMap::iterator iter = gTriggerInfoMap->find(obj);
    MOZ_RELEASE_ASSERT(iter != gTriggerInfoMap->end());
    if (iter->second.mThreadId == aThreadId) {
      gActivatedTriggers.erase(&gActivatedTriggers[i]);
      return Some(id);
    }
  }
  return Nothing();
}

MOZ_EXPORT void RecordReplayInterface_ExecuteTriggers() {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return;
  }

  if (IsRecording()) {
    // Invoke the callbacks for any triggers waiting for execution, including
    // any whose callbacks are triggered by earlier callback invocations.
    while (true) {
      Maybe<size_t> id = RemoveTriggerCallbackForThreadId(thread->Id());
      if (id.isNothing()) {
        break;
      }

      thread->Events().WriteScalar((size_t)ThreadEvent::ExecuteTrigger);
      thread->Events().WriteScalar(id.ref());
      InvokeTriggerCallback(id.ref());
    }
    thread->Events().WriteScalar((size_t)ThreadEvent::ExecuteTriggersFinished);
  } else {
    // Execute the same callbacks which were executed at this point while
    // recording.
    while (true) {
      ThreadEvent ev = (ThreadEvent)thread->Events().ReadScalar();
      if (ev != ThreadEvent::ExecuteTrigger) {
        if (ev != ThreadEvent::ExecuteTriggersFinished) {
          child::ReportFatalError(Nothing(), "ExecuteTrigger Mismatch");
          Unreachable();
        }
        break;
      }
      size_t id = thread->Events().ReadScalar();
      InvokeTriggerCallback(id);
    }
  }
}

}  // extern "C"

}  // namespace recordreplay
}  // namespace mozilla
