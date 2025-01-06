/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "mozilla/DataMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prenv.h"

#include "chrome/common/process_watcher.h"

#ifdef MOZ_ENABLE_FORKSERVER
#  include "mozilla/ipc/ForkServiceChild.h"
#endif

#if !defined(XP_DARWIN)
// Linux, {Free,Net,Open}BSD, and Solaris; but not macOS, yet.
#  define HAVE_PIPE2 1
#endif

// The basic idea here is a minimal SIGCHLD handler which writes to a
// pipe and a libevent callback on the I/O thread which fires when the
// other end becomes readable.  When we start waiting for process
// termination we check if it had already terminated, and otherwise
// register it to be checked later when SIGCHLD fires.
//
// Making this more complicated is that we usually want to kill the
// process after a timeout, in case it hangs trying to exit, but not
// if it's already exited by that point (see `DelayedKill`).
// But we also support waiting indefinitely, for debug/CI use cases
// like refcount logging / leak detection / code coverage, and in that
// case we block parent process shutdown until all children exit
// (which is done by blocking the I/O thread late in shutdown, which
// isn't ideal, but the Windows implementation has the same issue).

// Maximum amount of time (in milliseconds) to wait for the process to exit.
// XXX/cjones: fairly arbitrary, chosen to match process_watcher_win.cc
static constexpr int kMaxWaitMs = 2000;

// This is also somewhat arbitrary, but loosely based on Try results.
// See also toolkit.asyncshutdown.crash_timeout (currently 60s) after
// which the parent process will be killed.
#ifdef MOZ_CODE_COVERAGE
// Code coverage instrumentation can be slow (especially when writing
// out data, which has to take a lock on the data files).
static constexpr int kShutdownWaitMs = 80000;
#elif defined(MOZ_ASAN) || defined(MOZ_TSAN)
// Sanitizers slow things down in some cases; see bug 1806224.
static constexpr int kShutdownWaitMs = 40000;
#else
static constexpr int kShutdownWaitMs = 8000;
#endif

