/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_riscv64_Architecture_riscv64_h
#define jit_riscv64_Architecture_riscv64_h

// JitSpewer.h is included through MacroAssembler implementations for other
// platforms, so include it here to avoid inadvertent build bustage.
#include "jit/JitSpewer.h"

#include "jit/shared/Architecture-shared.h"

namespace js {
namespace jit {

static const uint32_t SimdMemoryAlignment =
    16;  // Make it 4 to avoid a bunch of div-by-zero warnings
static const uint32_t WasmStackAlignment = 16;
static const uint32_t WasmTrapInstructionLength = 4;

// See comments in wasm::GenerateFunctionPrologue.
static constexpr uint32_t WasmCheckedCallEntryOffset = 0u;
static constexpr uint32_t WasmCheckedTailEntryOffset = 1u;

// RISCV64 has 32 64-bit integer registers, x0 though x31.
//  The program counter is not accessible as a register.

// RISCV INT Register Convention:
// Name          Alias          Usage
// x0            zero           hardwired to 0, ignores writes
// x1            ra             return address for jumps 
// x2            sp             stack pointer
// x3            gp             global pointer
// x4            tp             thread pointer
// x5-x7         t0-t2          temporary register 0 
// x8            fp/s0          saved register 0 or frame pointer
// x9            s1             saved register 1
// x10-x11       a0-a1          return value or function argument
// x12-x17       a2-a7          function argument 2
// x18-x27       s2-s11         saved register
// x28-x31       t3-t6          temporary register 3

// RISCV-64 FP Register Convention:
//  Name         Alias           Usage
//  $f0-$f7      $ft0-$ft7       Temporary registers
//  $f8-$f9      $fs0-$fs1       Callee-saved registers
//  $f10-$f11    $fa0-$fa1       Return values
//  $f12-$f17    $fa2-$fa7       Args values
//  $f18-$f27    $fs2-$fs11      Callee-saved registers
//  $f28-$f31    $ft8-$ft11      Temporary registers
class Registers {
 public:
  enum RegisterID {
    x0 = 0,
    x1,
    x2,
    x3,
    x4,
    x5,
    x6,
    x7,
    x8,
    x9,
    x10,
    x11,
    x12,
    x13,
    x14,
    x15,
    x16,
    x17,
    x18,
    x19,
    x20,
    x21,
    x22,
    x23,
    x24,
    x25,
    x26,
    x27,
    x28,
    x29,
    x30,
    x31,
    zero = x0,
    ra = x1,
    sp = x2,
    gp = x3,
    tp = x4,
    t0 = x5,
    t1 = x6,
    t2 = x7,
    fp = x8,
    s1 = x9,
    a0 = x10,
    a1 = x11,
    a2 = x12,
    a3 = x13,
    a4 = x14,
    a5 = x15,
    a6 = x16,
    a7 = x17,
    s2 = x18,
    s3 = x19,
    s4 = x20,
    s5 = x21,
    s6 = x22,
    s7 = x23,
    s8 = x24,
    s9 = x25,
    s10 = x26,
    s11 = x27,
    t3 = x28,
    t4 = x29,
    t5 = x30,
    t6 = x31,
    invalid_reg,
  };
  typedef uint8_t Code;
  typedef RegisterID Encoding;
  union RegisterContent {
    uintptr_t r;
  };

  typedef uint32_t SetType;

