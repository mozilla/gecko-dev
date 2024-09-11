/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/MacroAssembler.h"

#include "jsapi-tests/tests.h"
#include "jsapi-tests/testsJit.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64) || \
    defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)

static bool GenerateAndRunSub32FromMem(JSContext* cx, int32_t init, int delta) {
  js::LifoAlloc lifo(4096, js::MallocArena);
  TempAllocator alloc(&lifo);
  JitContext jc(cx);
  StackMacroAssembler masm(cx, alloc);
  AutoCreatedBy acb(masm, __func__);

  volatile int32_t memory = init;
  const Register Reg = CallTempReg0;

  PrepareJit(masm);

  Label itWentNegative, fail, end;
  masm.mov(ImmPtr((void*)&memory, ImmPtr::NoCheckToken()), Reg);
  CodeOffset patchAt = masm.sub32FromMemAndBranchIfNegativeWithPatch(
      Address(Reg, 0), &itWentNegative);
  if (init >= delta) {
    // The initial value is >= the delta.  So we don't expect the value to go
    // negative.
    masm.jump(&end);
    masm.bind(&itWentNegative);
    masm.printf("Failed\n");
    masm.breakpoint();
  } else {
    // The initial value is < the delta.  We *do* expect the value to go
    // negative.
    masm.printf("Failed\n");
    masm.breakpoint();
    masm.bind(&itWentNegative);
  }
  masm.bind(&end);

  masm.patchSub32FromMemAndBranchIfNegative(patchAt, Imm32(delta));
  if (!ExecuteJit(cx, masm)) {
    return false;
  }

  MOZ_ASSERT(init - delta == memory);
  return true;
}

BEGIN_TEST(testWasmSub32FromMem) {
  return GenerateAndRunSub32FromMem(cx, 1, 123) &&
         GenerateAndRunSub32FromMem(cx, 120, 3) &&
         GenerateAndRunSub32FromMem(cx, 2, 2);
}
END_TEST(testWasmSub32FromMem)

#endif
