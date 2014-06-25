/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/mips/CodeGenerator-mips.h"

#include "mozilla/MathAlgorithms.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsnum.h"

#include "jit/CodeGenerator.h"
#include "jit/IonFrames.h"
#include "jit/JitCompartment.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/Shape.h"
#include "vm/TraceLogging.h"

#include "jsscriptinlines.h"

#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;
using mozilla::NegativeInfinity;
using JS::GenericNaN;

// shared
CodeGeneratorMIPS::CodeGeneratorMIPS(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorShared(gen, graph, masm)
{
}

bool
CodeGeneratorMIPS::generatePrologue()
{
    MOZ_ASSERT(!gen->compilingAsmJS());
    // Note that this automatically sets MacroAssembler::framePushed().
    masm.reserveStack(frameSize());
    masm.checkStackAlignment();
    return true;
}

bool
CodeGeneratorMIPS::generateAsmJSPrologue(Label *stackOverflowLabel)
{
    JS_ASSERT(gen->compilingAsmJS());

    // See comment in Assembler-mips.h about AsmJSFrameSize.
    masm.push(ra);

    // The asm.js over-recursed handler wants to be able to assume that SP
    // points to the return address, so perform the check after pushing ra but
    // before pushing frameDepth.
    if (!omitOverRecursedCheck()) {
        masm.branchPtr(Assembler::AboveOrEqual,
                       AsmJSAbsoluteAddress(AsmJSImm_StackLimit),
                       StackPointer,
                       stackOverflowLabel);
    }

    // Note that this automatically sets MacroAssembler::framePushed().
    masm.reserveStack(frameDepth_);
    masm.checkStackAlignment();
    return true;
}

bool
CodeGeneratorMIPS::generateEpilogue()
{
    masm.bind(&returnLabel_);

#ifdef JS_TRACE_LOGGING
    if (!gen->compilingAsmJS() && gen->info().executionMode() == SequentialExecution) {
        if (!emitTracelogStopEvent(TraceLogger::IonMonkey))
            return false;
        if (!emitTracelogScriptStop())
            return false;
    }
#endif

    if (gen->compilingAsmJS())
        masm.freeStack(frameDepth_);
    else
        masm.freeStack(frameSize());
    JS_ASSERT(masm.framePushed() == 0);
    masm.ret();
    return true;
}

void
CodeGeneratorMIPS::branchToBlock(Assembler::FloatFormat fmt, FloatRegister lhs, FloatRegister rhs,
                                 MBasicBlock *mir, Assembler::DoubleCondition cond)
{
    // Skip past trivial blocks.
    mir = skipTrivialBlocks(mir);

    Label *label = mir->lir()->label();
    if (Label *oolEntry = labelForBackedgeWithImplicitCheck(mir)) {
        // Note: the backedge is initially a jump to the next instruction.
        // It will be patched to the target block's label during link().
        RepatchLabel rejoin;

        CodeOffsetJump backedge;
        Label skip;
        if (fmt == Assembler::DoubleFloat)
            masm.ma_bc1d(lhs, rhs, &skip, Assembler::InvertCondition(cond), ShortJump);
        else
            masm.ma_bc1s(lhs, rhs, &skip, Assembler::InvertCondition(cond), ShortJump);

        backedge = masm.jumpWithPatch(&rejoin);
        masm.bind(&rejoin);
        masm.bind(&skip);

        if (!patchableBackedges_.append(PatchableBackedgeInfo(backedge, label, oolEntry)))
            MOZ_CRASH();
    } else {
        if (fmt == Assembler::DoubleFloat)
            masm.branchDouble(cond, lhs, rhs, mir->lir()->label());
        else
            masm.branchFloat(cond, lhs, rhs, mir->lir()->label());
    }
}

bool
OutOfLineBailout::accept(CodeGeneratorMIPS *codegen)
{
    return codegen->visitOutOfLineBailout(this);
}

bool
CodeGeneratorMIPS::visitTestIAndBranch(LTestIAndBranch *test)
{
    const LAllocation *opd = test->getOperand(0);
    MBasicBlock *ifTrue = test->ifTrue();
    MBasicBlock *ifFalse = test->ifFalse();

    emitBranch(ToRegister(opd), Imm32(0), Assembler::NonZero, ifTrue, ifFalse);
    return true;
}

bool
CodeGeneratorMIPS::visitCompare(LCompare *comp)
{
    Assembler::Condition cond = JSOpToCondition(comp->mir()->compareType(), comp->jsop());
    const LAllocation *left = comp->getOperand(0);
    const LAllocation *right = comp->getOperand(1);
    const LDefinition *def = comp->getDef(0);

    if (right->isConstant())
        masm.cmp32Set(cond, ToRegister(left), Imm32(ToInt32(right)), ToRegister(def));
    else if (right->isGeneralReg())
        masm.cmp32Set(cond, ToRegister(left), ToRegister(right), ToRegister(def));
    else
        masm.cmp32Set(cond, ToRegister(left), ToAddress(right), ToRegister(def));

    return true;
}

bool
CodeGeneratorMIPS::visitCompareAndBranch(LCompareAndBranch *comp)
{
    Assembler::Condition cond = JSOpToCondition(comp->cmpMir()->compareType(), comp->jsop());
    if (comp->right()->isConstant()) {
        emitBranch(ToRegister(comp->left()), Imm32(ToInt32(comp->right())), cond,
                   comp->ifTrue(), comp->ifFalse());
    } else if (comp->right()->isGeneralReg()) {
        emitBranch(ToRegister(comp->left()), ToRegister(comp->right()), cond,
                   comp->ifTrue(), comp->ifFalse());
    } else {
        emitBranch(ToRegister(comp->left()), ToAddress(comp->right()), cond,
                   comp->ifTrue(), comp->ifFalse());
    }

    return true;
}

bool
CodeGeneratorMIPS::generateOutOfLineCode()
{
    if (!CodeGeneratorShared::generateOutOfLineCode())
        return false;

    if (deoptLabel_.used()) {
        // All non-table-based bailouts will go here.
        masm.bind(&deoptLabel_);

        // Push the frame size, so the handler can recover the IonScript.
        // Frame size is stored in 'ra' and pushed by GenerateBailoutThunk
        // We have to use 'ra' because generateBailoutTable will implicitly do
        // the same.
        masm.move32(Imm32(frameSize()), ra);

        JitCode *handler = gen->jitRuntime()->getGenericBailoutHandler();

        masm.branch(handler);
    }

    return true;
}

bool
CodeGeneratorMIPS::bailoutFrom(Label *label, LSnapshot *snapshot)
{
    if (masm.bailed())
        return false;
    MOZ_ASSERT(label->used());
    MOZ_ASSERT(!label->bound());

    if (!encode(snapshot))
        return false;

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense. We can't use bailout tables if the stack
    // isn't properly aligned to the static frame size.
    MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None(),
                  frameClass_.frameSize() == masm.framePushed());

    // We don't use table bailouts because retargeting is easier this way.
    OutOfLineBailout *ool = new(alloc()) OutOfLineBailout(snapshot, masm.framePushed());
    if (!addOutOfLineCode(ool)) {
        return false;
    }

    masm.retarget(label, ool->entry());

    return true;
}

bool
CodeGeneratorMIPS::bailout(LSnapshot *snapshot)
{
    Label label;
    masm.jump(&label);
    return bailoutFrom(&label, snapshot);
}

bool
CodeGeneratorMIPS::visitOutOfLineBailout(OutOfLineBailout *ool)
{
    // Push snapshotOffset and make sure stack is aligned.
    masm.subPtr(Imm32(2 * sizeof(void *)), StackPointer);
    masm.storePtr(ImmWord(ool->snapshot()->snapshotOffset()), Address(StackPointer, 0));

    masm.jump(&deoptLabel_);
    return true;
}

