/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips32_LIR_mips32_h
#define jit_mips32_LIR_mips32_h

namespace js {
namespace jit {

class LUnbox : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(Unbox);

  LUnbox() : LInstructionHelper(classOpcode) {}

  MUnbox* mir() const { return mir_->toUnbox(); }
  const LAllocation* payload() { return getOperand(0); }
  const LAllocation* type() { return getOperand(1); }
  const char* extraName() const { return StringFromMIRType(mir()->type()); }
};

class LDivOrModI64
    : public LCallInstructionHelper<INT64_PIECES, INT64_PIECES * 2, 0> {
 public:
  LIR_HEADER(DivOrModI64)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;

  LDivOrModI64(const LInt64Allocation& lhs, const LInt64Allocation& rhs)
      : LCallInstructionHelper(classOpcode) {
    setInt64Operand(Lhs, lhs);
    setInt64Operand(Rhs, rhs);
  }

  LInt64Allocation lhs() const { return getInt64Operand(Lhs); }
  LInt64Allocation rhs() const { return getInt64Operand(Rhs); }

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

class LUDivOrModI64
    : public LCallInstructionHelper<INT64_PIECES, INT64_PIECES * 2, 0> {
 public:
  LIR_HEADER(UDivOrModI64)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;

  LUDivOrModI64(const LInt64Allocation& lhs, const LInt64Allocation& rhs)
      : LCallInstructionHelper(classOpcode) {
    setInt64Operand(Lhs, lhs);
    setInt64Operand(Rhs, rhs);
  }

  LInt64Allocation lhs() const { return getInt64Operand(Lhs); }
  LInt64Allocation rhs() const { return getInt64Operand(Rhs); }

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

// Definitions for `extraName` methods of generated LIR instructions.

#ifdef JS_JITSPEW
const char* LBoxFloatingPoint::extraName() const {
  return StringFromMIRType(type_);
}
#endif

}  // namespace jit
}  // namespace js

#endif /* jit_mips32_LIR_mips32_h */
