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
#include "jit/shared/Assembler-shared.h"

namespace js {
namespace jit {

class MacroAssembler;

static constexpr Register StackPointer{Registers::sp};
static constexpr Register FramePointer{Registers::fp};
static constexpr Register ReturnReg{Registers::a0};
static constexpr FloatRegister InvalidFloatReg = {FloatRegisters::invalid_reg};
static constexpr FloatRegister ReturnFloat32Reg = {FloatRegisters::fa0, Single};
static constexpr FloatRegister ReturnDoubleReg = {FloatRegisters::fa0, Double};
static constexpr FloatRegister ReturnSimd128Reg = InvalidFloatReg;
static constexpr FloatRegister ScratchSimd128Reg = InvalidFloatReg;
static constexpr FloatRegister ScratchFloat32Reg_ =
    FloatRegister(FloatRegisters::ft0, FloatRegisters::Single);
static constexpr FloatRegister ScratchDoubleReg_ = {FloatResgisters::ft0,
                                                    FloatRegisters::Double};

struct ScratchFloat32Scope : AutoFloatRegisterScope {
  explicit ScratchFloat32Scope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchFloat32Reg_) {}
};

struct ScratchDoubleScope : AutoFloatRegisterScope {
  explicit ScratchDoubleScope(MacroAssembler& masm)
      : AutoFloatRegisterScope(masm, ScratchDoubleReg_) {}
};

static constexpr Register OsrFrameReg{Registers::invalid_reg};
static constexpr Register PreBarrierReg{Registers::invalid_reg};
static constexpr Register InterpreterPCReg{Registers::invalid_reg};
static constexpr Register CallTempReg0{Registers::t0};
static constexpr Register CallTempReg1{Registers::t1};
static constexpr Register CallTempReg2{Registers::t2};
static constexpr Register CallTempReg3{Registers::t3};
static constexpr Register CallTempReg4{Registers::t4};
static constexpr Register CallTempReg5{Registers::t5};
static constexpr Register CallTempReg6{Registers::t6};
static constexpr Register InvalidReg{Registers::invalid_reg};
static constexpr Register CallTempNonArgRegs[] = {
    Registers::t0, Registers::t1, Registers::t2, Registers::t3,
    Registers::t4, Registers::t5, Registers::t6};
static const uint32_t NumCallTempNonArgRegs = std::size(CallTempNonArgRegs);

static constexpr Register IntArgReg0{Registers::a0};
static constexpr Register IntArgReg1{Registers::a1};
static constexpr Register IntArgReg2{Registers::a2};
static constexpr Register IntArgReg3{Registers::a3};
static constexpr Register IntArgReg4{Registers::a4};
static constexpr Register IntArgReg5{Registers::a5};
static constexpr Register IntArgReg6{Registers::a6};
static constexpr Register IntArgReg7{Registers::a7};
static constexpr Register HeapReg{Registers::invalid_reg};

// Registerd used in RegExpTester instruction (do not use ReturnReg).
static constexpr Register RegExpTesterRegExpReg = CallTempReg0;
static constexpr Register RegExpTesterStringReg = CallTempReg1;
static constexpr Register RegExpTesterLastIndexReg = CallTempReg2;

// Registerd used in RegExpMatcher instruction (do not use JSReturnOperand).
static constexpr Register RegExpMatcherRegExpReg = CallTempReg0;
static constexpr Register RegExpMatcherStringReg = CallTempReg1;
static constexpr Register RegExpMatcherLastIndexReg = CallTempReg2;

static constexpr Register JSReturnReg_Type{Registers::a3};
static constexpr Register JSReturnReg_Data{Registers::a2};
static constexpr Register JSReturnReg{Registers::a2};

static constexpr ValueOperand ValueOperand(JSReturnReg);
static constexpr Register64 ReturnReg64(a0);

static constexpr Register ABINonArgReg0{Registers::s0};
static constexpr Register ABINonArgReg1{Registers::s1};
static constexpr Register ABINonArgReg2{Registers::s2};
static constexpr Register ABINonArgReg3{Registers::s3};
static constexpr Register ABINonArgReturnReg0{Registers::s0};
static constexpr Register ABINonArgReturnReg1{Registers::s1};
static constexpr Register ABINonVolatileReg{Registers::fp};
static constexpr Register ABINonArgReturnVolatileReg{Registers::ra};

static constexpr FloatRegister ABINonArgDoubleReg = {FloatRegisters::fs0, Double};

static constexpr Register WasmTableCallScratchReg0{Registers::invalid_reg};
static constexpr Register WasmTableCallScratchReg1{Registers::invalid_reg};
static constexpr Register WasmTableCallSigReg{Registers::invalid_reg};
static constexpr Register WasmTableCallIndexReg{Registers::invalid_reg};
static constexpr Register WasmTlsReg{Registers::invalid_reg};
static constexpr Register WasmJitEntryReturnScratch{Registers::invalid_reg};

static constexpr uint32_t ABIStackAlignment = 16;
static constexpr uint32_t CodeAlignment = 16;
static constexpr uint32_t JitStackAlignment = 8;
static constexpr uint32_t JitStackValueAlignment =
    JitStackAlignment / sizeof(Value);

static const Scale ScalePointer = TimesOne;

class Instruction;
typedef js::jit::AssemblerBuffer<1024, Instruction> RISCVBuffer;

class Assembler : public AssemblerShared {
 public:
  enum RISCVCondition : uint32_t {
    EQ = 0b000,
    NE = 0b001,
    LT = 0b100,
    GE = 0b101,
    LTU = 0b110,
    GEU = 0b111,
  };

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
    DoubleOrdered,
    DoubleEqual,
    DoubleNotEqual,
    DoubleGreaterThan,
    DoubleGreaterThanOrEqual,
    DoubleLessThan,
    DoubleLessThanOrEqual,
    DoubleUnordered,
    DoubleEqualOrUnordered,
    DoubleNotEqualOrUnordered,
    DoubleGreaterThanOrUnordered,
    DoubleGreaterThanOrEqualOrUnordered,
    DoubleLessThanOrUnordered,
    DoubleLessThanOrEqualOrUnordered
  };

