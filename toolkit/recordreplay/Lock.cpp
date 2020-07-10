/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Lock.h"

#include "ChunkAllocator.h"
#include "InfallibleVector.h"
#include "SpinLock.h"
#include "Thread.h"
#include "ipc/ChildInternal.h"

#include <unordered_map>

namespace mozilla {
namespace recordreplay {

// The total number of locks that have been created. Each Lock is given a
// non-zero id based on this counter.
static AtomicUInt gNumLocks;

struct LockAcquires {
  // Associated lock ID.
  size_t mId;

  // List of thread acquire orders for the lock. This is protected by the lock
  // itself.
  Stream* mAcquires;

  // During replay, the current owner of this lock, zero if not owned.
  AtomicUInt mOwner;

  // During replay, the number of times the lock has been acquired by its owner.
  AtomicUInt mDepth;

  // During replay, the next thread id to acquire the lock. Writes to this are
  // protected by the lock itself, though reads may occur on other threads.
  AtomicUInt mNextOwner;

  static const size_t NoNextOwner = 0;

  void ReadNextOwner() {
    if (mAcquires->AtEnd()) {
      mNextOwner = NoNextOwner;
    } else {
      mNextOwner = mAcquires->ReadScalar();
      if (!mNextOwner) {
        Print("Error: ReadNextOwner ZeroId\n");
      }
    }
  }

  void NotifyNextOwner(Thread* aCurrentThread) {
    if (mNextOwner && mNextOwner != aCurrentThread->Id()) {
      Thread::Notify(mNextOwner);
    }
  }

  void ReadAndNotifyNextOwner(Thread* aCurrentThread) {
    ReadNextOwner();
    NotifyNextOwner(aCurrentThread);
  }
};

// Acquires for each lock, indexed by the lock ID.
static ChunkAllocator<LockAcquires> gLockAcquires;

///////////////////////////////////////////////////////////////////////////////
// Locking Interface
///////////////////////////////////////////////////////////////////////////////

// Table mapping native lock pointers to the associated Lock structure, for
// every recorded lock in existence.
typedef std::unordered_map<NativeLock*, Lock*> LockMap;
static LockMap* gLocks;
static ReadWriteSpinLock gLocksLock;

static Lock* CreateNewLock(Thread* aThread, size_t aId) {
  LockAcquires* info = gLockAcquires.Create(aId);
  info->mId = aId;
  info->mAcquires = gRecording->OpenStream(StreamName::Lock, aId);

  if (IsReplaying()) {
    info->ReadAndNotifyNextOwner(aThread);
  }

  return new Lock(aId);
}

/* static */
void Lock::New(NativeLock* aNativeLock) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    Destroy(aNativeLock);  // Clean up any old lock, as below.
    return;
  }

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::CreateLock);

  size_t id;
  if (IsRecording()) {
    id = gNumLocks++;
  }
  thread->Events().RecordOrReplayScalar(&id);

  Lock* lock = CreateNewLock(thread, id);

  // Tolerate new locks being created with identical pointers, even if there
  // was no explicit Destroy() call for the old one.
  Destroy(aNativeLock);

  AutoWriteSpinLock ex(gLocksLock);
  thread->BeginDisallowEvents();

  if (!gLocks) {
    gLocks = new LockMap();
  }

  gLocks->insert(LockMap::value_type(aNativeLock, lock));

  /*
  if (IsReplaying()) {
    char buf[2000];
    child::ReadStack(buf, sizeof(buf));
    lock->mCreateStack = nsCString(buf);
  }
  */

  thread->EndDisallowEvents();
}

/* static */
void Lock::Destroy(NativeLock* aNativeLock) {
  // Destroying a lock owned by the current thread is allowed.
  Thread* thread = Thread::Current();
  if (thread) {
    thread->MaybeRemoveDestroyedOwnedLock(aNativeLock);
  }

  Lock* lock = nullptr;
  {
    AutoWriteSpinLock ex(gLocksLock);
    if (gLocks) {
      LockMap::iterator iter = gLocks->find(aNativeLock);
      if (iter != gLocks->end()) {
        lock = iter->second;
        gLocks->erase(iter);
      }
    }
  }
  delete lock;
}

/* static */
Lock* Lock::Find(NativeLock* aNativeLock) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  AutoReadSpinLock ex(gLocksLock);

  if (gLocks) {
    LockMap::iterator iter = gLocks->find(aNativeLock);
    if (iter != gLocks->end()) {
      // Now that we know the lock is recorded, check whether thread events
      // should be generated right now. Doing things in this order avoids
      // reentrancy issues when initializing the thread-local state used by
      // these calls.
      Lock* lock = iter->second;
      if (AreThreadEventsPassedThrough()) {
        return nullptr;
      }
      if (HasDivergedFromRecording()) {
        return nullptr;
      }
      return lock;
    }
  }

  return nullptr;
}

