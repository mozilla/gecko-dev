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

void MacroAssemblerRiscv64Compat::movePtr(Register src, Register dest) {
  ma_move(dest, src);
}
void MacroAssemblerRiscv64Compat::movePtr(ImmWord imm, Register dest) {
  ma_li(dest, imm);
}

void MacroAssemblerRiscv64Compat::movePtr(ImmGCPtr imm, Register dest) {
  ma_li(dest, imm);
}

void MacroAssemblerRiscv64Compat::movePtr(ImmPtr imm, Register dest) {
  movePtr(ImmWord(uintptr_t(imm.value)), dest);
}
void MacroAssemblerRiscv64Compat::movePtr(wasm::SymbolicAddress imm,
                                         Register dest) {
  append(wasm::SymbolicAccess(CodeOffset(nextOffset().getOffset()), imm));
  ma_liPatchable(dest, ImmWord(-1));
}

bool MacroAssemblerRiscv64Compat::buildOOLFakeExitFrame(void* fakeReturnAddr) {
  asMasm().PushFrameDescriptor(FrameType::IonJS);  // descriptor_
  asMasm().Push(ImmPtr(fakeReturnAddr));
  asMasm().Push(FramePointer);
  return true;
}

void MacroAssemblerRiscv64Compat::convertUInt32ToDouble(Register src,
                                                        FloatRegister dest) {
  ScratchRegisterScope scratch(asMasm());
  as_bstrpick_d(scratch, src, 31, 0);
  asMasm().convertInt64ToDouble(Register64(scratch), dest);
}

void MacroAssemblerRiscv64Compat::convertUInt64ToDouble(Register src,
                                                        FloatRegister dest) {
  Label positive, done;
  ma_b(src, src, &positive, NotSigned, ShortJump);
  ScratchRegisterScope scratch(asMasm());
  SecondScratchRegisterScope scratch2(asMasm());

  MOZ_ASSERT(src != scratch);
  MOZ_ASSERT(src != scratch2);

  ma_and(scratch, src, Imm32(1));
  as_srli_d(scratch2, src, 1);
  as_or(scratch, scratch, scratch2);
  as_movgr2fr_d(dest, scratch);
  as_ffint_d_l(dest, dest);
  asMasm().addDouble(dest, dest);
  ma_b(&done, ShortJump);

  bind(&positive);
  as_movgr2fr_d(dest, src);
  as_ffint_d_l(dest, dest);

  bind(&done);
}

void MacroAssemblerRiscv64Compat::convertUInt32ToFloat32(Register src,
                                                         FloatRegister dest) {
  ScratchRegisterScope scratch(asMasm());
  as_bstrpick_d(scratch, src, 31, 0);
  asMasm().convertInt64ToFloat32(Register64(scratch), dest);
}

void MacroAssemblerRiscv64Compat::convertDoubleToFloat32(FloatRegister src,
                                                         FloatRegister dest) {
  as_fcvt_s_d(dest, src);
}

const int CauseBitPos = int(Assembler::CauseI);
const int CauseBitCount = 1 + int(Assembler::CauseV) - int(Assembler::CauseI);
const int CauseIOrVMask = ((1 << int(Assembler::CauseI)) |
                           (1 << int(Assembler::CauseV))) >>
                          int(Assembler::CauseI);

