/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Windows Timer Primer
//
// A good article:  http://www.ddj.com/windows/184416651
// A good mozilla bug:  http://bugzilla.mozilla.org/show_bug.cgi?id=363258
//
// The default windows timer, GetSystemTimeAsFileTime is not very precise.
// It is only good to ~15.5ms.
//
// QueryPerformanceCounter is the logical choice for a high-precision timer.
// However, it is known to be buggy on some hardware.  Specifically, it can
// sometimes "jump".  On laptops, QPC can also be very expensive to call.
// It's 3-4x slower than timeGetTime() on desktops, but can be 10x slower
// on laptops.  A unittest exists which will show the relative cost of various
// timers on any system.
//
// The next logical choice is timeGetTime().  timeGetTime has a precision of
// 1ms, but only if you call APIs (timeBeginPeriod()) which affect all other
// applications on the system.  By default, precision is only 15.5ms.
// Unfortunately, we don't want to call timeBeginPeriod because we don't
// want to affect other applications.  Further, on mobile platforms, use of
// faster multimedia timers can hurt battery life.  See the intel
// article about this here:
// http://softwarecommunity.intel.com/articles/eng/1086.htm
//
// To work around all this, we're going to generally use timeGetTime().  We
// will only increase the system-wide timer if we're not running on battery
// power.  Using timeBeginPeriod(1) is a requirement in order to make our
// message loop waits have the same resolution that our time measurements
// do.  Otherwise, WaitForSingleObject(..., 1) will no less than 15ms when
// there is nothing else to waken the Wait.

#include "base/time.h"

#pragma comment(lib, "winmm.lib")
#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/cpu.h"
#include "base/singleton.h"
#include "mozilla/Casting.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using mozilla::BitwiseCast;

namespace {

// From MSDN, FILETIME "Contains a 64-bit value representing the number of
// 100-nanosecond intervals since January 1, 1601 (UTC)."
int64_t FileTimeToMicroseconds(const FILETIME& ft) {
  // Need to BitwiseCast to fix alignment, then divide by 10 to convert
  // 100-nanoseconds to milliseconds. This only works on little-endian
  // machines.
  return BitwiseCast<int64_t>(ft) / 10;
}

void MicrosecondsToFileTime(int64_t us, FILETIME* ft) {
  DCHECK(us >= 0) << "Time is less than 0, negative values are not "
      "representable in FILETIME";

  // Multiply by 10 to convert milliseconds to 100-nanoseconds. BitwiseCast will
  // handle alignment problems. This only works on little-endian machines.
  *ft = BitwiseCast<FILETIME>(us * 10);
}

int64_t CurrentWallclockMicroseconds() {
  FILETIME ft;
  ::GetSystemTimeAsFileTime(&ft);
  return FileTimeToMicroseconds(ft);
}

// Time between resampling the un-granular clock for this API.  60 seconds.
const int kMaxMillisecondsToAvoidDrift = 60 * Time::kMillisecondsPerSecond;

int64_t initial_time = 0;
TimeTicks initial_ticks;

void InitializeClock() {
  initial_ticks = TimeTicks::Now();
  initial_time = CurrentWallclockMicroseconds();
}

}  // namespace

// Time -----------------------------------------------------------------------

// The internal representation of Time uses FILETIME, whose epoch is 1601-01-01
// 00:00:00 UTC.  ((1970-1601)*365+89)*24*60*60*1000*1000, where 89 is the
// number of leap year days between 1601 and 1970: (1970-1601)/4 excluding
// 1700, 1800, and 1900.
// static
const int64_t Time::kTimeTToMicrosecondsOffset = GG_INT64_C(11644473600000000);

// static
Time Time::Now() {
  if (initial_time == 0)
    InitializeClock();

  // We implement time using the high-resolution timers so that we can get
  // timeouts which are smaller than 10-15ms.  If we just used
  // CurrentWallclockMicroseconds(), we'd have the less-granular timer.
  //
  // To make this work, we initialize the clock (initial_time) and the
  // counter (initial_ctr).  To compute the initial time, we can check
  // the number of ticks that have elapsed, and compute the delta.
  //
  // To avoid any drift, we periodically resync the counters to the system
  // clock.
  while(true) {
    TimeTicks ticks = TimeTicks::Now();

    // Calculate the time elapsed since we started our timer
    TimeDelta elapsed = ticks - initial_ticks;

    // Check if enough time has elapsed that we need to resync the clock.
    if (elapsed.InMilliseconds() > kMaxMillisecondsToAvoidDrift) {
      InitializeClock();
      continue;
    }

    return Time(elapsed + Time(initial_time));
  }
}

