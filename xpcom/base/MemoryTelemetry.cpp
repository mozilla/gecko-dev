/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryTelemetry.h"
#include "nsMemoryReporterManager.h"

#include "mozilla/ClearOnShutdown.h"
#ifdef MOZ_PHC
#  include "mozilla/PHCManager.h"
#endif
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SimpleEnumerator.h"
#include "mozilla/glean/XpcomMetrics.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowOuter.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIMemoryReporter.h"
#include "nsIWindowMediator.h"
#include "nsImportModule.h"
#include "nsITelemetry.h"
#include "nsNetCID.h"
#include "nsObserverService.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

#include <cstdlib>

using namespace mozilla;

using mozilla::dom::AutoJSAPI;
using mozilla::dom::ContentParent;

// Do not gather data more than once a minute (in seconds)
static constexpr uint32_t kTelemetryIntervalS = 60;

// Do not create a timer for telemetry this many seconds after the previous one
// fires.  This exists so that we don't respond to our own timer.
static constexpr uint32_t kTelemetryCooldownS = 10;

// We use a sliding window to detect a reasonable amount of activity.  If there
// are more than kPokeWindowEvents events within kPokeWindowSeconds seconds then
// that counts as "active".
static constexpr unsigned kPokeWindowEvents = 10;
static constexpr unsigned kPokeWindowSeconds = 1;

static constexpr const char* kTopicShutdown = "content-child-shutdown";

namespace {

enum class PrevValue : uint32_t {
#ifdef XP_WIN
  low_memory_events_physical,
#endif
#if defined(XP_LINUX) && !defined(ANDROID)
  page_faults_hard,
#endif
  SIZE_,
};

}  // anonymous namespace

constexpr uint32_t kUninitialized = ~0;

static uint32_t gPrevValues[uint32_t(PrevValue::SIZE_)];

/*
 * Because even in "idle" processes there may be some background events,
 * ideally there shouldn't, we use a sliding window to determine if the process
 * is active or not.  If there are N recent calls to Poke() the browser is
 * active.
 *
 * This class implements the sliding window of timestamps.
 */
class TimeStampWindow {
 public:
  void push(TimeStamp aNow) {
    mEvents.insertBack(new Event(aNow));
    mNumEvents++;
  }

  // Remove any events older than aOld.
  void clearExpired(TimeStamp aOld) {
    Event* e = mEvents.getFirst();
    while (e && e->olderThan(aOld)) {
      e->removeFrom(mEvents);
      mNumEvents--;
      delete e;
      e = mEvents.getFirst();
    }
  }

  size_t numEvents() const { return mNumEvents; }

 private:
  class Event : public LinkedListElement<Event> {
   public:
    explicit Event(TimeStamp aTime) : mTime(aTime) {}

    bool olderThan(TimeStamp aOld) const { return mTime < aOld; }

   private:
    TimeStamp mTime;
  };

  size_t mNumEvents = 0;
  AutoCleanLinkedList<Event> mEvents;
};

NS_IMPL_ISUPPORTS(MemoryTelemetry, nsIObserver, nsISupportsWeakReference)

MemoryTelemetry::MemoryTelemetry()
    : mThreadPool(do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID)) {}

void MemoryTelemetry::Init() {
  for (auto& val : gPrevValues) {
    val = kUninitialized;
  }

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    MOZ_RELEASE_ASSERT(obs);

    obs->AddObserver(this, kTopicShutdown, true);
  }
}

/* static */ MemoryTelemetry& MemoryTelemetry::Get() {
  static RefPtr<MemoryTelemetry> sInstance;

  MOZ_ASSERT(NS_IsMainThread());

  if (!sInstance) {
    sInstance = new MemoryTelemetry();
    sInstance->Init();
    ClearOnShutdown(&sInstance);
  }
  return *sInstance;
}

void MemoryTelemetry::DelayedInit() {
  mCanRun = true;
  Poke();
}