// Checks whether a double is representable as a 32-bit integer. If so, the
// integer is written to the output register. Otherwise, a bailout is taken to
// the given snapshot. This function overwrites the scratch float register.
void MacroAssemblerRiscv64Compat::convertDoubleToInt32(FloatRegister src,
                                                       Register dest,
                                                       Label* fail,
                                                       bool negativeZeroCheck) {
  if (negativeZeroCheck) {
    moveFromDouble(src, dest);
    as_rotri_d(dest, dest, 63);
    ma_b(dest, Imm32(1), fail, Assembler::Equal);
  }

  ScratchRegisterScope scratch(asMasm());
  ScratchFloat32Scope fpscratch(asMasm());
  // Truncate double to int ; if result is inexact or invalid fail.
  as_ftintrz_w_d(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromFloat32(fpscratch, dest);
  as_bstrpick_d(scratch, scratch, CauseBitPos + CauseBitCount - 1, CauseBitPos);
  as_andi(scratch, scratch,
          CauseIOrVMask);  // masking for Inexact and Invalid flag.
  ma_b(scratch, zero, fail, Assembler::NotEqual);
}

void MacroAssemblerRiscv64Compat::convertDoubleToPtr(FloatRegister src,
                                                     Register dest, Label* fail,
                                                     bool negativeZeroCheck) {
  if (negativeZeroCheck) {
    moveFromDouble(src, dest);
    as_rotri_d(dest, dest, 63);
    ma_b(dest, Imm32(1), fail, Assembler::Equal);
  }

  ScratchRegisterScope scratch(asMasm());
  ScratchDoubleScope fpscratch(asMasm());
  // Truncate double to int64 ; if result is inexact or invalid fail.
  as_ftintrz_l_d(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromDouble(fpscratch, dest);
  as_bstrpick_d(scratch, scratch, CauseBitPos + CauseBitCount - 1, CauseBitPos);
  as_andi(scratch, scratch,
          CauseIOrVMask);  // masking for Inexact and Invalid flag.
  ma_b(scratch, zero, fail, Assembler::NotEqual);
}

// Checks whether a float32 is representable as a 32-bit integer. If so, the
// integer is written to the output register. Otherwise, a bailout is taken to
// the given snapshot. This function overwrites the scratch float register.
void MacroAssemblerRiscv64Compat::convertFloat32ToInt32(
    FloatRegister src, Register dest, Label* fail, bool negativeZeroCheck) {
  if (negativeZeroCheck) {
    moveFromFloat32(src, dest);
    ma_b(dest, Imm32(INT32_MIN), fail, Assembler::Equal);
  }

  ScratchRegisterScope scratch(asMasm());
  ScratchFloat32Scope fpscratch(asMasm());
  as_ftintrz_w_s(fpscratch, src);
  as_movfcsr2gr(scratch);
  moveFromFloat32(fpscratch, dest);
  MOZ_ASSERT(CauseBitPos + CauseBitCount < 33);
  MOZ_ASSERT(CauseBitPos < 32);
  as_bstrpick_w(scratch, scratch, CauseBitPos + CauseBitCount - 1, CauseBitPos);
  as_andi(scratch, scratch, CauseIOrVMask);
  ma_b(scratch, zero, fail, Assembler::NotEqual);
}

void MacroAssemblerRiscv64Compat::convertFloat32ToDouble(FloatRegister src,
                                                         FloatRegister dest) {
  as_fcvt_d_s(dest, src);
}

void MacroAssemblerRiscv64Compat::convertInt32ToFloat32(Register src,
                                                        FloatRegister dest) {
  as_movgr2fr_w(dest, src);
  as_ffint_s_w(dest, dest);
}

void MacroAssemblerRiscv64Compat::convertInt32ToFloat32(const Address& src,
                                                        FloatRegister dest) {
  ma_fld_s(dest, src);
  as_ffint_s_w(dest, dest);
}

void MacroAssemblerRiscv64Compat::movq(Register rj, Register rd) {
  as_or(rd, rj, zero);
}


// Memory.
void MacroAssemblerRiscv64::ma_load(Register dest, Address address,
                                   LoadStoreSize size,
                                   LoadStoreExtension extension) {
  int16_t encodedOffset;
  Register base;

  if (!is_int12(address.offset)) {
    ma_li(ScratchRegister, Imm32(address.offset));
    add(ScratchRegister, address.base, ScratchRegister);
    base = ScratchRegister;
    encodedOffset = 0;
  } else {
    encodedOffset = address.offset
    base = address.base;
  }

  switch (size) {
    case SizeByte:
      if (ZeroExtend == extension) {
        lbu(dest, base, encodedOffset);
      } else {
        lb(dest, base, encodedOffset);
      }
      break;
    case SizeHalfWord:
      if (ZeroExtend == extension) {
        lhu(dest, base, encodedOffset);
      } else {
        lh(dest, base, encodedOffset);
      }
      break;
    case SizeWord:
      if (ZeroExtend == extension) {
        lwu(dest, base, encodedOffset);
      } else {
        lw(dest, base, encodedOffset);
      }
      break;
    case SizeDouble:
      ld(dest, base, encodedOffset);
      break;
    default:
      MOZ_CRASH("Invalid argument for ma_load");
  }
}

void MacroAssemblerRiscv64::ma_store(Register data, Address address,
                                    LoadStoreSize size,
                                    LoadStoreExtension extension) {
  int16_t encodedOffset;
  Register base;

  if (!is_int12(address.offset)) {
    ma_li(ScratchRegister, Imm32(address.offset));
    add(ScratchRegister, address.base, ScratchRegister);
    base = ScratchRegister;
    encodedOffset = 0;
  } else {
    encodedOffset = address.offset
    base = address.base;
  }

  switch (size) {
    case SizeByte:
      sb(data, base, encodedOffset);
      break;
    case SizeHalfWord:
      sh(data, base, encodedOffset);
      break;
    case SizeWord:
      sw(data, base, encodedOffset);
      break;
    case SizeDouble:
      sd(data, base, encodedOffset);
      break;
    default:
      MOZ_CRASH("Invalid argument for ma_store");
  }
}


void MacroAssemblerRiscv64::loadPtr(const Address& address,
                                         Register dest) {
  ma_load(dest, address, SizeDouble);
}

void MacroAssemblerRiscv64::loadPtr(const BaseIndex& src, Register dest) {
  ma_load(dest, src, SizeDouble);
}

void MacroAssemblerRiscv64::loadPtr(AbsoluteAddress address,
                                         Register dest) {
  movePtr(ImmPtr(address.addr), ScratchRegister);
  loadPtr(Address(ScratchRegister, 0), dest);
}

void MacroAssemblerRiscv64::loadPtr(wasm::SymbolicAddress address,
                                         Register dest) {
  movePtr(address, ScratchRegister);
  loadPtr(Address(ScratchRegister, 0), dest);
}

void MacroAssemblerRiscv64Compat::computeScaledAddress(const BaseIndex& address,
                                                       Register dest) {
  Register base = address.base;
  Register index = address.index;
  int32_t shift = Imm32::ShiftOf(address.scale).value;
  UseScratchRegisterScope temps(this);
  Register tmp = rd == rt ? temps.Acquire() : rd;
  if (shift) {
    MOZ_ASSERT(shift <= 4);
    slli(tmp, index, shift);
  }
  add(dest, base, index);
}

void MacroAssemblerRiscv64Compat::wasmLoadI64Impl(
    const wasm::MemoryAccessDesc& access, Register memoryBase, Register ptr,
    Register ptrScratch, Register64 output, Register tmp) {
  uint32_t offset = access.offset();
  MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
  MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

  // Maybe add the offset.
  if (offset) {
    asMasm().addPtr(ImmWord(offset), ptrScratch);
    ptr = ptrScratch;
  }

  asMasm().memoryBarrierBefore(access.sync());

  switch (access.type()) {
    case Scalar::Int8:
      as_ldx_b(output.reg, memoryBase, ptr);
      break;
    case Scalar::Uint8:
      as_ldx_bu(output.reg, memoryBase, ptr);
      break;
    case Scalar::Int16:
      as_ldx_h(output.reg, memoryBase, ptr);
      break;
    case Scalar::Uint16:
      as_ldx_hu(output.reg, memoryBase, ptr);
      break;
    case Scalar::Int32:
      as_ldx_w(output.reg, memoryBase, ptr);
      break;
    case Scalar::Uint32:
      // TODO(loong64): Why need zero-extension here?
      as_ldx_wu(output.reg, memoryBase, ptr);
      break;
    case Scalar::Int64:
      as_ldx_d(output.reg, memoryBase, ptr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }

  asMasm().append(access, asMasm().size() - 4);
  asMasm().memoryBarrierAfter(access.sync());
}

void MacroAssemblerRiscv64Compat::wasmStoreI64Impl(
    const wasm::MemoryAccessDesc& access, Register64 value, Register memoryBase,
    Register ptr, Register ptrScratch, Register tmp) {
  uint32_t offset = access.offset();
  MOZ_ASSERT(offset < asMasm().wasmMaxOffsetGuardLimit());
  MOZ_ASSERT_IF(offset, ptrScratch != InvalidReg);

  // Maybe add the offset.
  if (offset) {
    asMasm().addPtr(ImmWord(offset), ptrScratch);
    ptr = ptrScratch;
  }

  asMasm().memoryBarrierBefore(access.sync());

  switch (access.type()) {
    case Scalar::Int8:
    case Scalar::Uint8:
      as_stx_b(value.reg, memoryBase, ptr);
      break;
    case Scalar::Int16:
    case Scalar::Uint16:
      as_stx_h(value.reg, memoryBase, ptr);
      break;
    case Scalar::Int32:
    case Scalar::Uint32:
      as_stx_w(value.reg, memoryBase, ptr);
      break;
    case Scalar::Int64:
      as_stx_d(value.reg, memoryBase, ptr);
      break;
    default:
      MOZ_CRASH("unexpected array type");
  }

  asMasm().append(access, asMasm().size() - 4);
  asMasm().memoryBarrierAfter(access.sync());
}

void MacroAssemblerRiscv64Compat::profilerEnterFrame(Register framePtr,
                                                     Register scratch) {
  asMasm().loadJSContext(scratch);
  loadPtr(Address(scratch, offsetof(JSContext, profilingActivation_)), scratch);
  storePtr(framePtr,
           Address(scratch, JitActivation::offsetOfLastProfilingFrame()));
  storePtr(ImmPtr(nullptr),
           Address(scratch, JitActivation::offsetOfLastProfilingCallSite()));
}

void MacroAssemblerRiscv64Compat::profilerExitFrame() {
  jump(asMasm().runtime()->jitRuntime()->getProfilerExitFrameTail());
}

void MacroAssemblerRiscv64Compat::move32(Imm32 imm, Register dest) {
  ma_li(dest, imm);
}

void MacroAssemblerRiscv64Compat::move32(Register src, Register dest) {
  slliw(dest, src, 0);
}

void MacroAssemblerRiscv64Compat::load8ZeroExtend(const Address& address,
                                                  Register dest) {
  ma_load(dest, address, SizeByte, ZeroExtend);
}

void MacroAssemblerRiscv64Compat::load8ZeroExtend(const BaseIndex& src,
                                                  Register dest) {
  ma_load(dest, src, SizeByte, ZeroExtend);
}

void MacroAssemblerRiscv64Compat::load8SignExtend(const Address& address,
                                                  Register dest) {
  ma_load(dest, address, SizeByte, SignExtend);
}

void MacroAssemblerRiscv64Compat::load8SignExtend(const BaseIndex& src,
                                                  Register dest) {
  ma_load(dest, src, SizeByte, SignExtend);
}

void MacroAssemblerRiscv64Compat::load16ZeroExtend(const Address& address,
                                                   Register dest) {
  ma_load(dest, address, SizeHalfWord, ZeroExtend);
}

void MacroAssemblerRiscv64Compat::load16ZeroExtend(const BaseIndex& src,
                                                   Register dest) {
  ma_load(dest, src, SizeHalfWord, ZeroExtend);
}

void MacroAssemblerRiscv64Compat::load16SignExtend(const Address& address,
                                                   Register dest) {
  ma_load(dest, address, SizeHalfWord, SignExtend);
}

void MacroAssemblerRiscv64Compat::load16SignExtend(const BaseIndex& src,
                                                   Register dest) {
  ma_load(dest, src, SizeHalfWord, SignExtend);
}

void MacroAssemblerRiscv64Compat::load32(const Address& address, Register dest) {
  ma_load(dest, address, SizeWord);
}

void MacroAssemblerRiscv64Compat::load32(const BaseIndex& address,
                                        Register dest) {
  ma_load(dest, address, SizeWord);
}

void MacroAssemblerRiscv64Compat::load32(AbsoluteAddress address,
                                        Register dest) {
  movePtr(ImmPtr(address.addr), ScratchRegister);
  load32(Address(ScratchRegister, 0), dest);
}

void MacroAssemblerRiscv64Compat::load32(wasm::SymbolicAddress address,
                                        Register dest) {
  movePtr(address, ScratchRegister);
  load32(Address(ScratchRegister, 0), dest);
}

void MacroAssemblerRiscv64Compat::loadPtr(const Address& address,
                                         Register dest) {
  ma_load(dest, address, SizeDouble);
}

void MacroAssemblerRiscv64Compat::loadPtr(const BaseIndex& src, Register dest) {
  ma_load(dest, src, SizeDouble);
}

void MacroAssemblerRiscv64Compat::loadPtr(AbsoluteAddress address,
                                         Register dest) {
  movePtr(ImmPtr(address.addr), ScratchRegister);
  loadPtr(Address(ScratchRegister, 0), dest);
}

void MacroAssemblerRiscv64Compat::loadPtr(wasm::SymbolicAddress address,
                                         Register dest) {
  movePtr(address, ScratchRegister);
  loadPtr(Address(ScratchRegister, 0), dest);
}

void MacroAssemblerRiscv64Compat::loadPrivate(const Address& address,
                                              Register dest) {
  loadPtr(address, dest);
}

void MacroAssemblerRiscv64Compat::store8(Imm32 imm, const Address& address) {
  ma_li(SecondScratchReg, imm);
  ma_store(SecondScratchReg, address, SizeByte);
}

void MacroAssemblerRiscv64Compat::store8(Register src, const Address& address) {
  ma_store(src, address, SizeByte);
}

void MacroAssemblerRiscv64Compat::store8(Imm32 imm, const BaseIndex& dest) {
  ma_store(imm, dest, SizeByte);
}

void MacroAssemblerRiscv64Compat::store8(Register src, const BaseIndex& dest) {
  ma_store(src, dest, SizeByte);
}

void MacroAssemblerRiscv64Compat::store16(Imm32 imm, const Address& address) {
  ma_li(SecondScratchReg, imm);
  ma_store(SecondScratchReg, address, SizeHalfWord);
}

void MacroAssemblerRiscv64Compat::store16(Register src, const Address& address) {
  ma_store(src, address, SizeHalfWord);
}

void MacroAssemblerRiscv64Compat::store16(Imm32 imm, const BaseIndex& dest) {
  ma_store(imm, dest, SizeHalfWord);
}

void MacroAssemblerRiscv64Compat::store16(Register src,
                                         const BaseIndex& address) {
  ma_store(src, address, SizeHalfWord);
}

void MacroAssemblerRiscv64Compat::store32(Register src,
                                         AbsoluteAddress address) {
  movePtr(ImmPtr(address.addr), ScratchRegister);
  store32(src, Address(ScratchRegister, 0));
}

void MacroAssemblerRiscv64Compat::store32(Register src, const Address& address) {
  ma_store(src, address, SizeWord);
}

void MacroAssemblerRiscv64Compat::store32(Imm32 src, const Address& address) {
  move32(src, SecondScratchReg);
  ma_store(SecondScratchReg, address, SizeWord);
}

void MacroAssemblerRiscv64Compat::store32(Imm32 imm, const BaseIndex& dest) {
  ma_store(imm, dest, SizeWord);
}

void MacroAssemblerRiscv64Compat::store32(Register src, const BaseIndex& dest) {
  ma_store(src, dest, SizeWord);
}

template <typename T>
void MacroAssemblerRiscv64Compat::storePtr(ImmWord imm, T address) {
  ma_li(SecondScratchReg, imm);
  ma_store(SecondScratchReg, address, SizeDouble);
}

template void MacroAssemblerRiscv64Compat::storePtr<Address>(ImmWord imm,
                                                            Address address);
template void MacroAssemblerRiscv64Compat::storePtr<BaseIndex>(
    ImmWord imm, BaseIndex address);

template <typename T>
void MacroAssemblerRiscv64Compat::storePtr(ImmPtr imm, T address) {
  storePtr(ImmWord(uintptr_t(imm.value)), address);
}

template void MacroAssemblerRiscv64Compat::storePtr<Address>(ImmPtr imm,
                                                            Address address);
template void MacroAssemblerRiscv64Compat::storePtr<BaseIndex>(
    ImmPtr imm, BaseIndex address);

template <typename T>
void MacroAssemblerRiscv64Compat::storePtr(ImmGCPtr imm, T address) {
  movePtr(imm, SecondScratchReg);
  storePtr(SecondScratchReg, address);
}

template void MacroAssemblerRiscv64Compat::storePtr<Address>(ImmGCPtr imm,
                                                            Address address);
template void MacroAssemblerRiscv64Compat::storePtr<BaseIndex>(
    ImmGCPtr imm, BaseIndex address);

void MacroAssemblerRiscv64Compat::storePtr(Register src,
                                          const Address& address) {
  ma_store(src, address, SizeDouble);
}

void MacroAssemblerRiscv64Compat::storePtr(Register src,
                                          const BaseIndex& address) {
  ma_store(src, address, SizeDouble);
}

void MacroAssemblerRiscv64Compat::storePtr(Register src, AbsoluteAddress dest) {
  movePtr(ImmPtr(dest.addr), ScratchRegister);
  storePtr(src, Address(ScratchRegister, 0));
}

void MacroAssemblerRiscv64Compat::testNullSet(Condition cond,
                                             const ValueOperand& value,
                                             Register dest) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  splitTag(value, SecondScratchReg);
  ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_NULL), cond);
}

