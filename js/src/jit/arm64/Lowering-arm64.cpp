/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/Lowering-arm64.h"

#include "jit/arm64/Assembler-arm64.h"
#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;

LBoxAllocation LIRGeneratorARM64::useBoxFixed(MDefinition* mir, Register reg1,
                                              Register, bool useAtStart) {
  MOZ_ASSERT(mir->type() == MIRType::Value);

  ensureDefined(mir);
  return LBoxAllocation(LUse(reg1, mir->virtualRegister(), useAtStart));
}

LAllocation LIRGeneratorARM64::useByteOpRegister(MDefinition* mir) {
  return useRegister(mir);
}

LAllocation LIRGeneratorARM64::useByteOpRegisterAtStart(MDefinition* mir) {
  return useRegisterAtStart(mir);
}

LAllocation LIRGeneratorARM64::useByteOpRegisterOrNonDoubleConstant(
    MDefinition* mir) {
  return useRegisterOrNonDoubleConstant(mir);
}

void LIRGenerator::visitBox(MBox* box) {
  MDefinition* opd = box->getOperand(0);

  // If the operand is a constant, emit near its uses.
  if (opd->isConstant() && box->canEmitAtUses()) {
    emitAtUses(box);
    return;
  }

  if (opd->isConstant()) {
    define(new (alloc()) LValue(opd->toConstant()->toJSValue()), box,
           LDefinition(LDefinition::BOX));
  } else {
    LBox* ins = new (alloc()) LBox(useRegister(opd), opd->type());
    define(ins, box, LDefinition(LDefinition::BOX));
  }
}

void LIRGenerator::visitUnbox(MUnbox* unbox) {
  MDefinition* box = unbox->getOperand(0);

  if (box->type() == MIRType::ObjectOrNull) {
    LUnboxObjectOrNull* lir =
        new (alloc()) LUnboxObjectOrNull(useRegisterAtStart(box));
    if (unbox->fallible()) {
      assignSnapshot(lir, unbox->bailoutKind());
    }
    defineReuseInput(lir, unbox, 0);
    return;
  }

  MOZ_ASSERT(box->type() == MIRType::Value);

  LUnboxBase* lir;
  if (IsFloatingPointType(unbox->type())) {
    lir = new (alloc())
        LUnboxFloatingPoint(useRegisterAtStart(box), unbox->type());
  } else if (unbox->fallible()) {
    // If the unbox is fallible, load the Value in a register first to
    // avoid multiple loads.
    lir = new (alloc()) LUnbox(useRegisterAtStart(box));
  } else {
    // FIXME: It should be possible to useAtStart() here, but the DEBUG
    // code in CodeGenerator::visitUnbox() needs to handle non-Register
    // cases. ARM64 doesn't have an Operand type.
    lir = new (alloc()) LUnbox(useRegisterAtStart(box));
  }

  if (unbox->fallible()) {
    assignSnapshot(lir, unbox->bailoutKind());
  }

  define(lir, unbox);
}

void LIRGenerator::visitReturn(MReturn* ret) {
  MDefinition* opd = ret->getOperand(0);
  MOZ_ASSERT(opd->type() == MIRType::Value);

  LReturn* ins = new (alloc()) LReturn;
  ins->setOperand(0, useFixed(opd, JSReturnReg));
  add(ins);
}

// x = !y
void LIRGeneratorARM64::lowerForALU(LInstructionHelper<1, 1, 0>* ins,
                                    MDefinition* mir, MDefinition* input) {
  ins->setOperand(
      0, ins->snapshot() ? useRegister(input) : useRegisterAtStart(input));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

// z = x+y
void LIRGeneratorARM64::lowerForALU(LInstructionHelper<1, 2, 0>* ins,
                                    MDefinition* mir, MDefinition* lhs,
                                    MDefinition* rhs) {
  ins->setOperand(0,
                  ins->snapshot() ? useRegister(lhs) : useRegisterAtStart(lhs));
  ins->setOperand(1, ins->snapshot() ? useRegisterOrConstant(rhs)
                                     : useRegisterOrConstantAtStart(rhs));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

void LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 1, 0>* ins,
                                    MDefinition* mir, MDefinition* input) {
  ins->setOperand(0, useRegisterAtStart(input));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

template <size_t Temps>
void LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 2, Temps>* ins,
                                    MDefinition* mir, MDefinition* lhs,
                                    MDefinition* rhs) {
  ins->setOperand(0, useRegisterAtStart(lhs));
  ins->setOperand(1, useRegisterAtStart(rhs));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

template void LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 2, 0>* ins,
                                             MDefinition* mir, MDefinition* lhs,
                                             MDefinition* rhs);
template void LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 2, 1>* ins,
                                             MDefinition* mir, MDefinition* lhs,
                                             MDefinition* rhs);

