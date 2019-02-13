/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Sandbox.h"

#include "LinuxCapabilities.h"
#include "LinuxSched.h"
#include "SandboxChroot.h"
#include "SandboxFilter.h"
#include "SandboxInternal.h"
#include "SandboxLogging.h"
#include "SandboxUtil.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include "mozilla/Atomics.h"
#include "mozilla/SandboxInfo.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"
#include "sandbox/linux/bpf_dsl/dump_bpf.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#if defined(ANDROID)
#include "sandbox/linux/services/android_ucontext.h"
#endif
#include "sandbox/linux/services/linux_syscalls.h"

#ifdef MOZ_ASAN
// Copy libsanitizer declarations to avoid depending on ASAN headers.
// See also bug 1081242 comment #4.
extern "C" {
namespace __sanitizer {
// Win64 uses long long, but this is Linux.
typedef signed long sptr;
} // namespace __sanitizer

typedef struct {
  int coverage_sandboxed;
  __sanitizer::sptr coverage_fd;
  unsigned int coverage_max_block_size;
} __sanitizer_sandbox_arguments;

MOZ_IMPORT_API void
__sanitizer_sandbox_on_notify(__sanitizer_sandbox_arguments *args);
} // extern "C"
#endif // MOZ_ASAN

namespace mozilla {

#ifdef ANDROID
SandboxCrashFunc gSandboxCrashFunc;
#endif

#ifdef MOZ_GMP_SANDBOX
// For media plugins, we can start the sandbox before we dlopen the
// module, so we have to pre-open the file and simulate the sandboxed
// open().
static SandboxOpenedFile gMediaPluginFile;
#endif

static UniquePtr<SandboxChroot> gChrootHelper;
static void (*gChromiumSigSysHandler)(int, siginfo_t*, void*);

// Test whether a ucontext, interpreted as the state after a syscall,
// indicates the given error.  See also sandbox::Syscall::PutValueInUcontext.
static bool
ContextIsError(const ucontext_t *aContext, int aError)
{
  // Avoid integer promotion warnings.  (The unary addition makes
  // the decltype not evaluate to a reference type.)
  typedef decltype(+SECCOMP_RESULT(aContext)) reg_t;

#ifdef __mips__
  return SECCOMP_PARM4(aContext) != 0
    && SECCOMP_RESULT(aContext) == static_cast<reg_t>(aError);
#else
  return SECCOMP_RESULT(aContext) == static_cast<reg_t>(-aError);
#endif
}

/**
 * This is the SIGSYS handler function.  It delegates to the Chromium
 * TrapRegistry handler (see InstallSigSysHandler, below) and, if the
 * trap handler installed by the policy would fail with ENOSYS,
 * crashes the process.  This allows unintentional policy failures to
 * be reported as crash dumps and fixed.  It also logs information
 * about the failed system call.
 *
 * Note that this could be invoked in parallel on multiple threads and
 * that it could be in async signal context (e.g., intercepting an
 * open() called from an async signal handler).
 */
static void
SigSysHandler(int nr, siginfo_t *info, void *void_context)
{
  ucontext_t *ctx = static_cast<ucontext_t*>(void_context);
  // This shouldn't ever be null, but the Chromium handler checks for
  // that and refrains from crashing, so let's not crash release builds:
  MOZ_DIAGNOSTIC_ASSERT(ctx);
  if (!ctx) {
    return;
  }

  // Save a copy of the context before invoking the trap handler,
  // which will overwrite one or more registers with the return value.
  ucontext_t savedCtx = *ctx;

  gChromiumSigSysHandler(nr, info, ctx);
  if (!ContextIsError(ctx, ENOSYS)) {
    return;
  }

  pid_t pid = getpid();
  unsigned long syscall_nr = SECCOMP_SYSCALL(&savedCtx);
  unsigned long args[6];
  args[0] = SECCOMP_PARM1(&savedCtx);
  args[1] = SECCOMP_PARM2(&savedCtx);
  args[2] = SECCOMP_PARM3(&savedCtx);
  args[3] = SECCOMP_PARM4(&savedCtx);
  args[4] = SECCOMP_PARM5(&savedCtx);
  args[5] = SECCOMP_PARM6(&savedCtx);

  // TODO, someday when this is enabled on MIPS: include the two extra
  // args in the error message.
  SANDBOX_LOG_ERROR("seccomp sandbox violation: pid %d, syscall %lu,"
                    " args %lu %lu %lu %lu %lu %lu.  Killing process.",
                    pid, syscall_nr,
                    args[0], args[1], args[2], args[3], args[4], args[5]);

  // Bug 1017393: record syscall number somewhere useful.
  info->si_addr = reinterpret_cast<void*>(syscall_nr);

  gSandboxCrashFunc(nr, info, &savedCtx);
  _exit(127);
}

/**
 * This function installs the SIGSYS handler.  This is slightly
 * complicated because we want to use Chromium's handler to dispatch
 * to specific trap handlers defined in the policy, but we also need
 * the full original signal context to give to Breakpad for crash
 * dumps.  So we install Chromium's handler first, then retrieve its
 * address so our replacement can delegate to it.
 */
static void
InstallSigSysHandler(void)
{
  struct sigaction act;

  // Ensure that the Chromium handler is installed.
  unused << sandbox::Trap::Registry();

  // If the signal handling state isn't as expected, crash now instead
  // of crashing later (and more confusingly) when SIGSYS happens.

  if (sigaction(SIGSYS, nullptr, &act) != 0) {
    MOZ_CRASH("Couldn't read old SIGSYS disposition");
  }
  if ((act.sa_flags & SA_SIGINFO) != SA_SIGINFO) {
    MOZ_CRASH("SIGSYS not already set to a siginfo handler?");
  }
  MOZ_RELEASE_ASSERT(act.sa_sigaction);
  gChromiumSigSysHandler = act.sa_sigaction;
  act.sa_sigaction = SigSysHandler;
  // Currently, SA_NODEFER should already be set by the Chromium code,
  // but it's harmless to ensure that it's set:
  MOZ_ASSERT(act.sa_flags & SA_NODEFER);
  act.sa_flags |= SA_NODEFER;
  if (sigaction(SIGSYS, &act, nullptr) < 0) {
    MOZ_CRASH("Couldn't change SIGSYS disposition");
  }
}

/**
 * This function installs the syscall filter, a.k.a. seccomp.
 * PR_SET_NO_NEW_PRIVS ensures that it is impossible to grant more
 * syscalls to the process beyond this point (even after fork()).
 * SECCOMP_MODE_FILTER is the "bpf" mode of seccomp which allows
 * to pass a bpf program (in our case, it contains a syscall
 * whitelist).
 *
 * Reports failure by crashing.
 *
 * @see sock_fprog (the seccomp_prog).
 */
static void
InstallSyscallFilter(const sock_fprog *prog)
{
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_LOG_ERROR("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", strerror(errno));
    MOZ_CRASH("prctl(PR_SET_NO_NEW_PRIVS)");
  }

