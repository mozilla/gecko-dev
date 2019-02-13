/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 *
 * Chrome and each tab have their own RefreshDriver, which in turn
 * hooks into one of a few global timer based on RefreshDriverTimer,
 * defined below.  There are two main global timers -- one for active
 * animations, and one for inactive ones.  These are implemented as
 * subclasses of RefreshDriverTimer; see below for a description of
 * their implementations.  In the future, additional timer types may
 * implement things like blocking on vsync.
 */

#ifdef XP_WIN
#include <windows.h>
// mmsystem isn't part of WIN32_LEAN_AND_MEAN, so we have
// to manually include it
#include <mmsystem.h>
#include "WinUtils.h"
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsRefreshDriver.h"
#include "nsITimer.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Logging.h"
#include "nsAutoPtr.h"
#include "nsIDocument.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/Preferences.h"
#include "nsViewManager.h"
#include "GeckoProfiler.h"
#include "nsNPAPIPluginInstance.h"
#include "nsPerformance.h"
#include "mozilla/dom/WindowBinding.h"
#include "RestyleManager.h"
#include "Layers.h"
#include "imgIContainer.h"
#include "nsIFrameRequestCallback.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsDocShell.h"
#include "nsISimpleEnumerator.h"
#include "nsJSEnvironment.h"
#include "mozilla/Telemetry.h"
#include "gfxPrefs.h"
#include "BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "mozilla/layout/VsyncChild.h"
#include "VsyncSource.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsThreadUtils.h"
#include "mozilla/unused.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

using namespace mozilla;
using namespace mozilla::widget;
using namespace mozilla::ipc;
using namespace mozilla::layout;

static PRLogModuleInfo *gLog = nullptr;
#define LOG(...) MOZ_LOG(gLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

#define DEFAULT_FRAME_RATE 60
#define DEFAULT_THROTTLED_FRAME_RATE 1
#define DEFAULT_RECOMPUTE_VISIBILITY_INTERVAL_MS 1000
// after 10 minutes, stop firing off inactive timers
#define DEFAULT_INACTIVE_TIMER_DISABLE_SECONDS 600

namespace mozilla {

/*
 * The base class for all global refresh driver timers.  It takes care
 * of managing the list of refresh drivers attached to them and
 * provides interfaces for querying/setting the rate and actually
 * running a timer 'Tick'.  Subclasses must implement StartTimer(),
 * StopTimer(), and ScheduleNextTick() -- the first two just
 * start/stop whatever timer mechanism is in use, and ScheduleNextTick
 * is called at the start of the Tick() implementation to set a time
 * for the next tick.
 */
class RefreshDriverTimer {
public:
  RefreshDriverTimer()
    : mLastFireEpoch(0)
  {
  }

  virtual ~RefreshDriverTimer()
  {
    NS_ASSERTION(mRefreshDrivers.Length() == 0, "Should have removed all refresh drivers from here by now!");
  }

  virtual void AddRefreshDriver(nsRefreshDriver* aDriver)
  {
    LOG("[%p] AddRefreshDriver %p", this, aDriver);

    NS_ASSERTION(!mRefreshDrivers.Contains(aDriver), "AddRefreshDriver for a refresh driver that's already in the list!");
    mRefreshDrivers.AppendElement(aDriver);

    if (mRefreshDrivers.Length() == 1) {
      StartTimer();
    }
  }

  virtual void RemoveRefreshDriver(nsRefreshDriver* aDriver)
  {
    LOG("[%p] RemoveRefreshDriver %p", this, aDriver);

    NS_ASSERTION(mRefreshDrivers.Contains(aDriver), "RemoveRefreshDriver for a refresh driver that's not in the list!");
    mRefreshDrivers.RemoveElement(aDriver);

    if (mRefreshDrivers.Length() == 0) {
      StopTimer();
    }
  }

  TimeStamp MostRecentRefresh() const { return mLastFireTime; }
  int64_t MostRecentRefreshEpochTime() const { return mLastFireEpoch; }

  void SwapRefreshDrivers(RefreshDriverTimer* aNewTimer)
  {
    MOZ_ASSERT(NS_IsMainThread());

    for (nsRefreshDriver* driver : mRefreshDrivers) {
      aNewTimer->AddRefreshDriver(driver);
      driver->mActiveTimer = aNewTimer;
    }
    mRefreshDrivers.Clear();

    aNewTimer->mLastFireEpoch = mLastFireEpoch;
    aNewTimer->mLastFireTime = mLastFireTime;
  }

protected:
  virtual void StartTimer() = 0;
  virtual void StopTimer() = 0;
  virtual void ScheduleNextTick(TimeStamp aNowTime) = 0;

  /*
   * Actually runs a tick, poking all the attached RefreshDrivers.
   * Grabs the "now" time via JS_Now and TimeStamp::Now().
   */
  void Tick()
  {
    int64_t jsnow = JS_Now();
    TimeStamp now = TimeStamp::Now();
    Tick(jsnow, now);
  }

  /*
   * Tick the refresh drivers based on the given timestamp.
   */
  void Tick(int64_t jsnow, TimeStamp now)
  {
    ScheduleNextTick(now);

    mLastFireEpoch = jsnow;
    mLastFireTime = now;

    LOG("[%p] ticking drivers...", this);
    nsTArray<nsRefPtr<nsRefreshDriver> > drivers(mRefreshDrivers);
    // RD is short for RefreshDriver
    profiler_tracing("Paint", "RD", TRACING_INTERVAL_START);
    for (nsRefreshDriver* driver : drivers) {
      // don't poke this driver if it's in test mode
      if (driver->IsTestControllingRefreshesEnabled()) {
        continue;
      }

      TickDriver(driver, jsnow, now);
    }
    profiler_tracing("Paint", "RD", TRACING_INTERVAL_END);
    LOG("[%p] done.", this);
  }

  static void TickDriver(nsRefreshDriver* driver, int64_t jsnow, TimeStamp now)
  {
    LOG(">> TickDriver: %p (jsnow: %lld)", driver, jsnow);
    driver->Tick(jsnow, now);
  }

  int64_t mLastFireEpoch;
  TimeStamp mLastFireTime;
  TimeStamp mTargetTime;

  nsTArray<nsRefPtr<nsRefreshDriver> > mRefreshDrivers;

  // useful callback for nsITimer-based derived classes, here
  // bacause of c++ protected shenanigans
  static void TimerTick(nsITimer* aTimer, void* aClosure)
  {
    RefreshDriverTimer *timer = static_cast<RefreshDriverTimer*>(aClosure);
    timer->Tick();
  }
};

/*
 * A RefreshDriverTimer that uses a nsITimer as the underlying timer.  Note that
 * this is a ONE_SHOT timer, not a repeating one!  Subclasses are expected to
 * implement ScheduleNextTick and intelligently calculate the next time to tick,
 * and to reset mTimer.  Using a repeating nsITimer gets us into a lot of pain
 * with its attempt at intelligent slack removal and such, so we don't do it.
 */
class SimpleTimerBasedRefreshDriverTimer :
    public RefreshDriverTimer
{
public:
  /*
   * aRate -- the delay, in milliseconds, requested between timer firings
   */
  explicit SimpleTimerBasedRefreshDriverTimer(double aRate)
  {
    SetRate(aRate);
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  }

  virtual ~SimpleTimerBasedRefreshDriverTimer()
  {
    StopTimer();
  }

  // will take effect at next timer tick
  virtual void SetRate(double aNewRate)
  {
    mRateMilliseconds = aNewRate;
    mRateDuration = TimeDuration::FromMilliseconds(mRateMilliseconds);
  }

  double GetRate() const
  {
    return mRateMilliseconds;
  }

protected:

  virtual void StartTimer()
  {
    // pretend we just fired, and we schedule the next tick normally
    mLastFireEpoch = JS_Now();
    mLastFireTime = TimeStamp::Now();

    mTargetTime = mLastFireTime + mRateDuration;

    uint32_t delay = static_cast<uint32_t>(mRateMilliseconds);
    mTimer->InitWithFuncCallback(TimerTick, this, delay, nsITimer::TYPE_ONE_SHOT);
  }

  virtual void StopTimer()
  {
    mTimer->Cancel();
  }

  double mRateMilliseconds;
  TimeDuration mRateDuration;
  nsRefPtr<nsITimer> mTimer;
};

/*
 * A refresh driver that listens to vsync events and ticks the refresh driver
 * on vsync intervals. We throttle the refresh driver if we get too many
 * vsync events and wait to catch up again.
 */
class VsyncRefreshDriverTimer : public RefreshDriverTimer
{
public:
  VsyncRefreshDriverTimer()
    : mVsyncChild(nullptr)
  {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    mVsyncObserver = new RefreshDriverVsyncObserver(this);
    nsRefPtr<mozilla::gfx::VsyncSource> vsyncSource = gfxPlatform::GetPlatform()->GetHardwareVsync();
    MOZ_ALWAYS_TRUE(mVsyncDispatcher = vsyncSource->GetRefreshTimerVsyncDispatcher());
    mVsyncDispatcher->SetParentRefreshTimer(mVsyncObserver);
  }

