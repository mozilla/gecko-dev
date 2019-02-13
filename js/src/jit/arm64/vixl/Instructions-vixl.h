// Copyright 2013, ARM Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef VIXL_A64_INSTRUCTIONS_A64_H_
#define VIXL_A64_INSTRUCTIONS_A64_H_

#include "jit/arm64/vixl/Constants-vixl.h"
#include "jit/arm64/vixl/Globals-vixl.h"
#include "jit/arm64/vixl/Utils-vixl.h"

namespace vixl {
// ISA constants. --------------------------------------------------------------

typedef uint32_t Instr;
const unsigned kInstructionSize = 4;
const unsigned kInstructionSizeLog2 = 2;
const unsigned kLiteralEntrySize = 4;
const unsigned kLiteralEntrySizeLog2 = 2;
const unsigned kMaxLoadLiteralRange = 1 * MBytes;

// This is the nominal page size (as used by the adrp instruction); the actual
// size of the memory pages allocated by the kernel is likely to differ.
const unsigned kPageSize = 4 * KBytes;
const unsigned kPageSizeLog2 = 12;

const unsigned kWRegSize = 32;
const unsigned kWRegSizeLog2 = 5;
const unsigned kWRegSizeInBytes = kWRegSize / 8;
const unsigned kWRegSizeInBytesLog2 = kWRegSizeLog2 - 3;
const unsigned kXRegSize = 64;
const unsigned kXRegSizeLog2 = 6;
const unsigned kXRegSizeInBytes = kXRegSize / 8;
const unsigned kXRegSizeInBytesLog2 = kXRegSizeLog2 - 3;
const unsigned kSRegSize = 32;
const unsigned kSRegSizeLog2 = 5;
const unsigned kSRegSizeInBytes = kSRegSize / 8;
const unsigned kSRegSizeInBytesLog2 = kSRegSizeLog2 - 3;
const unsigned kDRegSize = 64;
const unsigned kDRegSizeLog2 = 6;
const unsigned kDRegSizeInBytes = kDRegSize / 8;
const unsigned kDRegSizeInBytesLog2 = kDRegSizeLog2 - 3;
const uint64_t kWRegMask = UINT64_C(0xffffffff);
const uint64_t kXRegMask = UINT64_C(0xffffffffffffffff);
const uint64_t kSRegMask = UINT64_C(0xffffffff);
const uint64_t kDRegMask = UINT64_C(0xffffffffffffffff);
const uint64_t kSSignMask = UINT64_C(0x80000000);
const uint64_t kDSignMask = UINT64_C(0x8000000000000000);
const uint64_t kWSignMask = UINT64_C(0x80000000);
const uint64_t kXSignMask = UINT64_C(0x8000000000000000);
const uint64_t kByteMask = UINT64_C(0xff);
const uint64_t kHalfWordMask = UINT64_C(0xffff);
const uint64_t kWordMask = UINT64_C(0xffffffff);
const uint64_t kXMaxUInt = UINT64_C(0xffffffffffffffff);
const uint64_t kWMaxUInt = UINT64_C(0xffffffff);
const int64_t kXMaxInt = INT64_C(0x7fffffffffffffff);
const int64_t kXMinInt = INT64_C(0x8000000000000000);
const int32_t kWMaxInt = INT32_C(0x7fffffff);
const int32_t kWMinInt = INT32_C(0x80000000);
const unsigned kLinkRegCode = 30;
const unsigned kZeroRegCode = 31;
const unsigned kSPRegInternalCode = 63;
const unsigned kRegCodeMask = 0x1f;

const unsigned kAddressTagOffset = 56;
const unsigned kAddressTagWidth = 8;
const uint64_t kAddressTagMask =
    ((UINT64_C(1) << kAddressTagWidth) - 1) << kAddressTagOffset;
VIXL_STATIC_ASSERT(kAddressTagMask == UINT64_C(0xff00000000000000));

// AArch64 floating-point specifics. These match IEEE-754.
const unsigned kDoubleMantissaBits = 52;
const unsigned kDoubleExponentBits = 11;
const unsigned kFloatMantissaBits = 23;
const unsigned kFloatExponentBits = 8;

// Floating-point infinity values.
extern const float kFP32PositiveInfinity;
extern const float kFP32NegativeInfinity;
extern const double kFP64PositiveInfinity;
extern const double kFP64NegativeInfinity;

// The default NaN values (for FPCR.DN=1).
extern const double kFP64DefaultNaN;
extern const float kFP32DefaultNaN;


enum LSDataSize {
  LSByte        = 0,
  LSHalfword    = 1,
  LSWord        = 2,
  LSDoubleWord  = 3
};

LSDataSize CalcLSPairDataSize(LoadStorePairOp op);

enum ImmBranchType {
  UnknownBranchType = 0,
  CondBranchType    = 1,
  UncondBranchType  = 2,
  CompareBranchType = 3,
  TestBranchType    = 4
};

enum AddrMode {
  Offset,
  PreIndex,
  PostIndex
};

enum FPRounding {
  // The first four values are encodable directly by FPCR<RMode>.
  FPTieEven = 0x0,
  FPPositiveInfinity = 0x1,
  FPNegativeInfinity = 0x2,
  FPZero = 0x3,