namespace {

using base::BlockingWait;

// Represents a child process being awaited (which is expected to exit
// soon, or already has).
//
// If `mForce` is null then we will wait indefinitely (and block
// parent shutdown; see above); otherwise it will be killed after a
// timeout (or during parent shutdown, if that happens first).
struct PendingChild {
  pid_t mPid;
  nsCOMPtr<nsITimer> mForce;
};

// `EnsureProcessTerminated` is called when a process is expected to
// be shutting down, so there should be relatively few `PendingChild`
// instances at any given time, meaning that using an array and doing
// O(n) operations should be fine.
static mozilla::StaticDataMutex<mozilla::StaticAutoPtr<nsTArray<PendingChild>>>
    gPendingChildren("ProcessWatcher::gPendingChildren");
static int gSignalPipe[2] = {-1, -1};

// A wrapper around WaitForProcess to simplify the result (true if the
// process exited and the pid is now freed for reuse, false if it's
// still running), and handle the case where "blocking" mode doesn't
// block (so this function will always return true if `aBlock` is
// `YES`), and log a warning message if the process didn't exit
// successfully (as in `exit(0)`).
static bool IsProcessDead(pid_t pid, BlockingWait aBlock) {
  int info = 0;

  auto status = WaitForProcess(pid, aBlock, &info);
  while (aBlock == BlockingWait::Yes &&
         status == base::ProcessStatus::Running) {
    // It doesn't matter if this is interrupted; we just need to
    // wait for some amount of time while the other process status
    // event is (hopefully) handled.  This is used only during an
    // error case at shutdown, so a 1s wait won't be too noticeable.
    sleep(1);
    status = WaitForProcess(pid, aBlock, &info);
  }

  switch (status) {
    case base::ProcessStatus::Running:
      return false;

    case base::ProcessStatus::Exited:
      if (info != 0) {
        CHROMIUM_LOG(WARNING)
            << "process " << pid << " exited with status " << info;
      }
      return true;

    case base::ProcessStatus::Killed:
      CHROMIUM_LOG(WARNING)
          << "process " << pid << " exited on signal " << info;
      return true;

    case base::ProcessStatus::Error:
      CHROMIUM_LOG(ERROR) << "waiting for process " << pid
                          << " failed with error " << info;
      // Don't keep trying.
      return true;

    default:
      DCHECK(false) << "can't happen";
      return true;
  }
}

// Creates a timer to kill the process after a delay, for the
// `force=true` case.  The timer is bound to the I/O thread, which
// means it needs to be cancelled there (and thus that child exit
// notifications need to be handled on the I/O thread).
already_AddRefed<nsITimer> DelayedKill(pid_t aPid) {
  nsCOMPtr<nsITimer> timer;

  nsresult rv = NS_NewTimerWithCallback(
      getter_AddRefs(timer),
      [aPid](nsITimer*) {
        if (kill(aPid, SIGKILL) != 0) {
          CHROMIUM_LOG(ERROR) << "failed to send SIGKILL to process " << aPid;
        }
      },
      kMaxWaitMs, nsITimer::TYPE_ONE_SHOT, "ProcessWatcher::DelayedKill",
      XRE_GetAsyncIOEventTarget());

  // This should happen only during shutdown, in which case we're
  // about to kill the process anyway during I/O thread destruction.
  if (NS_FAILED(rv)) {
    CHROMIUM_LOG(WARNING) << "failed to start kill timer for process " << aPid
                          << "; killing immediately";
    kill(aPid, SIGKILL);
    return nullptr;
  }

  return timer.forget();
}

bool CrashProcessIfHanging(pid_t aPid) {
  if (IsProcessDead(aPid, BlockingWait::No)) {
    return false;
  }

  // If child processes seems to be hanging on shutdown, wait for a
  // reasonable time.  The wait is global instead of per-process
  // because the child processes should be shutting down in
  // parallel, and also we're potentially racing global timeouts
  // like nsTerminator.  (The counter doesn't need to be atomic;
  // this is always called on the I/O thread.)
  static int sWaitMs = kShutdownWaitMs;
  if (sWaitMs > 0) {
    CHROMIUM_LOG(WARNING) << "Process " << aPid
                          << " may be hanging at shutdown; will wait for up to "
                          << sWaitMs << "ms";
  }
  // There isn't a way to do a time-limited wait that's both
  // portable and doesn't require messing with signals.  Instead, we
  // sleep in short increments and poll the process status.
  while (sWaitMs > 0) {
    static constexpr int kWaitTickMs = 200;
    struct timespec ts = {kWaitTickMs / 1000, (kWaitTickMs % 1000) * 1000000};
    HANDLE_EINTR(nanosleep(&ts, &ts));
    sWaitMs -= kWaitTickMs;

    if (IsProcessDead(aPid, BlockingWait::No)) {
      return false;
    }
  }

  // We want TreeHerder to flag this log line as an error, so that
  // this is more obviously a deliberate crash; "fatal error" is one
  // of the strings it looks for.
  CHROMIUM_LOG(ERROR)
      << "Process " << aPid
      << " hanging at shutdown; attempting crash report (fatal error).";

  kill(aPid, SIGABRT);
  return true;
}

// Most of the logic is here.  Reponds to SIGCHLD via the self-pipe,
// and handles shutdown behavior in `WillDestroyCurrentMessageLoop`.
// There is one instance of this class; it's created the first time
// it's used and destroys itself during IPC shutdown.
class ProcessCleaner final : public MessageLoopForIO::Watcher,
                             public MessageLoop::DestructionObserver {
 public:
  // Safety: this must be called on the I/O thread.
  void Register() {
    MessageLoopForIO* loop = MessageLoopForIO::current();
    loop->AddDestructionObserver(this);
    loop->WatchFileDescriptor(gSignalPipe[0], /* persistent= */ true,
                              MessageLoopForIO::WATCH_READ, &mWatcher, this);
  }

  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(fd == gSignalPipe[0]);
    ssize_t rv;
    // Drain the pipe and prune dead processes.
    do {
      char msg[32];
      rv = HANDLE_EINTR(read(gSignalPipe[0], msg, sizeof msg));
      CHECK(rv != 0);
      if (rv < 0) {
        DCHECK(errno == EAGAIN || errno == EWOULDBLOCK);
      } else {
#ifdef DEBUG
        for (size_t i = 0; i < (size_t)rv; ++i) {
          DCHECK(msg[i] == 0);
        }
#endif
      }
    } while (rv > 0);
    PruneDeadProcesses();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    CHROMIUM_LOG(FATAL) << "unreachable";
  }