// static
Time Time::NowFromSystemTime() {
  // Force resync.
  InitializeClock();
  return Time(initial_time);
}

// static
Time Time::FromFileTime(FILETIME ft) {
  return Time(FileTimeToMicroseconds(ft));
}

FILETIME Time::ToFileTime() const {
  FILETIME utc_ft;
  MicrosecondsToFileTime(us_, &utc_ft);
  return utc_ft;
}

// static
Time Time::FromExploded(bool is_local, const Exploded& exploded) {
  // Create the system struct representing our exploded time. It will either be
  // in local time or UTC.
  SYSTEMTIME st;
  st.wYear = exploded.year;
  st.wMonth = exploded.month;
  st.wDayOfWeek = exploded.day_of_week;
  st.wDay = exploded.day_of_month;
  st.wHour = exploded.hour;
  st.wMinute = exploded.minute;
  st.wSecond = exploded.second;
  st.wMilliseconds = exploded.millisecond;

  // Convert to FILETIME.
  FILETIME ft;
  if (!SystemTimeToFileTime(&st, &ft)) {
    NOTREACHED() << "Unable to convert time";
    return Time(0);
  }

  // Ensure that it's in UTC.
  if (is_local) {
    FILETIME utc_ft;
    LocalFileTimeToFileTime(&ft, &utc_ft);
    return Time(FileTimeToMicroseconds(utc_ft));
  }
  return Time(FileTimeToMicroseconds(ft));
}

void Time::Explode(bool is_local, Exploded* exploded) const {
  // FILETIME in UTC.
  FILETIME utc_ft;
  MicrosecondsToFileTime(us_, &utc_ft);

  // FILETIME in local time if necessary.
  BOOL success = TRUE;
  FILETIME ft;
  if (is_local)
    success = FileTimeToLocalFileTime(&utc_ft, &ft);
  else
    ft = utc_ft;

  // FILETIME in SYSTEMTIME (exploded).
  SYSTEMTIME st;
  if (!success || !FileTimeToSystemTime(&ft, &st)) {
    NOTREACHED() << "Unable to convert time, don't know why";
    ZeroMemory(exploded, sizeof(*exploded));
    return;
  }

  exploded->year = st.wYear;
  exploded->month = st.wMonth;
  exploded->day_of_week = st.wDayOfWeek;
  exploded->day_of_month = st.wDay;
  exploded->hour = st.wHour;
  exploded->minute = st.wMinute;
  exploded->second = st.wSecond;
  exploded->millisecond = st.wMilliseconds;
}

// TimeTicks ------------------------------------------------------------------
namespace {

// We define a wrapper to adapt between the __stdcall and __cdecl call of the
// mock function, and to avoid a static constructor.  Assigning an import to a
// function pointer directly would require setup code to fetch from the IAT.
DWORD timeGetTimeWrapper() {
  return timeGetTime();
}


DWORD (*tick_function)(void) = &timeGetTimeWrapper;

// We use timeGetTime() to implement TimeTicks::Now().  This can be problematic
// because it returns the number of milliseconds since Windows has started,
// which will roll over the 32-bit value every ~49 days.  We try to track
// rollover ourselves, which works if TimeTicks::Now() is called at least every
// 49 days.
class NowSingleton {
 public:
  NowSingleton()
    : rollover_(TimeDelta::FromMilliseconds(0)),
      last_seen_(0) {
  }

  TimeDelta Now() {
    AutoLock locked(lock_);
    // We should hold the lock while calling tick_function to make sure that
    // we keep our last_seen_ stay correctly in sync.
    DWORD now = tick_function();
    if (now < last_seen_)
      rollover_ += TimeDelta::FromMilliseconds(GG_LONGLONG(0x100000000));  // ~49.7 days.
    last_seen_ = now;
    return TimeDelta::FromMilliseconds(now) + rollover_;
  }

 private:
  Lock lock_;  // To protected last_seen_ and rollover_.
  TimeDelta rollover_;  // Accumulation of time lost due to rollover.
  DWORD last_seen_;  // The last timeGetTime value we saw, to detect rollover.