  // The final rounding mode is only available when explicitly specified by the
  // instruction (such as with fcvta). It cannot be set in FPCR.
  FPTieAway
};

enum Reg31Mode {
  Reg31IsStackPointer,
  Reg31IsZeroRegister
};

// Instructions. ---------------------------------------------------------------

class Instruction {
 public:
  Instr InstructionBits() const {
    return *(reinterpret_cast<const Instr*>(this));
  }

  void SetInstructionBits(Instr new_instr) {
    *(reinterpret_cast<Instr*>(this)) = new_instr;
  }

  int Bit(int pos) const {
    return (InstructionBits() >> pos) & 1;
  }

  uint32_t Bits(int msb, int lsb) const {
    return unsigned_bitextract_32(msb, lsb, InstructionBits());
  }

  int32_t SignedBits(int msb, int lsb) const {
    int32_t bits = *(reinterpret_cast<const int32_t*>(this));
    return signed_bitextract_32(msb, lsb, bits);
  }

  Instr Mask(uint32_t mask) const {
    return InstructionBits() & mask;
  }

  #define DEFINE_GETTER(Name, HighBit, LowBit, Func)             \
  int64_t Name() const { return Func(HighBit, LowBit); }
  INSTRUCTION_FIELDS_LIST(DEFINE_GETTER)
  #undef DEFINE_GETTER

  // ImmPCRel is a compound field (not present in INSTRUCTION_FIELDS_LIST),
  // formed from ImmPCRelLo and ImmPCRelHi.
  int ImmPCRel() const {
    int const offset = ((ImmPCRelHi() << ImmPCRelLo_width) | ImmPCRelLo());
    int const width = ImmPCRelLo_width + ImmPCRelHi_width;
    return signed_bitextract_32(width-1, 0, offset);
  }

  uint64_t ImmLogical() const;
  float ImmFP32() const;
  double ImmFP64() const;

  LSDataSize SizeLSPair() const {
    return CalcLSPairDataSize(
             static_cast<LoadStorePairOp>(Mask(LoadStorePairMask)));
  }

  // Helpers.
  bool IsCondBranchImm() const {
    return Mask(ConditionalBranchFMask) == ConditionalBranchFixed;
  }

  bool IsUncondBranchImm() const {
    return Mask(UnconditionalBranchFMask) == UnconditionalBranchFixed;
  }

  bool IsCompareBranch() const {
    return Mask(CompareBranchFMask) == CompareBranchFixed;
  }

  bool IsTestBranch() const {
    return Mask(TestBranchFMask) == TestBranchFixed;
  }

  bool IsPCRelAddressing() const {
    return Mask(PCRelAddressingFMask) == PCRelAddressingFixed;
  }

  bool IsLogicalImmediate() const {
    return Mask(LogicalImmediateFMask) == LogicalImmediateFixed;
  }

  bool IsAddSubImmediate() const {
    return Mask(AddSubImmediateFMask) == AddSubImmediateFixed;
  }

  bool IsAddSubExtended() const {
    return Mask(AddSubExtendedFMask) == AddSubExtendedFixed;
  }

  bool IsLoadOrStore() const {
    return Mask(LoadStoreAnyFMask) == LoadStoreAnyFixed;
  }

  bool IsLoad() const;
  bool IsStore() const;

  bool IsLoadLiteral() const {
    // This includes PRFM_lit.
    return Mask(LoadLiteralFMask) == LoadLiteralFixed;
  }

  bool IsMovn() const {
    return (Mask(MoveWideImmediateMask) == MOVN_x) ||
           (Mask(MoveWideImmediateMask) == MOVN_w);
  }

  // Mozilla modifications.
  bool IsUncondB() const;
  bool IsCondB() const;
  bool IsBL() const;
  bool IsBR() const;
  bool IsBLR() const;
  bool IsTBZ() const;
  bool IsTBNZ() const;
  bool IsCBZ() const;
  bool IsCBNZ() const;
  bool IsLDR() const;
  bool IsADR() const;
  bool IsADRP() const;
  bool IsBranchLinkImm() const;
  bool IsTargetReachable(Instruction* target) const;
  ptrdiff_t ImmPCRawOffset() const;
  void SetBits32(int msb, int lsb, unsigned value);

#define DEFINE_SETTERS(Name, HighBit, LowBit, Func)  \
  inline void Set##Name(unsigned n) { SetBits32(HighBit, LowBit, n); }
INSTRUCTION_FIELDS_LIST(DEFINE_SETTERS)
#undef DEFINE_SETTERS

