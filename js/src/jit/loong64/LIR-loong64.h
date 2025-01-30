/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_loong64_LIR_loong64_h
#define jit_loong64_LIR_loong64_h

namespace js {
namespace jit {

class LUnbox : public LInstructionHelper<1, 1, 0> {
 protected:
  LUnbox(LNode::Opcode opcode, const LAllocation& input)
      : LInstructionHelper(opcode) {
    setOperand(0, input);
  }

 public:
  LIR_HEADER(Unbox);

  explicit LUnbox(const LAllocation& input) : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  static const size_t Input = 0;

  MUnbox* mir() const { return mir_->toUnbox(); }
  const char* extraName() const { return StringFromMIRType(mir()->type()); }
};

class LUnboxFloatingPoint : public LUnbox {
 public:
  LIR_HEADER(UnboxFloatingPoint);

  explicit LUnboxFloatingPoint(const LAllocation& input)
      : LUnbox(classOpcode, input) {}
};

class LDivPowTwoI : public LInstructionHelper<1, 1, 1> {
  const int32_t shift_;

 public:
  LIR_HEADER(DivPowTwoI)

  LDivPowTwoI(const LAllocation& lhs, int32_t shift, const LDefinition& temp)
      : LInstructionHelper(classOpcode), shift_(shift) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }

  const LAllocation* numerator() { return getOperand(0); }
  int32_t shift() const { return shift_; }
  MDiv* mir() const { return mir_->toDiv(); }
};

class LModI : public LBinaryMath<1> {
 public:
  LIR_HEADER(ModI);

  LModI(const LAllocation& lhs, const LAllocation& rhs,
        const LDefinition& callTemp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, callTemp);
  }

  const LDefinition* callTemp() { return getTemp(0); }
  MMod* mir() const { return mir_->toMod(); }
};

class LModMaskI : public LInstructionHelper<1, 1, 2> {
  const int32_t shift_;

 public:
  LIR_HEADER(ModMaskI);

  LModMaskI(const LAllocation& lhs, const LDefinition& temp0,
            const LDefinition& temp1, int32_t shift)
      : LInstructionHelper(classOpcode), shift_(shift) {
    setOperand(0, lhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  int32_t shift() const { return shift_; }
  MMod* mir() const { return mir_->toMod(); }
};

class LMulI : public LBinaryMath<0> {
 public:
  LIR_HEADER(MulI);

  LMulI() : LBinaryMath(classOpcode) {}

  MMul* mir() { return mir_->toMul(); }
};

class LUDivOrMod : public LBinaryMath<0> {
 public:
  LIR_HEADER(UDivOrMod);

  LUDivOrMod() : LBinaryMath(classOpcode) {}

  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }

  bool canBeDivideByZero() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeDivideByZero();
    }
    return mir_->toDiv()->canBeDivideByZero();
  }

  bool trapOnError() const {
    if (mir_->isMod()) {
      return mir_->toMod()->trapOnError();
    }
    return mir_->toDiv()->trapOnError();
  }

  wasm::TrapSiteDesc trapSiteDesc() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    if (mir_->isMod()) {
      return mir_->toMod()->trapSiteDesc();
    }
    return mir_->toDiv()->trapSiteDesc();
  }
};

class LWasmCompareExchangeI64
    : public LInstructionHelper<INT64_PIECES, 2 + INT64_PIECES + INT64_PIECES,
                                0> {
 public:
  LIR_HEADER(WasmCompareExchangeI64);

  LWasmCompareExchangeI64(const LAllocation& ptr,
                          const LInt64Allocation& oldValue,
                          const LInt64Allocation& newValue,
                          const LAllocation& memoryBase)
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setInt64Operand(1, oldValue);
    setInt64Operand(1 + INT64_PIECES, newValue);
    setOperand(1 + 2 * INT64_PIECES, memoryBase);
  }

  const LAllocation* ptr() { return getOperand(0); }
  LInt64Allocation oldValue() { return getInt64Operand(1); }
  LInt64Allocation newValue() { return getInt64Operand(1 + INT64_PIECES); }
  const LAllocation* memoryBase() { return getOperand(1 + 2 * INT64_PIECES); }
  const MWasmCompareExchangeHeap* mir() const {
    return mir_->toWasmCompareExchangeHeap();
  }
};

class LWasmAtomicExchangeI64
    : public LInstructionHelper<INT64_PIECES, 2 + INT64_PIECES, 0> {
 public:
  LIR_HEADER(WasmAtomicExchangeI64);

  LWasmAtomicExchangeI64(const LAllocation& ptr, const LInt64Allocation& value,
                         const LAllocation& memoryBase)
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setInt64Operand(1, value);
    setOperand(1 + INT64_PIECES, memoryBase);
  }

  const LAllocation* ptr() { return getOperand(0); }
  LInt64Allocation value() { return getInt64Operand(1); }
  const LAllocation* memoryBase() { return getOperand(1 + INT64_PIECES); }
  const MWasmAtomicExchangeHeap* mir() const {
    return mir_->toWasmAtomicExchangeHeap();
  }
};

class LWasmAtomicBinopI64
    : public LInstructionHelper<INT64_PIECES, 2 + INT64_PIECES, 2> {
 public:
  LIR_HEADER(WasmAtomicBinopI64);

  LWasmAtomicBinopI64(const LAllocation& ptr, const LInt64Allocation& value,
                      const LAllocation& memoryBase)
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setInt64Operand(1, value);
    setOperand(1 + INT64_PIECES, memoryBase);
  }

  const LAllocation* ptr() { return getOperand(0); }
  LInt64Allocation value() { return getInt64Operand(1); }
  const LAllocation* memoryBase() { return getOperand(1 + INT64_PIECES); }
  const MWasmAtomicBinopHeap* mir() const {
    return mir_->toWasmAtomicBinopHeap();
  }
};

class LDivOrModI64 : public LBinaryMath<1> {
 public:
  LIR_HEADER(DivOrModI64)

  LDivOrModI64(const LAllocation& lhs, const LAllocation& rhs,
               const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }

  const LDefinition* remainder() { return getTemp(0); }
  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }

  bool canBeDivideByZero() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeDivideByZero();
    }
    return mir_->toDiv()->canBeDivideByZero();
  }
  bool canBeNegativeOverflow() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeNegativeDividend();
    }
    return mir_->toDiv()->canBeNegativeOverflow();
  }
  wasm::TrapSiteDesc trapSiteDesc() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    if (mir_->isMod()) {
      return mir_->toMod()->trapSiteDesc();
    }
    return mir_->toDiv()->trapSiteDesc();
  }
};

class LUDivOrModI64 : public LBinaryMath<1> {
 public:
  LIR_HEADER(UDivOrModI64);

  LUDivOrModI64(const LAllocation& lhs, const LAllocation& rhs,
                const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }

  const LDefinition* remainder() { return getTemp(0); }
  const char* extraName() const {
    return mir()->isTruncated() ? "Truncated" : nullptr;
  }

  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }
  bool canBeDivideByZero() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeDivideByZero();
    }
    return mir_->toDiv()->canBeDivideByZero();
  }
  wasm::TrapSiteDesc trapSiteDesc() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    if (mir_->isMod()) {
      return mir_->toMod()->trapSiteDesc();
    }
    return mir_->toDiv()->trapSiteDesc();
  }
};

}  // namespace jit
}  // namespace js

#endif /* jit_loong64_LIR_loong64_h */