bool
CodeGeneratorMIPS::visitMinMaxD(LMinMaxD *ins)
{
    FloatRegister first = ToFloatRegister(ins->first());
    FloatRegister second = ToFloatRegister(ins->second());
    FloatRegister output = ToFloatRegister(ins->output());

    MOZ_ASSERT(first == output);

    Assembler::DoubleCondition cond = ins->mir()->isMax()
                                      ? Assembler::DoubleLessThanOrEqual
                                      : Assembler::DoubleGreaterThanOrEqual;
    Label nan, equal, returnSecond, done;

    // First or second is NaN, result is NaN.
    masm.ma_bc1d(first, second, &nan, Assembler::DoubleUnordered, ShortJump);
    // Make sure we handle -0 and 0 right.
    masm.ma_bc1d(first, second, &equal, Assembler::DoubleEqual, ShortJump);
    masm.ma_bc1d(first, second, &returnSecond, cond, ShortJump);
    masm.ma_b(&done, ShortJump);

    // Check for zero.
    masm.bind(&equal);
    masm.loadConstantDouble(0.0, ScratchFloatReg);
    // First wasn't 0 or -0, so just return it.
    masm.ma_bc1d(first, ScratchFloatReg, &done, Assembler::DoubleNotEqualOrUnordered, ShortJump);

    // So now both operands are either -0 or 0.
    if (ins->mir()->isMax()) {
        // -0 + -0 = -0 and -0 + 0 = 0.
        masm.addDouble(second, first);
    } else {
        masm.negateDouble(first);
        masm.subDouble(second, first);
        masm.negateDouble(first);
    }
    masm.ma_b(&done, ShortJump);

    masm.bind(&nan);
    masm.loadConstantDouble(GenericNaN(), output);
    masm.ma_b(&done, ShortJump);

    masm.bind(&returnSecond);
    masm.moveDouble(second, output);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitAbsD(LAbsD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));
    masm.as_absd(input, input);
    return true;
}

bool
CodeGeneratorMIPS::visitAbsF(LAbsF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));
    masm.as_abss(input, input);
    return true;
}

bool
CodeGeneratorMIPS::visitSqrtD(LSqrtD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.as_sqrtd(output, input);
    return true;
}

bool
CodeGeneratorMIPS::visitSqrtF(LSqrtF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.as_sqrts(output, input);
    return true;
}

bool
CodeGeneratorMIPS::visitAddI(LAddI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);

    MOZ_ASSERT(rhs->isConstant() || rhs->isGeneralReg());

    // If there is no snapshot, we don't need to check for overflow
    if (!ins->snapshot()) {
        if (rhs->isConstant())
            masm.ma_addu(ToRegister(dest), ToRegister(lhs), Imm32(ToInt32(rhs)));
        else
            masm.as_addu(ToRegister(dest), ToRegister(lhs), ToRegister(rhs));
        return true;
    }

    Label overflow;
    if (rhs->isConstant())
        masm.ma_addTestOverflow(ToRegister(dest), ToRegister(lhs), Imm32(ToInt32(rhs)), &overflow);
    else
        masm.ma_addTestOverflow(ToRegister(dest), ToRegister(lhs), ToRegister(rhs), &overflow);

    if (!bailoutFrom(&overflow, ins->snapshot()))
        return false;

    return true;
}

bool
CodeGeneratorMIPS::visitSubI(LSubI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);

    MOZ_ASSERT(rhs->isConstant() || rhs->isGeneralReg());

    // If there is no snapshot, we don't need to check for overflow
    if (!ins->snapshot()) {
        if (rhs->isConstant())
            masm.ma_subu(ToRegister(dest), ToRegister(lhs), Imm32(ToInt32(rhs)));
        else
            masm.as_subu(ToRegister(dest), ToRegister(lhs), ToRegister(rhs));
        return true;
    }

    Label overflow;
    if (rhs->isConstant())
        masm.ma_subTestOverflow(ToRegister(dest), ToRegister(lhs), Imm32(ToInt32(rhs)), &overflow);
    else
        masm.ma_subTestOverflow(ToRegister(dest), ToRegister(lhs), ToRegister(rhs), &overflow);

    if (!bailoutFrom(&overflow, ins->snapshot()))
        return false;

    return true;
}

bool
CodeGeneratorMIPS::visitMulI(LMulI *ins)
{
    const LAllocation *lhs = ins->lhs();
    const LAllocation *rhs = ins->rhs();
    Register dest = ToRegister(ins->output());
    MMul *mul = ins->mir();

    MOZ_ASSERT_IF(mul->mode() == MMul::Integer, !mul->canBeNegativeZero() && !mul->canOverflow());

    if (rhs->isConstant()) {
        int32_t constant = ToInt32(rhs);
        Register src = ToRegister(lhs);

        // Bailout on -0.0
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition cond = (constant == 0) ? Assembler::LessThan : Assembler::Equal;
            if (!bailoutCmp32(cond, src, Imm32(0), ins->snapshot()))
                return false;
        }

        switch (constant) {
          case -1:
            if (mul->canOverflow()) {
                if (!bailoutCmp32(Assembler::Equal, src, Imm32(INT32_MIN), ins->snapshot()))
                    return false;
            }
            masm.ma_negu(dest, src);
            break;
          case 0:
            masm.move32(Imm32(0), dest);
            break;
          case 1:
            masm.move32(src, dest);
            break;
          case 2:
            if (mul->canOverflow()) {
                Label mulTwoOverflow;
                masm.ma_addTestOverflow(dest, src, src, &mulTwoOverflow);

                if (!bailoutFrom(&mulTwoOverflow, ins->snapshot()))
                    return false;
            } else {
                masm.as_addu(dest, src, src);
            }
            break;
          default:
            uint32_t shift = FloorLog2(constant);

            if (!mul->canOverflow() && (constant > 0)) {
                // If it cannot overflow, we can do lots of optimizations.
                uint32_t rest = constant - (1 << shift);

                // See if the constant has one bit set, meaning it can be
                // encoded as a bitshift.
                if ((1 << shift) == constant) {
                    masm.ma_sll(dest, src, Imm32(shift));
                    return true;
                }

                // If the constant cannot be encoded as (1<<C1), see if it can
                // be encoded as (1<<C1) | (1<<C2), which can be computed
                // using an add and a shift.
                uint32_t shift_rest = FloorLog2(rest);
                if (src != dest && (1u << shift_rest) == rest) {
                    masm.ma_sll(dest, src, Imm32(shift - shift_rest));
                    masm.add32(src, dest);
                    if (shift_rest != 0)
                        masm.ma_sll(dest, dest, Imm32(shift_rest));
                    return true;
                }
            }

            if (mul->canOverflow() && (constant > 0) && (src != dest)) {
                // To stay on the safe side, only optimize things that are a
                // power of 2.

                if ((1 << shift) == constant) {
                    // dest = lhs * pow(2, shift)
                    masm.ma_sll(dest, src, Imm32(shift));
                    // At runtime, check (lhs == dest >> shift), if this does
                    // not hold, some bits were lost due to overflow, and the
                    // computation should be resumed as a double.
                    masm.ma_sra(ScratchRegister, dest, Imm32(shift));
                    if (!bailoutCmp32(Assembler::NotEqual, src, ScratchRegister, ins->snapshot()))
                        return false;
                    return true;
                }
            }

            if (mul->canOverflow()) {
                Label mulConstOverflow;
                masm.ma_mul_branch_overflow(dest, ToRegister(lhs), Imm32(ToInt32(rhs)),
                                            &mulConstOverflow);

                if (!bailoutFrom(&mulConstOverflow, ins->snapshot()))
                    return false;
            } else {
                masm.ma_mult(src, Imm32(ToInt32(rhs)));
                masm.as_mflo(dest);
            }
            break;
        }
    } else {
        Label multRegOverflow;

        if (mul->canOverflow()) {
            masm.ma_mul_branch_overflow(dest, ToRegister(lhs), ToRegister(rhs), &multRegOverflow);
            if (!bailoutFrom(&multRegOverflow, ins->snapshot()))
                return false;
        } else {
            masm.as_mult(ToRegister(lhs), ToRegister(rhs));
            masm.as_mflo(dest);
        }

        if (mul->canBeNegativeZero()) {
            Label done;
            masm.ma_b(dest, dest, &done, Assembler::NonZero, ShortJump);

            // Result is -0 if lhs or rhs is negative.
            // In that case result must be double value so bailout
            Register scratch = SecondScratchReg;
            masm.ma_or(scratch, ToRegister(lhs), ToRegister(rhs));
            if (!bailoutCmp32(Assembler::Signed, scratch, scratch, ins->snapshot()))
                return false;

            masm.bind(&done);
        }
    }

    return true;
}