void MemoryTelemetry::Poke() {
  // Don't do anything that might delay process startup
  if (!mCanRun) {
    return;
  }

  if (XRE_IsContentProcess() && !Telemetry::CanRecordReleaseData()) {
    // All memory telemetry produced by content processes is release data, so if
    // we're not recording release data then don't setup the timers on content
    // processes.
    return;
  }

  if (XRE_IsContentProcess()) {
    auto& remoteType = dom::ContentChild::GetSingleton()->GetRemoteType();
    if (remoteType == PREALLOC_REMOTE_TYPE) {
      // Preallocated processes should stay dormant and not run this telemetry
      // code.
      return;
    }
  }

  TimeStamp now = TimeStamp::Now();
  if (mPokeWindow) {
    mPokeWindow->clearExpired(now -
                              TimeDuration::FromSeconds(kPokeWindowSeconds));
  }

  if (mLastRun &&
      now - mLastRun < TimeDuration::FromSeconds(kTelemetryCooldownS)) {
    // If we last gathered telemetry less than kTelemetryCooldownS seconds ago
    // then Poke() does nothing.  This is to prevent our own timer waking us up.
    // In the condition above `now - mLastRun` is how long ago we last gathered
    // telemetry.
    return;
  }

  // Even idle processes have some events, so we only want to create the timer
  // if there's been several events in the last small window.
  if (!mPokeWindow) {
    mPokeWindow = MakeUnique<TimeStampWindow>();
  }
  mPokeWindow->push(now);
  if (mPokeWindow->numEvents() < kPokeWindowEvents) {
    return;
  }
  mPokeWindow = nullptr;

  mLastPoke = now;
  if (!mTimer) {
    TimeDuration delay = TimeDuration::FromSeconds(kTelemetryIntervalS);
    if (mLastRun) {
      delay = std::min(delay,
                       std::max(TimeDuration::FromSeconds(kTelemetryCooldownS),
                                delay - (now - mLastRun)));
    }
    RefPtr<MemoryTelemetry> self(this);
    auto res = NS_NewTimerWithCallback(
        [self](nsITimer* aTimer) { self->GatherReports(); }, delay,
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "MemoryTelemetry::GatherReports");

    if (res.isOk()) {
      // Errors are ignored, if there was an error then we just don't get
      // telemetry.
      mTimer = res.unwrap();
    }
  }
}

