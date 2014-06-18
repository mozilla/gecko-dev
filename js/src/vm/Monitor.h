/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Monitor_h
#define vm_Monitor_h

#ifdef JS_THREADSAFE
#include "mozilla/DebugOnly.h"
#endif

#include <stddef.h>

#include "jslock.h"

#include "js/Utility.h"

namespace js {

// A base class used for types intended to be used in a parallel
// fashion, such as the workers in the |ThreadPool| class.  Combines a
// lock and a condition variable.  You can acquire the lock or signal
// the condition variable using the |AutoLockMonitor| type.

class Monitor
{
  protected:
    friend class AutoLockMonitor;
    friend class AutoUnlockMonitor;

    PRLock *lock_;
    PRCondVar *condVar_;

  public:
    Monitor()
      : lock_(nullptr),
        condVar_(nullptr)
    { }

    ~Monitor() {
#ifdef JS_THREADSAFE
        if (lock_)
            PR_DestroyLock(lock_);
        if (condVar_)
            PR_DestroyCondVar(condVar_);
#endif
    }

    bool init();
};

class AutoLockMonitor
{
  private:
#ifdef JS_THREADSAFE
    Monitor &monitor;
#endif

  public:
    explicit AutoLockMonitor(Monitor &monitor)
#ifdef JS_THREADSAFE
      : monitor(monitor)
    {
        PR_Lock(monitor.lock_);
    }
#else
    {}
#endif

    ~AutoLockMonitor() {
#ifdef JS_THREADSAFE
        PR_Unlock(monitor.lock_);
#endif
    }

    bool isFor(Monitor &other) const {
#ifdef JS_THREADSAFE
        return monitor.lock_ == other.lock_;
#else
        return true;
#endif
    }

    void wait(PRCondVar *condVar) {
#ifdef JS_THREADSAFE
        mozilla::DebugOnly<PRStatus> status =
          PR_WaitCondVar(condVar, PR_INTERVAL_NO_TIMEOUT);
        MOZ_ASSERT(status == PR_SUCCESS);
#endif
    }

    void wait() {
#ifdef JS_THREADSAFE
        wait(monitor.condVar_);
#endif
    }

    void notify(PRCondVar *condVar) {
#ifdef JS_THREADSAFE
        mozilla::DebugOnly<PRStatus> status = PR_NotifyCondVar(condVar);
        MOZ_ASSERT(status == PR_SUCCESS);
#endif
    }

    void notify() {
#ifdef JS_THREADSAFE
        notify(monitor.condVar_);
#endif
    }

    void notifyAll(PRCondVar *condVar) {
#ifdef JS_THREADSAFE
        mozilla::DebugOnly<PRStatus> status = PR_NotifyAllCondVar(monitor.condVar_);
        MOZ_ASSERT(status == PR_SUCCESS);
#endif
    }

    void notifyAll() {
#ifdef JS_THREADSAFE
        notifyAll(monitor.condVar_);
#endif
    }
};

class AutoUnlockMonitor
{
  private:
#ifdef JS_THREADSAFE
    Monitor &monitor;
#endif

  public:
    explicit AutoUnlockMonitor(Monitor &monitor)
#ifdef JS_THREADSAFE
      : monitor(monitor)
    {
        PR_Unlock(monitor.lock_);
    }
#else
    {}
#endif

    ~AutoUnlockMonitor() {
#ifdef JS_THREADSAFE
        PR_Lock(monitor.lock_);
#endif
    }

    bool isFor(Monitor &other) const {
#ifdef JS_THREADSAFE
        return monitor.lock_ == other.lock_;
#else
        return true;
#endif
    }
};

} // namespace js

#endif /* vm_Monitor_h */