  DISALLOW_COPY_AND_ASSIGN(NowSingleton);
};

// Overview of time counters:
// (1) CPU cycle counter. (Retrieved via RDTSC)
// The CPU counter provides the highest resolution time stamp and is the least
// expensive to retrieve. However, the CPU counter is unreliable and should not
// be used in production. Its biggest issue is that it is per processor and it
// is not synchronized between processors. Also, on some computers, the counters
// will change frequency due to thermal and power changes, and stop in some
// states.
//
// (2) QueryPerformanceCounter (QPC). The QPC counter provides a high-
// resolution (100 nanoseconds) time stamp but is comparatively more expensive
// to retrieve. What QueryPerformanceCounter actually does is up to the HAL.
// (with some help from ACPI).
// According to http://blogs.msdn.com/oldnewthing/archive/2005/09/02/459952.aspx
// in the worst case, it gets the counter from the rollover interrupt on the
// programmable interrupt timer. In best cases, the HAL may conclude that the
// RDTSC counter runs at a constant frequency, then it uses that instead. On
// multiprocessor machines, it will try to verify the values returned from
// RDTSC on each processor are consistent with each other, and apply a handful
// of workarounds for known buggy hardware. In other words, QPC is supposed to
// give consistent result on a multiprocessor computer, but it is unreliable in
// reality due to bugs in BIOS or HAL on some, especially old computers.
// With recent updates on HAL and newer BIOS, QPC is getting more reliable but
// it should be used with caution.
//
// (3) System time. The system time provides a low-resolution (typically 10ms
// to 55 milliseconds) time stamp but is comparatively less expensive to
// retrieve and more reliable.
class HighResNowSingleton {
 public:
  HighResNowSingleton()
    : ticks_per_microsecond_(0.0),
      skew_(0) {
    InitializeClock();

    // On Athlon X2 CPUs (e.g. model 15) QueryPerformanceCounter is
    // unreliable.  Fallback to low-res clock.
    base::CPU cpu;
    if (cpu.vendor_name() == "AuthenticAMD" && cpu.family() == 15)
      DisableHighResClock();
  }

  bool IsUsingHighResClock() {
    return ticks_per_microsecond_ != 0.0;
  }

  void DisableHighResClock() {
    ticks_per_microsecond_ = 0.0;
  }

  TimeDelta Now() {
    // Our maximum tolerance for QPC drifting.
    const int kMaxTimeDrift = 50 * Time::kMicrosecondsPerMillisecond;

    if (IsUsingHighResClock()) {
      int64_t now = UnreliableNow();

      // Verify that QPC does not seem to drift.
      DCHECK(now - ReliableNow() - skew_ < kMaxTimeDrift);

      return TimeDelta::FromMicroseconds(now);
    }

    // Just fallback to the slower clock.
    return Singleton<NowSingleton>::get()->Now();
  }

 private:
  // Synchronize the QPC clock with GetSystemTimeAsFileTime.
  void InitializeClock() {
    LARGE_INTEGER ticks_per_sec = {{0}};
    if (!QueryPerformanceFrequency(&ticks_per_sec))
      return;  // Broken, we don't guarantee this function works.
    ticks_per_microsecond_ = static_cast<float>(ticks_per_sec.QuadPart) /
      static_cast<float>(Time::kMicrosecondsPerSecond);

    skew_ = UnreliableNow() - ReliableNow();
  }

  // Get the number of microseconds since boot in a reliable fashion
  int64_t UnreliableNow() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<int64_t>(now.QuadPart / ticks_per_microsecond_);
  }

  // Get the number of microseconds since boot in a reliable fashion
  int64_t ReliableNow() {
    return Singleton<NowSingleton>::get()->Now().InMicroseconds();
  }

  // Cached clock frequency -> microseconds. This assumes that the clock
  // frequency is faster than one microsecond (which is 1MHz, should be OK).
  float ticks_per_microsecond_;  // 0 indicates QPF failed and we're broken.
  int64_t skew_;  // Skew between lo-res and hi-res clocks (for debugging).

  DISALLOW_COPY_AND_ASSIGN(HighResNowSingleton);
};

}  // namespace

// static
TimeTicks::TickFunctionType TimeTicks::SetMockTickFunction(
    TickFunctionType ticker) {
  TickFunctionType old = tick_function;
  tick_function = ticker;
  return old;
}

// static
TimeTicks TimeTicks::Now() {
  return TimeTicks() + Singleton<NowSingleton>::get()->Now();
}

// static
TimeTicks TimeTicks::HighResNow() {
  return TimeTicks() + Singleton<HighResNowSingleton>::get()->Now();
}
