/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/CodeGenerator-arm64.h"

#include "mozilla/MathAlgorithms.h"

#include "jsnum.h"

#include "jit/CodeGenerator.h"
#include "jit/JitFrames.h"
#include "jit/JitRealm.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/TraceLogging.h"

#include "jit/shared/CodeGenerator-shared-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

using JS::GenericNaN;
using mozilla::FloorLog2;
using mozilla::NegativeInfinity;

// shared
CodeGeneratorARM64::CodeGeneratorARM64(MIRGenerator* gen, LIRGraph* graph,
                                       MacroAssembler* masm)
    : CodeGeneratorShared(gen, graph, masm) {}

bool CodeGeneratorARM64::generateOutOfLineCode() {
  if (!CodeGeneratorShared::generateOutOfLineCode()) {
    return false;
  }

  if (deoptLabel_.used()) {
    // All non-table-based bailouts will go here.
    masm.bind(&deoptLabel_);

    // Store the frame size, so the handler can recover the IonScript.
    masm.push(Imm32(frameSize()));

    TrampolinePtr handler = gen->jitRuntime()->getGenericBailoutHandler();
    masm.jump(handler);
  }

  return !masm.oom();
}

void CodeGeneratorARM64::emitBranch(Assembler::Condition cond,
                                    MBasicBlock* mirTrue,
                                    MBasicBlock* mirFalse) {
  if (isNextBlock(mirFalse->lir())) {
    jumpToBlock(mirTrue, cond);
  } else {
    jumpToBlock(mirFalse, Assembler::InvertCondition(cond));
    jumpToBlock(mirTrue);
  }
}

void OutOfLineBailout::accept(CodeGeneratorARM64* codegen) {
  codegen->visitOutOfLineBailout(this);
}

void CodeGenerator::visitTestIAndBranch(LTestIAndBranch* test) {
  Register input = ToRegister(test->input());
  MBasicBlock* mirTrue = test->ifTrue();
  MBasicBlock* mirFalse = test->ifFalse();

  masm.test32(input, input);

  // Jump to the True block if NonZero.
  // Jump to the False block if Zero.
  if (isNextBlock(mirFalse->lir())) {
    jumpToBlock(mirTrue, Assembler::NonZero);
  } else {
    jumpToBlock(mirFalse, Assembler::Zero);
    if (!isNextBlock(mirTrue->lir())) {
      jumpToBlock(mirTrue);
    }
  }
}

void CodeGenerator::visitCompare(LCompare* comp) {
  const MCompare* mir = comp->mir();
  const MCompare::CompareType type = mir->compareType();
  const Assembler::Condition cond = JSOpToCondition(type, comp->jsop());
  const Register leftreg = ToRegister(comp->getOperand(0));
  const LAllocation* right = comp->getOperand(1);
  const Register defreg = ToRegister(comp->getDef(0));

  if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol) {
    masm.cmpPtrSet(cond, leftreg, ToRegister(right), defreg);
    return;
  }

  if (right->isConstant()) {
    masm.cmp32Set(cond, leftreg, Imm32(ToInt32(right)), defreg);
  } else {
    masm.cmp32Set(cond, leftreg, ToRegister(right), defreg);
  }
}

void CodeGenerator::visitCompareAndBranch(LCompareAndBranch* comp) {
  const MCompare* mir = comp->cmpMir();
  const MCompare::CompareType type = mir->compareType();
  const LAllocation* left = comp->left();
  const LAllocation* right = comp->right();

  if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol) {
    masm.cmpPtr(ToRegister(left), ToRegister(right));
  } else if (right->isConstant()) {
    masm.cmp32(ToRegister(left), Imm32(ToInt32(right)));
  } else {
    masm.cmp32(ToRegister(left), ToRegister(right));
  }

  Assembler::Condition cond = JSOpToCondition(type, comp->jsop());
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGeneratorARM64::bailoutIf(Assembler::Condition condition,
                                   LSnapshot* snapshot) {
  encode(snapshot);

  // Though the assembler doesn't track all frame pushes, at least make sure
  // the known value makes sense.
  MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                frameClass_.frameSize() == masm.framePushed());

  // ARM64 doesn't use a bailout table.
  InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
  OutOfLineBailout* ool = new (alloc()) OutOfLineBailout(snapshot);
  addOutOfLineCode(ool,
                   new (alloc()) BytecodeSite(tree, tree->script()->code()));

  masm.B(ool->entry(), condition);
}

void CodeGeneratorARM64::bailoutFrom(Label* label, LSnapshot* snapshot) {
  MOZ_ASSERT_IF(!masm.oom(), label->used());
  MOZ_ASSERT_IF(!masm.oom(), !label->bound());

  encode(snapshot);

  // Though the assembler doesn't track all frame pushes, at least make sure
  // the known value makes sense.
  MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                frameClass_.frameSize() == masm.framePushed());

  // ARM64 doesn't use a bailout table.
  InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
  OutOfLineBailout* ool = new (alloc()) OutOfLineBailout(snapshot);
  addOutOfLineCode(ool,
                   new (alloc()) BytecodeSite(tree, tree->script()->code()));

  masm.retarget(label, ool->entry());
}

void CodeGeneratorARM64::bailout(LSnapshot* snapshot) {
  Label label;
  masm.b(&label);
  bailoutFrom(&label, snapshot);
}

void CodeGeneratorARM64::visitOutOfLineBailout(OutOfLineBailout* ool) {
  masm.push(Imm32(ool->snapshot()->snapshotOffset()));
  masm.B(&deoptLabel_);
}

