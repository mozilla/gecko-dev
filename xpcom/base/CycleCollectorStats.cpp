/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "nsCycleCollector.h"
#include "nsDebug.h"
#include "CycleCollectorStats.h"
#include "MainThreadUtils.h"
#include "mozilla/BaseProfilerMarkersPrerequisites.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

using namespace mozilla;

mozilla::CycleCollectorStats::CycleCollectorStats() {
  char* env = getenv("MOZ_CCTIMER");
  if (!env) {
    return;
  }
  if (strcmp(env, "none") == 0) {
    mFile = nullptr;
  } else if (strcmp(env, "stdout") == 0) {
    mFile = stdout;
  } else if (strcmp(env, "stderr") == 0) {
    mFile = stderr;
  } else {
    mFile = fopen(env, "a");
    if (!mFile) {
      NS_WARNING("Failed to open MOZ_CCTIMER log file.");
    }
  }
}

void mozilla::CycleCollectorStats::Clear() {
  if (mFile && mFile != stdout && mFile != stderr) {
    fclose(mFile);
  }
  *this = CycleCollectorStats();
}

MOZ_ALWAYS_INLINE
static TimeDuration TimeBetween(TimeStamp aStart, TimeStamp aEnd) {
  MOZ_ASSERT(aEnd >= aStart);
  return aEnd - aStart;
}

static TimeDuration TimeUntilNow(TimeStamp start) {
  if (start.IsNull()) {
    return TimeDuration();
  }
  return TimeBetween(start, TimeStamp::Now());
}

namespace geckoprofiler::markers {
class CCSliceMarker : public BaseMarkerType<CCSliceMarker> {
 public:
  static constexpr const char* Name = "CCSlice";
  static constexpr const char* Description =
      "Information for an individual CC slice.";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"idle", MS::InputType::Boolean, "Idle", MS::Format::Integer}};

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable,
                                               MS::Location::TimelineMemory};
  static constexpr const char* AllLabels =
      "{marker.name} (idle={marker.data.idle})";

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Memory;

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      bool aIsDuringIdle) {
    StreamJSONMarkerDataImpl(aWriter, aIsDuringIdle);
  }
};
}  // namespace geckoprofiler::markers

void mozilla::CycleCollectorStats::AfterCycleCollectionSlice() {
  // The meaning of the telemetry is specific to the main thread. No worker
  // should be calling this method. (And workers currently do not have
  // incremental CC, so the profiler marker is not needed either.)
  MOZ_ASSERT(NS_IsMainThread());

  if (mBeginSliceTime.IsNull()) {
    // We already called this method from EndCycleCollectionCallback for this
    // slice.
    return;
  }

  mEndSliceTime = TimeStamp::Now();
  TimeDuration duration = mEndSliceTime - mBeginSliceTime;

  PROFILER_MARKER(
      "CCSlice", GCCC, MarkerTiming::Interval(mBeginSliceTime, mEndSliceTime),
      CCSliceMarker, !mIdleDeadline.IsNull() && mIdleDeadline >= mEndSliceTime);

  if (duration.ToSeconds()) {
    TimeDuration idleDuration;
    if (!mIdleDeadline.IsNull()) {
      if (mIdleDeadline < mEndSliceTime) {
        // This slice overflowed the idle period.
        if (mIdleDeadline > mBeginSliceTime) {
          idleDuration = mIdleDeadline - mBeginSliceTime;
        }
      } else {
        idleDuration = duration;
      }
    }

    uint32_t percent =
        uint32_t(idleDuration.ToSeconds() / duration.ToSeconds() * 100);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_SLICE_DURING_IDLE,
                          percent);
  }

  TimeDuration sliceTime = TimeBetween(mBeginSliceTime, mEndSliceTime);
  mMaxSliceTime = std::max(mMaxSliceTime, sliceTime);
  mMaxSliceTimeSinceClear = std::max(mMaxSliceTimeSinceClear, sliceTime);
  mTotalSliceTime += sliceTime;
  mBeginSliceTime = TimeStamp();
}

void mozilla::CycleCollectorStats::PrepareForCycleCollection(TimeStamp aNow) {
  mBeginTime = aNow;
  mSuspected = nsCycleCollector_suspectedCount();
}

void mozilla::CycleCollectorStats::AfterPrepareForCycleCollectionSlice(
    TimeStamp aDeadline, TimeStamp aBeginTime, TimeStamp aMaybeAfterGCTime) {
  mBeginSliceTime = aBeginTime;
  mIdleDeadline = aDeadline;

  if (!aMaybeAfterGCTime.IsNull()) {
    mAnyLockedOut = true;
    mMaxGCDuration = std::max(mMaxGCDuration, aMaybeAfterGCTime - aBeginTime);
  }
}

void mozilla::CycleCollectorStats::AfterSyncForgetSkippable(
    TimeStamp beginTime) {
  mMaxSkippableDuration =
      std::max(mMaxSkippableDuration, TimeUntilNow(beginTime));
  mRanSyncForgetSkippable = true;
}

void mozilla::CycleCollectorStats::AfterForgetSkippable(
    mozilla::TimeStamp aStartTime, mozilla::TimeStamp aEndTime,
    uint32_t aRemovedPurples, bool aInIdle) {
  mozilla::TimeDuration duration = aEndTime - aStartTime;
  if (!mMinForgetSkippableTime || mMinForgetSkippableTime > duration) {
    mMinForgetSkippableTime = duration;
  }
  if (!mMaxForgetSkippableTime || mMaxForgetSkippableTime < duration) {
    mMaxForgetSkippableTime = duration;
  }
  mTotalForgetSkippableTime += duration;
  ++mForgetSkippableBeforeCC;

  mRemovedPurples += aRemovedPurples;

  PROFILER_MARKER("ForgetSkippable", GCCC,
                  MarkerTiming::IntervalUntilNowFrom(aStartTime), CCSliceMarker,
                  aInIdle);
}

void mozilla::CycleCollectorStats::SendTelemetry(TimeDuration aCCNowDuration,
                                                 TimeStamp aPrevCCEnd) const {
  // Many of the telemetry measures would not make sense off the main thread (on
  // workers), and even for those that do, we don't want to mix mainthread and
  // other threads' measures.
  MOZ_ASSERT(NS_IsMainThread());

  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FINISH_IGC, mAnyLockedOut);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_SYNC_SKIPPABLE,
                        mRanSyncForgetSkippable);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FULL,
                        aCCNowDuration.ToMilliseconds());
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_MAX_PAUSE,
                        mMaxSliceTime.ToMilliseconds());

  if (!aPrevCCEnd.IsNull()) {
    TimeDuration timeBetween = TimeBetween(aPrevCCEnd, mBeginTime);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_TIME_BETWEEN,
                          timeBetween.ToSeconds());
  }

  Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_MAX,
                        mMaxForgetSkippableTime.ToMilliseconds());
}