bool
CodeGeneratorMIPS::visitDivI(LDivI *ins)
{
    // Extract the registers from this instruction
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register dest = ToRegister(ins->output());
    Register temp = ToRegister(ins->getTemp(0));
    MDiv *mir = ins->mir();

    Label done;

    // Handle divide by zero.
    if (mir->canBeDivideByZero()) {
        if (mir->canTruncateInfinities()) {
            // Truncated division by zero is zero (Infinity|0 == 0)
            Label notzero;
            masm.ma_b(rhs, rhs, &notzero, Assembler::NonZero, ShortJump);
            masm.move32(Imm32(0), dest);
            masm.ma_b(&done, ShortJump);
            masm.bind(&notzero);
        } else {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutCmp32(Assembler::Zero, rhs, rhs, ins->snapshot()))
                return false;
        }
    }

    // Handle an integer overflow exception from -2147483648 / -1.
    if (mir->canBeNegativeOverflow()) {
        Label notMinInt;
        masm.move32(Imm32(INT32_MIN), temp);
        masm.ma_b(lhs, temp, &notMinInt, Assembler::NotEqual, ShortJump);

        masm.move32(Imm32(-1), temp);
        if (mir->canTruncateOverflow()) {
            // (-INT32_MIN)|0 == INT32_MIN
            Label skip;
            masm.ma_b(rhs, temp, &skip, Assembler::NotEqual, ShortJump);
            masm.move32(Imm32(INT32_MIN), dest);
            masm.ma_b(&done, ShortJump);
            masm.bind(&skip);
        } else {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutCmp32(Assembler::Equal, rhs, temp, ins->snapshot()))
                return false;
        }
        masm.bind(&notMinInt);
    }

    // Handle negative 0. (0/-Y)
    if (!mir->canTruncateNegativeZero() && mir->canBeNegativeZero()) {
        Label nonzero;
        masm.ma_b(lhs, lhs, &nonzero, Assembler::NonZero, ShortJump);
        if (!bailoutCmp32(Assembler::LessThan, rhs, Imm32(0), ins->snapshot()))
            return false;
        masm.bind(&nonzero);
    }
    // Note: above safety checks could not be verified as Ion seems to be
    // smarter and requires double arithmetic in such cases.

    // All regular. Lets call div.
    if (mir->canTruncateRemainder()) {
        masm.as_div(lhs, rhs);
        masm.as_mflo(dest);
    } else {
        MOZ_ASSERT(mir->fallible());

        Label remainderNonZero;
        masm.ma_div_branch_overflow(dest, lhs, rhs, &remainderNonZero);
        if (!bailoutFrom(&remainderNonZero, ins->snapshot()))
            return false;
    }

    masm.bind(&done);

    return true;
}

bool
CodeGeneratorMIPS::visitDivPowTwoI(LDivPowTwoI *ins)
{
    Register lhs = ToRegister(ins->numerator());
    Register dest = ToRegister(ins->output());
    Register tmp = ToRegister(ins->getTemp(0));
    int32_t shift = ins->shift();

    if (shift != 0) {
        MDiv *mir = ins->mir();
        if (!mir->isTruncated()) {
            // If the remainder is going to be != 0, bailout since this must
            // be a double.
            masm.ma_sll(tmp, lhs, Imm32(32 - shift));
            if (!bailoutCmp32(Assembler::NonZero, tmp, tmp, ins->snapshot()))
                return false;
        }

        if (!mir->canBeNegativeDividend()) {
            // Numerator is unsigned, so needs no adjusting. Do the shift.
            masm.ma_sra(dest, lhs, Imm32(shift));
            return true;
        }

        // Adjust the value so that shifting produces a correctly rounded result
        // when the numerator is negative. See 10-1 "Signed Division by a Known
        // Power of 2" in Henry S. Warren, Jr.'s Hacker's Delight.
        if (shift > 1) {
            masm.ma_sra(tmp, lhs, Imm32(31));
            masm.ma_srl(tmp, tmp, Imm32(32 - shift));
            masm.add32(lhs, tmp);
        } else {
            masm.ma_srl(tmp, lhs, Imm32(32 - shift));
            masm.add32(lhs, tmp);
        }

        // Do the shift.
        masm.ma_sra(dest, tmp, Imm32(shift));
    } else {
        masm.move32(lhs, dest);
    }

    return true;

}

bool
CodeGeneratorMIPS::visitModI(LModI *ins)
{
    // Extract the registers from this instruction
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register dest = ToRegister(ins->output());
    Register callTemp = ToRegister(ins->callTemp());
    MMod *mir = ins->mir();
    Label done, prevent;

    masm.move32(lhs, callTemp);

    // Prevent INT_MIN % -1;
    // The integer division will give INT_MIN, but we want -(double)INT_MIN.
    if (mir->canBeNegativeDividend()) {
        masm.ma_b(lhs, Imm32(INT_MIN), &prevent, Assembler::NotEqual, ShortJump);
        if (mir->isTruncated()) {
            // (INT_MIN % -1)|0 == 0
            Label skip;
            masm.ma_b(rhs, Imm32(-1), &skip, Assembler::NotEqual, ShortJump);
            masm.move32(Imm32(0), dest);
            masm.ma_b(&done, ShortJump);
            masm.bind(&skip);
        } else {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutCmp32(Assembler::Equal, rhs, Imm32(-1), ins->snapshot()))
                return false;
        }
        masm.bind(&prevent);
    }

    // 0/X (with X < 0) is bad because both of these values *should* be
    // doubles, and the result should be -0.0, which cannot be represented in
    // integers. X/0 is bad because it will give garbage (or abort), when it
    // should give either \infty, -\infty or NAN.

    // Prevent 0 / X (with X < 0) and X / 0
    // testing X / Y.  Compare Y with 0.
    // There are three cases: (Y < 0), (Y == 0) and (Y > 0)
    // If (Y < 0), then we compare X with 0, and bail if X == 0
    // If (Y == 0), then we simply want to bail.
    // if (Y > 0), we don't bail.

    if (mir->canBeDivideByZero()) {
        if (mir->isTruncated()) {
            Label skip;
            masm.ma_b(rhs, Imm32(0), &skip, Assembler::NotEqual, ShortJump);
            masm.move32(Imm32(0), dest);
            masm.ma_b(&done, ShortJump);
            masm.bind(&skip);
        } else {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutCmp32(Assembler::Equal, rhs, Imm32(0), ins->snapshot()))
                return false;
        }
    }

    if (mir->canBeNegativeDividend()) {
        Label notNegative;
        masm.ma_b(rhs, Imm32(0), &notNegative, Assembler::GreaterThan, ShortJump);
        if (mir->isTruncated()) {
            // NaN|0 == 0 and (0 % -X)|0 == 0
            Label skip;
            masm.ma_b(lhs, Imm32(0), &skip, Assembler::NotEqual, ShortJump);
            masm.move32(Imm32(0), dest);
            masm.ma_b(&done, ShortJump);
            masm.bind(&skip);
        } else {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutCmp32(Assembler::Equal, lhs, Imm32(0), ins->snapshot()))
                return false;
        }
        masm.bind(&notNegative);
    }

    masm.as_div(lhs, rhs);
    masm.as_mfhi(dest);

    // If X%Y == 0 and X < 0, then we *actually* wanted to return -0.0
    if (mir->canBeNegativeDividend()) {
        if (mir->isTruncated()) {
            // -0.0|0 == 0
        } else {
            MOZ_ASSERT(mir->fallible());
            // See if X < 0
            masm.ma_b(dest, Imm32(0), &done, Assembler::NotEqual, ShortJump);
            if (!bailoutCmp32(Assembler::Signed, callTemp, Imm32(0), ins->snapshot()))
                return false;
        }
    }
    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitModPowTwoI(LModPowTwoI *ins)
{
    Register in = ToRegister(ins->getOperand(0));
    Register out = ToRegister(ins->getDef(0));
    MMod *mir = ins->mir();
    Label negative, done;

    masm.move32(in, out);
    masm.ma_b(in, in, &done, Assembler::Zero, ShortJump);
    // Switch based on sign of the lhs.
    // Positive numbers are just a bitmask
    masm.ma_b(in, in, &negative, Assembler::Signed, ShortJump);
    {
        masm.and32(Imm32((1 << ins->shift()) - 1), out);
        masm.ma_b(&done, ShortJump);
    }

    // Negative numbers need a negate, bitmask, negate
    {
        masm.bind(&negative);
        masm.neg32(out);
        masm.and32(Imm32((1 << ins->shift()) - 1), out);
        masm.neg32(out);
    }
    if (mir->canBeNegativeDividend()) {
        if (!mir->isTruncated()) {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutCmp32(Assembler::Equal, out, zero, ins->snapshot()))
                return false;
        } else {
            // -0|0 == 0
        }
    }
    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitModMaskI(LModMaskI *ins)
{
    Register src = ToRegister(ins->getOperand(0));
    Register dest = ToRegister(ins->getDef(0));
    Register tmp = ToRegister(ins->getTemp(0));
    MMod *mir = ins->mir();

    if (!mir->isTruncated() && mir->canBeNegativeDividend()) {
        MOZ_ASSERT(mir->fallible());

        Label bail;
        masm.ma_mod_mask(src, dest, tmp, ins->shift(), &bail);
        if (!bailoutFrom(&bail, ins->snapshot()))
            return false;
    } else {
        masm.ma_mod_mask(src, dest, tmp, ins->shift(), nullptr);
    }
    return true;
}
bool
CodeGeneratorMIPS::visitBitNotI(LBitNotI *ins)
{
    const LAllocation *input = ins->getOperand(0);
    const LDefinition *dest = ins->getDef(0);
    MOZ_ASSERT(!input->isConstant());

    masm.ma_not(ToRegister(dest), ToRegister(input));
    return true;
}

