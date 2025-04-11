/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventTargetAndLockCapability_h
#define mozilla_EventTargetAndLockCapability_h

#include "MainThreadUtils.h"
#include "mozilla/ThreadSafety.h"
#include "mozilla/EventTargetCapability.h"

// This header contains helper types for combining a Lock and a thread
// capability allow using both independently, as well as combined together.
//
// This is useful for reflecting the "Single-Writer Mutex" pattern for the clang
// thread-safety analysis, allowing compile-time validation of the correct use
// of the combined capability.
//
// See https://firefox-source-docs.mozilla.org/xpcom/thread-safety.html for
// additional high-level documentation on the thread-safety analysis.

namespace mozilla {

// A thread-safety capability used to combine a Lock with the main thread
// capability to allow using each independently, as well as combined together.
//
// The MainThreadAndLockCapability grants shared (read-only) access if either on
// the main thread, or the inner mutex is held, and only allows exclusive
// (mutable) access if both the lock and the thread capability are held.
//
// This is used to implement the "Single-Writer Mutex" pattern, where a mutex is
// used to guard off-thread access to a value only mutated on a single thread,
// while not requiring the mutex for reads on that thread.
//
// There are no `AutoLock` helper types, instead the lock needs to be acquired
// using the normal locking mechanism (e.g. `MutexAutoLock lock(cap.Lock())`),
// followed by the relevant call to `NoteLockHeld` or `NoteExclusiveAccess`.
template <typename LockT>
class MOZ_CAPABILITY("combo (main thread + mutex)")
    MainThreadAndLockCapability {
 public:
  explicit MainThreadAndLockCapability(const char* aName) : mLock(aName) {}

  // Get access to the internal Lock. This can be used both to have values
  // normally guarded by this lock, as well as to acquire the lock as-needed.
  LockT& Lock() MOZ_RETURN_CAPABILITY(mLock) { return mLock; }

  // Note that we're on the main thread, and thus have shared (read-only) access
  // to values guarded by the MainThreadAndLockCapability for the thread-safety
  // analysis.
  void NoteOnMainThread() const MOZ_REQUIRES(sMainThreadCapability)
      MOZ_ASSERT_SHARED_CAPABILITY(this) {}

  // Note that we're holding the lock, and thus have shared (read-only) access
  // to values guarded by the MainThreadAndLockCapability for the thread-safety
  // analysis.
  void NoteLockHeld() const MOZ_REQUIRES(mLock)
      MOZ_ASSERT_SHARED_CAPABILITY(this) {}

  // Note that we're holding the lock while on the main thread, and thus have
  // exclusive (mutable) access to values guarded by the
  // MainThreadAndLockCapability for the thread-safety analysis.
  void NoteExclusiveAccess() const MOZ_REQUIRES(sMainThreadCapability, mLock)
      MOZ_ASSERT_CAPABILITY(this) {}

  // If you have previously called one of the above `Note` methods in the
  // current function scope, then have acquired `Lock()` and now want to
  // `NoteExclusiveAccess()`, this method can be called to clear the thread
  // safety analysis's understanding that the MainThreadAndLockCapability is
  // currently held.
  void ClearCurrentAccess() const
      MOZ_RELEASE_GENERIC(this) MOZ_NO_THREAD_SAFETY_ANALYSIS {}

 private:
  LockT mLock;
};

// Similar to MainThreadAndLockCapability, this is a thread-safety capability
// used to combine a Lock with an EventTarget capability to allow using each
// independently, as well as combined together.
//
// The EventTargetAndLockCapability grants shared (read-only) access if either
// on the event target, or the inner lock is held, and only allows exclusive
// (mutable) access if both the lock and the event target capability are held.
//
// This is used to implement the "Single-Writer Mutex" pattern, where a mutex is
// used to guard off-thread access to a value only mutated on a single thread,
// while not requiring the mutex for reads on that thread.
//
// There are no `AutoLock` helper types, instead the lock needs to be acquired
// using the normal locking mechanism (e.g. `MutexAutoLock lock(cap.Lock())`),
// followed by the relevant call to `NoteLockHeld` or `NoteExclusiveAccess`.
template <typename TargetT, typename LockT>
class MOZ_CAPABILITY("combo (event target + mutex)")
    EventTargetAndLockCapability {
 public:
  EventTargetAndLockCapability(const char* aName, TargetT* aTarget)
      : mLock(aName), mTarget(aTarget) {}

  // Get access to the internal Lock. This can be used both to have values
  // normally guarded by this lock, as well as to acquire the lock as-needed.
  LockT& Lock() MOZ_RETURN_CAPABILITY(mLock) { return mLock; }

  // Get access to the internal EventTargetCapability. This can be used both to
  // have values normally guarded by this capability, as well as to assert the
  // capability as needed.
  const mozilla::EventTargetCapability<TargetT>& Target() const
      MOZ_RETURN_CAPABILITY(mTarget) {
    return mTarget;
  }

  // Note that we're on the event target, and thus have shared (read-only)
  // access to values guarded by the EventTargetAndLockCapability for the
  // thread-safety analysis.
  void NoteOnTarget() const MOZ_REQUIRES(mTarget)
      MOZ_ASSERT_SHARED_CAPABILITY(this) {}

  // Note that we're holding the lock, and thus have shared (read-only) access
  // to values guarded by the EventTargetAndLockCapability for the thread-safety
  // analysis.
  void NoteLockHeld() const MOZ_REQUIRES(mLock)
      MOZ_ASSERT_SHARED_CAPABILITY(this) {}

  // Note that we're holding the lock while on the event target, and thus have
  // exclusive (mutable) access to values guarded by the
  // EventTargetAndLockCapability for the thread-safety analysis.
  void NoteExclusiveAccess() const MOZ_REQUIRES(mTarget, mLock)
      MOZ_ASSERT_CAPABILITY(this) {}

  // If you have previously called one of the above `Note` methods in the
  // current function scope, then have acquired `Mutex()` and now want to
  // `NoteExclusiveAccess()`, this method can be called to clear the thread
  // safety analysis's understanding that the EventTargetAndLockCapability is
  // currently held.
  void ClearCurrentAccess() const
      MOZ_RELEASE_GENERIC(this) MOZ_NO_THREAD_SAFETY_ANALYSIS {}

 private:
  LockT mLock;
  mozilla::EventTargetCapability<TargetT> mTarget;
};

}  // namespace mozilla

#endif  // mozilla_EventTargetAndLockCapability_h
