/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x86/CodeGenerator-x86.h"

#include "mozilla/DebugOnly.h"

#include "jsnum.h"

#include "jit/IonCaches.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/Shape.h"

#include "jsscriptinlines.h"

#include "jit/ExecutionMode-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::FloatingPoint;
using JS::GenericNaN;

CodeGeneratorX86::CodeGeneratorX86(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorX86Shared(gen, graph, masm)
{
}

static const uint32_t FrameSizes[] = { 128, 256, 512, 1024 };

FrameSizeClass
FrameSizeClass::FromDepth(uint32_t frameDepth)
{
    for (uint32_t i = 0; i < JS_ARRAY_LENGTH(FrameSizes); i++) {
        if (frameDepth < FrameSizes[i])
            return FrameSizeClass(i);
    }

    return FrameSizeClass::None();
}

FrameSizeClass
FrameSizeClass::ClassLimit()
{
    return FrameSizeClass(JS_ARRAY_LENGTH(FrameSizes));
}

uint32_t
FrameSizeClass::frameSize() const
{
    JS_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);
    JS_ASSERT(class_ < JS_ARRAY_LENGTH(FrameSizes));

    return FrameSizes[class_];
}

ValueOperand
CodeGeneratorX86::ToValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorX86::ToOutValue(LInstruction *ins)
{
    Register typeReg = ToRegister(ins->getDef(TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getDef(PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorX86::ToTempValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getTemp(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getTemp(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

bool
CodeGeneratorX86::visitValue(LValue *value)
{
    const ValueOperand out = ToOutValue(value);
    masm.moveValue(value->value(), out);
    return true;
}

bool
CodeGeneratorX86::visitBox(LBox *box)
{
    const LDefinition *type = box->getDef(TYPE_INDEX);

    DebugOnly<const LAllocation *> a = box->getOperand(0);
    JS_ASSERT(!a->isConstant());

    // On x86, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    masm.mov(ImmWord(MIRTypeToTag(box->type())), ToRegister(type));
    return true;
}

bool
CodeGeneratorX86::visitBoxFloatingPoint(LBoxFloatingPoint *box)
{
    const LAllocation *in = box->getOperand(0);
    const ValueOperand out = ToOutValue(box);

    FloatRegister reg = ToFloatRegister(in);
    if (box->type() == MIRType_Float32) {
        masm.convertFloat32ToDouble(reg, ScratchFloatReg);
        reg = ScratchFloatReg;
    }
    masm.boxDouble(reg, out);
    return true;
}

bool
CodeGeneratorX86::visitUnbox(LUnbox *unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox *mir = unbox->mir();

    if (mir->fallible()) {
        masm.cmpl(ToOperand(unbox->type()), Imm32(MIRTypeToTag(mir->type())));
        if (!bailoutIf(Assembler::NotEqual, unbox->snapshot()))
            return false;
    }
    return true;
}

bool
CodeGeneratorX86::visitCompareB(LCompareB *lir)
{
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation *rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    Label notBoolean, done;
    masm.branchTestBoolean(Assembler::NotEqual, lhs, &notBoolean);
    {
        if (rhs->isConstant())
            masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
        else
            masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
        masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
        masm.jump(&done);
    }
    masm.bind(&notBoolean);
    {
        masm.move32(Imm32(mir->jsop() == JSOP_STRICTNE), output);
    }

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorX86::visitCompareBAndBranch(LCompareBAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation *rhs = lir->rhs();

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    Assembler::Condition cond = masm.testBoolean(Assembler::NotEqual, lhs);
    jumpToBlock((mir->jsop() == JSOP_STRICTEQ) ? lir->ifFalse() : lir->ifTrue(), cond);

    if (rhs->isConstant())
        masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
    else
        masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
    return true;
}

bool
CodeGeneratorX86::visitCompareV(LCompareV *lir)
{
    MCompare *mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    JS_ASSERT(IsEqualityOp(mir->jsop()));

    Label notEqual, done;
    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    masm.j(Assembler::NotEqual, &notEqual);
    {
        masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
        masm.emitSet(cond, output);
        masm.jump(&done);
    }
    masm.bind(&notEqual);
    {
        masm.move32(Imm32(cond == Assembler::NotEqual), output);
    }

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorX86::visitCompareVAndBranch(LCompareVAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    JS_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
              mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock *notEqual = (cond == Assembler::Equal) ? lir->ifFalse() : lir->ifTrue();

    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    jumpToBlock(notEqual, Assembler::NotEqual);
    masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
    emitBranch(cond, lir->ifTrue(), lir->ifFalse());

    return true;
}

bool
CodeGeneratorX86::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble *lir)
{
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());

    if (input != temp)
        masm.mov(input, temp);

    // Beware: convertUInt32ToDouble clobbers input.
    masm.convertUInt32ToDouble(temp, ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGeneratorX86::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32 *lir)
{
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());
    FloatRegister output = ToFloatRegister(lir->output());

    if (input != temp)
        masm.mov(input, temp);

    // Beware: convertUInt32ToFloat32 clobbers input.
    masm.convertUInt32ToFloat32(temp, output);
    return true;
}

// Load a NaN or zero into a register for an out of bounds AsmJS or static
// typed array load.
class jit::OutOfLineLoadTypedArrayOutOfBounds : public OutOfLineCodeBase<CodeGeneratorX86>
{
    AnyRegister dest_;
    bool isFloat32Load_;
  public:
    OutOfLineLoadTypedArrayOutOfBounds(AnyRegister dest, bool isFloat32Load)
      : dest_(dest), isFloat32Load_(isFloat32Load)
    {}

    AnyRegister dest() const { return dest_; }
    bool isFloat32Load() const { return isFloat32Load_; }
    bool accept(CodeGeneratorX86 *codegen) { return codegen->visitOutOfLineLoadTypedArrayOutOfBounds(this); }
};

template<typename T>
void
CodeGeneratorX86::loadViewTypeElement(ArrayBufferView::ViewType vt, const T &srcAddr,
                                      const LDefinition *out)
{
    switch (vt) {
      case ArrayBufferView::TYPE_INT8:    masm.movsblWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT8_CLAMPED:
      case ArrayBufferView::TYPE_UINT8:   masm.movzblWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_INT16:   masm.movswlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT16:  masm.movzwlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT32:  masm.movlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_FLOAT32: masm.movssWithPatch(srcAddr, ToFloatRegister(out)); break;
      case ArrayBufferView::TYPE_FLOAT64: masm.movsdWithPatch(srcAddr, ToFloatRegister(out)); break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }
}

template<typename T>
bool
CodeGeneratorX86::loadAndNoteViewTypeElement(ArrayBufferView::ViewType vt, const T &srcAddr,
                                             const LDefinition *out)
{
    uint32_t before = masm.size();
    loadViewTypeElement(vt, srcAddr, out);
    uint32_t after = masm.size();
    return masm.append(AsmJSHeapAccess(before, after, vt, ToAnyRegister(out)));
}

bool
CodeGeneratorX86::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins)
{
    const MLoadTypedArrayElementStatic *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();
    JS_ASSERT_IF(vt == ArrayBufferView::TYPE_FLOAT32, mir->type() == MIRType_Float32);

    Register ptr = ToRegister(ins->ptr());
    const LDefinition *out = ins->output();

    OutOfLineLoadTypedArrayOutOfBounds *ool = nullptr;
    bool isFloat32Load = (vt == ArrayBufferView::TYPE_FLOAT32);
    if (!mir->fallible()) {
        ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), isFloat32Load);
        if (!addOutOfLineCode(ool))
            return false;
    }

    masm.cmpl(ptr, Imm32(mir->length()));
    if (ool)
        masm.j(Assembler::AboveOrEqual, ool->entry());
    else if (!bailoutIf(Assembler::AboveOrEqual, ins->snapshot()))
        return false;

    Address srcAddr(ptr, (int32_t) mir->base());
    loadViewTypeElement(vt, srcAddr, out);
    if (vt == ArrayBufferView::TYPE_FLOAT64)
        masm.canonicalizeDouble(ToFloatRegister(out));
    if (vt == ArrayBufferView::TYPE_FLOAT32)
        masm.canonicalizeFloat(ToFloatRegister(out));
    if (ool)
        masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    const MAsmJSLoadHeap *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();
    const LAllocation *ptr = ins->ptr();
    const LDefinition *out = ins->output();

    if (ptr->isConstant()) {
        // The constant displacement still needs to be added to the as-yet-unknown
        // base address of the heap. For now, embed the displacement as an
        // immediate in the instruction. This displacement will fixed up when the
        // base address is known during dynamic linking (AsmJSModule::initHeap).
        PatchedAbsoluteAddress srcAddr((void *) ptr->toConstant()->toInt32());
        return loadAndNoteViewTypeElement(vt, srcAddr, out);
    }

    Register ptrReg = ToRegister(ptr);
    Address srcAddr(ptrReg, 0);

    if (mir->skipBoundsCheck())
        return loadAndNoteViewTypeElement(vt, srcAddr, out);

    bool isFloat32Load = vt == ArrayBufferView::TYPE_FLOAT32;
    OutOfLineLoadTypedArrayOutOfBounds *ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), isFloat32Load);
    if (!addOutOfLineCode(ool))
        return false;

    CodeOffsetLabel cmp = masm.cmplWithPatch(ptrReg, Imm32(0));
    masm.j(Assembler::AboveOrEqual, ool->entry());

    uint32_t before = masm.size();
    loadViewTypeElement(vt, srcAddr, out);
    uint32_t after = masm.size();
    masm.bind(ool->rejoin());
    return masm.append(AsmJSHeapAccess(before, after, vt, ToAnyRegister(out), cmp.offset()));
}

bool
CodeGeneratorX86::visitOutOfLineLoadTypedArrayOutOfBounds(OutOfLineLoadTypedArrayOutOfBounds *ool)
{
    if (ool->dest().isFloat()) {
        if (ool->isFloat32Load())
            masm.loadConstantFloat32(float(GenericNaN()), ool->dest().fpu());
        else
            masm.loadConstantDouble(GenericNaN(), ool->dest().fpu());
    } else {
        Register destReg = ool->dest().gpr();
        masm.mov(ImmWord(0), destReg);
    }
    masm.jmp(ool->rejoin());
    return true;
}

template<typename T>
void
CodeGeneratorX86::storeViewTypeElement(ArrayBufferView::ViewType vt, const LAllocation *value,
                                       const T &dstAddr)
{
    switch (vt) {
      case ArrayBufferView::TYPE_INT8:
      case ArrayBufferView::TYPE_UINT8_CLAMPED:
      case ArrayBufferView::TYPE_UINT8:   masm.movbWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_INT16:
      case ArrayBufferView::TYPE_UINT16:  masm.movwWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT32:  masm.movlWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_FLOAT32: masm.movssWithPatch(ToFloatRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_FLOAT64: masm.movsdWithPatch(ToFloatRegister(value), dstAddr); break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }
}

template<typename T>
bool
CodeGeneratorX86::storeAndNoteViewTypeElement(ArrayBufferView::ViewType vt, const LAllocation *value,
                                              const T &dstAddr)
{
    uint32_t before = masm.size();
    storeViewTypeElement(vt, value, dstAddr);
    uint32_t after = masm.size();
    return masm.append(AsmJSHeapAccess(before, after));
}

bool
CodeGeneratorX86::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins)
{
    MStoreTypedArrayElementStatic *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Register ptr = ToRegister(ins->ptr());
    const LAllocation *value = ins->value();

    masm.cmpl(ptr, Imm32(mir->length()));
    Label rejoin;
    masm.j(Assembler::AboveOrEqual, &rejoin);

    Address dstAddr(ptr, (int32_t) mir->base());
    storeViewTypeElement(vt, value, dstAddr);
    masm.bind(&rejoin);
    return true;
}

bool
CodeGeneratorX86::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    MAsmJSStoreHeap *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();
    const LAllocation *value = ins->value();
    const LAllocation *ptr = ins->ptr();

    if (ptr->isConstant()) {
        // The constant displacement still needs to be added to the as-yet-unknown
        // base address of the heap. For now, embed the displacement as an
        // immediate in the instruction. This displacement will fixed up when the
        // base address is known during dynamic linking (AsmJSModule::initHeap).
        PatchedAbsoluteAddress dstAddr((void *) ptr->toConstant()->toInt32());
        return storeAndNoteViewTypeElement(vt, value, dstAddr);
    }

    Register ptrReg = ToRegister(ptr);
    Address dstAddr(ptrReg, 0);

    if (mir->skipBoundsCheck())
        return storeAndNoteViewTypeElement(vt, value, dstAddr);

    CodeOffsetLabel cmp = masm.cmplWithPatch(ptrReg, Imm32(0));
    Label rejoin;
    masm.j(Assembler::AboveOrEqual, &rejoin);

    uint32_t before = masm.size();
    storeViewTypeElement(vt, value, dstAddr);
    uint32_t after = masm.size();
    masm.bind(&rejoin);
    return masm.append(AsmJSHeapAccess(before, after, cmp.offset()));
}