  RISCVBuffer m_buffer;

  BufferOffset nextOffset() { return m_buffer.nextOffset(); }

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

  static void Bind(uint8_t* rawCode, const CodeLabel& label) { MOZ_CRASH(); }

  static uintptr_t GetPointer(uint8_t*) { MOZ_CRASH(); }

  static bool HasRoundInstruction(RoundingMode) { return false; }

  void verifyHeapAccessDisassembly(uint32_t begin, uint32_t end,
                                   const Disassembler::HeapAccess& heapAccess) {
    MOZ_CRASH();
  }

  void setUnlimitedBuffer() { MOZ_CRASH(); }

  MOZ_ALWAYS_INLINE BufferOffset writeInst(uint32_t x) {
    MOZ_ASSERT(hasCreator());
    return m_buffer.putInt(x);
  }
};

class Instruction {
 protected:
  uint32_t data;

  // Standard constructor
  Instruction(uint32_t data_) : data(data_) {}

 public:
  uint32_t encode() const { return data; }

  void setData(uint32_t data) { this->data = data; }
  Instruction* next();
  const uint32_t* raw() const { return &data; }
  uint32_t size() const { return sizeof(data); }
};

class Operand {
 public:
  enum Kind { REG };

 private
  Kind kind_ : 4;
  uint32_t reg_ : 5;
  int32_t offset_;

 public:
  explicit Operand(const Register reg)
      : kind_(REG), reg_(reg.code()), offset_(0) {}
  explicit Operand(const FloatRegister) { MOZ_CRASH(); }
  explicit Operand(const Address& adress) { MOZ_CRASH(); }
  explicit Operand(Register reg, Imm32 offset)
      : kind_(REG), reg_(reg.code()), offset_(offset.value) {}
  explicit Operand(Register reg, int32_t offset)
      : kind_(REG), reg_(reg.code()), offset_(offset) {}

  Kind kind() const { return kind_; }
};

class ABIArgGenerator {
 public:
  ABIArgGenerator()
      : intRegIndex_(0), floatRegIndex_(0), stackOffset_(0), current_() {}

  ABIArg next(MIRType);
  ABIArg& current() { return current_; }
  uint32_t stackBytesConsumedSoFar() const { return stackOffset_; }
  void increaseStackOffset(uint32_t bytes) { stackOffset_ += bytes; }

 private:
  unsigned intRegIndex_;
  unsigned floatRegIndex_;
  uint32_t stackOffset_;
  ABIArg current_;
};

}  // namespace jit
}  // namespace js

#endif /* jit_riscv64_Assembler_riscv64_h */
