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
    4;  // Make it 4 to avoid a bunch of div-by-zero warnings
static const uint32_t WasmStackAlignment = 8;
static const uint32_t WasmTrapInstructionLength = 0;

// See comments in wasm::GenerateFunctionPrologue.
static constexpr uint32_t WasmCheckedCallEntryOffset = 0u;
static constexpr uint32_t WasmCheckedTailEntryOffset = 1u;

class Registers {
 public:
  enum RegisterID {
    x0 = 0,
    zero = 0,
    x1 = 1,
    ra = 1,
    x2 = 2,
    sp = 2,
    x3 = 3,
    gp = 3,
    x4 = 4,
    tp = 4,
    x5 = 5,
    t0 = 5,
    x6 = 6,
    t1 = 6,
    x7 = 7,
    t2 = 7,
    x8 = 8,
    fp = 8,
    s0 = 8,
    x9 = 9,
    s1 = 9,
    x10 = 10,
    a0 = 10,
    x11 = 11,
    a1 = 11,
    x12 = 12,
    a2 = 12,
    x13 = 13,
    a3 = 13,
    x14 = 14,
    a4 = 14,
    x15 = 15,
    a5 = 15,
    x16 = 16,
    a6 = 16,
    x17 = 17,
    a7 = 17,
    x18 = 18,
    s2 = 18,
    x19 = 19,
    s3 = 19,
    x20 = 20,
    s4 = 20,
    x21 = 21,
    s5 = 21,
    x22 = 22,
    s6 = 22,
    x23 = 23,
    s7 = 23,
    x24 = 24,
    s8 = 24,
    x25 = 25,
    s9 = 25,
    x26 = 26,
    s10 = 26,
    x27 = 27,
    s11 = 27,
    x28 = 28,
    t3 = 28,
    x29 = 29,
    t4 = 29,
    x30 = 30,
    t5 = 30,
    x31 = 31,
    t6 = 31,
    invalid_reg
  };
  typedef uint8_t Code;
  typedef RegisterID Encoding;
  union RegisterContent {
    uintptr_t r;
  };

  typedef uint32_t SetType;

  static uint32_t SetSize(SetType x) {
    static_assert(sizeof(SetType) == 4, "SetType must be 32 bits");
    return mozilla::CountPopulation32(x);
  }
  static uint32_t FirstBit(SetType) {
    return mozilla::CountTrailingZeroes32(x);
  }
  static uint32_t LastBit(SetType) {
    return 31 - mozilla::CountLeadingZeroes32(x);
  }

  static const char* GetName(uint32_t code) {
    // clang-format off
    static const char* const Names[] = {
        "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
        "fp",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
        "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
        "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    // clang-format on
    static_assert(Total == sizeof(Names) / sizeof(Names[0]),
                  "Table is the correct size");
    if (code >= Total) {
      return "invalid";
    }
    return Names[code];
  }

  static Code FromName(const char* name) {
    for (size_t i = 0; i < Total; i++) {
      if (strcmp(GetName(Code(i)), name) == 0) {
        return Code(i);
      }
    }
    return Invalid;
  }

  static const Encoding StackPointer = sp;
  static const Encoding Invalid = invalid_reg;
  static const uint32_t Total = 32;
  static const uint32_t TotalPhys = 32;
  static const uint32_t Allocatable = 28;
  static const SetType AllMask = 0xffffffff;
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
  static const SetType NonVolatileMask =
      (1 << Registers::s0) | (1 << Registers::s1) | (1 << Registers::s2) |
      (1 << Registers::s3) | (1 << Registers::s4) | (1 << Registers::s5) |
      (1 << Registers::s6) | (1 << Registers::s7) | (1 << Registers::s8) |
      (1 << Registers::s9) | (1 << Registers::s10) | (1 << Registers::s11);
  static const SetType NonAllocatableMask =
      (1 << Registers::zero) | (1 << Registers::sp) | (1 << Registers::tp) |
      (1 << Registers::gp);
  static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;
  static const SetType JSCallMask = 0;
  static const SetType CallMask = 0;
};

typedef uint8_t PackedRegisterMask;

class FloatRegisters {
 public:
  enum FPRegisterID {
    f0 = 0,
    ft0 = 0,
    f1 = 1,
    ft1 = 1,
    f2 = 2,
    ft2 = 2,
    f3 = 3,
    ft3 = 3,
    f4 = 4,
    ft4 = 4,
    f5 = 5,
    ft5 = 5,
    f6 = 6,
    ft6 = 6,
    f7 = 7,
    ft7 = 7,
    f8 = 8,
    fs0 = 8,
    f9 = 9,
    fs1 = 9,
    f10 = 10,
    fa0 = 10,
    f11 = 11,
    fa1 = 11,
    f12 = 12,
    fa2 = 12,
    f13 = 13,
    fa3 = 13,
    f14 = 14,
    fa4 = 14,
    f15 = 15,
    fa5 = 15,
    f16 = 16,
    fa6 = 16,
    f17 = 17,
    fa7 = 17,
    f18 = 18,
    fs2 = 18,
    f19 = 19,
    fs3 = 19,
    f20 = 20,
    fs4 = 20,
    f21 = 21,
    fs5 = 21,
    f22 = 22,
    fs6 = 22,
    f23 = 23,
    fs7 = 23,
    f24 = 24,
    fs8 = 24,
    f25 = 25,
    fs9 = 25,
    f26 = 26,
    fs10 = 26,
    f27 = 27,
    fs11 = 27,
    f28 = 28,
    ft8 = 28,
    f29 = 29,
    ft9 = 29,
    f30 = 30,
    ft10 = 30,
    f31 = 31,
    ft11 = 31,
    invalid_reg
  };
  typedef FPRegisterID Code;
  typedef FPRegisterID Encoding;
  union RegisterContent {
    float s;
    double d;
  };