void MacroAssemblerRiscv64Compat::testObjectSet(Condition cond,
                                               const ValueOperand& value,
                                               Register dest) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  splitTag(value, SecondScratchReg);
  ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_OBJECT), cond);
}

void MacroAssemblerRiscv64Compat::testUndefinedSet(Condition cond,
                                                  const ValueOperand& value,
                                                  Register dest) {
  MOZ_ASSERT(cond == Equal || cond == NotEqual);
  splitTag(value, SecondScratchReg);
  ma_cmp_set(dest, SecondScratchReg, ImmTag(JSVAL_TAG_UNDEFINED), cond);
}

void MacroAssemblerMIPS64Compat::unboxInt32(const ValueOperand& operand,
                                            Register dest) {
  ma_sll(dest, operand.valueReg(), Imm32(0));
}

void MacroAssemblerMIPS64Compat::unboxInt32(Register src, Register dest) {
  ma_sll(dest, src, Imm32(0));
}

void MacroAssemblerMIPS64Compat::unboxInt32(const Address& src, Register dest) {
  load32(Address(src.base, src.offset), dest);
}

void MacroAssemblerMIPS64Compat::unboxInt32(const BaseIndex& src,
                                            Register dest) {
  computeScaledAddress(src, SecondScratchReg);
  load32(Address(SecondScratchReg, src.offset), dest);
}

