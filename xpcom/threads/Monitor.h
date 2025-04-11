/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Monitor_h
#define mozilla_Monitor_h

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

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
class MOZ_CAPABILITY("monitor") Monitor {
 public:
  explicit Monitor(const char* aName)
      : mMutex(aName), mCondVar(mMutex, "[Monitor.mCondVar]") {}

  ~Monitor() = default;

  void Lock() MOZ_CAPABILITY_ACQUIRE() { mMutex.Lock(); }
  [[nodiscard]] bool TryLock() MOZ_TRY_ACQUIRE(true) {
    return mMutex.TryLock();
  }
  void Unlock() MOZ_CAPABILITY_RELEASE() { mMutex.Unlock(); }

  void Wait() MOZ_REQUIRES(this) { mCondVar.Wait(); }
  CVStatus Wait(TimeDuration aDuration) MOZ_REQUIRES(this) {
    return mCondVar.Wait(aDuration);
  }

  void Notify() { mCondVar.Notify(); }
  void NotifyAll() { mCondVar.NotifyAll(); }

  void AssertCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(this) {
    mMutex.AssertCurrentThreadOwns();
  }
  void AssertNotCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY(!this) {
    mMutex.AssertNotCurrentThreadOwns();
  }

 private:
  Monitor() = delete;
  Monitor(const Monitor&) = delete;
  Monitor& operator=(const Monitor&) = delete;

  Mutex mMutex;
  CondVar mCondVar;
};

/**
 * Lock the monitor for the lexical scope instances of this class are
 * bound to (except for MonitorAutoUnlock in nested scopes).
 *
 * The monitor must be unlocked when instances of this class are
 * created.
 */
class MOZ_SCOPED_CAPABILITY MOZ_STACK_CLASS MonitorAutoLock {
 public:
  explicit MonitorAutoLock(Monitor& aMonitor) MOZ_CAPABILITY_ACQUIRE(aMonitor)
      : mMonitor(&aMonitor) {
    mMonitor->Lock();
  }

  ~MonitorAutoLock() MOZ_CAPABILITY_RELEASE() { mMonitor->Unlock(); }
  // It's very hard to mess up MonitorAutoLock lock(mMonitor); ... lock.Wait().
  // The only way you can fail to hold the lock when you call lock.Wait() is to
  // use MonitorAutoUnlock.   For now we'll ignore that case.
  void Wait() {
    mMonitor->AssertCurrentThreadOwns();
    mMonitor->Wait();
  }
  CVStatus Wait(TimeDuration aDuration) {
    mMonitor->AssertCurrentThreadOwns();
    return mMonitor->Wait(aDuration);
  }

  void Notify() { mMonitor->Notify(); }
  void NotifyAll() { mMonitor->NotifyAll(); }

  // Assert that aLock is the monitor passed to the constructor and that the
  // current thread owns the monitor.  In coding patterns such as:
  //
  // void LockedMethod(const BaseAutoLock<T>& aProofOfLock)
  // {
  //   aProofOfLock.AssertOwns(mMonitor);
  //   ...
  // }
  //
  // Without this assertion, it could be that mMonitor is not actually
  // locked. It's possible to have code like:
  //
  // BaseAutoLock lock(someMonitor);
  // ...
  // BaseAutoUnlock unlock(someMonitor);
  // ...
  // LockedMethod(lock);
  //
  // and in such a case, simply asserting that the monitor pointers match is not
  // sufficient; monitor ownership must be asserted as well.
  //
  // Note that if you are going to use the coding pattern presented above, you
  // should use this method in preference to using AssertCurrentThreadOwns on
  // the mutex you expected to be held, since this method provides stronger
  // guarantees.
  void AssertOwns(const Monitor& aMonitor) const
      MOZ_ASSERT_CAPABILITY(aMonitor) {
    MOZ_ASSERT(&aMonitor == mMonitor);
    mMonitor->AssertCurrentThreadOwns();
  }

