/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonCaches.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/TemplateLib.h"

#include "jsproxy.h"
#include "jstypes.h"

#include "builtin/TypeRepresentation.h"
#include "jit/Ion.h"
#include "jit/IonLinker.h"
#include "jit/IonSpewer.h"
#include "jit/Lowering.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/ParallelFunctions.h"
#include "jit/VMFunctions.h"
#include "vm/Shape.h"

#include "jit/IonFrames-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Shape-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::tl::FloorLog2;

void
CodeLocationJump::repoint(JitCode *code, MacroAssembler *masm)
{
    JS_ASSERT(state_ == Relative);
    size_t new_off = (size_t)raw_;
#ifdef JS_SMALL_BRANCH
    size_t jumpTableEntryOffset = reinterpret_cast<size_t>(jumpTableEntry_);
#endif
    if (masm != nullptr) {
#ifdef JS_CODEGEN_X64
        JS_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = masm->actualOffset((uintptr_t)raw_);
#ifdef JS_SMALL_BRANCH
        jumpTableEntryOffset = masm->actualIndex(jumpTableEntryOffset);
#endif
    }
    raw_ = code->raw() + new_off;
#ifdef JS_SMALL_BRANCH
    jumpTableEntry_ = Assembler::PatchableJumpAddress(code, (size_t) jumpTableEntryOffset);
#endif
    setAbsolute();
}

void
CodeLocationLabel::repoint(JitCode *code, MacroAssembler *masm)
{
     JS_ASSERT(state_ == Relative);
     size_t new_off = (size_t)raw_;
     if (masm != nullptr) {
#ifdef JS_CODEGEN_X64
        JS_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = masm->actualOffset((uintptr_t)raw_);
     }
     JS_ASSERT(new_off < code->instructionsSize());

     raw_ = code->raw() + new_off;
     setAbsolute();
}

void
CodeOffsetLabel::fixup(MacroAssembler *masm)
{
     offset_ = masm->actualOffset(offset_);
}

void
CodeOffsetJump::fixup(MacroAssembler *masm)
{
     offset_ = masm->actualOffset(offset_);
#ifdef JS_SMALL_BRANCH
     jumpTableIndex_ = masm->actualIndex(jumpTableIndex_);
#endif
}

const char *
IonCache::CacheName(IonCache::Kind kind)
{
    static const char * const names[] =
    {
#define NAME(x) #x,
        IONCACHE_KIND_LIST(NAME)
#undef NAME
    };
    return names[kind];
}

IonCache::LinkStatus
IonCache::linkCode(JSContext *cx, MacroAssembler &masm, IonScript *ion, JitCode **code)
{
    Linker linker(masm);
    *code = linker.newCode<CanGC>(cx, JSC::ION_CODE);
    if (!*code)
        return LINK_ERROR;

    if (ion->invalidated())
        return CACHE_FLUSHED;

    return LINK_GOOD;
}

const size_t IonCache::MAX_STUBS = 16;

// Helper class which encapsulates logic to attach a stub to an IC by hooking
// up rejoins and next stub jumps.
//
// The simplest stubs have a single jump to the next stub and look like the
// following:
//
//    branch guard NEXTSTUB
//    ... IC-specific code ...
//    jump REJOIN
//
// This corresponds to:
//
//    attacher.branchNextStub(masm, ...);
//    ... emit IC-specific code ...
//    attacher.jumpRejoin(masm);
//
// Whether the stub needs multiple next stub jumps look like:
//
//   branch guard FAILURES
//   ... IC-specific code ...
//   branch another-guard FAILURES
//   ... IC-specific code ...
//   jump REJOIN
//   FAILURES:
//   jump NEXTSTUB
//
// This corresponds to:
//
//   Label failures;
//   masm.branchX(..., &failures);
//   ... emit IC-specific code ...
//   masm.branchY(..., failures);
//   ... emit more IC-specific code ...
//   attacher.jumpRejoin(masm);
//   masm.bind(&failures);
//   attacher.jumpNextStub(masm);
//
// A convenience function |branchNextStubOrLabel| is provided in the case that
// the stub sometimes has multiple next stub jumps and sometimes a single
// one. If a non-nullptr label is passed in, a |branchPtr| will be made to
// that label instead of a |branchPtrWithPatch| to the next stub.
class IonCache::StubAttacher
{
  protected:
    bool hasNextStubOffset_ : 1;
    bool hasStubCodePatchOffset_ : 1;

    CodeLocationLabel rejoinLabel_;
    CodeOffsetJump nextStubOffset_;
    CodeOffsetJump rejoinOffset_;
    CodeOffsetLabel stubCodePatchOffset_;

  public:
    StubAttacher(CodeLocationLabel rejoinLabel)
      : hasNextStubOffset_(false),
        hasStubCodePatchOffset_(false),
        rejoinLabel_(rejoinLabel),
        nextStubOffset_(),
        rejoinOffset_(),
        stubCodePatchOffset_()
    { }

    // Value used instead of the JitCode self-reference of generated
    // stubs. This value is needed for marking calls made inside stubs. This
    // value would be replaced by the attachStub function after the allocation
    // of the JitCode. The self-reference is used to keep the stub path alive
    // even if the IonScript is invalidated or if the IC is flushed.
    static const ImmPtr STUB_ADDR;

    template <class T1, class T2>
    void branchNextStub(MacroAssembler &masm, Assembler::Condition cond, T1 op1, T2 op2) {
        JS_ASSERT(!hasNextStubOffset_);
        RepatchLabel nextStub;
        nextStubOffset_ = masm.branchPtrWithPatch(cond, op1, op2, &nextStub);
        hasNextStubOffset_ = true;
        masm.bind(&nextStub);
    }

    template <class T1, class T2>
    void branchNextStubOrLabel(MacroAssembler &masm, Assembler::Condition cond, T1 op1, T2 op2,
                               Label *label)
    {
        if (label != nullptr)
            masm.branchPtr(cond, op1, op2, label);
        else
            branchNextStub(masm, cond, op1, op2);
    }

    void jumpRejoin(MacroAssembler &masm) {
        RepatchLabel rejoin;
        rejoinOffset_ = masm.jumpWithPatch(&rejoin);
        masm.bind(&rejoin);
    }

    void jumpNextStub(MacroAssembler &masm) {
        JS_ASSERT(!hasNextStubOffset_);
        RepatchLabel nextStub;
        nextStubOffset_ = masm.jumpWithPatch(&nextStub);
        hasNextStubOffset_ = true;
        masm.bind(&nextStub);
    }

    void pushStubCodePointer(MacroAssembler &masm) {
        // Push the JitCode pointer for the stub we're generating.
        // WARNING:
        // WARNING: If JitCode ever becomes relocatable, the following code is incorrect.
        // WARNING: Note that we're not marking the pointer being pushed as an ImmGCPtr.
        // WARNING: This location will be patched with the pointer of the generated stub,
        // WARNING: such as it can be marked when a call is made with this stub. Be aware
        // WARNING: that ICs are not marked and so this stub will only be kept alive iff
        // WARNING: it is on the stack at the time of the GC. No ImmGCPtr is needed as the
        // WARNING: stubs are flushed on GC.
        // WARNING:
        JS_ASSERT(!hasStubCodePatchOffset_);
        stubCodePatchOffset_ = masm.PushWithPatch(STUB_ADDR);
        hasStubCodePatchOffset_ = true;
    }

    void patchRejoinJump(MacroAssembler &masm, JitCode *code) {
        rejoinOffset_.fixup(&masm);
        CodeLocationJump rejoinJump(code, rejoinOffset_);
        PatchJump(rejoinJump, rejoinLabel_);
    }

    void patchStubCodePointer(MacroAssembler &masm, JitCode *code) {
        if (hasStubCodePatchOffset_) {
            stubCodePatchOffset_.fixup(&masm);
            Assembler::patchDataWithValueCheck(CodeLocationLabel(code, stubCodePatchOffset_),
                                               ImmPtr(code), STUB_ADDR);
        }
    }

    virtual void patchNextStubJump(MacroAssembler &masm, JitCode *code) = 0;
};

const ImmPtr IonCache::StubAttacher::STUB_ADDR = ImmPtr((void*)0xdeadc0de);

class RepatchIonCache::RepatchStubAppender : public IonCache::StubAttacher
{
    RepatchIonCache &cache_;

  public:
    RepatchStubAppender(RepatchIonCache &cache)
      : StubAttacher(cache.rejoinLabel()),
        cache_(cache)
    {
    }

    void patchNextStubJump(MacroAssembler &masm, JitCode *code) {
        // Patch the previous nextStubJump of the last stub, or the jump from the
        // codeGen, to jump into the newly allocated code.
        PatchJump(cache_.lastJump_, CodeLocationLabel(code));

        // If this path is not taken, we are producing an entry which can no
        // longer go back into the update function.
        if (hasNextStubOffset_) {
            nextStubOffset_.fixup(&masm);
            CodeLocationJump nextStubJump(code, nextStubOffset_);
            PatchJump(nextStubJump, cache_.fallbackLabel_);

            // When the last stub fails, it fallback to the ool call which can
            // produce a stub. Next time we generate a stub, we will patch the
            // nextStub jump to try the new stub.
            cache_.lastJump_ = nextStubJump;
        }
    }
};

void
RepatchIonCache::reset()
{
    IonCache::reset();
    PatchJump(initialJump_, fallbackLabel_);
    lastJump_ = initialJump_;
}

void
RepatchIonCache::emitInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    initialJump_ = masm.jumpWithPatch(&addState.repatchEntry);
    lastJump_ = initialJump_;
}

void
RepatchIonCache::bindInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    masm.bind(&addState.repatchEntry);
}

void
RepatchIonCache::updateBaseAddress(JitCode *code, MacroAssembler &masm)
{
    IonCache::updateBaseAddress(code, masm);
    initialJump_.repoint(code, &masm);
    lastJump_.repoint(code, &masm);
}

class DispatchIonCache::DispatchStubPrepender : public IonCache::StubAttacher
{
    DispatchIonCache &cache_;

  public:
    DispatchStubPrepender(DispatchIonCache &cache)
      : StubAttacher(cache.rejoinLabel_),
        cache_(cache)
    {
    }

    void patchNextStubJump(MacroAssembler &masm, JitCode *code) {
        JS_ASSERT(hasNextStubOffset_);

        // Jump to the previous entry in the stub dispatch table. We
        // have not yet executed the code we're patching the jump in.
        nextStubOffset_.fixup(&masm);
        CodeLocationJump nextStubJump(code, nextStubOffset_);
        PatchJump(nextStubJump, CodeLocationLabel(cache_.firstStub_));

        // Update the dispatch table. Modification to jumps after the dispatch
        // table is updated is disallowed, lest we race on entry into an
        // unfinalized stub.
        cache_.firstStub_ = code->raw();
    }
};

void
DispatchIonCache::reset()
{
    IonCache::reset();
    firstStub_ = fallbackLabel_.raw();
}
void
DispatchIonCache::emitInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    Register scratch = addState.dispatchScratch;
    dispatchLabel_ = masm.movWithPatch(ImmPtr((void*)-1), scratch);
    masm.loadPtr(Address(scratch, 0), scratch);
    masm.jump(scratch);
    rejoinLabel_ = masm.labelForPatch();
}

void
DispatchIonCache::bindInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    // Do nothing.
}

void
DispatchIonCache::updateBaseAddress(JitCode *code, MacroAssembler &masm)
{
    // The address of firstStub_ should be pointer aligned.
    JS_ASSERT(uintptr_t(&firstStub_) % sizeof(uintptr_t) == 0);

    IonCache::updateBaseAddress(code, masm);
    dispatchLabel_.fixup(&masm);
    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, dispatchLabel_),
                                       ImmPtr(&firstStub_),
                                       ImmPtr((void*)-1));
    firstStub_ = fallbackLabel_.raw();
    rejoinLabel_.repoint(code, &masm);
}

void
IonCache::attachStub(MacroAssembler &masm, StubAttacher &attacher, Handle<JitCode *> code)
{
    JS_ASSERT(canAttachStub());
    incrementStubCount();

    // Update the success path to continue after the IC initial jump.
    attacher.patchRejoinJump(masm, code);

    // Update the failure path.
    attacher.patchNextStubJump(masm, code);

    // Replace the STUB_ADDR constant by the address of the generated stub, such
    // as it can be kept alive even if the cache is flushed (see
    // MarkJitExitFrame).
    attacher.patchStubCodePointer(masm, code);
}

bool
IonCache::linkAndAttachStub(JSContext *cx, MacroAssembler &masm, StubAttacher &attacher,
                            IonScript *ion, const char *attachKind)
{
    Rooted<JitCode *> code(cx);
    LinkStatus status = linkCode(cx, masm, ion, code.address());
    if (status != LINK_GOOD)
        return status != LINK_ERROR;

    attachStub(masm, attacher, code);

    if (pc_) {
        IonSpew(IonSpew_InlineCaches, "Cache %p(%s:%d/%d) generated %s %s stub at %p",
                this, script_->filename(), script_->lineno(), script_->pcToOffset(pc_),
                attachKind, CacheName(kind()), code->raw());
    } else {
        IonSpew(IonSpew_InlineCaches, "Cache %p generated %s %s stub at %p",
                this, attachKind, CacheName(kind()), code->raw());
    }

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(code, "IonCache");
#endif

    return true;
}

void
IonCache::updateBaseAddress(JitCode *code, MacroAssembler &masm)
{
    fallbackLabel_.repoint(code, &masm);
}

void
IonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
}

static bool
IsCacheableDOMProxy(JSObject *obj)
{
    if (!obj->is<ProxyObject>())
        return false;

    BaseProxyHandler *handler = obj->as<ProxyObject>().handler();

    if (handler->family() != GetDOMProxyHandlerFamily())
        return false;

    if (obj->numFixedSlots() <= GetDOMProxyExpandoSlot())
        return false;

    return true;
}

static void
GeneratePrototypeGuards(JSContext *cx, IonScript *ion, MacroAssembler &masm, JSObject *obj,
                        JSObject *holder, Register objectReg, Register scratchReg,
                        Label *failures)
{
    /* The guards here protect against the effects of TradeGuts(). If the prototype chain
     * is directly altered, then TI will toss the jitcode, so we don't have to worry about
     * it, and any other change to the holder, or adding a shadowing property will result
     * in reshaping the holder, and thus the failure of the shape guard.
     */
    JS_ASSERT(obj != holder);

    if (obj->hasUncacheableProto()) {
        // Note: objectReg and scratchReg may be the same register, so we cannot
        // use objectReg in the rest of this function.
        masm.loadPtr(Address(objectReg, JSObject::offsetOfType()), scratchReg);
        Address proto(scratchReg, types::TypeObject::offsetOfProto());
        masm.branchNurseryPtr(Assembler::NotEqual, proto,
                              ImmMaybeNurseryPtr(obj->getProto()), failures);
    }

    JSObject *pobj = IsCacheableDOMProxy(obj)
                     ? obj->getTaggedProto().toObjectOrNull()
                     : obj->getProto();
    if (!pobj)
        return;
    while (pobj != holder) {
        if (pobj->hasUncacheableProto()) {
            JS_ASSERT(!pobj->hasSingletonType());
            masm.moveNurseryPtr(ImmMaybeNurseryPtr(pobj), scratchReg);
            Address objType(scratchReg, JSObject::offsetOfType());
            masm.branchPtr(Assembler::NotEqual, objType, ImmGCPtr(pobj->type()), failures);
        }
        pobj = pobj->getProto();
    }
}

static bool
IsCacheableProtoChain(JSObject *obj, JSObject *holder)
{
    while (obj != holder) {
        /*
         * We cannot assume that we find the holder object on the prototype
         * chain and must check for null proto. The prototype chain can be
         * altered during the lookupProperty call.
         */
        JSObject *proto = obj->getProto();
        if (!proto || !proto->isNative())
            return false;
        obj = proto;
    }
    return true;
}

static bool
IsCacheableGetPropReadSlot(JSObject *obj, JSObject *holder, Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (!shape->hasSlot() || !shape->hasDefaultGetter())
        return false;

    return true;
}

static bool
IsCacheableNoProperty(JSObject *obj, JSObject *holder, Shape *shape, jsbytecode *pc,
                      const TypedOrValueRegister &output)
{
    if (shape)
        return false;

    JS_ASSERT(!holder);

    // Just because we didn't find the property on the object doesn't mean it
    // won't magically appear through various engine hacks:
    if (obj->getClass()->getProperty && obj->getClass()->getProperty != JS_PropertyStub)
        return false;

    // Don't generate missing property ICs if we skipped a non-native object, as
    // lookups may extend beyond the prototype chain (e.g.  for DOMProxy
    // proxies).
    JSObject *obj2 = obj;
    while (obj2) {
        if (!obj2->isNative())
            return false;
        obj2 = obj2->getProto();
    }

    // The pc is nullptr if the cache is idempotent. We cannot share missing
    // properties between caches because TI can only try to prove that a type is
    // contained, but does not attempts to check if something does not exists.
    // So the infered type of getprop would be missing and would not contain
    // undefined, as expected for missing properties.
    if (!pc)
        return false;

#if JS_HAS_NO_SUCH_METHOD
    // The __noSuchMethod__ hook may substitute in a valid method.  Since,
    // if o.m is missing, o.m() will probably be an error, just mark all
    // missing callprops as uncacheable.
    if (JSOp(*pc) == JSOP_CALLPROP ||
        JSOp(*pc) == JSOP_CALLELEM)
    {
        return false;
    }
#endif

    // TI has not yet monitored an Undefined value. The fallback path will
    // monitor and invalidate the script.
    if (!output.hasValue())
        return false;

    return true;
}