void MacroAssemblerMIPS64Compat::unboxBoolean(const ValueOperand& operand,
                                              Register dest) {
  ma_dext(dest, operand.valueReg(), Imm32(0), Imm32(32));
}

void MacroAssemblerMIPS64Compat::unboxBoolean(Register src, Register dest) {
  ma_dext(dest, src, Imm32(0), Imm32(32));
}

void MacroAssemblerMIPS64Compat::unboxBoolean(const Address& src,
                                              Register dest) {
  ma_load(dest, Address(src.base, src.offset), SizeWord, ZeroExtend);
}

void MacroAssemblerMIPS64Compat::unboxBoolean(const BaseIndex& src,
                                              Register dest) {
  computeScaledAddress(src, SecondScratchReg);
  ma_load(dest, Address(SecondScratchReg, src.offset), SizeWord, ZeroExtend);
}

void MacroAssemblerMIPS64Compat::unboxDouble(const ValueOperand& operand,
                                             FloatRegister dest) {
  as_dmtc1(operand.valueReg(), dest);
}

void MacroAssemblerMIPS64Compat::unboxDouble(const Address& src,
                                             FloatRegister dest) {
  ma_ld(dest, Address(src.base, src.offset));
}
void MacroAssemblerMIPS64Compat::unboxDouble(const BaseIndex& src,
                                             FloatRegister dest) {
  SecondScratchRegisterScope scratch(asMasm());
  loadPtr(src, scratch);
  unboxDouble(ValueOperand(scratch), dest);
}