void CodeGenerator::visitMinMaxD(LMinMaxD* ins) {
  ARMFPRegister lhs(ToFloatRegister(ins->first()), 64);
  ARMFPRegister rhs(ToFloatRegister(ins->second()), 64);
  ARMFPRegister output(ToFloatRegister(ins->output()), 64);
  if (ins->mir()->isMax()) {
    masm.Fmax(output, lhs, rhs);
  } else {
    masm.Fmin(output, lhs, rhs);
  }
}

void CodeGenerator::visitMinMaxF(LMinMaxF* ins) {
  ARMFPRegister lhs(ToFloatRegister(ins->first()), 32);
  ARMFPRegister rhs(ToFloatRegister(ins->second()), 32);
  ARMFPRegister output(ToFloatRegister(ins->output()), 32);
  if (ins->mir()->isMax()) {
    masm.Fmax(output, lhs, rhs);
  } else {
    masm.Fmin(output, lhs, rhs);
  }
}

void CodeGenerator::visitAbsD(LAbsD* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  masm.Fabs(input, input);
}

void CodeGenerator::visitAbsF(LAbsF* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  masm.Fabs(input, input);
}

void CodeGenerator::visitSqrtD(LSqrtD* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  ARMFPRegister output(ToFloatRegister(ins->output()), 64);
  masm.Fsqrt(output, input);
}

void CodeGenerator::visitSqrtF(LSqrtF* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  ARMFPRegister output(ToFloatRegister(ins->output()), 32);
  masm.Fsqrt(output, input);
}

// FIXME: Uh, is this a static function? It looks like it is...
template <typename T>
ARMRegister toWRegister(const T* a) {
  return ARMRegister(ToRegister(a), 32);
}

// FIXME: Uh, is this a static function? It looks like it is...
template <typename T>
ARMRegister toXRegister(const T* a) {
  return ARMRegister(ToRegister(a), 64);
}

Operand toWOperand(const LAllocation* a) {
  if (a->isConstant()) {
    return Operand(ToInt32(a));
  }
  return Operand(toWRegister(a));
}

vixl::CPURegister ToCPURegister(const LAllocation* a, Scalar::Type type) {
  if (a->isFloatReg() && type == Scalar::Float64) {
    return ARMFPRegister(ToFloatRegister(a), 64);
  }
  if (a->isFloatReg() && type == Scalar::Float32) {
    return ARMFPRegister(ToFloatRegister(a), 32);
  }
  if (a->isGeneralReg()) {
    return ARMRegister(ToRegister(a), 32);
  }
  MOZ_CRASH("Unknown LAllocation");
}

vixl::CPURegister ToCPURegister(const LDefinition* d, Scalar::Type type) {
  return ToCPURegister(d->output(), type);
}

