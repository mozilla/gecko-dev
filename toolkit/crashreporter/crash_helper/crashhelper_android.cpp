/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

#include <sys/syscall.h>
#include <unistd.h>

#include "mozilla/crash_helper_ffi_generated.h"

extern "C" JNIEXPORT void JNICALL
Java_org_mozilla_gecko_crashhelper_CrashHelper_crash_1generator(
    JNIEnv*, jclass, jint client_pid) {
  crash_generator_logic(client_pid, /* user_app_data_dir */ nullptr);
}