  explicit VsyncRefreshDriverTimer(VsyncChild* aVsyncChild)
    : mVsyncChild(aVsyncChild)
  {
    MOZ_ASSERT(!XRE_IsParentProcess());
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mVsyncChild);
    mVsyncObserver = new RefreshDriverVsyncObserver(this);
    mVsyncChild->SetVsyncObserver(mVsyncObserver);
  }

private:
  // Since VsyncObservers are refCounted, but the RefreshDriverTimer are
  // explicitly shutdown. We create an inner class that has the VsyncObserver
  // and is shutdown when the RefreshDriverTimer is deleted. The alternative is
  // to (a) make all RefreshDriverTimer RefCounted or (b) use different
  // VsyncObserver types.
  class RefreshDriverVsyncObserver final : public VsyncObserver
  {
  public:
    explicit RefreshDriverVsyncObserver(VsyncRefreshDriverTimer* aVsyncRefreshDriverTimer)
      : mVsyncRefreshDriverTimer(aVsyncRefreshDriverTimer)
      , mRefreshTickLock("RefreshTickLock")
      , mProcessedVsync(true)
    {
      MOZ_ASSERT(NS_IsMainThread());
    }

    virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) override
    {
      if (!NS_IsMainThread()) {
        MOZ_ASSERT(XRE_IsParentProcess());
        // Compress vsync notifications such that only 1 may run at a time
        // This is so that we don't flood the refresh driver with vsync messages
        // if the main thread is blocked for long periods of time
        { // scope lock
          MonitorAutoLock lock(mRefreshTickLock);
          mRecentVsync = aVsyncTimestamp;
          if (!mProcessedVsync) {
            return true;
          }
          mProcessedVsync = false;
        }

        nsCOMPtr<nsIRunnable> vsyncEvent =
             NS_NewRunnableMethodWithArg<TimeStamp>(this,
                                                    &RefreshDriverVsyncObserver::TickRefreshDriver,
                                                    aVsyncTimestamp);
        NS_DispatchToMainThread(vsyncEvent);
      } else {
        TickRefreshDriver(aVsyncTimestamp);
      }

      return true;
    }

    void Shutdown()
    {
      MOZ_ASSERT(NS_IsMainThread());
      mVsyncRefreshDriverTimer = nullptr;
    }

  private:
    virtual ~RefreshDriverVsyncObserver() {}

    void TickRefreshDriver(TimeStamp aVsyncTimestamp)
    {
      MOZ_ASSERT(NS_IsMainThread());

      if (XRE_IsParentProcess()) {
        MonitorAutoLock lock(mRefreshTickLock);
        aVsyncTimestamp = mRecentVsync;
        mProcessedVsync = true;
      }
      MOZ_ASSERT(aVsyncTimestamp <= TimeStamp::Now());

      // We might have a problem that we call ~VsyncRefreshDriverTimer() before
      // the scheduled TickRefreshDriver() runs. Check mVsyncRefreshDriverTimer
      // before use.
      if (mVsyncRefreshDriverTimer) {
        mVsyncRefreshDriverTimer->RunRefreshDrivers(aVsyncTimestamp);
      }
    }

    // VsyncRefreshDriverTimer holds this RefreshDriverVsyncObserver and it will
    // be always available before Shutdown(). We can just use the raw pointer
    // here.
    VsyncRefreshDriverTimer* mVsyncRefreshDriverTimer;
    Monitor mRefreshTickLock;
    TimeStamp mRecentVsync;
    bool mProcessedVsync;
  }; // RefreshDriverVsyncObserver

  virtual ~VsyncRefreshDriverTimer()
  {
    if (XRE_IsParentProcess()) {
      mVsyncDispatcher->SetParentRefreshTimer(nullptr);
      mVsyncDispatcher = nullptr;
    } else {
      // Since the PVsyncChild actors live through the life of the process, just
      // send the unobserveVsync message to disable vsync event. We don't need
      // to handle the cleanup stuff of this actor. PVsyncChild::ActorDestroy()
      // will be called and clean up this actor.
      unused << mVsyncChild->SendUnobserve();
      mVsyncChild->SetVsyncObserver(nullptr);
      mVsyncChild = nullptr;
    }

    // Detach current vsync timer from this VsyncObserver. The observer will no
    // longer tick this timer.
    mVsyncObserver->Shutdown();
    mVsyncObserver = nullptr;
  }

  virtual void StartTimer() override
  {
    mLastFireEpoch = JS_Now();
    mLastFireTime = TimeStamp::Now();

    if (XRE_IsParentProcess()) {
      mVsyncDispatcher->SetParentRefreshTimer(mVsyncObserver);
    } else {
      unused << mVsyncChild->SendObserve();
    }
  }

  virtual void StopTimer() override
  {
    if (XRE_IsParentProcess()) {
      mVsyncDispatcher->SetParentRefreshTimer(nullptr);
    } else {
      unused << mVsyncChild->SendUnobserve();
    }
  }

  virtual void ScheduleNextTick(TimeStamp aNowTime) override
  {
    // Do nothing since we just wait for the next vsync from
    // RefreshDriverVsyncObserver.
  }

  void RunRefreshDrivers(TimeStamp aTimeStamp)
  {
    int64_t jsnow = JS_Now();
    TimeDuration diff = TimeStamp::Now() - aTimeStamp;
    int64_t vsyncJsNow = jsnow - diff.ToMicroseconds();
    Tick(vsyncJsNow, aTimeStamp);
  }

  nsRefPtr<RefreshDriverVsyncObserver> mVsyncObserver;
  // Used for parent process.
  nsRefPtr<RefreshTimerVsyncDispatcher> mVsyncDispatcher;
  // Used for child process.
  // The mVsyncChild will be always available before VsncChild::ActorDestroy().
  // After ActorDestroy(), StartTimer() and StopTimer() calls will be non-op.
  nsRefPtr<VsyncChild> mVsyncChild;
}; // VsyncRefreshDriverTimer

/*
 * PreciseRefreshDriverTimer schedules ticks based on the current time
 * and when the next tick -should- be sent if we were hitting our
 * rate.  It always schedules ticks on multiples of aRate -- meaning that
 * if some execution takes longer than an alloted slot, the next tick
 * will be delayed instead of triggering instantly.  This might not be
 * desired -- there's an #if 0'd block below that we could put behind
 * a pref to control this behaviour.
 */
class PreciseRefreshDriverTimer :
    public SimpleTimerBasedRefreshDriverTimer
{
public:
  explicit PreciseRefreshDriverTimer(double aRate)
    : SimpleTimerBasedRefreshDriverTimer(aRate)
  {
  }

protected:
  virtual void ScheduleNextTick(TimeStamp aNowTime)
  {
    // The number of (whole) elapsed intervals between the last target
    // time and the actual time.  We want to truncate the double down
    // to an int number of intervals.
    int numElapsedIntervals = static_cast<int>((aNowTime - mTargetTime) / mRateDuration);

    if (numElapsedIntervals < 0) {
      // It's possible that numElapsedIntervals is negative (e.g. timer compensation
      // may result in (aNowTime - mTargetTime) < -1.0/mRateDuration, which will result in
      // negative numElapsedIntervals), so make sure we don't target the same timestamp.
      numElapsedIntervals = 0;
    }

    // the last "tick" that may or may not have been actually sent was
    // at this time.  For example, if the rate is 15ms, the target
    // time is 200ms, and it's now 225ms, the last effective tick
    // would have been at 215ms.  The next one should then be
    // scheduled for 5 ms from now.
    //
    // We then add another mRateDuration to find the next tick target.
    TimeStamp newTarget = mTargetTime + mRateDuration * (numElapsedIntervals + 1);

    // the amount of (integer) ms until the next time we should tick
    uint32_t delay = static_cast<uint32_t>((newTarget - aNowTime).ToMilliseconds());

    // Without this block, we'll always schedule on interval ticks;
    // with it, we'll schedule immediately if we missed our tick target
    // last time.
#if 0
    if (numElapsedIntervals > 0) {
      // we're late, so reset
      newTarget = aNowTime;
      delay = 0;
    }
#endif

    // log info & lateness
    LOG("[%p] precise timer last tick late by %f ms, next tick in %d ms",
        this,
        (aNowTime - mTargetTime).ToMilliseconds(),
        delay);
#ifndef ANDROID  /* bug 1142079 */
    Telemetry::Accumulate(Telemetry::FX_REFRESH_DRIVER_FRAME_DELAY_MS, (aNowTime - mTargetTime).ToMilliseconds());
#endif

    // then schedule the timer
    LOG("[%p] scheduling callback for %d ms (2)", this, delay);
    mTimer->InitWithFuncCallback(TimerTick, this, delay, nsITimer::TYPE_ONE_SHOT);

    mTargetTime = newTarget;
  }
};

#ifdef XP_WIN
/*
 * Uses vsync timing on windows with DWM. Falls back dynamically to fixed rate if required.
 */