void LIRGeneratorARM64::lowerForALUInt64(
    LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs) {
  MOZ_CRASH("NYI");
}

void LIRGeneratorARM64::lowerForMulInt64(LMulI64* ins, MMul* mir,
                                         MDefinition* lhs, MDefinition* rhs) {
  MOZ_CRASH("NYI");
}

template <size_t Temps>
void LIRGeneratorARM64::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, Temps>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs) {
  MOZ_CRASH("NYI");
}

template void LIRGeneratorARM64::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, 0>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs);
template void LIRGeneratorARM64::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, 1>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs);

void LIRGeneratorARM64::lowerForBitAndAndBranch(LBitAndAndBranch* baab,
                                                MInstruction* mir,
                                                MDefinition* lhs,
                                                MDefinition* rhs) {
  baab->setOperand(0, useRegisterAtStart(lhs));
  baab->setOperand(1, useRegisterOrConstantAtStart(rhs));
  add(baab, mir);
}

void LIRGeneratorARM64::lowerUntypedPhiInput(MPhi* phi, uint32_t inputPosition,
                                             LBlock* block, size_t lirIndex) {
  lowerTypedPhiInput(phi, inputPosition, block, lirIndex);
}

void LIRGeneratorARM64::lowerForShift(LInstructionHelper<1, 2, 0>* ins,
                                      MDefinition* mir, MDefinition* lhs,
                                      MDefinition* rhs) {
  ins->setOperand(0, useRegister(lhs));
  ins->setOperand(1, useRegisterOrConstant(rhs));
  define(ins, mir);
}

void LIRGeneratorARM64::lowerDivI(MDiv* div) {
  if (div->isUnsigned()) {
    lowerUDiv(div);
    return;
  }

  // TODO: Implement the division-avoidance paths when rhs is constant.

  LDivI* lir = new (alloc())
      LDivI(useRegister(div->lhs()), useRegister(div->rhs()), temp());
  if (div->fallible()) {
    assignSnapshot(lir, Bailout_DoubleOutput);
  }
  define(lir, div);
}

void LIRGeneratorARM64::lowerMulI(MMul* mul, MDefinition* lhs,
                                  MDefinition* rhs) {
  LMulI* lir = new(alloc()) LMulI;
  if (mul->fallible()) {
      assignSnapshot(lir, Bailout_DoubleOutput);
  }
  lowerForALU(lir, mul, lhs, rhs);
}

void LIRGeneratorARM64::lowerModI(MMod* mod) { MOZ_CRASH("lowerModI"); }

void LIRGeneratorARM64::lowerDivI64(MDiv* div) { MOZ_CRASH("NYI"); }

void LIRGeneratorARM64::lowerModI64(MMod* mod) { MOZ_CRASH("NYI"); }

void LIRGenerator::visitPowHalf(MPowHalf* ins) {
  MDefinition* input = ins->input();
  MOZ_ASSERT(input->type() == MIRType::Double);
  LPowHalfD* lir = new (alloc()) LPowHalfD(useRegister(input));
  define(lir, ins);
}

LTableSwitch* LIRGeneratorARM64::newLTableSwitch(const LAllocation& in,
                                                 const LDefinition& inputCopy,
                                                 MTableSwitch* tableswitch) {
  return new (alloc()) LTableSwitch(in, inputCopy, tableswitch);
}

LTableSwitchV* LIRGeneratorARM64::newLTableSwitchV(MTableSwitch* tableswitch) {
  MOZ_CRASH("NYI");
}

void LIRGeneratorARM64::lowerUrshD(MUrsh* mir) {
  MDefinition* lhs = mir->lhs();
  MDefinition* rhs = mir->rhs();

  MOZ_ASSERT(lhs->type() == MIRType::Int32);
  MOZ_ASSERT(rhs->type() == MIRType::Int32);

  LUrshD* lir = new (alloc())
      LUrshD(useRegister(lhs), useRegisterOrConstant(rhs), temp());
  define(lir, mir);
}

