/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "mozilla/ipc/FileDescriptorShuffle.h"
#include "mozilla/UniquePtr.h"

// WARNING: despite the name, this file is also used on the BSDs and
// Solaris (basically, Unixes that aren't Mac OS), not just Linux.

namespace {

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

bool LaunchApp(const std::vector<std::string>& argv,
               const LaunchOptions& options, ProcessHandle* process_handle) {
  mozilla::UniquePtr<char*[]> argv_cstr(new char*[argv.size() + 1]);

  EnvironmentArray envp = BuildEnvironmentArray(options.env_map);
  mozilla::ipc::FileDescriptorShuffle shuffle;
  if (!shuffle.Init(options.fds_to_remap)) {
    return false;
  }

#ifdef OS_LINUX
  pid_t pid = options.fork_delegate ? options.fork_delegate->Fork() : fork();
  // WARNING: if pid == 0, only async signal safe operations are permitted from
  // here until exec or _exit.
  //
  // Specifically, heap allocation is not safe: the sandbox's fork substitute
  // won't run the pthread_atfork handlers that fix up the malloc locks.
#else
  pid_t pid = fork();
#endif

  if (pid < 0) return false;

  if (pid == 0) {
    // In the child:
    for (const auto& fds : shuffle.Dup2Sequence()) {
      if (HANDLE_EINTR(dup2(fds.first, fds.second)) != fds.second) {
        // This shouldn't happen, but check for it.  And see below
        // about logging being unsafe here, so this is debug only.
        DLOG(ERROR) << "dup2 failed";
        _exit(127);
      }
    }

    CloseSuperfluousFds(&shuffle, [](void* aCtx, int aFd) {
      return static_cast<decltype(&shuffle)>(aCtx)->MapsTo(aFd);
    });

    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;

    execve(argv_cstr[0], argv_cstr.get(), envp.get());
    // if we get here, we're in serious trouble and should complain loudly
    // NOTE: This is async signal unsafe; it could deadlock instead.  (But
    // only on debug builds; otherwise it's a signal-safe no-op.)
    DLOG(ERROR) << "FAILED TO exec() CHILD PROCESS, path: " << argv_cstr[0];
    _exit(127);
  }

  // In the parent:
  gProcessLog.print("==> process %d launched child process %d\n",
                    GetCurrentProcId(), pid);
  if (options.wait) HANDLE_EINTR(waitpid(pid, 0, 0));

  if (process_handle) *process_handle = pid;

  return true;
}

bool LaunchApp(const CommandLine& cl, const LaunchOptions& options,
               ProcessHandle* process_handle) {
  return LaunchApp(cl.argv(), options, process_handle);
}

}  // namespace base