class PreciseRefreshDriverTimerWindowsDwmVsync :
  public PreciseRefreshDriverTimer
{
public:
  // Checks if the vsync API is accessible.
  static bool IsSupported()
  {
    return WinUtils::dwmGetCompositionTimingInfoPtr != nullptr;
  }

  PreciseRefreshDriverTimerWindowsDwmVsync(double aRate, bool aPreferHwTiming = false)
    : PreciseRefreshDriverTimer(aRate)
    , mPreferHwTiming(aPreferHwTiming)
  {
  }

protected:
  // Indicates we should try to adjust to the HW's timing (get rate from the OS or use vsync)
  // This is typically true if the default refresh-rate value was not modified by the user.
  bool mPreferHwTiming;

  nsresult GetVBlankInfo(mozilla::TimeStamp &aLastVBlank, mozilla::TimeDuration &aInterval)
  {
    MOZ_ASSERT(WinUtils::dwmGetCompositionTimingInfoPtr,
               "DwmGetCompositionTimingInfoPtr is unavailable (windows vsync)");

    DWM_TIMING_INFO timingInfo;
    timingInfo.cbSize = sizeof(DWM_TIMING_INFO);
    HRESULT hr = WinUtils::dwmGetCompositionTimingInfoPtr(0, &timingInfo); // For the desktop window instead of a specific one.

    if (FAILED(hr)) {
      // This happens first time this is called.
      return NS_ERROR_NOT_INITIALIZED;
    }

    LARGE_INTEGER time, freq;
    ::QueryPerformanceCounter(&time);
    ::QueryPerformanceFrequency(&freq);
    aLastVBlank = TimeStamp::Now();
    double secondsPassed = double(time.QuadPart - timingInfo.qpcVBlank) / double(freq.QuadPart);

    aLastVBlank -= TimeDuration::FromSeconds(secondsPassed);
    aInterval = TimeDuration::FromSeconds(double(timingInfo.qpcRefreshPeriod) / double(freq.QuadPart));

    return NS_OK;
  }

  virtual void ScheduleNextTick(TimeStamp aNowTime)
  {
    static const TimeDuration kMinSaneInterval = TimeDuration::FromMilliseconds(3); // 330Hz
    static const TimeDuration kMaxSaneInterval = TimeDuration::FromMilliseconds(44); // 23Hz
    static const TimeDuration kNegativeMaxSaneInterval = TimeDuration::FromMilliseconds(-44); // Saves conversions for abs interval
    TimeStamp lastVblank;
    TimeDuration vblankInterval;

    if (!mPreferHwTiming ||
        NS_OK != GetVBlankInfo(lastVblank, vblankInterval) ||
        vblankInterval > kMaxSaneInterval ||
        vblankInterval < kMinSaneInterval ||
        (aNowTime - lastVblank) > kMaxSaneInterval ||
        (aNowTime - lastVblank) < kNegativeMaxSaneInterval) {
      // Use the default timing without vsync
      PreciseRefreshDriverTimer::ScheduleNextTick(aNowTime);
      return;
    }

    TimeStamp newTarget = lastVblank + vblankInterval; // Base target

    // However, timer callback might return early (or late, but that wouldn't bother us), and vblankInterval
    // appears to be slightly (~1%) different on each call (probably the OS measuring recent actual interval[s])
    // and since we don't want to re-target the same vsync, we keep advancing in vblank intervals until we find the
    // next safe target (next vsync, but not within 10% interval of previous target).
    // This is typically 0 or 1 iteration:
    // If we're too early, next vsync would be the one we've already targeted (1 iteration).
    // If the timer returned late, no iteration will be required.

    const double kSameVsyncThreshold = 0.1;
    while (newTarget <= mTargetTime + vblankInterval.MultDouble(kSameVsyncThreshold)) {
      newTarget += vblankInterval;
    }

    // To make sure we always hit the same "side" of the signal:
    // round the delay up (by adding 1, since we later floor) and add a little (10% by default).
    // Note that newTarget doesn't change (and is the next vblank) as a reference when we're back.
    static const double kDefaultPhaseShiftPercent = 10;
    static const double phaseShiftFactor = 0.01 *
      (Preferences::GetInt("layout.frame_rate.vsync.phasePercentage", kDefaultPhaseShiftPercent) % 100);

    double phaseDelay = 1.0 + vblankInterval.ToMilliseconds() * phaseShiftFactor;

    // ms until the next time we should tick
    double delayMs = (newTarget - aNowTime).ToMilliseconds() + phaseDelay;

    // Make sure the delay is never negative.
    uint32_t delay = static_cast<uint32_t>(delayMs < 0 ? 0 : delayMs);

    // log info & lateness
    LOG("[%p] precise dwm-vsync timer last tick late by %f ms, next tick in %d ms",
        this,
        (aNowTime - mTargetTime).ToMilliseconds(),
        delay);
#ifndef ANDROID  /* bug 1142079 */
    Telemetry::Accumulate(Telemetry::FX_REFRESH_DRIVER_FRAME_DELAY_MS, (aNowTime - mTargetTime).ToMilliseconds());
#endif

    // then schedule the timer
    LOG("[%p] scheduling callback for %d ms (2)", this, delay);
    mTimer->InitWithFuncCallback(TimerTick, this, delay, nsITimer::TYPE_ONE_SHOT);

    mTargetTime = newTarget;
  }
};
#endif

/*
 * A RefreshDriverTimer for inactive documents.  When a new refresh driver is
 * added, the rate is reset to the base (normally 1s/1fps).  Every time
 * it ticks, a single refresh driver is poked.  Once they have all been poked,
 * the duration between ticks doubles, up to mDisableAfterMilliseconds.  At that point,
 * the timer is quiet and doesn't tick (until something is added to it again).
 *
 * When a timer is removed, there is a possibility of another timer
 * being skipped for one cycle.  We could avoid this by adjusting
 * mNextDriverIndex in RemoveRefreshDriver, but there's little need to
 * add that complexity.  All we want is for inactive drivers to tick
 * at some point, but we don't care too much about how often.
 */
class InactiveRefreshDriverTimer final :
    public SimpleTimerBasedRefreshDriverTimer
{
public:
  explicit InactiveRefreshDriverTimer(double aRate)
    : SimpleTimerBasedRefreshDriverTimer(aRate),
      mNextTickDuration(aRate),
      mDisableAfterMilliseconds(-1.0),
      mNextDriverIndex(0)
  {
  }

  InactiveRefreshDriverTimer(double aRate, double aDisableAfterMilliseconds)
    : SimpleTimerBasedRefreshDriverTimer(aRate),
      mNextTickDuration(aRate),
      mDisableAfterMilliseconds(aDisableAfterMilliseconds),
      mNextDriverIndex(0)
  {
  }

  virtual void AddRefreshDriver(nsRefreshDriver* aDriver)
  {
    RefreshDriverTimer::AddRefreshDriver(aDriver);

    LOG("[%p] inactive timer got new refresh driver %p, resetting rate",
        this, aDriver);

    // reset the timer, and start with the newly added one next time.
    mNextTickDuration = mRateMilliseconds;

    // we don't really have to start with the newly added one, but we may as well
    // not tick the old ones at the fastest rate any more than we need to.
    mNextDriverIndex = mRefreshDrivers.Length() - 1;

    StopTimer();
    StartTimer();
  }

protected:
  virtual void StartTimer()
  {
    mLastFireEpoch = JS_Now();
    mLastFireTime = TimeStamp::Now();

    mTargetTime = mLastFireTime + mRateDuration;

    uint32_t delay = static_cast<uint32_t>(mRateMilliseconds);
    mTimer->InitWithFuncCallback(TimerTickOne, this, delay, nsITimer::TYPE_ONE_SHOT);
  }

  virtual void StopTimer()
  {
    mTimer->Cancel();
  }

  virtual void ScheduleNextTick(TimeStamp aNowTime)
  {
    if (mDisableAfterMilliseconds > 0.0 &&
        mNextTickDuration > mDisableAfterMilliseconds)
    {
      // We hit the time after which we should disable
      // inactive window refreshes; don't schedule anything
      // until we get kicked by an AddRefreshDriver call.
      return;
    }

    // double the next tick time if we've already gone through all of them once
    if (mNextDriverIndex >= mRefreshDrivers.Length()) {
      mNextTickDuration *= 2.0;
      mNextDriverIndex = 0;
    }

    // this doesn't need to be precise; do a simple schedule
    uint32_t delay = static_cast<uint32_t>(mNextTickDuration);
    mTimer->InitWithFuncCallback(TimerTickOne, this, delay, nsITimer::TYPE_ONE_SHOT);

    LOG("[%p] inactive timer next tick in %f ms [index %d/%d]", this, mNextTickDuration,
        mNextDriverIndex, mRefreshDrivers.Length());
  }

  /* Runs just one driver's tick. */
  void TickOne()
  {
    int64_t jsnow = JS_Now();
    TimeStamp now = TimeStamp::Now();

    ScheduleNextTick(now);

    mLastFireEpoch = jsnow;
    mLastFireTime = now;

    nsTArray<nsRefPtr<nsRefreshDriver> > drivers(mRefreshDrivers);
    if (mNextDriverIndex < drivers.Length() &&
        !drivers[mNextDriverIndex]->IsTestControllingRefreshesEnabled())
    {
      TickDriver(drivers[mNextDriverIndex], jsnow, now);
    }

    mNextDriverIndex++;
  }

  static void TimerTickOne(nsITimer* aTimer, void* aClosure)
  {
    InactiveRefreshDriverTimer *timer = static_cast<InactiveRefreshDriverTimer*>(aClosure);
    timer->TickOne();
  }

  double mNextTickDuration;
  double mDisableAfterMilliseconds;
  uint32_t mNextDriverIndex;
};

// The PBackground protocol connection callback. It will be called when
// PBackground is ready. Then we create the PVsync sub-protocol for our
// vsync-base RefreshTimer.
class VsyncChildCreateCallback final : public nsIIPCBackgroundChildCreateCallback
{
  NS_DECL_ISUPPORTS

public:
  VsyncChildCreateCallback()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  static void CreateVsyncActor(PBackgroundChild* aPBackgroundChild)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aPBackgroundChild);

    layout::PVsyncChild* actor = aPBackgroundChild->SendPVsyncConstructor();
    layout::VsyncChild* child = static_cast<layout::VsyncChild*>(actor);
    nsRefreshDriver::PVsyncActorCreated(child);
  }