static bool
IsOptimizableArgumentsObjectForLength(JSObject *obj)
{
    if (!obj->is<ArgumentsObject>())
        return false;

    if (obj->as<ArgumentsObject>().hasOverriddenLength())
        return false;

    return true;
}

static bool
IsOptimizableArgumentsObjectForGetElem(JSObject *obj, Value idval)
{
    if (!IsOptimizableArgumentsObjectForLength(obj))
        return false;

    ArgumentsObject &argsObj = obj->as<ArgumentsObject>();

    if (argsObj.isAnyElementDeleted())
        return false;

    if (!idval.isInt32())
        return false;

    int32_t idint = idval.toInt32();
    if (idint < 0 || static_cast<uint32_t>(idint) >= argsObj.initialLength())
        return false;

    return true;
}

static bool
IsCacheableGetPropCallNative(JSObject *obj, JSObject *holder, Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (!shape->hasGetterValue() || !shape->getterValue().isObject())
        return false;

    if (!shape->getterValue().toObject().is<JSFunction>())
        return false;

    JSFunction& getter = shape->getterValue().toObject().as<JSFunction>();
    if (!getter.isNative())
        return false;

    // Check for a DOM method; those are OK with both inner and outer objects.
    if (getter.jitInfo() && getter.jitInfo()->isDOMJitInfo())
        return true;

    // For non-DOM methods, don't cache if obj has an outerObject hook.
    return !obj->getClass()->ext.outerObject;
}

static bool
IsCacheableGetPropCallPropertyOp(JSObject *obj, JSObject *holder, Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (shape->hasSlot() || shape->hasGetterValue() || shape->hasDefaultGetter())
        return false;

    return true;
}

static inline void
EmitLoadSlot(MacroAssembler &masm, JSObject *holder, Shape *shape, Register holderReg,
             TypedOrValueRegister output, Register scratchReg)
{
    JS_ASSERT(holder);
    if (holder->isFixedSlot(shape->slot())) {
        Address addr(holderReg, JSObject::getFixedSlotOffset(shape->slot()));
        masm.loadTypedOrValue(addr, output);
    } else {
        masm.loadPtr(Address(holderReg, JSObject::offsetOfSlots()), scratchReg);

        Address addr(scratchReg, holder->dynamicSlotIndex(shape->slot()) * sizeof(Value));
        masm.loadTypedOrValue(addr, output);
    }
}

static void
GenerateDOMProxyChecks(JSContext *cx, MacroAssembler &masm, JSObject *obj,
                       PropertyName *name, Register object, Label *stubFailure,
                       bool skipExpandoCheck = false)
{
    JS_ASSERT(IsCacheableDOMProxy(obj));

    // Guard the following:
    //      1. The object is a DOMProxy.
    //      2. The object does not have expando properties, or has an expando
    //          which is known to not have the desired property.
    Address handlerAddr(object, ProxyObject::offsetOfHandler());
    Address expandoSlotAddr(object, JSObject::getFixedSlotOffset(GetDOMProxyExpandoSlot()));

    // Check that object is a DOMProxy.
    masm.branchPrivatePtr(Assembler::NotEqual, handlerAddr,
                          ImmPtr(obj->as<ProxyObject>().handler()), stubFailure);

    if (skipExpandoCheck)
        return;

    // For the remaining code, we need to reserve some registers to load a value.
    // This is ugly, but unvaoidable.
    RegisterSet domProxyRegSet(RegisterSet::All());
    domProxyRegSet.take(AnyRegister(object));
    ValueOperand tempVal = domProxyRegSet.takeValueOperand();
    masm.pushValue(tempVal);

    Label failDOMProxyCheck;
    Label domProxyOk;

    Value expandoVal = obj->getFixedSlot(GetDOMProxyExpandoSlot());
    masm.loadValue(expandoSlotAddr, tempVal);

    if (!expandoVal.isObject() && !expandoVal.isUndefined()) {
        masm.branchTestValue(Assembler::NotEqual, tempVal, expandoVal, &failDOMProxyCheck);

        ExpandoAndGeneration *expandoAndGeneration = (ExpandoAndGeneration*)expandoVal.toPrivate();
        masm.movePtr(ImmPtr(expandoAndGeneration), tempVal.scratchReg());

        masm.branch32(Assembler::NotEqual,
                      Address(tempVal.scratchReg(),
                              ExpandoAndGeneration::offsetOfGeneration()),
                      Imm32(expandoAndGeneration->generation),
                      &failDOMProxyCheck);

        expandoVal = expandoAndGeneration->expando;
        masm.loadValue(Address(tempVal.scratchReg(),
                               ExpandoAndGeneration::offsetOfExpando()),
                       tempVal);
    }

    // If the incoming object does not have an expando object then we're sure we're not
    // shadowing.
    masm.branchTestUndefined(Assembler::Equal, tempVal, &domProxyOk);

    if (expandoVal.isObject()) {
        JS_ASSERT(!expandoVal.toObject().nativeContains(cx, name));

        // Reference object has an expando object that doesn't define the name. Check that
        // the incoming object has an expando object with the same shape.
        masm.branchTestObject(Assembler::NotEqual, tempVal, &failDOMProxyCheck);
        masm.extractObject(tempVal, tempVal.scratchReg());
        masm.branchPtr(Assembler::Equal,
                       Address(tempVal.scratchReg(), JSObject::offsetOfShape()),
                       ImmGCPtr(expandoVal.toObject().lastProperty()),
                       &domProxyOk);
    }

    // Failure case: restore the tempVal registers and jump to failures.
    masm.bind(&failDOMProxyCheck);
    masm.popValue(tempVal);
    masm.jump(stubFailure);

    // Success case: restore the tempval and proceed.
    masm.bind(&domProxyOk);
    masm.popValue(tempVal);
}

static void
GenerateReadSlot(JSContext *cx, IonScript *ion, MacroAssembler &masm,
                 IonCache::StubAttacher &attacher, JSObject *obj, JSObject *holder,
                 Shape *shape, Register object, TypedOrValueRegister output,
                 Label *failures = nullptr)
{
    JS_ASSERT(obj->isNative());
    // If there's a single jump to |failures|, we can patch the shape guard
    // jump directly. Otherwise, jump to the end of the stub, so there's a
    // common point to patch.
    bool multipleFailureJumps = (obj != holder) || (failures != nullptr && failures->used());

    // If we have multiple failure jumps but didn't get a label from the
    // outside, make one ourselves.
    Label failures_;
    if (multipleFailureJumps && !failures)
        failures = &failures_;

    // Guard on the shape of the object.
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(object, JSObject::offsetOfShape()),
                                   ImmGCPtr(obj->lastProperty()),
                                   failures);

    // If we need a scratch register, use either an output register or the
    // object register. After this point, we cannot jump directly to
    // |failures| since we may still have to pop the object register.
    bool restoreScratch = false;
    Register scratchReg = Register::FromCode(0); // Quell compiler warning.

    if (obj != holder || !holder->isFixedSlot(shape->slot())) {
        if (output.hasValue()) {
            scratchReg = output.valueReg().scratchReg();
        } else if (output.type() == MIRType_Double) {
            scratchReg = object;
            masm.push(scratchReg);
            restoreScratch = true;
        } else {
            scratchReg = output.typedReg().gpr();
        }
    }

    // Fast path: single failure jump, no prototype guards.
    if (!multipleFailureJumps) {
        EmitLoadSlot(masm, holder, shape, object, output, scratchReg);
        if (restoreScratch)
            masm.pop(scratchReg);
        attacher.jumpRejoin(masm);
        return;
    }

    // Slow path: multiple jumps; generate prototype guards.
    Label prototypeFailures;
    Register holderReg;
    if (obj != holder) {
        // Note: this may clobber the object register if it's used as scratch.
        GeneratePrototypeGuards(cx, ion, masm, obj, holder, object, scratchReg,
                                &prototypeFailures);

        if (holder) {
            // Guard on the holder's shape.
            holderReg = scratchReg;
            masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), holderReg);
            masm.branchPtr(Assembler::NotEqual,
                           Address(holderReg, JSObject::offsetOfShape()),
                           ImmGCPtr(holder->lastProperty()),
                           &prototypeFailures);
        } else {
            // The property does not exist. Guard on everything in the
            // prototype chain.
            JSObject *proto = obj->getTaggedProto().toObjectOrNull();
            Register lastReg = object;
            JS_ASSERT(scratchReg != object);
            while (proto) {
                masm.loadObjProto(lastReg, scratchReg);

                // Guard the shape of the current prototype.
                masm.branchPtr(Assembler::NotEqual,
                               Address(scratchReg, JSObject::offsetOfShape()),
                               ImmGCPtr(proto->lastProperty()),
                               &prototypeFailures);

                proto = proto->getProto();
                lastReg = scratchReg;
            }

            holderReg = InvalidReg;
        }
    } else {
        holderReg = object;
    }

    // Slot access.
    if (holder)
        EmitLoadSlot(masm, holder, shape, holderReg, output, scratchReg);
    else
        masm.moveValue(UndefinedValue(), output.valueReg());

    // Restore scratch on success.
    if (restoreScratch)
        masm.pop(scratchReg);

    attacher.jumpRejoin(masm);

    masm.bind(&prototypeFailures);
    if (restoreScratch)
        masm.pop(scratchReg);
    masm.bind(failures);

    attacher.jumpNextStub(masm);

}

static bool
EmitGetterCall(JSContext *cx, MacroAssembler &masm,
               IonCache::StubAttacher &attacher, JSObject *obj,
               JSObject *holder, HandleShape shape,
               RegisterSet liveRegs, Register object,
               Register scratchReg, TypedOrValueRegister output,
               void *returnAddr)
{
    JS_ASSERT(output.hasValue());
    MacroAssembler::AfterICSaveLive aic = masm.icSaveLive(liveRegs);

    // Remaining registers should basically be free, but we need to use |object| still
    // so leave it alone.
    RegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object));

    // This is a slower stub path, and we're going to be doing a call anyway.  Don't need
    // to try so hard to not use the stack.  Scratch regs are just taken from the register
    // set not including the input, current value saved on the stack, and restored when
    // we're done with it.
    scratchReg               = regSet.takeGeneral();
    Register argJSContextReg = regSet.takeGeneral();
    Register argUintNReg     = regSet.takeGeneral();
    Register argVpReg        = regSet.takeGeneral();

    // Shape has a getter function.
    bool callNative = IsCacheableGetPropCallNative(obj, holder, shape);
    JS_ASSERT_IF(!callNative, IsCacheableGetPropCallPropertyOp(obj, holder, shape));

    if (callNative) {
        JS_ASSERT(shape->hasGetterValue() && shape->getterValue().isObject() &&
                  shape->getterValue().toObject().is<JSFunction>());
        JSFunction *target = &shape->getterValue().toObject().as<JSFunction>();

        JS_ASSERT(target);
        JS_ASSERT(target->isNative());

        // Native functions have the signature:
        //  bool (*)(JSContext *, unsigned, Value *vp)
        // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
        // are the function arguments.

        // Construct vp array:
        // Push object value for |this|
        masm.Push(TypedOrValueRegister(MIRType_Object, AnyRegister(object)));
        // Push callee/outparam.
        masm.Push(ObjectValue(*target));

        // Preload arguments into registers.
        masm.loadJSContext(argJSContextReg);
        masm.move32(Imm32(0), argUintNReg);
        masm.movePtr(StackPointer, argVpReg);

        // Push marking data for later use.
        masm.Push(argUintNReg);
        attacher.pushStubCodePointer(masm);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(ION_FRAME_OOL_NATIVE);

        // Construct and execute call.
        masm.setupUnalignedABICall(3, scratchReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argUintNReg);
        masm.passABIArg(argVpReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->native()));

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Load the outparam vp[0] into output register(s).
        Address outparam(StackPointer, IonOOLNativeExitFrameLayout::offsetOfResult());
        masm.loadTypedOrValue(outparam, output);

        // masm.leaveExitFrame & pop locals
        masm.adjustStack(IonOOLNativeExitFrameLayout::Size(0));
    } else {
        Register argObjReg       = argUintNReg;
        Register argIdReg        = regSet.takeGeneral();

        PropertyOp target = shape->getterOp();
        JS_ASSERT(target);

        // Push stubCode for marking.
        attacher.pushStubCodePointer(masm);

        // JSPropertyOp: bool fn(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)

        // Push args on stack first so we can take pointers to make handles.
        masm.Push(UndefinedValue());
        masm.movePtr(StackPointer, argVpReg);

        // push canonical jsid from shape instead of propertyname.
        RootedId propId(cx);
        if (!shape->getUserId(cx, &propId))
            return false;
        masm.Push(propId, scratchReg);
        masm.movePtr(StackPointer, argIdReg);

        masm.Push(object);
        masm.movePtr(StackPointer, argObjReg);

        masm.loadJSContext(argJSContextReg);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(ION_FRAME_OOL_PROPERTY_OP);

        // Make the call.
        masm.setupUnalignedABICall(4, scratchReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argObjReg);
        masm.passABIArg(argIdReg);
        masm.passABIArg(argVpReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target));

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Load the outparam vp[0] into output register(s).
        Address outparam(StackPointer, IonOOLPropertyOpExitFrameLayout::offsetOfResult());
        masm.loadTypedOrValue(outparam, output);

        // masm.leaveExitFrame & pop locals.
        masm.adjustStack(IonOOLPropertyOpExitFrameLayout::Size());
    }

    masm.icRestoreLive(liveRegs, aic);
    return true;
}

static bool
GenerateCallGetter(JSContext *cx, IonScript *ion, MacroAssembler &masm,
                   IonCache::StubAttacher &attacher, JSObject *obj, PropertyName *name,
                   JSObject *holder, HandleShape shape, RegisterSet &liveRegs, Register object,
                   TypedOrValueRegister output, void *returnAddr, Label *failures = nullptr)
{
    JS_ASSERT(obj->isNative());
    JS_ASSERT(output.hasValue());

    // Use the passed in label if there was one. Otherwise, we'll have to make our own.
    Label stubFailure;
    failures = failures ? failures : &stubFailure;

    // Initial shape check.
    masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfShape()),
                   ImmGCPtr(obj->lastProperty()), failures);

    Register scratchReg = output.valueReg().scratchReg();

    // Note: this may clobber the object register if it's used as scratch.
    if (obj != holder)
        GeneratePrototypeGuards(cx, ion, masm, obj, holder, object, scratchReg, failures);

    // Guard on the holder's shape.
    Register holderReg = scratchReg;
    masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), holderReg);
    masm.branchPtr(Assembler::NotEqual,
                   Address(holderReg, JSObject::offsetOfShape()),
                   ImmGCPtr(holder->lastProperty()),
                   failures);

    // Now we're good to go to invoke the native call.
    if (!EmitGetterCall(cx, masm, attacher, obj, holder, shape, liveRegs, object,
                        scratchReg, output, returnAddr))
        return false;

    // Rejoin jump.
    attacher.jumpRejoin(masm);

    // Jump to next stub.
    masm.bind(failures);
    attacher.jumpNextStub(masm);

    return true;
}

static bool
GenerateArrayLength(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                    JSObject *obj, Register object, TypedOrValueRegister output)
{
    JS_ASSERT(obj->is<ArrayObject>());

    Label failures;

    // Guard object is a dense array.
    RootedShape shape(cx, obj->lastProperty());
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Load length.
    Register outReg;
    if (output.hasValue()) {
        outReg = output.valueReg().scratchReg();
    } else {
        JS_ASSERT(output.type() == MIRType_Int32);
        outReg = output.typedReg().gpr();
    }

    masm.loadPtr(Address(object, JSObject::offsetOfElements()), outReg);
    masm.load32(Address(outReg, ObjectElements::offsetOfLength()), outReg);

    // The length is an unsigned int, but the value encodes a signed int.
    JS_ASSERT(object != outReg);
    masm.branchTest32(Assembler::Signed, outReg, outReg, &failures);

    if (output.hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, outReg, output.valueReg());

    /* Success. */
    attacher.jumpRejoin(masm);

    /* Failure. */
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return true;
}