bool
CodeGeneratorX86::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins)
{
    MAsmJSLoadGlobalVar *mir = ins->mir();
    MIRType type = mir->type();
    JS_ASSERT(IsNumberType(type));

    CodeOffsetLabel label;
    if (type == MIRType_Int32)
        label = masm.movlWithPatch(PatchedAbsoluteAddress(), ToRegister(ins->output()));
    else if (type == MIRType_Float32)
        label = masm.movssWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
    else
        label = masm.movsdWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));

    return masm.append(AsmJSGlobalAccess(CodeOffsetLabel(label.offset()), mir->globalDataOffset()));
}

bool
CodeGeneratorX86::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    JS_ASSERT(IsNumberType(type));

    CodeOffsetLabel label;
    if (type == MIRType_Int32)
        label = masm.movlWithPatch(ToRegister(ins->value()), PatchedAbsoluteAddress());
    else if (type == MIRType_Float32)
        label = masm.movssWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
    else
        label = masm.movsdWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());

    return masm.append(AsmJSGlobalAccess(CodeOffsetLabel(label.offset()), mir->globalDataOffset()));
}

bool
CodeGeneratorX86::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins)
{
    MAsmJSLoadFuncPtr *mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register out = ToRegister(ins->output());
    CodeOffsetLabel label = masm.movlWithPatch(PatchedAbsoluteAddress(), index, TimesFour, out);

    return masm.append(AsmJSGlobalAccess(CodeOffsetLabel(label.offset()), mir->globalDataOffset()));
}