void CodeGenerator::visitAddI(LAddI* ins) {
  const LAllocation* lhs = ins->getOperand(0);
  const LAllocation* rhs = ins->getOperand(1);
  const LDefinition* dest = ins->getDef(0);

  // Platforms with three-operand arithmetic ops don't need recovery.
  MOZ_ASSERT(!ins->recoversInput());

  if (ins->snapshot()) {
    masm.Adds(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
    bailoutIf(Assembler::Overflow, ins->snapshot());
  } else {
    masm.Add(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
  }
}

void CodeGenerator::visitSubI(LSubI* ins) {
  const LAllocation* lhs = ins->getOperand(0);
  const LAllocation* rhs = ins->getOperand(1);
  const LDefinition* dest = ins->getDef(0);

  // Platforms with three-operand arithmetic ops don't need recovery.
  MOZ_ASSERT(!ins->recoversInput());

  if (ins->snapshot()) {
    masm.Subs(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
    bailoutIf(Assembler::Overflow, ins->snapshot());
  } else {
    masm.Sub(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
  }
}

void CodeGenerator::visitMulI(LMulI* ins) {
  const LAllocation* lhs = ins->getOperand(0);
  const LAllocation* rhs = ins->getOperand(1);
  const LDefinition* dest = ins->getDef(0);
  MMul* mul = ins->mir();
  MOZ_ASSERT_IF(mul->mode() == MMul::Integer,
                !mul->canBeNegativeZero() && !mul->canOverflow());

  Register lhsreg = ToRegister(lhs);
  const ARMRegister lhsreg32 = ARMRegister(lhsreg, 32);
  Register destreg = ToRegister(dest);
  const ARMRegister destreg32 = ARMRegister(destreg, 32);

  if (rhs->isConstant()) {
    // Bailout on -0.0.
    int32_t constant = ToInt32(rhs);
    if (mul->canBeNegativeZero() && constant <= 0) {
      Assembler::Condition bailoutCond =
          (constant == 0) ? Assembler::LessThan : Assembler::Equal;
      masm.Cmp(toWRegister(lhs), Operand(0));
      bailoutIf(bailoutCond, ins->snapshot());
    }

    switch (constant) {
      case -1:
        masm.Negs(destreg32, Operand(lhsreg32));
        break;  // Go to overflow check.
      case 0:
        masm.Mov(destreg32, wzr);
        return;  // Avoid overflow check.
      case 1:
        // nop
        return;  // Avoid overflow check.
      case 2:
        masm.Adds(destreg32, lhsreg32, Operand(lhsreg32));
        break;  // Go to overflow check.
      default:
        // Use shift if cannot overflow and constant is a power of 2
        if (!mul->canOverflow() && constant > 0) {
          int32_t shift = FloorLog2(constant);
          if ((1 << shift) == constant) {
            masm.Lsl(destreg32, lhsreg32, shift);
            return;
          }
        }

        // Otherwise, just multiply.
        Label bailout;
        Label* onZero = mul->canBeNegativeZero() ? &bailout : nullptr;
        Label* onOverflow = mul->canOverflow() ? &bailout : nullptr;

        vixl::UseScratchRegisterScope temps(&masm.asVIXL());
        const Register scratch = temps.AcquireW().asUnsized();

        masm.move32(Imm32(constant), scratch);
        masm.mul32(lhsreg, scratch, destreg, onOverflow, onZero);
        if (onZero || onOverflow) {
          bailoutFrom(&bailout, ins->snapshot());
        }
        return;  // escape overflow check;
    }

    // Overflow check.
    if (mul->canOverflow()) {
      bailoutIf(Assembler::Overflow, ins->snapshot());
    }
  } else {
    Register rhsreg = ToRegister(rhs);

    Label bailout;
    // TODO: x64 (but not other platforms) have an OOL path for onZero.
    Label* onZero = mul->canBeNegativeZero() ? &bailout : nullptr;
    Label* onOverflow = mul->canOverflow() ? &bailout : nullptr;

    masm.mul32(lhsreg, rhsreg, destreg, onOverflow, onZero);
    if (onZero || onOverflow) {
      bailoutFrom(&bailout, ins->snapshot());
    }
  }
}

void CodeGenerator::visitDivI(LDivI* ins) {
  const Register lhs = ToRegister(ins->lhs());
  const Register rhs = ToRegister(ins->rhs());
  const Register output = ToRegister(ins->output());

  const ARMRegister lhs32 = toWRegister(ins->lhs());
  const ARMRegister rhs32 = toWRegister(ins->rhs());
  const ARMRegister temp32 = toWRegister(ins->getTemp(0));
  const ARMRegister output32 = toWRegister(ins->output());

  MDiv* mir = ins->mir();

  Label done;

  // Handle division by zero.
  if (mir->canBeDivideByZero()) {
    masm.test32(rhs, rhs);
    // TODO: x64 has an additional mir->canTruncateInfinities() handler
    // TODO: to avoid taking a bailout.
    if (mir->trapOnError()) {
      Label nonZero;
      masm.j(Assembler::NonZero, &nonZero);
      masm.wasmTrap(wasm::Trap::IntegerDivideByZero, mir->bytecodeOffset());
      masm.bind(&nonZero);
    } else {
      MOZ_ASSERT(mir->fallible());
      bailoutIf(Assembler::Zero, ins->snapshot());
    }
  }

  // Handle an integer overflow from (INT32_MIN / -1).
  // The integer division gives INT32_MIN, but should be -(double)INT32_MIN.
  if (mir->canBeNegativeOverflow()) {
    Label notOverflow;

    // Branch to handle the non-overflow cases.
    masm.branch32(Assembler::NotEqual, lhs, Imm32(INT32_MIN), &notOverflow);
    masm.branch32(Assembler::NotEqual, rhs, Imm32(-1), &notOverflow);

    // Handle overflow.
    if (mir->trapOnError()) {
      masm.wasmTrap(wasm::Trap::IntegerOverflow, mir->bytecodeOffset());
    } else if (mir->canTruncateOverflow()) {
      // (-INT32_MIN)|0 == INT32_MIN, which is already in lhs.
      masm.move32(lhs, output);
      masm.jump(&done);
    } else {
      MOZ_ASSERT(mir->fallible());
      bailout(ins->snapshot());
    }
    masm.bind(&notOverflow);
  }

  // Handle negative zero: lhs == 0 && rhs < 0.
  if (!mir->canTruncateNegativeZero() && mir->canBeNegativeZero()) {
    Label nonZero;
    masm.branch32(Assembler::NotEqual, lhs, Imm32(0), &nonZero);
    masm.cmp32(rhs, Imm32(0));
    bailoutIf(Assembler::LessThan, ins->snapshot());
    masm.bind(&nonZero);
  }

  // Perform integer division.
  if (mir->canTruncateRemainder()) {
    masm.Sdiv(output32, lhs32, rhs32);
  } else {
    vixl::UseScratchRegisterScope temps(&masm.asVIXL());
    ARMRegister scratch32 = temps.AcquireW();

    // ARM does not automatically calculate the remainder.
    // The ISR suggests multiplication to determine whether a remainder exists.
    masm.Sdiv(scratch32, lhs32, rhs32);
    masm.Mul(temp32, scratch32, rhs32);
    masm.Cmp(lhs32, temp32);
    bailoutIf(Assembler::NotEqual, ins->snapshot());
    masm.Mov(output32, scratch32);
  }

  masm.bind(&done);
}

void CodeGenerator::visitDivPowTwoI(LDivPowTwoI* ins) {
  MOZ_CRASH("CodeGenerator::visitDivPowTwoI");
}

void CodeGeneratorARM64::modICommon(MMod* mir, Register lhs, Register rhs,
                                    Register output, LSnapshot* snapshot,
                                    Label& done) {
  MOZ_CRASH("CodeGeneratorARM64::modICommon");
}

void CodeGenerator::visitModI(LModI* ins) { MOZ_CRASH("visitModI"); }

void CodeGenerator::visitModPowTwoI(LModPowTwoI* ins) {
  Register lhs = ToRegister(ins->getOperand(0));
  ARMRegister lhsw = toWRegister(ins->getOperand(0));
  ARMRegister outw = toWRegister(ins->output());

  int32_t shift = ins->shift();
  bool canBeNegative =
      !ins->mir()->isUnsigned() && ins->mir()->canBeNegativeDividend();

  Label negative;
  if (canBeNegative) {
    // Switch based on sign of the lhs.
    // Positive numbers are just a bitmask.
    masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);
  }

  masm.And(outw, lhsw, Operand((uint32_t(1) << shift) - 1));

  if (canBeNegative) {
    Label done;
    masm.jump(&done);

    // Negative numbers need a negate, bitmask, negate.
    masm.bind(&negative);
    masm.Neg(outw, Operand(lhsw));
    masm.And(outw, outw, Operand((uint32_t(1) << shift) - 1));

    // Since a%b has the same sign as b, and a is negative in this branch,
    // an answer of 0 means the correct result is actually -0. Bail out.
    if (!ins->mir()->isTruncated()) {
      masm.Negs(outw, Operand(outw));
      bailoutIf(Assembler::Zero, ins->snapshot());
    } else {
      masm.Neg(outw, Operand(outw));
    }

    masm.bind(&done);
  }
}

void CodeGenerator::visitModMaskI(LModMaskI* ins) {
  MOZ_CRASH("CodeGenerator::visitModMaskI");
}

void CodeGenerator::visitBitNotI(LBitNotI* ins) {
  const LAllocation* input = ins->getOperand(0);
  const LDefinition* output = ins->getDef(0);
  masm.Mvn(toWRegister(output), toWOperand(input));
}

void CodeGenerator::visitBitOpI(LBitOpI* ins) {
  const ARMRegister lhs = toWRegister(ins->getOperand(0));
  const Operand rhs = toWOperand(ins->getOperand(1));
  const ARMRegister dest = toWRegister(ins->getDef(0));

  switch (ins->bitop()) {
    case JSOP_BITOR:
      masm.Orr(dest, lhs, rhs);
      break;
    case JSOP_BITXOR:
      masm.Eor(dest, lhs, rhs);
      break;
    case JSOP_BITAND:
      masm.And(dest, lhs, rhs);
      break;
    default:
      MOZ_CRASH("unexpected binary opcode");
  }
}

void CodeGenerator::visitShiftI(LShiftI* ins) {
  const ARMRegister lhs = toWRegister(ins->lhs());
  const LAllocation* rhs = ins->rhs();
  const ARMRegister dest = toWRegister(ins->output());

  if (rhs->isConstant()) {
    int32_t shift = ToInt32(rhs) & 0x1F;
    switch (ins->bitop()) {
      case JSOP_LSH:
        masm.Lsl(dest, lhs, shift);
        break;
      case JSOP_RSH:
        masm.Asr(dest, lhs, shift);
        break;
      case JSOP_URSH:
        if (shift) {
          masm.Lsr(dest, lhs, shift);
        } else if (ins->mir()->toUrsh()->fallible()) {
          // x >>> 0 can overflow.
          masm.Ands(dest, lhs, Operand(0xFFFFFFFF));
          bailoutIf(Assembler::Signed, ins->snapshot());
        } else {
          masm.Mov(dest, lhs);
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
  } else {
    const ARMRegister rhsreg = toWRegister(rhs);
    switch (ins->bitop()) {
      case JSOP_LSH:
        masm.Lsl(dest, lhs, rhsreg);
        break;
      case JSOP_RSH:
        masm.Asr(dest, lhs, rhsreg);
        break;
      case JSOP_URSH:
        masm.Lsr(dest, lhs, rhsreg);
        if (ins->mir()->toUrsh()->fallible()) {
          /// x >>> 0 can overflow.
          Label nonzero;
          masm.Cbnz(rhsreg, &nonzero);
          masm.Cmp(dest, Operand(0));
          bailoutIf(Assembler::LessThan, ins->snapshot());
          masm.bind(&nonzero);
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
  }
}

void CodeGenerator::visitUrshD(LUrshD* ins) {
  const ARMRegister lhs = toWRegister(ins->lhs());
  const LAllocation* rhs = ins->rhs();
  const FloatRegister out = ToFloatRegister(ins->output());

  const Register temp = ToRegister(ins->temp());
  const ARMRegister temp32 = toWRegister(ins->temp());

  if (rhs->isConstant()) {
    int32_t shift = ToInt32(rhs) & 0x1F;
    if (shift) {
      masm.Lsr(temp32, lhs, shift);
      masm.convertUInt32ToDouble(temp, out);
    } else {
      masm.convertUInt32ToDouble(ToRegister(ins->lhs()), out);
    }
  } else {
    masm.And(temp32, toWRegister(rhs), Operand(0x1F));
    masm.Lsr(temp32, lhs, temp32);
    masm.convertUInt32ToDouble(temp, out);
  }
}

void CodeGenerator::visitPowHalfD(LPowHalfD* ins) {
  MOZ_CRASH("visitPowHalfD");
}

MoveOperand CodeGeneratorARM64::toMoveOperand(const LAllocation a) const {
  if (a.isGeneralReg()) {
    return MoveOperand(ToRegister(a));
  }
  if (a.isFloatReg()) {
    return MoveOperand(ToFloatRegister(a));
  }
  return MoveOperand(AsRegister(masm.getStackPointer()), ToStackOffset(a));
}

class js::jit::OutOfLineTableSwitch
    : public OutOfLineCodeBase<CodeGeneratorARM64> {
  MTableSwitch* mir_;
  Vector<CodeLabel, 8, JitAllocPolicy> codeLabels_;

  void accept(CodeGeneratorARM64* codegen) override {
    codegen->visitOutOfLineTableSwitch(this);
  }

 public:
  OutOfLineTableSwitch(TempAllocator& alloc, MTableSwitch* mir)
      : mir_(mir), codeLabels_(alloc) {}

  MTableSwitch* mir() const { return mir_; }

  bool addCodeLabel(CodeLabel label) { return codeLabels_.append(label); }
  CodeLabel codeLabel(unsigned i) { return codeLabels_[i]; }
};

void CodeGeneratorARM64::visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool) {
  MOZ_CRASH("visitOutOfLineTableSwitch");
}

void CodeGeneratorARM64::emitTableSwitchDispatch(MTableSwitch* mir,
                                                 Register index_,
                                                 Register base_) {
  MOZ_CRASH("emitTableSwitchDispatch");
}

void CodeGenerator::visitMathD(LMathD* math) {
  ARMFPRegister lhs(ToFloatRegister(math->lhs()), 64);
  ARMFPRegister rhs(ToFloatRegister(math->rhs()), 64);
  ARMFPRegister output(ToFloatRegister(math->output()), 64);

  switch (math->jsop()) {
    case JSOP_ADD:
      masm.Fadd(output, lhs, rhs);
      break;
    case JSOP_SUB:
      masm.Fsub(output, lhs, rhs);
      break;
    case JSOP_MUL:
      masm.Fmul(output, lhs, rhs);
      break;
    case JSOP_DIV:
      masm.Fdiv(output, lhs, rhs);
      break;
    default:
      MOZ_CRASH("unexpected opcode");
  }
}

void CodeGenerator::visitMathF(LMathF* math) {
  ARMFPRegister lhs(ToFloatRegister(math->lhs()), 32);
  ARMFPRegister rhs(ToFloatRegister(math->rhs()), 32);
  ARMFPRegister output(ToFloatRegister(math->output()), 32);

  switch (math->jsop()) {
    case JSOP_ADD:
      masm.Fadd(output, lhs, rhs);
      break;
    case JSOP_SUB:
      masm.Fsub(output, lhs, rhs);
      break;
    case JSOP_MUL:
      masm.Fmul(output, lhs, rhs);
      break;
    case JSOP_DIV:
      masm.Fdiv(output, lhs, rhs);
      break;
    default:
      MOZ_CRASH("unexpected opcode");
  }
}

void CodeGenerator::visitFloor(LFloor* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.floor(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitFloorF(LFloorF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.floorf(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitCeil(LCeil* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.ceil(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitCeilF(LCeilF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;
  masm.ceilf(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());
}

void CodeGenerator::visitRound(LRound* lir) { MOZ_CRASH("visitRound"); }

void CodeGenerator::visitRoundF(LRoundF* lir) { MOZ_CRASH("visitRoundF"); }

void CodeGenerator::visitTrunc(LTrunc* lir) { MOZ_CRASH("visitTrunc"); }

void CodeGenerator::visitTruncF(LTruncF* lir) { MOZ_CRASH("visitTruncF"); }

void CodeGenerator::visitClzI(LClzI* lir) {
  ARMRegister input = toWRegister(lir->input());
  ARMRegister output = toWRegister(lir->output());
  masm.Clz(output, input);
}

void CodeGenerator::visitCtzI(LCtzI* lir) {
  Register input = ToRegister(lir->input());
  Register output = ToRegister(lir->output());
  masm.ctz32(input, output, /* knownNotZero = */ false);
}

void CodeGeneratorARM64::emitRoundDouble(FloatRegister src, Register dest,
                                         Label* fail) {
  MOZ_CRASH("CodeGeneratorARM64::emitRoundDouble");
}

void CodeGenerator::visitTruncateDToInt32(LTruncateDToInt32* ins) {
  emitTruncateDouble(ToFloatRegister(ins->input()), ToRegister(ins->output()),
                     ins->mir());
}

void CodeGenerator::visitTruncateFToInt32(LTruncateFToInt32* ins) {
  emitTruncateFloat32(ToFloatRegister(ins->input()), ToRegister(ins->output()),
                      ins->mir());
}

FrameSizeClass FrameSizeClass::FromDepth(uint32_t frameDepth) {
  return FrameSizeClass::None();
}

FrameSizeClass FrameSizeClass::ClassLimit() { return FrameSizeClass(0); }

uint32_t FrameSizeClass::frameSize() const {
  MOZ_CRASH("arm64 does not use frame size classes");
}

ValueOperand CodeGeneratorARM64::ToValue(LInstruction* ins, size_t pos) {
  return ValueOperand(ToRegister(ins->getOperand(pos)));
}

ValueOperand CodeGeneratorARM64::ToTempValue(LInstruction* ins, size_t pos) {
  MOZ_CRASH("CodeGeneratorARM64::ToTempValue");
}

void CodeGenerator::visitValue(LValue* value) {
  ValueOperand result = ToOutValue(value);
  masm.moveValue(value->value(), result);
}

void CodeGenerator::visitBox(LBox* box) {
  const LAllocation* in = box->getOperand(0);
  ValueOperand result = ToOutValue(box);

  masm.moveValue(TypedOrValueRegister(box->type(), ToAnyRegister(in)), result);
}

void CodeGenerator::visitUnbox(LUnbox* unbox) {
  MUnbox* mir = unbox->mir();

  if (mir->fallible()) {
    const ValueOperand value = ToValue(unbox, LUnbox::Input);
    Assembler::Condition cond;
    switch (mir->type()) {
      case MIRType::Int32:
        cond = masm.testInt32(Assembler::NotEqual, value);
        break;
      case MIRType::Boolean:
        cond = masm.testBoolean(Assembler::NotEqual, value);
        break;
      case MIRType::Object:
        cond = masm.testObject(Assembler::NotEqual, value);
        break;
      case MIRType::String:
        cond = masm.testString(Assembler::NotEqual, value);
        break;
      case MIRType::Symbol:
        cond = masm.testSymbol(Assembler::NotEqual, value);
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }
    bailoutIf(cond, unbox->snapshot());
  } else {
#ifdef DEBUG
    JSValueTag tag = MIRTypeToTag(mir->type());
    Label ok;

    ValueOperand input = ToValue(unbox, LUnbox::Input);
    ScratchTagScope scratch(masm, input);
    masm.splitTagForTest(input, scratch);
    masm.branchTest32(Assembler::Condition::Equal, scratch, Imm32(tag), &ok);
    masm.assumeUnreachable("Infallible unbox type mismatch");
    masm.bind(&ok);
#endif
  }

  ValueOperand input = ToValue(unbox, LUnbox::Input);
  Register result = ToRegister(unbox->output());
  switch (mir->type()) {
    case MIRType::Int32:
      masm.unboxInt32(input, result);
      break;
    case MIRType::Boolean:
      masm.unboxBoolean(input, result);
      break;
    case MIRType::Object:
      masm.unboxObject(input, result);
      break;
    case MIRType::String:
      masm.unboxString(input, result);
      break;
    case MIRType::Symbol:
      masm.unboxSymbol(input, result);
      break;
    default:
      MOZ_CRASH("Given MIRType cannot be unboxed.");
  }
}

void CodeGenerator::visitDouble(LDouble* ins) {
  ARMFPRegister output(ToFloatRegister(ins->getDef(0)), 64);
  masm.Fmov(output, ins->getDouble());
}

void CodeGenerator::visitFloat32(LFloat32* ins) {
  ARMFPRegister output(ToFloatRegister(ins->getDef(0)), 32);
  masm.Fmov(output, ins->getFloat());
}

void CodeGenerator::visitTestDAndBranch(LTestDAndBranch* test) {
  const LAllocation* opd = test->input();
  MBasicBlock* ifTrue = test->ifTrue();
  MBasicBlock* ifFalse = test->ifFalse();

  masm.Fcmp(ARMFPRegister(ToFloatRegister(opd), 64), 0.0);

  // If the compare set the 0 bit, then the result is definitely false.
  jumpToBlock(ifFalse, Assembler::Zero);

  // Overflow means one of the operands was NaN, which is also false.
  jumpToBlock(ifFalse, Assembler::Overflow);
  jumpToBlock(ifTrue);
}

void CodeGenerator::visitTestFAndBranch(LTestFAndBranch* test) {
  const LAllocation* opd = test->input();
  MBasicBlock* ifTrue = test->ifTrue();
  MBasicBlock* ifFalse = test->ifFalse();

  masm.Fcmp(ARMFPRegister(ToFloatRegister(opd), 32), 0.0);

  // If the compare set the 0 bit, then the result is definitely false.
  jumpToBlock(ifFalse, Assembler::Zero);

  // Overflow means one of the operands was NaN, which is also false.
  jumpToBlock(ifFalse, Assembler::Overflow);
  jumpToBlock(ifTrue);
}

void CodeGenerator::visitCompareD(LCompareD* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  ARMRegister output = toWRegister(comp->output());
  Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

  masm.compareDouble(cond, left, right);
  masm.cset(output, Assembler::ConditionFromDoubleCondition(cond));
}

void CodeGenerator::visitCompareF(LCompareF* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  ARMRegister output = toWRegister(comp->output());
  Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

  masm.compareFloat(cond, left, right);
  masm.cset(output, Assembler::ConditionFromDoubleCondition(cond));
}

void CodeGenerator::visitCompareDAndBranch(LCompareDAndBranch* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  Assembler::DoubleCondition doubleCond =
      JSOpToDoubleCondition(comp->cmpMir()->jsop());
  Assembler::Condition cond =
      Assembler::ConditionFromDoubleCondition(doubleCond);

  masm.compareDouble(doubleCond, left, right);
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGenerator::visitCompareFAndBranch(LCompareFAndBranch* comp) {
  const FloatRegister left = ToFloatRegister(comp->left());
  const FloatRegister right = ToFloatRegister(comp->right());
  Assembler::DoubleCondition doubleCond =
      JSOpToDoubleCondition(comp->cmpMir()->jsop());
  Assembler::Condition cond =
      Assembler::ConditionFromDoubleCondition(doubleCond);

  masm.compareFloat(doubleCond, left, right);
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGenerator::visitCompareB(LCompareB* lir) {
  MCompare* mir = lir->mir();
  const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
  const LAllocation* rhs = lir->rhs();
  const Register output = ToRegister(lir->output());
  const Assembler::Condition cond =
      JSOpToCondition(mir->compareType(), mir->jsop());

  vixl::UseScratchRegisterScope temps(&masm.asVIXL());
  const Register scratch = temps.AcquireX().asUnsized();

  MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

  // Load boxed boolean into scratch.
  if (rhs->isConstant()) {
    masm.moveValue(rhs->toConstant()->toJSValue(), ValueOperand(scratch));
  } else {
    masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), scratch);
  }

  // Compare the entire Value.
  masm.cmpPtrSet(cond, lhs.valueReg(), scratch, output);
}

void CodeGenerator::visitCompareBAndBranch(LCompareBAndBranch* lir) {
  MCompare* mir = lir->cmpMir();
  const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
  const LAllocation* rhs = lir->rhs();
  const Assembler::Condition cond =
      JSOpToCondition(mir->compareType(), mir->jsop());

  vixl::UseScratchRegisterScope temps(&masm.asVIXL());
  const Register scratch = temps.AcquireX().asUnsized();

  MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

  // Load boxed boolean into scratch.
  if (rhs->isConstant()) {
    masm.moveValue(rhs->toConstant()->toJSValue(), ValueOperand(scratch));
  } else {
    masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), scratch);
  }

  // Compare the entire Value.
  masm.cmpPtr(lhs.valueReg(), scratch);
  emitBranch(cond, lir->ifTrue(), lir->ifFalse());
}

void CodeGenerator::visitCompareBitwise(LCompareBitwise* lir) {
  MCompare* mir = lir->mir();
  Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
  const ValueOperand lhs = ToValue(lir, LCompareBitwise::LhsInput);
  const ValueOperand rhs = ToValue(lir, LCompareBitwise::RhsInput);
  const Register output = ToRegister(lir->output());

  MOZ_ASSERT(IsEqualityOp(mir->jsop()));

  masm.cmpPtrSet(cond, lhs.valueReg(), rhs.valueReg(), output);
}

void CodeGenerator::visitCompareBitwiseAndBranch(
    LCompareBitwiseAndBranch* lir) {
  MCompare* mir = lir->cmpMir();
  Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
  const ValueOperand lhs = ToValue(lir, LCompareBitwiseAndBranch::LhsInput);
  const ValueOperand rhs = ToValue(lir, LCompareBitwiseAndBranch::RhsInput);

  MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
             mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

  masm.cmpPtr(lhs.valueReg(), rhs.valueReg());
  emitBranch(cond, lir->ifTrue(), lir->ifFalse());
}

void CodeGenerator::visitBitAndAndBranch(LBitAndAndBranch* baab) {
  if (baab->right()->isConstant()) {
    masm.Tst(toWRegister(baab->left()), Operand(ToInt32(baab->right())));
  } else {
    masm.Tst(toWRegister(baab->left()), toWRegister(baab->right()));
  }
  emitBranch(Assembler::NonZero, baab->ifTrue(), baab->ifFalse());
}

void CodeGenerator::visitWasmUint32ToDouble(LWasmUint32ToDouble* lir) {
  masm.convertUInt32ToDouble(ToRegister(lir->input()),
                             ToFloatRegister(lir->output()));
}

void CodeGenerator::visitWasmUint32ToFloat32(LWasmUint32ToFloat32* lir) {
  masm.convertUInt32ToFloat32(ToRegister(lir->input()),
                              ToFloatRegister(lir->output()));
}

void CodeGenerator::visitNotI(LNotI* ins) {
  ARMRegister input = toWRegister(ins->input());
  ARMRegister output = toWRegister(ins->output());

  masm.Cmp(input, ZeroRegister32);
  masm.Cset(output, Assembler::Zero);
}

//        NZCV
// NAN -> 0011
// ==  -> 0110
// <   -> 1000
// >   -> 0010
void CodeGenerator::visitNotD(LNotD* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  ARMRegister output = toWRegister(ins->output());

  // Set output to 1 if input compares equal to 0.0, else 0.
  masm.Fcmp(input, 0.0);
  masm.Cset(output, Assembler::Equal);

  // Comparison with NaN sets V in the NZCV register.
  // If the input was NaN, output must now be zero, so it can be incremented.
  // The instruction is read: "output = if NoOverflow then output else 0+1".
  masm.Csinc(output, output, ZeroRegister32, Assembler::NoOverflow);
}

void CodeGenerator::visitNotF(LNotF* ins) {
  ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  ARMRegister output = toWRegister(ins->output());

  // Set output to 1 input compares equal to 0.0, else 0.
  masm.Fcmp(input, 0.0);
  masm.Cset(output, Assembler::Equal);

  // Comparison with NaN sets V in the NZCV register.
  // If the input was NaN, output must now be zero, so it can be incremented.
  // The instruction is read: "output = if NoOverflow then output else 0+1".
  masm.Csinc(output, output, ZeroRegister32, Assembler::NoOverflow);
}

void CodeGeneratorARM64::storeElementTyped(const LAllocation* value,
                                           MIRType valueType,
                                           MIRType elementType,
                                           Register elements,
                                           const LAllocation* index) {
  MOZ_CRASH("CodeGeneratorARM64::storeElementTyped");
}

void CodeGeneratorARM64::generateInvalidateEpilogue() {
  // Ensure that there is enough space in the buffer for the OsiPoint patching
  // to occur. Otherwise, we could overwrite the invalidation epilogue.
  for (size_t i = 0; i < sizeof(void*); i += Assembler::NopSize()) {
    masm.nop();
  }

  masm.bind(&invalidate_);

  // Push the return address of the point that we bailout out onto the stack.
  masm.push(lr);

  // Push the Ion script onto the stack (when we determine what that pointer
  // is).
  invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));

  TrampolinePtr thunk = gen->jitRuntime()->getInvalidationThunk();
  masm.call(thunk);

  // We should never reach this point in JIT code -- the invalidation thunk
  // should pop the invalidated JS frame and return directly to its caller.
  masm.assumeUnreachable(
      "Should have returned directly to its caller instead of here.");
}

template <class U>
Register getBase(U* mir) {
  switch (mir->base()) {
    case U::Heap:
      return HeapReg;
  }
  return InvalidReg;
}

void CodeGenerator::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins) {
  MOZ_CRASH("visitAsmJSLoadHeap");
}

void CodeGenerator::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins) {
  MOZ_CRASH("visitAsmJSStoreHeap");
}

void CodeGenerator::visitWasmCompareExchangeHeap(
    LWasmCompareExchangeHeap* ins) {
  MOZ_CRASH("visitWasmCompareExchangeHeap");
}

void CodeGenerator::visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap* ins) {
  MOZ_CRASH("visitWasmAtomicBinopHeap");
}

void CodeGenerator::visitWasmStackArg(LWasmStackArg* ins) {
  MOZ_CRASH("visitWasmStackArg");
}

void CodeGenerator::visitUDiv(LUDiv* ins) { MOZ_CRASH("visitUDiv"); }

void CodeGenerator::visitUMod(LUMod* ins) { MOZ_CRASH("visitUMod"); }

void CodeGenerator::visitEffectiveAddress(LEffectiveAddress* ins) {
  const MEffectiveAddress* mir = ins->mir();
  const ARMRegister base = toXRegister(ins->base());
  const ARMRegister index = toXRegister(ins->index());
  const ARMRegister output = toXRegister(ins->output());

  masm.Add(output, base, Operand(index, vixl::LSL, mir->scale()));
  masm.Add(output, output, Operand(mir->displacement()));
}

void CodeGenerator::visitNegI(LNegI* ins) {
  const ARMRegister input = toWRegister(ins->input());
  const ARMRegister output = toWRegister(ins->output());
  masm.Neg(output, input);
}

void CodeGenerator::visitNegD(LNegD* ins) {
  const ARMFPRegister input(ToFloatRegister(ins->input()), 64);
  const ARMFPRegister output(ToFloatRegister(ins->input()), 64);
  masm.Fneg(output, input);
}

void CodeGenerator::visitNegF(LNegF* ins) {
  const ARMFPRegister input(ToFloatRegister(ins->input()), 32);
  const ARMFPRegister output(ToFloatRegister(ins->input()), 32);
  masm.Fneg(output, input);
}

void CodeGenerator::visitCompareExchangeTypedArrayElement(
    LCompareExchangeTypedArrayElement* lir) {
  Register elements = ToRegister(lir->elements());
  AnyRegister output = ToAnyRegister(lir->output());
  Register temp =
      lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

  Register oldval = ToRegister(lir->oldval());
  Register newval = ToRegister(lir->newval());

  Scalar::Type arrayType = lir->mir()->arrayType();
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address dest(elements, ToInt32(lir->index()) * width);
    masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval,
                           newval, temp, output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromElemWidth(width));
    masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval,
                           newval, temp, output);
  }
}

void CodeGenerator::visitAtomicExchangeTypedArrayElement(
    LAtomicExchangeTypedArrayElement* lir) {
  Register elements = ToRegister(lir->elements());
  AnyRegister output = ToAnyRegister(lir->output());
  Register temp =
      lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

  Register value = ToRegister(lir->value());

  Scalar::Type arrayType = lir->mir()->arrayType();
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address dest(elements, ToInt32(lir->index()) * width);
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromElemWidth(width));
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  }
}

