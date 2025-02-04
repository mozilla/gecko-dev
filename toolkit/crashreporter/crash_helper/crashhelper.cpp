/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>

#if defined(XP_WIN)
#  include <windows.h>
#endif  // defined(XP_WIN)

#include "mozilla/crash_helper_ffi_generated.h"

int main(int argc, char* argv[]) {
  if (argc < 3) {
    exit(EXIT_FAILURE);
  }

  errno = 0;
  long value = strtol(argv[1], nullptr, 10);
  Pid client_pid = static_cast<Pid>(value);

  if (errno != 0) {
    exit(EXIT_FAILURE);
  }

  char* user_app_data_dir = argv[2];

  crash_generator_logic(client_pid, user_app_data_dir);
  exit(EXIT_SUCCESS);
}
