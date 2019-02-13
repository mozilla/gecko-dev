/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_H
#define GECKO_TASK_TRACER_H

#include "nsCOMPtr.h"

/**
 * TaskTracer provides a way to trace the correlation between different tasks
 * across threads and processes. Unlike sampling based profilers, TaskTracer can
 * tell you where a task is dispatched from, what its original source was, how
 * long it waited in the event queue, and how long it took to execute.
 *
 * Source Events are usually some kinds of I/O events we're interested in, such
 * as touch events, timer events, network events, etc. When a source event is
 * created, TaskTracer records the entire chain of Tasks and nsRunnables as they
 * are dispatched to different threads and processes. It records latency,
 * execution time, etc. for each Task and nsRunnable that chains back to the
 * original source event.
 */

class Task;
class nsIRunnable;
class nsCString;
template <class> class nsTArray;

namespace mozilla {

class TimeStamp;

namespace tasktracer {

enum {
  FORKED_AFTER_NUWA = 1 << 0
};

enum SourceEventType {
  Unknown = 0,
  Touch,
  Mouse,
  Key,
  Bluetooth,
  Unixsocket,
  Wifi
};

class AutoSourceEvent
{
public:
  AutoSourceEvent(SourceEventType aType);
  ~AutoSourceEvent();
};

void InitTaskTracer(uint32_t aFlags = 0);
void ShutdownTaskTracer();

// Add a label to the currently running task, aFormat is the message to log,
// followed by corresponding parameters.
void AddLabel(const char* aFormat, ...);

void StartLogging();
void StopLogging();
nsTArray<nsCString>* GetLoggedData(TimeStamp aStartTime);

// Returns the timestamp when Task Tracer is enabled in this process.
const PRTime GetStartTime();

/**
 * Internal functions.
 */

Task* CreateTracedTask(Task* aTask);

already_AddRefed<nsIRunnable> CreateTracedRunnable(nsIRunnable* aRunnable);

// Free the TraceInfo allocated on a thread's TLS. Currently we are wrapping
// tasks running on nsThreads and base::thread, so FreeTraceInfo is called at
// where nsThread and base::thread release themselves.
void FreeTraceInfo();

const char* GetJSLabelPrefix();

} // namespace tasktracer
} // namespace mozilla.

#endif
