/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaTimer_h_)
#  define MediaTimer_h_

#  include <queue>

#  include "mozilla/AbstractThread.h"
#  include "mozilla/AwakeTimeStamp.h"
#  include "mozilla/Monitor.h"
#  include "mozilla/MozPromise.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/SharedThreadPool.h"
#  include "mozilla/TimeStamp.h"
#  include "mozilla/Unused.h"
#  include "nsITimer.h"

namespace mozilla {

extern LazyLogModule gMediaTimerLog;

#  define TIMER_LOG(x, ...)                                    \
    MOZ_ASSERT(gMediaTimerLog);                                \
    MOZ_LOG(gMediaTimerLog, LogLevel::Debug,                   \
            ("[MediaTimer=%p relative_t=%" PRId64 "]" x, this, \
             RelativeMicroseconds(T::Now()), ##__VA_ARGS__))

// This promise type is only exclusive because so far there isn't a reason for
// it not to be. Feel free to change that.
using MediaTimerPromise = MozPromise<bool, bool, true>;

// Timers only know how to fire at a given thread, which creates an impedence
// mismatch with code that operates with TaskQueues. This class solves
// that mismatch with a dedicated (but shared) thread and a nice MozPromise-y
// interface.
template <typename T>
class MediaTimer {
 public:
  explicit MediaTimer(bool aFuzzy = false);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(MediaTimer,
                                                     DispatchDestroy());

  RefPtr<MediaTimerPromise> WaitFor(const typename T::DurationType& aDuration,
                                    StaticString aCallSite);

  RefPtr<MediaTimerPromise> WaitUntil(const T& aTimeStamp,
                                      StaticString aCallSite);

  // Cancel and reject any unresolved promises with false.
  void Cancel();

 private:
  virtual ~MediaTimer() { MOZ_ASSERT(OnMediaTimerThread()); }

  void DispatchDestroy();
  // Runs on the timer thread.
  void Destroy();
  bool OnMediaTimerThread();
  void ScheduleUpdate();
  void Update();
  void UpdateLocked();
  bool IsExpired(const T& aTarget, const T& aNow);
  void Reject();
  /*
   * We use a callback function, rather than a callback method, to ensure that
   * the nsITimer does not artifically keep the refcount of the MediaTimer above
   * zero. When the MediaTimer is destroyed, it safely cancels the nsITimer so
   * that we never fire against a dangling closure.
   */
  static void TimerCallback(nsITimer* aTimer, void* aClosure);
  void TimerFired();
  void ArmTimer(const T& aTarget, const T& aNow);

  bool TimerIsArmed();
  void CancelTimerIfArmed();

  struct Entry {
    T mTimeStamp;
    RefPtr<MediaTimerPromise::Private> mPromise;

    explicit Entry(const T& aTimeStamp, StaticString aCallSite)
        : mTimeStamp(aTimeStamp),
          mPromise(new MediaTimerPromise::Private(aCallSite)) {}

    // Define a < overload that reverses ordering because std::priority_queue
    // provides access to the largest element, and we want the smallest
    // (i.e. the soonest).
    bool operator<(const Entry& aOther) const {
      return mTimeStamp > aOther.mTimeStamp;
    }
  };

  nsCOMPtr<nsIEventTarget> mThread;
  std::priority_queue<Entry> mEntries;
  Monitor mMonitor MOZ_UNANNOTATED;
  nsCOMPtr<nsITimer> mTimer;
  Maybe<T> mCurrentTimerTarget;

  // Timestamps only have relative meaning, so we need a base timestamp for
  // logging purposes.
  T mCreationTimeStamp;
  int64_t RelativeMicroseconds(const T& aTimeStamp) {
    return (int64_t)(aTimeStamp - mCreationTimeStamp).ToMicroseconds();
  }

  bool mUpdateScheduled;
  const bool mFuzzy;
};

// Class for managing delayed dispatches on target thread.
template <typename T>
class DelayedScheduler {
 public:
  explicit DelayedScheduler(nsISerialEventTarget* aTargetThread,
                            bool aFuzzy = false)
      : mTargetThread(aTargetThread), mMediaTimer(new MediaTimer<T>(aFuzzy)) {
    MOZ_ASSERT(mTargetThread);
  }

  bool IsScheduled() const { return mTarget.isSome(); }

  void Reset() {
    MOZ_ASSERT(mTargetThread->IsOnCurrentThread(),
               "Must be on target thread to disconnect");
    mRequest.DisconnectIfExists();
    mTarget = Nothing();
  }

  template <typename ResolveFunc, typename RejectFunc>
  void Ensure(T& aTarget, ResolveFunc&& aResolver, RejectFunc&& aRejector) {
    MOZ_ASSERT(mTargetThread->IsOnCurrentThread());
    if (IsScheduled() && mTarget.value() <= aTarget) {
      return;
    }
    Reset();
    mTarget.emplace(aTarget);
    mMediaTimer->WaitUntil(mTarget.value(), __func__)
        ->Then(mTargetThread, __func__, std::forward<ResolveFunc>(aResolver),
               std::forward<RejectFunc>(aRejector))
        ->Track(mRequest);
  }

  void CompleteRequest() {
    MOZ_ASSERT(mTargetThread->IsOnCurrentThread());
    mRequest.Complete();
    mTarget = Nothing();
  }

 private:
  nsCOMPtr<nsISerialEventTarget> mTargetThread;
  RefPtr<MediaTimer<T>> mMediaTimer;
  Maybe<T> mTarget;
  MozPromiseRequestHolder<mozilla::MediaTimerPromise> mRequest;
};

using MediaTimerTimeStamp = MediaTimer<TimeStamp>;
using MediaTimerAwakeTimeStamp = MediaTimer<AwakeTimeStamp>;

}  // namespace mozilla

#endif