private:
  virtual ~VsyncChildCreateCallback() {}

  virtual void ActorCreated(PBackgroundChild* aPBackgroundChild) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aPBackgroundChild);
    CreateVsyncActor(aPBackgroundChild);
  }

  virtual void ActorFailed() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_CRASH("Failed To Create VsyncChild Actor");
  }
}; // VsyncChildCreateCallback
NS_IMPL_ISUPPORTS(VsyncChildCreateCallback, nsIIPCBackgroundChildCreateCallback)

} // namespace mozilla

static RefreshDriverTimer* sRegularRateTimer;
static InactiveRefreshDriverTimer* sThrottledRateTimer;

#ifdef XP_WIN
static int32_t sHighPrecisionTimerRequests = 0;
// a bare pointer to avoid introducing a static constructor
static nsITimer *sDisableHighPrecisionTimersTimer = nullptr;
#endif

static void
CreateContentVsyncRefreshTimer(void*)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!XRE_IsParentProcess());

  // Create the PVsync actor child for vsync-base refresh timer.
  // PBackgroundChild is created asynchronously. If PBackgroundChild is still
  // unavailable, setup VsyncChildCreateCallback callback to handle the async
  // connect. We will still use software timer before PVsync ready, and change
  // to use hw timer when the connection is done. Please check
  // VsyncChildCreateCallback::CreateVsyncActor() and
  // nsRefreshDriver::PVsyncActorCreated().
  PBackgroundChild* backgroundChild = BackgroundChild::GetForCurrentThread();
  if (backgroundChild) {
    // If we already have PBackgroundChild, create the
    // child VsyncRefreshDriverTimer here.
    VsyncChildCreateCallback::CreateVsyncActor(backgroundChild);
    return;
  }
  // Setup VsyncChildCreateCallback callback
  nsRefPtr<nsIIPCBackgroundChildCreateCallback> callback = new VsyncChildCreateCallback();
  if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread(callback))) {
    MOZ_CRASH("PVsync actor create failed!");
  }
}

static void
CreateVsyncRefreshTimer()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Sometimes, gfxPrefs is not initialized here. Make sure the gfxPrefs is
  // ready.
  gfxPrefs::GetSingleton();

  if (!gfxPrefs::VsyncAlignedRefreshDriver()
        || !gfxPrefs::HardwareVsyncEnabled()
        || gfxPlatform::IsInLayoutAsapMode()) {
    return;
  }

  NS_WARNING("Enabling vsync refresh driver");

  if (XRE_IsParentProcess()) {
    // Make sure all vsync systems are ready.
    gfxPlatform::GetPlatform();
    // In parent process, we don't need to use ipc. We can create the
    // VsyncRefreshDriverTimer directly.
    sRegularRateTimer = new VsyncRefreshDriverTimer();
    return;
  }

#ifdef MOZ_NUWA_PROCESS
  // NUWA process will just use software timer. Use NuwaAddFinalConstructor()
  // to register a callback to create the vsync-base refresh timer after a
  // process is created.
  if (IsNuwaProcess()) {
    NuwaAddFinalConstructor(&CreateContentVsyncRefreshTimer, nullptr);
    return;
  }
#endif
  // If this process is not created by NUWA, just create the vsync timer here.
  CreateContentVsyncRefreshTimer(nullptr);
}

static uint32_t
GetFirstFrameDelay(imgIRequest* req)
{
  nsCOMPtr<imgIContainer> container;
  if (NS_FAILED(req->GetImage(getter_AddRefs(container))) || !container) {
    return 0;
  }

  // If this image isn't animated, there isn't a first frame delay.
  int32_t delay = container->GetFirstFrameDelay();
  if (delay < 0)
    return 0;

  return static_cast<uint32_t>(delay);
}

/* static */ void
nsRefreshDriver::InitializeStatics()
{
  if (!gLog) {
    gLog = PR_NewLogModule("nsRefreshDriver");
  }
}

/* static */ void
nsRefreshDriver::Shutdown()
{
  // clean up our timers
  delete sRegularRateTimer;
  delete sThrottledRateTimer;

  sRegularRateTimer = nullptr;
  sThrottledRateTimer = nullptr;

#ifdef XP_WIN
  if (sDisableHighPrecisionTimersTimer) {
    sDisableHighPrecisionTimersTimer->Cancel();
    NS_RELEASE(sDisableHighPrecisionTimersTimer);
    timeEndPeriod(1);
  } else if (sHighPrecisionTimerRequests) {
    timeEndPeriod(1);
  }
#endif
}

/* static */ int32_t
nsRefreshDriver::DefaultInterval()
{
  return NSToIntRound(1000.0 / DEFAULT_FRAME_RATE);
}

// Compute the interval to use for the refresh driver timer, in milliseconds.
// outIsDefault indicates that rate was not explicitly set by the user
// so we might choose other, more appropriate rates (e.g. vsync, etc)
// layout.frame_rate=0 indicates "ASAP mode".
// In ASAP mode rendering is iterated as fast as possible (typically for stress testing).
// A target rate of 10k is used internally instead of special-handling 0.
// Backends which block on swap/present/etc should try to not block
// when layout.frame_rate=0 - to comply with "ASAP" as much as possible.
double
nsRefreshDriver::GetRegularTimerInterval(bool *outIsDefault) const
{
  int32_t rate = Preferences::GetInt("layout.frame_rate", -1);
  if (rate < 0) {
    rate = DEFAULT_FRAME_RATE;
    if (outIsDefault) {
      *outIsDefault = true;
    }
  } else {
    if (outIsDefault) {
      *outIsDefault = false;
    }
  }

  if (rate == 0) {
    rate = 10000;
  }

  return 1000.0 / rate;
}

/* static */ double
nsRefreshDriver::GetThrottledTimerInterval()
{
  int32_t rate = Preferences::GetInt("layout.throttled_frame_rate", -1);
  if (rate <= 0) {
    rate = DEFAULT_THROTTLED_FRAME_RATE;
  }
  return 1000.0 / rate;
}

/* static */ mozilla::TimeDuration
nsRefreshDriver::GetMinRecomputeVisibilityInterval()
{
  int32_t interval =
    Preferences::GetInt("layout.visibility.min-recompute-interval-ms", -1);
  if (interval <= 0) {
    interval = DEFAULT_RECOMPUTE_VISIBILITY_INTERVAL_MS;
  }
  return TimeDuration::FromMilliseconds(interval);
}

double
nsRefreshDriver::GetRefreshTimerInterval() const
{
  return mThrottled ? GetThrottledTimerInterval() : GetRegularTimerInterval();
}

RefreshDriverTimer*
nsRefreshDriver::ChooseTimer() const
{
  if (mThrottled) {
    if (!sThrottledRateTimer) 
      sThrottledRateTimer = new InactiveRefreshDriverTimer(GetThrottledTimerInterval(),
                                                           DEFAULT_INACTIVE_TIMER_DISABLE_SECONDS * 1000.0);
    return sThrottledRateTimer;
  }

  if (!sRegularRateTimer) {
    bool isDefault = true;
    double rate = GetRegularTimerInterval(&isDefault);

    // Try to use vsync-base refresh timer first for sRegularRateTimer.
    CreateVsyncRefreshTimer();

#ifdef XP_WIN
    if (!sRegularRateTimer && PreciseRefreshDriverTimerWindowsDwmVsync::IsSupported()) {
      sRegularRateTimer = new PreciseRefreshDriverTimerWindowsDwmVsync(rate, isDefault);
    }
#endif
    if (!sRegularRateTimer) {
      sRegularRateTimer = new PreciseRefreshDriverTimer(rate);
    }
  }
  return sRegularRateTimer;
}

nsRefreshDriver::nsRefreshDriver(nsPresContext* aPresContext)
  : mActiveTimer(nullptr),
    mReflowCause(nullptr),
    mStyleCause(nullptr),
    mPresContext(aPresContext),
    mRootRefresh(nullptr),
    mPendingTransaction(0),
    mCompletedTransaction(0),
    mFreezeCount(0),
    mThrottledFrameRequestInterval(TimeDuration::FromMilliseconds(
                                     GetThrottledTimerInterval())),
    mMinRecomputeVisibilityInterval(GetMinRecomputeVisibilityInterval()),
    mThrottled(false),
    mNeedToRecomputeVisibility(false),
    mTestControllingRefreshes(false),
    mViewManagerFlushIsPending(false),
    mRequestedHighPrecision(false),
    mInRefresh(false),
    mWaitingForTransaction(false),
    mSkippedPaints(false)
{
  mMostRecentRefreshEpochTime = JS_Now();
  mMostRecentRefresh = TimeStamp::Now();
  mMostRecentTick = mMostRecentRefresh;
  mNextThrottledFrameRequestTick = mMostRecentTick;
  mNextRecomputeVisibilityTick = mMostRecentTick;
}