nsresult MemoryTelemetry::Shutdown() {
  if (mTimer) {
    mTimer->Cancel();
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  MOZ_RELEASE_ASSERT(obs);

  obs->RemoveObserver(this, kTopicShutdown);

  return NS_OK;
}

nsresult MemoryTelemetry::GatherReports(
    const std::function<void()>& aCompletionCallback) {
  auto cleanup = MakeScopeExit([&]() {
    if (aCompletionCallback) {
      aCompletionCallback();
    }
  });

  mLastRun = TimeStamp::Now();
  mTimer = nullptr;

  RefPtr<nsMemoryReporterManager> mgr = nsMemoryReporterManager::GetOrCreate();
  MOZ_DIAGNOSTIC_ASSERT(mgr);
  NS_ENSURE_TRUE(mgr, NS_ERROR_FAILURE);

#define RECORD_OUTER(metric, inner)                                   \
  do {                                                                \
    int64_t amt;                                                      \
    nsresult rv = mgr->Get##metric(&amt);                             \
    if (NS_SUCCEEDED(rv)) {                                           \
      inner                                                           \
    } else if (rv != NS_ERROR_NOT_AVAILABLE) {                        \
      NS_WARNING("Failed to retrieve memory telemetry for " #metric); \
    }                                                                 \
  } while (0)
#define RECORD_COUNT(id, metric) \
  RECORD_OUTER(metric, glean::memory::id.AccumulateSingleSample(amt);)
#define RECORD_BYTES(id, metric) \
  RECORD_OUTER(metric, glean::memory::id.Accumulate(amt / 1024);)
#define RECORD_PERCENTAGE(id, metric) \
  RECORD_OUTER(metric, glean::memory::id.AccumulateSingleSample(amt / 100);)
#define RECORD_COUNT_CUMULATIVE(id, metric)                               \
  RECORD_OUTER(                                                           \
      metric, uint32_t prev = gPrevValues[uint32_t(PrevValue::id)];       \
      gPrevValues[uint32_t(PrevValue::id)] = amt;                         \
                                                                          \
      /* If this is the first time we're reading this reporter, store its \
       * current value but don't report it in the telemetry ping, so we   \
       * ignore the effect startup had on the reporter. */                \
      if (prev != kUninitialized) {                                       \
        glean::memory::id.AccumulateSingleSample(amt - prev);             \
      })

  // GHOST_WINDOWS is opt-out as of Firefox 55
  RECORD_COUNT(ghost_windows, GhostWindows);

  // If we're running in the parent process, collect data from all processes for
  // the MEMORY_TOTAL histogram.
  if (XRE_IsParentProcess() && !mGatheringTotalMemory) {
    GatherTotalMemory();
  }

  if (!Telemetry::CanRecordReleaseData()) {
    return NS_OK;
  }

  // Get memory measurements from distinguished amount attributes.  We used
  // to measure "explicit" too, but it could cause hangs, and the data was
  // always really noisy anyway.  See bug 859657.
  //
  // test_TelemetrySession.js relies on some of these histograms being
  // here.  If you remove any of the following histograms from here, you'll
  // have to modify test_TelemetrySession.js:
  //
  //   * MEMORY_TOTAL,
  //   * MEMORY_JS_GC_HEAP, and
  //   * MEMORY_JS_COMPARTMENTS_SYSTEM.
  //
  // The distinguished amount attribute names don't match the telemetry id
  // names in some cases due to a combination of (a) historical reasons, and
  // (b) the fact that we can't change telemetry id names without breaking
  // data continuity.

  // Collect cheap or main-thread only metrics synchronously, on the main
  // thread.
  RECORD_BYTES(js_gc_heap, JSMainRuntimeGCHeap);
  RECORD_COUNT(js_compartments_system, JSMainRuntimeCompartmentsSystem);
  RECORD_COUNT(js_compartments_user, JSMainRuntimeCompartmentsUser);
  RECORD_COUNT(js_realms_system, JSMainRuntimeRealmsSystem);
  RECORD_COUNT(js_realms_user, JSMainRuntimeRealmsUser);
  RECORD_BYTES(images_content_used_uncompressed, ImagesContentUsedUncompressed);
  RECORD_BYTES(storage_sqlite, StorageSQLite);
#ifdef XP_WIN
  RECORD_COUNT_CUMULATIVE(low_memory_events_physical, LowMemoryEventsPhysical);
#endif
#if defined(XP_LINUX) && !defined(ANDROID)
  RECORD_COUNT_CUMULATIVE(page_faults_hard, PageFaultsHard);
#endif

#ifdef HAVE_JEMALLOC_STATS
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);
  glean::memory::heap_allocated.Accumulate(mgr->HeapAllocated(stats) / 1024);
  glean::memory::heap_overhead_fraction.AccumulateSingleSample(
      mgr->HeapOverheadFraction(stats) / 100);
#endif

#ifdef MOZ_PHC
  ReportPHCTelemetry();
#endif

  RefPtr<Runnable> completionRunnable;
  if (aCompletionCallback) {
    completionRunnable = NS_NewRunnableFunction(__func__, aCompletionCallback);
  }

  // Collect expensive metrics that can be calculated off-main-thread
  // asynchronously, on a background thread.
  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "MemoryTelemetry::GatherReports", [mgr, completionRunnable]() mutable {
        auto timer = glean::memory::collection_time.Measure();
// Each WebAssembly program eats up an entire 32-bits worth of address space,
// which makes vsize rather useless on 64-bit systems, and will cause telemetry
// to frequently hit the max value of 1TB, so only record it in 32-bit builds.
#if !defined(HAVE_64BIT_BUILD)
        RECORD_BYTES(vsize, Vsize);
#endif
#if !defined(HAVE_64BIT_BUILD) || !defined(XP_WIN)
        RECORD_BYTES(vsize_max_contiguous, VsizeMaxContiguous);
#endif
        RECORD_BYTES(resident_fast, ResidentFast);
        RECORD_BYTES(resident_peak, ResidentPeak);
// Although we can measure unique memory on MacOS we choose not to, because
// doing so is too slow for telemetry.
#ifndef XP_MACOSX
        RECORD_BYTES(unique, ResidentUnique);
#endif

        if (completionRunnable) {
          NS_DispatchToMainThread(completionRunnable.forget(),
                                  NS_DISPATCH_NORMAL);
        }
      });

#undef RECORD

  nsresult rv = mThreadPool->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    cleanup.release();
  }

  return NS_OK;
}

