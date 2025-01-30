/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm_LIR_arm_h
#define jit_arm_LIR_arm_h

namespace js {
namespace jit {

class LBoxFloatingPoint : public LInstructionHelper<2, 1, 1> {
  MIRType type_;

 public:
  LIR_HEADER(BoxFloatingPoint);

  LBoxFloatingPoint(const LAllocation& in, const LDefinition& temp,
                    MIRType type)
      : LInstructionHelper(classOpcode), type_(type) {
    setOperand(0, in);
    setTemp(0, temp);
  }

  MIRType type() const { return type_; }
  const char* extraName() const { return StringFromMIRType(type_); }
};

class LUnbox : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(Unbox);

  LUnbox() : LInstructionHelper(classOpcode) {}

  MUnbox* mir() const { return mir_->toUnbox(); }
  const LAllocation* payload() { return getOperand(0); }
  const LAllocation* type() { return getOperand(1); }
  const char* extraName() const { return StringFromMIRType(mir()->type()); }
};

class LUnboxFloatingPoint : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(UnboxFloatingPoint);

  static const size_t Input = 0;

  explicit LUnboxFloatingPoint(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  MUnbox* mir() const { return mir_->toUnbox(); }
};

class LDivOrModI64
    : public LCallInstructionHelper<INT64_PIECES, INT64_PIECES * 2 + 1, 0> {
 public:
  LIR_HEADER(DivOrModI64)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;
  static const size_t Instance = 2 * INT64_PIECES;

  LDivOrModI64(const LInt64Allocation& lhs, const LInt64Allocation& rhs,
               const LAllocation& instance)
      : LCallInstructionHelper(classOpcode) {
    setInt64Operand(Lhs, lhs);
    setInt64Operand(Rhs, rhs);
    setOperand(Instance, instance);
  }

  LInt64Allocation lhs() const { return getInt64Operand(Lhs); }
  LInt64Allocation rhs() const { return getInt64Operand(Rhs); }
  const LAllocation* instance() const { return getOperand(Instance); }

  MDefinition* mir() const {
    MOZ_ASSERT(mir_->isWasmBuiltinDivI64() || mir_->isWasmBuiltinModI64());
    return mir_;
  }
  bool canBeDivideByZero() const {
    if (mir_->isWasmBuiltinModI64()) {
      return mir_->toWasmBuiltinModI64()->canBeDivideByZero();
    }
    return mir_->toWasmBuiltinDivI64()->canBeDivideByZero();
  }
  bool canBeNegativeOverflow() const {
    if (mir_->isWasmBuiltinModI64()) {
      return mir_->toWasmBuiltinModI64()->canBeNegativeDividend();
    }
    return mir_->toWasmBuiltinDivI64()->canBeNegativeOverflow();
  }
  const wasm::TrapSiteDesc& trapSiteDesc() const {
    MOZ_ASSERT(mir_->isWasmBuiltinDivI64() || mir_->isWasmBuiltinModI64());
    if (mir_->isWasmBuiltinModI64()) {
      return mir_->toWasmBuiltinModI64()->trapSiteDesc();
    }
    return mir_->toWasmBuiltinDivI64()->trapSiteDesc();
  }
};

class LUDivOrModI64
    : public LCallInstructionHelper<INT64_PIECES, INT64_PIECES * 2 + 1, 0> {
 public:
  LIR_HEADER(UDivOrModI64)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;
  static const size_t Instance = 2 * INT64_PIECES;

  LUDivOrModI64(const LInt64Allocation& lhs, const LInt64Allocation& rhs,
                const LAllocation& instance)
      : LCallInstructionHelper(classOpcode) {
    setInt64Operand(Lhs, lhs);
    setInt64Operand(Rhs, rhs);
    setOperand(Instance, instance);
  }

  LInt64Allocation lhs() const { return getInt64Operand(Lhs); }
  LInt64Allocation rhs() const { return getInt64Operand(Rhs); }
  const LAllocation* instance() const { return getOperand(Instance); }

  MDefinition* mir() const {
    MOZ_ASSERT(mir_->isWasmBuiltinDivI64() || mir_->isWasmBuiltinModI64());
    return mir_;
  }
  bool canBeDivideByZero() const {
    if (mir_->isWasmBuiltinModI64()) {
      return mir_->toWasmBuiltinModI64()->canBeDivideByZero();
    }
    return mir_->toWasmBuiltinDivI64()->canBeDivideByZero();
  }
  bool canBeNegativeOverflow() const {
    if (mir_->isWasmBuiltinModI64()) {
      return mir_->toWasmBuiltinModI64()->canBeNegativeDividend();
    }
    return mir_->toWasmBuiltinDivI64()->canBeNegativeOverflow();
  }
  const wasm::TrapSiteDesc& trapSiteDesc() const {
    MOZ_ASSERT(mir_->isWasmBuiltinDivI64() || mir_->isWasmBuiltinModI64());
    if (mir_->isWasmBuiltinModI64()) {
      return mir_->toWasmBuiltinModI64()->trapSiteDesc();
    }
    return mir_->toWasmBuiltinDivI64()->trapSiteDesc();
  }
};

// LSoftDivI is a software divide for ARM cores that don't support a hardware
// divide instruction, implemented as a C++ native call.
class LSoftDivI : public LBinaryCallInstructionHelper<1, 0> {
 public:
  LIR_HEADER(SoftDivI);