nsRefreshDriver::~nsRefreshDriver()
{
  MOZ_ASSERT(ObserverCount() == 0,
             "observers should have unregistered");
  MOZ_ASSERT(!mActiveTimer, "timer should be gone");
  
  if (mRootRefresh) {
    mRootRefresh->RemoveRefreshObserver(this, Flush_Style);
    mRootRefresh = nullptr;
  }
  for (nsIPresShell* shell : mPresShellsToInvalidateIfHidden) {
    shell->InvalidatePresShellIfHidden();
  }
  mPresShellsToInvalidateIfHidden.Clear();

  profiler_free_backtrace(mStyleCause);
  profiler_free_backtrace(mReflowCause);
}

// Method for testing.  See nsIDOMWindowUtils.advanceTimeAndRefresh
// for description.
void
nsRefreshDriver::AdvanceTimeAndRefresh(int64_t aMilliseconds)
{
  // ensure that we're removed from our driver
  StopTimer();

  if (!mTestControllingRefreshes) {
    mMostRecentRefreshEpochTime = JS_Now();
    mMostRecentRefresh = TimeStamp::Now();

    mTestControllingRefreshes = true;
    if (mWaitingForTransaction) {
      // Disable any refresh driver throttling when entering test mode
      mWaitingForTransaction = false;
      mSkippedPaints = false;
    }
  }

  mMostRecentRefreshEpochTime += aMilliseconds * 1000;
  mMostRecentRefresh += TimeDuration::FromMilliseconds((double) aMilliseconds);

  mozilla::dom::AutoNoJSAPI nojsapi;
  DoTick();
}

void
nsRefreshDriver::RestoreNormalRefresh()
{
  mTestControllingRefreshes = false;
  EnsureTimerStarted(eAllowTimeToGoBackwards);
  mCompletedTransaction = mPendingTransaction;
}

TimeStamp
nsRefreshDriver::MostRecentRefresh() const
{
  const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted();

  return mMostRecentRefresh;
}

int64_t
nsRefreshDriver::MostRecentRefreshEpochTime() const
{
  const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted();

  return mMostRecentRefreshEpochTime;
}

bool
nsRefreshDriver::AddRefreshObserver(nsARefreshObserver* aObserver,
                                    mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  bool success = array.AppendElement(aObserver) != nullptr;
  EnsureTimerStarted();
  return success;
}

bool
nsRefreshDriver::RemoveRefreshObserver(nsARefreshObserver* aObserver,
                                       mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  return array.RemoveElement(aObserver);
}

void
nsRefreshDriver::AddPostRefreshObserver(nsAPostRefreshObserver* aObserver)
{
  mPostRefreshObservers.AppendElement(aObserver);
}

void
nsRefreshDriver::RemovePostRefreshObserver(nsAPostRefreshObserver* aObserver)
{
  mPostRefreshObservers.RemoveElement(aObserver);
}

bool
nsRefreshDriver::AddImageRequest(imgIRequest* aRequest)
{
  uint32_t delay = GetFirstFrameDelay(aRequest);
  if (delay == 0) {
    if (!mRequests.PutEntry(aRequest)) {
      return false;
    }
  } else {
    ImageStartData* start = mStartTable.Get(delay);
    if (!start) {
      start = new ImageStartData();
      mStartTable.Put(delay, start);
    }
    start->mEntries.PutEntry(aRequest);
  }

  EnsureTimerStarted();

  return true;
}

void
nsRefreshDriver::RemoveImageRequest(imgIRequest* aRequest)
{
  // Try to remove from both places, just in case, because we can't tell
  // whether RemoveEntry() succeeds.
  mRequests.RemoveEntry(aRequest);
  uint32_t delay = GetFirstFrameDelay(aRequest);
  if (delay != 0) {
    ImageStartData* start = mStartTable.Get(delay);
    if (start) {
      start->mEntries.RemoveEntry(aRequest);
    }
  }
}

void
nsRefreshDriver::EnsureTimerStarted(EnsureTimerStartedFlags aFlags)
{
  if (mTestControllingRefreshes)
    return;

  // will it already fire, and no other changes needed?
  if (mActiveTimer && !(aFlags & eAdjustingTimer))
    return;

  if (IsFrozen() || !mPresContext) {
    // If we don't want to start it now, or we've been disconnected.
    StopTimer();
    return;
  }

  if (mPresContext->Document()->IsBeingUsedAsImage()) {
    // Image documents receive ticks from clients' refresh drivers.
    // XXXdholbert Exclude SVG-in-opentype fonts from this optimization, until
    // they receive refresh-driver ticks from their client docs (bug 1107252).
    nsIURI* uri = mPresContext->Document()->GetDocumentURI();
    if (!uri || !IsFontTableURI(uri)) {
      MOZ_ASSERT(!mActiveTimer,
                 "image doc refresh driver should never have its own timer");
      return;
    }
  }

  // We got here because we're either adjusting the time *or* we're
  // starting it for the first time.  Add to the right timer,
  // prehaps removing it from a previously-set one.
  RefreshDriverTimer *newTimer = ChooseTimer();
  if (newTimer != mActiveTimer) {
    if (mActiveTimer)
      mActiveTimer->RemoveRefreshDriver(this);
    mActiveTimer = newTimer;
    mActiveTimer->AddRefreshDriver(this);
  }

  // Since the different timers are sampled at different rates, when switching
  // timers, the most recent refresh of the new timer may be *before* the
  // most recent refresh of the old timer. However, the refresh driver time
  // should not go backwards so we clamp the most recent refresh time.
  //
  // The one exception to this is when we are restoring the refresh driver
  // from test control in which case the time is expected to go backwards
  // (see bug 1043078).
  mMostRecentRefresh =
    aFlags & eAllowTimeToGoBackwards
    ? mActiveTimer->MostRecentRefresh()
    : std::max(mActiveTimer->MostRecentRefresh(), mMostRecentRefresh);
  mMostRecentRefreshEpochTime =
    aFlags & eAllowTimeToGoBackwards
    ? mActiveTimer->MostRecentRefreshEpochTime()
    : std::max(mActiveTimer->MostRecentRefreshEpochTime(),
               mMostRecentRefreshEpochTime);
}

void
nsRefreshDriver::StopTimer()
{
  if (!mActiveTimer)
    return;

  mActiveTimer->RemoveRefreshDriver(this);
  mActiveTimer = nullptr;

  if (mRequestedHighPrecision) {
    SetHighPrecisionTimersEnabled(false);
  }
}

#ifdef XP_WIN
static void
DisableHighPrecisionTimersCallback(nsITimer *aTimer, void *aClosure)
{
  timeEndPeriod(1);
  NS_RELEASE(sDisableHighPrecisionTimersTimer);
}
#endif

void
nsRefreshDriver::ConfigureHighPrecision()
{
  bool haveUnthrottledFrameRequestCallbacks =
    mFrameRequestCallbackDocs.Length() > 0;

  // if the only change that's needed is that we need high precision,
  // then just set that
  if (!mThrottled && !mRequestedHighPrecision &&
      haveUnthrottledFrameRequestCallbacks) {
    SetHighPrecisionTimersEnabled(true);
  } else if (mRequestedHighPrecision && !haveUnthrottledFrameRequestCallbacks) {
    SetHighPrecisionTimersEnabled(false);
  }
}

void
nsRefreshDriver::SetHighPrecisionTimersEnabled(bool aEnable)
{
  LOG("[%p] SetHighPrecisionTimersEnabled (%s)", this, aEnable ? "true" : "false");

  if (aEnable) {
    NS_ASSERTION(!mRequestedHighPrecision, "SetHighPrecisionTimersEnabled(true) called when already requested!");
#ifdef XP_WIN
    if (++sHighPrecisionTimerRequests == 1) {
      // If we had a timer scheduled to disable it, that means that it's already
      // enabled; just cancel the timer.  Otherwise, really enable it.
      if (sDisableHighPrecisionTimersTimer) {
        sDisableHighPrecisionTimersTimer->Cancel();
        NS_RELEASE(sDisableHighPrecisionTimersTimer);
      } else {
        timeBeginPeriod(1);
      }
    }
#endif
    mRequestedHighPrecision = true;
  } else {
    NS_ASSERTION(mRequestedHighPrecision, "SetHighPrecisionTimersEnabled(false) called when not requested!");
#ifdef XP_WIN
    if (--sHighPrecisionTimerRequests == 0) {
      // Don't jerk us around between high precision and low precision
      // timers; instead, only allow leaving high precision timers
      // after 90 seconds.  This is arbitrary, but hopefully good
      // enough.
      NS_ASSERTION(!sDisableHighPrecisionTimersTimer, "We shouldn't have an outstanding disable-high-precision timer !");

      nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
      if (timer) {
        timer.forget(&sDisableHighPrecisionTimersTimer);
        sDisableHighPrecisionTimersTimer->InitWithFuncCallback(DisableHighPrecisionTimersCallback,
                                                               nullptr,
                                                               90 * 1000,
                                                               nsITimer::TYPE_ONE_SHOT);
      } else {
        // might happen if we're shutting down XPCOM; just drop the time period down
        // immediately
        timeEndPeriod(1);
      }
    }
#endif
    mRequestedHighPrecision = false;
  }
}

uint32_t
nsRefreshDriver::ObserverCount() const
{
  uint32_t sum = 0;
  for (uint32_t i = 0; i < ArrayLength(mObservers); ++i) {
    sum += mObservers[i].Length();
  }

  // Even while throttled, we need to process layout and style changes.  Style
  // changes can trigger transitions which fire events when they complete, and
  // layout changes can affect media queries on child documents, triggering
  // style changes, etc.
  sum += mStyleFlushObservers.Length();
  sum += mLayoutFlushObservers.Length();
  sum += mFrameRequestCallbackDocs.Length();
  sum += mThrottledFrameRequestCallbackDocs.Length();
  sum += mViewManagerFlushIsPending;
  return sum;
}