  if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, (unsigned long)prog, 0, 0)) {
    SANDBOX_LOG_ERROR("prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER) failed: %s",
                      strerror(errno));
    MOZ_CRASH("prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)");
  }
}

// Use signals for permissions that need to be set per-thread.
// The communication channel from the signal handler back to the main thread.
static mozilla::Atomic<int> gSetSandboxDone;
// Pass the filter itself through a global.
static sock_fprog gSetSandboxFilter;

// We have to dynamically allocate the signal number; see bug 1038900.
// This function returns the first realtime signal currently set to
// default handling (i.e., not in use), or 0 if none could be found.
//
// WARNING: if this function or anything similar to it (including in
// external libraries) is used on multiple threads concurrently, there
// will be a race condition.
static int
FindFreeSignalNumber()
{
  for (int signum = SIGRTMIN; signum <= SIGRTMAX; ++signum) {
    struct sigaction sa;

    if (sigaction(signum, nullptr, &sa) == 0 &&
        (sa.sa_flags & SA_SIGINFO) == 0 &&
        sa.sa_handler == SIG_DFL) {
      return signum;
    }
  }
  return 0;
}

// Returns true if sandboxing was enabled, or false if sandboxing
// already was enabled.  Crashes if sandboxing could not be enabled.
static bool
SetThreadSandbox()
{
  if (prctl(PR_GET_SECCOMP, 0, 0, 0, 0) == 0) {
    InstallSyscallFilter(&gSetSandboxFilter);
    return true;
  }
  return false;
}

