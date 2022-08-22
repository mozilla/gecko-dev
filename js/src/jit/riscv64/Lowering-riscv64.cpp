/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/riscv64/Lowering-riscv64.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/riscv64/Assembler-riscv64.h"
#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;

void LIRGenerator::visitAbs(MAbs*) {
  MOZ_CRASH();
}
void LIRGenerator::visitAsmJSLoadHeap(MAsmJSLoadHeap*) {
  MOZ_CRASH();
}
void LIRGenerator::visitAsmJSStoreHeap(MAsmJSStoreHeap*) {
  MOZ_CRASH();
}
void LIRGenerator::visitAtomicExchangeTypedArrayElement(
    MAtomicExchangeTypedArrayElement*) {
  MOZ_CRASH();
}
void LIRGenerator::visitAtomicTypedArrayElementBinop(
    MAtomicTypedArrayElementBinop*) {
  MOZ_CRASH();
}
void LIRGenerator::visitBox(MBox*) {
  MOZ_CRASH();
}
void LIRGenerator::visitCompareExchangeTypedArrayElement(
    MCompareExchangeTypedArrayElement*) {
  MOZ_CRASH();
}
void LIRGenerator::visitCopySign(MCopySign*) {
  MOZ_CRASH();
}
void LIRGenerator::visitExtendInt32ToInt64(MExtendInt32ToInt64*) {
  MOZ_CRASH();
}
void LIRGenerator::visitInt64ToFloatingPoint(MInt64ToFloatingPoint*) {
  MOZ_CRASH();
}
void LIRGenerator::visitPowHalf(MPowHalf*) {
  MOZ_CRASH();
}
void LIRGenerator::visitReturnImpl(MDefinition*, bool) {
  MOZ_CRASH();
}
void LIRGenerator::visitSignExtendInt64(MSignExtendInt64*) {
  MOZ_CRASH();
}
void LIRGenerator::visitSubstr(MSubstr*) {
  MOZ_CRASH();
}
void LIRGenerator::visitUnbox(MUnbox*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmAtomicBinopHeap(MWasmAtomicBinopHeap*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmAtomicExchangeHeap(MWasmAtomicExchangeHeap*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmBinarySimd128(MWasmBinarySimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmBinarySimd128WithConstant(
    MWasmBinarySimd128WithConstant*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmCompareExchangeHeap(MWasmCompareExchangeHeap*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmHeapBase(MWasmHeapBase*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmLoad(MWasmLoad*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmLoadLaneSimd128(MWasmLoadLaneSimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmNeg(MWasmNeg*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmReduceSimd128(MWasmReduceSimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmReplaceLaneSimd128(MWasmReplaceLaneSimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmScalarToSimd128(MWasmScalarToSimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmShiftSimd128(MWasmShiftSimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmShuffleSimd128(MWasmShuffleSimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmStore(MWasmStore*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmStoreLaneSimd128(MWasmStoreLaneSimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmTernarySimd128(MWasmTernarySimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmTruncateToInt64(MWasmTruncateToInt64*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmUnarySimd128(MWasmUnarySimd128*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmUnsignedToDouble(MWasmUnsignedToDouble*) {
  MOZ_CRASH();
}
void LIRGenerator::visitWasmUnsignedToFloat32(MWasmUnsignedToFloat32*) {
  MOZ_CRASH();
}