namespace {
struct ChildProcessInfo {
  GeckoProcessType mType;
#if defined(XP_WIN)
  HANDLE mHandle;
#elif defined(XP_MACOSX)
  task_t mHandle;
#else
  pid_t mHandle;
#endif
};
}  // namespace

/**
 * Runs a task on the background thread pool to fetch the memory usage of all
 * processes.
 */
void MemoryTelemetry::GatherTotalMemory() {
  MOZ_ASSERT(!mGatheringTotalMemory);
  mGatheringTotalMemory = true;

  nsTArray<ChildProcessInfo> infos;
  mozilla::ipc::GeckoChildProcessHost::GetAll(
      [&](mozilla::ipc::GeckoChildProcessHost* aGeckoProcess) {
        if (!aGeckoProcess->GetChildProcessHandle()) {
          return;
        }

        ChildProcessInfo info{};
        info.mType = aGeckoProcess->GetProcessType();

        // NOTE: For now we ignore non-content processes here for compatibility
        // with the existing probe. We may want to introduce a new probe in the
        // future which also collects data for non-content processes.
        if (info.mType != GeckoProcessType_Content) {
          return;
        }

#if defined(XP_WIN)
        if (!::DuplicateHandle(::GetCurrentProcess(),
                               aGeckoProcess->GetChildProcessHandle(),
                               ::GetCurrentProcess(), &info.mHandle, 0, false,
                               DUPLICATE_SAME_ACCESS)) {
          return;
        }
#elif defined(XP_MACOSX)
        info.mHandle = aGeckoProcess->GetChildTask();
        if (mach_port_mod_refs(mach_task_self(), info.mHandle,
                               MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS) {
          return;
        }
#else
        info.mHandle = aGeckoProcess->GetChildProcessId();
#endif

        infos.AppendElement(info);
      });

  mThreadPool->Dispatch(NS_NewRunnableFunction(
      "MemoryTelemetry::GatherTotalMemory", [infos = std::move(infos)] {
        RefPtr<nsMemoryReporterManager> mgr =
            nsMemoryReporterManager::GetOrCreate();
        MOZ_RELEASE_ASSERT(mgr);

        int64_t totalMemory = mgr->ResidentFast();
        nsTArray<int64_t> childSizes(infos.Length());

        // Use our handle for the remote process to collect resident unique set
        // size information for that process.
        bool success = true;
        for (const auto& info : infos) {
#ifdef XP_MACOSX
          int64_t memory =
              nsMemoryReporterManager::PhysicalFootprint(info.mHandle);
#else
          int64_t memory =
              nsMemoryReporterManager::ResidentUnique(info.mHandle);
#endif
          if (memory > 0) {
            childSizes.AppendElement(memory);
            totalMemory += memory;
          } else {
            // We don't break out of the loop otherwise the cleanup code
            // wouldn't run.
            success = false;
          }

#if defined(XP_WIN)
          ::CloseHandle(info.mHandle);
#elif defined(XP_MACOSX)
          mach_port_deallocate(mach_task_self(), info.mHandle);
#endif
        }

        Maybe<int64_t> mbTotal;
        if (success) {
          mbTotal = Some(totalMemory);
        }

        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "MemoryTelemetry::FinishGatheringTotalMemory",
            [mbTotal, childSizes = std::move(childSizes)] {
              MemoryTelemetry::Get().FinishGatheringTotalMemory(mbTotal,
                                                                childSizes);
            }));
      }));
}

