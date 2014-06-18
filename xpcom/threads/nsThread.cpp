/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThread.h"

#include "base/message_loop.h"

// Chromium's logging can sometimes leak through...
#ifdef LOG
#undef LOG
#endif

#include "mozilla/ReentrantMonitor.h"
#include "nsMemoryPressure.h"
#include "nsThreadManager.h"
#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "pratom.h"
#include "prlog.h"
#include "nsIObserverService.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/Services.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/ChaosMode.h"

#ifdef XP_LINUX
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#endif

#define HAVE_UALARM _BSD_SOURCE || (_XOPEN_SOURCE >= 500 ||                 \
                      _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED) &&           \
                      !(_POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)

#if defined(XP_LINUX) && !defined(ANDROID) && defined(_GNU_SOURCE)
#define HAVE_SCHED_SETAFFINITY
#endif

#ifdef MOZ_CANARY
# include <unistd.h>
# include <execinfo.h>
# include <signal.h>
# include <fcntl.h>
# include "nsXULAppAPI.h"
#endif

#if defined(NS_FUNCTION_TIMER) && defined(_MSC_VER)
#include "nsTimerImpl.h"
#include "nsStackWalk.h"
#endif
#ifdef NS_FUNCTION_TIMER
#include "nsCRT.h"
#endif

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetThreadLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("nsThread");
  }
  return sLog;
}
#endif
#ifdef LOG
#undef LOG
#endif
#define LOG(args) PR_LOG(GetThreadLog(), PR_LOG_DEBUG, args)

NS_DECL_CI_INTERFACE_GETTER(nsThread)

nsIThreadObserver* nsThread::sMainThreadObserver = nullptr;

//-----------------------------------------------------------------------------
// Because we do not have our own nsIFactory, we have to implement nsIClassInfo
// somewhat manually.

class nsThreadClassInfo : public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS_INHERITED  // no mRefCnt
  NS_DECL_NSICLASSINFO

  nsThreadClassInfo()
  {
  }
};

NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadClassInfo::AddRef()
{
  return 2;
}
NS_IMETHODIMP_(MozExternalRefCountType)
nsThreadClassInfo::Release()
{
  return 1;
}
NS_IMPL_QUERY_INTERFACE(nsThreadClassInfo, nsIClassInfo)

NS_IMETHODIMP
nsThreadClassInfo::GetInterfaces(uint32_t* aCount, nsIID*** aArray)
{
  return NS_CI_INTERFACE_GETTER_NAME(nsThread)(aCount, aArray);
}