  // Indicate whether Rd can be the stack pointer or the zero register. This
  // does not check that the instruction actually has an Rd field.
  Reg31Mode RdMode() const {
    // The following instructions use sp or wsp as Rd:
    //  Add/sub (immediate) when not setting the flags.
    //  Add/sub (extended) when not setting the flags.
    //  Logical (immediate) when not setting the flags.
    // Otherwise, r31 is the zero register.
    if (IsAddSubImmediate() || IsAddSubExtended()) {
      if (Mask(AddSubSetFlagsBit)) {
        return Reg31IsZeroRegister;
      } else {
        return Reg31IsStackPointer;
      }
    }
    if (IsLogicalImmediate()) {
      // Of the logical (immediate) instructions, only ANDS (and its aliases)
      // can set the flags. The others can all write into sp.
      // Note that some logical operations are not available to
      // immediate-operand instructions, so we have to combine two masks here.
      if (Mask(LogicalImmediateMask & LogicalOpMask) == ANDS) {
        return Reg31IsZeroRegister;
      } else {
        return Reg31IsStackPointer;
      }
    }
    return Reg31IsZeroRegister;
  }

  // Indicate whether Rn can be the stack pointer or the zero register. This
  // does not check that the instruction actually has an Rn field.
  Reg31Mode RnMode() const {
    // The following instructions use sp or wsp as Rn:
    //  All loads and stores.
    //  Add/sub (immediate).
    //  Add/sub (extended).
    // Otherwise, r31 is the zero register.
    if (IsLoadOrStore() || IsAddSubImmediate() || IsAddSubExtended()) {
      return Reg31IsStackPointer;
    }
    return Reg31IsZeroRegister;
  }

  ImmBranchType BranchType() const {
    if (IsCondBranchImm()) {
      return CondBranchType;
    } else if (IsUncondBranchImm()) {
      return UncondBranchType;
    } else if (IsCompareBranch()) {
      return CompareBranchType;
    } else if (IsTestBranch()) {
      return TestBranchType;
    } else {
      return UnknownBranchType;
    }
  }

  // Find the target of this instruction. 'this' may be a branch or a
  // PC-relative addressing instruction.
  const Instruction* ImmPCOffsetTarget() const;

  // Patch a PC-relative offset to refer to 'target'. 'this' may be a branch or
  // a PC-relative addressing instruction.
  void SetImmPCOffsetTarget(const Instruction* target);
  // Patch a literal load instruction to load from 'source'.
  void SetImmLLiteral(const Instruction* source);

  // Calculate the address of a literal referred to by a load-literal
  // instruction, and return it as the specified type.
  //
  // The literal itself is safely mutable only if the backing buffer is safely
  // mutable.
  template <typename T>
  T LiteralAddress() const {
    uint64_t base_raw = reinterpret_cast<uintptr_t>(this);
    ptrdiff_t offset = ImmLLiteral() << kLiteralEntrySizeLog2;
    uint64_t address_raw = base_raw + offset;

    // Cast the address using a C-style cast. A reinterpret_cast would be
    // appropriate, but it can't cast one integral type to another.
    T address = (T)(address_raw);

    // Assert that the address can be represented by the specified type.
    VIXL_ASSERT((uint64_t)(address) == address_raw);

    return address;
  }

  uint32_t Literal32() const {
    uint32_t literal;
    memcpy(&literal, LiteralAddress<const void*>(), sizeof(literal));
    return literal;
  }

  uint64_t Literal64() const {
    uint64_t literal;
    memcpy(&literal, LiteralAddress<const void*>(), sizeof(literal));
    return literal;
  }

  float LiteralFP32() const {
    return rawbits_to_float(Literal32());
  }

  double LiteralFP64() const {
    return rawbits_to_double(Literal64());
  }

  const Instruction* NextInstruction() const {
    return this + kInstructionSize;
  }

  const Instruction* InstructionAtOffset(int64_t offset) const {
    VIXL_ASSERT(IsWordAligned(this + offset));
    return this + offset;
  }

  template<typename T> static Instruction* Cast(T src) {
    return reinterpret_cast<Instruction*>(src);
  }

  template<typename T> static const Instruction* CastConst(T src) {
    return reinterpret_cast<const Instruction*>(src);
  }

 private:
  int ImmBranch() const;

  void SetPCRelImmTarget(const Instruction* target);
  void SetBranchImmTarget(const Instruction* target);
};
}  // namespace vixl

#endif  // VIXL_A64_INSTRUCTIONS_A64_H_