  static uint32_t SetSize(SetType) { MOZ_CRASH(); }
  static uint32_t FirstBit(SetType) { MOZ_CRASH(); }
  static uint32_t LastBit(SetType) { MOZ_CRASH(); }
  static const char* GetName(uint32_t code) {
    static const char* const Names[] = {
        "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "fp", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    static_assert(Total == std::size(Names), "Table is the correct size");
    if (code >= Total) {
      return "invalid";
    }
    return Names[code];
  }

  static Code FromName(const char*);

  static const Encoding StackPointer = sp;
  static const Encoding Invalid = invalid_reg;
  static const uint32_t Total = 32;
  static const uint32_t TotalPhys = 32;
  static const uint32_t Allocatable = 24;
  static const SetType NoneMask = 0x0;
  static const SetType AllMask = 0xFFFFFFFF;
  static const SetType ArgRegMask =
      (1 << Registers::a0) | (1 << Registers::a1) | (1 << Registers::a2) |
      (1 << Registers::a3) | (1 << Registers::a4) | (1 << Registers::a5) |
      (1 << Registers::a6) | (1 << Registers::a7);

  static const SetType VolatileMask =
      (1 << Registers::a0) | (1 << Registers::a1) | (1 << Registers::a2) |
      (1 << Registers::a3) | (1 << Registers::a4) | (1 << Registers::a5) |
      (1 << Registers::a6) | (1 << Registers::a7) | (1 << Registers::t0) |
      (1 << Registers::t1) | (1 << Registers::t2) | (1 << Registers::t3) |
      (1 << Registers::t4) | (1 << Registers::t5) | (1 << Registers::t6);

  // We use this constant to save registers when entering functions. This
  // is why $ra is added here even though it is not "Non Volatile".
  static const SetType NonVolatileMask =
      (1 << Registers::ra) | (1 << Registers::fp) | (1 << Registers::s1) |
      (1 << Registers::s2) | (1 << Registers::s3) | (1 << Registers::s4) |
      (1 << Registers::s5) | (1 << Registers::s6) | (1 << Registers::s7) |
      (1 << Registers::s8);

  static const SetType SingleByteRegs = VolatileMask | NonVolatileMask;

  static const SetType NonAllocatableMask =
      (1 << Registers::zero) |  // Always be zero.
      (1 << Registers::t5) |    // Scratch reg
      (1 << Registers::t6) |    // Scratch reg
      (1 << Registers::ra) | (1 << Registers::tp) | (1 << Registers::sp) |
      (1 << Registers::fp);

  static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;

  // Registers returned from a JS -> JS call.
  static const SetType JSCallMask = (1 << Registers::a2);

  // Registers returned from a JS -> C call.
  static const SetType CallMask = (1 << Registers::a0);

  static const SetType WrapperMask = VolatileMask;
};

typedef uint8_t PackedRegisterMask;

class FloatRegisters {
 public:
  enum FPRegisterID {
    f0 = 0,
    f1,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12,
    f13,
    f14,
    f15,
    f16,
    f17,
    f18,
    f19,
    f20,
    f21,
    f22,
    f23,  // Scratch register.
    f24,
    f25,
    f26,
    f27,
    f28,
    f29,
    f30,
    f31,
    invalid_reg,
    ft0 = f0,
    ft1 = f1,
    ft2 = f2,
    ft3 = f3,
    ft4 = f4,
    ft5 = f5,
    ft6 = f6,
    ft7 = f7,
    fs0 = f8,
    fs1 = f9,
    fa0 = f10,
    fa1 = f11,
    fa2 = f12,
    fa3 = f13,
    fa4 = f14,
    fa5 = f15,
    fa6 = f16,
    fa7 = f17,
    fs2 = f18,
    fs3 = f19,
    fs4 = f20,
    fs5 = f21,
    fs6 = f22,
    fs7 = f23,
    fs8 = f24,
    fs9 = f25,
    fs10 = f26,
    fs11 = f27,
    ft8 = f28,
    ft9 = f29,
    ft10 = f30,
    ft11 = f31
  };

  enum Kind : uint8_t { Double, Single, NumTypes };

  typedef FPRegisterID Code;
  typedef FPRegisterID Encoding;
  union RegisterContent {
    float s;
    double d;
  };

  static const char* GetName(uint32_t code) {
    static const char* const Names[] = {
        "ft0",  "ft1",  "ft2",  "ft3",  "ft4",  "ft5",  "ft6",  "ft7",
        "fs0",  "fs2",  "fa0", "fa1", "fa2", "fa3", "fa4", "fa5",
        "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
        "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"};
    static_assert(TotalPhys == std::size(Names), "Table is the correct size");
    if (code >= Total) {
      return "invalid";
    }
    return Names[code];
  }

  static Code FromName(const char* name);

  typedef uint32_t SetType;

