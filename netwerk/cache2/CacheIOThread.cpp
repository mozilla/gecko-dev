/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheIOThread.h"
#include "CacheFileIOManager.h"
#include "CacheLog.h"
#include "CacheObserver.h"

#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsPrintfCString.h"
#include "nsThread.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "mozilla/EventQueue.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ThreadEventQueue.h"
#include "GeckoProfiler.h"

#ifdef XP_WIN
#  include <windows.h>
#endif

namespace mozilla::net {

namespace detail {

/**
 * Helper class encapsulating platform-specific code to cancel
 * any pending IO operation taking too long.  Solely used during
 * shutdown to prevent any IO shutdown hangs.
 * Mainly designed for using Win32 CancelSynchronousIo function.
 */
class NativeThreadHandle {
#ifdef XP_WIN
  // The native handle to the thread
  HANDLE mThread;
#endif

 public:
  // Created and destroyed on the main thread only
  NativeThreadHandle();
  ~NativeThreadHandle();

  // Called on the IO thread to grab the platform specific
  // reference to it.
  void InitThread();
  // If there is a blocking operation being handled on the IO
  // thread, this is called on the main thread during shutdown.
  void CancelBlockingIO(Monitor& aMonitor);
};

#ifdef XP_WIN

NativeThreadHandle::NativeThreadHandle() : mThread(NULL) {}

NativeThreadHandle::~NativeThreadHandle() {
  if (mThread) {
    CloseHandle(mThread);
  }
}

void NativeThreadHandle::InitThread() {
  // GetCurrentThread() only returns a pseudo handle, hence DuplicateHandle
  ::DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                    GetCurrentProcess(), &mThread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
}

void NativeThreadHandle::CancelBlockingIO(Monitor& aMonitor) {
  HANDLE thread;
  {
    MonitorAutoLock lock(aMonitor);
    thread = mThread;

    if (!thread) {
      return;
    }
  }

  LOG(("CacheIOThread: Attempting to cancel a long blocking IO operation"));
  BOOL result = ::CancelSynchronousIo(thread);
  if (result) {
    LOG(("  cancelation signal succeeded"));
  } else {
    DWORD error = GetLastError();
    LOG(("  cancelation signal failed with GetLastError=%lu", error));
  }
}

#else  // WIN

// Stub code only (we don't implement IO cancelation for this platform)

NativeThreadHandle::NativeThreadHandle() = default;
NativeThreadHandle::~NativeThreadHandle() = default;
void NativeThreadHandle::InitThread() {}
void NativeThreadHandle::CancelBlockingIO(Monitor&) {}

#endif

}  // namespace detail

CacheIOThread* CacheIOThread::sSelf = nullptr;

NS_IMPL_ISUPPORTS(CacheIOThread, nsIThreadObserver)

CacheIOThread::CacheIOThread() {
  for (auto& item : mQueueLength) {
    item = 0;
  }

  sSelf = this;
}

CacheIOThread::~CacheIOThread() {
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_RELEASE_ASSERT(mShutdown);
  }

  if (mXPCOMThread) {
    nsIThread* thread = mXPCOMThread;
    thread->Release();
  }

  sSelf = nullptr;
#ifdef DEBUG
  for (auto& event : mEventQueue) {
    MOZ_ASSERT(!event.Length());
  }
#endif
}