  LSoftDivI(const LAllocation& lhs, const LAllocation& rhs)
      : LBinaryCallInstructionHelper(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
  }

  MDiv* mir() const { return mir_->toDiv(); }
};

class LDivPowTwoI : public LInstructionHelper<1, 1, 0> {
  const int32_t shift_;

 public:
  LIR_HEADER(DivPowTwoI)

  LDivPowTwoI(const LAllocation& lhs, int32_t shift)
      : LInstructionHelper(classOpcode), shift_(shift) {
    setOperand(0, lhs);
  }

  const LAllocation* numerator() { return getOperand(0); }

  int32_t shift() { return shift_; }

  MDiv* mir() const { return mir_->toDiv(); }
};

class LModI : public LBinaryMath<0> {
 public:
  LIR_HEADER(ModI);

  LModI(const LAllocation& lhs, const LAllocation& rhs)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
  }

  MMod* mir() const { return mir_->toMod(); }
};

class LSoftModI : public LBinaryCallInstructionHelper<1, 1> {
 public:
  LIR_HEADER(SoftModI);

  LSoftModI(const LAllocation& lhs, const LAllocation& rhs,
            const LDefinition& temp)
      : LBinaryCallInstructionHelper(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }

  const LDefinition* callTemp() { return getTemp(0); }

  MMod* mir() const { return mir_->toMod(); }
};

class LMulI : public LBinaryMath<0> {
 public:
  LIR_HEADER(MulI);

  LMulI() : LBinaryMath(classOpcode) {}

  MMul* mir() { return mir_->toMul(); }
};

class LUDiv : public LBinaryMath<0> {
 public:
  LIR_HEADER(UDiv);

  LUDiv() : LBinaryMath(classOpcode) {}

  MDiv* mir() { return mir_->toDiv(); }
};

class LUMod : public LBinaryMath<0> {
 public:
  LIR_HEADER(UMod);

  LUMod() : LBinaryMath(classOpcode) {}

  MMod* mir() { return mir_->toMod(); }
};

class LSoftUDivOrMod : public LBinaryCallInstructionHelper<1, 0> {
 public:
  LIR_HEADER(SoftUDivOrMod);

  LSoftUDivOrMod(const LAllocation& lhs, const LAllocation& rhs)
      : LBinaryCallInstructionHelper(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
  }

  MInstruction* mir() { return mir_->toInstruction(); }
};

}  // namespace jit
}  // namespace js

#endif /* jit_arm_LIR_arm_h */