  typedef uint32_t SetType;

  static const char* GetName(uint32_t code) {
    // clang-format off
    static const char* const Names[] = {
        "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
        "fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5",
        "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
        "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11",
    };
    // clang-format on
    static_assert(Total == sizeof(Names) / sizeof(Names[0]),
                  "Table is the correct size");
    if (code >= Total) {
      return "invalid";
    }
    return Names[code];
  }
  static Code FromName(const char*) {
    for (size_t i = 0; i < Total; i++) {
      if (strcmp(GetName(Code(i)), name) == 0) {
        return Code(i);
      }
    }
    return Invalid;
  }

  static const Code Invalid = invalid_reg;
  static const uint32_t Total = 32;
  static const uint32_t TotalPhys = 32;
  static const uint32_t Allocatable = 32;
  static const SetType AllMask = 0xffffffff;
  static const SetType AllDoubleMask = 0;
  static const SetType AllSingleMask = 0;
  static const SetType VolatileMask =
      (1 << FloatRegisters::ft0) | (1 << FloatRegisters::ft1) |
      (1 << FloatRegisters::ft2) | (1 << FloatRegisters::ft3) |
      (1 << FloastRegisters::ft4) | (1 << FloatRegisters::ft5) |
      (1 << FloatRegisters::ft6) | (1 << FloatRegisters::ft7) |
      (1 << FloatRegisters::ft8) | (1 << FloatRegisters::ft9) |
      (1 << FloatRegisters::ft10) | (1 << FloatRegisters::ft11) |
      (1 << FloatRegisters::fa0) | (1 << FloatRegisters::fa1) |
      (1 << FloatRegisters::fa2) | (1 << FloatRegisters::fa3) |
      (1 << FloatRegisters::fa4) | (1 << FloatRegisters::fa5) |
      (1 << FloatRegisters::fa6) | (1 << FloatRegisters::fa7);
  static const SetType NonVolatileMask =
      (1 << FloatRegisters::fs0) | (1 << FloatRegisters::fs1) |
      (1 << FloatRegisters::fs2) | (1 << FloatRegisters::fs3) |
      (1 << FloatRegisters::fs4) | (1 << FloatRegisters::fs5) |
      (1 << FloatRegisters::fs6) | (1 << FloatRegisters::fs7) |
      (1 << FloatRegisters::fs8) | (1 << FloatRegisters::fs9) |
      (1 << FloatRegisters::fs10) | (1 << FloatRegisters::fs11);
  static const SetType NonAllocatableMask = 0;
  static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;
};

template <typename T>
class TypedRegisterSet;

struct FloatRegister {
  typedef FloatRegisters Codes;
  typedef Codes::Code Code;
  typedef Codes::Encoding Encoding;
  typedef Codes::SetType SetType;

  enum RegType { Single, Double };

  Code code_ : 5;
  uint32_t kind_ : 2;

  FloatRegister(uint32_t r) : code_(r), kind_(Double) {}
  FloatRegister(uint32_t r, RegType k) : code_(r), kind_(k) {}

  static uint32_t FirstBit(SetType) { MOZ_CRASH(); }
  static uint32_t LastBit(SetType) { MOZ_CRASH(); }
  static FloatRegister FromCode(uint32_t) { MOZ_CRASH(); }
  bool isSingle() const { return kind_ == Single; }
  bool isDouble() const { return kind_ == Double; }
  bool isSimd128() const { return false; }
  bool isInvalid() const { MOZ_CRASH(); }
  FloatRegister asSingle() const { MOZ_CRASH(); }
  FloatRegister asDouble() const { MOZ_CRASH(); }
  FloatRegister asSimd128() const { MOZ_CRASH(); }
  Code code() const { return code_; }
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

}  // namespace jit
}  // namespace js

#endif /* jit_riscv64_Architecture_riscv64_h */
