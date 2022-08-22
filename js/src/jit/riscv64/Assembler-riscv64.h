/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_riscv64_Assembler_riscv64_h
#define jit_riscv64_Assembler_riscv64_h

#include "mozilla/Assertions.h"

#include <stdint.h>

#include "jit/Registers.h"
#include "jit/RegisterSets.h"
#include "jit/riscv64/Architecture-riscv64.h"
#include "jit/riscv64/extension/base-assembler-riscv.h"
#include "jit/riscv64/extension/base-riscv-i.h"
#include "jit/riscv64/extension/extension-riscv-a.h"
#include "jit/riscv64/extension/extension-riscv-c.h"
#include "jit/riscv64/extension/extension-riscv-d.h"
#include "jit/riscv64/extension/extension-riscv-f.h"
#include "jit/riscv64/extension/extension-riscv-m.h"
#include "jit/riscv64/extension/extension-riscv-v.h"
#include "jit/riscv64/extension/extension-riscv-zicsr.h"
#include "jit/riscv64/extension/extension-riscv-zifencei.h"
#include "jit/riscv64/Register-riscv64.h"
#include "jit/shared/Assembler-shared.h"
namespace js {
namespace jit {

struct ScratchFloat32Scope : public AutoFloatRegisterScope {
  explicit ScratchFloat32Scope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchFloat32Reg) {}
};

struct ScratchDoubleScope : public AutoFloatRegisterScope {
  explicit ScratchDoubleScope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchDoubleReg) {}
};

class MacroAssembler;

#if defined(JS_NUNBOX32)
static constexpr ValueOperand JSReturnOperand(InvalidReg, InvalidReg);
static constexpr Register64 ReturnReg64(InvalidReg, InvalidReg);
#elif defined(JS_PUNBOX64)
static constexpr ValueOperand JSReturnOperand(InvalidReg);
static constexpr Register64 ReturnReg64(InvalidReg);
#else
#  error "Bad architecture"
#endif

static constexpr uint32_t ABIStackAlignment = 16;
static constexpr uint32_t CodeAlignment = 16;
static constexpr uint32_t JitStackAlignment = 16;
static constexpr uint32_t JitStackValueAlignment =
    JitStackAlignment / sizeof(Value);

static const Scale ScalePointer = TimesEight;

class Assembler : public AssemblerShared,
                  public AssemblerRISCVI,
                  public AssemblerRISCVA,
                  public AssemblerRISCVF,
                  public AssemblerRISCVD,
                  public AssemblerRISCVM,
                  public AssemblerRISCVC,
                  public AssemblerRISCVZicsr,
                  public AssemblerRISCVZifencei {
 public:
  enum Condition {
    Equal,
    NotEqual,
    Above,
    AboveOrEqual,
    Below,
    BelowOrEqual,
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual,
    Overflow,
    CarrySet,
    CarryClear,
    Signed,
    NotSigned,
    Zero,
    NonZero,
    Always,
  };

  enum DoubleCondition {
    // These conditions will only evaluate to true if the comparison is ordered
    // - i.e. neither operand is NaN.
    DoubleOrdered,
    DoubleEqual,
    DoubleNotEqual,
    DoubleGreaterThan,
    DoubleGreaterThanOrEqual,
    DoubleLessThan,
    DoubleLessThanOrEqual,
    // If either operand is NaN, these conditions always evaluate to true.
    DoubleUnordered,
    DoubleEqualOrUnordered,
    DoubleNotEqualOrUnordered,
    DoubleGreaterThanOrUnordered,
    DoubleGreaterThanOrEqualOrUnordered,
    DoubleLessThanOrUnordered,
    DoubleLessThanOrEqualOrUnordered
  };

  virtual int32_t branch_offset_helper(Label* L, OffsetSize bits) { MOZ_CRASH(); }

  virtual void emit(Instr x) { MOZ_CRASH(); }
  virtual void emit(ShortInstr x) { MOZ_CRASH(); }
  virtual void emit(uint64_t x) { MOZ_CRASH(); }

  virtual void BlockTrampolinePoolFor(int instructions) { MOZ_CRASH(); }

  static Condition InvertCondition(Condition) { MOZ_CRASH(); }

  static DoubleCondition InvertCondition(DoubleCondition) { MOZ_CRASH(); }

  template <typename T, typename S>
  static void PatchDataWithValueCheck(CodeLocationLabel, T, S) {
    MOZ_CRASH();
  }
  static void PatchWrite_Imm32(CodeLocationLabel, Imm32) { MOZ_CRASH(); }

  static void PatchWrite_NearCall(CodeLocationLabel, CodeLocationLabel) {
    MOZ_CRASH();
  }
  static uint32_t PatchWrite_NearCallSize() { MOZ_CRASH(); }

  static void ToggleToJmp(CodeLocationLabel) { MOZ_CRASH(); }
  static void ToggleToCmp(CodeLocationLabel) { MOZ_CRASH(); }
  static void ToggleCall(CodeLocationLabel, bool) { MOZ_CRASH(); }

  static void Bind(uint8_t*, const CodeLabel&) { MOZ_CRASH(); }

  static uintptr_t GetPointer(uint8_t*) { MOZ_CRASH(); }

  static bool HasRoundInstruction(RoundingMode) { return false; }

  void verifyHeapAccessDisassembly(uint32_t begin, uint32_t end,
                                   const Disassembler::HeapAccess& heapAccess) {
    MOZ_CRASH();
  }

  void setUnlimitedBuffer() { MOZ_CRASH(); }
};

class Operand {
 public:
  explicit Operand(const Address&) { MOZ_CRASH(); }
  explicit Operand(const Register) { MOZ_CRASH(); }
  explicit Operand(const FloatRegister) { MOZ_CRASH(); }
  explicit Operand(Register, Imm32) { MOZ_CRASH(); }
  explicit Operand(Register, int32_t) { MOZ_CRASH(); }
};

class ABIArgGenerator {
 public:
  ABIArgGenerator() { MOZ_CRASH(); }
  ABIArg next(MIRType) { MOZ_CRASH(); }
  ABIArg& current() { MOZ_CRASH(); }
  uint32_t stackBytesConsumedSoFar() const { MOZ_CRASH(); }
  void increaseStackOffset(uint32_t) { MOZ_CRASH(); }
};

// Helper classes for ScratchRegister usage. Asserts that only one piece
// of code thinks it has exclusive ownership of each scratch register.
struct ScratchRegisterScope : public AutoRegisterScope {
  explicit ScratchRegisterScope(MacroAssembler& masm)
      : AutoRegisterScope(masm, ScratchRegister) {}
};

struct SecondScratchRegisterScope : public AutoRegisterScope {
  explicit SecondScratchRegisterScope(MacroAssembler& masm)
      : AutoRegisterScope(masm, SecondScratchReg) {}
};

static const uint32_t NumIntArgRegs = 8;
static const uint32_t NumFloatArgRegs = 8;

static inline bool GetIntArgReg(uint32_t usedIntArgs, Register& out) {
  if (usedIntArgs < NumIntArgRegs) {
    out = Register::FromCode(a0.code() + usedIntArgs);
    return true;
  }
  return false;
}

static inline bool GetFloatArgReg(uint32_t usedFloatArgs, FloatRegister* out) {
  if (usedFloatArgs < NumFloatArgRegs) {
    *out = FloatRegister::FromCode(fa0.code() + usedFloatArgs);
    return true;
  }
  return false;
}
}  // namespace jit
}  // namespace js

#endif /* jit_riscv64_Assembler_riscv64_h */