bool
CodeGeneratorMIPS::visitBitOpI(LBitOpI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);
    // all of these bitops should be either imm32's, or integer registers.
    switch (ins->bitop()) {
      case JSOP_BITOR:
        if (rhs->isConstant())
            masm.ma_or(ToRegister(dest), ToRegister(lhs), Imm32(ToInt32(rhs)));
        else
            masm.ma_or(ToRegister(dest), ToRegister(lhs), ToRegister(rhs));
        break;
      case JSOP_BITXOR:
        if (rhs->isConstant())
            masm.ma_xor(ToRegister(dest), ToRegister(lhs), Imm32(ToInt32(rhs)));
        else
            masm.ma_xor(ToRegister(dest), ToRegister(lhs), ToRegister(rhs));
        break;
      case JSOP_BITAND:
        if (rhs->isConstant())
            masm.ma_and(ToRegister(dest), ToRegister(lhs), Imm32(ToInt32(rhs)));
        else
            masm.ma_and(ToRegister(dest), ToRegister(lhs), ToRegister(rhs));
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected binary opcode");
    }

    return true;
}

bool
CodeGeneratorMIPS::visitShiftI(LShiftI *ins)
{
    Register lhs = ToRegister(ins->lhs());
    const LAllocation *rhs = ins->rhs();
    Register dest = ToRegister(ins->output());

    if (rhs->isConstant()) {
        int32_t shift = ToInt32(rhs) & 0x1F;
        switch (ins->bitop()) {
          case JSOP_LSH:
            if (shift)
                masm.ma_sll(dest, lhs, Imm32(shift));
            else
                masm.move32(lhs, dest);
            break;
          case JSOP_RSH:
            if (shift)
                masm.ma_sra(dest, lhs, Imm32(shift));
            else
                masm.move32(lhs, dest);
            break;
          case JSOP_URSH:
            if (shift) {
                masm.ma_srl(dest, lhs, Imm32(shift));
            } else {
                // x >>> 0 can overflow.
                masm.move32(lhs, dest);
                if (ins->mir()->toUrsh()->fallible()) {
                    if (!bailoutCmp32(Assembler::LessThan, dest, Imm32(0), ins->snapshot()))
                        return false;
                }
            }
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unexpected shift op");
        }
    } else {
        // The shift amounts should be AND'ed into the 0-31 range
        masm.ma_and(dest, ToRegister(rhs), Imm32(0x1F));

        switch (ins->bitop()) {
          case JSOP_LSH:
            masm.ma_sll(dest, lhs, dest);
            break;
          case JSOP_RSH:
            masm.ma_sra(dest, lhs, dest);
            break;
          case JSOP_URSH:
            masm.ma_srl(dest, lhs, dest);
            if (ins->mir()->toUrsh()->fallible()) {
                // x >>> 0 can overflow.
                if (!bailoutCmp32(Assembler::LessThan, dest, Imm32(0), ins->snapshot()))
                    return false;
            }
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unexpected shift op");
        }
    }

    return true;
}

bool
CodeGeneratorMIPS::visitUrshD(LUrshD *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register temp = ToRegister(ins->temp());

    const LAllocation *rhs = ins->rhs();
    FloatRegister out = ToFloatRegister(ins->output());

    if (rhs->isConstant()) {
        masm.ma_srl(temp, lhs, Imm32(ToInt32(rhs)));
    } else {
        masm.ma_srl(temp, lhs, ToRegister(rhs));
    }

    masm.convertUInt32ToDouble(temp, out);
    return true;
}

bool
CodeGeneratorMIPS::visitPowHalfD(LPowHalfD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());

    Label done, skip;

    // Masm.pow(-Infinity, 0.5) == Infinity.
    masm.loadConstantDouble(NegativeInfinity<double>(), ScratchFloatReg);
    masm.ma_bc1d(input, ScratchFloatReg, &skip, Assembler::DoubleNotEqualOrUnordered, ShortJump);
    masm.as_negd(output, ScratchFloatReg);
    masm.ma_b(&done, ShortJump);

    masm.bind(&skip);
    // Math.pow(-0, 0.5) == 0 == Math.pow(0, 0.5).
    // Adding 0 converts any -0 to 0.
    masm.loadConstantDouble(0.0, ScratchFloatReg);
    masm.as_addd(output, input, ScratchFloatReg);
    masm.as_sqrtd(output, output);

    masm.bind(&done);
    return true;
}

MoveOperand
CodeGeneratorMIPS::toMoveOperand(const LAllocation *a) const
{
    if (a->isGeneralReg())
        return MoveOperand(ToRegister(a));
    if (a->isFloatReg()) {
        return MoveOperand(ToFloatRegister(a));
    }
    int32_t offset = ToStackOffset(a);
    MOZ_ASSERT((offset & 3) == 0);

    return MoveOperand(StackPointer, offset);
}

class js::jit::OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorMIPS>
{
    MTableSwitch *mir_;
    CodeLabel jumpLabel_;

    bool accept(CodeGeneratorMIPS *codegen) {
        return codegen->visitOutOfLineTableSwitch(this);
    }

  public:
    OutOfLineTableSwitch(MTableSwitch *mir)
      : mir_(mir)
    {}

    MTableSwitch *mir() const {
        return mir_;
    }

    CodeLabel *jumpLabel() {
        return &jumpLabel_;
    }
};

bool
CodeGeneratorMIPS::visitOutOfLineTableSwitch(OutOfLineTableSwitch *ool)
{
    MTableSwitch *mir = ool->mir();

    masm.align(sizeof(void*));
    masm.bind(ool->jumpLabel()->src());
    if (!masm.addCodeLabel(*ool->jumpLabel()))
        return false;

    for (size_t i = 0; i < mir->numCases(); i++) {
        LBlock *caseblock = skipTrivialBlocks(mir->getCase(i))->lir();
        Label *caseheader = caseblock->label();
        uint32_t caseoffset = caseheader->offset();

        // The entries of the jump table need to be absolute addresses and thus
        // must be patched after codegen is finished.
        CodeLabel cl;
        masm.ma_li(ScratchRegister, cl.dest());
        masm.branch(ScratchRegister);
        cl.src()->bind(caseoffset);
        if (!masm.addCodeLabel(cl))
            return false;
    }

    return true;
}

bool
CodeGeneratorMIPS::emitTableSwitchDispatch(MTableSwitch *mir, Register index,
                                           Register address)
{
    Label *defaultcase = skipTrivialBlocks(mir->getDefault())->lir()->label();

    // Lower value with low value
    if (mir->low() != 0)
        masm.subPtr(Imm32(mir->low()), index);

    // Jump to default case if input is out of range
    int32_t cases = mir->numCases();
    masm.branchPtr(Assembler::AboveOrEqual, index, ImmWord(cases), defaultcase);

    // To fill in the CodeLabels for the case entries, we need to first
    // generate the case entries (we don't yet know their offsets in the
    // instruction stream).
    OutOfLineTableSwitch *ool = new(alloc()) OutOfLineTableSwitch(mir);
    if (!addOutOfLineCode(ool))
        return false;

    // Compute the position where a pointer to the right case stands.
    masm.ma_li(address, ool->jumpLabel()->dest());
    masm.lshiftPtr(Imm32(4), index);
    masm.addPtr(index, address);

    masm.branch(address);
    return true;
}