/* static */ PLDHashOperator
nsRefreshDriver::StartTableRequestCounter(const uint32_t& aKey,
                                          ImageStartData* aEntry,
                                          void* aUserArg)
{
  uint32_t *count = static_cast<uint32_t*>(aUserArg);
  *count += aEntry->mEntries.Count();

  return PL_DHASH_NEXT;
}

uint32_t
nsRefreshDriver::ImageRequestCount() const
{
  uint32_t count = 0;
  mStartTable.EnumerateRead(nsRefreshDriver::StartTableRequestCounter, &count);
  return count + mRequests.Count();
}

nsRefreshDriver::ObserverArray&
nsRefreshDriver::ArrayFor(mozFlushType aFlushType)
{
  switch (aFlushType) {
    case Flush_Style:
      return mObservers[0];
    case Flush_Layout:
      return mObservers[1];
    case Flush_Display:
      return mObservers[2];
    default:
      MOZ_ASSERT(false, "bad flush type");
      return *static_cast<ObserverArray*>(nullptr);
  }
}

/*
 * nsITimerCallback implementation
 */

void
nsRefreshDriver::DoTick()
{
  NS_PRECONDITION(!IsFrozen(), "Why are we notified while frozen?");
  NS_PRECONDITION(mPresContext, "Why are we notified after disconnection?");
  NS_PRECONDITION(!nsContentUtils::GetCurrentJSContext(),
                  "Shouldn't have a JSContext on the stack");

  if (mTestControllingRefreshes) {
    Tick(mMostRecentRefreshEpochTime, mMostRecentRefresh);
  } else {
    Tick(JS_Now(), TimeStamp::Now());
  }
}

struct DocumentFrameCallbacks {
  explicit DocumentFrameCallbacks(nsIDocument* aDocument) :
    mDocument(aDocument)
  {}

  nsCOMPtr<nsIDocument> mDocument;
  nsIDocument::FrameRequestCallbackList mCallbacks;
};

static nsDocShell* GetDocShell(nsPresContext* aPresContext)
{
  return static_cast<nsDocShell*>(aPresContext->GetDocShell());
}

static bool
HasPendingAnimations(nsIPresShell* aShell)
{
  nsIDocument* doc = aShell->GetDocument();
  if (!doc) {
    return false;
  }

  PendingAnimationTracker* tracker = doc->GetPendingAnimationTracker();
  return tracker && tracker->HasPendingAnimations();
}

/**
 * Return a list of all the child docShells in a given root docShell that are
 * visible and are recording markers for the profilingTimeline
 */
static void GetProfileTimelineSubDocShells(nsDocShell* aRootDocShell,
                                           nsTArray<nsDocShell*>& aShells)
{
  if (!aRootDocShell || nsDocShell::gProfileTimelineRecordingsCount == 0) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = aRootDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeAll,
    nsIDocShell::ENUMERATE_BACKWARDS, getter_AddRefs(enumerator));

  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIDocShell> curItem;
  bool hasMore = false;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> curSupports;
    enumerator->GetNext(getter_AddRefs(curSupports));
    curItem = do_QueryInterface(curSupports);

    if (!curItem || !curItem->GetRecordProfileTimelineMarkers()) {
      continue;
    }

    nsDocShell* shell = static_cast<nsDocShell*>(curItem.get());
    bool isVisible = false;
    shell->GetVisibility(&isVisible);
    if (!isVisible) {
      continue;
    }

    aShells.AppendElement(shell);
  };
}

static void
TakeFrameRequestCallbacksFrom(nsIDocument* aDocument,
                              nsTArray<DocumentFrameCallbacks>& aTarget)
{
  aTarget.AppendElement(aDocument);
  aDocument->TakeFrameRequestCallbacks(aTarget.LastElement().mCallbacks);
}

void
nsRefreshDriver::RunFrameRequestCallbacks(int64_t aNowEpoch, TimeStamp aNowTime)
{
  // Grab all of our frame request callbacks up front.
  nsTArray<DocumentFrameCallbacks>
    frameRequestCallbacks(mFrameRequestCallbackDocs.Length() +
                          mThrottledFrameRequestCallbackDocs.Length());

  // First, grab throttled frame request callbacks.
  {
    nsTArray<nsIDocument*> docsToRemove;

    // We always tick throttled frame requests if the entire refresh driver is
    // throttled, because in that situation throttled frame requests tick at the
    // same frequency as non-throttled frame requests.
    bool tickThrottledFrameRequests = mThrottled;

    if (!tickThrottledFrameRequests &&
        aNowTime >= mNextThrottledFrameRequestTick) {
      mNextThrottledFrameRequestTick = aNowTime + mThrottledFrameRequestInterval;
      tickThrottledFrameRequests = true;
    }

    for (nsIDocument* doc : mThrottledFrameRequestCallbackDocs) {
      if (tickThrottledFrameRequests) {
        // We're ticking throttled documents, so grab this document's requests.
        // We don't bother appending to docsToRemove because we're going to
        // clear mThrottledFrameRequestCallbackDocs anyway.
        TakeFrameRequestCallbacksFrom(doc, frameRequestCallbacks);
      } else if (!doc->ShouldThrottleFrameRequests()) {
        // This document is no longer throttled, so grab its requests even
        // though we're not ticking throttled frame requests right now. If
        // this is the first unthrottled document with frame requests, we'll
        // enter high precision mode the next time the callback is scheduled.
        TakeFrameRequestCallbacksFrom(doc, frameRequestCallbacks);
        docsToRemove.AppendElement(doc);
      }
    }

    // Remove all the documents we're ticking from
    // mThrottledFrameRequestCallbackDocs so they can be readded as needed.
    if (tickThrottledFrameRequests) {
      mThrottledFrameRequestCallbackDocs.Clear();
    } else {
      // XXX(seth): We're using this approach to avoid concurrent modification
      // of mThrottledFrameRequestCallbackDocs. docsToRemove usually has either
      // zero elements or a very small number, so this should be OK in practice.
      for (nsIDocument* doc : docsToRemove) {
        mThrottledFrameRequestCallbackDocs.RemoveElement(doc);
      }
    }
  }

  // Now grab unthrottled frame request callbacks.
  for (nsIDocument* doc : mFrameRequestCallbackDocs) {
    TakeFrameRequestCallbacksFrom(doc, frameRequestCallbacks);
  }

  // Reset mFrameRequestCallbackDocs so they can be readded as needed.
  mFrameRequestCallbackDocs.Clear();

  if (!frameRequestCallbacks.IsEmpty()) {
    profiler_tracing("Paint", "Scripts", TRACING_INTERVAL_START);
    int64_t eventTime = aNowEpoch / PR_USEC_PER_MSEC;
    for (const DocumentFrameCallbacks& docCallbacks : frameRequestCallbacks) {
      // XXXbz Bug 863140: GetInnerWindow can return the outer
      // window in some cases.
      nsPIDOMWindow* innerWindow = docCallbacks.mDocument->GetInnerWindow();
      DOMHighResTimeStamp timeStamp = 0;
      if (innerWindow && innerWindow->IsInnerWindow()) {
        nsPerformance* perf = innerWindow->GetPerformance();
        if (perf) {
          timeStamp = perf->GetDOMTiming()->TimeStampToDOMHighRes(aNowTime);
        }
        // else window is partially torn down already
      }
      for (const nsIDocument::FrameRequestCallbackHolder& holder :
           docCallbacks.mCallbacks) {
        nsAutoMicroTask mt;
        if (holder.HasWebIDLCallback()) {
          ErrorResult ignored;
          holder.GetWebIDLCallback()->Call(timeStamp, ignored);
        } else {
          holder.GetXPCOMCallback()->Sample(eventTime);
        }
      }
    }
    profiler_tracing("Paint", "Scripts", TRACING_INTERVAL_END);
  }
}