  void WillDestroyCurrentMessageLoop() override {
    mWatcher.StopWatchingFileDescriptor();
    auto lock = gPendingChildren.Lock();
    auto& children = lock.ref();
    if (children) {
      for (const auto& child : *children) {
        // If the child still has force-termination pending, do that now.
        if (child.mForce) {
          // This is too late for timers to run, so no need to Cancel().
          //
          // FIXME (bug 1724337, approximately): This code isn't run at
          // all in practice, because the parent process will already have
          // exited (unless the fastShutdownStage pref is changed).
          if (kill(child.mPid, SIGKILL) != 0) {
            CHROMIUM_LOG(ERROR)
                << "failed to send SIGKILL to process " << child.mPid;
            continue;
          }
        } else {
          // Exception for the fake hang tests in ipc/glue/test/browser
          // (See also the comment in `~ProcessChild()`.)
          if (!PR_GetEnv("MOZ_TEST_CHILD_EXIT_HANG") &&
              !CrashProcessIfHanging(child.mPid)) {
            continue;
          }
        }
        // If the process was just killed, it should exit immediately;
        // otherwise, block until it exits on its own.
        IsProcessDead(child.mPid, BlockingWait::Yes);
      }
      children = nullptr;
    }
    delete this;
  }

 private:
  MessageLoopForIO::FileDescriptorWatcher mWatcher;

  static void PruneDeadProcesses() {
    auto lock = gPendingChildren.Lock();
    auto& children = lock.ref();
    if (!children || children->IsEmpty()) {
      return;
    }
    nsTArray<PendingChild> live;
    for (const auto& child : *children) {
      if (IsProcessDead(child.mPid, BlockingWait::No)) {
        if (child.mForce) {
          child.mForce->Cancel();
        }
      } else {
        live.AppendElement(child);
      }
    }
    *children = std::move(live);
  }
};

static void HandleSigChld(int signum) {
  DCHECK(signum == SIGCHLD);
  char msg = 0;
  HANDLE_EINTR(write(gSignalPipe[1], &msg, 1));
  // Can't log here if this fails (at least not normally; SafeSPrintf
  // from security/sandbox/chromium could be used).
  //
  // (Note that this could fail with EAGAIN if the pipe buffer becomes
  // full; this is extremely unlikely, and it doesn't matter because
  // the reader will be woken up regardless and doesn't care about the
  // number of signals delivered.)
}