nsresult CacheIOThread::Init() {
  {
    MonitorAutoLock lock(mMonitor);
    // Yeah, there is not a thread yet, but we want to make sure
    // the sequencing is correct.
    mNativeThreadHandle = MakeUnique<detail::NativeThreadHandle>();
  }

  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("CacheIOThread::ThreadFunc",
                             [self = RefPtr{this}] { self->ThreadFunc(); });

  nsCOMPtr<nsIThread> thread;
  nsresult rv =
      NS_NewNamedThread("Cache2 I/O", getter_AddRefs(thread), runnable);
  if (NS_FAILED(rv) || NS_FAILED(thread->GetPRThread(&mThread)) || !mThread) {
    MonitorAutoLock lock(mMonitor);
    mShutdown = true;
    return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult CacheIOThread::Dispatch(nsIRunnable* aRunnable, uint32_t aLevel) {
  return Dispatch(do_AddRef(aRunnable), aLevel);
}

nsresult CacheIOThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                 uint32_t aLevel) {
  NS_ENSURE_ARG(aLevel < LAST_LEVEL);

  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  // Runnable is always expected to be non-null, hard null-check bellow.
  MOZ_ASSERT(runnable);

  MonitorAutoLock lock(mMonitor);

  if (mShutdown && (PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_UNEXPECTED;
  }

  return DispatchInternal(runnable.forget(), aLevel);
}

nsresult CacheIOThread::DispatchAfterPendingOpens(nsIRunnable* aRunnable) {
  // Runnable is always expected to be non-null, hard null-check bellow.
  MOZ_ASSERT(aRunnable);

  MonitorAutoLock lock(mMonitor);

  if (mShutdown && (PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Move everything from later executed OPEN level to the OPEN_PRIORITY level
  // where we post the (eviction) runnable.
  mQueueLength[OPEN_PRIORITY] += mEventQueue[OPEN].Length();
  mQueueLength[OPEN] -= mEventQueue[OPEN].Length();
  mEventQueue[OPEN_PRIORITY].AppendElements(mEventQueue[OPEN]);
  mEventQueue[OPEN].Clear();

  return DispatchInternal(do_AddRef(aRunnable), OPEN_PRIORITY);
}

nsresult CacheIOThread::DispatchInternal(
    already_AddRefed<nsIRunnable> aRunnable, uint32_t aLevel) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  LogRunnable::LogDispatch(runnable.get());

  if (NS_WARN_IF(!runnable)) return NS_ERROR_NULL_POINTER;

  mMonitor.AssertCurrentThreadOwns();

  ++mQueueLength[aLevel];
  mEventQueue[aLevel].AppendElement(runnable.forget());
  if (mLowestLevelWaiting > aLevel) mLowestLevelWaiting = aLevel;

  mMonitor.NotifyAll();

  return NS_OK;
}

bool CacheIOThread::IsCurrentThread() {
  return mThread == PR_GetCurrentThread();
}

uint32_t CacheIOThread::QueueSize(bool highPriority) {
  MonitorAutoLock lock(mMonitor);
  if (highPriority) {
    return mQueueLength[OPEN_PRIORITY] + mQueueLength[READ_PRIORITY];
  }

  return mQueueLength[OPEN_PRIORITY] + mQueueLength[READ_PRIORITY] +
         mQueueLength[MANAGEMENT] + mQueueLength[OPEN] + mQueueLength[READ];
}

bool CacheIOThread::YieldInternal() {
  if (!IsCurrentThread()) {
    NS_WARNING(
        "Trying to yield to priority events on non-cache2 I/O thread? "
        "You probably do something wrong.");
    return false;
  }

  if (mCurrentlyExecutingLevel == XPCOM_LEVEL) {
    // Doesn't make any sense, since this handler is the one
    // that would be executed as the next one.
    return false;
  }

  if (!EventsPending(mCurrentlyExecutingLevel)) return false;

  mRerunCurrentEvent = true;
  return true;
}

void CacheIOThread::Shutdown() {
  if (!mThread) {
    return;
  }

  {
    MonitorAutoLock lock(mMonitor);
    mShutdown = true;
    mMonitor.NotifyAll();
  }

  if (nsIThread* thread = mXPCOMThread) {
    thread->Shutdown();
  }
  mThread = nullptr;
}

void CacheIOThread::CancelBlockingIO() {
  // This is an attempt to cancel any blocking I/O operation taking
  // too long time.
  if (!mNativeThreadHandle) {
    return;
  }

  if (!mIOCancelableEvents) {
    LOG(("CacheIOThread::CancelBlockingIO, no blocking operation to cancel"));
    return;
  }

  // OK, when we are here, we are processing an IO on the thread that
  // can be cancelled.
  mNativeThreadHandle->CancelBlockingIO(mMonitor);
}

already_AddRefed<nsIEventTarget> CacheIOThread::Target() {
  nsCOMPtr<nsIEventTarget> target;

  target = mXPCOMThread;
  if (!target && mThread) {
    MonitorAutoLock lock(mMonitor);
    while (!mXPCOMThread) {
      lock.Wait();
    }

    target = mXPCOMThread;
  }

  return target.forget();
}

void CacheIOThread::ThreadFunc() {
  AUTO_PROFILER_REGISTER_THREAD("Cache2 I/O");
  nsCOMPtr<nsIThreadInternal> threadInternal;

  {
    MonitorAutoLock lock(mMonitor);

    MOZ_ASSERT(mNativeThreadHandle);
    mNativeThreadHandle->InitThread();

    nsCOMPtr<nsIThread> xpcomThread = NS_GetCurrentThread();
    nsCOMPtr<nsIThread> thread = xpcomThread;

    threadInternal = do_QueryInterface(xpcomThread);
    if (threadInternal) {
      threadInternal->SetObserver(this);
    }

    mXPCOMThread = xpcomThread.forget().take();

    lock.NotifyAll();

    do {
    loopStart:
      // Reset the lowest level now, so that we can detect a new event on
      // a lower level (i.e. higher priority) has been scheduled while
      // executing any previously scheduled event.
      mLowestLevelWaiting = LAST_LEVEL;

      // Process xpcom events first
      while (mHasXPCOMEvents) {
        mHasXPCOMEvents = false;
        mCurrentlyExecutingLevel = XPCOM_LEVEL;

        MonitorAutoUnlock unlock(mMonitor);

        bool processedEvent;
        nsresult rv;
        do {
          rv = thread->ProcessNextEvent(false, &processedEvent);

          ++mEventCounter;
          MOZ_ASSERT(mNativeThreadHandle);
        } while (NS_SUCCEEDED(rv) && processedEvent);
      }

      uint32_t level;
      for (level = 0; level < LAST_LEVEL; ++level) {
        if (!mEventQueue[level].Length()) {
          // no events on this level, go to the next level
          continue;
        }

        LoopOneLevel(level);

        // Go to the first (lowest) level again
        goto loopStart;
      }

      if (EventsPending()) {
        continue;
      }

      if (mShutdown) {
        break;
      }

      AUTO_PROFILER_LABEL("CacheIOThread::ThreadFunc::Wait", IDLE);
      lock.Wait();

    } while (true);

    MOZ_ASSERT(!EventsPending());

    if (threadInternal) {
      threadInternal->SetObserver(nullptr);
    }

#ifdef DEBUG
    // This is for correct assertion on XPCOM events dispatch.
    mInsideLoop = false;
#endif
  }  // lock
}

void CacheIOThread::LoopOneLevel(uint32_t aLevel) {
  mMonitor.AssertCurrentThreadOwns();
  EventQueue events = std::move(mEventQueue[aLevel]);
  EventQueue::size_type length = events.Length();

  mCurrentlyExecutingLevel = aLevel;

  bool returnEvents = false;

  EventQueue::size_type index;
  {
    MonitorAutoUnlock unlock(mMonitor);

    for (index = 0; index < length; ++index) {
      if (EventsPending(aLevel)) {
        // Somebody scheduled a new event on a lower level, break and harry
        // to execute it!  Don't forget to return what we haven't exec.
        returnEvents = true;
        break;
      }

      // Drop any previous flagging, only an event on the current level may set
      // this flag.
      mRerunCurrentEvent = false;

      LogRunnable::Run log(events[index].get());

      events[index]->Run();

      MOZ_ASSERT(mNativeThreadHandle);

      if (mRerunCurrentEvent) {
        // The event handler yields to higher priority events and wants to
        // rerun.
        log.WillRunAgain();
        returnEvents = true;
        break;
      }

      ++mEventCounter;
      --mQueueLength[aLevel];

      // Release outside the lock.
      events[index] = nullptr;
    }
  }

  if (returnEvents) {
    // This code must prevent any AddRef/Release calls on the stored COMPtrs as
    // it might be exhaustive and block the monitor's lock for an excessive
    // amout of time.

    // 'index' points at the event that was interrupted and asked for re-run,
    // all events before have run, been nullified, and can be removed.
    events.RemoveElementsAt(0, index);
    // Move events that might have been scheduled on this queue to the tail to
    // preserve the expected per-queue FIFO order.
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    events.AppendElements(std::move(mEventQueue[aLevel]));
    // And finally move everything back to the main queue.
    mEventQueue[aLevel] = std::move(events);
  }
}

bool CacheIOThread::EventsPending(uint32_t aLastLevel) {
  return mLowestLevelWaiting < aLastLevel || mHasXPCOMEvents;
}

NS_IMETHODIMP CacheIOThread::OnDispatchedEvent() {
  MonitorAutoLock lock(mMonitor);
  mHasXPCOMEvents = true;
  // There's a race between the observer being removed on the CacheIOThread
  // and the thread being shutdown on the main thread.
  // When shutting down, even if there are more events, they will be processed
  // by the XPCOM thread instead of ThreadFunc.
  MOZ_ASSERT(mInsideLoop || mShutdown);
  lock.Notify();
  return NS_OK;
}

NS_IMETHODIMP CacheIOThread::OnProcessNextEvent(nsIThreadInternal* thread,
                                                bool mayWait) {
  return NS_OK;
}

NS_IMETHODIMP CacheIOThread::AfterProcessNextEvent(nsIThreadInternal* thread,
                                                   bool eventWasProcessed) {
  return NS_OK;
}

// Memory reporting

size_t CacheIOThread::SizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  MonitorAutoLock lock(const_cast<CacheIOThread*>(this)->mMonitor);

  size_t n = 0;
  for (const auto& event : mEventQueue) {
    n += event.ShallowSizeOfExcludingThis(mallocSizeOf);
    // Events referenced by the queues are arbitrary objects we cannot be sure
    // are reported elsewhere as well as probably not implementing nsISizeOf
    // interface.  Deliberatly omitting them from reporting here.
  }

  return n;
}

size_t CacheIOThread::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
}

CacheIOThread::Cancelable::Cancelable(bool aCancelable)
    : mCancelable(aCancelable) {
  // This will only ever be used on the I/O thread,
  // which is expected to be alive longer than this class.
  MOZ_ASSERT(CacheIOThread::sSelf);
  MOZ_ASSERT(CacheIOThread::sSelf->IsCurrentThread());

  if (mCancelable) {
    ++CacheIOThread::sSelf->mIOCancelableEvents;
  }
}

CacheIOThread::Cancelable::~Cancelable() {
  MOZ_ASSERT(CacheIOThread::sSelf);

  if (mCancelable) {
    --CacheIOThread::sSelf->mIOCancelableEvents;
  }
}

}  // namespace mozilla::net
