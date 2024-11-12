/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_PROFILER_H
#define SANDBOX_PROFILER_H

#include <thread>
#include <utility>
#include <linux/limits.h>
#include <semaphore.h>

#include "mozilla/UniquePtr.h"
#include "ProfilerNativeStack.h"
#include "MicroGeckoProfiler.h"

#include "mozilla/ProfileChunkedBuffer.h"

#include "mozilla/ArrayUtils.h"

#include "mozilla/MPSCQueue.h"

#if defined(HAVE_REPORT_UPROFILER_PARENT) && \
    defined(HAVE_REPORT_UPROFILER_CHILD)
#  error Cannot include SandboxProfilerChild.h AND SandboxProfilerParent.h
#endif

// stolen from GeckoTraceEvent.h which is not public
static constexpr uint8_t TRACE_VALUE_TYPE_UINT = 2;
static constexpr uint8_t TRACE_VALUE_TYPE_STRING = 6;

namespace mozilla {

#if defined(DEBUG)
extern thread_local Atomic<bool> sInSignalContext;

class MOZ_STACK_CLASS AutoForbidSignalContext {
 public:
  AutoForbidSignalContext();
  ~AutoForbidSignalContext();
};
#endif  // defined(DEBUG)

enum class SandboxProfilerPayloadType : uint8_t { Init, Request, Log };

using SandboxProfilerPayload = struct {
  NativeStack mStack;
  uint64_t mId;
  const char* mOp;
  int mFlags;
  char mPath[PATH_MAX];
  char mPath2[PATH_MAX];
  pid_t mPid;
  SandboxProfilerPayloadType mType;
};

using SandboxProfilerQueue = MPSCQueue<SandboxProfilerPayload>;

extern struct UprofilerFuncPtrs uprofiler;
extern bool uprofiler_initted;

class SandboxProfiler final {
 public:
  SandboxProfiler();
  ~SandboxProfiler();

  // Needs to be accessible in both child (within libmozsandbox.so) and parent
  // (within libxul.so) it's easier and less of a mess if this is done from the
  // header which can more easily be included by both sides.
  static bool Active() {
    if (!uprofiler_initted) {
      return false;
    }

    if (!uprofiler.is_active || uprofiler.is_active == is_active_noop) {
      return false;
    }

    return uprofiler.is_active();
  }

  static void Shutdown();

  // Should NOT BE CALLED UNDER SIGSYS, this is here to make sure we do the
  // dlopen()/dlsym() on the main thread so it is available for later use on
  // others threads. We expect that only stack trace would be collected under
  // SIGSYS context, and the rest of the profiler marker would happen on another
  // safer thread.
  static inline bool Init();

  static void Create();
  static void inline ReportAudit(const char* aKind, const char* aOp, int aFlags,
                                 uint64_t aId, int aPerms, const char* aPath,
                                 pid_t aPid);
  static void ReportRequest(const void* top, uint64_t aId, const char* aOp,
                            int aFlags, const char* aPath, const char* aPath2,
                            pid_t aPid);
  static void ReportInit(const void* top);
  static void ReportLog(const char* buf);

 private:
  std::thread mThreadLogs;
  std::thread mThreadSyscalls;

  // For child and parent, same reason as Active() above
  template <typename N, typename T, typename V, size_t len>
  static constexpr void Report(const char* aKind, std::array<N, len> aArgNames,
                               std::array<T, len> aArgTypes,
                               std::array<V, len> aArgValues, void* aStack) {
    if (!Active()) {
      return;
    }

    if (!uprofiler_initted) {
      fprintf(stderr, "[%d] no uprofiler, skip\n", getpid());
      return;
    }

    MOZ_ASSERT(!sInSignalContext,
               "SandboxProfiler::Report called in SIGSYS handler");

    if (aStack) {
      uprofiler.simple_event_marker_with_stack(
          aKind, 'S', 'I', aArgNames.size(), aArgNames.data(), aArgTypes.data(),
          aArgValues.data(), aStack);
    } else {
      uprofiler.simple_event_marker(aKind, 'S', 'I', aArgNames.size(),
                                    aArgNames.data(), aArgTypes.data(),
                                    aArgValues.data());
    }
  }

  // Verify that:
  //  - Not in shutdown
  //  - SandboxProfiler exists
  //  - Profiler is active
  //  - aQueue exists
  static bool ActiveWithQueue(SandboxProfilerQueue* aQueue);

  // Rely on semaphore to handle consuming the queue:
  //  - One semaphore
  //  - Gets SIGNAL'd when a payload has been pushed to the SandboxProfilerQueue
  //  - SandboxProfiler dedicated thread WAIT's on the semaphore, gets unblocked
  //    on signal
  //  - Timed wait allows to have a timeout to ensure the thread has a chance to
  //    release its resources on shutdown
  //  - Semaphore's sem_post() is safe to use in a signal context
  //  - Using semaphores allows to wake the SandboxProfiler dedicated thread
  //    only when needed and avoid using sleep()
  static void Signal(sem_t* aSem);
  static int TimedWait(sem_t* aSem, int aSec, int aNSec);

  void ThreadMain(const char* aThreadName, SandboxProfilerQueue* aQueue);
  void ReportInitImpl(SandboxProfilerPayload& payload,
                      ProfileChunkedBuffer& buffer);
  void ReportLogImpl(SandboxProfilerPayload& payload);
  void ReportRequestImpl(SandboxProfilerPayload& payload,
                         ProfileChunkedBuffer& buffer);
};

}  // namespace mozilla

#endif  // SANDBOX_PROFILER_H