void MacroAssemblerMIPS64Compat::unboxString(const ValueOperand& operand,
                                             Register dest) {
  unboxNonDouble(operand, dest, JSVAL_TYPE_STRING);
}

void MacroAssemblerMIPS64Compat::unboxString(Register src, Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_STRING);
}

void MacroAssemblerMIPS64Compat::unboxString(const Address& src,
                                             Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_STRING);
}

void MacroAssemblerMIPS64Compat::unboxSymbol(const ValueOperand& operand,
                                             Register dest) {
  unboxNonDouble(operand, dest, JSVAL_TYPE_SYMBOL);
}

void MacroAssemblerMIPS64Compat::unboxSymbol(Register src, Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_SYMBOL);
}

void MacroAssemblerMIPS64Compat::unboxSymbol(const Address& src,
                                             Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_SYMBOL);
}

void MacroAssemblerMIPS64Compat::unboxBigInt(const ValueOperand& operand,
                                             Register dest) {
  unboxNonDouble(operand, dest, JSVAL_TYPE_BIGINT);
}

void MacroAssemblerMIPS64Compat::unboxBigInt(Register src, Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_BIGINT);
}

void MacroAssemblerMIPS64Compat::unboxBigInt(const Address& src,
                                             Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_BIGINT);
}

void MacroAssemblerMIPS64Compat::unboxObject(const ValueOperand& src,
                                             Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
}

void MacroAssemblerMIPS64Compat::unboxObject(Register src, Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
}

void MacroAssemblerMIPS64Compat::unboxObject(const Address& src,
                                             Register dest) {
  unboxNonDouble(src, dest, JSVAL_TYPE_OBJECT);
}

void MacroAssemblerMIPS64Compat::unboxValue(const ValueOperand& src,
                                            AnyRegister dest,
                                            JSValueType type) {
  if (dest.isFloat()) {
    Label notInt32, end;
    asMasm().branchTestInt32(Assembler::NotEqual, src, &notInt32);
    convertInt32ToDouble(src.valueReg(), dest.fpu());
    ma_b(&end, ShortJump);
    bind(&notInt32);
    unboxDouble(src, dest.fpu());
    bind(&end);
  } else {
    unboxNonDouble(src, dest.gpr(), type);
  }
}

void MacroAssemblerMIPS64Compat::boxDouble(FloatRegister src,
                                           const ValueOperand& dest,
                                           FloatRegister) {
  as_dmfc1(dest.valueReg(), src);
}

void MacroAssemblerMIPS64Compat::boxNonDouble(JSValueType type, Register src,
                                              const ValueOperand& dest) {
  MOZ_ASSERT(src != dest.valueReg());
  boxValue(type, src, dest.valueReg());
}

void MacroAssemblerMIPS64Compat::boolValueToDouble(const ValueOperand& operand,
                                                   FloatRegister dest) {
  convertBoolToInt32(operand.valueReg(), ScratchRegister);
  convertInt32ToDouble(ScratchRegister, dest);
}

void MacroAssemblerMIPS64Compat::int32ValueToDouble(const ValueOperand& operand,
                                                    FloatRegister dest) {
  convertInt32ToDouble(operand.valueReg(), dest);
}

void MacroAssemblerMIPS64Compat::boolValueToFloat32(const ValueOperand& operand,
                                                    FloatRegister dest) {
  convertBoolToInt32(operand.valueReg(), ScratchRegister);
  convertInt32ToFloat32(ScratchRegister, dest);
}

void MacroAssemblerMIPS64Compat::int32ValueToFloat32(
    const ValueOperand& operand, FloatRegister dest) {
  convertInt32ToFloat32(operand.valueReg(), dest);
}

void MacroAssemblerMIPS64Compat::loadConstantFloat32(float f,
                                                     FloatRegister dest) {
  ma_lis(dest, f);
}

void MacroAssemblerMIPS64Compat::loadInt32OrDouble(const Address& src,
                                                   FloatRegister dest) {
  Label notInt32, end;
  // If it's an int, convert it to double.
  loadPtr(Address(src.base, src.offset), ScratchRegister);
  ma_dsrl(SecondScratchReg, ScratchRegister, Imm32(JSVAL_TAG_SHIFT));
  asMasm().branchTestInt32(Assembler::NotEqual, SecondScratchReg, &notInt32);
  loadPtr(Address(src.base, src.offset), SecondScratchReg);
  convertInt32ToDouble(SecondScratchReg, dest);
  ma_b(&end, ShortJump);

  // Not an int, just load as double.
  bind(&notInt32);
  unboxDouble(src, dest);
  bind(&end);
}

void MacroAssemblerMIPS64Compat::loadInt32OrDouble(const BaseIndex& addr,
                                                   FloatRegister dest) {
  Label notInt32, end;

  // If it's an int, convert it to double.
  computeScaledAddress(addr, SecondScratchReg);
  // Since we only have one scratch, we need to stomp over it with the tag.
  loadPtr(Address(SecondScratchReg, 0), ScratchRegister);
  ma_dsrl(SecondScratchReg, ScratchRegister, Imm32(JSVAL_TAG_SHIFT));
  asMasm().branchTestInt32(Assembler::NotEqual, SecondScratchReg, &notInt32);

  computeScaledAddress(addr, SecondScratchReg);
  loadPtr(Address(SecondScratchReg, 0), SecondScratchReg);
  convertInt32ToDouble(SecondScratchReg, dest);
  ma_b(&end, ShortJump);

  // Not an int, just load as double.
  bind(&notInt32);
  // First, recompute the offset that had been stored in the scratch register
  // since the scratch register was overwritten loading in the type.
  computeScaledAddress(addr, SecondScratchReg);
  unboxDouble(Address(SecondScratchReg, 0), dest);
  bind(&end);
}

void MacroAssemblerMIPS64Compat::loadConstantDouble(double dp,
                                                    FloatRegister dest) {
  ma_lid(dest, dp);
}

Register MacroAssemblerMIPS64Compat::extractObject(const Address& address,
                                                   Register scratch) {
  loadPtr(Address(address.base, address.offset), scratch);
  ma_dext(scratch, scratch, Imm32(0), Imm32(JSVAL_TAG_SHIFT));
  return scratch;
}

