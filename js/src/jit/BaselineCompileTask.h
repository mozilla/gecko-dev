/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCompileTask_h
#define jit_BaselineCompileTask_h

#include "jit/BaselineCodeGen.h"
#include "vm/HelperThreadTask.h"

namespace js::jit {

class BaselineSnapshot {
  JSScript* script_;
  GlobalLexicalEnvironmentObject* globalLexical_;
  JSObject* globalThis_;
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
};

class BaselineCompileTask final : public HelperThreadTask {
 public:
  ThreadType threadType() override { return THREAD_TYPE_BASELINE; }
  void runTask();
  void runHelperThreadTask(AutoLockHelperThreadState& locked) override;

  JSRuntime* runtimeFromAnyThread() const { MOZ_CRASH("TODO"); }

  const char* getName() override { return "BaselineCompileTask"; }

  void trace(JSTracer* trc);
};

}  // namespace js::jit

#endif /* jit_BaselineCompileTask_h */