bool
CodeGeneratorX86::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    MAsmJSLoadFFIFunc *mir = ins->mir();

    Register out = ToRegister(ins->output());
    CodeOffsetLabel label = masm.movlWithPatch(PatchedAbsoluteAddress(), out);

    return masm.append(AsmJSGlobalAccess(CodeOffsetLabel(label.offset()), mir->globalDataOffset()));
}

void
CodeGeneratorX86::postAsmJSCall(LAsmJSCall *lir)
{
    MAsmJSCall *mir = lir->mir();
    if (!IsFloatingPointType(mir->type()) || mir->callee().which() != MAsmJSCall::Callee::Builtin)
        return;

    if (mir->type() == MIRType_Float32) {
        masm.reserveStack(sizeof(float));
        Operand op(esp, 0);
        masm.fstp32(op);
        masm.loadFloat32(op, ReturnFloatReg);
        masm.freeStack(sizeof(float));
    } else {
        masm.reserveStack(sizeof(double));
        Operand op(esp, 0);
        masm.fstp(op);
        masm.loadDouble(op, ReturnFloatReg);
        masm.freeStack(sizeof(double));
    }
}

void
DispatchIonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // On x86, where there is no general purpose scratch register available,
    // child cache classes must manually specify a dispatch scratch register.
    MOZ_ASSUME_UNREACHABLE("x86 needs manual assignment of dispatchScratch");
}

