/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCompileTask.h"
#include "jit/JitRuntime.h"
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

// Debugging RAII class which marks the current thread as performing
// an offthread baseline compilation.
class MOZ_RAII AutoEnterBaselineBackend {
 public:
  AutoEnterBaselineBackend() {
#ifdef DEBUG
    JitContext* jcx = GetJitContext();
    jcx->enterBaselineBackend();
#endif
  }

#ifdef DEBUG
  ~AutoEnterBaselineBackend() {
    JitContext* jcx = GetJitContext();
    jcx->leaveBaselineBackend();
  }
#endif
};

void BaselineCompileTask::runTask() {
  jit::JitContext jctx(realm_->runtime());
  AutoEnterBaselineBackend enter;

  masm_.emplace(*alloc_, realm_);
  compiler_.emplace(*alloc_, realm_->runtime(), masm_.ref(), snapshot_);

  if (!compiler_->init()) {
    failed_ = true;
    return;
  }
  MethodStatus status = compiler_->compileOffThread();
  if (status == Method_Error) {
    failed_ = true;
  }
}

void BaselineSnapshot::trace(JSTracer* trc) {
  TraceOffthreadGCPtr(trc, script_, "baseline-snapshot-script");
  TraceOffthreadGCPtr(trc, globalLexical_, "baseline-snapshot-lexical");
  TraceOffthreadGCPtr(trc, globalThis_, "baseline-snapshot-this");
}

void BaselineCompileTask::trace(JSTracer* trc) {
  if (!realm_->runtime()->runtimeMatches(trc->runtime())) {
    return;
  }
  snapshot_->trace(trc);
}
