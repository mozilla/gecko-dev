/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TimerThread_h___
#define TimerThread_h___

#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIThread.h"

#include "nsTimerImpl.h"
#include "nsThreadUtils.h"

#include "nsTArray.h"

#include "mozilla/Attributes.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Monitor.h"
#include "mozilla/ProfilerUtils.h"

// Enable this to compute lots of interesting statistics and print them out when
// PrintStatistics() is called.
#define TIMER_THREAD_STATISTICS 0

class TimerThread final : public mozilla::Runnable, public nsIObserver {
 public:
  typedef mozilla::Monitor Monitor;
  typedef mozilla::MutexAutoLock MutexAutoLock;
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  TimerThread();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsresult Shutdown();

  nsresult AddTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  nsresult RemoveTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  // Considering only the first 'aSearchBound' timers (in firing order), returns
  // the timeout of the first non-low-priority timer, on the current thread,
  // that will fire before 'aDefault'. If no such timer exists, 'aDefault' is
  // returned.
  TimeStamp FindNextFireTimeForCurrentThread(TimeStamp aDefault,
                                             uint32_t aSearchBound);

  void DoBeforeSleep();
  void DoAfterSleep();

  bool IsOnTimerThread() const { return mThread->IsOnCurrentThread(); }

  uint32_t AllowedEarlyFiringMicroseconds();
  nsresult GetTimers(nsTArray<RefPtr<nsITimer>>& aRetVal);

 private:
  ~TimerThread();

  bool mInitialized;

  // These internal helper methods must be called while mMonitor is held.
  void AddTimerInternal(nsTimerImpl& aTimer) MOZ_REQUIRES(mMonitor);
  bool RemoveTimerInternal(nsTimerImpl& aTimer)
      MOZ_REQUIRES(mMonitor, aTimer.mMutex);
  void RemoveLeadingCanceledTimersInternal() MOZ_REQUIRES(mMonitor);
  nsresult Init() MOZ_REQUIRES(mMonitor);
  void AssertTimersSortedAndUnique() MOZ_REQUIRES(mMonitor);

  // Using atomic because this value is written to in one place, and read from
  // in another, and those two locations are likely to be executed from separate
  // threads. Reads/writes to an aligned value this size should be atomic even
  // without using std::atomic, but doing this explicitly provides a good
  // reminder that this is accessed from multiple threads.
  std::atomic<mozilla::hal::ProcessPriority> mCachedPriority =
      mozilla::hal::PROCESS_PRIORITY_UNKNOWN;

  nsCOMPtr<nsIThread> mThread;
  // Lock ordering requirements:
  // (optional) ThreadWrapper::sMutex ->
  // (optional) nsTimerImpl::mMutex   ->
  // TimerThread::mMonitor
  Monitor mMonitor;

  bool mShutdown MOZ_GUARDED_BY(mMonitor);
  bool mWaiting MOZ_GUARDED_BY(mMonitor);
  bool mNotified MOZ_GUARDED_BY(mMonitor);
  bool mSleeping MOZ_GUARDED_BY(mMonitor);

  struct EntryKey {
    explicit EntryKey(nsTimerImpl& aTimerImpl)
        : mTimeout(aTimerImpl.mTimeout), mTimerSeq(aTimerImpl.mTimerSeq) {}

    // The comparison operators must ensure to detect equality only for
    // equal mTimerImpl except for canceled timers.
    // This is achieved through the sequence number.
    // Currently we maintain a FIFO order for timers with equal timeout.
    // Note that it might make sense to flip the sequence order to favor
    // timeouts with smaller delay as they are most likely more sensitive
    // to jitter. But we strictly test for FIFO order in our gtests.

    bool operator==(const EntryKey& aRhs) const {
      return (mTimeout == aRhs.mTimeout && mTimerSeq == aRhs.mTimerSeq);
    }

    bool operator<(const EntryKey& aRhs) const {
      if (mTimeout == aRhs.mTimeout) {
        return mTimerSeq < aRhs.mTimerSeq;
      }
      return mTimeout < aRhs.mTimeout;
    }

    TimeStamp mTimeout;
    uint64_t mTimerSeq;
  };

  struct Entry final : EntryKey {
    explicit Entry(nsTimerImpl& aTimerImpl)
        : EntryKey(aTimerImpl),
          mDelay(aTimerImpl.mDelay),
          mTimerImpl(&aTimerImpl) {}

    // No copies to not fiddle with mTimerImpl's ref-count.
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = default;
    Entry& operator=(Entry&&) = default;

#ifdef DEBUG
    // While the timer is stored in the thread's list, the timeout is
    // immutable, so it should be OK to read without holding the mutex.
    // We only allow this in debug builds.
    bool IsTimerInThreadAndUnchanged() MOZ_NO_THREAD_SAFETY_ANALYSIS {
      return (mTimerImpl && mTimerImpl->IsInTimerThread() &&
              mTimerImpl->mTimeout == mTimeout);
    }
#endif

