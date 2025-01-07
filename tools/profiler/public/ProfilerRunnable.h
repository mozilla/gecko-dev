/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerRunnable_h
#define ProfilerRunnable_h

#include "GeckoProfiler.h"
#include "nsIThreadPool.h"

// Treat runnable profiling separately, as this can add considerable overhead
// and ETW allows disabling it explicitly.
static inline bool profiler_thread_is_profiling_runnables() {
  return profiler_thread_is_being_profiled(ThreadProfilingFeatures::Markers) ||
         (profiler_is_etw_collecting_markers() &&
          ETW::IsProfilingGroup(
              mozilla::MarkerSchema::ETWMarkerGroup::Scheduling)) ||
         profiler_is_perfetto_tracing();
}

#if !defined(MOZ_GECKO_PROFILER) || !defined(MOZ_COLLECTING_RUNNABLE_TELEMETRY)
#  define AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable)
#else
#  define AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable)                  \
    mozilla::Maybe<mozilla::AutoProfileRunnable> raiiRunnableMarker; \
    if (profiler_thread_is_profiling_runnables()) {                  \
      raiiRunnableMarker.emplace(runnable);                          \
    }

namespace mozilla {

struct RunnableMarker : BaseMarkerType<RunnableMarker> {
  static constexpr const char* Name = "Runnable";
  static constexpr const char* Description =
      "Marker representing a runnable being executed.";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"name", MS::InputType::CString, "Runnable Name", MS::Format::String,
       MS::PayloadFlags::Searchable},
      {"runnable", MS::InputType::Uint64, "Runnable",
       MS::Format::TerminatingFlow, MS::PayloadFlags::Searchable},
  };

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static constexpr const char* TableLabel =
      "{marker.name} - {marker.data.name}"
      " runnable: {marker.data.runnable}";

  static constexpr bool IsStackBased = true;

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Scheduling;

  static void TranslateMarkerInputToSchema(void* aContext,
                                           const nsCString& aName, Flow aFlow) {
    ETW::OutputMarkerSchema(aContext, RunnableMarker{}, aName, aFlow);
  }

  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const nsCString& aName, Flow aFlow) {
    aWriter.StringProperty("name", aName);
    aWriter.FlowProperty("runnable", aFlow);
  }
};

class MOZ_RAII AutoProfileRunnable {
 public:
  explicit AutoProfileRunnable(Runnable* aRunnable)
      : mStartTime(TimeStamp::Now()), mRunnable(Flow::FromPointer(aRunnable)) {
    aRunnable->GetName(mName);
  }
  explicit AutoProfileRunnable(nsIRunnable* aRunnable)
      : mStartTime(TimeStamp::Now()), mRunnable(Flow::FromPointer(aRunnable)) {
    nsCOMPtr<nsIThreadPool> threadPool = do_QueryInterface(aRunnable);
    if (threadPool) {
      // nsThreadPool::Run has its own call to AUTO_PROFILE_FOLLOWING_RUNNABLE,
      // avoid nesting runnable markers.
      return;
    }

    nsCOMPtr<nsINamed> named = do_QueryInterface(aRunnable);
    if (named) {
      named->GetName(mName);
    }
  }
  // XXX: we should remove this constructor so that we can track flows properly
  explicit AutoProfileRunnable(nsACString& aName)
      : mStartTime(TimeStamp::Now()),
        mName(aName),
        mRunnable(Flow::FromPointer(&aName)) {}

  ~AutoProfileRunnable() {
    if (mName.IsEmpty()) {
      return;
    }

    AUTO_PROFILER_LABEL("AutoProfileRunnable", PROFILER);
    AUTO_PROFILER_STATS(AUTO_PROFILE_RUNNABLE);
    profiler_add_marker("Runnable", ::mozilla::baseprofiler::category::OTHER,
                        MarkerTiming::IntervalUntilNowFrom(mStartTime),
                        RunnableMarker{}, mName, mRunnable);
  }

 protected:
  TimeStamp mStartTime;
  nsAutoCString mName;
  Flow mRunnable;
};

}  // namespace mozilla

#endif

#endif  // ProfilerRunnable_h