void
nsRefreshDriver::Tick(int64_t aNowEpoch, TimeStamp aNowTime)
{
  NS_PRECONDITION(!nsContentUtils::GetCurrentJSContext(),
                  "Shouldn't have a JSContext on the stack");

  if (nsNPAPIPluginInstance::InPluginCallUnsafeForReentry()) {
    NS_ERROR("Refresh driver should not run during plugin call!");
    // Try to survive this by just ignoring the refresh tick.
    return;
  }

  PROFILER_LABEL("nsRefreshDriver", "Tick",
    js::ProfileEntry::Category::GRAPHICS);

  // We're either frozen or we were disconnected (likely in the middle
  // of a tick iteration).  Just do nothing here, since our
  // prescontext went away.
  if (IsFrozen() || !mPresContext) {
    return;
  }

  // We can have a race condition where the vsync timestamp
  // is before the most recent refresh due to a forced refresh.
  // The underlying assumption is that the refresh driver tick can only
  // go forward in time, not backwards. To prevent the refresh
  // driver from going back in time, just skip this tick and
  // wait until the next tick.
  if ((aNowTime <= mMostRecentRefresh) && !mTestControllingRefreshes) {
    return;
  }

  TimeStamp previousRefresh = mMostRecentRefresh;

  mMostRecentRefresh = aNowTime;
  mMostRecentRefreshEpochTime = aNowEpoch;

  if (IsWaitingForPaint(aNowTime)) {
    // We're currently suspended waiting for earlier Tick's to
    // be completed (on the Compositor). Mark that we missed the paint
    // and keep waiting.
    return;
  }
  mMostRecentTick = aNowTime;
  if (mRootRefresh) {
    mRootRefresh->RemoveRefreshObserver(this, Flush_Style);
    mRootRefresh = nullptr;
  }
  mSkippedPaints = false;

  nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
  if (!presShell || (ObserverCount() == 0 && ImageRequestCount() == 0)) {
    // Things are being destroyed, or we no longer have any observers.
    // We don't want to stop the timer when observers are initially
    // removed, because sometimes observers can be added and removed
    // often depending on what other things are going on and in that
    // situation we don't want to thrash our timer.  So instead we
    // wait until we get a Notify() call when we have no observers
    // before stopping the timer.
    StopTimer();
    return;
  }

  AutoRestore<bool> restoreInRefresh(mInRefresh);
  mInRefresh = true;

  AutoRestore<TimeStamp> restoreTickStart(mTickStart);
  mTickStart = TimeStamp::Now();

  /*
   * The timer holds a reference to |this| while calling |Notify|.
   * However, implementations of |WillRefresh| are permitted to destroy
   * the pres context, which will cause our |mPresContext| to become
   * null.  If this happens, we must stop notifying observers.
   */
  for (uint32_t i = 0; i < ArrayLength(mObservers); ++i) {
    ObserverArray::EndLimitedIterator etor(mObservers[i]);
    while (etor.HasMore()) {
      nsRefPtr<nsARefreshObserver> obs = etor.GetNext();
      obs->WillRefresh(aNowTime);

      if (!mPresContext || !mPresContext->GetPresShell()) {
        StopTimer();
        return;
      }
    }

    if (i == 0) {
      // This is the Flush_Style case.

      RunFrameRequestCallbacks(aNowEpoch, aNowTime);

      if (mPresContext && mPresContext->GetPresShell()) {
        bool tracingStyleFlush = false;
        nsAutoTArray<nsIPresShell*, 16> observers;
        observers.AppendElements(mStyleFlushObservers);
        for (uint32_t j = observers.Length();
             j && mPresContext && mPresContext->GetPresShell(); --j) {
          // Make sure to not process observers which might have been removed
          // during previous iterations.
          nsIPresShell* shell = observers[j - 1];
          if (!mStyleFlushObservers.Contains(shell))
            continue;

          if (!tracingStyleFlush) {
            tracingStyleFlush = true;
            profiler_tracing("Paint", "Styles", mStyleCause, TRACING_INTERVAL_START);
            mStyleCause = nullptr;
          }

          NS_ADDREF(shell);
          mStyleFlushObservers.RemoveElement(shell);
          shell->GetPresContext()->RestyleManager()->mObservingRefreshDriver = false;
          shell->FlushPendingNotifications(ChangesToFlush(Flush_Style, false));
          // Inform the FontFaceSet that we ticked, so that it can resolve its
          // ready promise if it needs to (though it might still be waiting on
          // a layout flush).
          nsPresContext* presContext = shell->GetPresContext();
          if (presContext) {
            presContext->NotifyFontFaceSetOnRefresh();
          }
          NS_RELEASE(shell);
        }

        mNeedToRecomputeVisibility = true;

        if (tracingStyleFlush) {
          profiler_tracing("Paint", "Styles", TRACING_INTERVAL_END);
        }
      }

      if (!nsLayoutUtils::AreAsyncAnimationsEnabled()) {
        mPresContext->TickLastStyleUpdateForAllAnimations();
      }
    } else if  (i == 1) {
      // This is the Flush_Layout case.
      if (mPresContext && mPresContext->GetPresShell()) {
        bool tracingLayoutFlush = false;
        nsAutoTArray<nsIPresShell*, 16> observers;
        observers.AppendElements(mLayoutFlushObservers);
        for (uint32_t j = observers.Length();
             j && mPresContext && mPresContext->GetPresShell(); --j) {
          // Make sure to not process observers which might have been removed
          // during previous iterations.
          nsIPresShell* shell = observers[j - 1];
          if (!mLayoutFlushObservers.Contains(shell))
            continue;

          if (!tracingLayoutFlush) {
            tracingLayoutFlush = true;
            profiler_tracing("Paint", "Reflow", mReflowCause, TRACING_INTERVAL_START);
            mReflowCause = nullptr;
          }

          NS_ADDREF(shell);
          mLayoutFlushObservers.RemoveElement(shell);
          shell->mReflowScheduled = false;
          shell->mSuppressInterruptibleReflows = false;
          mozFlushType flushType = HasPendingAnimations(shell)
                                 ? Flush_Layout
                                 : Flush_InterruptibleLayout;
          shell->FlushPendingNotifications(ChangesToFlush(flushType, false));
          // Inform the FontFaceSet that we ticked, so that it can resolve its
          // ready promise if it needs to.
          nsPresContext* presContext = shell->GetPresContext();
          if (presContext) {
            presContext->NotifyFontFaceSetOnRefresh();
          }
          NS_RELEASE(shell);
        }

        mNeedToRecomputeVisibility = true;

        if (tracingLayoutFlush) {
          profiler_tracing("Paint", "Reflow", TRACING_INTERVAL_END);
        }
      }
    }
  }

  // Recompute image visibility if it's necessary and enough time has passed
  // since the last time we did it.
  if (mNeedToRecomputeVisibility && !mThrottled &&
      aNowTime >= mNextRecomputeVisibilityTick &&
      !presShell->IsPaintingSuppressed()) {
    mNextRecomputeVisibilityTick = aNowTime + mMinRecomputeVisibilityInterval;
    mNeedToRecomputeVisibility = false;

    presShell->ScheduleImageVisibilityUpdate();
  }

  /*
   * Perform notification to imgIRequests subscribed to listen
   * for refresh events.
   */

  ImageRequestParameters parms = {aNowTime, previousRefresh, &mRequests};

  mStartTable.EnumerateRead(nsRefreshDriver::StartTableRefresh, &parms);

  if (mRequests.Count()) {
    // RequestRefresh may run scripts, so it's not safe to directly call it
    // while using a hashtable enumerator to enumerate mRequests in case
    // script modifies the hashtable. Instead, we build a (local) array of
    // images to refresh, and then we refresh each image in that array.
    nsCOMArray<imgIContainer> imagesToRefresh(mRequests.Count());
    mRequests.EnumerateEntries(nsRefreshDriver::ImageRequestEnumerator,
                               &imagesToRefresh);

    for (uint32_t i = 0; i < imagesToRefresh.Length(); i++) {
      imagesToRefresh[i]->RequestRefresh(aNowTime);
    }
  }

  for (nsIPresShell* shell : mPresShellsToInvalidateIfHidden) {
    shell->InvalidatePresShellIfHidden();
  }
  mPresShellsToInvalidateIfHidden.Clear();

  if (mViewManagerFlushIsPending) {
    nsTArray<nsDocShell*> profilingDocShells;
    GetProfileTimelineSubDocShells(GetDocShell(mPresContext), profilingDocShells);
    for (nsDocShell* docShell : profilingDocShells) {
      // For the sake of the profile timeline's simplicity, this is flagged as
      // paint even if it includes creating display lists
      docShell->AddProfileTimelineMarker("Paint", TRACING_INTERVAL_START);
    }
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Starting ProcessPendingUpdates\n");
    }
#endif

    mViewManagerFlushIsPending = false;
    nsRefPtr<nsViewManager> vm = mPresContext->GetPresShell()->GetViewManager();
    vm->ProcessPendingUpdates();
#ifdef MOZ_DUMP_PAINTING
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      printf_stderr("Ending ProcessPendingUpdates\n");
    }
#endif
    for (nsDocShell* docShell : profilingDocShells) {
      docShell->AddProfileTimelineMarker("Paint", TRACING_INTERVAL_END);
    }

    if (nsContentUtils::XPConnect()) {
      nsContentUtils::XPConnect()->NotifyDidPaint();
      nsJSContext::NotifyDidPaint();
    }
  }

#ifndef ANDROID  /* bug 1142079 */
  mozilla::Telemetry::AccumulateTimeDelta(mozilla::Telemetry::REFRESH_DRIVER_TICK, mTickStart);
#endif

  nsTObserverArray<nsAPostRefreshObserver*>::ForwardIterator iter(mPostRefreshObservers);
  while (iter.HasMore()) {
    nsAPostRefreshObserver* observer = iter.GetNext();
    observer->DidRefresh();
  }

  NS_ASSERTION(mInRefresh, "Still in refresh");
}