Register MacroAssemblerMIPS64Compat::extractTag(const Address& address,
                                                Register scratch) {
  loadPtr(Address(address.base, address.offset), scratch);
  ma_dext(scratch, scratch, Imm32(JSVAL_TAG_SHIFT),
          Imm32(64 - JSVAL_TAG_SHIFT));
  return scratch;
}

Register MacroAssemblerMIPS64Compat::extractTag(const BaseIndex& address,
                                                Register scratch) {
  computeScaledAddress(address, scratch);
  return extractTag(Address(scratch, address.offset), scratch);
}

/////////////////////////////////////////////////////////////////
// X86/X64-common/ARM/LoongArch interface.
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
// X86/X64-common/ARM/MIPS interface.
/////////////////////////////////////////////////////////////////
void MacroAssemblerMIPS64Compat::storeValue(ValueOperand val, Operand dst) {
  storeValue(val, Address(Register::FromCode(dst.base()), dst.disp()));
}

void MacroAssemblerMIPS64Compat::storeValue(ValueOperand val,
                                            const BaseIndex& dest) {
  computeScaledAddress(dest, SecondScratchReg);
  storeValue(val, Address(SecondScratchReg, dest.offset));
}

void MacroAssemblerMIPS64Compat::storeValue(JSValueType type, Register reg,
                                            BaseIndex dest) {
  computeScaledAddress(dest, ScratchRegister);

  int32_t offset = dest.offset;
  if (!Imm16::IsInSignedRange(offset)) {
    ma_li(SecondScratchReg, Imm32(offset));
    as_daddu(ScratchRegister, ScratchRegister, SecondScratchReg);
    offset = 0;
  }

  storeValue(type, reg, Address(ScratchRegister, offset));
}

void MacroAssemblerMIPS64Compat::storeValue(ValueOperand val,
                                            const Address& dest) {
  storePtr(val.valueReg(), Address(dest.base, dest.offset));
}

void MacroAssemblerMIPS64Compat::storeValue(JSValueType type, Register reg,
                                            Address dest) {
  MOZ_ASSERT(dest.base != SecondScratchReg);

  if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
    store32(reg, dest);
    JSValueShiftedTag tag = (JSValueShiftedTag)JSVAL_TYPE_TO_SHIFTED_TAG(type);
    store32(((Imm64(tag)).secondHalf()), Address(dest.base, dest.offset + 4));
  } else {
    ma_li(SecondScratchReg, ImmTag(JSVAL_TYPE_TO_TAG(type)));
    ma_dsll(SecondScratchReg, SecondScratchReg, Imm32(JSVAL_TAG_SHIFT));
    ma_dins(SecondScratchReg, reg, Imm32(0), Imm32(JSVAL_TAG_SHIFT));
    storePtr(SecondScratchReg, Address(dest.base, dest.offset));
  }
}

void MacroAssemblerMIPS64Compat::storeValue(const Value& val, Address dest) {
  if (val.isGCThing()) {
    writeDataRelocation(val);
    movWithPatch(ImmWord(val.asRawBits()), SecondScratchReg);
  } else {
    ma_li(SecondScratchReg, ImmWord(val.asRawBits()));
  }
  storePtr(SecondScratchReg, Address(dest.base, dest.offset));
}

void MacroAssemblerMIPS64Compat::storeValue(const Value& val, BaseIndex dest) {
  computeScaledAddress(dest, ScratchRegister);

  int32_t offset = dest.offset;
  if (!Imm16::IsInSignedRange(offset)) {
    ma_li(SecondScratchReg, Imm32(offset));
    as_daddu(ScratchRegister, ScratchRegister, SecondScratchReg);
    offset = 0;
  }
  storeValue(val, Address(ScratchRegister, offset));
}

void MacroAssemblerMIPS64Compat::loadValue(const BaseIndex& addr,
                                           ValueOperand val) {
  computeScaledAddress(addr, SecondScratchReg);
  loadValue(Address(SecondScratchReg, addr.offset), val);
}

void MacroAssemblerMIPS64Compat::loadValue(Address src, ValueOperand val) {
  loadPtr(Address(src.base, src.offset), val.valueReg());
}

void MacroAssemblerMIPS64Compat::tagValue(JSValueType type, Register payload,
                                          ValueOperand dest) {
  MOZ_ASSERT(dest.valueReg() != ScratchRegister);
  if (payload != dest.valueReg()) {
    ma_move(dest.valueReg(), payload);
  }
  ma_li(ScratchRegister, ImmTag(JSVAL_TYPE_TO_TAG(type)));
  ma_dins(dest.valueReg(), ScratchRegister, Imm32(JSVAL_TAG_SHIFT),
          Imm32(64 - JSVAL_TAG_SHIFT));
  if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
    ma_dins(dest.valueReg(), zero, Imm32(32), Imm32(JSVAL_TAG_SHIFT - 32));
  }
}

void MacroAssemblerMIPS64Compat::pushValue(ValueOperand val) {
  // Allocate stack slots for Value. One for each.
  asMasm().subPtr(Imm32(sizeof(Value)), StackPointer);
  // Store Value
  storeValue(val, Address(StackPointer, 0));
}

void MacroAssemblerMIPS64Compat::pushValue(const Address& addr) {
  // Load value before allocate stack, addr.base may be is sp.
  loadPtr(Address(addr.base, addr.offset), ScratchRegister);
  ma_dsubu(StackPointer, StackPointer, Imm32(sizeof(Value)));
  storePtr(ScratchRegister, Address(StackPointer, 0));
}

void MacroAssemblerMIPS64Compat::popValue(ValueOperand val) {
  as_ld(val.valueReg(), StackPointer, 0);
  as_daddiu(StackPointer, StackPointer, sizeof(Value));
}

void MacroAssemblerMIPS64Compat::breakpoint() { as_break(0); }

void MacroAssemblerMIPS64Compat::ensureDouble(const ValueOperand& source,
                                              FloatRegister dest,
                                              Label* failure) {
  Label isDouble, done;
  {
    ScratchTagScope tag(asMasm(), source);
    splitTagForTest(source, tag);
    asMasm().branchTestDouble(Assembler::Equal, tag, &isDouble);
    asMasm().branchTestInt32(Assembler::NotEqual, tag, failure);
  }

  unboxInt32(source, ScratchRegister);
  convertInt32ToDouble(ScratchRegister, dest);
  jump(&done);

  bind(&isDouble);
  unboxDouble(source, dest);

  bind(&done);
}

