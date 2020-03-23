/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// In the parent and middleman processes, a watchdog thread is spawned that
// captures JS stacks for long-running tasks on the main thread.

#include "Thread.h"

namespace JS {

extern JS_PUBLIC_API bool DescribeScriptedCallerAtIndex(
    JSContext* cx, size_t index, AutoFilename* filename = nullptr,
    unsigned* lineno = nullptr, unsigned* column = nullptr);

} // namespace JS

namespace mozilla {
namespace recordreplay {

// Only accessed on main thread.
static bool gHasWatchdogThread;
static nsAutoCString gWatchdogText;

static Monitor* gWatchdogMonitor;

// Protected by gWatchdogMonitor.
static bool gEventRunning;
static bool gWatchdogIdle;
static TimeStamp gLastInterrupt;

// JSContext for the main thread's runtime.
static JSContext* gMainThreadContext;

static AtomicBool gShouldInterrupt;

static bool InterruptCallback(JSContext* aCx) {
  if (!gShouldInterrupt) {
    return true;
  }
  gShouldInterrupt = false;

  gWatchdogText.Append("Interrupt\n");
  for (unsigned index = 0;; index++) {
    AutoFilename filename;
    unsigned lineno, column;
    if (!DescribeScriptedCallerAtIndex(aCx, index, &filename, &lineno, &column)) {
      break;
    }
    gWatchdogText.AppendPrintf("Frame %u: %s:%u:%u\n", index, filename.get(), lineno, column);
  }
  gWatchdogText.Append("\n");

  return true;
}

static const size_t PollingIntervalMs = 100;

static void WatchdogMain(void*) {
  MonitorAutoLock lock(*gWatchdogMonitor);
  while (true) {
    if (gEventRunning) {
      TimeStamp now = TimeStamp::Now();
      if ((now - gLastInterrupt).ToMilliseconds() >= PollingIntervalMs) {
        gShouldInterrupt = true;
        JS_RequestInterruptCallback(gMainThreadContext);
        gLastInterrupt = now;
      }

      TimeStamp deadline = now + TimeDuration::FromMilliseconds(PollingIntervalMs);
      gWatchdogMonitor->WaitUntil(deadline);
    } else {
      gWatchdogIdle = true;
      gWatchdogMonitor->Wait();
    }
  }
}

static bool UseWatchdog() {
  return XRE_IsParentProcess() || IsMiddleman() || HasDivergedFromRecording();
}

void BeginRunEvent(const TimeStamp& aNow) {
  if (!UseWatchdog()) {
    return;
  }

  if (!gHasWatchdogThread) {
    AutoSafeJSContext cx;
    gMainThreadContext = cx;

    if (!JS_AddInterruptCallback(cx, InterruptCallback)) {
      MOZ_CRASH("BeginRunEvent");
    }

    AutoEnsurePassThroughThreadEvents pt;

    gWatchdogMonitor = new Monitor();
    Thread::SpawnNonRecordedThread(WatchdogMain, nullptr);
    gHasWatchdogThread = true;
  }

  MonitorAutoLock lock(*gWatchdogMonitor);
  gEventRunning = true;
  gLastInterrupt = aNow;
  if (gWatchdogIdle) {
    gWatchdogMonitor->Notify();
  }
}

void EndRunEvent() {
  if (!UseWatchdog()) {
    return;
  }

  if (gWatchdogText.Length() != 0) {
    parent::AddToLog(NS_ConvertUTF8toUTF16(gWatchdogText));
    gWatchdogText.SetLength(0);
  }

  MonitorAutoLock lock(*gWatchdogMonitor);
  gEventRunning = false;
}

} // namespace recordreplay
} // namespace mozilla

