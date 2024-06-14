/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <time.h>
#include <unistd.h>
#include <cstring>

#include "SandboxInfo.h"

#include "SandboxProfilerChild.h"
#include "SandboxProfiler.h"

#include "mozilla/Atomics.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/PodOperations.h"

namespace mozilla {

#if defined(DEBUG)
thread_local Atomic<bool> sInSignalContext = Atomic<bool>(false);

AutoForbidSignalContext::AutoForbidSignalContext() { sInSignalContext = true; }

AutoForbidSignalContext::~AutoForbidSignalContext() {
  sInSignalContext = false;
}
#endif  // defined(DEBUG)

static StaticAutoPtr<SandboxProfiler> gProfiler;
static StaticAutoPtr<SandboxProfilerQueue> gSyscallsQueue;
static StaticAutoPtr<SandboxProfilerQueue> gLogsQueue;

static Atomic<bool> isShutdown = Atomic<bool>(false);

// Those function pointers are set by the main thread, and subsequently read by
// all other thread, in particular in SIGSYS handlers.
struct UprofilerFuncPtrs uprofiler;
bool uprofiler_initted = false;

static bool const SANDBOX_PROFILER_DEBUG = false;

// Semaphore that we use to signal the SandboxProfilerEmitter thread when data
// has been pushed to the SandboxProfilerQueue
static sem_t gRequest;

// This is only be called on main thread, and not within SIGSYS context
//
// We might be called either from the profiler-started notification observer in
// which case the !Active() call is not useful, but also directly from Sandbox'
// SandboxLateInit where we want to verify if we are not already active: that
// can happen if the user started the profiler via MOZ_PROFILER_STARTUP=1

/* static */
void SandboxProfiler::Create() {
  MOZ_ASSERT(!sInSignalContext,
             "SandboxProfiler::Create called in SIGSYS handler");

  if (!Init()) {
    return;
  }

  if (!Active()) {
    return;
  }

  if (!gSyscallsQueue) {
    gSyscallsQueue = new SandboxProfilerQueue(15);
  }

  if (!gLogsQueue) {
    gLogsQueue = new SandboxProfilerQueue(15);
  }

  if (!gProfiler) {
    gProfiler = new SandboxProfiler();
  }
}

SandboxProfiler::SandboxProfiler() {
  sem_init(&gRequest, /* pshared */ 0, /* value */ 0);
  mThreadLogs = std::thread(&SandboxProfiler::ThreadMain, this,
                            "SandboxProfilerEmitterLogs", gLogsQueue.get());
  mThreadSyscalls =
      std::thread(&SandboxProfiler::ThreadMain, this,
                  "SandboxProfilerEmitterSyscalls", gSyscallsQueue.get());
}

/* static */
void SandboxProfiler::Shutdown() {
  isShutdown = true;

  if (gProfiler) {
    // Unblock all
    SandboxProfiler::Signal(&gRequest);
  }

  gProfiler = nullptr;
  gSyscallsQueue = nullptr;
  gLogsQueue = nullptr;
}

SandboxProfiler::~SandboxProfiler() {
  if (mThreadLogs.joinable()) {
    mThreadLogs.join();
  }

  if (mThreadSyscalls.joinable()) {
    mThreadSyscalls.join();
  }

  sem_destroy(&gRequest);
}

/* static */
bool SandboxProfiler::ActiveWithQueue(SandboxProfilerQueue* aQueue) {
  return !isShutdown && gProfiler && Active() && aQueue;
}

/* static */
void SandboxProfiler::Signal(sem_t* aSem) {
  if (sem_post(aSem) < 0) {
    if constexpr (SANDBOX_PROFILER_DEBUG) {
      fprintf(stderr, "[%d] %s SEM_POST errno=%d\n", getpid(),
              __PRETTY_FUNCTION__, errno);
    }
  }
}

/* static */
int SandboxProfiler::TimedWait(sem_t* aSem, int aSec, int aNSec) {
  struct timespec timeout = {.tv_sec = 0, .tv_nsec = 0};
  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += aSec;
  timeout.tv_nsec += aNSec;
  return sem_timedwait(aSem, &timeout);
}

/* static */
void SandboxProfiler::ReportInit(const void* top) {
  if (!ActiveWithQueue(gSyscallsQueue)) {
    return;
  }

  SandboxProfilerPayload payload = {
      .mStack = NativeStack{.mCount = 0},
      .mType = SandboxProfilerPayloadType::Init,
  };
  uprofiler.native_backtrace(top, &payload.mStack);

  MOZ_ASSERT(gSyscallsQueue, "Queue is valid for Send() from ReportInit()");
  if (!gSyscallsQueue) {
    if constexpr (SANDBOX_PROFILER_DEBUG) {
      fprintf(stderr,
              "[%d] WARNING: Hello PRODUCER: gSyscallsQueue disappeared\n",
              getpid());
    }
    return;
  }

  int rv = gSyscallsQueue->Send(payload);
  if (rv == 0) {
    if constexpr (SANDBOX_PROFILER_DEBUG) {
      fprintf(stderr,
              "[%d] WARNING: Hello PRODUCER: one stack mCount=%zu DROPPED\n",
              getpid(), payload.mStack.mCount);
    }
  }

  SandboxProfiler::Signal(&gRequest);
}

void SandboxProfiler::ReportInitImpl(SandboxProfilerPayload& payload,
                                     ProfileChunkedBuffer& buffer) {
  const char buf[] = "uprofiler init";
  std::array arg_names = {"init"};
  std::array arg_types = {
      TRACE_VALUE_TYPE_STRING,
  };
  std::array arg_values = {reinterpret_cast<unsigned long long>(buf)};

  Report("SandboxBroker::InitWithStack", arg_names, arg_types, arg_values,
         &buffer);
}

/* static */
void SandboxProfiler::ReportLog(const char* aBuf) {
  if (!ActiveWithQueue(gLogsQueue)) {
    return;
  }

  if (!SandboxInfo::Get().Test(SandboxInfo::kVerbose) &&
      !SandboxInfo::Get().Test(SandboxInfo::kVerboseTests)) {
    return;
  }

  SandboxProfilerPayload payload = {
      .mStack = NativeStack{.mCount = 0},
      .mType = SandboxProfilerPayloadType::Log,
  };

  const size_t bufLen = strnlen(aBuf, PATH_MAX);
  PodCopy(payload.mPath, aBuf, bufLen);

  MOZ_ASSERT(gLogsQueue, "Queue is valid for Send() from ReportLog()");
  if (!gLogsQueue) {
    if constexpr (SANDBOX_PROFILER_DEBUG) {
      fprintf(stderr, "[%d] WARNING: Hello PRODUCER: gLogsQueue disappeared\n",
              getpid());
    }
    return;
  }

  int rv = gLogsQueue->Send(payload);
  if (rv == 0) {
    if constexpr (SANDBOX_PROFILER_DEBUG) {
      fprintf(stderr, "[%d] WARNING: Hello PRODUCER: one log stack DROPPED\n",
              getpid());
    }
  }

  SandboxProfiler::Signal(&gRequest);
}

void SandboxProfiler::ReportLogImpl(SandboxProfilerPayload& payload) {
  std::array arg_names = {"log"};
  std::array arg_types = {
      TRACE_VALUE_TYPE_STRING,
  };
  std::array arg_values = {
      reinterpret_cast<unsigned long long>(payload.mPath),
  };

  Report("SandboxBroker::Log", arg_names, arg_types, arg_values, nullptr);
}

/* static */
void SandboxProfiler::ReportRequest(const void* top, uint64_t aId,
                                    const char* aOp, int aFlags,
                                    const char* aPath, const char* aPath2,
                                    pid_t aPid) {
  if (!ActiveWithQueue(gSyscallsQueue)) {
    return;
  }

  // Take a stack, this should be safe to do in the context of SIGSYS
  SandboxProfilerPayload payload = {
      .mStack = NativeStack{.mCount = 0},
      .mId = aId,
      .mOp = aOp,
      .mFlags = aFlags,
      .mPid = aPid,
      .mType = SandboxProfilerPayloadType::Request,
  };

  if (aPath) {
    const size_t pathLen = strnlen(aPath, PATH_MAX);
    PodCopy(payload.mPath, aPath, pathLen);
  } else {
    payload.mPath[0] = '\0';
  }

  if (aPath2) {
    const size_t path2Len = strnlen(aPath2, PATH_MAX);
    PodCopy(payload.mPath2, aPath2, path2Len);
  } else {
    payload.mPath2[0] = '\0';
  }

  uprofiler.native_backtrace(top, &payload.mStack);

  MOZ_ASSERT(gSyscallsQueue, "Queue is valid for Send() from ReportRequest()");
  if (!gSyscallsQueue) {
    if constexpr (SANDBOX_PROFILER_DEBUG) {
      fprintf(stderr,
              "[%d] WARNING: Hello PRODUCER: gSyscallsQueue disappeared\n",
              getpid());
    }
    return;
  }

  int rv = gSyscallsQueue->Send(payload);
  if (rv == 0) {
    if constexpr (SANDBOX_PROFILER_DEBUG) {
      fprintf(stderr,
              "[%d] WARNING: Hello PRODUCER: one stack mCount=%zu DROPPED\n",
              getpid(), payload.mStack.mCount);
    }
  }

  SandboxProfiler::Signal(&gRequest);
}

void SandboxProfiler::ReportRequestImpl(SandboxProfilerPayload& payload,
                                        ProfileChunkedBuffer& buffer) {
  std::array arg_names = {"id", "op", "rflags", "path", "path2", "pid"};
  std::array arg_types = {
      TRACE_VALUE_TYPE_UINT,    // id
      TRACE_VALUE_TYPE_STRING,  // op
      TRACE_VALUE_TYPE_UINT,    // rflags
      TRACE_VALUE_TYPE_STRING,  // path
      TRACE_VALUE_TYPE_STRING,  // path2
      TRACE_VALUE_TYPE_UINT     // pid
  };

  std::array arg_values = {static_cast<unsigned long long>(payload.mId),
                           reinterpret_cast<unsigned long long>(payload.mOp),
                           static_cast<unsigned long long>(payload.mFlags),
                           reinterpret_cast<unsigned long long>(payload.mPath),
                           reinterpret_cast<unsigned long long>(payload.mPath2),
                           static_cast<unsigned long long>(payload.mPid)};

  Report("SandboxBrokerClient", arg_names, arg_types, arg_values, &buffer);
}

void SandboxProfiler::ThreadMain(const char* aThreadName,
                                 SandboxProfilerQueue* aQueue) {
  uprofiler.register_thread(aThreadName, CallerPC());
  SandboxProfilerPayload p;

  while (!isShutdown) {
    // On error dont consume ?
    if (SandboxProfiler::TimedWait(&gRequest, /* sec */ 0, /* nsec */ 1e8) <
        0) {
      if (errno == ETIMEDOUT) {
        continue;
      }
    }

    MOZ_ASSERT(aQueue, "Syscalls queue is valid for Recv()");
    if (!aQueue) {
      if constexpr (SANDBOX_PROFILER_DEBUG) {
        fprintf(stderr,
                "[%d] WARNING: Hello CONSUMER [%s]: aQueue disappeared\n",
                getpid(), aThreadName);
      }
      continue;
    }

    int deq = aQueue->Recv(&p);
    if (deq > 0) {
      switch (p.mType) {
        case SandboxProfilerPayloadType::Init:
        case SandboxProfilerPayloadType::Request: {
          ProfileBufferChunkManagerSingle chunkManager{
              mozilla::ProfileBufferChunkManager::scExpectedMaximumStackSize};
          ProfileChunkedBuffer chunkedBuffer{
              ProfileChunkedBuffer::ThreadSafety::WithoutMutex, chunkManager};
          uprofiler.backtrace_into_buffer(&p.mStack, &chunkedBuffer);

          switch (p.mType) {
            case SandboxProfilerPayloadType::Init:
              ReportInitImpl(p, chunkedBuffer);
              break;

            case SandboxProfilerPayloadType::Request:
              ReportRequestImpl(p, chunkedBuffer);
              break;

            default:
              // impossible?
              MOZ_ASSERT_UNREACHABLE("Should have been Init/Request");
              break;
          }
        } break;

        case SandboxProfilerPayloadType::Log:
          ReportLogImpl(p);
          break;

        default:
          fprintf(stderr, "[%d] mType=%hhu\n", getpid(),
                  static_cast<uint8_t>(p.mType));
          MOZ_CRASH("Unsupported type");
          break;
      }
    }
  }

  uprofiler.unregister_thread();
}

}  // namespace mozilla