void MacroAssemblerMIPS64Compat::checkStackAlignment() {
#ifdef DEBUG
  Label aligned;
  as_andi(ScratchRegister, sp, ABIStackAlignment - 1);
  ma_b(ScratchRegister, zero, &aligned, Equal, ShortJump);
  as_break(BREAK_STACK_UNALIGNED);
  bind(&aligned);
#endif
}


void MacroAssemblerMIPS64Compat::handleFailureWithHandlerTail(
    Label* profilerExitTail, Label* bailoutTail) {
  // Reserve space for exception information.
  int size = (sizeof(ResumeFromException) + ABIStackAlignment) &
             ~(ABIStackAlignment - 1);
  asMasm().subPtr(Imm32(size), StackPointer);
  ma_move(a0, StackPointer);  // Use a0 since it is a first function argument

  // Call the handler.
  using Fn = void (*)(ResumeFromException * rfe);
  asMasm().setupUnalignedABICall(a1);
  asMasm().passABIArg(a0);
  asMasm().callWithABI<Fn, HandleException>(
      MoveOp::GENERAL, CheckUnsafeCallWithABI::DontCheckHasExitFrame);

  Label entryFrame;
  Label catch_;
  Label finally;
  Label returnBaseline;
  Label returnIon;
  Label bailout;
  Label wasm;
  Label wasmCatch;

  // Already clobbered a0, so use it...
  load32(Address(StackPointer, ResumeFromException::offsetOfKind()), a0);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ExceptionResumeKind::EntryFrame), &entryFrame);
  asMasm().branch32(Assembler::Equal, a0, Imm32(ExceptionResumeKind::Catch),
                    &catch_);
  asMasm().branch32(Assembler::Equal, a0, Imm32(ExceptionResumeKind::Finally),
                    &finally);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ExceptionResumeKind::ForcedReturnBaseline),
                    &returnBaseline);
  asMasm().branch32(Assembler::Equal, a0,
                    Imm32(ExceptionResumeKind::ForcedReturnIon), &returnIon);
  asMasm().branch32(Assembler::Equal, a0, Imm32(ExceptionResumeKind::Bailout),
                    &bailout);
  asMasm().branch32(Assembler::Equal, a0, Imm32(ExceptionResumeKind::Wasm),
                    &wasm);
  asMasm().branch32(Assembler::Equal, a0, Imm32(ExceptionResumeKind::WasmCatch),
                    &wasmCatch);

  breakpoint();  // Invalid kind.

  // No exception handler. Load the error value, restore state and return from
  // the entry frame.
  bind(&entryFrame);
  asMasm().moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfFramePointer()),
          FramePointer);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfStackPointer()),
          StackPointer);

  // We're going to be returning by the ion calling convention
  ma_pop(ra);
  as_jr(ra);
  as_nop();

  // If we found a catch handler, this must be a baseline frame. Restore
  // state and jump to the catch block.
  bind(&catch_);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfTarget()), a0);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfFramePointer()),
          FramePointer);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfStackPointer()),
          StackPointer);
  jump(a0);

  // If we found a finally block, this must be a baseline frame. Push two
  // values expected by the finally block: the exception and BooleanValue(true).
  bind(&finally);
  ValueOperand exception = ValueOperand(a1);
  loadValue(Address(sp, ResumeFromException::offsetOfException()), exception);

  loadPtr(Address(sp, ResumeFromException::offsetOfTarget()), a0);
  loadPtr(Address(sp, ResumeFromException::offsetOfFramePointer()),
          FramePointer);
  loadPtr(Address(sp, ResumeFromException::offsetOfStackPointer()), sp);

  pushValue(exception);
  pushValue(BooleanValue(true));
  jump(a0);

  // Return BaselineFrame->returnValue() to the caller.
  // Used in debug mode and for GeneratorReturn.
  Label profilingInstrumentation;
  bind(&returnBaseline);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfFramePointer()),
          FramePointer);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfStackPointer()),
          StackPointer);
  loadValue(Address(FramePointer, BaselineFrame::reverseOffsetOfReturnValue()),
            JSReturnOperand);
  jump(&profilingInstrumentation);

  // Return the given value to the caller.
  bind(&returnIon);
  loadValue(Address(StackPointer, ResumeFromException::offsetOfException()),
            JSReturnOperand);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfFramePointer()),
          FramePointer);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfStackPointer()),
          StackPointer);

  // If profiling is enabled, then update the lastProfilingFrame to refer to
  // caller frame before returning. This code is shared by ForcedReturnIon
  // and ForcedReturnBaseline.
  bind(&profilingInstrumentation);
  {
    Label skipProfilingInstrumentation;
    // Test if profiler enabled.
    AbsoluteAddress addressOfEnabled(
        asMasm().runtime()->geckoProfiler().addressOfEnabled());
    asMasm().branch32(Assembler::Equal, addressOfEnabled, Imm32(0),
                      &skipProfilingInstrumentation);
    jump(profilerExitTail);
    bind(&skipProfilingInstrumentation);
  }

  ma_move(StackPointer, FramePointer);
  pop(FramePointer);
  ret();

  // If we are bailing out to baseline to handle an exception, jump to
  // the bailout tail stub. Load 1 (true) in ReturnReg to indicate success.
  bind(&bailout);
  loadPtr(Address(sp, ResumeFromException::offsetOfBailoutInfo()), a2);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfStackPointer()),
          StackPointer);
  ma_li(ReturnReg, Imm32(1));
  jump(bailoutTail);

  // If we are throwing and the innermost frame was a wasm frame, reset SP and
  // FP; SP is pointing to the unwound return address to the wasm entry, so
  // we can just ret().
  bind(&wasm);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfFramePointer()),
          FramePointer);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfStackPointer()),
          StackPointer);
  ret();

  // Found a wasm catch handler, restore state and jump to it.
  bind(&wasmCatch);
  loadPtr(Address(sp, ResumeFromException::offsetOfTarget()), a1);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfFramePointer()),
          FramePointer);
  loadPtr(Address(StackPointer, ResumeFromException::offsetOfStackPointer()),
          StackPointer);
  jump(a1);
}

