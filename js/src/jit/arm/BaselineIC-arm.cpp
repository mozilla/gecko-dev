/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCompiler.h"
#include "jit/BaselineHelpers.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/IonLinker.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

// ICCompare_Int32

bool
ICCompare_Int32::Compiler::generateStubCode(MacroAssembler &masm)
{
    // Guard that R0 is an integer and R1 is an integer.
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    // Compare payload regs of R0 and R1.
    Assembler::Condition cond = JSOpToCondition(op, /* signed = */true);
    masm.cmp32(R0.payloadReg(), R1.payloadReg());
    masm.ma_mov(Imm32(1), R0.payloadReg(), NoSetCond, cond);
    masm.ma_mov(Imm32(0), R0.payloadReg(), NoSetCond, Assembler::InvertCondition(cond));

    // Result is implicitly boxed already.
    masm.tagValue(JSVAL_TYPE_BOOLEAN, R0.payloadReg(), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

bool
ICCompare_Double::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure, isNaN;
    masm.ensureDouble(R0, FloatReg0, &failure);
    masm.ensureDouble(R1, FloatReg1, &failure);

    Register dest = R0.scratchReg();

    Assembler::DoubleCondition doubleCond = JSOpToDoubleCondition(op);
    Assembler::Condition cond = Assembler::ConditionFromDoubleCondition(doubleCond);

    masm.compareDouble(FloatReg0, FloatReg1);
    masm.ma_mov(Imm32(0), dest);
    masm.ma_mov(Imm32(1), dest, NoSetCond, cond);

    masm.tagValue(JSVAL_TYPE_BOOLEAN, dest, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

// ICBinaryArith_Int32

extern "C" {
    extern MOZ_EXPORT int64_t __aeabi_idivmod(int,int);
}

bool
ICBinaryArith_Int32::Compiler::generateStubCode(MacroAssembler &masm)
{
    // Guard that R0 is an integer and R1 is an integer.
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    // Add R0 and R1.  Don't need to explicitly unbox, just use R2's payloadReg.
    Register scratchReg = R2.payloadReg();

    // DIV and MOD need an extra non-volatile ValueOperand to hold R0.
    GeneralRegisterSet savedRegs = availableGeneralRegs(2);
    savedRegs = GeneralRegisterSet::Intersect(GeneralRegisterSet::NonVolatile(), savedRegs);
    ValueOperand savedValue = savedRegs.takeAnyValue();

    Label maybeNegZero, revertRegister;
    switch(op_) {
      case JSOP_ADD:
        masm.ma_add(R0.payloadReg(), R1.payloadReg(), scratchReg, SetCond);

        // Just jump to failure on overflow.  R0 and R1 are preserved, so we can just jump to
        // the next stub.
        masm.j(Assembler::Overflow, &failure);

        // Box the result and return.  We know R0.typeReg() already contains the integer
        // tag, so we just need to move the result value into place.
        masm.mov(scratchReg, R0.payloadReg());
        break;
      case JSOP_SUB:
        masm.ma_sub(R0.payloadReg(), R1.payloadReg(), scratchReg, SetCond);
        masm.j(Assembler::Overflow, &failure);
        masm.mov(scratchReg, R0.payloadReg());
        break;
      case JSOP_MUL: {
        Assembler::Condition cond = masm.ma_check_mul(R0.payloadReg(), R1.payloadReg(), scratchReg,
                                                      Assembler::Overflow);
        masm.j(cond, &failure);

        masm.ma_cmp(scratchReg, Imm32(0));
        masm.j(Assembler::Equal, &maybeNegZero);

        masm.mov(scratchReg, R0.payloadReg());
        break;
      }
      case JSOP_DIV:
      case JSOP_MOD: {
        // Check for INT_MIN / -1, it results in a double.
        masm.ma_cmp(R0.payloadReg(), Imm32(INT_MIN));
        masm.ma_cmp(R1.payloadReg(), Imm32(-1), Assembler::Equal);
        masm.j(Assembler::Equal, &failure);

        // Check for both division by zero and 0 / X with X < 0 (results in -0).
        masm.ma_cmp(R1.payloadReg(), Imm32(0));
        masm.ma_cmp(R0.payloadReg(), Imm32(0), Assembler::LessThan);
        masm.j(Assembler::Equal, &failure);

        // The call will preserve registers r4-r11. Save R0 and the link register.
        JS_ASSERT(R1 == ValueOperand(r5, r4));
        JS_ASSERT(R0 == ValueOperand(r3, r2));
        masm.moveValue(R0, savedValue);

        masm.setupAlignedABICall(2);
        masm.passABIArg(R0.payloadReg());
        masm.passABIArg(R1.payloadReg());
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, __aeabi_idivmod));

        // idivmod returns the quotient in r0, and the remainder in r1.
        if (op_ == JSOP_DIV) {
            // Result is a double if the remainder != 0.
            masm.branch32(Assembler::NotEqual, r1, Imm32(0), &revertRegister);
            masm.tagValue(JSVAL_TYPE_INT32, r0, R0);
        } else {
            // If X % Y == 0 and X < 0, the result is -0.
            Label done;
            masm.branch32(Assembler::NotEqual, r1, Imm32(0), &done);
            masm.branch32(Assembler::LessThan, savedValue.payloadReg(), Imm32(0), &revertRegister);
            masm.bind(&done);
            masm.tagValue(JSVAL_TYPE_INT32, r1, R0);
        }
        break;
      }
      case JSOP_BITOR:
        masm.ma_orr(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_BITXOR:
        masm.ma_eor(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_BITAND:
        masm.ma_and(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_LSH:
        // ARM will happily try to shift by more than 0x1f.
        masm.ma_and(Imm32(0x1F), R1.payloadReg(), R1.payloadReg());
        masm.ma_lsl(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_RSH:
        masm.ma_and(Imm32(0x1F), R1.payloadReg(), R1.payloadReg());
        masm.ma_asr(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_URSH:
        masm.ma_and(Imm32(0x1F), R1.payloadReg(), scratchReg);
        masm.ma_lsr(scratchReg, R0.payloadReg(), scratchReg);
        masm.ma_cmp(scratchReg, Imm32(0));
        if (allowDouble_) {
            Label toUint;
            masm.j(Assembler::LessThan, &toUint);

            // Move result and box for return.
            masm.mov(scratchReg, R0.payloadReg());
            EmitReturnFromIC(masm);

            masm.bind(&toUint);
            masm.convertUInt32ToDouble(scratchReg, ScratchDoubleReg);
            masm.boxDouble(ScratchDoubleReg, R0);
        } else {
            masm.j(Assembler::LessThan, &failure);
            // Move result for return.
            masm.mov(scratchReg, R0.payloadReg());
        }
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unhandled op for BinaryArith_Int32.");
    }

    EmitReturnFromIC(masm);

    switch (op_) {
      case JSOP_MUL:
        masm.bind(&maybeNegZero);

        // Result is -0 if exactly one of lhs or rhs is negative.
        masm.ma_cmn(R0.payloadReg(), R1.payloadReg());
        masm.j(Assembler::Signed, &failure);

        // Result is +0.
        masm.ma_mov(Imm32(0), R0.payloadReg());
        EmitReturnFromIC(masm);
        break;
      case JSOP_DIV:
      case JSOP_MOD:
        masm.bind(&revertRegister);
        masm.moveValue(savedValue, R0);
        break;
      default:
        break;
    }

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

bool
ICUnaryArith_Int32::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);

    switch (op) {
      case JSOP_BITNOT:
        masm.ma_mvn(R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_NEG:
        // Guard against 0 and MIN_INT, both result in a double.
        masm.branchTest32(Assembler::Zero, R0.payloadReg(), Imm32(0x7fffffff), &failure);

        // Compile -x as 0 - x.
        masm.ma_rsb(R0.payloadReg(), Imm32(0), R0.payloadReg());
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected op");
    }

    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

} // namespace jit
} // namespace js