void Lock::Enter(NativeLock* aNativeLock, size_t aRbp) {
  Thread* thread = Thread::Current();

  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return;
  }

  // Include an event in each thread's record when a lock acquire begins. This
  // is not required by the replay but is used to check that lock acquire order
  // is consistent with the recording and that we will fail explicitly instead
  // of deadlocking.
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Lock);
  thread->Events().CheckInput(mId);

  LockAcquires* acquires = gLockAcquires.Get(mId);
  if (IsRecording()) {
    acquires->mAcquires->WriteScalar(thread->Id());
    thread->Events().WriteScalar(acquires->mAcquires->StreamPosition());

    char buf[1000];
    ReadStack(aRbp, thread, buf, sizeof(buf));
    size_t len = strlen(buf) + 1;
    thread->Events().WriteScalar(len);
    thread->Events().WriteBytes(buf, len);
  } else {
    size_t acquiresPosition = thread->Events().ReadScalar();

    size_t len = thread->Events().ReadScalar();
    thread->Events().ReadBytes(nullptr, len);

    MOZ_RELEASE_ASSERT(thread->PendingLockId().isNothing());
    thread->PendingLockId().emplace(mId);
    thread->PendingLockAcquiresPosition().emplace(acquiresPosition);

    while (true) {
      if (thread->Id() == acquires->mNextOwner &&
          (!acquires->mOwner || acquires->mOwner == thread->Id())) {
        // It is this thread's turn to acquire the lock, and it can take it immediately.
        break;
      }
      if (thread->MaybeDivergeFromRecording()) {
        // This thread has diverged from the recording and should ignore the
        // acquire order when taking the lock.
        break;
      }
      Thread::Wait();
    }
  }
  if (aNativeLock) {
    thread->AddOwnedLock(aNativeLock);
  }
}

void Lock::FinishEnter() {
  MOZ_RELEASE_ASSERT(IsReplaying());

  Thread* thread = Thread::Current();
  if (!thread || thread->PassThroughEvents() || thread->HasDivergedFromRecording()) {
    return;
  }

  MOZ_RELEASE_ASSERT(thread->PendingLockId().isSome());
  size_t lockId = thread->PendingLockId().ref();

  LockAcquires* acquires = gLockAcquires.Get(lockId);
  MOZ_RELEASE_ASSERT(acquires->mOwner == 0 || acquires->mOwner == thread->Id());
  MOZ_RELEASE_ASSERT(acquires->mNextOwner == thread->Id());

  size_t acquiresPosition = thread->PendingLockAcquiresPosition().ref();

  // The acquires stream should be at the same position when replaying,
  // except for atomic lock accesses where we might have skipped over mismatched
  // accesses in the recording.
  if (acquires->mAcquires->StreamPosition() != acquiresPosition &&
      !IsAtomicLockId(lockId)) {
    child::ReportFatalError("AcquiresPosition Mismatch %lu Thread %lu: Recorded %lu Replayed %lu",
                            lockId, thread->Id(),
                            acquiresPosition, acquires->mAcquires->StreamPosition());
  }

  thread->PendingLockId().reset();
  thread->PendingLockAcquiresPosition().reset();

  acquires->mOwner = thread->Id();
  acquires->mDepth++;

  acquires->ReadNextOwner();
}

void Lock::Exit(NativeLock* aNativeLock) {
  Thread* thread = Thread::Current();
  if (aNativeLock) {
    thread->RemoveOwnedLock(aNativeLock);
  }

  if (IsReplaying() && !thread->HasDivergedFromRecording()) {
    // Update lock state and notify the next owner.
    LockAcquires* acquires = gLockAcquires.Get(mId);
    MOZ_RELEASE_ASSERT(acquires->mOwner == thread->Id());
    acquires->mDepth--;
    if (acquires->mDepth == 0) {
      acquires->mOwner = 0;
    }
    acquires->NotifyNextOwner(thread);
  }
}

/* static */
void Lock::LockAcquiresUpdated(size_t aLockId) {
  LockAcquires* acquires = gLockAcquires.MaybeGet(aLockId);
  if (acquires && acquires->mAcquires &&
      acquires->mNextOwner == LockAcquires::NoNextOwner) {
    acquires->ReadAndNotifyNextOwner(Thread::Current());
  }
}

// We use a set of Locks to record and replay the order in which atomic
// accesses occur. Each lock describes the acquire order for a disjoint set of
// values; this is done to reduce contention between threads, and ensures that
// when the same value pointer is used in two ordered atomic accesses, those
// accesses will replay in the same order as they did while recording.
// Instead of using platform mutexes, we manage the Locks directly to avoid
// overhead in Lock::Find. Atomics accesses are a major source of recording
// overhead, which we want to minimize.
static const size_t NumAtomicLocks = 89;
static Lock** gAtomicLocks;

