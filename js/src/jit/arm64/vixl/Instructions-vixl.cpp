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

#include "jit/arm64/vixl/Instructions-vixl.h"
#include "jit/arm64/vixl/Assembler-vixl.h"

namespace vixl {


// Floating-point infinity values.
const float kFP32PositiveInfinity = rawbits_to_float(0x7f800000);
const float kFP32NegativeInfinity = rawbits_to_float(0xff800000);
const double kFP64PositiveInfinity =
    rawbits_to_double(UINT64_C(0x7ff0000000000000));
const double kFP64NegativeInfinity =
    rawbits_to_double(UINT64_C(0xfff0000000000000));


// The default NaN values (for FPCR.DN=1).
const double kFP64DefaultNaN = rawbits_to_double(UINT64_C(0x7ff8000000000000));
const float kFP32DefaultNaN = rawbits_to_float(0x7fc00000);


static uint64_t RotateRight(uint64_t value,
                            unsigned int rotate,
                            unsigned int width) {
  VIXL_ASSERT(width <= 64);
  rotate &= 63;
  return ((value & ((UINT64_C(1) << rotate) - 1)) <<
          (width - rotate)) | (value >> rotate);
}


static uint64_t RepeatBitsAcrossReg(unsigned reg_size,
                                    uint64_t value,
                                    unsigned width) {
  VIXL_ASSERT((width == 2) || (width == 4) || (width == 8) || (width == 16) ||
              (width == 32));
  VIXL_ASSERT((reg_size == kWRegSize) || (reg_size == kXRegSize));
  uint64_t result = value & ((UINT64_C(1) << width) - 1);
  for (unsigned i = width; i < reg_size; i *= 2) {
    result |= (result << i);
  }
  return result;
}


bool Instruction::IsLoad() const {
  if (Mask(LoadStoreAnyFMask) != LoadStoreAnyFixed) {
    return false;
  }

  if (Mask(LoadStorePairAnyFMask) == LoadStorePairAnyFixed) {
    return Mask(LoadStorePairLBit) != 0;
  } else {
    LoadStoreOp op = static_cast<LoadStoreOp>(Mask(LoadStoreOpMask));
    switch (op) {
      case LDRB_w:
      case LDRH_w:
      case LDR_w:
      case LDR_x:
      case LDRSB_w:
      case LDRSB_x:
      case LDRSH_w:
      case LDRSH_x:
      case LDRSW_x:
      case LDR_s:
      case LDR_d: return true;
      default: return false;
    }
  }
}


bool Instruction::IsStore() const {
  if (Mask(LoadStoreAnyFMask) != LoadStoreAnyFixed) {
    return false;
  }

  if (Mask(LoadStorePairAnyFMask) == LoadStorePairAnyFixed) {
    return Mask(LoadStorePairLBit) == 0;
  } else {
    LoadStoreOp op = static_cast<LoadStoreOp>(Mask(LoadStoreOpMask));
    switch (op) {
      case STRB_w:
      case STRH_w:
      case STR_w:
      case STR_x:
      case STR_s:
      case STR_d: return true;
      default: return false;
    }
  }
}


// Logical immediates can't encode zero, so a return value of zero is used to
// indicate a failure case. Specifically, where the constraints on imm_s are
// not met.
uint64_t Instruction::ImmLogical() const {
  unsigned reg_size = SixtyFourBits() ? kXRegSize : kWRegSize;
  int64_t n = BitN();
  int64_t imm_s = ImmSetBits();
  int64_t imm_r = ImmRotate();

  // An integer is constructed from the n, imm_s and imm_r bits according to
  // the following table:
  //
  //  N   imms    immr    size        S             R
  //  1  ssssss  rrrrrr    64    UInt(ssssss)  UInt(rrrrrr)
  //  0  0sssss  xrrrrr    32    UInt(sssss)   UInt(rrrrr)
  //  0  10ssss  xxrrrr    16    UInt(ssss)    UInt(rrrr)
  //  0  110sss  xxxrrr     8    UInt(sss)     UInt(rrr)
  //  0  1110ss  xxxxrr     4    UInt(ss)      UInt(rr)
  //  0  11110s  xxxxxr     2    UInt(s)       UInt(r)
  // (s bits must not be all set)
  //
  // A pattern is constructed of size bits, where the least significant S+1
  // bits are set. The pattern is rotated right by R, and repeated across a
  // 32 or 64-bit value, depending on destination register width.
  //

  if (n == 1) {
    if (imm_s == 0x3F) {
      return 0;
    }
    uint64_t bits = (UINT64_C(1) << (imm_s + 1)) - 1;
    return RotateRight(bits, imm_r, 64);
  } else {
    if ((imm_s >> 1) == 0x1F) {
      return 0;
    }
    for (int width = 0x20; width >= 0x2; width >>= 1) {
      if ((imm_s & width) == 0) {
        int mask = width - 1;
        if ((imm_s & mask) == mask) {
          return 0;
        }
        uint64_t bits = (UINT64_C(1) << ((imm_s & mask) + 1)) - 1;
        return RepeatBitsAcrossReg(reg_size,
                                   RotateRight(bits, imm_r & mask, width),
                                   width);
      }
    }
  }
  VIXL_UNREACHABLE();
  return 0;
}


float Instruction::ImmFP32() const {
  //  ImmFP: abcdefgh (8 bits)
  // Single: aBbb.bbbc.defg.h000.0000.0000.0000.0000 (32 bits)
  // where B is b ^ 1
  uint32_t bits = ImmFP();
  uint32_t bit7 = (bits >> 7) & 0x1;
  uint32_t bit6 = (bits >> 6) & 0x1;
  uint32_t bit5_to_0 = bits & 0x3f;
  uint32_t result = (bit7 << 31) | ((32 - bit6) << 25) | (bit5_to_0 << 19);

  return rawbits_to_float(result);
}


double Instruction::ImmFP64() const {
  //  ImmFP: abcdefgh (8 bits)
  // Double: aBbb.bbbb.bbcd.efgh.0000.0000.0000.0000
  //         0000.0000.0000.0000.0000.0000.0000.0000 (64 bits)
  // where B is b ^ 1
  uint32_t bits = ImmFP();
  uint64_t bit7 = (bits >> 7) & 0x1;
  uint64_t bit6 = (bits >> 6) & 0x1;
  uint64_t bit5_to_0 = bits & 0x3f;
  uint64_t result = (bit7 << 63) | ((256 - bit6) << 54) | (bit5_to_0 << 48);

  return rawbits_to_double(result);
}


LSDataSize CalcLSPairDataSize(LoadStorePairOp op) {
  switch (op) {
    case STP_x:
    case LDP_x:
    case STP_d:
    case LDP_d: return LSDoubleWord;
    default: return LSWord;
  }
}


const Instruction* Instruction::ImmPCOffsetTarget() const {
  const Instruction * base = this;
  ptrdiff_t offset;
  if (IsPCRelAddressing()) {
    // ADR and ADRP.
    offset = ImmPCRel();
    if (Mask(PCRelAddressingMask) == ADRP) {
      base = AlignDown(base, kPageSize);
      offset *= kPageSize;
    } else {
      VIXL_ASSERT(Mask(PCRelAddressingMask) == ADR);
    }
  } else {
    // All PC-relative branches.
    VIXL_ASSERT(BranchType() != UnknownBranchType);
    // Relative branch offsets are instruction-size-aligned.
    offset = ImmBranch() << kInstructionSizeLog2;
  }
  return base + offset;
}


inline int Instruction::ImmBranch() const {
  switch (BranchType()) {
    case CondBranchType: return ImmCondBranch();
    case UncondBranchType: return ImmUncondBranch();
    case CompareBranchType: return ImmCmpBranch();
    case TestBranchType: return ImmTestBranch();
    default: VIXL_UNREACHABLE();
  }
  return 0;
}


void Instruction::SetImmPCOffsetTarget(const Instruction* target) {
  if (IsPCRelAddressing()) {
    SetPCRelImmTarget(target);
  } else {
    SetBranchImmTarget(target);
  }
}


void Instruction::SetPCRelImmTarget(const Instruction* target) {
  int32_t imm21;
  if ((Mask(PCRelAddressingMask) == ADR)) {
    imm21 = target - this;
  } else {
    VIXL_ASSERT(Mask(PCRelAddressingMask) == ADRP);
    uintptr_t this_page = reinterpret_cast<uintptr_t>(this) / kPageSize;
    uintptr_t target_page = reinterpret_cast<uintptr_t>(target) / kPageSize;
    imm21 = target_page - this_page;
  }
  Instr imm = vixl::Assembler::ImmPCRelAddress(imm21);

  SetInstructionBits(Mask(~ImmPCRel_mask) | imm);
}


void Instruction::SetBranchImmTarget(const Instruction* target) {
  VIXL_ASSERT(((target - this) & 3) == 0);
  Instr branch_imm = 0;
  uint32_t imm_mask = 0;
  int offset = (target - this) >> kInstructionSizeLog2;
  switch (BranchType()) {
    case CondBranchType: {
      branch_imm = vixl::Assembler::ImmCondBranch(offset);
      imm_mask = ImmCondBranch_mask;
      break;
    }
    case UncondBranchType: {
      branch_imm = vixl::Assembler::ImmUncondBranch(offset);
      imm_mask = ImmUncondBranch_mask;
      break;
    }
    case CompareBranchType: {
      branch_imm = vixl::Assembler::ImmCmpBranch(offset);
      imm_mask = ImmCmpBranch_mask;
      break;
    }
    case TestBranchType: {
      branch_imm = vixl::Assembler::ImmTestBranch(offset);
      imm_mask = ImmTestBranch_mask;
      break;
    }
    default: VIXL_UNREACHABLE();
  }
  SetInstructionBits(Mask(~imm_mask) | branch_imm);
}


void Instruction::SetImmLLiteral(const Instruction* source) {
  VIXL_ASSERT(IsWordAligned(source));
  ptrdiff_t offset = (source - this) >> kLiteralEntrySizeLog2;
  Instr imm = vixl::Assembler::ImmLLiteral(offset);
  Instr mask = ImmLLiteral_mask;

  SetInstructionBits(Mask(~mask) | imm);
}
}  // namespace vixl