    TimeDuration mDelay;
    RefPtr<nsTimerImpl> mTimerImpl;
  };

  void PostTimerEvent(Entry& aPostMe) MOZ_REQUIRES(mMonitor);

  // Computes and returns when we should next try to wake up in order to handle
  // the triggering of the timers in mTimers.
  // If mTimers is empty, returns a null TimeStamp. If mTimers is not empty,
  // returns the timeout of the last timer that can be bundled with the first
  // timer in mTimers.
  TimeStamp ComputeWakeupTimeFromTimers() const MOZ_REQUIRES(mMonitor);

  // Computes how late a timer can acceptably fire.
  // timerDuration is the duration of the timer whose delay we are calculating.
  // Longer timers can tolerate longer firing delays.
  // minDelay is an amount by which any timer can be delayed.
  // This function will never return a value smaller than minDelay (unless this
  // conflicts with maxDelay). maxDelay is the upper limit on the amount by
  // which we will ever delay any timer. Takes precedence over minDelay if there
  // is a conflict. (Zero will effectively disable timer coalescing.)
  TimeDuration ComputeAcceptableFiringDelay(TimeDuration timerDuration,
                                            TimeDuration minDelay,
                                            TimeDuration maxDelay) const;

  // Fires and removes all timers in mTimers that are "due" to be fired,
  // according to the current time and the passed-in early firing tolerance.
  // Return value is the number of timers that were fired by the operation.
  uint64_t FireDueTimers(TimeDuration aAllowedEarlyFiring)
      MOZ_REQUIRES(mMonitor);

  // Suspends thread execution using mMonitor.Wait(waitFor). Also sets and
  // clears a few flags before and after.
  void Wait(TimeDuration aWaitFor) MOZ_REQUIRES(mMonitor);

  // mTimers is sorted by timeout, followed by a unique sequence number.
  // Some entries are for cancelled entries, but remain in sorted order based
  // on the timeout and sequence number they were originally created with.
  nsTArray<Entry> mTimers MOZ_GUARDED_BY(mMonitor);

  // Set only at the start of the thread's Run():
  uint32_t mAllowedEarlyFiringMicroseconds MOZ_GUARDED_BY(mMonitor);

  ProfilerThreadId mProfilerThreadId MOZ_GUARDED_BY(mMonitor);

  // Time at which we were intending to wake up the last time that we slept.
  // Is "null" if we have never slept or if our last sleep was "forever".
  TimeStamp mIntendedWakeupTime;

#if TIMER_THREAD_STATISTICS
  static constexpr size_t sTimersFiredPerWakeupBucketCount = 16;
  static inline constexpr std::array<size_t, sTimersFiredPerWakeupBucketCount>
      sTimersFiredPerWakeupThresholds = {
          0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 20, 30, 40, 50, 70, (size_t)(-1)};

  mutable AutoTArray<size_t, sTimersFiredPerWakeupBucketCount>
      mTimersFiredPerWakeup MOZ_GUARDED_BY(mMonitor) = {0, 0, 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};
  mutable AutoTArray<size_t, sTimersFiredPerWakeupBucketCount>
      mTimersFiredPerUnnotifiedWakeup MOZ_GUARDED_BY(mMonitor) = {
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  mutable AutoTArray<size_t, sTimersFiredPerWakeupBucketCount>
      mTimersFiredPerNotifiedWakeup MOZ_GUARDED_BY(mMonitor) = {
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  mutable size_t mTotalTimersAdded MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalTimersRemoved MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalTimersFiredNotified MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalTimersFiredUnnotified MOZ_GUARDED_BY(mMonitor) = 0;

  mutable size_t mTotalWakeupCount MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalUnnotifiedWakeupCount MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalNotifiedWakeupCount MOZ_GUARDED_BY(mMonitor) = 0;

  mutable double mTotalActualTimerFiringDelayNotified MOZ_GUARDED_BY(mMonitor) =
      0.0;
  mutable double mTotalActualTimerFiringDelayUnnotified
      MOZ_GUARDED_BY(mMonitor) = 0.0;

  mutable TimeStamp mFirstTimerAdded MOZ_GUARDED_BY(mMonitor);

  mutable size_t mEarlyWakeups MOZ_GUARDED_BY(mMonitor) = 0;
  mutable double mTotalEarlyWakeupTime MOZ_GUARDED_BY(mMonitor) = 0.0;

  void CollectTimersFiredStatistics(uint64_t timersFiredThisWakeup);

  void CollectWakeupStatistics();

  void PrintStatistics() const;
#endif
};
#endif /* TimerThread_h___ */