bool
CodeGeneratorMIPS::visitMathD(LMathD *math)
{
    const LAllocation *src1 = math->getOperand(0);
    const LAllocation *src2 = math->getOperand(1);
    const LDefinition *output = math->getDef(0);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.as_addd(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      case JSOP_SUB:
        masm.as_subd(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      case JSOP_MUL:
        masm.as_muld(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      case JSOP_DIV:
        masm.as_divd(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected opcode");
    }
    return true;
}

bool
CodeGeneratorMIPS::visitMathF(LMathF *math)
{
    const LAllocation *src1 = math->getOperand(0);
    const LAllocation *src2 = math->getOperand(1);
    const LDefinition *output = math->getDef(0);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.as_adds(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      case JSOP_SUB:
        masm.as_subs(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      case JSOP_MUL:
        masm.as_muls(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      case JSOP_DIV:
        masm.as_divs(ToFloatRegister(output), ToFloatRegister(src1), ToFloatRegister(src2));
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected opcode");
    }
    return true;
}

bool
CodeGeneratorMIPS::visitFloor(LFloor *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    Label skipCheck, done;

    // If Nan, 0 or -0 check for bailout
    masm.loadConstantDouble(0.0, scratch);
    masm.ma_bc1d(input, scratch, &skipCheck, Assembler::DoubleNotEqual, ShortJump);

    // If high part is not zero, it is NaN or -0, so we bail.
    masm.moveFromDoubleHi(input, SecondScratchReg);
    if (!bailoutCmp32(Assembler::NotEqual, SecondScratchReg, Imm32(0), lir->snapshot()))
        return false;

    // Input was zero, so return zero.
    masm.move32(Imm32(0), output);
    masm.ma_b(&done, ShortJump);

    masm.bind(&skipCheck);
    masm.as_floorwd(scratch, input);
    masm.moveFromDoubleLo(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot()))
        return false;

    masm.bind(&done);

    return true;
}

bool
CodeGeneratorMIPS::visitFloorF(LFloorF *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    Label skipCheck, done;

    // If Nan, 0 or -0 check for bailout
    masm.loadConstantFloat32(0.0, scratch);
    masm.ma_bc1s(input, scratch, &skipCheck, Assembler::DoubleNotEqual, ShortJump);

    // If binary value is not zero, it is NaN or -0, so we bail.
    masm.moveFromDoubleLo(input, SecondScratchReg);
    if (!bailoutCmp32(Assembler::NotEqual, SecondScratchReg, Imm32(0), lir->snapshot()))
        return false;

    // Input was zero, so return zero.
    masm.move32(Imm32(0), output);
    masm.ma_b(&done, ShortJump);

    masm.bind(&skipCheck);
    masm.as_floorws(scratch, input);
    masm.moveFromDoubleLo(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot()))
        return false;

    masm.bind(&done);

    return true;
}

bool
CodeGeneratorMIPS::visitCeil(LCeil *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    Label performCeil, done;

    // If x < -1 or x > 0 then perform ceil.
    masm.loadConstantDouble(0, scratch);
    masm.branchDouble(Assembler::DoubleGreaterThan, input, scratch, &performCeil);
    masm.loadConstantDouble(-1, scratch);
    masm.branchDouble(Assembler::DoubleLessThanOrEqual, input, scratch, &performCeil);

    // If high part is not zero, the input was not 0, so we bail.
    masm.moveFromDoubleHi(input, SecondScratchReg);
    if (!bailoutCmp32(Assembler::NotEqual, SecondScratchReg, Imm32(0), lir->snapshot()))
        return false;

    // Input was zero, so return zero.
    masm.move32(Imm32(0), output);
    masm.ma_b(&done, ShortJump);

    masm.bind(&performCeil);
    masm.as_ceilwd(scratch, input);
    masm.moveFromDoubleLo(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;
    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot()))
        return false;

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitCeilF(LCeilF *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    Label performCeil, done;

    // If x < -1 or x > 0 then perform ceil.
    masm.loadConstantFloat32(0, scratch);
    masm.branchFloat(Assembler::DoubleGreaterThan, input, scratch, &performCeil);
    masm.loadConstantFloat32(-1, scratch);
    masm.branchFloat(Assembler::DoubleLessThanOrEqual, input, scratch, &performCeil);

    // If binary value is not zero, the input was not 0, so we bail.
    masm.moveFromFloat32(input, SecondScratchReg);
    if (!bailoutCmp32(Assembler::NotEqual, SecondScratchReg, Imm32(0), lir->snapshot()))
        return false;

    // Input was zero, so return zero.
    masm.move32(Imm32(0), output);
    masm.ma_b(&done, ShortJump);

    masm.bind(&performCeil);
    masm.as_ceilws(scratch, input);
    masm.moveFromFloat32(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;
    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot()))
        return false;

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitRound(LRound *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister temp = ToFloatRegister(lir->temp());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    Label bail, negative, end, skipCheck;

    // Load 0.5 in the temp register.
    masm.loadConstantDouble(0.5, temp);

    // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
    masm.loadConstantDouble(0.0, scratch);
    masm.ma_bc1d(input, scratch, &negative, Assembler::DoubleLessThan, ShortJump);

    // If Nan, 0 or -0 check for bailout
    masm.ma_bc1d(input, scratch, &skipCheck, Assembler::DoubleNotEqual, ShortJump);

    // If high part is not zero, it is NaN or -0, so we bail.
    masm.moveFromDoubleHi(input, SecondScratchReg);
    if (!bailoutCmp32(Assembler::NotEqual, SecondScratchReg, Imm32(0), lir->snapshot()))
        return false;

    // Input was zero, so return zero.
    masm.move32(Imm32(0), output);
    masm.ma_b(&end, ShortJump);

    masm.bind(&skipCheck);
    masm.loadConstantDouble(0.5, scratch);
    masm.addDouble(input, scratch);
    masm.as_floorwd(scratch, scratch);

    masm.moveFromDoubleLo(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot()))
        return false;

    masm.jump(&end);

    // Input is negative, but isn't -0.
    masm.bind(&negative);
    masm.addDouble(input, temp);

    // If input + 0.5 >= 0, input is a negative number >= -0.5 and the
    // result is -0.
    masm.branchDouble(Assembler::DoubleGreaterThanOrEqual, temp, scratch, &bail);
    if (!bailoutFrom(&bail, lir->snapshot()))
        return false;

    // Truncate and round toward zero.
    // This is off-by-one for everything but integer-valued inputs.
    masm.as_floorwd(scratch, temp);
    masm.moveFromDoubleLo(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;

    masm.bind(&end);
    return true;
}

bool
CodeGeneratorMIPS::visitRoundF(LRoundF *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister temp = ToFloatRegister(lir->temp());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    Label bail, negative, end, skipCheck;

    // Load 0.5 in the temp register.
    masm.loadConstantFloat32(0.5, temp);

    // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
    masm.loadConstantFloat32(0.0, scratch);
    masm.ma_bc1s(input, scratch, &negative, Assembler::DoubleLessThan, ShortJump);

    // If Nan, 0 or -0 check for bailout
    masm.ma_bc1s(input, scratch, &skipCheck, Assembler::DoubleNotEqual, ShortJump);

    // If binary value is not zero, it is NaN or -0, so we bail.
    masm.moveFromFloat32(input, SecondScratchReg);
    if (!bailoutCmp32(Assembler::NotEqual, SecondScratchReg, Imm32(0), lir->snapshot()))
        return false;

    // Input was zero, so return zero.
    masm.move32(Imm32(0), output);
    masm.ma_b(&end, ShortJump);

    masm.bind(&skipCheck);
    masm.loadConstantFloat32(0.5, scratch);
    masm.as_adds(scratch, input, scratch);
    masm.as_floorws(scratch, scratch);

    masm.moveFromFloat32(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MAX), lir->snapshot()))
        return false;

    masm.jump(&end);

    // Input is negative, but isn't -0.
    masm.bind(&negative);
    masm.as_adds(temp, input, temp);

    // If input + 0.5 >= 0, input is a negative number >= -0.5 and the
    // result is -0.
    masm.branchFloat(Assembler::DoubleGreaterThanOrEqual, temp, scratch, &bail);
    if (!bailoutFrom(&bail, lir->snapshot()))
        return false;

    // Truncate and round toward zero.
    // This is off-by-one for everything but integer-valued inputs.
    masm.as_floorws(scratch, temp);
    masm.moveFromFloat32(scratch, output);

    if (!bailoutCmp32(Assembler::Equal, output, Imm32(INT_MIN), lir->snapshot()))
        return false;

    masm.bind(&end);
    return true;
}

bool
CodeGeneratorMIPS::visitTruncateDToInt32(LTruncateDToInt32 *ins)
{
    return emitTruncateDouble(ToFloatRegister(ins->input()), ToRegister(ins->output()));
}

bool
CodeGeneratorMIPS::visitTruncateFToInt32(LTruncateFToInt32 *ins)
{
    return emitTruncateFloat32(ToFloatRegister(ins->input()), ToRegister(ins->output()));
}

static const uint32_t FrameSizes[] = { 128, 256, 512, 1024 };

FrameSizeClass
FrameSizeClass::FromDepth(uint32_t frameDepth)
{
    for (uint32_t i = 0; i < JS_ARRAY_LENGTH(FrameSizes); i++) {
        if (frameDepth < FrameSizes[i])
            return FrameSizeClass(i);
    }

    return FrameSizeClass::None();
}

FrameSizeClass
FrameSizeClass::ClassLimit()
{
    return FrameSizeClass(JS_ARRAY_LENGTH(FrameSizes));
}

uint32_t
FrameSizeClass::frameSize() const
{
    MOZ_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);
    MOZ_ASSERT(class_ < JS_ARRAY_LENGTH(FrameSizes));

    return FrameSizes[class_];
}