static void
SetThreadSandboxHandler(int signum)
{
  // The non-zero number sent back to the main thread indicates
  // whether action was taken.
  if (SetThreadSandbox()) {
    gSetSandboxDone = 2;
  } else {
    gSetSandboxDone = 1;
  }
  // Wake up the main thread.  See the FUTEX_WAIT call, below, for an
  // explanation.
  syscall(__NR_futex, reinterpret_cast<int*>(&gSetSandboxDone),
          FUTEX_WAKE, 1);
}

static void
BroadcastSetThreadSandbox(UniquePtr<sock_filter[]> aProgram, size_t aProgLen)
{
  int signum;
  pid_t pid, tid, myTid;
  DIR *taskdp;
  struct dirent *de;

  // Note: this is an unsafe copy of the unique pointer, but it's
  // zeroed (and the signal handler that would access it is removed)
  // before the end of this function.
  gSetSandboxFilter.filter = aProgram.get();
  gSetSandboxFilter.len = static_cast<unsigned short>(aProgLen);
  MOZ_RELEASE_ASSERT(static_cast<size_t>(gSetSandboxFilter.len) == aProgLen);

  static_assert(sizeof(mozilla::Atomic<int>) == sizeof(int),
                "mozilla::Atomic<int> isn't represented by an int");
  pid = getpid();
  myTid = syscall(__NR_gettid);
  taskdp = opendir("/proc/self/task");
  if (taskdp == nullptr) {
    SANDBOX_LOG_ERROR("opendir /proc/self/task: %s\n", strerror(errno));
    MOZ_CRASH();
  }

  if (gChrootHelper) {
    gChrootHelper->Invoke();
    gChrootHelper = nullptr;
  }

  signum = FindFreeSignalNumber();
  if (signum == 0) {
    SANDBOX_LOG_ERROR("No available signal numbers!");
    MOZ_CRASH();
  }
  void (*oldHandler)(int);
  oldHandler = signal(signum, SetThreadSandboxHandler);
  if (oldHandler != SIG_DFL) {
    // See the comment on FindFreeSignalNumber about race conditions.
    SANDBOX_LOG_ERROR("signal %d in use by handler %p!\n", signum, oldHandler);
    MOZ_CRASH();
  }

  // In case this races with a not-yet-deprivileged thread cloning
  // itself, repeat iterating over all threads until we find none
  // that are still privileged.
  bool sandboxProgress;
  do {
    sandboxProgress = false;
    // For each thread...
    while ((de = readdir(taskdp))) {
      char *endptr;
      tid = strtol(de->d_name, &endptr, 10);
      if (*endptr != '\0' || tid <= 0) {
        // Not a task ID.
        continue;
      }
      if (tid == myTid) {
        // Drop this thread's privileges last, below, so we can
        // continue to signal other threads.
        continue;
      }
      // Reset the futex cell and signal.
      gSetSandboxDone = 0;
      if (syscall(__NR_tgkill, pid, tid, signum) != 0) {
        if (errno == ESRCH) {
          SANDBOX_LOG_ERROR("Thread %d unexpectedly exited.", tid);
          // Rescan threads, in case it forked before exiting.
          sandboxProgress = true;
          continue;
        }
        SANDBOX_LOG_ERROR("tgkill(%d,%d): %s\n", pid, tid, strerror(errno));
        MOZ_CRASH();
      }
      // It's unlikely, but if the thread somehow manages to exit
      // after receiving the signal but before entering the signal
      // handler, we need to avoid blocking forever.
      //
      // Using futex directly lets the signal handler send the wakeup
      // from an async signal handler (pthread mutex/condvar calls
      // aren't allowed), and to use a relative timeout that isn't
      // affected by changes to the system clock (not possible with
      // POSIX semaphores).
      //
      // If a thread doesn't respond within a reasonable amount of
      // time, but still exists, we crash -- the alternative is either
      // blocking forever or silently losing security, and it
      // shouldn't actually happen.
      static const int crashDelay = 10; // seconds
      struct timespec timeLimit;
      clock_gettime(CLOCK_MONOTONIC, &timeLimit);
      timeLimit.tv_sec += crashDelay;
      while (true) {
        static const struct timespec futexTimeout = { 0, 10*1000*1000 }; // 10ms
        // Atomically: if gSetSandboxDone == 0, then sleep.
        if (syscall(__NR_futex, reinterpret_cast<int*>(&gSetSandboxDone),
                  FUTEX_WAIT, 0, &futexTimeout) != 0) {
          if (errno != EWOULDBLOCK && errno != ETIMEDOUT && errno != EINTR) {
            SANDBOX_LOG_ERROR("FUTEX_WAIT: %s\n", strerror(errno));
            MOZ_CRASH();
          }
        }
        // Did the handler finish?
        if (gSetSandboxDone > 0) {
          if (gSetSandboxDone == 2) {
            sandboxProgress = true;
          }
          break;
        }
        // Has the thread ceased to exist?
        if (syscall(__NR_tgkill, pid, tid, 0) != 0) {
          if (errno == ESRCH) {
            SANDBOX_LOG_ERROR("Thread %d unexpectedly exited.", tid);
          }
          // Rescan threads, in case it forked before exiting.
          // Also, if it somehow failed in a way that wasn't ESRCH,
          // and still exists, that will be handled on the next pass.
          sandboxProgress = true;
          break;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > timeLimit.tv_sec ||
            (now.tv_sec == timeLimit.tv_sec &&
             now.tv_nsec > timeLimit.tv_nsec)) {
          SANDBOX_LOG_ERROR("Thread %d unresponsive for %d seconds."
                            "  Killing process.",
                            tid, crashDelay);
          MOZ_CRASH();
        }
      }
    }
    rewinddir(taskdp);
  } while (sandboxProgress);
  oldHandler = signal(signum, SIG_DFL);
  if (oldHandler != SetThreadSandboxHandler) {
    // See the comment on FindFreeSignalNumber about race conditions.
    SANDBOX_LOG_ERROR("handler for signal %d was changed to %p!",
                      signum, oldHandler);
    MOZ_CRASH();
  }
  unused << closedir(taskdp);
  // And now, deprivilege the main thread:
  SetThreadSandbox();
  gSetSandboxFilter.filter = nullptr;
}

