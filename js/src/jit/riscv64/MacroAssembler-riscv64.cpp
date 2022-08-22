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

}  // namespace jit
}  // namespace js