ValueOperand
CodeGeneratorMIPS::ToValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorMIPS::ToOutValue(LInstruction *ins)
{
    Register typeReg = ToRegister(ins->getDef(TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getDef(PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorMIPS::ToTempValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getTemp(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getTemp(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

bool
CodeGeneratorMIPS::visitValue(LValue *value)
{
    const ValueOperand out = ToOutValue(value);

    masm.moveValue(value->value(), out);
    return true;
}

bool
CodeGeneratorMIPS::visitBox(LBox *box)
{
    const LDefinition *type = box->getDef(TYPE_INDEX);

    MOZ_ASSERT(!box->getOperand(0)->isConstant());

    // For NUNBOX32, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    masm.move32(Imm32(MIRTypeToTag(box->type())), ToRegister(type));
    return true;
}

bool
CodeGeneratorMIPS::visitBoxFloatingPoint(LBoxFloatingPoint *box)
{
    const LDefinition *payload = box->getDef(PAYLOAD_INDEX);
    const LDefinition *type = box->getDef(TYPE_INDEX);
    const LAllocation *in = box->getOperand(0);

    FloatRegister reg = ToFloatRegister(in);
    if (box->type() == MIRType_Float32) {
        masm.convertFloat32ToDouble(reg, ScratchFloatReg);
        reg = ScratchFloatReg;
    }
    masm.ma_mv(reg, ValueOperand(ToRegister(type), ToRegister(payload)));
    return true;
}

bool
CodeGeneratorMIPS::visitUnbox(LUnbox *unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox *mir = unbox->mir();
    Register type = ToRegister(unbox->type());

    if (mir->fallible()) {
        if (!bailoutCmp32(Assembler::NotEqual, type, Imm32(MIRTypeToTag(mir->type())),
                          unbox->snapshot()))
            return false;
    }
    return true;
}

bool
CodeGeneratorMIPS::visitDouble(LDouble *ins)
{
    const LDefinition *out = ins->getDef(0);

    masm.loadConstantDouble(ins->getDouble(), ToFloatRegister(out));
    return true;
}

bool
CodeGeneratorMIPS::visitFloat32(LFloat32 *ins)
{
    const LDefinition *out = ins->getDef(0);
    masm.loadConstantFloat32(ins->getFloat(), ToFloatRegister(out));
    return true;
}

Register
CodeGeneratorMIPS::splitTagForTest(const ValueOperand &value)
{
    return value.typeReg();
}

bool
CodeGeneratorMIPS::visitTestDAndBranch(LTestDAndBranch *test)
{
    FloatRegister input = ToFloatRegister(test->input());

    MBasicBlock *ifTrue = test->ifTrue();
    MBasicBlock *ifFalse = test->ifFalse();

    masm.loadConstantDouble(0.0, ScratchFloatReg);
    // If 0, or NaN, the result is false.

    if (isNextBlock(ifFalse->lir())) {
        branchToBlock(Assembler::DoubleFloat, input, ScratchFloatReg, ifTrue,
                      Assembler::DoubleNotEqual);
    } else {
        branchToBlock(Assembler::DoubleFloat, input, ScratchFloatReg, ifFalse,
                      Assembler::DoubleEqualOrUnordered);
        jumpToBlock(ifTrue);
    }

    return true;
}

bool
CodeGeneratorMIPS::visitTestFAndBranch(LTestFAndBranch *test)
{
    FloatRegister input = ToFloatRegister(test->input());

    MBasicBlock *ifTrue = test->ifTrue();
    MBasicBlock *ifFalse = test->ifFalse();

    masm.loadConstantFloat32(0.0, ScratchFloatReg);
    // If 0, or NaN, the result is false.

    if (isNextBlock(ifFalse->lir())) {
        branchToBlock(Assembler::SingleFloat, input, ScratchFloatReg, ifTrue,
                      Assembler::DoubleNotEqual);
    } else {
        branchToBlock(Assembler::SingleFloat, input, ScratchFloatReg, ifFalse,
                      Assembler::DoubleEqualOrUnordered);
        jumpToBlock(ifTrue);
    }

    return true;
}

bool
CodeGeneratorMIPS::visitCompareD(LCompareD *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());
    Register dest = ToRegister(comp->output());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());
    masm.ma_cmp_set_double(dest, lhs, rhs, cond);
    return true;
}

bool
CodeGeneratorMIPS::visitCompareF(LCompareF *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());
    Register dest = ToRegister(comp->output());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());
    masm.ma_cmp_set_float32(dest, lhs, rhs, cond);
    return true;
}


bool
CodeGeneratorMIPS::visitCompareDAndBranch(LCompareDAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());
    MBasicBlock *ifTrue = comp->ifTrue();
    MBasicBlock *ifFalse = comp->ifFalse();

    if (isNextBlock(ifFalse->lir())) {
        branchToBlock(Assembler::DoubleFloat, lhs, rhs, ifTrue, cond);
    } else {
        branchToBlock(Assembler::DoubleFloat, lhs, rhs, ifFalse,
                      Assembler::InvertCondition(cond));
        jumpToBlock(ifTrue);
    }

    return true;
}

bool
CodeGeneratorMIPS::visitCompareFAndBranch(LCompareFAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());
    MBasicBlock *ifTrue = comp->ifTrue();
    MBasicBlock *ifFalse = comp->ifFalse();

    if (isNextBlock(ifFalse->lir())) {
        branchToBlock(Assembler::SingleFloat, lhs, rhs, ifTrue, cond);
    } else {
        branchToBlock(Assembler::SingleFloat, lhs, rhs, ifFalse,
                      Assembler::InvertCondition(cond));
        jumpToBlock(ifTrue);
    }

    return true;
}

bool
CodeGeneratorMIPS::visitCompareB(LCompareB *lir)
{
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation *rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());

    Label notBoolean, done;
    masm.branchTestBoolean(Assembler::NotEqual, lhs, &notBoolean);
    {
        if (rhs->isConstant())
            masm.cmp32Set(cond, lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()), output);
        else
            masm.cmp32Set(cond, lhs.payloadReg(), ToRegister(rhs), output);
        masm.jump(&done);
    }

    masm.bind(&notBoolean);
    {
        masm.move32(Imm32(mir->jsop() == JSOP_STRICTNE), output);
    }

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitCompareBAndBranch(LCompareBAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation *rhs = lir->rhs();

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock *mirNotBoolean = (mir->jsop() == JSOP_STRICTEQ) ? lir->ifFalse() : lir->ifTrue();
    branchToBlock(lhs.typeReg(), ImmType(JSVAL_TYPE_BOOLEAN), mirNotBoolean, Assembler::NotEqual);

    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    if (rhs->isConstant())
        emitBranch(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()), cond, lir->ifTrue(),
                   lir->ifFalse());
    else
        emitBranch(lhs.payloadReg(), ToRegister(rhs), cond, lir->ifTrue(), lir->ifFalse());

    return true;
}

