/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Monitor_h
#define mozilla_Monitor_h

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "mozilla/RecordReplay.h"

namespace mozilla {

/**
 * Monitor provides a *non*-reentrant monitor: *not* a Java-style
 * monitor.  If your code needs support for reentrancy, use
 * ReentrantMonitor instead.  (Rarely should reentrancy be needed.)
 *
 * Instead of directly calling Monitor methods, it's safer and simpler
 * to instead use the RAII wrappers MonitorAutoLock and
 * MonitorAutoUnlock.
 */
class Monitor {
 public:
  explicit Monitor(const char* aName, bool aOrdered = false)
      : mMutex(aName), mCondVar(mMutex, "[Monitor.mCondVar]") {
    if (aOrdered) {
      mRecordReplayOrderedLockId = recordreplay::CreateOrderedLock(aName);
    }
  }

  ~Monitor() = default;

  void Lock() {
    if (mRecordReplayOrderedLockId) {
      // Adjust the order we take the lock based on whether we are recording,
      // so that lock acquires happen in a consistent order and avoid deadlocks.
      // When recording we take the ordered lock second, so that we can deal
      // with waking up from a condvar notify. When replaying we take the
      // ordered lock first, as it will block until we are next in line to
      // take the lock.
      if (recordreplay::IsRecording()) {
        mMutex.Lock();
        recordreplay::AutoOrderedLock ordered(mRecordReplayOrderedLockId);
      } else {
        recordreplay::AutoOrderedLock ordered(mRecordReplayOrderedLockId);
        mMutex.Lock();
      }
    } else {
      mMutex.Lock();
    }
  }
  bool TryLock() {
    Maybe<recordreplay::AutoOrderedLock> ordered;
    if (mRecordReplayOrderedLockId) {
      // Lock acquire here doesn't matter as above, because TryLock won't block.
      ordered.emplace(mRecordReplayOrderedLockId);
    }
    return mMutex.TryLock();
  }
  void Unlock() { mMutex.Unlock(); }

  void Wait() {
    if (mRecordReplayOrderedLockId) {
      if (recordreplay::IsRecording()) {
        mCondVar.Wait();
        recordreplay::AutoOrderedLock ordered(mRecordReplayOrderedLockId);
      } else {
        // When replaying, we don't wait on the condvar. Reproducing the
        // lock that occurred when recording will ensure this thread is at the
        // right place when replaying.
        Unlock();
        Lock();
      }
    } else {
      mCondVar.Wait();
    }
  }
  CVStatus Wait(TimeDuration aDuration) {
    if (mRecordReplayOrderedLockId) {
      if (recordreplay::IsRecording()) {
        CVStatus rv = mCondVar.Wait(aDuration);
        recordreplay::AutoOrderedLock ordered(mRecordReplayOrderedLockId);
        return (CVStatus)recordreplay::RecordReplayValue(mMutex.Name(), (int)rv);
      } else {
        // Don't wait on the condvar, as above.
        Unlock();
        Lock();
        return (CVStatus)recordreplay::RecordReplayValue(mMutex.Name(), 0);
      }
    } else {
      return mCondVar.Wait(aDuration);
    }
  }

  void Notify() { mCondVar.Notify(); }
  void NotifyAll() { mCondVar.NotifyAll(); }

  void AssertCurrentThreadOwns() const { mMutex.AssertCurrentThreadOwns(); }

  void AssertNotCurrentThreadOwns() const {
    mMutex.AssertNotCurrentThreadOwns();
  }

 private:
  Monitor();
  Monitor(const Monitor&);
  Monitor& operator=(const Monitor&);

  Mutex mMutex;
  CondVar mCondVar;
  int mRecordReplayOrderedLockId = 0;
};

/**
 * Lock the monitor for the lexical scope instances of this class are
 * bound to (except for MonitorAutoUnlock in nested scopes).
 *
 * The monitor must be unlocked when instances of this class are
 * created.
 */
class MOZ_STACK_CLASS MonitorAutoLock {
 public:
  explicit MonitorAutoLock(Monitor& aMonitor) : mMonitor(&aMonitor) {
    mMonitor->Lock();
  }

  ~MonitorAutoLock() { mMonitor->Unlock(); }

  void Wait() { mMonitor->Wait(); }
  CVStatus Wait(TimeDuration aDuration) { return mMonitor->Wait(aDuration); }

  void Notify() { mMonitor->Notify(); }
  void NotifyAll() { mMonitor->NotifyAll(); }

 private:
  MonitorAutoLock();
  MonitorAutoLock(const MonitorAutoLock&);
  MonitorAutoLock& operator=(const MonitorAutoLock&);
  static void* operator new(size_t) noexcept(true);

  friend class MonitorAutoUnlock;

  Monitor* mMonitor;
};

/**
 * Unlock the monitor for the lexical scope instances of this class
 * are bound to (except for MonitorAutoLock in nested scopes).
 *
 * The monitor must be locked by the current thread when instances of
 * this class are created.
 */
class MOZ_STACK_CLASS MonitorAutoUnlock {
 public:
  explicit MonitorAutoUnlock(Monitor& aMonitor) : mMonitor(&aMonitor) {
    mMonitor->Unlock();
  }

  explicit MonitorAutoUnlock(MonitorAutoLock& aMonitorLock)
      : mMonitor(aMonitorLock.mMonitor) {
    mMonitor->Unlock();
  }

  ~MonitorAutoUnlock() { mMonitor->Lock(); }

 private:
  MonitorAutoUnlock();
  MonitorAutoUnlock(const MonitorAutoUnlock&);
  MonitorAutoUnlock& operator=(const MonitorAutoUnlock&);
  static void* operator new(size_t) noexcept(true);

  Monitor* mMonitor;
};

}  // namespace mozilla

#endif  // mozilla_Monitor_h