// Common code for sandbox startup.
static void
SetCurrentProcessSandbox(UniquePtr<sandbox::bpf_dsl::Policy> aPolicy)
{
  MOZ_ASSERT(gSandboxCrashFunc);

  // Note: PolicyCompiler borrows the policy and registry for its
  // lifetime, but does not take ownership of them.
  sandbox::bpf_dsl::PolicyCompiler compiler(aPolicy.get(),
                                            sandbox::Trap::Registry());
  auto program = compiler.Compile();
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    sandbox::bpf_dsl::DumpBPF::PrintProgram(*program);
  }

  InstallSigSysHandler();

#ifdef MOZ_ASAN
  __sanitizer_sandbox_arguments asanArgs;
  asanArgs.coverage_sandboxed = 1;
  asanArgs.coverage_fd = -1;
  asanArgs.coverage_max_block_size = 0;
  __sanitizer_sandbox_on_notify(&asanArgs);
#endif

  // The syscall takes a C-style array, so copy the vector into one.
  UniquePtr<sock_filter[]> flatProgram(new sock_filter[program->size()]);
  for (auto i = program->begin(); i != program->end(); ++i) {
    flatProgram[i - program->begin()] = *i;
  }

  BroadcastSetThreadSandbox(Move(flatProgram), program->size());
}

