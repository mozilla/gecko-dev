/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCompileTask_h
#define jit_BaselineCompileTask_h

#include "mozilla/Maybe.h"

#include "jit/BaselineCodeGen.h"
#include "jit/OffthreadSnapshot.h"
#include "vm/HelperThreadTask.h"

namespace js::jit {

class BaselineSnapshot {
  OffthreadGCPtr<JSScript*> script_;
  OffthreadGCPtr<GlobalLexicalEnvironmentObject*> globalLexical_;
  OffthreadGCPtr<JSObject*> globalThis_;
  uint32_t baseWarmUpThreshold_;
  bool isIonCompileable_;
  bool compileDebugInstrumentation_;

 public:
  explicit BaselineSnapshot(JSScript* script,
                            GlobalLexicalEnvironmentObject* globalLexical,
                            JSObject* globalThis, uint32_t baseWarmUpThreshold,
                            bool isIonCompileable,
                            bool compileDebugInstrumentation)
      : script_(script),
        globalLexical_(globalLexical),
        globalThis_(globalThis),
        baseWarmUpThreshold_(baseWarmUpThreshold),
        isIonCompileable_(isIonCompileable),
        compileDebugInstrumentation_(compileDebugInstrumentation) {}

  JSScript* script() const { return script_; }
  GlobalLexicalEnvironmentObject* globalLexical() const {
    return globalLexical_;
  }
  JSObject* globalThis() const { return globalThis_; }
  uint32_t baseWarmUpThreshold() const { return baseWarmUpThreshold_; }
  bool isIonCompileable() const { return isIonCompileable_; }
  bool compileDebugInstrumentation() const {
    return compileDebugInstrumentation_;
  }

  void trace(JSTracer* trc);
};

class BaselineCompileTask final : public HelperThreadTask {
 public:
  BaselineCompileTask(CompileRealm* realm, TempAllocator* alloc,
                      BaselineSnapshot* snapshot)
      : realm_(realm), alloc_(alloc), snapshot_(snapshot) {}

  ThreadType threadType() override { return THREAD_TYPE_BASELINE; }
  void runTask();
  void runHelperThreadTask(AutoLockHelperThreadState& locked) override;

  JSRuntime* runtimeFromAnyThread() const {
    return snapshot_->script()->runtimeFromAnyThread();
  }

  const char* getName() override { return "BaselineCompileTask"; }

  bool failed() const { return failed_; }

  void trace(JSTracer* trc);

 private:
  CompileRealm* realm_;
  TempAllocator* alloc_;
  BaselineSnapshot* snapshot_;

  mozilla::Maybe<OffThreadMacroAssembler> masm_;
  mozilla::Maybe<BaselineCompiler> compiler_;

  bool failed_ = false;
};

}  // namespace js::jit

#endif /* jit_BaselineCompileTask_h */