void LIRGenerator::visitWasmNeg(MWasmNeg* ins) {
  switch (ins->type()) {
    case MIRType::Int32:
      define(new (alloc()) LNegI(useRegisterAtStart(ins->input())), ins);
      break;
    case MIRType::Float32:
      define(new (alloc()) LNegF(useRegisterAtStart(ins->input())), ins);
      break;
    case MIRType::Double:
      define(new (alloc()) LNegD(useRegisterAtStart(ins->input())), ins);
      break;
    default:
      MOZ_CRASH("unexpected type");
  }
}

void LIRGenerator::visitWasmSelect(MWasmSelect* ins) {
  MOZ_CRASH("visitWasmSelect");
}

void LIRGeneratorARM64::lowerUDiv(MDiv* div) { MOZ_CRASH("lowerUDiv"); }

void LIRGeneratorARM64::lowerUMod(MMod* mod) { MOZ_CRASH("lowerUMod"); }

void LIRGenerator::visitWasmUnsignedToDouble(MWasmUnsignedToDouble* ins) {
  MOZ_CRASH("visitWasmUnsignedToDouble");
}

void LIRGenerator::visitWasmUnsignedToFloat32(MWasmUnsignedToFloat32* ins) {
  MOZ_CRASH("visitWasmUnsignedToFloat32");
}

void LIRGenerator::visitAsmJSLoadHeap(MAsmJSLoadHeap* ins) {
  MOZ_CRASH("visitAsmJSLoadHeap");
}

void LIRGenerator::visitAsmJSStoreHeap(MAsmJSStoreHeap* ins) {
  MOZ_CRASH("visitAsmJSStoreHeap");
}

void LIRGenerator::visitWasmCompareExchangeHeap(MWasmCompareExchangeHeap* ins) {
  MOZ_CRASH("visitWasmCompareExchangeHeap");
}

void LIRGenerator::visitWasmAtomicExchangeHeap(MWasmAtomicExchangeHeap* ins) {
  MOZ_CRASH("visitWasmAtomicExchangeHeap");
}

void LIRGenerator::visitWasmAtomicBinopHeap(MWasmAtomicBinopHeap* ins) {
  MOZ_CRASH("visitWasmAtomicBinopHeap");
}

void LIRGeneratorARM64::lowerTruncateDToInt32(MTruncateToInt32* ins) {
  MOZ_CRASH("lowerTruncateDToInt32");
}

void LIRGeneratorARM64::lowerTruncateFToInt32(MTruncateToInt32* ins) {
  MOZ_CRASH("lowerTruncateFToInt32");
}

void LIRGenerator::visitAtomicTypedArrayElementBinop(
    MAtomicTypedArrayElementBinop* ins) {
  MOZ_CRASH("NYI");
}

void LIRGenerator::visitCompareExchangeTypedArrayElement(
    MCompareExchangeTypedArrayElement* ins) {
  MOZ_CRASH("NYI");
}

void LIRGenerator::visitAtomicExchangeTypedArrayElement(
    MAtomicExchangeTypedArrayElement* ins) {
  MOZ_CRASH("NYI");
}

void LIRGenerator::visitSubstr(MSubstr* ins) {
  LSubstr* lir = new (alloc())
      LSubstr(useRegister(ins->string()), useRegister(ins->begin()),
              useRegister(ins->length()), temp(), temp(), temp());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGenerator::visitRandom(MRandom* ins) {
  LRandom* lir = new (alloc()) LRandom(temp(), temp(), temp());
  defineFixed(lir, ins, LFloatReg(ReturnDoubleReg));
}

void LIRGenerator::visitWasmTruncateToInt64(MWasmTruncateToInt64* ins) {
  MOZ_CRASH("NYI");
}

void LIRGenerator::visitWasmLoad(MWasmLoad* ins) { MOZ_CRASH("NYI"); }

void LIRGenerator::visitWasmStore(MWasmStore* ins) { MOZ_CRASH("NYI"); }

void LIRGenerator::visitInt64ToFloatingPoint(MInt64ToFloatingPoint* ins) {
  MOZ_CRASH("NYI");
}

void LIRGenerator::visitCopySign(MCopySign* ins) { MOZ_CRASH("NYI"); }

void LIRGenerator::visitExtendInt32ToInt64(MExtendInt32ToInt64* ins) {
  MOZ_CRASH("NYI");
}

void LIRGenerator::visitSignExtendInt64(MSignExtendInt64* ins) {
  MOZ_CRASH("NYI");
}