static void
GenerateTypedArrayLength(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                         JSObject *obj, Register object, TypedOrValueRegister output)
{
    JS_ASSERT(obj->is<TypedArrayObject>());

    Label failures;

    Register tmpReg;
    if (output.hasValue()) {
        tmpReg = output.valueReg().scratchReg();
    } else {
        JS_ASSERT(output.type() == MIRType_Int32);
        tmpReg = output.typedReg().gpr();
    }
    JS_ASSERT(object != tmpReg);

    // Implement the negated version of JSObject::isTypedArray predicate.
    masm.loadObjClass(object, tmpReg);
    masm.branchPtr(Assembler::Below, tmpReg, ImmPtr(&TypedArrayObject::classes[0]),
                   &failures);
    masm.branchPtr(Assembler::AboveOrEqual, tmpReg,
                   ImmPtr(&TypedArrayObject::classes[ScalarTypeRepresentation::TYPE_MAX]),
                   &failures);

    // Load length.
    masm.loadTypedOrValue(Address(object, TypedArrayObject::lengthOffset()), output);

    /* Success. */
    attacher.jumpRejoin(masm);

    /* Failure. */
    masm.bind(&failures);
    attacher.jumpNextStub(masm);
}

static bool
IsCacheableArrayLength(JSContext *cx, HandleObject obj, HandlePropertyName name,
                       TypedOrValueRegister output)
{
    if (!obj->is<ArrayObject>())
        return false;

    if (output.type() != MIRType_Value && output.type() != MIRType_Int32) {
        // The stub assumes that we always output Int32, so make sure our output
        // is equipped to handle that.
        return false;
    }

    return true;
}

template <class GetPropCache>
static GetPropertyIC::NativeGetPropCacheability
CanAttachNativeGetProp(typename GetPropCache::Context cx, const GetPropCache &cache,
                       HandleObject obj, HandlePropertyName name,
                       MutableHandleObject holder, MutableHandleShape shape,
                       bool skipArrayLen = false)
{
    if (!obj || !obj->isNative())
        return GetPropertyIC::CanAttachNone;

    // The lookup needs to be universally pure, otherwise we risk calling hooks out
    // of turn. We don't mind doing this even when purity isn't required, because we
    // only miss out on shape hashification, which is only a temporary perf cost.
    // The limits were arbitrarily set, anyways.
    if (!LookupPropertyPure(obj, NameToId(name), holder.address(), shape.address()))
        return GetPropertyIC::CanAttachNone;

    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);
    if (IsCacheableGetPropReadSlot(obj, holder, shape) ||
        IsCacheableNoProperty(obj, holder, shape, pc, cache.output()))
    {
        return GetPropertyIC::CanAttachReadSlot;
    }

    // |length| is a non-configurable getter property on ArrayObjects. Any time this
    // check would have passed, we can install a getter stub instead. Allow people to
    // make that decision themselves with skipArrayLen
    if (!skipArrayLen && cx->names().length == name && cache.allowArrayLength(cx, obj) &&
        IsCacheableArrayLength(cx, obj, name, cache.output()))
    {
        // The array length property is non-configurable, which means both that
        // checking the class of the object and the name of the property is enough
        // and that we don't need to worry about monitoring, since we know the
        // return type statically.
        return GetPropertyIC::CanAttachArrayLength;
    }

    // IonBuilder guarantees that it's impossible to generate a GetPropertyIC with
    // allowGetters() true and cache.output().hasValue() false. If this isn't true,
    // we will quickly assert during stub generation.
    if (cache.allowGetters() &&
        (IsCacheableGetPropCallNative(obj, holder, shape) ||
         IsCacheableGetPropCallPropertyOp(obj, holder, shape)))
    {
        // Don't enable getter call if cache is parallel or idempotent, since
        // they can be effectful. This is handled by allowGetters()
        return GetPropertyIC::CanAttachCallGetter;
    }

    return GetPropertyIC::CanAttachNone;
}

bool
GetPropertyIC::allowArrayLength(Context cx, HandleObject obj) const
{
    if (!idempotent())
        return true;

    uint32_t locationIndex, numLocations;
    getLocationInfo(&locationIndex, &numLocations);

    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    CacheLocation *locs = ion->getCacheLocs(locationIndex);
    for (size_t i = 0; i < numLocations; i++) {
        CacheLocation &curLoc = locs[i];
        types::StackTypeSet *bcTypes =
            types::TypeScript::BytecodeTypes(curLoc.script, curLoc.pc);

        if (!bcTypes->hasType(types::Type::Int32Type()))
            return false;
    }

    return true;
}

bool
GetPropertyIC::tryAttachNative(JSContext *cx, IonScript *ion, HandleObject obj,
                               HandlePropertyName name, void *returnAddr, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    RootedShape shape(cx);
    RootedObject holder(cx);

    NativeGetPropCacheability type =
        CanAttachNativeGetProp(cx, *this, obj, name, &holder, &shape);
    if (type == CanAttachNone)
        return true;

    *emitted = true;

    MacroAssembler masm(cx, ion);
    SkipRoot skip(cx, &masm);

    RepatchStubAppender attacher(*this);
    const char *attachKind;

    switch (type) {
      case CanAttachReadSlot:
        GenerateReadSlot(cx, ion, masm, attacher, obj, holder,
                         shape, object(), output());
        attachKind = idempotent() ? "idempotent reading"
                                    : "non idempotent reading";
        break;
      case CanAttachCallGetter:
        if (!GenerateCallGetter(cx, ion, masm, attacher, obj, name, holder, shape,
                                liveRegs_, object(), output(), returnAddr))
        {
            return false;
        }
        attachKind = "getter call";
        break;
      case CanAttachArrayLength:
        if (!GenerateArrayLength(cx, masm, attacher, obj, object(), output()))
            return false;

        attachKind = "array length";
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Bad NativeGetPropCacheability");
    }
    return linkAndAttachStub(cx, masm, attacher, ion, attachKind);
}

bool
GetPropertyIC::tryAttachTypedArrayLength(JSContext *cx, IonScript *ion, HandleObject obj,
                                         HandlePropertyName name, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    if (!obj->is<TypedArrayObject>())
        return true;

    if (cx->names().length != name)
        return true;

    if (hasTypedArrayLengthStub())
        return true;

    if (output().type() != MIRType_Value && output().type() != MIRType_Int32) {
        // The next execution should cause an invalidation because the type
        // does not fit.
        return true;
    }

    if (idempotent())
        return true;

    *emitted = true;

    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);
    GenerateTypedArrayLength(cx, masm, attacher, obj, object(), output());

    JS_ASSERT(!hasTypedArrayLengthStub_);
    hasTypedArrayLengthStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "typed array length");
}


static bool
EmitCallProxyGet(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                 PropertyName *name, RegisterSet liveRegs, Register object,
                 TypedOrValueRegister output, jsbytecode *pc, void *returnAddr)
{
    JS_ASSERT(output.hasValue());
    MacroAssembler::AfterICSaveLive aic = masm.icSaveLive(liveRegs);

    // Remaining registers should be free, but we need to use |object| still
    // so leave it alone.
    RegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object));

    // Proxy::get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
    //            MutableHandleValue vp)
    Register argJSContextReg = regSet.takeGeneral();
    Register argProxyReg     = regSet.takeGeneral();
    Register argIdReg        = regSet.takeGeneral();
    Register argVpReg        = regSet.takeGeneral();

    Register scratch         = regSet.takeGeneral();

    void *getFunction = JSOp(*pc) == JSOP_CALLPROP                      ?
                            JS_FUNC_TO_DATA_PTR(void *, Proxy::callProp) :
                            JS_FUNC_TO_DATA_PTR(void *, Proxy::get);

    // Push stubCode for marking.
    attacher.pushStubCodePointer(masm);

    // Push args on stack first so we can take pointers to make handles.
    masm.Push(UndefinedValue());
    masm.movePtr(StackPointer, argVpReg);

    RootedId propId(cx, AtomToId(name));
    masm.Push(propId, scratch);
    masm.movePtr(StackPointer, argIdReg);

    // Pushing object and receiver.  Both are the same, so Handle to one is equivalent to
    // handle to other.
    masm.Push(object);
    masm.Push(object);
    masm.movePtr(StackPointer, argProxyReg);

    masm.loadJSContext(argJSContextReg);

    if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
        return false;
    masm.enterFakeExitFrame(ION_FRAME_OOL_PROXY);

    // Make the call.
    masm.setupUnalignedABICall(5, scratch);
    masm.passABIArg(argJSContextReg);
    masm.passABIArg(argProxyReg);
    masm.passABIArg(argProxyReg);
    masm.passABIArg(argIdReg);
    masm.passABIArg(argVpReg);
    masm.callWithABI(getFunction);

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // Load the outparam vp[0] into output register(s).
    Address outparam(StackPointer, IonOOLProxyExitFrameLayout::offsetOfResult());
    masm.loadTypedOrValue(outparam, output);

    // masm.leaveExitFrame & pop locals
    masm.adjustStack(IonOOLProxyExitFrameLayout::Size());

    masm.icRestoreLive(liveRegs, aic);
    return true;
}

bool
GetPropertyIC::tryAttachDOMProxyShadowed(JSContext *cx, IonScript *ion,
                                         HandleObject obj, void *returnAddr,
                                         bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);
    JS_ASSERT(IsCacheableDOMProxy(obj));

    if (idempotent() || !output().hasValue())
        return true;

    *emitted = true;

    Label failures;
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    // Guard on the shape of the object.
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(object(), JSObject::offsetOfShape()),
                                   ImmGCPtr(obj->lastProperty()),
                                   &failures);

    // Make sure object is a DOMProxy
    GenerateDOMProxyChecks(cx, masm, obj, name(), object(), &failures,
                           /*skipExpandoCheck=*/true);

    if (!EmitCallProxyGet(cx, masm, attacher, name(), liveRegs_, object(), output(),
                          pc(), returnAddr))
    {
        return false;
    }

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "list base shadowed get");
}

bool
GetPropertyIC::tryAttachDOMProxyUnshadowed(JSContext *cx, IonScript *ion, HandleObject obj,
                                           HandlePropertyName name, bool resetNeeded,
                                           void *returnAddr, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);
    JS_ASSERT(IsCacheableDOMProxy(obj));

    RootedObject checkObj(cx, obj->getTaggedProto().toObjectOrNull());
    RootedObject holder(cx);
    RootedShape shape(cx);

    NativeGetPropCacheability canCache =
        CanAttachNativeGetProp(cx, *this, checkObj, name, &holder, &shape,
                               /* skipArrayLen = */true);
    JS_ASSERT(canCache != CanAttachArrayLength);

    if (canCache == CanAttachNone)
        return true;

    // Make sure we observe our invariants if we're gonna deoptimize.
    if (!holder && (idempotent() || !output().hasValue()))
        return true;

    *emitted = true;

    if (resetNeeded) {
        // If we know that we have a DoesntShadowUnique object, then
        // we reset the cache to clear out an existing IC for the object
        // (if there is one). The generation is a constant in the generated
        // code and we will not have the same generation again for this
        // object, so the generation check in the existing IC would always
        // fail anyway.
        reset();
    }

    Label failures;
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    // Guard on the shape of the object.
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(object(), JSObject::offsetOfShape()),
                                   ImmGCPtr(obj->lastProperty()),
                                   &failures);

    // Make sure object is a DOMProxy proxy
    GenerateDOMProxyChecks(cx, masm, obj, name, object(), &failures);

    if (holder) {
        // Found the property on the prototype chain. Treat it like a native
        // getprop.
        Register scratchReg = output().valueReg().scratchReg();
        GeneratePrototypeGuards(cx, ion, masm, obj, holder, object(), scratchReg, &failures);

        // Rename scratch for clarity.
        Register holderReg = scratchReg;

        // Guard on the holder of the property
        masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), holderReg);
        masm.branchPtr(Assembler::NotEqual,
                    Address(holderReg, JSObject::offsetOfShape()),
                    ImmGCPtr(holder->lastProperty()),
                    &failures);

        if (canCache == CanAttachReadSlot) {
            EmitLoadSlot(masm, holder, shape, holderReg, output(), scratchReg);
        } else {
            // EmitGetterCall() expects |obj| to be the object the property is
            // on to do some checks. Since we actually looked at checkObj, and
            // no extra guards will be generated, we can just pass that instead.
            JS_ASSERT(canCache == CanAttachCallGetter);
            JS_ASSERT(!idempotent());
            if (!EmitGetterCall(cx, masm, attacher, checkObj, holder, shape, liveRegs_,
                                object(), scratchReg, output(), returnAddr))
            {
                return false;
            }
        }
    } else {
        // Property was not found on the prototype chain. Deoptimize down to
        // proxy get call
        JS_ASSERT(!idempotent());
        if (!EmitCallProxyGet(cx, masm, attacher, name, liveRegs_, object(), output(),
                              pc(), returnAddr))
        {
            return false;
        }
    }

    attacher.jumpRejoin(masm);
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "unshadowed proxy get");
}

bool
GetPropertyIC::tryAttachProxy(JSContext *cx, IonScript *ion, HandleObject obj,
                              HandlePropertyName name, void *returnAddr,
                              bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    if (!obj->is<ProxyObject>())
        return true;

    // Skim off DOM proxies.
    if (IsCacheableDOMProxy(obj)) {
        RootedId id(cx, NameToId(name));
        DOMProxyShadowsResult shadows = GetDOMProxyShadowsCheck()(cx, obj, id);
        if (shadows == ShadowCheckFailed)
            return false;
        if (shadows == Shadows)
            return tryAttachDOMProxyShadowed(cx, ion, obj, returnAddr, emitted);

        return tryAttachDOMProxyUnshadowed(cx, ion, obj, name, shadows == DoesntShadowUnique,
                                           returnAddr, emitted);
    }

    return tryAttachGenericProxy(cx, ion, obj, name, returnAddr, emitted);
}

static void
GenerateProxyClassGuards(MacroAssembler &masm, Register object, Register scratchReg,
                         Label *failures)
{
    masm.loadObjClass(object, scratchReg);
    masm.branchTest32(Assembler::Zero,
                      Address(scratchReg, Class::offsetOfFlags()),
                      Imm32(JSCLASS_IS_PROXY), failures);
}

bool
GetPropertyIC::tryAttachGenericProxy(JSContext *cx, IonScript *ion, HandleObject obj,
                                     HandlePropertyName name, void *returnAddr,
                                     bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);
    JS_ASSERT(obj->is<ProxyObject>());

    if (hasGenericProxyStub())
        return true;

    if (idempotent() || !output().hasValue())
        return true;

    *emitted = true;

    Label failures;
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    Register scratchReg = output().valueReg().scratchReg();

    GenerateProxyClassGuards(masm, object(), scratchReg, &failures);

    // Ensure that the incoming object is not a DOM proxy, so that we can get to
    // the specialized stubs
    masm.branchTestProxyHandlerFamily(Assembler::Equal, object(), scratchReg,
                                      GetDOMProxyHandlerFamily(), &failures);

    if (!EmitCallProxyGet(cx, masm, attacher, name, liveRegs_, object(), output(),
                          pc(), returnAddr))
    {
        return false;
    }

    attacher.jumpRejoin(masm);

    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    JS_ASSERT(!hasGenericProxyStub_);
    hasGenericProxyStub_ = true;

    return linkAndAttachStub(cx, masm, attacher, ion, "Generic Proxy get");
}

bool
GetPropertyIC::tryAttachArgumentsLength(JSContext *cx, IonScript *ion, HandleObject obj,
                                        HandlePropertyName name, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    if (name != cx->names().length)
        return true;
    if (!IsOptimizableArgumentsObjectForLength(obj))
        return true;

    MIRType outputType = output().type();
    if (!(outputType == MIRType_Value || outputType == MIRType_Int32))
        return true;

    if (hasArgumentsLengthStub(obj->is<StrictArgumentsObject>()))
        return true;

    *emitted = true;

    JS_ASSERT(!idempotent());

    Label failures;
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    Register tmpReg;
    if (output().hasValue()) {
        tmpReg = output().valueReg().scratchReg();
    } else {
        JS_ASSERT(output().type() == MIRType_Int32);
        tmpReg = output().typedReg().gpr();
    }
    JS_ASSERT(object() != tmpReg);

    const Class *clasp = obj->is<StrictArgumentsObject>() ? &StrictArgumentsObject::class_
                                                          : &NormalArgumentsObject::class_;

    masm.branchTestObjClass(Assembler::NotEqual, object(), tmpReg, clasp, &failures);

    // Get initial ArgsObj length value, test if length has been overridden.
    masm.unboxInt32(Address(object(), ArgumentsObject::getInitialLengthSlotOffset()), tmpReg);
    masm.branchTest32(Assembler::NonZero, tmpReg, Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      &failures);

    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), tmpReg);

    // If output is Int32, result is already in right place, otherwise box it into output.
    if (output().hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, tmpReg, output().valueReg());

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    if (obj->is<StrictArgumentsObject>()) {
        JS_ASSERT(!hasStrictArgumentsLengthStub_);
        hasStrictArgumentsLengthStub_ = true;
        return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj length (strict)");
    }

    JS_ASSERT(!hasNormalArgumentsLengthStub_);
    hasNormalArgumentsLengthStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj length (normal)");
}