void
GetPropertyParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have a scratch register, but only use the temp if we needed
    // one, it's BogusTemp otherwise.
    JS_ASSERT(ins->isGetPropertyCacheV() || ins->isGetPropertyCacheT());
    if (ins->isGetPropertyCacheV() || ins->toGetPropertyCacheT()->temp()->isBogusTemp())
        addState->dispatchScratch = output_.scratchReg().gpr();
    else
        addState->dispatchScratch = ToRegister(ins->toGetPropertyCacheT()->temp());
}

void
GetElementParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have a scratch register, but only use the temp if we needed
    // one, it's BogusTemp otherwise.
    JS_ASSERT(ins->isGetElementCacheV() || ins->isGetElementCacheT());
    if (ins->isGetElementCacheV() || ins->toGetElementCacheT()->temp()->isBogusTemp())
        addState->dispatchScratch = output_.scratchReg().gpr();
    else
        addState->dispatchScratch = ToRegister(ins->toGetElementCacheT()->temp());
}

void
SetPropertyParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have an output register to reuse, so we always need a temp.
    JS_ASSERT(ins->isSetPropertyCacheV() || ins->isSetPropertyCacheT());
    if (ins->isSetPropertyCacheV())
        addState->dispatchScratch = ToRegister(ins->toSetPropertyCacheV()->tempForDispatchCache());
    else
        addState->dispatchScratch = ToRegister(ins->toSetPropertyCacheT()->tempForDispatchCache());
}

