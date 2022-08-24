/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/CalleeToken.h"
#include "jit/JitFrames.h"
#include "jit/JitRuntime.h"
#ifdef JS_ION_PERF
#  include "jit/PerfSpewer.h"
#endif
#include "jit/VMFunctions.h"
#include "vm/JitActivation.h"  // js::jit::JitActivation
#include "vm/JSContext.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

// This file includes stubs for generating the JIT trampolines when there is no
// JIT backend, and also includes implementations for assorted random things
// which can't be implemented in headers.

void JitRuntime::generateEnterJIT(JSContext*, MacroAssembler&) { MOZ_CRASH(); }
// static
mozilla::Maybe<::JS::ProfilingFrameIterator::RegisterState>
JitRuntime::getCppEntryRegisters(JitFrameLayout* frameStackAddress) {
  return mozilla::Nothing{};
}
void JitRuntime::generateInvalidator(MacroAssembler&, Label*) { MOZ_CRASH(); }
void JitRuntime::generateArgumentsRectifier(MacroAssembler&,
                                            ArgumentsRectifierKind kind) {
  MOZ_CRASH();
}
void JitRuntime::generateBailoutHandler(MacroAssembler&, Label*) {
  MOZ_CRASH();
}
uint32_t JitRuntime::generatePreBarrier(JSContext*, MacroAssembler&, MIRType) {
  MOZ_CRASH();
}
void JitRuntime::generateBailoutTailStub(MacroAssembler& masm,
                                         Label* bailoutTail) {
  AutoCreatedBy acb(masm, "JitRuntime::generateBailoutTailStub");

  masm.bind(bailoutTail);
  masm.generateBailoutTail(a1, a2);
}

bool JitRuntime::generateVMWrapper(JSContext*, MacroAssembler&,
                                   const VMFunctionData&, DynFn, uint32_t*) {
  MOZ_CRASH();
}
