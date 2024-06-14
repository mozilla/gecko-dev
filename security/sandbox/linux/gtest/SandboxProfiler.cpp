/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxProfiler.h"

namespace mozilla {

/* static */
void SandboxProfiler::ReportRequest(const void* top, uint64_t aId,
                                    const char* aOp, int aFlags,
                                    const char* aPath, const char* aPath2,
                                    pid_t aPid) {
  /*
   * Just an empty no op for gtest, otherwise we end up with linkage failure,
   * and adding the real SandboxProfiler.cpp to the gtest linkage breaks
   * because we want to keep separated symbols for uprofiler/uprofiler_initted
   * and gtest links all together.
   * */
}

}  // namespace mozilla
