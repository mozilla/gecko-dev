/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/riscv64/MacroAssembler-riscv64.h"

#include "jsmath.h"

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/JitFrames.h"
#include "jit/JitRuntime.h"
#include "jit/MacroAssembler.h"
#include "jit/MoveEmitter.h"
#include "jit/riscv64/SharedICRegisters-riscv64.h"
#include "util/Memory.h"
#include "vm/JitActivation.h"  // jit::JitActivation
#include "vm/JSContext.h"

#include "jit/MacroAssembler-inl.h"

namespace js {
namespace jit {

CodeOffset MacroAssembler::call(Label* label) {
  MOZ_CRASH();
}

void MacroAssembler::branchPtrInNurseryChunk(Assembler::Condition,
                                             Register,
                                             Register,
                                             Label*) {
  MOZ_CRASH();
}
void MacroAssembler::branchValueIsNurseryCell(Assembler::Condition,
                                              ValueOperand,
                                              Register,
                                              Label*) {
  MOZ_CRASH();
}
void MacroAssembler::call(ImmPtr) {
  MOZ_CRASH();
}
void MacroAssembler::call(JitCode*) {
  MOZ_CRASH();
}
CodeOffset MacroAssembler::call(wasm::SymbolicAddress) {
  MOZ_CRASH();
}
void MacroAssembler::callWithABIPost(unsigned int, MoveOp::Type, bool) {
  MOZ_CRASH();
}
void MacroAssembler::callWithABIPre(unsigned int*, bool) {
  MOZ_CRASH();
}
CodeOffset MacroAssembler::callWithPatch() {
  MOZ_CRASH();
}
void MacroAssembler::convertInt64ToDouble(Register64, FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::convertInt64ToFloat32(Register64, FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::convertIntPtrToDouble(Register, FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::convertUInt64ToDouble(Register64,
                                           FloatRegister,
                                           Register) {
  MOZ_CRASH();
}
bool MacroAssembler::convertUInt64ToDoubleNeedsTemp() {
  MOZ_CRASH();
}
void MacroAssembler::convertUInt64ToFloat32(Register64,
                                            FloatRegister,
                                            Register) {
  MOZ_CRASH();
}
void MacroAssembler::flush() {
  MOZ_CRASH();
}
void MacroAssembler::moveValue(TypedOrValueRegister const&,
                               ValueOperand const&) {
  MOZ_CRASH();
}
void MacroAssembler::moveValue(ValueOperand const&, ValueOperand const&) {
  MOZ_CRASH();
}
void MacroAssembler::moveValue(Value const&, ValueOperand const&) {
  MOZ_CRASH();
}
void MacroAssembler::nearbyIntDouble(RoundingMode,
                                     FloatRegister,
                                     FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::nearbyIntFloat32(RoundingMode,
                                      FloatRegister,
                                      FloatRegister) {
  MOZ_CRASH();
}
CodeOffset MacroAssembler::nopPatchableToCall() {
  MOZ_CRASH();
}
void MacroAssembler::oolWasmTruncateCheckF32ToI32(FloatRegister,
                                                  Register,
                                                  unsigned int,
                                                  wasm::BytecodeOffset,
                                                  Label*) {
  MOZ_CRASH();
}
void MacroAssembler::oolWasmTruncateCheckF32ToI64(FloatRegister,
                                                  Register64,
                                                  unsigned int,
                                                  wasm::BytecodeOffset,
                                                  Label*) {
  MOZ_CRASH();
}
void MacroAssembler::oolWasmTruncateCheckF64ToI32(FloatRegister,
                                                  Register,
                                                  unsigned int,
                                                  wasm::BytecodeOffset,
                                                  Label*) {
  MOZ_CRASH();
}
void MacroAssembler::oolWasmTruncateCheckF64ToI64(FloatRegister,
                                                  Register64,
                                                  unsigned int,
                                                  wasm::BytecodeOffset,
                                                  Label*) {
  MOZ_CRASH();
}
void MacroAssembler::patchCallToNop(unsigned char*) {
  MOZ_CRASH();
}
void MacroAssembler::patchNopToCall(unsigned char*, unsigned char*) {
  MOZ_CRASH();
}
void MacroAssembler::Pop(Register) {
  MOZ_CRASH();
}
void MacroAssembler::Pop(ValueOperand const&) {
  MOZ_CRASH();
}
void MacroAssembler::PopRegsInMaskIgnore(LiveRegisterSet, LiveRegisterSet) {
  MOZ_CRASH();
}
uint32_t MacroAssembler::pushFakeReturnAddress(Register) {
  MOZ_CRASH();
}
void MacroAssembler::Push(FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::Push(Imm32) {
  MOZ_CRASH();
}
void MacroAssembler::Push(ImmGCPtr) {
  MOZ_CRASH();
}
void MacroAssembler::Push(ImmPtr) {
  MOZ_CRASH();
}
void MacroAssembler::Push(ImmWord) {
  MOZ_CRASH();
}
void MacroAssembler::Push(Register) {
  MOZ_CRASH();
}
void MacroAssembler::PushRegsInMask(LiveRegisterSet) {
  MOZ_CRASH();
}
size_t MacroAssembler::PushRegsInMaskSizeInBytes(LiveRegisterSet) {
  MOZ_CRASH();
}
void MacroAssembler::pushReturnAddress() {
  MOZ_CRASH();
}
void MacroAssembler::setupUnalignedABICall(Register) {
  MOZ_CRASH();
}

template <typename T>
void MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value,
                                       MIRType valueType,
                                       const T& dest,
                                       MIRType slotType) {
  MOZ_CRASH();
}

template void MacroAssembler::storeUnboxedValue(const ConstantOrRegister& value,
                                                MIRType valueType,
                                                const Address& dest,
                                                MIRType slotType);

template void MacroAssembler::storeUnboxedValue(
    const ConstantOrRegister& value,
    MIRType valueType,
    const BaseObjectElementIndex& dest,
    MIRType slotType);

void MacroAssembler::subFromStackPtr(Imm32) {
  MOZ_CRASH();
}
void MacroAssembler::wasmBoundsCheck32(Assembler::Condition,
                                       Register,
                                       Address,
                                       Label*) {
  MOZ_CRASH();
}
void MacroAssembler::wasmBoundsCheck32(Assembler::Condition,
                                       Register,
                                       Register,
                                       Label*) {
  MOZ_CRASH();
}
void MacroAssembler::wasmBoundsCheck64(Assembler::Condition,
                                       Register64,
                                       Address,
                                       Label*) {
  MOZ_CRASH();
}
void MacroAssembler::wasmBoundsCheck64(Assembler::Condition,
                                       Register64,
                                       Register64,
                                       Label*) {
  MOZ_CRASH();
}
CodeOffset MacroAssembler::wasmTrapInstruction() {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateDoubleToInt32(FloatRegister,
                                               Register,
                                               bool,
                                               Label*) {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateDoubleToInt64(FloatRegister,
                                               Register64,
                                               bool,
                                               Label*,
                                               Label*,
                                               FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateDoubleToUInt32(FloatRegister,
                                                Register,
                                                bool,
                                                Label*) {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateDoubleToUInt64(FloatRegister,
                                                Register64,
                                                bool,
                                                Label*,
                                                Label*,
                                                FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateFloat32ToInt32(FloatRegister,
                                                Register,
                                                bool,
                                                Label*) {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateFloat32ToInt64(FloatRegister,
                                                Register64,
                                                bool,
                                                Label*,
                                                Label*,
                                                FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateFloat32ToUInt32(FloatRegister,
                                                 Register,
                                                 bool,
                                                 Label*) {
  MOZ_CRASH();
}
void MacroAssembler::wasmTruncateFloat32ToUInt64(FloatRegister,
                                                 Register64,
                                                 bool,
                                                 Label*,
                                                 Label*,
                                                 FloatRegister) {
  MOZ_CRASH();
}
void MacroAssembler::widenInt32(Register r) {
  MOZ_CRASH();
}

CodeOffset MacroAssembler::moveNearAddressWithPatch(Register dest) {
  MOZ_CRASH();
}

void MacroAssembler::comment(char const*) {
  MOZ_CRASH();
}
void MacroAssembler::clampDoubleToUint8(FloatRegister, Register) {
  MOZ_CRASH();
}
void MacroAssembler::floorDoubleToInt32(FloatRegister, Register, Label*) {
  MOZ_CRASH();
}
void MacroAssembler::floorFloat32ToInt32(FloatRegister, Register, Label*) {
  MOZ_CRASH();
}
void MacroAssembler::ceilDoubleToInt32(FloatRegister, Register, Label*) {
  MOZ_CRASH();
}
void MacroAssembler::ceilFloat32ToInt32(FloatRegister, Register, Label*) {
  MOZ_CRASH();
}
void MacroAssembler::roundDoubleToInt32(FloatRegister,
                                        Register,
                                        FloatRegister,
                                        Label*) {
  MOZ_CRASH();
}
void MacroAssembler::roundFloat32ToInt32(FloatRegister,
                                         Register,
                                         FloatRegister,
                                         Label*) {
  MOZ_CRASH();
}
void MacroAssembler::truncDoubleToInt32(FloatRegister, Register, Label*) {
  MOZ_CRASH();
}
void MacroAssembler::truncFloat32ToInt32(FloatRegister, Register, Label*) {
  MOZ_CRASH();
}

void MacroAssembler::storeRegsInMask(LiveRegisterSet set, Address dest,
                                     Register) {
  MOZ_CRASH();
}

int32_t MacroAssemblerRiscv64::GetOffset(int32_t offset, Label* L, OffsetSize bits) {
  if (L) {
    offset = branch_offset_helper(L, bits);
  } else {
    MOZ_ASSERT(is_intn(offset, bits));
  }
  return offset;
}

bool MacroAssemblerRiscv64::CalculateOffset(Label* L, int32_t* offset,
                                     OffsetSize bits) {
  if (!is_near(L, bits)) return false;
  *offset = GetOffset(*offset, L, bits);
  return true;
}

void MacroAssemblerRiscv64::BranchShortHelper(int32_t offset, Label* L) {
  MOZ_ASSERT(L == nullptr || offset == 0);
  offset = GetOffset(offset, L, OffsetSize::kOffset21);
  Assembler::j(offset);
}

bool MacroAssemblerRiscv64::BranchShortHelper(int32_t offset, Label* L, Condition cond,
                                       Register rs, const Operand& rt) {
  MOZ_ASSERT(L == nullptr || offset == 0);
  MOZ_ASSERT(rt.is_reg() && rt.is_imm());
  UseScratchRegisterScope temps(this);
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register scratch = Register();
  if (rt.is_imm()) {
    scratch = temps.Acquire();
    ma_li(scratch, rt);
  } else {
    scratch = rt.rm();
  }
  {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    switch (cond) {
      case Always:
        if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
        Assembler::j(offset);
        EmitConstPoolWithJumpIfNeeded();
        break;
      case Equal:
        // rs == rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          Assembler::j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          Assembler::beq(rs, scratch, offset);
        }
        break;
      case NotEqual:
        // rs != rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          Assembler::bne(rs, scratch, offset);
        }
        break;

      // Signed comparison.
      case GreaterThan:
        // rs > rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          Assembler::bgt(rs, scratch, offset);
        }
        break;
      case GreaterThanOrEqual:
        // rs >= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          Assembler::j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          Assembler::bge(rs, scratch, offset);
        }
        break;
      case LessThan:
        // rs < rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          Assembler::blt(rs, scratch, offset);
        }
        break;
      case LessThanOrEqual:
        // rs <= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          Assembler::j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
         Assembler:: ble(rs, scratch, offset);
        }
        break;

      // Unsigned comparison.
      case Above:
        // rs > rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          Assembler::bgtu(rs, scratch, offset);
        }
        break;
      case AboveOrEqual:
        // rs >= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          Assembler::j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
         Assembler::bgeu(rs, scratch, offset);
        }
        break;
      case Below:
        // rs < rt
        if (rt.is_reg() && rs == rt.rm()) {
          break;  // No code needs to be emitted.
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          bltu(rs, scratch, offset);
        }
        break;
      case BelowOrEqual:
        // rs <= rt
        if (rt.is_reg() && rs == rt.rm()) {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset21)) return false;
          Assembler::j(offset);
        } else {
          if (!CalculateOffset(L, &offset, OffsetSize::kOffset13)) return false;
          Assembler::bleu(rs, scratch, offset);
        }
        break;
      default:
        MOZ_CRASH("UNREACHABLE");
    }
  }

  CheckTrampolinePoolQuick(1);
  return true;
}

// BRANCH_ARGS_CHECK checks that conditional jump arguments are correct.
#define BRANCH_ARGS_CHECK(cond, rs, rt)                          \
  MOZ_ASSERT((cond == Always && rs == zero && rt.rm() == zero) || \
         (cond != Always && (rs != zero || rt.rm() != zero)))

bool MacroAssemblerRiscv64::BranchShortCheck(int32_t offset, Label* L, Condition cond,
                                      Register rs, const Operand& rt) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  if (!L) {
    MOZ_ASSERT(is_int13(offset));
    return BranchShortHelper(offset, nullptr, cond, rs, rt);
  } else {
    MOZ_ASSERT(offset == 0);
    return BranchShortHelper(0, L, cond, rs, rt);
  }
}

void MacroAssemblerRiscv64::BranchShort(Label* L) { BranchShortHelper(0, L); }

void MacroAssemblerRiscv64::BranchShort(int32_t offset, Condition cond, Register rs,
                                 const Operand& rt) {
  BranchShortCheck(offset, nullptr, cond, rs, rt);
}

void MacroAssemblerRiscv64::BranchShort(Label* L, Condition cond, Register rs,
                                 const Operand& rt) {
  BranchShortCheck(0, L, cond, rs, rt);
}

void MacroAssemblerRiscv64::BranchLong(Label* L) {
  // Generate position independent long branch.
  BlockTrampolinePoolScope block_trampoline_pool(this);
  int32_t imm;
  imm = branch_long_offset(L);
  GenPCRelativeJump(t6, imm);
  EmitConstPoolWithJumpIfNeeded();
}

void MacroAssemblerRiscv64::ma_branch(Label* L,
                                      Condition cond,
                                      Register rs,
                                      const Operand& rt,
                                      JumpKind jumpKind) {
  if (L->used()) {
    if (!BranchShortCheck(0, L, cond, rs, rt)) {
      if (cond != Always) {
        Label skip;
        Condition neg_cond = NegateCondition(cond);
        BranchShort(&skip, neg_cond, rs, rt);
        BranchLong(L);
        bind(&skip);
      } else {
        BranchLong(L);
        EmitConstPoolWithJumpIfNeeded();
      }
    }
  } else {
    if (is_trampoline_emitted() && jumpKind == LongJump) {
      if (cond != Always) {
        Label skip;
        Condition neg_cond = NegateCondition(cond);
        BranchShort(&skip, neg_cond, rs, rt);
        BranchLong(L);
        bind(&skip);
      } else {
        BranchLong(L);
        EmitConstPoolWithJumpIfNeeded();
      }
    } else {
      BranchShort(L, cond, rs, rt);
    }
  }
}

// Branches when done from within riscv code.
void MacroAssemblerRiscv64::ma_b(Register lhs, Register rhs, Label* label,
                                 Condition c, JumpKind jumpKind) {
  switch (c) {
    case Equal:
    case NotEqual:
      ma_branch(label, c, lhs, rhs, jumpKind);
      break;
    case Always:
      ma_branch(label, c, zero, Operand(zero), jumpKind);
      break;
    case Zero:
      MOZ_ASSERT(lhs == rhs);
      ma_branch(label, Equal, lhs, Operand(zero), jumpKind);
      break;
    case NonZero:
      MOZ_ASSERT(lhs == rhs);
      ma_branch(label, NotEqual, lhs, Operand(zero), jumpKind);
      break;
    case Signed:
      MOZ_ASSERT(lhs == rhs);
      ma_branch(label, GreaterThan, lhs, Operand(zero), jumpKind);
      break;
    case NotSigned:
      MOZ_ASSERT(lhs == rhs);
      ma_branch(label, LessThan, lhs, Operand(zero), jumpKind);
      break;
    default: {
      ma_branch(label, c, lhs, rhs, jumpKind);
      break;
    }
  }
}

}  // namespace jit
}  // namespace js