void
SetElementParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have an output register to reuse, but luckily SetElementCache
    // already needs a temp.
    JS_ASSERT(ins->isSetElementCacheV() || ins->isSetElementCacheT());
    if (ins->isSetElementCacheV())
        addState->dispatchScratch = ToRegister(ins->toSetElementCacheV()->temp());
    else
        addState->dispatchScratch = ToRegister(ins->toSetElementCacheT()->temp());
}

namespace js {
namespace jit {

class OutOfLineTruncate : public OutOfLineCodeBase<CodeGeneratorX86>
{
    LTruncateDToInt32 *ins_;

  public:
    OutOfLineTruncate(LTruncateDToInt32 *ins)
      : ins_(ins)
    { }

    bool accept(CodeGeneratorX86 *codegen) {
        return codegen->visitOutOfLineTruncate(this);
    }
    LTruncateDToInt32 *ins() const {
        return ins_;
    }
};

class OutOfLineTruncateFloat32 : public OutOfLineCodeBase<CodeGeneratorX86>
{
    LTruncateFToInt32 *ins_;

  public:
    OutOfLineTruncateFloat32(LTruncateFToInt32 *ins)
      : ins_(ins)
    { }

    bool accept(CodeGeneratorX86 *codegen) {
        return codegen->visitOutOfLineTruncateFloat32(this);
    }
    LTruncateFToInt32 *ins() const {
        return ins_;
    }
};

} // namespace jit
} // namespace js