  static const Code Invalid = invalid_reg;
  static const uint32_t Total = 32;
  static const uint32_t TotalPhys = 32;
  static const uint32_t Allocatable = 23;
  static const SetType AllMask = 0xFFFFFFFF;
  static const SetType AllDoubleMask = AllMask;
  static const SetType AllSingleMask = AllMask;
  static const SetType NonVolatileMask =
      SetType((1 << FloatRegisters::fs0) | (1 << FloatRegisters::fs1) |
              (1 << FloatRegisters::fs2) | (1 << FloatRegisters::fs3) |
              (1 << FloatRegisters::fs4) | (1 << FloatRegisters::fs5) |
              (1 << FloatRegisters::fs6) | (1 << FloatRegisters::fs7) |
              (1 << FloatRegisters::fs8) | (1 << FloatRegisters::fs9) |
              (1 << FloatRegisters::fs10) | (1 << FloatRegisters::fs11));
  static const SetType VolatileMask = AllMask & ~NonVolatileMask;
  
  static const SetType NonAllocatableMask = 
        SetType((1 << FloatRegisters::ft10) | (1 << FloatRegisters::ft11));

  static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;
};

template <typename T>
class TypedRegisterSet;

struct FloatRegister {
 public:

  typedef FloatRegisters Codes;
  typedef Codes::Code Code;
  typedef Codes::Encoding Encoding;
  typedef Codes::SetType SetType;

  Code _;

  static uint32_t FirstBit(SetType) { MOZ_CRASH(); }
  static uint32_t LastBit(SetType) { MOZ_CRASH(); }
  static FloatRegister FromCode(uint32_t) { MOZ_CRASH(); }
  bool isSingle() const { MOZ_CRASH(); }
  bool isDouble() const { MOZ_CRASH(); }
  bool isSimd128() const { MOZ_CRASH(); }
  bool isInvalid() const { MOZ_CRASH(); }
  FloatRegister asSingle() const { MOZ_CRASH(); }
  FloatRegister asDouble() const { MOZ_CRASH(); }
  FloatRegister asSimd128() const { MOZ_CRASH(); }
  Code code() const { MOZ_CRASH(); }
  Encoding encoding() const { MOZ_CRASH(); }
  const char* name() const { MOZ_CRASH(); }
  bool volatile_() const { MOZ_CRASH(); }
  bool operator!=(FloatRegister) const { MOZ_CRASH(); }
  bool operator==(FloatRegister) const { MOZ_CRASH(); }
  bool aliases(FloatRegister) const { MOZ_CRASH(); }
  uint32_t numAliased() const { MOZ_CRASH(); }
  FloatRegister aliased(uint32_t) { MOZ_CRASH(); }
  bool equiv(FloatRegister) const { MOZ_CRASH(); }
  uint32_t size() const { MOZ_CRASH(); }
  uint32_t numAlignedAliased() const { MOZ_CRASH(); }
  FloatRegister alignedAliased(uint32_t) { MOZ_CRASH(); }
  SetType alignedOrDominatedAliasedSet() const { MOZ_CRASH(); }

  static constexpr RegTypeName DefaultType = RegTypeName::Float64;

  template <RegTypeName = DefaultType>
  static SetType LiveAsIndexableSet(SetType s) {
    return SetType(0);
  }

  template <RegTypeName Name = DefaultType>
  static SetType AllocatableAsIndexableSet(SetType s) {
    static_assert(Name != RegTypeName::Any, "Allocatable set are not iterable");
    return SetType(0);
  }

  template <typename T>
  static T ReduceSetForPush(T) {
    MOZ_CRASH();
  }
  uint32_t getRegisterDumpOffsetInBytes() { MOZ_CRASH(); }
  static uint32_t SetSize(SetType x) { MOZ_CRASH(); }
  static Code FromName(const char* name) { MOZ_CRASH(); }

  // This is used in static initializers, so produce a bogus value instead of
  // crashing.
  static uint32_t GetPushSizeInBytes(const TypedRegisterSet<FloatRegister>&) {
    return 0;
  }
};

inline bool hasUnaliasedDouble() { MOZ_CRASH(); }
inline bool hasMultiAlias() { MOZ_CRASH(); }

static const uint32_t ShadowStackSpace = 0;
static const uint32_t JumpImmediateRange = INT32_MAX;

#ifdef JS_NUNBOX32
static const int32_t NUNBOX32_TYPE_OFFSET = 4;
static const int32_t NUNBOX32_PAYLOAD_OFFSET = 0;
#endif

inline uint32_t GetRISCV64Flags() { MOZ_CRASH(); }

}  // namespace jit
}  // namespace js

#endif /* jit_riscv64_Architecture_riscv64_h */