 private:
  MonitorAutoLock() = delete;
  MonitorAutoLock(const MonitorAutoLock&) = delete;
  MonitorAutoLock& operator=(const MonitorAutoLock&) = delete;
  static void* operator new(size_t) noexcept(true);

  friend class MonitorAutoUnlock;

 protected:
  Monitor* mMonitor;
};

/**
 * Unlock the monitor for the lexical scope instances of this class
 * are bound to (except for MonitorAutoLock in nested scopes).
 *
 * The monitor must be locked by the current thread when instances of
 * this class are created.
 */
class MOZ_STACK_CLASS MOZ_SCOPED_CAPABILITY MonitorAutoUnlock {
 public:
  explicit MonitorAutoUnlock(Monitor& aMonitor)
      MOZ_SCOPED_UNLOCK_RELEASE(aMonitor)
      : mMonitor(&aMonitor) {
    mMonitor->Unlock();
  }

  ~MonitorAutoUnlock() MOZ_SCOPED_UNLOCK_REACQUIRE() { mMonitor->Lock(); }

 private:
  MonitorAutoUnlock() = delete;
  MonitorAutoUnlock(const MonitorAutoUnlock&) = delete;
  MonitorAutoUnlock& operator=(const MonitorAutoUnlock&) = delete;
  static void* operator new(size_t) noexcept(true);

  Monitor* mMonitor;
};

/**
 * Lock the monitor for the lexical scope instances of this class are
 * bound to (except for MonitorAutoUnlock in nested scopes).
 *
 * The monitor must be unlocked when instances of this class are
 * created.
 */
class MOZ_SCOPED_CAPABILITY MOZ_STACK_CLASS ReleasableMonitorAutoLock {
 public:
  explicit ReleasableMonitorAutoLock(Monitor& aMonitor)
      MOZ_CAPABILITY_ACQUIRE(aMonitor)
      : mMonitor(&aMonitor) {
    mMonitor->Lock();
    mLocked = true;
  }

  ~ReleasableMonitorAutoLock() MOZ_CAPABILITY_RELEASE() {
    if (mLocked) {
      mMonitor->Unlock();
    }
  }

  // See MonitorAutoLock::Wait
  void Wait() {
    mMonitor->AssertCurrentThreadOwns();  // someone could have called Unlock()
    mMonitor->Wait();
  }
  CVStatus Wait(TimeDuration aDuration) {
    mMonitor->AssertCurrentThreadOwns();
    return mMonitor->Wait(aDuration);
  }

  void Notify() {
    MOZ_ASSERT(mLocked);
    mMonitor->Notify();
  }
  void NotifyAll() {
    MOZ_ASSERT(mLocked);
    mMonitor->NotifyAll();
  }

  // Allow dropping the lock prematurely; for example to support something like:
  // clang-format off
  // MonitorAutoLock lock(mMonitor);
  // ...
  // if (foo) {
  //   lock.Unlock();
  //   MethodThatCantBeCalledWithLock()
  //   return;
  // }
  // clang-format on
  void Unlock() MOZ_CAPABILITY_RELEASE() {
    MOZ_ASSERT(mLocked);
    mMonitor->Unlock();
    mLocked = false;
  }
  void Lock() MOZ_CAPABILITY_ACQUIRE() {
    MOZ_ASSERT(!mLocked);
    mMonitor->Lock();
    mLocked = true;
  }
  void AssertCurrentThreadOwns() const MOZ_ASSERT_CAPABILITY() {
    mMonitor->AssertCurrentThreadOwns();
  }

 private:
  bool mLocked;
  Monitor* mMonitor;

  ReleasableMonitorAutoLock() = delete;
  ReleasableMonitorAutoLock(const ReleasableMonitorAutoLock&) = delete;
  ReleasableMonitorAutoLock& operator=(const ReleasableMonitorAutoLock&) =
      delete;
  static void* operator new(size_t) noexcept(true);
};

}  // namespace mozilla

#endif  // mozilla_Monitor_h
