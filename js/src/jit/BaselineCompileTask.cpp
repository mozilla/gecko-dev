/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCompileTask.h"
#include "vm/HelperThreadState.h"

using namespace js;
using namespace js::jit;

void BaselineCompileTask::runHelperThreadTask(
    AutoLockHelperThreadState& locked) {
  {
    AutoUnlockHelperThreadState unlock(locked);
    runTask();
  }

  FinishOffThreadBaselineCompile(this, locked);

  // TODO: Ping the main thread so that the compiled code can be incorporated at
  // the next interrupt callback.
}

void BaselineCompileTask::runTask() { MOZ_CRASH("TODO"); }

void BaselineCompileTask::trace(JSTracer* trc) { MOZ_CRASH("TODO"); }