static void ProcessWatcherInit() {
  int rv;

#ifdef HAVE_PIPE2
  rv = pipe2(gSignalPipe, O_NONBLOCK | O_CLOEXEC);
  CHECK(rv == 0)
  << "pipe2() failed";
#else
  rv = pipe(gSignalPipe);
  CHECK(rv == 0)
  << "pipe() failed";
  for (int fd : gSignalPipe) {
    rv = fcntl(fd, F_SETFL, O_NONBLOCK);
    CHECK(rv == 0)
    << "O_NONBLOCK failed";
    rv = fcntl(fd, F_SETFD, FD_CLOEXEC);
    CHECK(rv == 0)
    << "FD_CLOEXEC failed";
  }
#endif  // HAVE_PIPE2

  // Currently there are no other SIGCHLD handlers; this is debug
  // asserted.  If the situation changes, it should be relatively
  // simple to delegate; note that this ProcessWatcher doesn't
  // interfere with child processes it hasn't been asked to handle.
  auto oldHandler = signal(SIGCHLD, HandleSigChld);
  CHECK(oldHandler != SIG_ERR);
  DCHECK(oldHandler == SIG_DFL);

  // Start the ProcessCleaner; registering it with the I/O thread must
  // happen on the I/O thread itself.  It's okay for that to happen
  // asynchronously: the callback is level-triggered, so if the signal
  // handler already wrote to the pipe at that point then it will be
  // detected, and the signal itself is async so additional delay
  // doesn't change the semantics.
  XRE_GetAsyncIOEventTarget()->Dispatch(
      NS_NewRunnableFunction("ProcessCleaner::Register", [] {
        ProcessCleaner* pc = new ProcessCleaner();
        pc->Register();
      }));
}

}  // namespace

/**
 * Do everything possible to ensure that |process| has been reaped
 * before this process exits.
 *
 * |force| decides how strict to be with the child's shutdown.
 *
 *                | child exit timeout | upon parent shutdown:
 *                +--------------------+----------------------------------
 *   force=true   | 2 seconds          | kill(child, SIGKILL)
 *   force=false  | infinite           | waitpid(child)
 *
 * If a child process doesn't shut down properly, and |force=false|
 * used, then the parent will wait on the child forever.  So,
 * |force=false| is expected to be used when an external entity can be
 * responsible for terminating hung processes, e.g. automated test
 * harnesses.
 */
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process,
                                             bool force) {
  DCHECK(process != base::GetCurrentProcId());
  DCHECK(process > 0);

  static std::once_flag sInited;
  std::call_once(sInited, ProcessWatcherInit);

  auto lock = gPendingChildren.Lock();
  auto& children = lock.ref();

  // Check if the process already exited.  This needs to happen under
  // the `gPendingChildren` lock to prevent this sequence:
  //
  // A1. this non-blocking wait fails
  // B1. the process exits
  // B2. SIGCHLD is handled
  // B3. the ProcessCleaner wakes up and drains the signal pipe
  // A2. the process is added to `gPendingChildren`
  //
  // Holding the lock prevents B3 from occurring between A1 and A2.
  if (IsProcessDead(process, BlockingWait::No)) {
    return;
  }

  if (!children) {
    children = new nsTArray<PendingChild>();
  }
  // Check for duplicate pids.  This is safe even in corner cases with
  // pid reuse: the pid can't be reused by the OS until the zombie
  // process has been waited, and both the `waitpid` and the following
  // removal of the `PendingChild` object occur while continually
  // holding the lock, which is also held here.
  for (const auto& child : *children) {
    if (child.mPid == process) {
#ifdef MOZ_ENABLE_FORKSERVER
      if (mozilla::ipc::ForkServiceChild::WasUsed()) {
        // In theory we can end up here if an earlier child process
        // with the same pid was launched via the fork server, and
        // exited, and had its pid reused for a new process before we
        // noticed that it exited.

        CHROMIUM_LOG(WARNING) << "EnsureProcessTerminated: duplicate process"
                                 " ID "
                              << process
                              << "; assuming this is because of the fork"
                                 " server.";

        // So, we want to end up with a PendingChild for the new
        // process; we can just use the old one.  Ideally we'd fix the
        // `mForce` value, but that would involve needing to cancel a
        // timer when we aren't necessarily on the right thread, and
        // in practice the `force` parameter depends only on the build
        // type.  (Again, see bug 1752638 for the correct solution.)
        return;
      }
#endif
      MOZ_ASSERT(false,
                 "EnsureProcessTerminated must be called at most once for a "
                 "given process");
      return;
    }
  }

  PendingChild child{};
  child.mPid = process;
  if (force) {
    child.mForce = DelayedKill(process);
  }
  children->AppendElement(std::move(child));
}