void
SandboxEarlyInit(GeckoProcessType aType, bool aIsNuwa)
{
  // Bug 1168555: Nuwa isn't reliably single-threaded at this point;
  // it starts an IPC I/O thread and then shuts it down before calling
  // the plugin-container entry point, but that thread may not have
  // finished exiting.  If/when any type of sandboxing is used for the
  // Nuwa process (e.g., unsharing the network namespace there instead
  // of for each content process, to save memory), this will need to be
  // changed by moving the SandboxEarlyInit call to an earlier point.
  if (aIsNuwa) {
    return;
  }

  MOZ_RELEASE_ASSERT(IsSingleThreaded());

  // Which kinds of resource isolation (of those that need to be set
  // up at this point) can be used by this process?
  bool canChroot = false;
  bool canUnshareNet = false;
  bool canUnshareIPC = false;

  switch (aType) {
  case GeckoProcessType_Default:
    MOZ_ASSERT(false, "SandboxEarlyInit in parent process");
    return;
#ifdef MOZ_GMP_SANDBOX
  case GeckoProcessType_GMPlugin:
    canUnshareNet = true;
    canUnshareIPC = true;
    canChroot = true;
    break;
#endif
    // In the future, content processes will be able to use some of
    // these.
  default:
    // Other cases intentionally left blank.
    break;
  }

  // If there's nothing to do, then we're done.
  if (!canChroot && !canUnshareNet && !canUnshareIPC) {
    return;
  }

  // If capabilities can't be gained, then nothing can be done.
  const SandboxInfo info = SandboxInfo::Get();
  if (!info.Test(SandboxInfo::kHasUserNamespaces)) {
    return;
  }

  // The failure cases for the various unshares, and setting up the
  // chroot helper, don't strictly need to be fatal -- but they also
  // shouldn't fail on any reasonable system, so let's take the small
  // risk of breakage over the small risk of quietly providing less
  // security than we expect.  (Unlike in SandboxInfo, this is in the
  // child process, so crashing here isn't as severe a response to the
  // unexpected.)
  if (!UnshareUserNamespace()) {
    SANDBOX_LOG_ERROR("unshare(CLONE_NEWUSER): %s", strerror(errno));
    // If CanCreateUserNamespace (SandboxInfo.cpp) returns true, then
    // the unshare shouldn't have failed.
    MOZ_CRASH("unshare(CLONE_NEWUSER)");
  }
  // No early returns after this point!  We need to drop the
  // capabilities that were gained by unsharing the user namesapce.

  if (canUnshareIPC && syscall(__NR_unshare, CLONE_NEWIPC) != 0) {
    SANDBOX_LOG_ERROR("unshare(CLONE_NEWIPC): %s", strerror(errno));
    MOZ_CRASH("unshare(CLONE_NEWIPC)");
  }

  if (canUnshareNet && syscall(__NR_unshare, CLONE_NEWNET) != 0) {
    SANDBOX_LOG_ERROR("unshare(CLONE_NEWNET): %s", strerror(errno));
    MOZ_CRASH("unshare(CLONE_NEWNET)");
  }

  if (canChroot) {
    gChrootHelper = MakeUnique<SandboxChroot>();
    if (!gChrootHelper->Prepare()) {
      SANDBOX_LOG_ERROR("failed to set up chroot helper");
      MOZ_CRASH("SandboxChroot::Prepare");
    }
  }

  if (!LinuxCapabilities().SetCurrent()) {
    SANDBOX_LOG_ERROR("dropping capabilities: %s", strerror(errno));
    MOZ_CRASH("can't drop capabilities");
  }
}

#ifdef MOZ_CONTENT_SANDBOX
/**
 * Starts the seccomp sandbox for a content process.  Should be called
 * only once, and before any potentially harmful content is loaded.
 *
 * Will normally make the process exit on failure.
*/
void
SetContentProcessSandbox()
{
  if (!SandboxInfo::Get().Test(SandboxInfo::kEnabledForContent)) {
    return;
  }

  SetCurrentProcessSandbox(GetContentSandboxPolicy());
}
#endif // MOZ_CONTENT_SANDBOX

#ifdef MOZ_GMP_SANDBOX
/**
 * Starts the seccomp sandbox for a media plugin process.  Should be
 * called only once, and before any potentially harmful content is
 * loaded -- including the plugin itself, if it's considered untrusted.
 *
 * The file indicated by aFilePath, if non-null, can be open()ed
 * read-only, once, after the sandbox starts; it should be the .so
 * file implementing the not-yet-loaded plugin.
 *
 * Will normally make the process exit on failure.
*/
void
SetMediaPluginSandbox(const char *aFilePath)
{
  if (!SandboxInfo::Get().Test(SandboxInfo::kEnabledForMedia)) {
    return;
  }

  MOZ_ASSERT(!gMediaPluginFile.mPath);
  if (aFilePath) {
    gMediaPluginFile.mPath = strdup(aFilePath);
    gMediaPluginFile.mFd = open(aFilePath, O_RDONLY | O_CLOEXEC);
    if (gMediaPluginFile.mFd == -1) {
      SANDBOX_LOG_ERROR("failed to open plugin file %s: %s",
                        aFilePath, strerror(errno));
      MOZ_CRASH();
    }
  } else {
    gMediaPluginFile.mFd = -1;
  }
  // Finally, start the sandbox.
  SetCurrentProcessSandbox(GetMediaSandboxPolicy(&gMediaPluginFile));
}
#endif // MOZ_GMP_SANDBOX

} // namespace mozilla
