
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "jit/riscv64/CodeGenerator-riscv64.h"

#include "mozilla/MathAlgorithms.h"

#include "jsnum.h"

#include "jit/CodeGenerator.h"
#include "jit/InlineScriptTree.h"
#include "jit/JitRuntime.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/Shape.h"

#include "jit/shared/CodeGenerator-shared-inl.h"
#include "vm/JSScript-inl.h"
namespace js {
namespace jit {
void CodeGenerator::visitValue(LValue*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNotI(LNotI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReinterpretFromI64(LWasmReinterpretFromI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompare(LCompare*) {
  MOZ_CRASH();
}
void CodeGenerator::visitShiftI(LShiftI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitSignExtendInt64(LSignExtendInt64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCopySignD(LCopySignD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitPopcntI64(LPopcntI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitTestDAndBranch(LTestDAndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitSubI(LSubI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect(
    LAtomicTypedArrayElementBinopForEffect*) {
  MOZ_CRASH();
}
void CodeGenerator::visitTruncateFToInt32(LTruncateFToInt32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicTypedArrayElementBinop(
    LAtomicTypedArrayElementBinop*) {
  MOZ_CRASH();
}
void CodeGenerator::visitBitAndAndBranch(LBitAndAndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNotD(LNotD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitTestI64AndBranch(LTestI64AndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareF(LCompareF*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAddI64(LAddI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareD(LCompareD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect64(
    LAtomicTypedArrayElementBinopForEffect64*) {
  MOZ_CRASH();
}

void CodeGenerator::visitWasmTruncateToInt32(LWasmTruncateToInt32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitBitOpI64(LBitOpI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNearbyInt(LNearbyInt*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmWrapU32Index(LWasmWrapU32Index*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareFAndBranch(LCompareFAndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmSelect(LWasmSelect*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmAddOffset(LWasmAddOffset*) {
  MOZ_CRASH();
}
void CodeGenerator::visitPopcntI(LPopcntI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicTypedArrayElementBinop64(
    LAtomicTypedArrayElementBinop64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitMulI64(LMulI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareExchangeTypedArrayElement(
    LCompareExchangeTypedArrayElement*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAsmJSStoreHeap(LAsmJSStoreHeap*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareI64AndBranch(LCompareI64AndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitClzI64(LClzI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareExchangeTypedArrayElement64(
    LCompareExchangeTypedArrayElement64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCopySignF(LCopySignF*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmLoad(LWasmLoad*) {
  MOZ_CRASH();
}
void CodeGenerator::visitSimd128(LSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmStoreI64(LWasmStoreI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmSelectI64(LWasmSelectI64*) {
  MOZ_CRASH();
}

void CodeGenerator::visitBitNotI(LBitNotI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitMinMaxF(js::jit::LMinMaxF*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareI64(LCompareI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmHeapBase(LWasmHeapBase*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNegF(LNegF*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmCompareAndSelect(LWasmCompareAndSelect*) {
  MOZ_CRASH();
}
void CodeGenerator::visitTestIAndBranch(LTestIAndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitDouble(LDouble*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWrapInt64ToInt32(LWrapInt64ToInt32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNegI(LNegI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitBox(LBox*) {
  MOZ_CRASH();
}
void CodeGenerator::visitEffectiveAddress(LEffectiveAddress*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReinterpret(LWasmReinterpret*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCtzI(LCtzI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNegD(LNegD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitBitOpI(LBitOpI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReinterpretToI64(LWasmReinterpretToI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareDAndBranch(LCompareDAndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitClzI(LClzI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitTruncateDToInt32(LTruncateDToInt32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitUrshD(LUrshD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitMathD(LMathD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNotF(LNotF*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicExchangeTypedArrayElement(
    LAtomicExchangeTypedArrayElement*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAsmJSLoadHeap(LAsmJSLoadHeap*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmConstantShiftSimd128(LWasmConstantShiftSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmPermuteSimd128(LWasmPermuteSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmStackArgI64(LWasmStackArgI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmSignReplicationSimd128(
    LWasmSignReplicationSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNotI64(LNotI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAddI(LAddI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitPowHalfD(LPowHalfD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitRotateI64(LRotateI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitMathF(LMathF*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmBuiltinTruncateFToInt32(
    LWasmBuiltinTruncateFToInt32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmCompareExchangeHeap(LWasmCompareExchangeHeap*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmAtomicBinopHeapForEffect(
    LWasmAtomicBinopHeapForEffect*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmExtendU32Index(LWasmExtendU32Index*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmLoadI64(LWasmLoadI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitTestFAndBranch(LTestFAndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCtzI64(LCtzI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitCompareAndBranch(LCompareAndBranch*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicExchangeTypedArrayElement64(
    LAtomicExchangeTypedArrayElement64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicLoad64(LAtomicLoad64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmBinarySimd128WithConstant(
    LWasmBinarySimd128WithConstant*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReduceSimd128ToInt64(LWasmReduceSimd128ToInt64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitModPowTwoI(LModPowTwoI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitMulI(LMulI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmUint32ToDouble(LWasmUint32ToDouble*) {
  MOZ_CRASH();
}
void CodeGenerator::visitDivPowTwoI(LDivPowTwoI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNegI64(LNegI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmStackArg(LWasmStackArg*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReduceAndBranchSimd128(
    LWasmReduceAndBranchSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmLoadLaneSimd128(LWasmLoadLaneSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitUnbox(LUnbox*) {
  MOZ_CRASH();
}
void CodeGenerator::visitModI(LModI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmBinarySimd128(LWasmBinarySimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitDivI(LDivI*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmUint32ToFloat32(LWasmUint32ToFloat32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmStoreLaneSimd128(LWasmStoreLaneSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmVariableShiftSimd128(LWasmVariableShiftSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmScalarToSimd128(LWasmScalarToSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmUnarySimd128(LWasmUnarySimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmTernarySimd128(LWasmTernarySimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitShiftI64(LShiftI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitNearbyIntF(LNearbyIntF*) {
  MOZ_CRASH();
}
void CodeGenerator::visitFloat32(LFloat32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmInt64ToSimd128(LWasmInt64ToSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitMinMaxD(LMinMaxD*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmAddOffset64(LWasmAddOffset64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmStore(LWasmStore*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmShuffleSimd128(LWasmShuffleSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReduceSimd128(LWasmReduceSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitBitNotI64(LBitNotI64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReplaceInt64LaneSimd128(
    LWasmReplaceInt64LaneSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitAtomicStore64(LAtomicStore64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitMemoryBarrier(LMemoryBarrier*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmBuiltinTruncateDToInt32(
    LWasmBuiltinTruncateDToInt32*) {
  MOZ_CRASH();
}
void CodeGenerator::visitExtendInt32ToInt64(LExtendInt32ToInt64*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap*) {
  MOZ_CRASH();
}
void CodeGenerator::visitWasmReplaceLaneSimd128(LWasmReplaceLaneSimd128*) {
  MOZ_CRASH();
}
void CodeGenerator::visitSubI64(LSubI64*) {
  MOZ_CRASH();
}
}  // namespace jit
}  // namespace js