bool
CodeGeneratorMIPS::visitCompareV(LCompareV *lir)
{
    MCompare *mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(IsEqualityOp(mir->jsop()));

    Label notEqual, done;
    masm.ma_b(lhs.typeReg(), rhs.typeReg(), &notEqual, Assembler::NotEqual, ShortJump);
    {
        masm.cmp32Set(cond, lhs.payloadReg(), rhs.payloadReg(), output);
        masm.ma_b(&done, ShortJump);
    }
    masm.bind(&notEqual);
    {
        masm.move32(Imm32(cond == Assembler::NotEqual), output);
    }

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitCompareVAndBranch(LCompareVAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock *notEqual = (cond == Assembler::Equal) ? lir->ifFalse() : lir->ifTrue();

    branchToBlock(lhs.typeReg(), rhs.typeReg(), notEqual, Assembler::NotEqual);
    emitBranch(lhs.payloadReg(), rhs.payloadReg(), cond, lir->ifTrue(), lir->ifFalse());

    return true;
}

bool
CodeGeneratorMIPS::visitBitAndAndBranch(LBitAndAndBranch *lir)
{
    if (lir->right()->isConstant())
        masm.ma_and(ScratchRegister, ToRegister(lir->left()), Imm32(ToInt32(lir->right())));
    else
        masm.ma_and(ScratchRegister, ToRegister(lir->left()), ToRegister(lir->right()));
    emitBranch(ScratchRegister, ScratchRegister, Assembler::NonZero, lir->ifTrue(),
               lir->ifFalse());
    return true;
}

bool
CodeGeneratorMIPS::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble *lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGeneratorMIPS::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32 *lir)
{
    masm.convertUInt32ToFloat32(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGeneratorMIPS::visitNotI(LNotI *ins)
{
    masm.cmp32Set(Assembler::Equal, ToRegister(ins->input()), Imm32(0),
                  ToRegister(ins->output()));
    return true;
}

bool
CodeGeneratorMIPS::visitNotD(LNotD *ins)
{
    // Since this operation is not, we want to set a bit if
    // the double is falsey, which means 0.0, -0.0 or NaN.
    FloatRegister in = ToFloatRegister(ins->input());
    Register dest = ToRegister(ins->output());

    Label falsey, done;
    masm.loadConstantDouble(0.0, ScratchFloatReg);
    masm.ma_bc1d(in, ScratchFloatReg, &falsey, Assembler::DoubleEqualOrUnordered, ShortJump);

    masm.move32(Imm32(0), dest);
    masm.ma_b(&done, ShortJump);

    masm.bind(&falsey);
    masm.move32(Imm32(1), dest);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitNotF(LNotF *ins)
{
    // Since this operation is not, we want to set a bit if
    // the float32 is falsey, which means 0.0, -0.0 or NaN.
    FloatRegister in = ToFloatRegister(ins->input());
    Register dest = ToRegister(ins->output());

    Label falsey, done;
    masm.loadConstantFloat32(0.0, ScratchFloatReg);
    masm.ma_bc1s(in, ScratchFloatReg, &falsey, Assembler::DoubleEqualOrUnordered, ShortJump);

    masm.move32(Imm32(0), dest);
    masm.ma_b(&done, ShortJump);

    masm.bind(&falsey);
    masm.move32(Imm32(1), dest);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitGuardShape(LGuardShape *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadPtr(Address(obj, JSObject::offsetOfShape()), tmp);
    return bailoutCmpPtr(Assembler::NotEqual, tmp, ImmGCPtr(guard->mir()->shape()),
                         guard->snapshot());
}

bool
CodeGeneratorMIPS::visitGuardObjectType(LGuardObjectType *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadPtr(Address(obj, JSObject::offsetOfType()), tmp);
    Assembler::Condition cond = guard->mir()->bailOnEquality()
                                ? Assembler::Equal
                                : Assembler::NotEqual;
    return bailoutCmpPtr(cond, tmp, ImmGCPtr(guard->mir()->typeObject()), guard->snapshot());
}

bool
CodeGeneratorMIPS::visitGuardClass(LGuardClass *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadObjClass(obj, tmp);
    if (!bailoutCmpPtr(Assembler::NotEqual, tmp, Imm32((uint32_t)guard->mir()->getClass()),
                       guard->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorMIPS::generateInvalidateEpilogue()
{
    // Ensure that there is enough space in the buffer for the OsiPoint
    // patching to occur. Otherwise, we could overwrite the invalidation
    // epilogue.
    for (size_t i = 0; i < sizeof(void *); i += Assembler::nopSize())
        masm.nop();

    masm.bind(&invalidate_);

    // Push the return address of the point that we bailed out at to the stack
    masm.Push(ra);

    // Push the Ion script onto the stack (when we determine what that
    // pointer is).
    invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));
    JitCode *thunk = gen->jitRuntime()->getInvalidationThunk();

    masm.branch(thunk);

    // We should never reach this point in JIT code -- the invalidation thunk
    // should pop the invalidated JS frame and return directly to its caller.
    masm.assumeUnreachable("Should have returned directly to its caller instead of here.");
    return true;
}

void
DispatchIonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // Can always use the scratch register on MIPS.
    addState->dispatchScratch = ScratchRegister;
}

bool
CodeGeneratorMIPS::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}

bool
CodeGeneratorMIPS::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}

bool
CodeGeneratorMIPS::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    const MAsmJSLoadHeap *mir = ins->mir();
    const LAllocation *ptr = ins->ptr();
    const LDefinition *out = ins->output();

    bool isSigned;
    int size;
    bool isFloat = false;
    switch (mir->viewType()) {
      case ArrayBufferView::TYPE_INT8:    isSigned = true;  size =  8; break;
      case ArrayBufferView::TYPE_UINT8:   isSigned = false; size =  8; break;
      case ArrayBufferView::TYPE_INT16:   isSigned = true;  size = 16; break;
      case ArrayBufferView::TYPE_UINT16:  isSigned = false; size = 16; break;
      case ArrayBufferView::TYPE_INT32:   isSigned = true;  size = 32; break;
      case ArrayBufferView::TYPE_UINT32:  isSigned = false; size = 32; break;
      case ArrayBufferView::TYPE_FLOAT64: isFloat  = true;  size = 64; break;
      case ArrayBufferView::TYPE_FLOAT32: isFloat  = true;  size = 32; break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }

    if (ptr->isConstant()) {
        MOZ_ASSERT(mir->skipBoundsCheck());
        int32_t ptrImm = ptr->toConstant()->toInt32();
        MOZ_ASSERT(ptrImm >= 0);
        if (isFloat) {
            if (size == 32) {
                masm.loadFloat32(Address(HeapReg, ptrImm), ToFloatRegister(out));
            } else {
                masm.loadDouble(Address(HeapReg, ptrImm), ToFloatRegister(out));
            }
        }  else {
            masm.ma_load(ToRegister(out), Address(HeapReg, ptrImm),
                         static_cast<LoadStoreSize>(size), isSigned ? SignExtend : ZeroExtend);
        }
        return true;
    }

    Register ptrReg = ToRegister(ptr);

    if (mir->skipBoundsCheck()) {
        if (isFloat) {
            if (size == 32) {
                masm.loadFloat32(BaseIndex(HeapReg, ptrReg, TimesOne), ToFloatRegister(out));
            } else {
                masm.loadDouble(BaseIndex(HeapReg, ptrReg, TimesOne), ToFloatRegister(out));
            }
        } else {
            masm.ma_load(ToRegister(out), BaseIndex(HeapReg, ptrReg, TimesOne),
                         static_cast<LoadStoreSize>(size), isSigned ? SignExtend : ZeroExtend);
        }
        return true;
    }

    BufferOffset bo = masm.ma_BoundsCheck(ScratchRegister);

    Label outOfRange;
    Label done;
    masm.ma_b(ptrReg, ScratchRegister, &outOfRange, Assembler::AboveOrEqual, ShortJump);
    // Offset is ok, let's load value.
    if (isFloat) {
        if (size == 32)
            masm.loadFloat32(BaseIndex(HeapReg, ptrReg, TimesOne), ToFloatRegister(out));
        else
            masm.loadDouble(BaseIndex(HeapReg, ptrReg, TimesOne), ToFloatRegister(out));
    } else {
        masm.ma_load(ToRegister(out), BaseIndex(HeapReg, ptrReg, TimesOne),
                     static_cast<LoadStoreSize>(size), isSigned ? SignExtend : ZeroExtend);
    }
    masm.ma_b(&done, ShortJump);
    masm.bind(&outOfRange);
    // Offset is out of range. Load default values.
    if (isFloat) {
        if (size == 32)
            masm.convertDoubleToFloat32(NANReg, ToFloatRegister(out));
        else
            masm.moveDouble(NANReg, ToFloatRegister(out));
    } else {
        masm.move32(Imm32(0), ToRegister(out));
    }
    masm.bind(&done);

    return masm.append(AsmJSHeapAccess(bo.getOffset()));
}

bool
CodeGeneratorMIPS::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    const MAsmJSStoreHeap *mir = ins->mir();
    const LAllocation *value = ins->value();
    const LAllocation *ptr = ins->ptr();

    bool isSigned;
    int size;
    bool isFloat = false;
    switch (mir->viewType()) {
      case ArrayBufferView::TYPE_INT8:    isSigned = true;  size = 8;  break;
      case ArrayBufferView::TYPE_UINT8:   isSigned = false; size = 8;  break;
      case ArrayBufferView::TYPE_INT16:   isSigned = true;  size = 16; break;
      case ArrayBufferView::TYPE_UINT16:  isSigned = false; size = 16; break;
      case ArrayBufferView::TYPE_INT32:   isSigned = true;  size = 32; break;
      case ArrayBufferView::TYPE_UINT32:  isSigned = false; size = 32; break;
      case ArrayBufferView::TYPE_FLOAT64: isFloat  = true;  size = 64; break;
      case ArrayBufferView::TYPE_FLOAT32: isFloat  = true;  size = 32; break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }

    if (ptr->isConstant()) {
        MOZ_ASSERT(mir->skipBoundsCheck());
        int32_t ptrImm = ptr->toConstant()->toInt32();
        MOZ_ASSERT(ptrImm >= 0);

        if (isFloat) {
            if (size == 32) {
                masm.storeFloat32(ToFloatRegister(value), Address(HeapReg, ptrImm));
            } else {
                masm.storeDouble(ToFloatRegister(value), Address(HeapReg, ptrImm));
            }
        }  else {
            masm.ma_store(ToRegister(value), Address(HeapReg, ptrImm),
                          static_cast<LoadStoreSize>(size), isSigned ? SignExtend : ZeroExtend);
        }
        return true;
    }

    Register ptrReg = ToRegister(ptr);
    Address dstAddr(ptrReg, 0);

    if (mir->skipBoundsCheck()) {
        if (isFloat) {
            if (size == 32) {
                masm.storeFloat32(ToFloatRegister(value), BaseIndex(HeapReg, ptrReg, TimesOne));
            } else
                masm.storeDouble(ToFloatRegister(value), BaseIndex(HeapReg, ptrReg, TimesOne));
        } else {
            masm.ma_store(ToRegister(value), BaseIndex(HeapReg, ptrReg, TimesOne),
                          static_cast<LoadStoreSize>(size), isSigned ? SignExtend : ZeroExtend);
        }
        return true;
    }

    BufferOffset bo = masm.ma_BoundsCheck(ScratchRegister);

    Label rejoin;
    masm.ma_b(ptrReg, ScratchRegister, &rejoin, Assembler::AboveOrEqual, ShortJump);

    // Offset is ok, let's store value.
    if (isFloat) {
        if (size == 32) {
            masm.storeFloat32(ToFloatRegister(value), BaseIndex(HeapReg, ptrReg, TimesOne));
        } else
            masm.storeDouble(ToFloatRegister(value), BaseIndex(HeapReg, ptrReg, TimesOne));
    } else {
        masm.ma_store(ToRegister(value), BaseIndex(HeapReg, ptrReg, TimesOne),
                      static_cast<LoadStoreSize>(size), isSigned ? SignExtend : ZeroExtend);
    }
    masm.bind(&rejoin);

    return masm.append(AsmJSHeapAccess(bo.getOffset()));
}

