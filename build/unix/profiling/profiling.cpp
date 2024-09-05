/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_PROFILE_GENERATE) && defined(XP_LINUX) && !defined(ANDROID)

#  include <pthread.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <unistd.h>
#  include <errno.h>

#  if defined(__cplusplus)
extern "C" {
#  endif
void __llvm_profile_initialize(void);
void __llvm_profile_initialize_file(void);
void __llvm_profile_set_filename(const char*);

// Use the API to force a different filename, then set back the original one.
// This will make sure the pattern is re-parsed and thus the PID properly
// updated within the lprofCurFilename struct.
static void updateFilenameAfterFork(void) {
  __llvm_profile_set_filename("default.profraw");
  __llvm_profile_initialize_file();
  __llvm_profile_set_filename(getenv("LLVM_PROFILE_FILE"));
  __llvm_profile_initialize_file();
}

static int CustomRegisterRuntime(void) {
  __llvm_profile_initialize();
  if (pthread_atfork(NULL, NULL, updateFilenameAfterFork) < 0) {
    fprintf(stderr, "[%d] [%s] pthread_atfork()=%d\n", getpid(),
            __PRETTY_FUNCTION__, errno);
  }
  return 0;
}

int __llvm_profile_runtime = CustomRegisterRuntime();
#  if defined(__cplusplus)
}
#  endif
#endif  //  defined(MOZ_PROFILE_GENERATE) && defined(XP_LINUX) &&
        //  !defined(ANDROID)
