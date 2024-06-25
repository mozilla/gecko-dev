/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_BASE_CYCLECOLLECTORSTATS_H_
#define XPCOM_BASE_CYCLECOLLECTORSTATS_H_

#include <cstdint>
#include "mozilla/TimeStamp.h"

namespace mozilla {

struct CycleCollectorStats {
  // Return the statistics struct for the current cycle-collecting thread, which
  // will have initialized it during startup.
  static CycleCollectorStats* Get();

  CycleCollectorStats();
  void Clear();
  void PrepareForCycleCollection(TimeStamp aNow);
  void AfterPrepareForCycleCollectionSlice(TimeStamp aDeadline,
                                           TimeStamp aBeginTime,
                                           TimeStamp aMaybeAfterGCTime);
  void AfterCycleCollectionSlice();
  void AfterSyncForgetSkippable(TimeStamp beginTime);
  void AfterForgetSkippable(TimeStamp aStartTime, TimeStamp aEndTime,
                            uint32_t aRemovedPurples, bool aInIdle);
  void AfterCycleCollection();

  void SendTelemetry(TimeDuration aCCNowDuration, TimeStamp aPrevCCEnd) const;

  // Time the current slice began, including any GC finishing.
  TimeStamp mBeginSliceTime;

  // Time the previous slice of the current CC ended.
  TimeStamp mEndSliceTime;

  // Time the current cycle collection began.
  TimeStamp mBeginTime;

  // The longest GC finishing duration for any slice of the current CC.
  TimeDuration mMaxGCDuration;

  // True if we ran sync forget skippable in any slice of the current CC.
  bool mRanSyncForgetSkippable = false;

  // Number of suspected objects at the start of the current CC.
  uint32_t mSuspected = 0;

  // The longest duration spent on sync forget skippable in any slice of the
  // current CC.
  TimeDuration mMaxSkippableDuration;

  // The longest pause of any slice in the current CC.
  TimeDuration mMaxSliceTime;

  // The longest slice time since ClearMaxCCSliceTime() was called.
  TimeDuration mMaxSliceTimeSinceClear;

  // The total amount of time spent actually running the current CC.
  TimeDuration mTotalSliceTime;

  // True if we were locked out by the GC in any slice of the current CC.
  bool mAnyLockedOut = false;

  // A file to dump CC activity to; set by MOZ_CCTIMER environment variable.
  FILE* mFile = nullptr;

  // In case CC slice was triggered during idle time, set to the end of the idle
  // period.
  TimeStamp mIdleDeadline;

  TimeDuration mMinForgetSkippableTime;
  TimeDuration mMaxForgetSkippableTime;
  TimeDuration mTotalForgetSkippableTime;
  uint32_t mForgetSkippableBeforeCC = 0;

  uint32_t mRemovedPurples = 0;
};

}  // namespace mozilla

#endif  // XPCOM_BASE_CYCLECOLLECTORSTATS_H_