void CodeGenerator::visitAddI64(LAddI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitClzI64(LClzI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCtzI64(LCtzI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitMulI64(LMulI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitNotI64(LNotI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitSubI64(LSubI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitPopcntI(LPopcntI*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitBitOpI64(LBitOpI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitShiftI64(LShiftI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitSoftDivI(LSoftDivI*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitSoftModI(LSoftModI*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmLoad(LWasmLoad*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCopySignD(LCopySignD*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCopySignF(LCopySignF*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitNearbyInt(LNearbyInt*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitPopcntI64(LPopcntI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitRotateI64(LRotateI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmStore(LWasmStore*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitCompareI64(LCompareI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitNearbyIntF(LNearbyIntF*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmSelect(LWasmSelect*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmLoadI64(LWasmLoadI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmStoreI64(LWasmStoreI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitMemoryBarrier(LMemoryBarrier*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitSoftUDivOrMod(LSoftUDivOrMod*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmAddOffset(LWasmAddOffset*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitWasmSelectI64(LWasmSelectI64*) { MOZ_CRASH("NYI"); }

void CodeGenerator::visitSignExtendInt64(LSignExtendInt64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmReinterpret(LWasmReinterpret*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmStackArgI64(LWasmStackArgI64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitTestI64AndBranch(LTestI64AndBranch*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWrapInt64ToInt32(LWrapInt64ToInt32*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitExtendInt32ToInt64(LExtendInt32ToInt64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitCompareI64AndBranch(LCompareI64AndBranch*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmTruncateToInt32(LWasmTruncateToInt32*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmReinterpretToI64(LWasmReinterpretToI64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmReinterpretFromI64(LWasmReinterpretFromI64*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitAtomicTypedArrayElementBinop(
    LAtomicTypedArrayElementBinop*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitWasmAtomicBinopHeapForEffect(
    LWasmAtomicBinopHeapForEffect*) {
  MOZ_CRASH("NYI");
}

void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect(
    LAtomicTypedArrayElementBinopForEffect*) {
  MOZ_CRASH("NYI");
}