/* static */ PLDHashOperator
nsRefreshDriver::ImageRequestEnumerator(nsISupportsHashKey* aEntry,
                                        void* aUserArg)
{
  nsCOMArray<imgIContainer>* imagesToRefresh =
    static_cast<nsCOMArray<imgIContainer>*> (aUserArg);
  imgIRequest* req = static_cast<imgIRequest*>(aEntry->GetKey());
  MOZ_ASSERT(req, "Unable to retrieve the image request");
  nsCOMPtr<imgIContainer> image;
  if (NS_SUCCEEDED(req->GetImage(getter_AddRefs(image)))) {
    imagesToRefresh->AppendElement(image);
  }

  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator
nsRefreshDriver::BeginRefreshingImages(nsISupportsHashKey* aEntry,
                                       void* aUserArg)
{
  ImageRequestParameters* parms =
    static_cast<ImageRequestParameters*> (aUserArg);

  imgIRequest* req = static_cast<imgIRequest*>(aEntry->GetKey());
  MOZ_ASSERT(req, "Unable to retrieve the image request");

  parms->mRequests->PutEntry(req);

  nsCOMPtr<imgIContainer> image;
  if (NS_SUCCEEDED(req->GetImage(getter_AddRefs(image)))) {
    image->SetAnimationStartTime(parms->mDesired);
  }

  return PL_DHASH_REMOVE;
}

/* static */ PLDHashOperator
nsRefreshDriver::StartTableRefresh(const uint32_t& aDelay,
                                   ImageStartData* aData,
                                   void* aUserArg)
{
  ImageRequestParameters* parms =
    static_cast<ImageRequestParameters*> (aUserArg);

  if (aData->mStartTime) {
    TimeStamp& start = *aData->mStartTime;
    TimeDuration prev = parms->mPrevious - start;
    TimeDuration curr = parms->mCurrent - start;
    uint32_t prevMultiple = static_cast<uint32_t>(prev.ToMilliseconds()) / aDelay;

    // We want to trigger images' refresh if we've just crossed over a multiple
    // of the first image's start time. If so, set the animation start time to
    // the nearest multiple of the delay and move all the images in this table
    // to the main requests table.
    if (prevMultiple != static_cast<uint32_t>(curr.ToMilliseconds()) / aDelay) {
      parms->mDesired = start + TimeDuration::FromMilliseconds(prevMultiple * aDelay);
      aData->mEntries.EnumerateEntries(nsRefreshDriver::BeginRefreshingImages, parms);
    }
  } else {
    // This is the very first time we've drawn images with this time delay.
    // Set the animation start time to "now" and move all the images in this
    // table to the main requests table.
    parms->mDesired = parms->mCurrent;
    aData->mEntries.EnumerateEntries(nsRefreshDriver::BeginRefreshingImages, parms);
    aData->mStartTime.emplace(parms->mCurrent);
  }

  return PL_DHASH_NEXT;
}

void
nsRefreshDriver::Freeze()
{
  StopTimer();
  mFreezeCount++;
}

void
nsRefreshDriver::Thaw()
{
  NS_ASSERTION(mFreezeCount > 0, "Thaw() called on an unfrozen refresh driver");

  if (mFreezeCount > 0) {
    mFreezeCount--;
  }

  if (mFreezeCount == 0) {
    if (ObserverCount() || ImageRequestCount()) {
      // FIXME: This isn't quite right, since our EnsureTimerStarted call
      // updates our mMostRecentRefresh, but the DoRefresh call won't run
      // and notify our observers until we get back to the event loop.
      // Thus MostRecentRefresh() will lie between now and the DoRefresh.
      NS_DispatchToCurrentThread(NS_NewRunnableMethod(this, &nsRefreshDriver::DoRefresh));
      EnsureTimerStarted();
    }
  }
}

void
nsRefreshDriver::FinishedWaitingForTransaction()
{
  mWaitingForTransaction = false;
  if (mSkippedPaints &&
      !IsInRefresh() &&
      (ObserverCount() || ImageRequestCount())) {
    profiler_tracing("Paint", "RD", TRACING_INTERVAL_START);
    DoRefresh();
    profiler_tracing("Paint", "RD", TRACING_INTERVAL_END);
  }
  mSkippedPaints = false;
}

uint64_t
nsRefreshDriver::GetTransactionId()
{
  ++mPendingTransaction;

  if (mPendingTransaction >= mCompletedTransaction + 2 &&
      !mWaitingForTransaction &&
      !mTestControllingRefreshes) {
    mWaitingForTransaction = true;
    mSkippedPaints = false;
  }

  return mPendingTransaction;
}

void
nsRefreshDriver::RevokeTransactionId(uint64_t aTransactionId)
{
  MOZ_ASSERT(aTransactionId == mPendingTransaction);
  if (mPendingTransaction == mCompletedTransaction + 2 &&
      mWaitingForTransaction) {
    MOZ_ASSERT(!mSkippedPaints, "How did we skip a paint when we're in the middle of one?");
    FinishedWaitingForTransaction();
  }
  mPendingTransaction--;
}

mozilla::TimeStamp
nsRefreshDriver::GetTransactionStart()
{
  return mTickStart;
}

void
nsRefreshDriver::NotifyTransactionCompleted(uint64_t aTransactionId)
{
  if (aTransactionId > mCompletedTransaction) {
    if (mPendingTransaction > mCompletedTransaction + 1 &&
        mWaitingForTransaction) {
      mCompletedTransaction = aTransactionId;
      FinishedWaitingForTransaction();
    } else {
      mCompletedTransaction = aTransactionId;
    }
  }
}

void
nsRefreshDriver::WillRefresh(mozilla::TimeStamp aTime)
{
  mRootRefresh->RemoveRefreshObserver(this, Flush_Style);
  mRootRefresh = nullptr;
  if (mSkippedPaints) {
    DoRefresh();
  }
}

bool
nsRefreshDriver::IsWaitingForPaint(mozilla::TimeStamp aTime)
{
  if (mTestControllingRefreshes) {
    return false;
  }
  // If we've skipped too many ticks then it's possible
  // that something went wrong and we're waiting on
  // a notification that will never arrive.
  if (aTime > (mMostRecentTick + TimeDuration::FromMilliseconds(200))) {
    mSkippedPaints = false;
    mWaitingForTransaction = false;
    if (mRootRefresh) {
      mRootRefresh->RemoveRefreshObserver(this, Flush_Style);
    }
    return false;
  }
  if (mWaitingForTransaction) {
    mSkippedPaints = true;
    return true;
  }

  // Try find the 'root' refresh driver for the current window and check
  // if that is waiting for a paint.
  nsPresContext *displayRoot = PresContext()->GetDisplayRootPresContext();
  if (displayRoot) {
    nsRefreshDriver *rootRefresh = displayRoot->GetRootPresContext()->RefreshDriver();
    if (rootRefresh && rootRefresh != this) {
      if (rootRefresh->IsWaitingForPaint(aTime)) {
        if (mRootRefresh != rootRefresh) {
          if (mRootRefresh) {
            mRootRefresh->RemoveRefreshObserver(this, Flush_Style);
          }
          rootRefresh->AddRefreshObserver(this, Flush_Style);
          mRootRefresh = rootRefresh;
        }
        mSkippedPaints = true;
        return true;
      }
    }
  }
  return false;
}

void
nsRefreshDriver::SetThrottled(bool aThrottled)
{
  if (aThrottled != mThrottled) {
    mThrottled = aThrottled;
    if (mActiveTimer) {
      // We want to switch our timer type here, so just stop and
      // restart the timer.
      EnsureTimerStarted(eAdjustingTimer);
    }
  }
}

/*static*/ void
nsRefreshDriver::PVsyncActorCreated(VsyncChild* aVsyncChild)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!XRE_IsParentProcess());
  VsyncRefreshDriverTimer* vsyncRefreshDriverTimer =
                           new VsyncRefreshDriverTimer(aVsyncChild);

  // If we are using software timer, swap current timer to
  // VsyncRefreshDriverTimer.
  if (sRegularRateTimer) {
    sRegularRateTimer->SwapRefreshDrivers(vsyncRefreshDriverTimer);
    delete sRegularRateTimer;
  }
  sRegularRateTimer = vsyncRefreshDriverTimer;
}

void
nsRefreshDriver::DoRefresh()
{
  // Don't do a refresh unless we're in a state where we should be refreshing.
  if (!IsFrozen() && mPresContext && mActiveTimer) {
    DoTick();
  }
}

#ifdef DEBUG
bool
nsRefreshDriver::IsRefreshObserver(nsARefreshObserver* aObserver,
                                   mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  return array.Contains(aObserver);
}
#endif

void
nsRefreshDriver::ScheduleViewManagerFlush()
{
  NS_ASSERTION(mPresContext->IsRoot(),
               "Should only schedule view manager flush on root prescontexts");
  mViewManagerFlushIsPending = true;
  EnsureTimerStarted();
}

void
nsRefreshDriver::ScheduleFrameRequestCallbacks(nsIDocument* aDocument)
{
  NS_ASSERTION(mFrameRequestCallbackDocs.IndexOf(aDocument) ==
               mFrameRequestCallbackDocs.NoIndex &&
               mThrottledFrameRequestCallbackDocs.IndexOf(aDocument) ==
               mThrottledFrameRequestCallbackDocs.NoIndex,
               "Don't schedule the same document multiple times");
  if (aDocument->ShouldThrottleFrameRequests()) {
    mThrottledFrameRequestCallbackDocs.AppendElement(aDocument);
  } else {
    mFrameRequestCallbackDocs.AppendElement(aDocument);
  }

  // make sure that the timer is running
  ConfigureHighPrecision();
  EnsureTimerStarted();
}

void
nsRefreshDriver::RevokeFrameRequestCallbacks(nsIDocument* aDocument)
{
  mFrameRequestCallbackDocs.RemoveElement(aDocument);
  mThrottledFrameRequestCallbackDocs.RemoveElement(aDocument);
  ConfigureHighPrecision();
  // No need to worry about restarting our timer in slack mode if it's already
  // running; that will happen automatically when it fires.
}

#undef LOG
