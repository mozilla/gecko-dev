/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_PROFILER_PARENT_H
#define SANDBOX_PROFILER_PARENT_H

#include "SandboxProfiler.h"
#include "SandboxInfo.h"

#if defined(HAVE_REPORT_UPROFILER_CHILD)
#error Cannot include SandboxProfilerParent.h when already included SandboxProfilerChild.h
#endif

#define HAVE_REPORT_UPROFILER_PARENT

namespace mozilla {

struct UprofilerFuncPtrs uprofiler = {
    .register_thread = uprofiler_register_thread,
    .unregister_thread = uprofiler_unregister_thread,
    .simple_event_marker = uprofiler_simple_event_marker,
    .simple_event_marker_capture_stack =
        uprofiler_simple_event_marker_capture_stack,
    .simple_event_marker_with_stack = uprofiler_simple_event_marker_with_stack,
    .backtrace_into_buffer = uprofiler_backtrace_into_buffer,
    .native_backtrace = uprofiler_native_backtrace,
    .is_active = uprofiler_is_active,
    .feature_active = uprofiler_feature_active,
};

bool uprofiler_initted = true;

#if defined(DEBUG)
// On the parent side there is no sandbox so calls don't have to care about that
thread_local Atomic<bool> sInSignalContext = Atomic<bool>(false);
#endif  // defined(DEBUG)

/* static */
void SandboxProfiler::ReportAudit(const char* aKind, const char* aOp,
                                  int aFlags, uint64_t aId, int aPerms,
                                  const char* aPath, pid_t aPid) {
  if (!Active()) {
    return;
  }

  std::array arg_names = {"id", "op", "rflags", "path", "pid"};
  std::array arg_types = {
      TRACE_VALUE_TYPE_UINT,    // id
      TRACE_VALUE_TYPE_STRING,  // op
      TRACE_VALUE_TYPE_UINT,    // rflags
      TRACE_VALUE_TYPE_STRING,  // path
      TRACE_VALUE_TYPE_UINT     // pid
  };

  std::array arg_values = {static_cast<unsigned long long>(aId),
                           reinterpret_cast<unsigned long long>(aOp),
                           static_cast<unsigned long long>(aFlags),
                           reinterpret_cast<unsigned long long>(aPath),
                           static_cast<unsigned long long>(aPid)};

  Report(aKind, arg_names, arg_types, arg_values, nullptr);
}

/*  static */
void SandboxProfiler::ReportLog(const char* buf) {
  if (!Active()) {
    return;
  }

  if (!SandboxInfo::Get().Test(SandboxInfo::kVerbose) &&
      !SandboxInfo::Get().Test(SandboxInfo::kVerboseTests)) {
    return;
  }

  std::array arg_names = {"log"};
  std::array arg_types = {
      TRACE_VALUE_TYPE_STRING,
  };
  std::array arg_values = {reinterpret_cast<unsigned long long>(buf)};

  Report("SandboxBroker::Log", arg_names, arg_types, arg_values, nullptr);
}

}  // namespace mozilla

#endif  // SANDBOX_PROFILER_PARENT_H