nsresult MemoryTelemetry::FinishGatheringTotalMemory(
    Maybe<int64_t> aTotalMemory, const nsTArray<int64_t>& aChildSizes) {
  mGatheringTotalMemory = false;

  // Total memory usage can be difficult to measure both accurately and fast
  // enough for telemetry (iterating memory maps can jank whole processes on
  // MacOS).  Therefore this shouldn't be relied on as an absolute measurement
  // especially on MacOS where it double-counts shared memory.  For a more
  // detailed explaination see:
  // https://groups.google.com/a/mozilla.org/g/dev-platform/c/WGNOtjHdsdA
  if (aTotalMemory) {
    glean::memory::total.Accumulate(aTotalMemory.value() / 1024);
  }

  if (aChildSizes.Length() > 1) {
    int32_t tabsCount;
    MOZ_TRY_VAR(tabsCount, GetOpenTabsCount());

    nsCString key;
    if (tabsCount <= 10) {
      key = "0 - 10 tabs";
    } else if (tabsCount <= 500) {
      key = "11 - 500 tabs";
    } else {
      key = "more tabs";
    }

    // Mean of the USS of all the content processes.
    int64_t mean = 0;
    for (auto size : aChildSizes) {
      mean += size;
    }
    mean /= aChildSizes.Length();

    // For some users, for unknown reasons (though most likely because they're
    // in a sandbox without procfs mounted), we wind up with 0 here, which
    // triggers a floating point exception if we try to calculate values using
    // it.
    if (!mean) {
      return NS_ERROR_UNEXPECTED;
    }

    // Absolute error of USS for each content process, normalized by the mean
    // (*100 to get it in percentage). 20% means for a content process that it
    // is using 20% more or 20% less than the mean.
    for (auto size : aChildSizes) {
      int64_t diff = llabs(size - mean) * 100 / mean;

      glean::memory::distribution_among_content.Get(key).AccumulateSingleSample(
          diff);
    }
  }

  // This notification is for testing only.
  if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
    obs->NotifyObservers(nullptr, "gather-memory-telemetry-finished", nullptr);
  }

  return NS_OK;
}

/* static */ Result<uint32_t, nsresult> MemoryTelemetry::GetOpenTabsCount() {
  nsresult rv;

  nsCOMPtr<nsIWindowMediator> windowMediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  MOZ_TRY(rv);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  MOZ_TRY(windowMediator->GetEnumerator(u"navigator:browser",
                                        getter_AddRefs(enumerator)));

  uint32_t total = 0;
  for (const auto& window : SimpleEnumerator<nsPIDOMWindowOuter>(enumerator)) {
    nsCOMPtr<nsIBrowserDOMWindow> browserWin =
        nsGlobalWindowOuter::Cast(window)->GetBrowserDOMWindow();

    NS_ENSURE_TRUE(browserWin, Err(NS_ERROR_UNEXPECTED));

    uint32_t tabCount;
    MOZ_TRY(browserWin->GetTabCount(&tabCount));
    total += tabCount;
  }

  return total;
}

nsresult MemoryTelemetry::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  if (strcmp(aTopic, kTopicShutdown) == 0) {
    if (nsCOMPtr<nsITelemetry> telemetry =
            do_GetService("@mozilla.org/base/telemetry;1")) {
      telemetry->FlushBatchedChildTelemetry();
    }
  }
  return NS_OK;
}