bool
CodeGeneratorX86::visitTruncateDToInt32(LTruncateDToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    OutOfLineTruncate *ool = new(alloc()) OutOfLineTruncate(ins);
    if (!addOutOfLineCode(ool))
        return false;

    masm.branchTruncateDouble(input, output, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitTruncateFToInt32(LTruncateFToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    OutOfLineTruncateFloat32 *ool = new(alloc()) OutOfLineTruncateFloat32(ins);
    if (!addOutOfLineCode(ool))
        return false;

    masm.branchTruncateFloat32(input, output, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitOutOfLineTruncate(OutOfLineTruncate *ool)
{
    LTruncateDToInt32 *ins = ool->ins();
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    Label fail;

    if (Assembler::HasSSE3()) {
        // Push double.
        masm.subl(Imm32(sizeof(double)), esp);
        masm.storeDouble(input, Operand(esp, 0));

        static const uint32_t EXPONENT_MASK = 0x7ff00000;
        static const uint32_t EXPONENT_SHIFT = FloatingPoint<double>::kExponentShift - 32;
        static const uint32_t TOO_BIG_EXPONENT = (FloatingPoint<double>::kExponentBias + 63)
                                                 << EXPONENT_SHIFT;

        // Check exponent to avoid fp exceptions.
        Label failPopDouble;
        masm.load32(Address(esp, 4), output);
        masm.and32(Imm32(EXPONENT_MASK), output);
        masm.branch32(Assembler::GreaterThanOrEqual, output, Imm32(TOO_BIG_EXPONENT), &failPopDouble);

        // Load double, perform 64-bit truncation.
        masm.fld(Operand(esp, 0));
        masm.fisttp(Operand(esp, 0));

        // Load low word, pop double and jump back.
        masm.load32(Address(esp, 0), output);
        masm.addl(Imm32(sizeof(double)), esp);
        masm.jump(ool->rejoin());

        masm.bind(&failPopDouble);
        masm.addl(Imm32(sizeof(double)), esp);
        masm.jump(&fail);
    } else {
        FloatRegister temp = ToFloatRegister(ins->tempFloat());

        // Try to convert doubles representing integers within 2^32 of a signed
        // integer, by adding/subtracting 2^32 and then trying to convert to int32.
        // This has to be an exact conversion, as otherwise the truncation works
        // incorrectly on the modified value.
        masm.xorpd(ScratchFloatReg, ScratchFloatReg);
        masm.ucomisd(input, ScratchFloatReg);
        masm.j(Assembler::Parity, &fail);

        {
            Label positive;
            masm.j(Assembler::Above, &positive);

            masm.loadConstantDouble(4294967296.0, temp);
            Label skip;
            masm.jmp(&skip);

            masm.bind(&positive);
            masm.loadConstantDouble(-4294967296.0, temp);
            masm.bind(&skip);
        }

        masm.addsd(input, temp);
        masm.cvttsd2si(temp, output);
        masm.cvtsi2sd(output, ScratchFloatReg);

        masm.ucomisd(temp, ScratchFloatReg);
        masm.j(Assembler::Parity, &fail);
        masm.j(Assembler::Equal, ool->rejoin());
    }

    masm.bind(&fail);
    {
        saveVolatile(output);

        masm.setupUnalignedABICall(1, output);
        masm.passABIArg(input, MoveOp::DOUBLE);
        if (gen->compilingAsmJS())
            masm.callWithABI(AsmJSImm_ToInt32);
        else
            masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ToInt32));
        masm.storeCallResult(output);

        restoreVolatile(output);
    }

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitOutOfLineTruncateFloat32(OutOfLineTruncateFloat32 *ool)
{
    LTruncateFToInt32 *ins = ool->ins();
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    Label fail;

    if (Assembler::HasSSE3()) {
        // Push float32, but subtracts 64 bits so that the value popped by fisttp fits
        masm.subl(Imm32(sizeof(uint64_t)), esp);
        masm.storeFloat32(input, Operand(esp, 0));

        static const uint32_t EXPONENT_MASK = FloatingPoint<float>::kExponentBits;
        static const uint32_t EXPONENT_SHIFT = FloatingPoint<float>::kExponentShift;
        // Integers are still 64 bits long, so we can still test for an exponent > 63.
        static const uint32_t TOO_BIG_EXPONENT = (FloatingPoint<float>::kExponentBias + 63)
                                                 << EXPONENT_SHIFT;

        // Check exponent to avoid fp exceptions.
        Label failPopFloat;
        masm.movl(Operand(esp, 0), output);
        masm.and32(Imm32(EXPONENT_MASK), output);
        masm.branch32(Assembler::GreaterThanOrEqual, output, Imm32(TOO_BIG_EXPONENT), &failPopFloat);

        // Load float, perform 32-bit truncation.
        masm.fld32(Operand(esp, 0));
        masm.fisttp(Operand(esp, 0));

        // Load low word, pop 64bits and jump back.
        masm.movl(Operand(esp, 0), output);
        masm.addl(Imm32(sizeof(uint64_t)), esp);
        masm.jump(ool->rejoin());

        masm.bind(&failPopFloat);
        masm.addl(Imm32(sizeof(uint64_t)), esp);
        masm.jump(&fail);
    } else {
        FloatRegister temp = ToFloatRegister(ins->tempFloat());

        // Try to convert float32 representing integers within 2^32 of a signed
        // integer, by adding/subtracting 2^32 and then trying to convert to int32.
        // This has to be an exact conversion, as otherwise the truncation works
        // incorrectly on the modified value.
        masm.xorps(ScratchFloatReg, ScratchFloatReg);
        masm.ucomiss(input, ScratchFloatReg);
        masm.j(Assembler::Parity, &fail);

        {
            Label positive;
            masm.j(Assembler::Above, &positive);

            masm.loadConstantFloat32(4294967296.f, temp);
            Label skip;
            masm.jmp(&skip);

            masm.bind(&positive);
            masm.loadConstantFloat32(-4294967296.f, temp);
            masm.bind(&skip);
        }

        masm.addss(input, temp);
        masm.cvttss2si(temp, output);
        masm.cvtsi2ss(output, ScratchFloatReg);

        masm.ucomiss(temp, ScratchFloatReg);
        masm.j(Assembler::Parity, &fail);
        masm.j(Assembler::Equal, ool->rejoin());
    }

    masm.bind(&fail);
    {
        saveVolatile(output);

        masm.push(input);
        masm.setupUnalignedABICall(1, output);
        masm.cvtss2sd(input, input);
        masm.passABIArg(input, MoveOp::DOUBLE);

        if (gen->compilingAsmJS())
            masm.callWithABI(AsmJSImm_ToInt32);
        else
            masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ToInt32));

        masm.storeCallResult(output);
        masm.pop(input);

        restoreVolatile(output);
    }

    masm.jump(ool->rejoin());
    return true;
}