bool
GetPropertyIC::tryAttachStub(JSContext *cx, IonScript *ion, HandleObject obj,
                             HandlePropertyName name, void *returnAddr, bool *emitted)
{
    JS_ASSERT(!*emitted);

    if (!canAttachStub())
        return true;

    if (!*emitted && !tryAttachArgumentsLength(cx, ion, obj, name, emitted))
        return false;

    if (!*emitted && !tryAttachProxy(cx, ion, obj, name, returnAddr, emitted))
        return false;

    if (!*emitted && !tryAttachNative(cx, ion, obj, name, returnAddr, emitted))
        return false;

    if (!*emitted && !tryAttachTypedArrayLength(cx, ion, obj, name, emitted))
        return false;

    return true;
}

/* static */ bool
GetPropertyIC::update(JSContext *cx, size_t cacheIndex,
                      HandleObject obj, MutableHandleValue vp)
{
    void *returnAddr;
    RootedScript topScript(cx, GetTopIonJSScript(cx, &returnAddr));
    IonScript *ion = topScript->ionScript();

    GetPropertyIC &cache = ion->getCache(cacheIndex).toGetProperty();
    RootedPropertyName name(cx, cache.name());

    AutoFlushCache afc ("GetPropertyCache", cx->runtime()->jitRuntime());

    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, vp.address(), ion);

    // If the cache is idempotent, we will redo the op in the interpreter.
    if (cache.idempotent())
        adi.disable();

    // For now, just stop generating new stubs once we hit the stub count
    // limit. Once we can make calls from within generated stubs, a new call
    // stub will be generated instead and the previous stubs unlinked.
    bool emitted = false;
    if (!cache.tryAttachStub(cx, ion, obj, name, returnAddr, &emitted))
        return false;

    if (cache.idempotent() && !emitted) {
        // Invalidate the cache if the property was not found, or was found on
        // a non-native object. This ensures:
        // 1) The property read has no observable side-effects.
        // 2) There's no need to dynamically monitor the return type. This would
        //    be complicated since (due to GVN) there can be multiple pc's
        //    associated with a single idempotent cache.
        IonSpew(IonSpew_InlineCaches, "Invalidating from idempotent cache %s:%d",
                topScript->filename(), topScript->lineno());

        topScript->setInvalidatedIdempotentCache();

        // Do not re-invalidate if the lookup already caused invalidation.
        if (!topScript->hasIonScript())
            return true;

        return Invalidate(cx, topScript);
    }

    RootedId id(cx, NameToId(name));
    if (!JSObject::getGeneric(cx, obj, obj, id, vp))
        return false;

    if (!cache.idempotent()) {
        RootedScript script(cx);
        jsbytecode *pc;
        cache.getScriptedLocation(&script, &pc);

        // If the cache is idempotent, the property exists so we don't have to
        // call __noSuchMethod__.

#if JS_HAS_NO_SUCH_METHOD
        // Handle objects with __noSuchMethod__.
        if (JSOp(*pc) == JSOP_CALLPROP && MOZ_UNLIKELY(vp.isUndefined())) {
            if (!OnUnknownMethod(cx, obj, IdToValue(id), vp))
                return false;
        }
#endif

        // Monitor changes to cache entry.
        if (!cache.monitoredResult())
            types::TypeScript::Monitor(cx, script, pc, vp);
    }

    return true;
}

void
GetPropertyIC::reset()
{
    RepatchIonCache::reset();
    hasTypedArrayLengthStub_ = false;
    hasStrictArgumentsLengthStub_ = false;
    hasNormalArgumentsLengthStub_ = false;
    hasGenericProxyStub_ = false;
}

bool
ParallelIonCache::initStubbedShapes(JSContext *cx)
{
    JS_ASSERT(isAllocated());
    if (!stubbedShapes_) {
        stubbedShapes_ = cx->new_<ShapeSet>(cx);
        return stubbedShapes_ && stubbedShapes_->init();
    }
    return true;
}

bool
ParallelIonCache::hasOrAddStubbedShape(LockedJSContext &cx, Shape *shape, bool *alreadyStubbed)
{
    // Check if we have already stubbed the current object to avoid
    // attaching a duplicate stub.
    if (!initStubbedShapes(cx))
        return false;
    ShapeSet::AddPtr p = stubbedShapes_->lookupForAdd(shape);
    if ((*alreadyStubbed = !!p))
        return true;
    return stubbedShapes_->add(p, shape);
}

void
ParallelIonCache::reset()
{
    DispatchIonCache::reset();
    if (stubbedShapes_)
        stubbedShapes_->clear();
}

void
ParallelIonCache::destroy()
{
    DispatchIonCache::destroy();
    js_delete(stubbedShapes_);
}

void
GetPropertyParIC::reset()
{
    ParallelIonCache::reset();
    hasTypedArrayLengthStub_ = false;
}

bool
GetPropertyParIC::attachReadSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj,
                                 JSObject *holder, Shape *shape)
{
    // Ready to generate the read slot stub.
    DispatchStubPrepender attacher(*this);
    MacroAssembler masm(cx, ion);
    GenerateReadSlot(cx, ion, masm, attacher, obj, holder, shape, object(), output());

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel reading");
}

bool
GetPropertyParIC::attachArrayLength(LockedJSContext &cx, IonScript *ion, JSObject *obj)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    if (!GenerateArrayLength(cx, masm, attacher, obj, object(), output()))
        return false;

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel array length");
}

bool
GetPropertyParIC::attachTypedArrayLength(LockedJSContext &cx, IonScript *ion, JSObject *obj)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    GenerateTypedArrayLength(cx, masm, attacher, obj, object(), output());

    JS_ASSERT(!hasTypedArrayLengthStub_);
    hasTypedArrayLengthStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "parallel typed array length");
}

bool
GetPropertyParIC::update(ForkJoinContext *cx, size_t cacheIndex,
                         HandleObject obj, MutableHandleValue vp)
{
    AutoFlushCache afc("GetPropertyParCache", cx->runtime()->jitRuntime());

    IonScript *ion = GetTopIonJSScript(cx)->parallelIonScript();
    GetPropertyParIC &cache = ion->getCache(cacheIndex).toGetPropertyPar();

    // Grab the property early, as the pure path is fast anyways and doesn't
    // need a lock. If we can't do it purely, bail out of parallel execution.
    if (!GetPropertyPure(cx, obj, NameToId(cache.name()), vp.address()))
        return false;

    // Avoid unnecessary locking if cannot attach stubs.
    if (!cache.canAttachStub())
        return true;

    {
        // Lock the context before mutating the cache. Ideally we'd like to do
        // finer-grained locking, with one lock per cache. However, generating
        // new jitcode uses a global ExecutableAllocator tied to the runtime.
        LockedJSContext ncx(cx);

        if (cache.canAttachStub()) {
            bool alreadyStubbed;
            if (!cache.hasOrAddStubbedShape(ncx, obj->lastProperty(), &alreadyStubbed))
                return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
            if (alreadyStubbed)
                return true;

            // See note about the stub limit in GetPropertyCache.
            bool attachedStub = false;

            {
                RootedShape shape(ncx);
                RootedObject holder(ncx);
                RootedPropertyName name(ncx, cache.name());

                GetPropertyIC::NativeGetPropCacheability canCache =
                    CanAttachNativeGetProp(ncx, cache, obj, name, &holder, &shape);

                if (canCache == GetPropertyIC::CanAttachReadSlot) {
                    if (!cache.attachReadSlot(ncx, ion, obj, holder, shape))
                        return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                    attachedStub = true;
                }

                if (!attachedStub && canCache == GetPropertyIC::CanAttachArrayLength) {
                    if (!cache.attachArrayLength(ncx, ion, obj))
                        return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                    attachedStub = true;
                }
            }

            if (!attachedStub && !cache.hasTypedArrayLengthStub() &&
                obj->is<TypedArrayObject>() && cx->names().length == cache.name() &&
                (cache.output().type() == MIRType_Value || cache.output().type() == MIRType_Int32))
            {
                if (!cache.attachTypedArrayLength(ncx, ion, obj))
                    return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                attachedStub = true;
            }
        }
    }

    return true;
}

void
IonCache::disable()
{
    reset();
    this->disabled_ = 1;
}

void
IonCache::reset()
{
    this->stubCount_ = 0;
}

void
IonCache::destroy()
{
}

static void
GenerateSetSlot(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                JSObject *obj, Shape *shape, Register object, ConstantOrRegister value,
                bool needsTypeBarrier, bool checkTypeset)
{
    JS_ASSERT(obj->isNative());

    Label failures, barrierFailure;
    masm.branchPtr(Assembler::NotEqual,
                   Address(object, JSObject::offsetOfShape()),
                   ImmGCPtr(obj->lastProperty()), &failures);

    // Guard that the incoming value is in the type set for the property
    // if a type barrier is required.
    if (needsTypeBarrier) {
        // We can't do anything that would change the HeapTypeSet, so
        // just guard that it's already there.

        // Obtain and guard on the TypeObject of the object.
        types::TypeObject *type = obj->type();
        masm.branchPtr(Assembler::NotEqual,
                       Address(object, JSObject::offsetOfType()),
                       ImmGCPtr(type), &failures);

        if (checkTypeset) {
            TypedOrValueRegister valReg = value.reg();
            types::HeapTypeSet *propTypes = type->maybeGetProperty(shape->propid());
            JS_ASSERT(propTypes);
            JS_ASSERT(!propTypes->unknown());

            Register scratchReg = object;
            masm.push(scratchReg);

            masm.guardTypeSet(valReg, propTypes, scratchReg, &barrierFailure);
            masm.pop(object);
        }
    }

    if (obj->isFixedSlot(shape->slot())) {
        Address addr(object, JSObject::getFixedSlotOffset(shape->slot()));

        if (cx->zone()->needsBarrier())
            masm.callPreBarrier(addr, MIRType_Value);

        masm.storeConstantOrRegister(value, addr);
    } else {
        Register slotsReg = object;
        masm.loadPtr(Address(object, JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(shape->slot()) * sizeof(Value));

        if (cx->zone()->needsBarrier())
            masm.callPreBarrier(addr, MIRType_Value);

        masm.storeConstantOrRegister(value, addr);
    }

    attacher.jumpRejoin(masm);

    if (barrierFailure.used()) {
        masm.bind(&barrierFailure);
        masm.pop(object);
    }

    masm.bind(&failures);
    attacher.jumpNextStub(masm);
}

bool
SetPropertyIC::attachSetSlot(JSContext *cx, IonScript *ion, HandleObject obj,
                             HandleShape shape, bool checkTypeset)
{
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);
    GenerateSetSlot(cx, masm, attacher, obj, shape, object(), value(), needsTypeBarrier(),
                    checkTypeset);
    return linkAndAttachStub(cx, masm, attacher, ion, "setting");
}

static bool
IsCacheableSetPropCallNative(HandleObject obj, HandleObject holder, HandleShape shape)
{
    JS_ASSERT(obj->isNative());

    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    return shape->hasSetterValue() && shape->setterObject() &&
           shape->setterObject()->is<JSFunction>() &&
           shape->setterObject()->as<JSFunction>().isNative();
}

static bool
IsCacheableSetPropCallPropertyOp(HandleObject obj, HandleObject holder, HandleShape shape)
{
    JS_ASSERT(obj->isNative());

    if (!shape)
        return false;

    if (!IsCacheableProtoChain(obj, holder))
        return false;

    if (shape->hasSlot())
        return false;

    if (shape->hasDefaultSetter())
        return false;

    if (shape->hasSetterValue())
        return false;

    // Despite the vehement claims of Shape.h that writable() is only
    // relevant for data descriptors, some PropertyOp setters care
    // desperately about its value. The flag should be always true, apart
    // from these rare instances.
    if (!shape->writable())
        return false;

    return true;
}

static bool
EmitCallProxySet(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                 HandleId propId, RegisterSet liveRegs, Register object,
                 ConstantOrRegister value, void *returnAddr, bool strict)
{
    MacroAssembler::AfterICSaveLive aic = masm.icSaveLive(liveRegs);

    // Remaining registers should be free, but we need to use |object| still
    // so leave it alone.
    RegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object));

    // Proxy::set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
    //            bool strict, MutableHandleValue vp)
    Register argJSContextReg = regSet.takeGeneral();
    Register argProxyReg     = regSet.takeGeneral();
    Register argIdReg        = regSet.takeGeneral();
    Register argVpReg        = regSet.takeGeneral();
    Register argStrictReg    = regSet.takeGeneral();

    Register scratch         = regSet.takeGeneral();

    // Push stubCode for marking.
    attacher.pushStubCodePointer(masm);

    // Push args on stack first so we can take pointers to make handles.
    masm.Push(value);
    masm.movePtr(StackPointer, argVpReg);

    masm.Push(propId, scratch);
    masm.movePtr(StackPointer, argIdReg);

    // Pushing object and receiver.  Both are the same, so Handle to one is equivalent to
    // handle to other.
    masm.Push(object);
    masm.Push(object);
    masm.movePtr(StackPointer, argProxyReg);

    masm.loadJSContext(argJSContextReg);
    masm.move32(Imm32(strict? 1 : 0), argStrictReg);

    if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
        return false;
    masm.enterFakeExitFrame(ION_FRAME_OOL_PROXY);

    // Make the call.
    masm.setupUnalignedABICall(6, scratch);
    masm.passABIArg(argJSContextReg);
    masm.passABIArg(argProxyReg);
    masm.passABIArg(argProxyReg);
    masm.passABIArg(argIdReg);
    masm.passABIArg(argStrictReg);
    masm.passABIArg(argVpReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Proxy::set));

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // masm.leaveExitFrame & pop locals
    masm.adjustStack(IonOOLProxyExitFrameLayout::Size());

    masm.icRestoreLive(liveRegs, aic);
    return true;
}

bool
SetPropertyIC::attachGenericProxy(JSContext *cx, IonScript *ion, void *returnAddr)
{
    JS_ASSERT(!hasGenericProxyStub());

    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    Label failures;
    {
        Label proxyFailures;
        Label proxySuccess;

        RegisterSet regSet(RegisterSet::All());
        regSet.take(AnyRegister(object()));
        if (!value().constant())
            regSet.takeUnchecked(value().reg());

        Register scratch = regSet.takeGeneral();
        masm.push(scratch);

        GenerateProxyClassGuards(masm, object(), scratch, &proxyFailures);

        // Remove the DOM proxies. They'll take care of themselves so this stub doesn't
        // catch too much. The failure case is actually Equal. Fall through to the failure code.
        masm.branchTestProxyHandlerFamily(Assembler::NotEqual, object(), scratch,
                                          GetDOMProxyHandlerFamily(), &proxySuccess);

        masm.bind(&proxyFailures);
        masm.pop(scratch);
        // Unify the point of failure to allow for later DOM proxy handling.
        masm.jump(&failures);

        masm.bind(&proxySuccess);
        masm.pop(scratch);
    }

    RootedId propId(cx, AtomToId(name()));
    if (!EmitCallProxySet(cx, masm, attacher, propId, liveRegs_, object(), value(),
                          returnAddr, strict()))
    {
        return false;
    }

    attacher.jumpRejoin(masm);

    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    JS_ASSERT(!hasGenericProxyStub_);
    hasGenericProxyStub_ = true;

    return linkAndAttachStub(cx, masm, attacher, ion, "generic proxy set");
}

bool
SetPropertyIC::attachDOMProxyShadowed(JSContext *cx, IonScript *ion, HandleObject obj,
                                        void *returnAddr)
{
    JS_ASSERT(IsCacheableDOMProxy(obj));

    Label failures;
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    // Guard on the shape of the object.
    masm.branchPtr(Assembler::NotEqual,
                   Address(object(), JSObject::offsetOfShape()),
                   ImmGCPtr(obj->lastProperty()), &failures);

    // Make sure object is a DOMProxy
    GenerateDOMProxyChecks(cx, masm, obj, name(), object(), &failures,
                           /*skipExpandoCheck=*/true);

    RootedId propId(cx, AtomToId(name()));
    if (!EmitCallProxySet(cx, masm, attacher, propId, liveRegs_, object(),
                          value(), returnAddr, strict()))
    {
        return false;
    }

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "DOM proxy shadowed set");
}

