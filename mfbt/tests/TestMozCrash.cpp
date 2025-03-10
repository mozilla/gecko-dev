/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

void handler(int signum) { exit(gMozCrashReason == nullptr ? 1 : 0); }

int main(int argc, char** argv) {
  // libxul starts with gMozCrashReason set to nullptr.
  if (gMozCrashReason) {
    fprintf(stderr, "gMozCrashReason unexpectedly starts set to %s.\n",
            gMozCrashReason);
    return 1;
  }

  // The strategy here is simple: fork, trigger an abort from the child and
  // observe behavior.

  pid_t child = fork();
  if (child == 0) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    for (int i = 1; i < 10; ++i)
      if (sigaction(SIGSEGV, &sa, nullptr) == -1) {
        perror(argv[0]);
        return 1;
      }
    // Trigger an actual verbose crash. This calls a function within libxul to
    // make sure it produces verbose crash.
    mozilla::detail::InvalidArrayIndex_CRASH(2, 2);
  } else {
    // Recover exit status from child, check it's an actual crash and that
    // gMozCrashReason is set.
    int waitres;
    if (wait(&waitres) >= 0) {
      if (WIFEXITED(waitres) && WEXITSTATUS(waitres) == 0) return 0;
      fprintf(stderr,
              "Crash didn't happen in the expected way.\n"
              "normal exit: %d.\n"
              "signaled exit: %d.\n"
              "exit status: %d.\n"
              "signal used: %d.\n",
              WIFEXITED(waitres), WIFSIGNALED(waitres),
              (WIFEXITED(waitres) ? WEXITSTATUS(waitres) : -1),
              (WIFSIGNALED(waitres) ? WTERMSIG(waitres) : -1));
      return 1;
    }
  }
}