NS_IMETHODIMP
nsThreadClassInfo::GetHelperForLanguage(uint32_t aLang, nsISupports** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetContractID(char** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassDescription(char** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassID(nsCID** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetImplementationLanguage(uint32_t* aResult)
{
  *aResult = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetFlags(uint32_t* aResult)
{
  *aResult = THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadClassInfo::GetClassIDNoAlloc(nsCID* aResult)
{
  return NS_ERROR_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsThread)
NS_IMPL_RELEASE(nsThread)
NS_INTERFACE_MAP_BEGIN(nsThread)
  NS_INTERFACE_MAP_ENTRY(nsIThread)
  NS_INTERFACE_MAP_ENTRY(nsIThreadInternal)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIThread)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    static nsThreadClassInfo sThreadClassInfo;
    foundInterface = static_cast<nsIClassInfo*>(&sThreadClassInfo);
  } else
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER(nsThread, nsIThread, nsIThreadInternal,
                            nsIEventTarget, nsISupportsPriority)

//-----------------------------------------------------------------------------

class nsThreadStartupEvent : public nsRunnable
{
public:
  nsThreadStartupEvent()
    : mMon("nsThreadStartupEvent.mMon")
    , mInitialized(false)
  {
  }

  // This method does not return until the thread startup object is in the
  // completion state.
  void Wait()
  {
    if (mInitialized) {
      // Maybe avoid locking...
      return;
    }

    ReentrantMonitorAutoEnter mon(mMon);
    while (!mInitialized) {
      mon.Wait();
    }
  }

  // This method needs to be public to support older compilers (xlC_r on AIX).
  // It should be called directly as this class type is reference counted.
  virtual ~nsThreadStartupEvent()
  {
  }

private:
  NS_IMETHOD Run()
  {
    ReentrantMonitorAutoEnter mon(mMon);
    mInitialized = true;
    mon.Notify();
    return NS_OK;
  }

  ReentrantMonitor mMon;
  bool mInitialized;
};

//-----------------------------------------------------------------------------

struct nsThreadShutdownContext
{
  nsThread* joiningThread;
  bool      shutdownAck;
};

// This event is responsible for notifying nsThread::Shutdown that it is time
// to call PR_JoinThread.
class nsThreadShutdownAckEvent : public nsRunnable
{
public:
  nsThreadShutdownAckEvent(nsThreadShutdownContext* aCtx)
    : mShutdownContext(aCtx)
  {
  }
  NS_IMETHOD Run()
  {
    mShutdownContext->shutdownAck = true;
    return NS_OK;
  }
private:
  nsThreadShutdownContext* mShutdownContext;
};

// This event is responsible for setting mShutdownContext
class nsThreadShutdownEvent : public nsRunnable
{
public:
  nsThreadShutdownEvent(nsThread* aThr, nsThreadShutdownContext* aCtx)
    : mThread(aThr)
    , mShutdownContext(aCtx)
  {
  }
  NS_IMETHOD Run()
  {
    mThread->mShutdownContext = mShutdownContext;
    MessageLoop::current()->Quit();
    return NS_OK;
  }
private:
  nsRefPtr<nsThread>       mThread;
  nsThreadShutdownContext* mShutdownContext;
};

//-----------------------------------------------------------------------------

static void
SetupCurrentThreadForChaosMode()
{
  if (!ChaosMode::isActive()) {
    return;
  }

#ifdef XP_LINUX
  // PR_SetThreadPriority doesn't really work since priorities >
  // PR_PRIORITY_NORMAL can't be set by non-root users. Instead we'll just use
  // setpriority(2) to set random 'nice values'. In regular Linux this is only
  // a dynamic adjustment so it still doesn't really do what we want, but tools
  // like 'rr' can be more aggressive about honoring these values.
  // Some of these calls may fail due to trying to lower the priority
  // (e.g. something may have already called setpriority() for this thread).
  // This makes it hard to have non-main threads with higher priority than the
  // main thread, but that's hard to fix. Tools like rr can choose to honor the
  // requested values anyway.
  // Use just 4 priorities so there's a reasonable chance of any two threads
  // having equal priority.
  setpriority(PRIO_PROCESS, 0, ChaosMode::randomUint32LessThan(4));
#else
  // We should set the affinity here but NSPR doesn't provide a way to expose it.
  uint32_t priority = ChaosMode::randomUint32LessThan(PR_PRIORITY_LAST + 1);
  PR_SetThreadPriority(PR_GetCurrentThread(), PRThreadPriority(priority));
#endif

#ifdef HAVE_SCHED_SETAFFINITY
  // Force half the threads to CPU 0 so they compete for CPU
  if (ChaosMode::randomUint32LessThan(2)) {
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(0, &cpus);
    sched_setaffinity(0, sizeof(cpus), &cpus);
  }
#endif
}

/*static*/ void
nsThread::ThreadFunc(void* aArg)
{
  nsThread* self = static_cast<nsThread*>(aArg);  // strong reference
  self->mThread = PR_GetCurrentThread();
  SetupCurrentThreadForChaosMode();

  // Inform the ThreadManager
  nsThreadManager::get()->RegisterCurrentThread(self);

  mozilla::IOInterposer::RegisterCurrentThread();

  // Wait for and process startup event
  nsCOMPtr<nsIRunnable> event;
  if (!self->GetEvent(true, getter_AddRefs(event))) {
    NS_WARNING("failed waiting for thread startup event");
    return;
  }
  event->Run();  // unblocks nsThread::Init
  event = nullptr;

  {
    // Scope for MessageLoop.
    nsAutoPtr<MessageLoop> loop(
      new MessageLoop(MessageLoop::TYPE_MOZILLA_NONMAINTHREAD));

    // Now, process incoming events...
    loop->Run();

    // Do NS_ProcessPendingEvents but with special handling to set
    // mEventsAreDoomed atomically with the removal of the last event. The key
    // invariant here is that we will never permit PutEvent to succeed if the
    // event would be left in the queue after our final call to
    // NS_ProcessPendingEvents.
    while (true) {
      {
        MutexAutoLock lock(self->mLock);
        if (!self->mEvents->HasPendingEvent()) {
          // No events in the queue, so we will stop now. Don't let any more
          // events be added, since they won't be processed. It is critical
          // that no PutEvent can occur between testing that the event queue is
          // empty and setting mEventsAreDoomed!
          self->mEventsAreDoomed = true;
          break;
        }
      }
      NS_ProcessPendingEvents(self);
    }
  }

  mozilla::IOInterposer::UnregisterCurrentThread();

  // Inform the threadmanager that this thread is going away
  nsThreadManager::get()->UnregisterCurrentThread(self);

  // Dispatch shutdown ACK
  event = new nsThreadShutdownAckEvent(self->mShutdownContext);
  self->mShutdownContext->joiningThread->Dispatch(event, NS_DISPATCH_NORMAL);

  // Release any observer of the thread here.
  self->SetObserver(nullptr);

#ifdef MOZ_TASK_TRACER
  FreeTraceInfo();
#endif

  NS_RELEASE(self);
}

//-----------------------------------------------------------------------------

#ifdef MOZ_CANARY
int sCanaryOutputFD = -1;
#endif

nsThread::nsThread(MainThreadFlag aMainThread, uint32_t aStackSize)
  : mLock("nsThread.mLock")
  , mEvents(&mEventsRoot)
  , mPriority(PRIORITY_NORMAL)
  , mThread(nullptr)
  , mRunningEvent(0)
  , mStackSize(aStackSize)
  , mShutdownContext(nullptr)
  , mShutdownRequired(false)
  , mEventsAreDoomed(false)
  , mIsMainThread(aMainThread)
{
}

nsThread::~nsThread()
{
}

nsresult
nsThread::Init()
{
  // spawn thread and wait until it is fully setup
  nsRefPtr<nsThreadStartupEvent> startup = new nsThreadStartupEvent();

  NS_ADDREF_THIS();

  mShutdownRequired = true;

  // ThreadFunc is responsible for setting mThread
  PRThread* thr = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                                  PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                  PR_JOINABLE_THREAD, mStackSize);
  if (!thr) {
    NS_RELEASE_THIS();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // ThreadFunc will wait for this event to be run before it tries to access
  // mThread.  By delaying insertion of this event into the queue, we ensure
  // that mThread is set properly.
  {
    MutexAutoLock lock(mLock);
    mEventsRoot.PutEvent(startup);
  }

  // Wait for thread to call ThreadManager::SetupCurrentThread, which completes
  // initialization of ThreadFunc.
  startup->Wait();
  return NS_OK;
}

nsresult
nsThread::InitCurrentThread()
{
  mThread = PR_GetCurrentThread();
  SetupCurrentThreadForChaosMode();

  nsThreadManager::get()->RegisterCurrentThread(this);
  return NS_OK;
}

nsresult
nsThread::PutEvent(nsIRunnable* aEvent, nsNestedEventTarget* aTarget)
{
  {
    MutexAutoLock lock(mLock);
    nsChainedEventQueue* queue = aTarget ? aTarget->mQueue : &mEventsRoot;
    if (!queue || (queue == &mEventsRoot && mEventsAreDoomed)) {
      NS_WARNING("An event was posted to a thread that will never run it (rejected)");
      return NS_ERROR_UNEXPECTED;
    }
    queue->PutEvent(aEvent);
  }

  nsCOMPtr<nsIThreadObserver> obs = GetObserver();
  if (obs) {
    obs->OnDispatchedEvent(this);
  }

  return NS_OK;
}

nsresult
nsThread::DispatchInternal(nsIRunnable* aEvent, uint32_t aFlags,
                           nsNestedEventTarget* aTarget)
{
  if (NS_WARN_IF(!aEvent)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (gXPCOMThreadsShutDown && MAIN_THREAD != mIsMainThread && !aTarget) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

#ifdef MOZ_TASK_TRACER
  nsRefPtr<nsIRunnable> tracedRunnable = CreateTracedRunnable(aEvent);
  aEvent = tracedRunnable;
#endif

  if (aFlags & DISPATCH_SYNC) {
    nsThread* thread = nsThreadManager::get()->GetCurrentThread();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // XXX we should be able to do something better here... we should
    //     be able to monitor the slot occupied by this event and use
    //     that to tell us when the event has been processed.

    nsRefPtr<nsThreadSyncDispatch> wrapper =
      new nsThreadSyncDispatch(thread, aEvent);
    if (!wrapper) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = PutEvent(wrapper, aTarget);
    // Don't wait for the event to finish if we didn't dispatch it...
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Allows waiting; ensure no locks are held that would deadlock us!
    while (wrapper->IsPending()) {
      NS_ProcessNextEvent(thread, true);
    }
    return wrapper->Result();
  }

  NS_ASSERTION(aFlags == NS_DISPATCH_NORMAL, "unexpected dispatch flags");
  return PutEvent(aEvent, aTarget);
}

//-----------------------------------------------------------------------------
// nsIEventTarget

NS_IMETHODIMP
nsThread::Dispatch(nsIRunnable* aEvent, uint32_t aFlags)
{
  LOG(("THRD(%p) Dispatch [%p %x]\n", this, aEvent, aFlags));

  return DispatchInternal(aEvent, aFlags, nullptr);
}

NS_IMETHODIMP
nsThread::IsOnCurrentThread(bool* aResult)
{
  *aResult = (PR_GetCurrentThread() == mThread);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIThread

NS_IMETHODIMP
nsThread::GetPRThread(PRThread** aResult)
{
  *aResult = mThread;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::Shutdown()
{
  LOG(("THRD(%p) shutdown\n", this));

  // XXX If we make this warn, then we hit that warning at xpcom shutdown while
  //     shutting down a thread in a thread pool.  That happens b/c the thread
  //     in the thread pool is already shutdown by the thread manager.
  if (!mThread) {
    return NS_OK;
  }

  if (NS_WARN_IF(mThread == PR_GetCurrentThread())) {
    return NS_ERROR_UNEXPECTED;
  }

  // Prevent multiple calls to this method
  {
    MutexAutoLock lock(mLock);
    if (!mShutdownRequired) {
      return NS_ERROR_UNEXPECTED;
    }
    mShutdownRequired = false;
  }

  nsThreadShutdownContext context;
  context.joiningThread = nsThreadManager::get()->GetCurrentThread();
  context.shutdownAck = false;

  // Set mShutdownContext and wake up the thread in case it is waiting for
  // events to process.
  nsCOMPtr<nsIRunnable> event = new nsThreadShutdownEvent(this, &context);
  if (!event) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // XXXroc What if posting the event fails due to OOM?
  PutEvent(event, nullptr);

  // We could still end up with other events being added after the shutdown
  // task, but that's okay because we process pending events in ThreadFunc
  // after setting mShutdownContext just before exiting.

  // Process events on the current thread until we receive a shutdown ACK.
  // Allows waiting; ensure no locks are held that would deadlock us!
  while (!context.shutdownAck) {
    NS_ProcessNextEvent(context.joiningThread, true);
  }

  // Now, it should be safe to join without fear of dead-locking.

  PR_JoinThread(mThread);
  mThread = nullptr;

  // We hold strong references to our event observers, and once the thread is
  // shut down the observers can't easily unregister themselves. Do it here
  // to avoid leaking.
  ClearObservers();

#ifdef DEBUG
  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(!mObserver, "Should have been cleared at shutdown!");
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsThread::HasPendingEvents(bool* aResult)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  *aResult = mEvents->GetEvent(false, nullptr);
  return NS_OK;
}

#ifdef MOZ_CANARY
void canary_alarm_handler(int signum);

class Canary
{
  //XXX ToDo: support nested loops
public:
  Canary()
  {
    if (sCanaryOutputFD > 0 && EventLatencyIsImportant()) {
      signal(SIGALRM, canary_alarm_handler);
      ualarm(15000, 0);
    }
  }

  ~Canary()
  {
    if (sCanaryOutputFD != 0 && EventLatencyIsImportant()) {
      ualarm(0, 0);
    }
  }

  static bool EventLatencyIsImportant()
  {
    return NS_IsMainThread() && XRE_GetProcessType() == GeckoProcessType_Default;
  }
};

void canary_alarm_handler(int signum)
{
  void* array[30];
  const char msg[29] = "event took too long to run:\n";
  // use write to be safe in the signal handler
  write(sCanaryOutputFD, msg, sizeof(msg));
  backtrace_symbols_fd(array, backtrace(array, 30), sCanaryOutputFD);
}

#endif

#define NOTIFY_EVENT_OBSERVERS(func_, params_)                                 \
  PR_BEGIN_MACRO                                                               \
    if (!mEventObservers.IsEmpty()) {                                          \
      nsAutoTObserverArray<nsCOMPtr<nsIThreadObserver>, 2>::ForwardIterator    \
        iter_(mEventObservers);                                                \
      nsCOMPtr<nsIThreadObserver> obs_;                                        \
      while (iter_.HasMore()) {                                                \
        obs_ = iter_.GetNext();                                                \
        obs_ -> func_ params_ ;                                                \
      }                                                                        \
    }                                                                          \
  PR_END_MACRO

NS_IMETHODIMP
nsThread::ProcessNextEvent(bool aMayWait, bool* aResult)
{
  LOG(("THRD(%p) ProcessNextEvent [%u %u]\n", this, aMayWait, mRunningEvent));

  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  // The toplevel event loop normally blocks waiting for the next event, but
  // if we're trying to shut this thread down, we must exit the event loop when
  // the event queue is empty.
  // This only applys to the toplevel event loop! Nested event loops (e.g.
  // during sync dispatch) are waiting for some state change and must be able
  // to block even if something has requested shutdown of the thread. Otherwise
  // we'll just busywait as we endlessly look for an event, fail to find one,
  // and repeat the nested event loop since its state change hasn't happened yet.
  bool reallyWait = aMayWait && (mRunningEvent > 0 || !ShuttingDown());

  if (MAIN_THREAD == mIsMainThread && reallyWait) {
    HangMonitor::Suspend();
  }

  // Fire a memory pressure notification, if we're the main thread and one is
  // pending.
  if (MAIN_THREAD == mIsMainThread && !ShuttingDown()) {
    MemoryPressureState mpPending = NS_GetPendingMemoryPressure();
    if (mpPending != MemPressure_None) {
      nsCOMPtr<nsIObserverService> os = services::GetObserverService();

      // Use no-forward to prevent the notifications from being transferred to
      // the children of this process.
      NS_NAMED_LITERAL_STRING(lowMem, "low-memory-no-forward");
      NS_NAMED_LITERAL_STRING(lowMemOngoing, "low-memory-ongoing-no-forward");

      if (os) {
        os->NotifyObservers(nullptr, "memory-pressure",
                            mpPending == MemPressure_New ? lowMem.get() :
                            lowMemOngoing.get());
      } else {
        NS_WARNING("Can't get observer service!");
      }
    }
  }

  bool notifyMainThreadObserver =
    (MAIN_THREAD == mIsMainThread) && sMainThreadObserver;
  if (notifyMainThreadObserver) {
    sMainThreadObserver->OnProcessNextEvent(this, reallyWait, mRunningEvent);
  }

  nsCOMPtr<nsIThreadObserver> obs = mObserver;
  if (obs) {
    obs->OnProcessNextEvent(this, reallyWait, mRunningEvent);
  }

  NOTIFY_EVENT_OBSERVERS(OnProcessNextEvent,
                         (this, reallyWait, mRunningEvent));

  ++mRunningEvent;

#ifdef MOZ_CANARY
  Canary canary;
#endif
  nsresult rv = NS_OK;

  {
    // Scope for |event| to make sure that its destructor fires while
    // mRunningEvent has been incremented, since that destructor can
    // also do work.

    // If we are shutting down, then do not wait for new events.
    nsCOMPtr<nsIRunnable> event;
    mEvents->GetEvent(reallyWait, getter_AddRefs(event));

    *aResult = (event.get() != nullptr);

    if (event) {
      LOG(("THRD(%p) running [%p]\n", this, event.get()));
      if (MAIN_THREAD == mIsMainThread) {
        HangMonitor::NotifyActivity();
      }
      event->Run();
    } else if (aMayWait) {
      MOZ_ASSERT(ShuttingDown(),
                 "This should only happen when shutting down");
      rv = NS_ERROR_UNEXPECTED;
    }
  }

  --mRunningEvent;

  NOTIFY_EVENT_OBSERVERS(AfterProcessNextEvent,
                         (this, mRunningEvent, *aResult));

  if (obs) {
    obs->AfterProcessNextEvent(this, mRunningEvent, *aResult);
  }

  if (notifyMainThreadObserver && sMainThreadObserver) {
    sMainThreadObserver->AfterProcessNextEvent(this, mRunningEvent, *aResult);
  }

  return rv;
}

//-----------------------------------------------------------------------------
// nsISupportsPriority

NS_IMETHODIMP
nsThread::GetPriority(int32_t* aPriority)
{
  *aPriority = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetPriority(int32_t aPriority)
{
  if (NS_WARN_IF(!mThread)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // NSPR defines the following four thread priorities:
  //   PR_PRIORITY_LOW
  //   PR_PRIORITY_NORMAL
  //   PR_PRIORITY_HIGH
  //   PR_PRIORITY_URGENT
  // We map the priority values defined on nsISupportsPriority to these values.

  mPriority = aPriority;

  PRThreadPriority pri;
  if (mPriority <= PRIORITY_HIGHEST) {
    pri = PR_PRIORITY_URGENT;
  } else if (mPriority < PRIORITY_NORMAL) {
    pri = PR_PRIORITY_HIGH;
  } else if (mPriority > PRIORITY_NORMAL) {
    pri = PR_PRIORITY_LOW;
  } else {
    pri = PR_PRIORITY_NORMAL;
  }
  // If chaos mode is active, retain the randomly chosen priority
  if (!ChaosMode::isActive()) {
    PR_SetThreadPriority(mThread, pri);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThread::AdjustPriority(int32_t aDelta)
{
  return SetPriority(mPriority + aDelta);
}

//-----------------------------------------------------------------------------
// nsIThreadInternal

NS_IMETHODIMP
nsThread::GetObserver(nsIThreadObserver** aObs)
{
  MutexAutoLock lock(mLock);
  NS_IF_ADDREF(*aObs = mObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::SetObserver(nsIThreadObserver* aObs)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  MutexAutoLock lock(mLock);
  mObserver = aObs;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::GetRecursionDepth(uint32_t* aDepth)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  *aDepth = mRunningEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsThread::AddObserver(nsIThreadObserver* aObserver)
{
  if (NS_WARN_IF(!aObserver)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_WARN_IF_FALSE(!mEventObservers.Contains(aObserver),
                   "Adding an observer twice!");

  if (!mEventObservers.AppendElement(aObserver)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThread::RemoveObserver(nsIThreadObserver* aObserver)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (aObserver && !mEventObservers.RemoveElement(aObserver)) {
    NS_WARNING("Removing an observer that was never added!");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThread::PushEventQueue(nsIEventTarget** aResult)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsChainedEventQueue* queue = new nsChainedEventQueue();
  queue->mEventTarget = new nsNestedEventTarget(this, queue);

  {
    MutexAutoLock lock(mLock);
    queue->mNext = mEvents;
    mEvents = queue;
  }

  NS_ADDREF(*aResult = queue->mEventTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsThread::PopEventQueue(nsIEventTarget* aInnermostTarget)
{
  if (NS_WARN_IF(PR_GetCurrentThread() != mThread)) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (NS_WARN_IF(!aInnermostTarget)) {
    return NS_ERROR_NULL_POINTER;
  }

  // Don't delete or release anything while holding the lock.
  nsAutoPtr<nsChainedEventQueue> queue;
  nsRefPtr<nsNestedEventTarget> target;

  {
    MutexAutoLock lock(mLock);

    // Make sure we're popping the innermost event target.
    if (NS_WARN_IF(mEvents->mEventTarget != aInnermostTarget)) {
      return NS_ERROR_UNEXPECTED;
    }

    MOZ_ASSERT(mEvents != &mEventsRoot);

    queue = mEvents;
    mEvents = mEvents->mNext;

    nsCOMPtr<nsIRunnable> event;
    while (queue->GetEvent(false, getter_AddRefs(event))) {
      mEvents->PutEvent(event);
    }

    // Don't let the event target post any more events.
    queue->mEventTarget.swap(target);
    target->mQueue = nullptr;
  }

  return NS_OK;
}

nsresult
nsThread::SetMainThreadObserver(nsIThreadObserver* aObserver)
{
  if (aObserver && nsThread::sMainThreadObserver) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!NS_IsMainThread()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsThread::sMainThreadObserver = aObserver;
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsThreadSyncDispatch::Run()
{
  if (mSyncTask) {
    mResult = mSyncTask->Run();
    mSyncTask = nullptr;
    // unblock the origin thread
    mOrigin->Dispatch(this, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsThread::nsNestedEventTarget, nsIEventTarget)

NS_IMETHODIMP
nsThread::nsNestedEventTarget::Dispatch(nsIRunnable* aEvent, uint32_t aFlags)
{
  LOG(("THRD(%p) Dispatch [%p %x] to nested loop %p\n", mThread.get(), aEvent,
       aFlags, this));

  return mThread->DispatchInternal(aEvent, aFlags, this);
}

NS_IMETHODIMP
nsThread::nsNestedEventTarget::IsOnCurrentThread(bool* aResult)
{
  return mThread->IsOnCurrentThread(aResult);
}