static bool
GenerateCallSetter(JSContext *cx, IonScript *ion, MacroAssembler &masm,
                   IonCache::StubAttacher &attacher, HandleObject obj,
                   HandleObject holder, HandleShape shape, bool strict, Register object,
                   ConstantOrRegister value, Label *failure, RegisterSet liveRegs,
                   void *returnAddr)
{
    // Generate prototype guards if needed.
    // Take a scratch register for use, save on stack.
    {
        RegisterSet regSet(RegisterSet::All());
        regSet.take(AnyRegister(object));
        if (!value.constant())
            regSet.takeUnchecked(value.reg());
        Register scratchReg = regSet.takeGeneral();
        masm.push(scratchReg);

        Label protoFailure;
        Label protoSuccess;

        // Generate prototype/shape guards.
        if (obj != holder)
            GeneratePrototypeGuards(cx, ion, masm, obj, holder, object, scratchReg, &protoFailure);

        masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), scratchReg);
        masm.branchPtr(Assembler::NotEqual,
                       Address(scratchReg, JSObject::offsetOfShape()),
                       ImmGCPtr(holder->lastProperty()),
                       &protoFailure);

        masm.jump(&protoSuccess);

        masm.bind(&protoFailure);
        masm.pop(scratchReg);
        masm.jump(failure);

        masm.bind(&protoSuccess);
        masm.pop(scratchReg);
    }

    // Good to go for invoking setter.

    MacroAssembler::AfterICSaveLive aic = masm.icSaveLive(liveRegs);

    // Remaining registers should basically be free, but we need to use |object| still
    // so leave it alone.
    RegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object));

    // This is a slower stub path, and we're going to be doing a call anyway.  Don't need
    // to try so hard to not use the stack.  Scratch regs are just taken from the register
    // set not including the input, current value saved on the stack, and restored when
    // we're done with it.
    //
    // Be very careful not to use any of these before value is pushed, since they
    // might shadow.
    Register scratchReg     = regSet.takeGeneral();
    Register argJSContextReg = regSet.takeGeneral();
    Register argVpReg        = regSet.takeGeneral();

    bool callNative = IsCacheableSetPropCallNative(obj, holder, shape);
    JS_ASSERT_IF(!callNative, IsCacheableSetPropCallPropertyOp(obj, holder, shape));

    if (callNative) {
        JS_ASSERT(shape->hasSetterValue() && shape->setterObject() &&
                  shape->setterObject()->is<JSFunction>());
        JSFunction *target = &shape->setterObject()->as<JSFunction>();

        JS_ASSERT(target->isNative());

        Register argUintNReg = regSet.takeGeneral();

        // Set up the call:
        //  bool (*)(JSContext *, unsigned, Value *vp)
        // vp[0] is callee/outparam
        // vp[1] is |this|
        // vp[2] is the value

        // Build vp and move the base into argVpReg.
        masm.Push(value);
        masm.Push(TypedOrValueRegister(MIRType_Object, AnyRegister(object)));
        masm.Push(ObjectValue(*target));
        masm.movePtr(StackPointer, argVpReg);

        // Preload other regs
        masm.loadJSContext(argJSContextReg);
        masm.move32(Imm32(1), argUintNReg);

        // Push data for GC marking
        masm.Push(argUintNReg);
        attacher.pushStubCodePointer(masm);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(ION_FRAME_OOL_NATIVE);

        // Make the call
        masm.setupUnalignedABICall(3, scratchReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argUintNReg);
        masm.passABIArg(argVpReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->native()));

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // masm.leaveExitFrame & pop locals.
        masm.adjustStack(IonOOLNativeExitFrameLayout::Size(1));
    } else {
        Register argObjReg       = regSet.takeGeneral();
        Register argIdReg        = regSet.takeGeneral();
        Register argStrictReg    = regSet.takeGeneral();

        attacher.pushStubCodePointer(masm);

        StrictPropertyOp target = shape->setterOp();
        JS_ASSERT(target);
        // JSStrictPropertyOp: bool fn(JSContext *cx, HandleObject obj,
        //                               HandleId id, bool strict, MutableHandleValue vp);

        // Push args on stack first so we can take pointers to make handles.
        if (value.constant())
            masm.Push(value.value());
        else
            masm.Push(value.reg());
        masm.movePtr(StackPointer, argVpReg);

        masm.move32(Imm32(strict ? 1 : 0), argStrictReg);

        // push canonical jsid from shape instead of propertyname.
        RootedId propId(cx);
        if (!shape->getUserId(cx, &propId))
            return false;
        masm.Push(propId, argIdReg);
        masm.movePtr(StackPointer, argIdReg);

        masm.Push(object);
        masm.movePtr(StackPointer, argObjReg);

        masm.loadJSContext(argJSContextReg);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(ION_FRAME_OOL_PROPERTY_OP);

        // Make the call.
        masm.setupUnalignedABICall(5, scratchReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argObjReg);
        masm.passABIArg(argIdReg);
        masm.passABIArg(argStrictReg);
        masm.passABIArg(argVpReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target));

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // masm.leaveExitFrame & pop locals.
        masm.adjustStack(IonOOLPropertyOpExitFrameLayout::Size());
    }

    masm.icRestoreLive(liveRegs, aic);
    return true;
}

static bool
IsCacheableDOMProxyUnshadowedSetterCall(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                        MutableHandleObject holder, MutableHandleShape shape,
                                        bool *isSetter)
{
    JS_ASSERT(IsCacheableDOMProxy(obj));

    *isSetter = false;

    RootedObject checkObj(cx, obj->getTaggedProto().toObjectOrNull());
    if (!checkObj)
        return true;

    if (!JSObject::lookupProperty(cx, obj, name, holder, shape))
        return false;

    if (!holder)
        return true;

    if (!IsCacheableSetPropCallNative(checkObj, holder, shape) &&
        !IsCacheableSetPropCallPropertyOp(checkObj, holder, shape))
    {
        return true;
    }

    *isSetter = true;
    return true;
}

bool
SetPropertyIC::attachDOMProxyUnshadowed(JSContext *cx, IonScript *ion, HandleObject obj,
                                        void *returnAddr)
{
    JS_ASSERT(IsCacheableDOMProxy(obj));

    Label failures;
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    // Guard on the shape of the object.
    masm.branchPtr(Assembler::NotEqual,
                   Address(object(), JSObject::offsetOfShape()),
                   ImmGCPtr(obj->lastProperty()), &failures);

    // Make sure object is a DOMProxy
    GenerateDOMProxyChecks(cx, masm, obj, name(), object(), &failures);

    RootedPropertyName propName(cx, name());
    RootedObject holder(cx);
    RootedShape shape(cx);
    bool isSetter;
    if (!IsCacheableDOMProxyUnshadowedSetterCall(cx, obj, propName, &holder,
                                                 &shape, &isSetter))
    {
        return false;
    }

    if (isSetter) {
        if (!GenerateCallSetter(cx, ion, masm, attacher, obj, holder, shape, strict(),
                                object(), value(), &failures, liveRegs_, returnAddr))
        {
            return false;
        }
    } else {
        // Either there was no proto, or the property wasn't appropriately found on it.
        // Drop back to just a call to Proxy::set().
        RootedId propId(cx, AtomToId(name()));
        if (!EmitCallProxySet(cx, masm, attacher, propId, liveRegs_, object(),
                            value(), returnAddr, strict()))
        {
            return false;
        }
    }

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "DOM proxy unshadowed set");
}

bool
SetPropertyIC::attachCallSetter(JSContext *cx, IonScript *ion,
                                HandleObject obj, HandleObject holder, HandleShape shape,
                                void *returnAddr)
{
    JS_ASSERT(obj->isNative());

    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    Label failure;
    masm.branchPtr(Assembler::NotEqual,
                   Address(object(), JSObject::offsetOfShape()),
                   ImmGCPtr(obj->lastProperty()),
                   &failure);

    if (!GenerateCallSetter(cx, ion, masm, attacher, obj, holder, shape, strict(),
                            object(), value(), &failure, liveRegs_, returnAddr))
    {
        return false;
    }

    // Rejoin jump.
    attacher.jumpRejoin(masm);

    // Jump to next stub.
    masm.bind(&failure);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "setter call");
}

static void
GenerateAddSlot(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                JSObject *obj, Shape *oldShape, Register object, ConstantOrRegister value,
                bool checkTypeset)
{
    JS_ASSERT(obj->isNative());

    Label failures;

    // Guard the type of the object
    masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfType()),
                   ImmGCPtr(obj->type()), &failures);

    // Guard shapes along prototype chain.
    masm.branchTestObjShape(Assembler::NotEqual, object, oldShape, &failures);

    Label failuresPopObject;
    masm.push(object);    // save object reg because we clobber it

    // Guard that the incoming value is in the type set for the property
    // if a type barrier is required.
    if (checkTypeset) {
        TypedOrValueRegister valReg = value.reg();
        types::TypeObject *type = obj->type();
        types::HeapTypeSet *propTypes = type->maybeGetProperty(obj->lastProperty()->propid());
        JS_ASSERT(propTypes);
        JS_ASSERT(!propTypes->unknown());

        Register scratchReg = object;
        masm.guardTypeSet(valReg, propTypes, scratchReg, &failuresPopObject);
        masm.loadPtr(Address(StackPointer, 0), object);
    }

    JSObject *proto = obj->getProto();
    Register protoReg = object;
    while (proto) {
        Shape *protoShape = proto->lastProperty();

        // load next prototype
        masm.loadObjProto(protoReg, protoReg);

        // Ensure that its shape matches.
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, protoShape, &failuresPopObject);

        proto = proto->getProto();
    }

    masm.pop(object);     // restore object reg

    // Changing object shape.  Write the object's new shape.
    Shape *newShape = obj->lastProperty();
    Address shapeAddr(object, JSObject::offsetOfShape());
    if (cx->zone()->needsBarrier())
        masm.callPreBarrier(shapeAddr, MIRType_Shape);
    masm.storePtr(ImmGCPtr(newShape), shapeAddr);

    // Set the value on the object. Since this is an add, obj->lastProperty()
    // must be the shape of the property we are adding.
    if (obj->isFixedSlot(newShape->slot())) {
        Address addr(object, JSObject::getFixedSlotOffset(newShape->slot()));
        masm.storeConstantOrRegister(value, addr);
    } else {
        Register slotsReg = object;

        masm.loadPtr(Address(object, JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(newShape->slot()) * sizeof(Value));
        masm.storeConstantOrRegister(value, addr);
    }

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failuresPopObject);
    masm.pop(object);
    masm.bind(&failures);

    attacher.jumpNextStub(masm);
}

bool
SetPropertyIC::attachAddSlot(JSContext *cx, IonScript *ion, JSObject *obj, HandleShape oldShape,
                             bool checkTypeset)
{
    JS_ASSERT_IF(!needsTypeBarrier(), !checkTypeset);

    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);
    GenerateAddSlot(cx, masm, attacher, obj, oldShape, object(), value(), checkTypeset);
    return linkAndAttachStub(cx, masm, attacher, ion, "adding");
}

static bool
CanInlineSetPropTypeCheck(JSObject *obj, jsid id, ConstantOrRegister val, bool *checkTypeset)
{
    bool shouldCheck = false;
    types::TypeObject *type = obj->type();
    if (!type->unknownProperties()) {
        types::HeapTypeSet *propTypes = type->maybeGetProperty(id);
        if (!propTypes)
            return false;
        if (!propTypes->unknown()) {
            shouldCheck = true;
            if (val.constant()) {
                // If the input is a constant, then don't bother if the barrier will always fail.
                if (!propTypes->hasType(types::GetValueType(val.value())))
                    return false;
                shouldCheck = false;
            } else {
                TypedOrValueRegister reg = val.reg();
                // We can do the same trick as above for primitive types of specialized registers.
                // TIs handling of objects is complicated enough to warrant a runtime
                // check, as we can't statically handle the case where the typeset
                // contains the specific object, but doesn't have ANYOBJECT set.
                if (reg.hasTyped() && reg.type() != MIRType_Object) {
                    JSValueType valType = ValueTypeFromMIRType(reg.type());
                    if (!propTypes->hasType(types::Type::PrimitiveType(valType)))
                        return false;
                    shouldCheck = false;
                }
            }
        }
    }

    *checkTypeset = shouldCheck;
    return true;
}

static bool
IsPropertySetInlineable(HandleObject obj, HandleId id, MutableHandleShape pshape,
                        ConstantOrRegister val, bool needsTypeBarrier, bool *checkTypeset)
{
    JS_ASSERT(obj->isNative());

    // Do a pure non-proto chain climbing lookup. See note in
    // CanAttachNativeGetProp.
    pshape.set(obj->nativeLookupPure(id));

    if (!pshape)
        return false;

    if (!pshape->hasSlot())
        return false;

    if (!pshape->hasDefaultSetter())
        return false;

    if (!pshape->writable())
        return false;

    if (needsTypeBarrier)
        return CanInlineSetPropTypeCheck(obj, id, val, checkTypeset);

    return true;
}

static bool
IsPropertyAddInlineable(HandleObject obj, HandleId id, ConstantOrRegister val, uint32_t oldSlots,
                        HandleShape oldShape, bool needsTypeBarrier, bool *checkTypeset)
{
    JS_ASSERT(obj->isNative());

    // If the shape of the object did not change, then this was not an add.
    if (obj->lastProperty() == oldShape)
        return false;

    Shape *shape = obj->nativeLookupPure(id);
    if (!shape || shape->inDictionary() || !shape->hasSlot() || !shape->hasDefaultSetter())
        return false;

    // If we have a shape at this point and the object's shape changed, then
    // the shape must be the one we just added.
    JS_ASSERT(shape == obj->lastProperty());

    // If object has a non-default resolve hook, don't inline
    if (obj->getClass()->resolve != JS_ResolveStub)
        return false;

    // Likewise for a non-default addProperty hook, since we'll need
    // to invoke it.
    if (obj->getClass()->addProperty != JS_PropertyStub)
        return false;

    if (!obj->nonProxyIsExtensible() || !shape->writable())
        return false;

    // Walk up the object prototype chain and ensure that all prototypes
    // are native, and that all prototypes have no getter or setter
    // defined on the property
    for (JSObject *proto = obj->getProto(); proto; proto = proto->getProto()) {
        // If prototype is non-native, don't optimize
        if (!proto->isNative())
            return false;

        // If prototype defines this property in a non-plain way, don't optimize
        Shape *protoShape = proto->nativeLookupPure(id);
        if (protoShape && !protoShape->hasDefaultSetter())
            return false;

        // Otherwise, if there's no such property, watch out for a resolve
        // hook that would need to be invoked and thus prevent inlining of
        // property addition.
        if (proto->getClass()->resolve != JS_ResolveStub)
             return false;
    }

    // Only add a IC entry if the dynamic slots didn't change when the shapes
    // changed.  Need to ensure that a shape change for a subsequent object
    // won't involve reallocating the slot array.
    if (obj->numDynamicSlots() != oldSlots)
        return false;

    if (needsTypeBarrier)
        return CanInlineSetPropTypeCheck(obj, id, val, checkTypeset);

    *checkTypeset = false;
    return true;
}

static SetPropertyIC::NativeSetPropCacheability
CanAttachNativeSetProp(HandleObject obj, HandleId id, ConstantOrRegister val,
                       bool needsTypeBarrier, MutableHandleObject holder,
                       MutableHandleShape shape, bool *checkTypeset)
{
    if (!obj->isNative())
        return SetPropertyIC::CanAttachNone;

    // See if the property exists on the object.
    if (IsPropertySetInlineable(obj, id, shape, val, needsTypeBarrier, checkTypeset))
        return SetPropertyIC::CanAttachSetSlot;

    // If we couldn't find the property on the object itself, do a full, but
    // still pure lookup for setters.
    if (!LookupPropertyPure(obj, id, holder.address(), shape.address()))
        return SetPropertyIC::CanAttachNone;

    // If the object doesn't have the property, we don't know if we can attach
    // a stub to add the property until we do the VM call to add. If the
    // property exists as a data property on the prototype, we should add
    // a new, shadowing property.
    if (!shape || (obj != holder && shape->hasDefaultSetter() && shape->hasSlot()))
        return SetPropertyIC::MaybeCanAttachAddSlot;

    if (IsCacheableSetPropCallPropertyOp(obj, holder, shape) ||
        IsCacheableSetPropCallNative(obj, holder, shape))
    {
        return SetPropertyIC::CanAttachCallSetter;
    }

    return SetPropertyIC::CanAttachNone;
}