CodeOffset MacroAssemblerMIPS64Compat::toggledJump(Label* label) {
  CodeOffset ret(nextOffset().getOffset());
  ma_b(label);
  return ret;
}

CodeOffset MacroAssemblerMIPS64Compat::toggledCall(JitCode* target,
                                                   bool enabled) {
  BufferOffset bo = nextOffset();
  CodeOffset offset(bo.getOffset());
  addPendingJump(bo, ImmPtr(target->raw()), RelocationKind::JITCODE);
  ma_liPatchable(ScratchRegister, ImmPtr(target->raw()));
  if (enabled) {
    as_jalr(ScratchRegister);
    as_nop();
  } else {
    as_nop();
    as_nop();
  }
  MOZ_ASSERT_IF(!oom(), nextOffset().getOffset() - offset.offset() ==
                            ToggledCallSize(nullptr));
  return offset;
}

void MacroAssembler::subFromStackPtr(Imm32 imm32) {
  if (imm32.value) {
    asMasm().subPtr(imm32, StackPointer);
  }
}

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

// This method generates lui, dsll and ori instruction block that can be
// modified by UpdateLoad64Value, either during compilation (eg.
// Assembler::bind), or during execution (eg. jit::PatchJump).
void MacroAssemblerRiscv64::ma_liPatchable(Register dest, ImmPtr imm) {
  return ma_liPatchable(dest, ImmWord(uintptr_t(imm.value)));
}

void MacroAssemblerRiscv64::ma_liPatchable(Register dest, ImmWord imm,
                                          LiFlags flags) {
  if (Li64 == flags) {
    m_buffer.ensureSpace(8 * sizeof(uint32_t));
    li_constant(dest, imm.value);
  } else {
    m_buffer.ensureSpace(6 * sizeof(uint32_t));
    li_ptr(dest, imm.value);
  }
}

void MacroAssemblerRiscv64::ma_li(Register dest, ImmGCPtr ptr) {
  writeDataRelocation(ptr);
  ma_liPatchable(dest, ImmPtr(ptr.value));
}
void MacroAssemblerRiscv64::ma_li(Register dest, Imm32 imm) {
   RV_li(dest, imm.value);
}
void MacroAssemblerRiscv64::ma_li(Register dest, CodeLabel* label) {
  BufferOffset bo = m_buffer.nextOffset();
  ma_liPatchable(dest, ImmWord(/* placeholder */ 0));
  label->patchAt()->bind(bo.getOffset());
  label->setLinkMode(CodeLabel::MoveImmediate);
}
void MacroAssemblerRiscv64::ma_li(Register dest, ImmWord imm) {
   RV_li(dest, imm.value);
}

// Shortcut for when we know we're transferring 32 bits of data.
void MacroAssemblerRiscv64::ma_pop(Register r) {
  ld(r, StackPointer, 0);
  addi(StackPointer, StackPointer, sizeof(intptr_t));
}

void MacroAssemblerRiscv64::ma_push(Register r) {
  if (r == sp) {
    // Pushing sp requires one more instruction.
    ma_move(ScratchRegister, sp);
    r = ScratchRegister;
  }

  addi(StackPointer, StackPointer, (int32_t) - sizeof(intptr_t));
  sd(r, StackPointer, 0);
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
    ma_li(scratch, rt.immediate());
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

void MacroAssemblerRiscv64::ExtractBits(Register rt, Register rs, uint16_t pos,
                                 uint16_t size, bool sign_extend) {
#if JS_CODEGEN_RISCV64
  DCHECK(pos < 64 && 0 < size && size <= 64 && 0 < pos + size &&
         pos + size <= 64);
  slli(rt, rs, 64 - (pos + size));
  if (sign_extend) {
    srai(rt, rt, 64 - size);
  } else {
    srli(rt, rt, 64 - size);
  }
#elif JS_CODEGEN_RISCV32
  DCHECK_LT(pos, 32);
  DCHECK_GT(size, 0);
  DCHECK_LE(size, 32);
  DCHECK_GT(pos + size, 0);
  DCHECK_LE(pos + size, 32);
  slli(rt, rs, 32 - (pos + size));
  if (sign_extend) {
    srai(rt, rt, 32 - size);
  } else {
    srli(rt, rt, 32 - size);
  }
#endif
}

void MacroAssemblerRiscv64::InsertBits(Register dest, Register source, int pos, int size) {
  #if JS_CODEGEN_RISCV64
  DCHECK_LT(size, 64);
#elif JS_CODEGEN_RISCV32
  DCHECK_LT(size, 32);
#endif
  UseScratchRegisterScope temps(this);
  Register mask = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register source_ = temps.Acquire();
  // Create a mask of the length=size.
  li(mask, 1);
  slli(mask, mask, size);
  addi(mask, mask, -1);
  and_(source_, mask, source);
  slli(source_, source_, pos);
  // Make a mask containing 0's. 0's start at "pos" with length=size.
  slli(mask, mask, pos);
  not_(mask, mask);
  // cut area for insertion of source.
  and_(dest, mask, dest);
  // insert source
  or_(dest, dest, source_);
}

void MacroAssemblerRiscv64::InsertBits(Register dest, Register source, Register pos,
                                int size) {
#if JS_CODEGEN_RISCV64
  DCHECK_LT(size, 64);
#elif JS_CODEGEN_RISCV32
  DCHECK_LT(size, 32);
#endif
  UseScratchRegisterScope temps(this);
  Register mask = temps.Acquire();
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Register source_ = temps.Acquire();
  // Create a mask of the length=size.
  li(mask, 1);
  slli(mask, mask, size);
  addi(mask, mask, -1);
  and_(source_, mask, source);
  sll(source_, source_, pos);
  // Make a mask containing 0's. 0's start at "pos" with length=size.
  sll(mask, mask, pos);
  not_(mask, mask);
  // cut area for insertion of source.
  and_(dest, mask, dest);
  // insert source
  or_(dest, dest, source_);
}


}  // namespace jit
}  // namespace js
