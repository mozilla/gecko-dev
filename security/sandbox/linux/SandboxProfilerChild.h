/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_PROFILER_CHILD_H
#define SANDBOX_PROFILER_CHILD_H

#include "SandboxProfiler.h"

#if defined(HAVE_REPORT_UPROFILER_PARENT)
#error Cannot include SandboxProfilerChild.h when already included SandboxProfilerParent.h
#endif

#define HAVE_REPORT_UPROFILER_CHILD

namespace mozilla {

/* static */
bool SandboxProfiler::Init() {
  MOZ_ASSERT(!sInSignalContext,
             "SandboxProfiler::Init called in SIGSYS handler");

  if (!uprofiler_initted) {
    UPROFILER_GET(g);
    if (g && !g(&uprofiler)) {
      return false;
    }
  }

  MOZ_ASSERT(uprofiler.simple_event_marker_with_stack !=
                 simple_event_marker_with_stack_noop,
             "Marker sym OK");
  MOZ_ASSERT(uprofiler.native_backtrace != native_backtrace_noop,
             "Backtrace sym OK");

  if (uprofiler.native_backtrace &&
      uprofiler.native_backtrace != native_backtrace_noop) {
    uprofiler_initted = true;
    return true;
  }

  return false;
}

}  // namespace mozilla

#endif  // SANDBOX_PROFILER_CHILD_H