bool
SetPropertyIC::update(JSContext *cx, size_t cacheIndex, HandleObject obj,
                      HandleValue value)
{
    AutoFlushCache afc ("SetPropertyCache", cx->runtime()->jitRuntime());

    void *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, &returnAddr));
    IonScript *ion = script->ionScript();
    SetPropertyIC &cache = ion->getCache(cacheIndex).toSetProperty();
    RootedPropertyName name(cx, cache.name());
    RootedId id(cx, AtomToId(name));

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    bool inlinable = cache.canAttachStub() && !obj->watched();
    NativeSetPropCacheability canCache = CanAttachNone;
    bool addedSetterStub = false;
    if (inlinable) {
        if (!addedSetterStub && obj->is<ProxyObject>()) {
            if (IsCacheableDOMProxy(obj)) {
                DOMProxyShadowsResult shadows = GetDOMProxyShadowsCheck()(cx, obj, id);
                if (shadows == ShadowCheckFailed)
                    return false;
                if (shadows == Shadows) {
                    if (!cache.attachDOMProxyShadowed(cx, ion, obj, returnAddr))
                        return false;
                    addedSetterStub = true;
                } else {
                    JS_ASSERT(shadows == DoesntShadow || shadows == DoesntShadowUnique);
                    if (shadows == DoesntShadowUnique)
                        cache.reset();
                    if (!cache.attachDOMProxyUnshadowed(cx, ion, obj, returnAddr))
                        return false;
                    addedSetterStub = true;
                }
            }

            if (!addedSetterStub && !cache.hasGenericProxyStub()) {
                if (!cache.attachGenericProxy(cx, ion, returnAddr))
                    return false;
                addedSetterStub = true;
            }
        }

        // Make sure the object de-lazifies its type. We do this here so that
        // the parallel IC can share code that assumes that native objects all
        // have a type object.
        if (obj->isNative() && !obj->getType(cx))
            return false;

        RootedShape shape(cx);
        RootedObject holder(cx);
        bool checkTypeset;
        canCache = CanAttachNativeSetProp(obj, id, cache.value(), cache.needsTypeBarrier(),
                                          &holder, &shape, &checkTypeset);

        if (!addedSetterStub && canCache == CanAttachSetSlot) {
            if (!cache.attachSetSlot(cx, ion, obj, shape, checkTypeset))
                return false;
            addedSetterStub = true;
        }

        if (!addedSetterStub && canCache == CanAttachCallSetter) {
            if (!cache.attachCallSetter(cx, ion, obj, holder, shape, returnAddr))
                return false;
            addedSetterStub = true;
        }
    }

    uint32_t oldSlots = obj->numDynamicSlots();
    RootedShape oldShape(cx, obj->lastProperty());

    // Set/Add the property on the object, the inlined cache are setup for the next execution.
    if (!SetProperty(cx, obj, name, value, cache.strict(), cache.pc()))
        return false;

    // The property did not exist before, now we can try to inline the property add.
    bool checkTypeset;
    if (!addedSetterStub && canCache == MaybeCanAttachAddSlot &&
        IsPropertyAddInlineable(obj, id, cache.value(), oldSlots, oldShape, cache.needsTypeBarrier(),
                                &checkTypeset))
    {
        if (!cache.attachAddSlot(cx, ion, obj, oldShape, checkTypeset))
            return false;
    }

    return true;
}

void
SetPropertyIC::reset()
{
    RepatchIonCache::reset();
    hasGenericProxyStub_ = false;
}

bool
SetPropertyParIC::update(ForkJoinContext *cx, size_t cacheIndex, HandleObject obj,
                         HandleValue value)
{
    JS_ASSERT(cx->isThreadLocal(obj));

    AutoFlushCache afc("SetPropertyParCache", cx->runtime()->jitRuntime());

    IonScript *ion = GetTopIonJSScript(cx)->parallelIonScript();
    SetPropertyParIC &cache = ion->getCache(cacheIndex).toSetPropertyPar();

    RootedValue v(cx, value);
    RootedId id(cx, AtomToId(cache.name()));

    // Avoid unnecessary locking if cannot attach stubs.
    if (!cache.canAttachStub()) {
        return baseops::SetPropertyHelper<ParallelExecution>(cx, obj, obj, id, 0,
                                                             &v, cache.strict());
    }

    SetPropertyIC::NativeSetPropCacheability canCache = SetPropertyIC::CanAttachNone;
    bool attachedStub = false;

    {
        LockedJSContext ncx(cx);

        if (cache.canAttachStub()) {
            bool alreadyStubbed;
            if (!cache.hasOrAddStubbedShape(ncx, obj->lastProperty(), &alreadyStubbed))
                return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
            if (alreadyStubbed) {
                return baseops::SetPropertyHelper<ParallelExecution>(cx, obj, obj, id, 0,
                                                                     &v, cache.strict());
            }

            // If the object has a lazy type, we need to de-lazify it, but
            // this is not safe in parallel.
            if (obj->hasLazyType())
                return false;

            {
                RootedShape shape(cx);
                RootedObject holder(cx);
                bool checkTypeset;
                canCache = CanAttachNativeSetProp(obj, id, cache.value(), cache.needsTypeBarrier(),
                                                  &holder, &shape, &checkTypeset);

                if (canCache == SetPropertyIC::CanAttachSetSlot) {
                    if (!cache.attachSetSlot(ncx, ion, obj, shape, checkTypeset))
                        return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                    attachedStub = true;
                }
            }
        }
    }

    uint32_t oldSlots = obj->numDynamicSlots();
    RootedShape oldShape(cx, obj->lastProperty());

    if (!baseops::SetPropertyHelper<ParallelExecution>(cx, obj, obj, id, 0, &v, cache.strict()))
        return false;

    bool checkTypeset;
    if (!attachedStub && canCache == SetPropertyIC::MaybeCanAttachAddSlot &&
        IsPropertyAddInlineable(obj, id, cache.value(), oldSlots, oldShape, cache.needsTypeBarrier(),
                                &checkTypeset))
    {
        LockedJSContext ncx(cx);
        if (cache.canAttachStub() && !cache.attachAddSlot(ncx, ion, obj, oldShape, checkTypeset))
            return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
    }

    return true;
}

bool
SetPropertyParIC::attachSetSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj, Shape *shape,
                                bool checkTypeset)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    GenerateSetSlot(cx, masm, attacher, obj, shape, object(), value(), needsTypeBarrier(),
                    checkTypeset);
    return linkAndAttachStub(cx, masm, attacher, ion, "parallel setting");
}

bool
SetPropertyParIC::attachAddSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj, Shape *oldShape,
                                bool checkTypeset)
{
    JS_ASSERT_IF(!needsTypeBarrier(), !checkTypeset);

    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    GenerateAddSlot(cx, masm, attacher, obj, oldShape, object(), value(), checkTypeset);
    return linkAndAttachStub(cx, masm, attacher, ion, "parallel adding");
}

const size_t GetElementIC::MAX_FAILED_UPDATES = 16;

/* static */ bool
GetElementIC::canAttachGetProp(JSObject *obj, const Value &idval, jsid id)
{
    uint32_t dummy;
    return (obj->isNative() &&
            idval.isString() &&
            JSID_IS_ATOM(id) &&
            !JSID_TO_ATOM(id)->isIndex(&dummy));
}

static bool
EqualStringsHelper(JSString *str1, JSString *str2)
{
    JS_ASSERT(str1->isAtom());
    JS_ASSERT(!str2->isAtom());
    JS_ASSERT(str1->length() == str2->length());

    const jschar *chars = str2->getChars(nullptr);
    if (!chars)
        return false;
    return mozilla::PodEqual(str1->asAtom().chars(), chars, str1->length());
}

bool
GetElementIC::attachGetProp(JSContext *cx, IonScript *ion, HandleObject obj,
                            const Value &idval, HandlePropertyName name,
                            void *returnAddr)
{
    JS_ASSERT(index().reg().hasValue());

    RootedObject holder(cx);
    RootedShape shape(cx);

    GetPropertyIC::NativeGetPropCacheability canCache =
        CanAttachNativeGetProp(cx, *this, obj, name, &holder, &shape,
                               /* skipArrayLen =*/true);

    bool cacheable = canCache == GetPropertyIC::CanAttachReadSlot ||
                     (canCache == GetPropertyIC::CanAttachCallGetter &&
                      output().hasValue());

    if (!cacheable) {
        IonSpew(IonSpew_InlineCaches, "GETELEM uncacheable property");
        return true;
    }

    JS_ASSERT(idval.isString());
    JS_ASSERT(idval.toString()->length() == name->length());

    Label failures;
    MacroAssembler masm(cx, ion);
    SkipRoot skip(cx, &masm);

    // Ensure the index is a string.
    ValueOperand val = index().reg().valueReg();
    masm.branchTestString(Assembler::NotEqual, val, &failures);

    Register scratch = output().valueReg().scratchReg();
    masm.unboxString(val, scratch);

    Label equal;
    masm.branchPtr(Assembler::Equal, scratch, ImmGCPtr(name), &equal);

    // The pointers are not equal, so if the input string is also an atom it
    // must be a different string.
    masm.loadPtr(Address(scratch, JSString::offsetOfLengthAndFlags()), scratch);
    masm.branchTest32(Assembler::NonZero, scratch, Imm32(JSString::ATOM_BIT), &failures);

    // Check the length.
    masm.rshiftPtr(Imm32(JSString::LENGTH_SHIFT), scratch);
    masm.branch32(Assembler::NotEqual, scratch, Imm32(name->length()), &failures);

    // We have a non-atomized string with the same length. For now call a helper
    // function to do the comparison.
    RegisterSet volatileRegs = RegisterSet::Volatile();
    masm.PushRegsInMask(volatileRegs);

    Register objReg = object();
    JS_ASSERT(objReg != scratch);

    if (!volatileRegs.has(objReg))
        masm.push(objReg);

    masm.setupUnalignedABICall(2, scratch);
    masm.movePtr(ImmGCPtr(name), objReg);
    masm.passABIArg(objReg);
    masm.unboxString(val, scratch);
    masm.passABIArg(scratch);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, EqualStringsHelper));
    masm.mov(ReturnReg, scratch);

    if (!volatileRegs.has(objReg))
        masm.pop(objReg);

    RegisterSet ignore = RegisterSet();
    ignore.add(scratch);
    masm.PopRegsInMaskIgnore(volatileRegs, ignore);

    masm.branchIfFalseBool(scratch, &failures);
    masm.bind(&equal);

    RepatchStubAppender attacher(*this);
    if (canCache == GetPropertyIC::CanAttachReadSlot) {
        GenerateReadSlot(cx, ion, masm, attacher, obj, holder, shape, object(), output(),
                         &failures);
    } else {
        JS_ASSERT(canCache == GetPropertyIC::CanAttachCallGetter);
        // Set the frame for bailout safety of the OOL call.
        if (!GenerateCallGetter(cx, ion, masm, attacher, obj, name, holder, shape, liveRegs_,
                                object(), output(), returnAddr, &failures))
        {
            return false;
        }
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "property");
}

/* static */ bool
GetElementIC::canAttachDenseElement(JSObject *obj, const Value &idval)
{
    return obj->isNative() && idval.isInt32();
}

static bool
GenerateDenseElement(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                     JSObject *obj, const Value &idval, Register object,
                     ConstantOrRegister index, TypedOrValueRegister output)
{
    JS_ASSERT(GetElementIC::canAttachDenseElement(obj, idval));

    Label failures;

    // Guard object's shape.
    RootedShape shape(cx, obj->lastProperty());
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Ensure the index is an int32 value.
    Register indexReg = InvalidReg;

    if (index.reg().hasValue()) {
        indexReg = output.scratchReg().gpr();
        JS_ASSERT(indexReg != InvalidReg);
        ValueOperand val = index.reg().valueReg();

        masm.branchTestInt32(Assembler::NotEqual, val, &failures);

        // Unbox the index.
        masm.unboxInt32(val, indexReg);
    } else {
        JS_ASSERT(!index.reg().typedReg().isFloat());
        indexReg = index.reg().typedReg().gpr();
    }

    // Load elements vector.
    masm.push(object);
    masm.loadPtr(Address(object, JSObject::offsetOfElements()), object);

    Label hole;

    // Guard on the initialized length.
    Address initLength(object, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, indexReg, &hole);

    // Check for holes & load the value.
    masm.loadElementTypedOrValue(BaseIndex(object, indexReg, TimesEight),
                                 output, true, &hole);

    masm.pop(object);
    attacher.jumpRejoin(masm);

    // All failures flow to here.
    masm.bind(&hole);
    masm.pop(object);
    masm.bind(&failures);

    attacher.jumpNextStub(masm);

    return true;
}

bool
GetElementIC::attachDenseElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval)
{
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);
    if (!GenerateDenseElement(cx, masm, attacher, obj, idval, object(), index(), output()))
        return false;

    setHasDenseStub();
    return linkAndAttachStub(cx, masm, attacher, ion, "dense array");
}

/* static */ bool
GetElementIC::canAttachTypedArrayElement(JSObject *obj, const Value &idval,
                                         TypedOrValueRegister output)
{
    if (!obj->is<TypedArrayObject>())
        return false;

    if (!idval.isInt32() && !idval.isString())
        return false;


    // Don't emit a stub if the access is out of bounds. We make to make
    // certain that we monitor the type coming out of the typed array when
    // we generate the stub. Out of bounds accesses will hit the fallback
    // path.
    uint32_t index;
    if (idval.isInt32()) {
        index = idval.toInt32();
    } else {
        index = GetIndexFromString(idval.toString());
        if (index == UINT32_MAX)
            return false;
    }
    if (index >= obj->as<TypedArrayObject>().length())
        return false;

    // The output register is not yet specialized as a float register, the only
    // way to accept float typed arrays for now is to return a Value type.
    uint32_t arrayType = obj->as<TypedArrayObject>().type();
    if (arrayType == ScalarTypeRepresentation::TYPE_FLOAT32 ||
        arrayType == ScalarTypeRepresentation::TYPE_FLOAT64)
    {
        return output.hasValue();
    }

    return output.hasValue() || !output.typedReg().isFloat();
}

static void
GenerateGetTypedArrayElement(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                             TypedArrayObject *tarr, const Value &idval, Register object,
                             ConstantOrRegister index, TypedOrValueRegister output,
                             bool allowDoubleResult)
{
    JS_ASSERT(GetElementIC::canAttachTypedArrayElement(tarr, idval, output));

    Label failures;

    // The array type is the object within the table of typed array classes.
    int arrayType = tarr->type();

    // Guard on the shape.
    Shape *shape = tarr->lastProperty();
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Decide to what type index the stub should be optimized
    Register tmpReg = output.scratchReg().gpr();
    JS_ASSERT(tmpReg != InvalidReg);
    Register indexReg = tmpReg;
    JS_ASSERT(!index.constant());
    if (idval.isString()) {
        JS_ASSERT(GetIndexFromString(idval.toString()) != UINT32_MAX);

        // Part 1: Get the string into a register
        Register str;
        if (index.reg().hasValue()) {
            ValueOperand val = index.reg().valueReg();
            masm.branchTestString(Assembler::NotEqual, val, &failures);

            str = masm.extractString(val, indexReg);
        } else {
            JS_ASSERT(!index.reg().typedReg().isFloat());
            str = index.reg().typedReg().gpr();
        }

        // Part 2: Call to translate the str into index
        RegisterSet regs = RegisterSet::Volatile();
        masm.PushRegsInMask(regs);
        regs.takeUnchecked(str);

        Register temp = regs.takeGeneral();

        masm.setupUnalignedABICall(1, temp);
        masm.passABIArg(str);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, GetIndexFromString));
        masm.mov(ReturnReg, indexReg);

        RegisterSet ignore = RegisterSet();
        ignore.add(indexReg);
        masm.PopRegsInMaskIgnore(RegisterSet::Volatile(), ignore);

        masm.branch32(Assembler::Equal, indexReg, Imm32(UINT32_MAX), &failures);

    } else {
        JS_ASSERT(idval.isInt32());

        if (index.reg().hasValue()) {
            ValueOperand val = index.reg().valueReg();
            masm.branchTestInt32(Assembler::NotEqual, val, &failures);

            // Unbox the index.
            masm.unboxInt32(val, indexReg);
        } else {
            JS_ASSERT(!index.reg().typedReg().isFloat());
            indexReg = index.reg().typedReg().gpr();
        }
    }

    // Guard on the initialized length.
    Address length(object, TypedArrayObject::lengthOffset());
    masm.branch32(Assembler::BelowOrEqual, length, indexReg, &failures);

    // Save the object register on the stack in case of failure.
    Label popAndFail;
    Register elementReg = object;
    masm.push(object);

    // Load elements vector.
    masm.loadPtr(Address(object, TypedArrayObject::dataOffset()), elementReg);

    // Load the value. We use an invalid register because the destination
    // register is necessary a non double register.
    int width = TypedArrayObject::slotWidth(arrayType);
    BaseIndex source(elementReg, indexReg, ScaleFromElemWidth(width));
    if (output.hasValue()) {
        masm.loadFromTypedArray(arrayType, source, output.valueReg(), allowDoubleResult,
                                elementReg, &popAndFail);
    } else {
        masm.loadFromTypedArray(arrayType, source, output.typedReg(), elementReg, &popAndFail);
    }

    masm.pop(object);
    attacher.jumpRejoin(masm);

    // Restore the object before continuing to the next stub.
    masm.bind(&popAndFail);
    masm.pop(object);
    masm.bind(&failures);

    attacher.jumpNextStub(masm);
}

bool
GetElementIC::attachTypedArrayElement(JSContext *cx, IonScript *ion, TypedArrayObject *tarr,
                                      const Value &idval)
{
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);
    GenerateGetTypedArrayElement(cx, masm, attacher, tarr, idval, object(), index(), output(),
                                 allowDoubleResult());
    return linkAndAttachStub(cx, masm, attacher, ion, "typed array");
}

