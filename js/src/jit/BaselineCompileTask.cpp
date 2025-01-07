/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCompileTask.h"
#include "jit/JitRuntime.h"
#include "jit/JitScript.h"
#include "vm/HelperThreadState.h"

#include "vm/JSScript-inl.h"
#include "vm/Realm-inl.h"

using namespace js;
using namespace js::jit;

void BaselineCompileTask::runHelperThreadTask(
    AutoLockHelperThreadState& locked) {
  {
    AutoUnlockHelperThreadState unlock(locked);
    runTask();
  }

  FinishOffThreadBaselineCompile(this, locked);

  // Ping the main thread so that the compiled code can be incorporated at
  // the next interrupt callback.
  runtimeFromAnyThread()->mainContextFromAnyThread()->requestInterrupt(
      InterruptReason::AttachOffThreadCompilations);
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

/* static */
void BaselineCompileTask::FinishOffThreadTask(BaselineCompileTask* task) {
  JSScript* script = task->script();
  if (script->isBaselineCompilingOffThread()) {
    script->jitScript()->clearIsBaselineCompiling(script);
  }

  task->masm_.reset();

  // The task is allocated into its LifoAlloc, so destroying that will
  // destroy the task and all other data accumulated during compilation.
  js_delete(task->alloc_->lifoAlloc());
}

void BaselineCompileTask::finishOnMainThread(JSContext* cx) {
  if (!compiler_->finishCompile(cx)) {
    cx->recoverFromOutOfMemory();
  }
}

void js::AttachFinishedBaselineCompilations(JSContext* cx,
                                            AutoLockHelperThreadState& lock) {
  JSRuntime* rt = cx->runtime();

  while (true) {
    GlobalHelperThreadState::BaselineCompileTaskVector& finished =
        HelperThreadState().baselineFinishedList(lock);

    // Find a finished task for this runtime.
    bool found = false;
    for (size_t i = 0; i < finished.length(); i++) {
      BaselineCompileTask* task = finished[i];
      if (task->runtimeFromAnyThread() != rt) {
        continue;
      }
      found = true;

      HelperThreadState().remove(finished, &i);
      rt->jitRuntime()->numFinishedOffThreadTasksRef(lock)--;
      {
        if (!task->failed()) {
          AutoUnlockHelperThreadState unlock(lock);
          AutoRealm ar(cx, task->script());
          task->finishOnMainThread(cx);
          BaselineCompileTask::FinishOffThreadTask(task);
        }
      }
    }
    if (!found) {
      break;
    }
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
