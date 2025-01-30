/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm64_LIR_arm64_h
#define jit_arm64_LIR_arm64_h

namespace js {
namespace jit {

class LUnboxBase : public LInstructionHelper<1, 1, 0> {
 public:
  LUnboxBase(LNode::Opcode opcode, const LAllocation& input)
      : LInstructionHelper(opcode) {
    setOperand(0, input);
  }

  static const size_t Input = 0;

  MUnbox* mir() const { return mir_->toUnbox(); }
};

class LUnbox : public LUnboxBase {
 public:
  LIR_HEADER(Unbox);

  explicit LUnbox(const LAllocation& input) : LUnboxBase(classOpcode, input) {}

  const char* extraName() const { return StringFromMIRType(mir()->type()); }
};

class LUnboxFloatingPoint : public LUnboxBase {
 public:
  LIR_HEADER(UnboxFloatingPoint);

  explicit LUnboxFloatingPoint(const LAllocation& input)
      : LUnboxBase(classOpcode, input) {}
};

class LDivPowTwoI : public LInstructionHelper<1, 1, 0> {
  const int32_t shift_;
  const bool negativeDivisor_;

 public:
  LIR_HEADER(DivPowTwoI)

  LDivPowTwoI(const LAllocation& lhs, int32_t shift, bool negativeDivisor)
      : LInstructionHelper(classOpcode),
        shift_(shift),
        negativeDivisor_(negativeDivisor) {
    setOperand(0, lhs);
  }

  const LAllocation* numerator() { return getOperand(0); }

  int32_t shift() { return shift_; }
  bool negativeDivisor() { return negativeDivisor_; }

  MDiv* mir() const { return mir_->toDiv(); }
};

class LDivConstantI : public LInstructionHelper<1, 1, 1> {
  const int32_t denominator_;

 public:
  LIR_HEADER(DivConstantI)

  LDivConstantI(const LAllocation& lhs, int32_t denominator,
                const LDefinition& temp)
      : LInstructionHelper(classOpcode), denominator_(denominator) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }

  const LAllocation* numerator() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  int32_t denominator() const { return denominator_; }
  MDiv* mir() const { return mir_->toDiv(); }
  bool canBeNegativeDividend() const { return mir()->canBeNegativeDividend(); }
};

class LUDivConstantI : public LInstructionHelper<1, 1, 1> {
  const int32_t denominator_;

 public:
  LIR_HEADER(UDivConstantI)

  LUDivConstantI(const LAllocation& lhs, int32_t denominator,
                 const LDefinition& temp)
      : LInstructionHelper(classOpcode), denominator_(denominator) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }

  const LAllocation* numerator() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  int32_t denominator() const { return denominator_; }
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

class LMulI : public LBinaryMath<0> {
 public:
  LIR_HEADER(MulI);

  LMulI() : LBinaryMath(classOpcode) {}

  MMul* mir() { return mir_->toMul(); }
};

class LUDiv : public LBinaryMath<1> {
 public:
  LIR_HEADER(UDiv);

  LUDiv(const LAllocation& lhs, const LAllocation& rhs,
        const LDefinition& remainder)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, remainder);
  }

  const LDefinition* remainder() { return getTemp(0); }

  MDiv* mir() { return mir_->toDiv(); }
};

class LUMod : public LBinaryMath<0> {
 public:
  LIR_HEADER(UMod);

  LUMod(const LAllocation& lhs, const LAllocation& rhs)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
  }

  MMod* mir() { return mir_->toMod(); }
};

class LDivOrModI64 : public LBinaryMath<0> {
 public:
  LIR_HEADER(DivOrModI64)

  LDivOrModI64(const LAllocation& lhs, const LAllocation& rhs)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
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

class LUDivOrModI64 : public LBinaryMath<0> {
 public:
  LIR_HEADER(UDivOrModI64);

  LUDivOrModI64(const LAllocation& lhs, const LAllocation& rhs)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
  }

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

#endif /* jit_arm64_LIR_arm64_h */