bool
GetElementIC::attachArgumentsElement(JSContext *cx, IonScript *ion, JSObject *obj)
{
    JS_ASSERT(obj->is<ArgumentsObject>());

    Label failures;
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    Register tmpReg = output().scratchReg().gpr();
    JS_ASSERT(tmpReg != InvalidReg);

    const Class *clasp = obj->is<StrictArgumentsObject>() ? &StrictArgumentsObject::class_
                                                          : &NormalArgumentsObject::class_;

    masm.branchTestObjClass(Assembler::NotEqual, object(), tmpReg, clasp, &failures);

    // Get initial ArgsObj length value, test if length has been overridden.
    masm.unboxInt32(Address(object(), ArgumentsObject::getInitialLengthSlotOffset()), tmpReg);
    masm.branchTest32(Assembler::NonZero, tmpReg, Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      &failures);
    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), tmpReg);

    // Decide to what type index the stub should be optimized
    Register indexReg;
    JS_ASSERT(!index().constant());

    // Check index against length.
    Label failureRestoreIndex;
    if (index().reg().hasValue()) {
        ValueOperand val = index().reg().valueReg();
        masm.branchTestInt32(Assembler::NotEqual, val, &failures);
        indexReg = val.scratchReg();

        masm.unboxInt32(val, indexReg);
        masm.branch32(Assembler::AboveOrEqual, indexReg, tmpReg, &failureRestoreIndex);
    } else {
        JS_ASSERT(index().reg().type() == MIRType_Int32);
        indexReg = index().reg().typedReg().gpr();
        masm.branch32(Assembler::AboveOrEqual, indexReg, tmpReg, &failures);
    }
    // Save indexReg because it needs to be clobbered to check deleted bit.
    Label failurePopIndex;
    masm.push(indexReg);

    // Check if property was deleted on arguments object.
    masm.loadPrivate(Address(object(), ArgumentsObject::getDataSlotOffset()), tmpReg);
    masm.loadPtr(Address(tmpReg, offsetof(ArgumentsData, deletedBits)), tmpReg);

    // In tempReg, calculate index of word containing bit: (idx >> logBitsPerWord)
    const uint32_t shift = FloorLog2<(sizeof(size_t) * JS_BITS_PER_BYTE)>::value;
    JS_ASSERT(shift == 5 || shift == 6);
    masm.rshiftPtr(Imm32(shift), indexReg);
    masm.loadPtr(BaseIndex(tmpReg, indexReg, ScaleFromElemWidth(sizeof(size_t))), tmpReg);

    // Don't bother testing specific bit, if any bit is set in the word, fail.
    masm.branchPtr(Assembler::NotEqual, tmpReg, ImmPtr(nullptr), &failurePopIndex);

    // Get the address to load from into tmpReg
    masm.loadPrivate(Address(object(), ArgumentsObject::getDataSlotOffset()), tmpReg);
    masm.addPtr(Imm32(ArgumentsData::offsetOfArgs()), tmpReg);

    // Restore original index register value, to use for indexing element.
    masm.pop(indexReg);
    BaseIndex elemIdx(tmpReg, indexReg, ScaleFromElemWidth(sizeof(Value)));

    // Ensure result is not magic value, and type-check result.
    masm.branchTestMagic(Assembler::Equal, elemIdx, &failureRestoreIndex);

    if (output().hasTyped()) {
        JS_ASSERT(!output().typedReg().isFloat());
        JS_ASSERT(index().reg().type() == MIRType_Boolean ||
                  index().reg().type() == MIRType_Int32 ||
                  index().reg().type() == MIRType_String ||
                  index().reg().type() == MIRType_Object);
        masm.branchTestMIRType(Assembler::NotEqual, elemIdx, index().reg().type(),
                               &failureRestoreIndex);
    }

    masm.loadTypedOrValue(elemIdx, output());

    // indexReg may need to be reconstructed if it was originally a value.
    if (index().reg().hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, indexReg, index().reg().valueReg());

    // Success.
    attacher.jumpRejoin(masm);

    // Restore the object before continuing to the next stub.
    masm.bind(&failurePopIndex);
    masm.pop(indexReg);
    masm.bind(&failureRestoreIndex);
    if (index().reg().hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, indexReg, index().reg().valueReg());
    masm.bind(&failures);
    attacher.jumpNextStub(masm);


    if (obj->is<StrictArgumentsObject>()) {
        JS_ASSERT(!hasStrictArgumentsStub_);
        hasStrictArgumentsStub_ = true;
        return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj element (strict)");
    }

    JS_ASSERT(!hasNormalArgumentsStub_);
    hasNormalArgumentsStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj element (normal)");
}

bool
GetElementIC::update(JSContext *cx, size_t cacheIndex, HandleObject obj,
                     HandleValue idval, MutableHandleValue res)
{
    void *returnAddr;
    IonScript *ion = GetTopIonJSScript(cx, &returnAddr)->ionScript();
    GetElementIC &cache = ion->getCache(cacheIndex).toGetElement();
    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    // Override the return value when the script is invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, res.address(), ion);

    if (cache.isDisabled()) {
        if (!GetObjectElementOperation(cx, JSOp(*pc), obj, /* wasObject = */true, idval, res))
            return false;
        if (!cache.monitoredResult())
            types::TypeScript::Monitor(cx, script, pc, res);
        return true;
    }

    AutoFlushCache afc("GetElementCache", cx->runtime()->jitRuntime());

    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, idval, &id))
        return false;

    bool attachedStub = false;
    if (cache.canAttachStub()) {
        if (IsOptimizableArgumentsObjectForGetElem(obj, idval) &&
            !cache.hasArgumentsStub(obj->is<StrictArgumentsObject>()) &&
            !cache.index().constant() &&
            (cache.index().reg().hasValue() ||
             cache.index().reg().type() == MIRType_Int32) &&
            (cache.output().hasValue() || !cache.output().typedReg().isFloat()))
        {
            if (!cache.attachArgumentsElement(cx, ion, obj))
                return false;
            attachedStub = true;
        }
        if (!attachedStub && cache.monitoredResult() && canAttachGetProp(obj, idval, id)) {
            RootedPropertyName name(cx, JSID_TO_ATOM(id)->asPropertyName());
            if (!cache.attachGetProp(cx, ion, obj, idval, name, returnAddr))
                return false;
            attachedStub = true;
        }
        if (!attachedStub && !cache.hasDenseStub() && canAttachDenseElement(obj, idval)) {
            if (!cache.attachDenseElement(cx, ion, obj, idval))
                return false;
            attachedStub = true;
        }
        if (!attachedStub && canAttachTypedArrayElement(obj, idval, cache.output())) {
            Rooted<TypedArrayObject*> tarr(cx, &obj->as<TypedArrayObject>());
            if (!cache.attachTypedArrayElement(cx, ion, tarr, idval))
                return false;
            attachedStub = true;
        }
    }

    if (!GetObjectElementOperation(cx, JSOp(*pc), obj, /* wasObject = */true, idval, res))
        return false;

    // Disable cache when we reach max stubs or update failed too much.
    if (!attachedStub) {
        cache.incFailedUpdates();
        if (cache.shouldDisable()) {
            IonSpew(IonSpew_InlineCaches, "Disable inline cache");
            cache.disable();
        }
    } else {
        cache.resetFailedUpdates();
    }

    if (!cache.monitoredResult())
        types::TypeScript::Monitor(cx, script, pc, res);
    return true;
}

void
GetElementIC::reset()
{
    RepatchIonCache::reset();
    hasDenseStub_ = false;
    hasStrictArgumentsStub_ = false;
    hasNormalArgumentsStub_ = false;
}

static bool
IsDenseElementSetInlineable(JSObject *obj, const Value &idval)
{
    if (!obj->is<ArrayObject>())
        return false;

    if (obj->watched())
        return false;

    if (!idval.isInt32())
        return false;

    // The object may have a setter definition,
    // either directly, or via a prototype, or via the target object for a prototype
    // which is a proxy, that handles a particular integer write.
    // Scan the prototype and shape chain to make sure that this is not the case.
    JSObject *curObj = obj;
    while (curObj) {
        // Ensure object is native.
        if (!curObj->isNative())
            return false;

        // Ensure all indexed properties are stored in dense elements.
        if (curObj->isIndexed())
            return false;

        curObj = curObj->getProto();
    }

    return true;
}

static bool
IsTypedArrayElementSetInlineable(JSObject *obj, const Value &idval, const Value &value)
{
    // Don't bother attaching stubs for assigning strings and objects.
    return (obj->is<TypedArrayObject>() && idval.isInt32() &&
            !value.isString() && !value.isObject());
}

static void
StoreDenseElement(MacroAssembler &masm, ConstantOrRegister value, Register elements,
                  BaseIndex target)
{
    // If the ObjectElements::CONVERT_DOUBLE_ELEMENTS flag is set, int32 values
    // have to be converted to double first. If the value is not int32, it can
    // always be stored directly.

    Address elementsFlags(elements, ObjectElements::offsetOfFlags());
    if (value.constant()) {
        Value v = value.value();
        Label done;
        if (v.isInt32()) {
            Label dontConvert;
            masm.branchTest32(Assembler::Zero, elementsFlags,
                              Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                              &dontConvert);
            masm.storeValue(DoubleValue(v.toInt32()), target);
            masm.jump(&done);
            masm.bind(&dontConvert);
        }
        masm.storeValue(v, target);
        masm.bind(&done);
        return;
    }

    TypedOrValueRegister reg = value.reg();
    if (reg.hasTyped() && reg.type() != MIRType_Int32) {
        masm.storeTypedOrValue(reg, target);
        return;
    }

    Label convert, storeValue, done;
    masm.branchTest32(Assembler::NonZero, elementsFlags,
                      Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                      &convert);
    masm.bind(&storeValue);
    masm.storeTypedOrValue(reg, target);
    masm.jump(&done);

    masm.bind(&convert);
    if (reg.hasValue()) {
        masm.branchTestInt32(Assembler::NotEqual, reg.valueReg(), &storeValue);
        masm.int32ValueToDouble(reg.valueReg(), ScratchFloatReg);
        masm.storeDouble(ScratchFloatReg, target);
    } else {
        JS_ASSERT(reg.type() == MIRType_Int32);
        masm.convertInt32ToDouble(reg.typedReg().gpr(), ScratchFloatReg);
        masm.storeDouble(ScratchFloatReg, target);
    }

    masm.bind(&done);
}

static bool
GenerateSetDenseElement(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                        JSObject *obj, const Value &idval, bool guardHoles, Register object,
                        ValueOperand indexVal, ConstantOrRegister value, Register tempToUnboxIndex,
                        Register temp)
{
    JS_ASSERT(obj->isNative());
    JS_ASSERT(idval.isInt32());

    Label failures;
    Label outOfBounds; // index represents a known hole, or an illegal append

    Label markElem, storeElement; // used if TI protects us from worrying about holes.

    // Guard object is a dense array.
    Shape *shape = obj->lastProperty();
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Ensure the index is an int32 value.
    masm.branchTestInt32(Assembler::NotEqual, indexVal, &failures);

    // Unbox the index.
    Register index = masm.extractInt32(indexVal, tempToUnboxIndex);

    {
        // Load obj->elements.
        Register elements = temp;
        masm.loadPtr(Address(object, JSObject::offsetOfElements()), elements);

        // Compute the location of the element.
        BaseIndex target(elements, index, TimesEight);

        // If TI cannot help us deal with HOLES by preventing indexed properties
        // on the prototype chain, we have to be very careful to check for ourselves
        // to avoid stomping on what should be a setter call. Start by only allowing things
        // within the initialized length.
        if (guardHoles) {
            Address initLength(elements, ObjectElements::offsetOfInitializedLength());
            masm.branch32(Assembler::BelowOrEqual, initLength, index, &outOfBounds);
        } else {
            // Guard that we can increase the initialized length.
            Address capacity(elements, ObjectElements::offsetOfCapacity());
            masm.branch32(Assembler::BelowOrEqual, capacity, index, &outOfBounds);

            // Guard on the initialized length.
            Address initLength(elements, ObjectElements::offsetOfInitializedLength());
            masm.branch32(Assembler::Below, initLength, index, &outOfBounds);

            // if (initLength == index)
            masm.branch32(Assembler::NotEqual, initLength, index, &markElem);
            {
                // Increase initialize length.
                Int32Key newLength(index);
                masm.bumpKey(&newLength, 1);
                masm.storeKey(newLength, initLength);

                // Increase length if needed.
                Label bumpedLength;
                Address length(elements, ObjectElements::offsetOfLength());
                masm.branch32(Assembler::AboveOrEqual, length, index, &bumpedLength);
                masm.storeKey(newLength, length);
                masm.bind(&bumpedLength);

                // Restore the index.
                masm.bumpKey(&newLength, -1);
                masm.jump(&storeElement);
            }
            // else
            masm.bind(&markElem);
        }

        if (cx->zone()->needsBarrier())
            masm.callPreBarrier(target, MIRType_Value);

        // Store the value.
        if (guardHoles)
            masm.branchTestMagic(Assembler::Equal, target, &failures);
        else
            masm.bind(&storeElement);
        StoreDenseElement(masm, value, elements, target);
    }
    attacher.jumpRejoin(masm);

    // All failures flow to here.
    masm.bind(&outOfBounds);
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return true;
}

bool
SetElementIC::attachDenseElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval)
{
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);
    if (!GenerateSetDenseElement(cx, masm, attacher, obj, idval,
                                 guardHoles(), object(), index(),
                                 value(), tempToUnboxIndex(),
                                 temp()))
    {
        return false;
    }

    setHasDenseStub();
    const char *message = guardHoles()            ?
                            "dense array (holes)" :
                            "dense array";
    return linkAndAttachStub(cx, masm, attacher, ion, message);
}

static bool
GenerateSetTypedArrayElement(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                             TypedArrayObject *tarr, Register object,
                             ValueOperand indexVal, ConstantOrRegister value,
                             Register tempUnbox, Register temp, FloatRegister tempFloat)
{
    Label failures, done, popObjectAndFail;

    // Guard on the shape.
    Shape *shape = tarr->lastProperty();
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Ensure the index is an int32.
    masm.branchTestInt32(Assembler::NotEqual, indexVal, &failures);
    Register index = masm.extractInt32(indexVal, tempUnbox);

    // Guard on the length.
    Address length(object, TypedArrayObject::lengthOffset());
    masm.unboxInt32(length, temp);
    masm.branch32(Assembler::BelowOrEqual, temp, index, &done);

    // Load the elements vector.
    Register elements = temp;
    masm.loadPtr(Address(object, TypedArrayObject::dataOffset()), elements);

    // Set the value.
    int arrayType = tarr->type();
    int width = TypedArrayObject::slotWidth(arrayType);
    BaseIndex target(elements, index, ScaleFromElemWidth(width));

    if (arrayType == ScalarTypeRepresentation::TYPE_FLOAT32) {
        if (LIRGenerator::allowFloat32Optimizations()) {
            if (!masm.convertConstantOrRegisterToFloat(cx, value, tempFloat, &failures))
                return false;
        } else {
            if (!masm.convertConstantOrRegisterToDouble(cx, value, tempFloat, &failures))
                return false;
        }
        masm.storeToTypedFloatArray(arrayType, tempFloat, target);
    } else if (arrayType == ScalarTypeRepresentation::TYPE_FLOAT64) {
        if (!masm.convertConstantOrRegisterToDouble(cx, value, tempFloat, &failures))
            return false;
        masm.storeToTypedFloatArray(arrayType, tempFloat, target);
    } else {
        // On x86 we only have 6 registers available to use, so reuse the object
        // register to compute the intermediate value to store and restore it
        // afterwards.
        masm.push(object);

        if (arrayType == ScalarTypeRepresentation::TYPE_UINT8_CLAMPED) {
            if (!masm.clampConstantOrRegisterToUint8(cx, value, tempFloat, object,
                                                     &popObjectAndFail))
            {
                return false;
            }
        } else {
            if (!masm.truncateConstantOrRegisterToInt32(cx, value, tempFloat, object,
                                                        &popObjectAndFail))
            {
                return false;
            }
        }
        masm.storeToTypedIntArray(arrayType, object, target);

        masm.pop(object);
    }

    // Out-of-bound writes jump here as they are no-ops.
    masm.bind(&done);
    attacher.jumpRejoin(masm);

    if (popObjectAndFail.used()) {
        masm.bind(&popObjectAndFail);
        masm.pop(object);
    }

    masm.bind(&failures);
    attacher.jumpNextStub(masm);
    return true;
}

bool
SetElementIC::attachTypedArrayElement(JSContext *cx, IonScript *ion, TypedArrayObject *tarr)
{
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);
    if (!GenerateSetTypedArrayElement(cx, masm, attacher, tarr,
                                      object(), index(), value(),
                                      tempToUnboxIndex(), temp(), tempFloat()))
    {
        return false;
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "typed array");
}

bool
SetElementIC::update(JSContext *cx, size_t cacheIndex, HandleObject obj,
                     HandleValue idval, HandleValue value)
{
    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    SetElementIC &cache = ion->getCache(cacheIndex).toSetElement();

    bool attachedStub = false;
    if (cache.canAttachStub()) {
        if (!cache.hasDenseStub() && IsDenseElementSetInlineable(obj, idval)) {
            if (!cache.attachDenseElement(cx, ion, obj, idval))
                return false;
            attachedStub = true;
        }
        if (!attachedStub && IsTypedArrayElementSetInlineable(obj, idval, value)) {
            TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
            if (!cache.attachTypedArrayElement(cx, ion, tarr))
                return false;
        }
    }

    if (!SetObjectElement(cx, obj, idval, value, cache.strict()))
        return false;
    return true;
}