bool
CodeGeneratorMIPS::visitAsmJSPassStackArg(LAsmJSPassStackArg *ins)
{
    const MAsmJSPassStackArg *mir = ins->mir();
    if (ins->arg()->isConstant()) {
        masm.storePtr(ImmWord(ToInt32(ins->arg())), Address(StackPointer, mir->spOffset()));
    } else {
        if (ins->arg()->isGeneralReg()) {
            masm.storePtr(ToRegister(ins->arg()), Address(StackPointer, mir->spOffset()));
        } else {
            masm.storeDouble(ToFloatRegister(ins->arg()), Address(StackPointer, mir->spOffset()));
        }
    }

    return true;
}

bool
CodeGeneratorMIPS::visitUDiv(LUDiv *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    Label done;
    if (ins->mir()->canBeDivideByZero()) {
        if (ins->mir()->isTruncated()) {
            Label notzero;
            masm.ma_b(rhs, rhs, &notzero, Assembler::NonZero, ShortJump);
            masm.move32(Imm32(0), output);
            masm.ma_b(&done, ShortJump);
            masm.bind(&notzero);
        } else {
            MOZ_ASSERT(ins->mir()->fallible());
            if (!bailoutCmp32(Assembler::Equal, rhs, Imm32(0), ins->snapshot()))
                return false;
        }
    }

    masm.as_divu(lhs, rhs);
    masm.as_mflo(output);

    if (!ins->mir()->isTruncated()) {
        if (!bailoutCmp32(Assembler::LessThan, output, Imm32(0), ins->snapshot()))
            return false;
    }

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitUMod(LUMod *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    Label done;

    if (ins->mir()->canBeDivideByZero()) {
        if (ins->mir()->isTruncated()) {
            // Infinity|0 == 0
            Label notzero;
            masm.ma_b(rhs, rhs, &notzero, Assembler::NonZero, ShortJump);
            masm.move32(Imm32(0), output);
            masm.ma_b(&done, ShortJump);
            masm.bind(&notzero);
        } else {
            MOZ_ASSERT(ins->mir()->fallible());
            if (!bailoutCmp32(Assembler::Equal, rhs, Imm32(0), ins->snapshot()))
                return false;
        }
    }

    masm.as_divu(lhs, rhs);
    masm.as_mfhi(output);

    if (!ins->mir()->isTruncated()) {
        if (!bailoutCmp32(Assembler::LessThan, output, Imm32(0), ins->snapshot()))
            return false;
    }

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorMIPS::visitEffectiveAddress(LEffectiveAddress *ins)
{
    const MEffectiveAddress *mir = ins->mir();
    Register base = ToRegister(ins->base());
    Register index = ToRegister(ins->index());
    Register output = ToRegister(ins->output());

    BaseIndex address(base, index, mir->scale(), mir->displacement());
    masm.computeEffectiveAddress(address, output);
    return true;
}

bool
CodeGeneratorMIPS::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins)
{
    const MAsmJSLoadGlobalVar *mir = ins->mir();
    unsigned addr = mir->globalDataOffset();
    if (mir->type() == MIRType_Int32)
        masm.load32(Address(GlobalReg, addr), ToRegister(ins->output()));
    else if (mir->type() == MIRType_Float32)
        masm.loadFloat32(Address(GlobalReg, addr), ToFloatRegister(ins->output()));
    else
        masm.loadDouble(Address(GlobalReg, addr), ToFloatRegister(ins->output()));
    return true;
}

bool
CodeGeneratorMIPS::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    const MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    MOZ_ASSERT(IsNumberType(type));
    unsigned addr = mir->globalDataOffset();
    if (mir->value()->type() == MIRType_Int32)
        masm.store32(ToRegister(ins->value()), Address(GlobalReg, addr));
    else if (mir->value()->type() == MIRType_Float32)
        masm.storeFloat32(ToFloatRegister(ins->value()), Address(GlobalReg, addr));
    else
        masm.storeDouble(ToFloatRegister(ins->value()), Address(GlobalReg, addr));
    return true;
}

bool
CodeGeneratorMIPS::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins)
{
    const MAsmJSLoadFuncPtr *mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register tmp = ToRegister(ins->temp());
    Register out = ToRegister(ins->output());
    unsigned addr = mir->globalDataOffset();

    BaseIndex source(GlobalReg, index, TimesFour, addr);
    masm.load32(source, out);
    return true;
}

bool
CodeGeneratorMIPS::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    const MAsmJSLoadFFIFunc *mir = ins->mir();
    masm.loadPtr(Address(GlobalReg, mir->globalDataOffset()), ToRegister(ins->output()));
    return true;
}

bool
CodeGeneratorMIPS::visitNegI(LNegI *ins)
{
    Register input = ToRegister(ins->input());
    Register output = ToRegister(ins->output());

    masm.ma_negu(output, input);
    return true;
}

bool
CodeGeneratorMIPS::visitNegD(LNegD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());

    masm.as_negd(output, input);
    return true;
}

bool
CodeGeneratorMIPS::visitNegF(LNegF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());

    masm.as_negs(output, input);
    return true;
}

bool
CodeGeneratorMIPS::visitForkJoinGetSlice(LForkJoinGetSlice *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}

JitCode *
JitRuntime::generateForkJoinGetSliceStub(JSContext *cx)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}