// Substitute for the platform mutex associated with each atomic lock.
static SpinLock* gAtomicLockOwners;

/* static */
void Lock::InitializeLocks() {
  Thread* thread = Thread::Current();

  gNumLocks = 1;
  gAtomicLocks = new Lock*[NumAtomicLocks];
  for (size_t i = 0; i < NumAtomicLocks; i++) {
    gAtomicLocks[i] = CreateNewLock(thread, gNumLocks++);
  }
  gAtomicLockOwners = new SpinLock[NumAtomicLocks];
  PodZero(gAtomicLockOwners, NumAtomicLocks);
}

bool IsAtomicLockId(size_t aLockId) {
  return aLockId <= NumAtomicLocks;
}

extern "C" {

MOZ_EXPORT void RecordReplayInterface_InternalBeginOrderedAtomicAccess(
    const void* aValue) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  Thread* thread = Thread::Current();

  // Determine which atomic lock to use for this access.
  size_t atomicId;
  {
    // Allow atomic accesses to occur normally when events are disallowed during GC.
    RecordingEventSection res(thread);
    if (!res.CanAccessEvents(/* aTolerateDisallowedEvents */ true)) {
      return;
    }

    thread->Events().RecordOrReplayThreadEvent(ThreadEvent::AtomicAccess);

    atomicId = IsRecording() ? (HashGeneric(aValue) % NumAtomicLocks) : 0;
    thread->Events().RecordOrReplayScalar(&atomicId);

    MOZ_RELEASE_ASSERT(atomicId < NumAtomicLocks);
  }

  if (IsRecording()) {
    gAtomicLockOwners[atomicId].Lock();
  }

  gAtomicLocks[atomicId]->Enter(nullptr, 0);

  if (IsReplaying()) {
    gAtomicLockOwners[atomicId].Lock();
    gAtomicLocks[atomicId]->FinishEnter();
  }

  MOZ_RELEASE_ASSERT(thread->AtomicLockId().isNothing());
  thread->AtomicLockId().emplace(atomicId);
}

MOZ_EXPORT void RecordReplayInterface_InternalEndOrderedAtomicAccess() {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  Thread* thread = Thread::Current();
  if (!thread) {
    return;
  }

  if (thread->AtomicLockId().isNothing()) {
    MOZ_RELEASE_ASSERT(!thread->CanAccessRecording());
    return;
  }

  size_t atomicId = thread->AtomicLockId().ref();
  thread->AtomicLockId().reset();

  gAtomicLocks[atomicId]->Exit(nullptr);
  gAtomicLockOwners[atomicId].Unlock();
}

}  // extern "C"

/* static */
void Lock::DumpLock(size_t aLockId) {
  // This isn't threadsafe, but is only called when the process has hanged.
  LockAcquires* acquires = gLockAcquires.Get(aLockId);
  Print("Lock %lu: Owner %lu Depth %lu NextOwner %lu Position %lu AtEnd %d\n",
        aLockId, (size_t)acquires->mOwner,
        (size_t)acquires->mDepth, (size_t)acquires->mNextOwner,
        acquires->mAcquires ? acquires->mAcquires->StreamPosition() : -1,
        acquires->mAcquires ? acquires->mAcquires->AtEnd() : true);
}

/* static */
void Lock::DumpCreateStack(size_t aLockId) {
  AutoReadSpinLock ex(gLocksLock);

  if (gLocks) {
    for (LockMap::iterator iter = gLocks->begin(); iter != gLocks->end(); ++iter) {
      Lock* lock = iter->second;
      if (lock->Id() == aLockId) {
        Print("LockCreateStack %lu\n%s\n", aLockId, lock->mCreateStack.get());
        break;
      }
    }
  }
}

// This hidden API can be used when writing record/replay asserts.
void LastAcquiredLock(size_t* aId, size_t* aPosition) {
  Thread* thread = Thread::Current();
  NativeLock* nativeLock = thread->LastOwnedLock();
  if (!nativeLock) {
    Print("CRASH NoNativeLock\n");
  }
  MOZ_RELEASE_ASSERT(nativeLock);

  Lock* lock = Lock::Find(nativeLock);
  if (!lock) {
    Print("CRASH NoLockForNativeLock\n");
  }
  MOZ_RELEASE_ASSERT(lock);

  LockAcquires* acquires = gLockAcquires.Get(lock->Id());

  *aId = lock->Id();
  *aPosition = acquires->mAcquires->StreamPosition();
}

}  // namespace recordreplay
}  // namespace mozilla