void
SetElementIC::reset()
{
    RepatchIonCache::reset();
    hasDenseStub_ = false;
}

bool
SetElementParIC::attachDenseElement(LockedJSContext &cx, IonScript *ion, JSObject *obj,
                                    const Value &idval)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    if (!GenerateSetDenseElement(cx, masm, attacher, obj, idval,
                                 guardHoles(), object(), index(),
                                 value(), tempToUnboxIndex(),
                                 temp()))
    {
        return false;
    }

    const char *message = guardHoles()                     ?
                            "parallel dense array (holes)" :
                            "parallel dense array";

    return linkAndAttachStub(cx, masm, attacher, ion, message);
}

bool
SetElementParIC::attachTypedArrayElement(LockedJSContext &cx, IonScript *ion,
                                         TypedArrayObject *tarr)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    if (!GenerateSetTypedArrayElement(cx, masm, attacher, tarr,
                                      object(), index(), value(),
                                      tempToUnboxIndex(), temp(), tempFloat()))
    {
        return false;
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel typed array");
}

bool
SetElementParIC::update(ForkJoinContext *cx, size_t cacheIndex, HandleObject obj,
                        HandleValue idval, HandleValue value)
{
    IonScript *ion = GetTopIonJSScript(cx)->parallelIonScript();
    SetElementParIC &cache = ion->getCache(cacheIndex).toSetElementPar();

    // Avoid unnecessary locking if cannot attach stubs.
    if (!cache.canAttachStub())
        return SetElementPar(cx, obj, idval, value, cache.strict());

    {
        LockedJSContext ncx(cx);

        if (cache.canAttachStub()) {
            bool alreadyStubbed;
            if (!cache.hasOrAddStubbedShape(ncx, obj->lastProperty(), &alreadyStubbed))
                return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
            if (alreadyStubbed)
                return SetElementPar(cx, obj, idval, value, cache.strict());

            bool attachedStub = false;
            if (IsDenseElementSetInlineable(obj, idval)) {
                if (!cache.attachDenseElement(ncx, ion, obj, idval))
                    return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                attachedStub = true;
            }
            if (!attachedStub && IsTypedArrayElementSetInlineable(obj, idval, value)) {
                TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
                if (!cache.attachTypedArrayElement(ncx, ion, tarr))
                    return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
            }
        }
    }

    return SetElementPar(cx, obj, idval, value, cache.strict());
}

bool
GetElementParIC::attachReadSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj,
                                const Value &idval, PropertyName *name, JSObject *holder,
                                Shape *shape)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);

    // Guard on the index value.
    Label failures;
    ValueOperand val = index().reg().valueReg();
    masm.branchTestValue(Assembler::NotEqual, val, idval, &failures);

    GenerateReadSlot(cx, ion, masm, attacher, obj, holder, shape, object(), output(),
                     &failures);

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel getelem reading");
}

bool
GetElementParIC::attachDenseElement(LockedJSContext &cx, IonScript *ion, JSObject *obj,
                                    const Value &idval)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    if (!GenerateDenseElement(cx, masm, attacher, obj, idval, object(), index(), output()))
        return false;

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel dense element");
}

bool
GetElementParIC::attachTypedArrayElement(LockedJSContext &cx, IonScript *ion,
                                         TypedArrayObject *tarr, const Value &idval)
{
    MacroAssembler masm(cx, ion);
    DispatchStubPrepender attacher(*this);
    GenerateGetTypedArrayElement(cx, masm, attacher, tarr, idval, object(), index(), output(),
                                 allowDoubleResult());
    return linkAndAttachStub(cx, masm, attacher, ion, "parallel typed array");
}

bool
GetElementParIC::update(ForkJoinContext *cx, size_t cacheIndex, HandleObject obj,
                        HandleValue idval, MutableHandleValue vp)
{
    AutoFlushCache afc("GetElementParCache", cx->runtime()->jitRuntime());

    IonScript *ion = GetTopIonJSScript(cx)->parallelIonScript();
    GetElementParIC &cache = ion->getCache(cacheIndex).toGetElementPar();

    // Try to get the element early, as the pure path doesn't need a lock. If
    // we can't do it purely, bail out of parallel execution.
    if (!GetObjectElementOperationPure(cx, obj, idval, vp.address()))
        return false;

    // Avoid unnecessary locking if cannot attach stubs.
    if (!cache.canAttachStub())
        return true;

    {
        LockedJSContext ncx(cx);

        if (cache.canAttachStub()) {
            bool alreadyStubbed;
            if (!cache.hasOrAddStubbedShape(ncx, obj->lastProperty(), &alreadyStubbed))
                return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
            if (alreadyStubbed)
                return true;

            jsid id;
            if (!ValueToIdPure(idval, &id))
                return false;

            bool attachedStub = false;
            if (cache.monitoredResult() &&
                GetElementIC::canAttachGetProp(obj, idval, id))
            {
                RootedShape shape(ncx);
                RootedObject holder(ncx);
                RootedPropertyName name(ncx, JSID_TO_ATOM(id)->asPropertyName());

                GetPropertyIC::NativeGetPropCacheability canCache =
                    CanAttachNativeGetProp(ncx, cache, obj, name, &holder, &shape);

                if (canCache == GetPropertyIC::CanAttachReadSlot)
                {
                    if (!cache.attachReadSlot(ncx, ion, obj, idval, name, holder, shape))
                        return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                    attachedStub = true;
                }
            }
            if (!attachedStub &&
                GetElementIC::canAttachDenseElement(obj, idval))
            {
                if (!cache.attachDenseElement(ncx, ion, obj, idval))
                    return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                attachedStub = true;
            }
            if (!attachedStub &&
                GetElementIC::canAttachTypedArrayElement(obj, idval, cache.output()))
            {
                if (!cache.attachTypedArrayElement(ncx, ion, &obj->as<TypedArrayObject>(), idval))
                    return cx->setPendingAbortFatal(ParallelBailoutFailedIC);
                attachedStub = true;
            }
        }
    }

    return true;
}

bool
BindNameIC::attachGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain)
{
    JS_ASSERT(scopeChain->is<GlobalObject>());

    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    // Guard on the scope chain.
    attacher.branchNextStub(masm, Assembler::NotEqual, scopeChainReg(),
                            ImmGCPtr(scopeChain));
    masm.movePtr(ImmGCPtr(scopeChain), outputReg());

    attacher.jumpRejoin(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "global");
}

static inline void
GenerateScopeChainGuard(MacroAssembler &masm, JSObject *scopeObj,
                        Register scopeObjReg, Shape *shape, Label *failures)
{
    if (scopeObj->is<CallObject>()) {
        // We can skip a guard on the call object if the script's bindings are
        // guaranteed to be immutable (and thus cannot introduce shadowing
        // variables).
        CallObject *callObj = &scopeObj->as<CallObject>();
        if (!callObj->isForEval()) {
            JSFunction *fun = &callObj->callee();
            // The function might have been relazified under rare conditions.
            // In that case, we pessimistically create the guard, as we'd
            // need to root various pointers to delazify,
            if (fun->hasScript()) {
                JSScript *script = fun->nonLazyScript();
                if (!script->funHasExtensibleScope())
                    return;
            }
        }
    } else if (scopeObj->is<GlobalObject>()) {
        // If this is the last object on the scope walk, and the property we've
        // found is not configurable, then we don't need a shape guard because
        // the shape cannot be removed.
        if (shape && !shape->configurable())
            return;
    }

    Address shapeAddr(scopeObjReg, JSObject::offsetOfShape());
    masm.branchPtr(Assembler::NotEqual, shapeAddr, ImmGCPtr(scopeObj->lastProperty()), failures);
}

static void
GenerateScopeChainGuards(MacroAssembler &masm, JSObject *scopeChain, JSObject *holder,
                         Register outputReg, Label *failures, bool skipLastGuard = false)
{
    JSObject *tobj = scopeChain;

    // Walk up the scope chain. Note that IsCacheableScopeChain guarantees the
    // |tobj == holder| condition terminates the loop.
    while (true) {
        JS_ASSERT(IsCacheableNonGlobalScope(tobj) || tobj->is<GlobalObject>());

        if (skipLastGuard && tobj == holder)
            break;

        GenerateScopeChainGuard(masm, tobj, outputReg, nullptr, failures);

        if (tobj == holder)
            break;

        // Load the next link.
        tobj = &tobj->as<ScopeObject>().enclosingScope();
        masm.extractObject(Address(outputReg, ScopeObject::offsetOfEnclosingScope()), outputReg);
    }
}

bool
BindNameIC::attachNonGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain, JSObject *holder)
{
    JS_ASSERT(IsCacheableNonGlobalScope(scopeChain));

    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    // Guard on the shape of the scope chain.
    Label failures;
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(scopeChainReg(), JSObject::offsetOfShape()),
                                   ImmGCPtr(scopeChain->lastProperty()),
                                   holder != scopeChain ? &failures : nullptr);

    if (holder != scopeChain) {
        JSObject *parent = &scopeChain->as<ScopeObject>().enclosingScope();
        masm.extractObject(Address(scopeChainReg(), ScopeObject::offsetOfEnclosingScope()), outputReg());

        GenerateScopeChainGuards(masm, parent, holder, outputReg(), &failures);
    } else {
        masm.movePtr(scopeChainReg(), outputReg());
    }

    // At this point outputReg holds the object on which the property
    // was found, so we're done.
    attacher.jumpRejoin(masm);

    // All failures flow to here, so there is a common point to patch.
    if (holder != scopeChain) {
        masm.bind(&failures);
        attacher.jumpNextStub(masm);
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "non-global");
}

static bool
IsCacheableScopeChain(JSObject *scopeChain, JSObject *holder)
{
    while (true) {
        if (!IsCacheableNonGlobalScope(scopeChain)) {
            IonSpew(IonSpew_InlineCaches, "Non-cacheable object on scope chain");
            return false;
        }

        if (scopeChain == holder)
            return true;

        scopeChain = &scopeChain->as<ScopeObject>().enclosingScope();
        if (!scopeChain) {
            IonSpew(IonSpew_InlineCaches, "Scope chain indirect hit");
            return false;
        }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid scope chain");
}

JSObject *
BindNameIC::update(JSContext *cx, size_t cacheIndex, HandleObject scopeChain)
{
    AutoFlushCache afc ("BindNameCache", cx->runtime()->jitRuntime());

    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    BindNameIC &cache = ion->getCache(cacheIndex).toBindName();
    HandlePropertyName name = cache.name();

    RootedObject holder(cx);
    if (scopeChain->is<GlobalObject>()) {
        holder = scopeChain;
    } else {
        if (!LookupNameWithGlobalDefault(cx, name, scopeChain, &holder))
            return nullptr;
    }

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.canAttachStub()) {
        if (scopeChain->is<GlobalObject>()) {
            if (!cache.attachGlobal(cx, ion, scopeChain))
                return nullptr;
        } else if (IsCacheableScopeChain(scopeChain, holder)) {
            if (!cache.attachNonGlobal(cx, ion, scopeChain, holder))
                return nullptr;
        } else {
            IonSpew(IonSpew_InlineCaches, "BINDNAME uncacheable scope chain");
        }
    }

    return holder;
}

bool
NameIC::attachReadSlot(JSContext *cx, IonScript *ion, HandleObject scopeChain,
                       HandleObject holderBase, HandleObject holder,
                       HandleShape shape)
{
    MacroAssembler masm(cx, ion);
    Label failures;
    RepatchStubAppender attacher(*this);

    Register scratchReg = outputReg().valueReg().scratchReg();

    // Don't guard the base of the proto chain the name was found on. It will be guarded
    // by GenerateReadSlot().
    masm.mov(scopeChainReg(), scratchReg);
    GenerateScopeChainGuards(masm, scopeChain, holderBase, scratchReg, &failures,
                             /* skipLastGuard = */true);

    // GenerateScopeChain leaves the last scope chain in scrachReg, even though it
    // doesn't generate the extra guard.
    GenerateReadSlot(cx, ion, masm, attacher, holderBase, holder, shape, scratchReg,
                     outputReg(), failures.used() ? &failures : nullptr);

    return linkAndAttachStub(cx, masm, attacher, ion, "generic");
}

static bool
IsCacheableNameReadSlot(JSContext *cx, HandleObject scopeChain, HandleObject obj,
                        HandleObject holder, HandleShape shape, jsbytecode *pc,
                        const TypedOrValueRegister &output)
{
    if (!shape)
        return false;
    if (!obj->isNative())
        return false;

    if (obj->is<GlobalObject>()) {
        // Support only simple property lookups.
        if (!IsCacheableGetPropReadSlot(obj, holder, shape) &&
            !IsCacheableNoProperty(obj, holder, shape, pc, output))
            return false;
    } else if (obj->is<CallObject>()) {
        JS_ASSERT(obj == holder);
        if (!shape->hasDefaultGetter())
            return false;
    } else {
        // We don't yet support lookups on Block or DeclEnv objects.
        return false;
    }

    RootedObject obj2(cx, scopeChain);
    while (obj2) {
        if (!IsCacheableNonGlobalScope(obj2) && !obj2->is<GlobalObject>())
            return false;

        // Stop once we hit the global or target obj.
        if (obj2->is<GlobalObject>() || obj2 == obj)
            break;

        obj2 = obj2->enclosingScope();
    }

    return obj == obj2;
}

bool
NameIC::attachCallGetter(JSContext *cx, IonScript *ion, JSObject *obj, JSObject *holder,
                         HandleShape shape, void *returnAddr)
{
    MacroAssembler masm(cx, ion);

    RepatchStubAppender attacher(*this);
    if (!GenerateCallGetter(cx, ion, masm, attacher, obj, name(), holder, shape, liveRegs_,
                            scopeChainReg(), outputReg(), returnAddr))
    {
         return false;
    }

    const char *attachKind = "name getter";
    return linkAndAttachStub(cx, masm, attacher, ion, attachKind);
}

static bool
IsCacheableNameCallGetter(JSObject *scopeChain, JSObject *obj, JSObject *holder, Shape *shape)
{
    if (obj != scopeChain)
        return false;

    if (!obj->is<GlobalObject>())
        return false;

    return IsCacheableGetPropCallNative(obj, holder, shape) ||
        IsCacheableGetPropCallPropertyOp(obj, holder, shape);
}

bool
NameIC::update(JSContext *cx, size_t cacheIndex, HandleObject scopeChain,
               MutableHandleValue vp)
{
    AutoFlushCache afc ("GetNameCache", cx->runtime()->jitRuntime());

    void *returnAddr;
    IonScript *ion = GetTopIonJSScript(cx, &returnAddr)->ionScript();

    NameIC &cache = ion->getCache(cacheIndex).toName();
    RootedPropertyName name(cx, cache.name());

    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    RootedObject obj(cx);
    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!LookupName(cx, name, scopeChain, &obj, &holder, &shape))
        return false;

    if (cache.canAttachStub()) {
        if (IsCacheableNameReadSlot(cx, scopeChain, obj, holder, shape, pc, cache.outputReg())) {
            if (!cache.attachReadSlot(cx, ion, scopeChain, obj, holder, shape))
                return false;
        } else if (IsCacheableNameCallGetter(scopeChain, obj, holder, shape)) {
            if (!cache.attachCallGetter(cx, ion, obj, holder, shape, returnAddr))
                return false;
        }
    }

    if (cache.isTypeOf()) {
        if (!FetchName<true>(cx, obj, holder, name, shape, vp))
            return false;
    } else {
        if (!FetchName<false>(cx, obj, holder, name, shape, vp))
            return false;
    }

    // Monitor changes to cache entry.
    types::TypeScript::Monitor(cx, script, pc, vp);

    return true;
}

bool
CallsiteCloneIC::attach(JSContext *cx, IonScript *ion, HandleFunction original,
                        HandleFunction clone)
{
    MacroAssembler masm(cx, ion);
    RepatchStubAppender attacher(*this);

    // Guard against object identity on the original.
    attacher.branchNextStub(masm, Assembler::NotEqual, calleeReg(), ImmGCPtr(original));

    // Load the clone.
    masm.movePtr(ImmGCPtr(clone), outputReg());

    attacher.jumpRejoin(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "generic");
}

JSObject *
CallsiteCloneIC::update(JSContext *cx, size_t cacheIndex, HandleObject callee)
{
    AutoFlushCache afc ("CallsiteCloneCache", cx->runtime()->jitRuntime());

    // Act as the identity for functions that are not clone-at-callsite, as we
    // generate this cache as long as some callees are clone-at-callsite.
    RootedFunction fun(cx, &callee->as<JSFunction>());
    if (!fun->hasScript() || !fun->nonLazyScript()->shouldCloneAtCallsite())
        return fun;

    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    CallsiteCloneIC &cache = ion->getCache(cacheIndex).toCallsiteClone();

    RootedFunction clone(cx, CloneFunctionAtCallsite(cx, fun, cache.callScript(), cache.callPc()));
    if (!clone)
        return nullptr;

    if (cache.canAttachStub()) {
        if (!cache.attach(cx, ion, fun, clone))
            return nullptr;
    }

    return clone;
}
