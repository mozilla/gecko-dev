/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArgumentsObject-inl.h"

#include "mozilla/PodOperations.h"

#include "jit/JitFrames.h"
#include "vm/GlobalObject.h"
#include "vm/Stack.h"

#include "jsobjinlines.h"

#include "gc/Nursery-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;

static void
CopyStackFrameArguments(const AbstractFramePtr frame, HeapValue* dst, unsigned totalArgs)
{
    MOZ_ASSERT_IF(frame.isInterpreterFrame(), !frame.asInterpreterFrame()->runningInJit());

    MOZ_ASSERT(Max(frame.numActualArgs(), frame.numFormalArgs()) == totalArgs);

    /* Copy arguments. */
    Value* src = frame.argv();
    Value* end = src + totalArgs;
    while (src != end)
        (dst++)->init(*src++);
}

/* static */ void
ArgumentsObject::MaybeForwardToCallObject(AbstractFramePtr frame, ArgumentsObject* obj,
                                          ArgumentsData* data)
{
    JSScript* script = frame.script();
    if (frame.fun()->isHeavyweight() && script->argsObjAliasesFormals()) {
        obj->initFixedSlot(MAYBE_CALL_SLOT, ObjectValue(frame.callObj()));
        for (AliasedFormalIter fi(script); fi; fi++)
            data->args[fi.frameIndex()] = MagicScopeSlotValue(fi.scopeSlot());
    }
}

/* static */ void
ArgumentsObject::MaybeForwardToCallObject(jit::JitFrameLayout* frame, HandleObject callObj,
                                          ArgumentsObject* obj, ArgumentsData* data)
{
    JSFunction* callee = jit::CalleeTokenToFunction(frame->calleeToken());
    JSScript* script = callee->nonLazyScript();
    if (callee->isHeavyweight() && script->argsObjAliasesFormals()) {
        MOZ_ASSERT(callObj && callObj->is<CallObject>());
        obj->initFixedSlot(MAYBE_CALL_SLOT, ObjectValue(*callObj.get()));
        for (AliasedFormalIter fi(script); fi; fi++)
            data->args[fi.frameIndex()] = MagicScopeSlotValue(fi.scopeSlot());
    }
}

struct CopyFrameArgs
{
    AbstractFramePtr frame_;

    explicit CopyFrameArgs(AbstractFramePtr frame)
      : frame_(frame)
    { }

    void copyArgs(JSContext*, HeapValue* dst, unsigned totalArgs) const {
        CopyStackFrameArguments(frame_, dst, totalArgs);
    }

    /*
     * If a call object exists and the arguments object aliases formals, the
     * call object is the canonical location for formals.
     */
    void maybeForwardToCallObject(ArgumentsObject* obj, ArgumentsData* data) {
        ArgumentsObject::MaybeForwardToCallObject(frame_, obj, data);
    }
};

struct CopyJitFrameArgs
{
    jit::JitFrameLayout* frame_;
    HandleObject callObj_;

    CopyJitFrameArgs(jit::JitFrameLayout* frame, HandleObject callObj)
      : frame_(frame), callObj_(callObj)
    { }

    void copyArgs(JSContext*, HeapValue* dstBase, unsigned totalArgs) const {
        unsigned numActuals = frame_->numActualArgs();
        unsigned numFormals = jit::CalleeTokenToFunction(frame_->calleeToken())->nargs();
        MOZ_ASSERT(numActuals <= totalArgs);
        MOZ_ASSERT(numFormals <= totalArgs);
        MOZ_ASSERT(Max(numActuals, numFormals) == totalArgs);

        /* Copy all arguments. */
        Value* src = frame_->argv() + 1;  /* +1 to skip this. */
        Value* end = src + numActuals;
        HeapValue* dst = dstBase;
        while (src != end)
            (dst++)->init(*src++);

        if (numActuals < numFormals) {
            HeapValue* dstEnd = dstBase + totalArgs;
            while (dst != dstEnd)
                (dst++)->init(UndefinedValue());
        }
    }

    /*
     * If a call object exists and the arguments object aliases formals, the
     * call object is the canonical location for formals.
     */
    void maybeForwardToCallObject(ArgumentsObject* obj, ArgumentsData* data) {
        ArgumentsObject::MaybeForwardToCallObject(frame_, callObj_, obj, data);
    }
};

struct CopyScriptFrameIterArgs
{
    ScriptFrameIter& iter_;

    explicit CopyScriptFrameIterArgs(ScriptFrameIter& iter)
      : iter_(iter)
    { }

    void copyArgs(JSContext* cx, HeapValue* dstBase, unsigned totalArgs) const {
        /* Copy actual arguments. */
        iter_.unaliasedForEachActual(cx, CopyToHeap(dstBase));

        /* Define formals which are not part of the actuals. */
        unsigned numActuals = iter_.numActualArgs();
        unsigned numFormals = iter_.calleeTemplate()->nargs();
        MOZ_ASSERT(numActuals <= totalArgs);
        MOZ_ASSERT(numFormals <= totalArgs);
        MOZ_ASSERT(Max(numActuals, numFormals) == totalArgs);

        if (numActuals < numFormals) {
            HeapValue* dst = dstBase + numActuals;
            HeapValue* dstEnd = dstBase + totalArgs;
            while (dst != dstEnd)
                (dst++)->init(UndefinedValue());
        }
    }

    /*
     * Ion frames are copying every argument onto the stack, other locations are
     * invalid.
     */
    void maybeForwardToCallObject(ArgumentsObject* obj, ArgumentsData* data) {
        if (!iter_.isIon())
            ArgumentsObject::MaybeForwardToCallObject(iter_.abstractFramePtr(), obj, data);
    }
};

template <typename CopyArgs>
/* static */ ArgumentsObject*
ArgumentsObject::create(JSContext* cx, HandleScript script, HandleFunction callee,
                        unsigned numActuals, CopyArgs& copy)
{
    RootedObject proto(cx, callee->global().getOrCreateObjectPrototype(cx));
    if (!proto)
        return nullptr;

    bool strict = callee->strict();
    const Class* clasp = strict ? &StrictArgumentsObject::class_ : &NormalArgumentsObject::class_;

    RootedObjectGroup group(cx, ObjectGroup::defaultNewGroup(cx, clasp, TaggedProto(proto.get())));
    if (!group)
        return nullptr;

    RootedShape shape(cx, EmptyShape::getInitialShape(cx, clasp, TaggedProto(proto),
                                                      FINALIZE_KIND, BaseShape::INDEXED));
    if (!shape)
        return nullptr;

    unsigned numFormals = callee->nargs();
    unsigned numDeletedWords = NumWordsForBitArrayOfLength(numActuals);
    unsigned numArgs = Max(numActuals, numFormals);
    unsigned numBytes = offsetof(ArgumentsData, args) +
                        numDeletedWords * sizeof(size_t) +
                        numArgs * sizeof(Value);

    Rooted<ArgumentsObject*> obj(cx);
    JSObject* base = JSObject::create(cx, FINALIZE_KIND,
                                      GetInitialHeap(GenericObject, clasp),
                                      shape, group);
    if (!base)
        return nullptr;
    obj = &base->as<ArgumentsObject>();

    ArgumentsData* data =
        reinterpret_cast<ArgumentsData*>(AllocateObjectBuffer<uint8_t>(cx, obj, numBytes));
    if (!data) {
        // Make the object safe for GC.
        obj->initFixedSlot(DATA_SLOT, PrivateValue(nullptr));
        return nullptr;
    }

    data->numArgs = numArgs;
    data->dataBytes = numBytes;
    data->callee.init(ObjectValue(*callee.get()));
    data->script = script;

    // Zero the argument Values. This sets each value to DoubleValue(0), which
    // is safe for GC tracing.
    memset(data->args, 0, numArgs * sizeof(Value));
    MOZ_ASSERT(DoubleValue(0).asRawBits() == 0x0);
    MOZ_ASSERT_IF(numArgs > 0, data->args[0].asRawBits() == 0x0);

    obj->initFixedSlot(DATA_SLOT, PrivateValue(data));

    /* Copy [0, numArgs) into data->slots. */
    copy.copyArgs(cx, data->args, numArgs);

    data->deletedBits = reinterpret_cast<size_t*>(data->args + numArgs);
    ClearAllBitArrayElements(data->deletedBits, numDeletedWords);

    obj->initFixedSlot(INITIAL_LENGTH_SLOT, Int32Value(numActuals << PACKED_BITS_COUNT));

    copy.maybeForwardToCallObject(obj, data);

    MOZ_ASSERT(obj->initialLength() == numActuals);
    MOZ_ASSERT(!obj->hasOverriddenLength());
    return obj;
}

ArgumentsObject*
ArgumentsObject::createExpected(JSContext* cx, AbstractFramePtr frame)
{
    MOZ_ASSERT(frame.script()->needsArgsObj());
    RootedScript script(cx, frame.script());
    RootedFunction callee(cx, frame.callee());
    CopyFrameArgs copy(frame);
    ArgumentsObject* argsobj = create(cx, script, callee, frame.numActualArgs(), copy);
    if (!argsobj)
        return nullptr;

    frame.initArgsObj(*argsobj);
    return argsobj;
}

ArgumentsObject*
ArgumentsObject::createUnexpected(JSContext* cx, ScriptFrameIter& iter)
{
    RootedScript script(cx, iter.script());
    RootedFunction callee(cx, iter.callee(cx));
    CopyScriptFrameIterArgs copy(iter);
    return create(cx, script, callee, iter.numActualArgs(), copy);
}

ArgumentsObject*
ArgumentsObject::createUnexpected(JSContext* cx, AbstractFramePtr frame)
{
    RootedScript script(cx, frame.script());
    RootedFunction callee(cx, frame.callee());
    CopyFrameArgs copy(frame);
    return create(cx, script, callee, frame.numActualArgs(), copy);
}

ArgumentsObject*
ArgumentsObject::createForIon(JSContext* cx, jit::JitFrameLayout* frame, HandleObject scopeChain)
{
    jit::CalleeToken token = frame->calleeToken();
    MOZ_ASSERT(jit::CalleeTokenIsFunction(token));
    RootedScript script(cx, jit::ScriptFromCalleeToken(token));
    RootedFunction callee(cx, jit::CalleeTokenToFunction(token));
    RootedObject callObj(cx, scopeChain->is<CallObject>() ? scopeChain.get() : nullptr);
    CopyJitFrameArgs copy(frame, callObj);
    return create(cx, script, callee, frame->numActualArgs(), copy);
}

static bool
args_delProperty(JSContext* cx, HandleObject obj, HandleId id, ObjectOpResult& result)
{
    ArgumentsObject& argsobj = obj->as<ArgumentsObject>();
    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            argsobj.markElementDeleted(arg);
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        argsobj.markLengthOverridden();
    } else if (JSID_IS_ATOM(id, cx->names().callee)) {
        argsobj.as<NormalArgumentsObject>().clearCallee();
    }
    return result.succeed();
}

static bool
ArgGetter(JSContext* cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    NormalArgumentsObject& argsobj = obj->as<NormalArgumentsObject>();
    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            vp.set(argsobj.element(arg));
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        if (!argsobj.hasOverriddenLength())
            vp.setInt32(argsobj.initialLength());
    } else {
        MOZ_ASSERT(JSID_IS_ATOM(id, cx->names().callee));
        if (!argsobj.callee().isMagic(JS_OVERWRITTEN_CALLEE))
            vp.set(argsobj.callee());
    }
    return true;
}

static bool
ArgSetter(JSContext* cx, HandleObject obj, HandleId id, MutableHandleValue vp,
          ObjectOpResult& result)
{
    if (!obj->is<NormalArgumentsObject>())
        return result.succeed();
    Handle<NormalArgumentsObject*> argsobj = obj.as<NormalArgumentsObject>();

    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, argsobj, id, &desc))
        return false;
    MOZ_ASSERT(desc.object());
    unsigned attrs = desc.attributes();
    MOZ_ASSERT(!(attrs & JSPROP_READONLY));
    attrs &= (JSPROP_ENUMERATE | JSPROP_PERMANENT); /* only valid attributes */

    RootedScript script(cx, argsobj->containingScript());

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj->initialLength() && !argsobj->isElementDeleted(arg)) {
            argsobj->setElement(cx, arg, vp);
            if (arg < script->functionNonDelazifying()->nargs())
                TypeScript::SetArgument(cx, script, arg, vp);
            return result.succeed();
        }
    } else {
        MOZ_ASSERT(JSID_IS_ATOM(id, cx->names().length) || JSID_IS_ATOM(id, cx->names().callee));
    }

    /*
     * For simplicity we use delete/define to replace the property with a
     * simple data property. Note that we rely on args_delProperty to clear the
     * corresponding reserved slot so the GC can collect its value. Note also
     * that we must define the property instead of setting it in case the user
     * has changed the prototype to an object that has a setter for this id.
     */
    ObjectOpResult ignored;
    return NativeDeleteProperty(cx, argsobj, id, ignored) &&
           NativeDefineProperty(cx, argsobj, id, vp, nullptr, nullptr, attrs, result);
}

static bool
args_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    Rooted<NormalArgumentsObject*> argsobj(cx, &obj->as<NormalArgumentsObject>());

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE | JSPROP_RESOLVING;
    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        if (argsobj->hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->names().callee))
            return true;

        if (argsobj->callee().isMagic(JS_OVERWRITTEN_CALLEE))
            return true;
    }

    if (!NativeDefineProperty(cx, argsobj, id, UndefinedHandleValue, ArgGetter, ArgSetter, attrs))
        return false;

    *resolvedp = true;
    return true;
}

static bool
args_enumerate(JSContext* cx, HandleObject obj)
{
    Rooted<NormalArgumentsObject*> argsobj(cx, &obj->as<NormalArgumentsObject>());

    RootedId id(cx);
    bool found;

    // Trigger reflection.
    id = NameToId(cx->names().length);
    if (!HasProperty(cx, argsobj, id, &found))
        return false;

    id = NameToId(cx->names().callee);
    if (!HasProperty(cx, argsobj, id, &found))
        return false;

    for (unsigned i = 0; i < argsobj->initialLength(); i++) {
        id = INT_TO_JSID(i);
        if (!HasProperty(cx, argsobj, id, &found))
            return false;
    }

    return true;
}

static bool
StrictArgGetter(JSContext* cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    StrictArgumentsObject& argsobj = obj->as<StrictArgumentsObject>();

    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj.initialLength() && !argsobj.isElementDeleted(arg))
            vp.set(argsobj.element(arg));
    } else {
        MOZ_ASSERT(JSID_IS_ATOM(id, cx->names().length));
        if (!argsobj.hasOverriddenLength())
            vp.setInt32(argsobj.initialLength());
    }
    return true;
}

static bool
StrictArgSetter(JSContext* cx, HandleObject obj, HandleId id, MutableHandleValue vp,
                ObjectOpResult& result)
{
    if (!obj->is<StrictArgumentsObject>())
        return result.succeed();
    Handle<StrictArgumentsObject*> argsobj = obj.as<StrictArgumentsObject>();

    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, argsobj, id, &desc))
        return false;
    MOZ_ASSERT(desc.object());
    unsigned attrs = desc.attributes();
    MOZ_ASSERT(!(attrs & JSPROP_READONLY));
    attrs &= (JSPROP_ENUMERATE | JSPROP_PERMANENT); /* only valid attributes */

    if (JSID_IS_INT(id)) {
        unsigned arg = unsigned(JSID_TO_INT(id));
        if (arg < argsobj->initialLength()) {
            argsobj->setElement(cx, arg, vp);
            return result.succeed();
        }
    } else {
        MOZ_ASSERT(JSID_IS_ATOM(id, cx->names().length));
    }

    /*
     * For simplicity we use delete/define to replace the property with a
     * simple data property. Note that we rely on args_delProperty to clear the
     * corresponding reserved slot so the GC can collect its value.
     */
    ObjectOpResult ignored;
    return NativeDeleteProperty(cx, argsobj, id, ignored) &&
           NativeDefineProperty(cx, argsobj, id, vp, nullptr, nullptr, attrs, result);
}

static bool
strictargs_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->as<StrictArgumentsObject>());

    unsigned attrs = JSPROP_SHARED | JSPROP_SHADOWABLE;
    GetterOp getter = StrictArgGetter;
    SetterOp setter = StrictArgSetter;

    if (JSID_IS_INT(id)) {
        uint32_t arg = uint32_t(JSID_TO_INT(id));
        if (arg >= argsobj->initialLength() || argsobj->isElementDeleted(arg))
            return true;

        attrs |= JSPROP_ENUMERATE;
    } else if (JSID_IS_ATOM(id, cx->names().length)) {
        if (argsobj->hasOverriddenLength())
            return true;
    } else {
        if (!JSID_IS_ATOM(id, cx->names().callee) && !JSID_IS_ATOM(id, cx->names().caller))
            return true;

        attrs = JSPROP_PERMANENT | JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED;
        getter = CastAsGetterOp(argsobj->global().getThrowTypeError());
        setter = CastAsSetterOp(argsobj->global().getThrowTypeError());
    }

    attrs |= JSPROP_RESOLVING;
    if (!NativeDefineProperty(cx, argsobj, id, UndefinedHandleValue, getter, setter, attrs))
        return false;

    *resolvedp = true;
    return true;
}

static bool
strictargs_enumerate(JSContext* cx, HandleObject obj)
{
    Rooted<StrictArgumentsObject*> argsobj(cx, &obj->as<StrictArgumentsObject>());

    RootedId id(cx);
    bool found;

    // Trigger reflection.
    id = NameToId(cx->names().length);
    if (!HasProperty(cx, argsobj, id, &found))
        return false;

    id = NameToId(cx->names().callee);
    if (!HasProperty(cx, argsobj, id, &found))
        return false;

    id = NameToId(cx->names().caller);
    if (!HasProperty(cx, argsobj, id, &found))
        return false;

    for (unsigned i = 0; i < argsobj->initialLength(); i++) {
        id = INT_TO_JSID(i);
        if (!HasProperty(cx, argsobj, id, &found))
            return false;
    }

    return true;
}

void
ArgumentsObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(!IsInsideNursery(obj));
    fop->free_(reinterpret_cast<void*>(obj->as<ArgumentsObject>().data()));
}

void
ArgumentsObject::trace(JSTracer* trc, JSObject* obj)
{
    ArgumentsObject& argsobj = obj->as<ArgumentsObject>();
    ArgumentsData* data = argsobj.data();
    TraceEdge(trc, &data->callee, js_callee_str);
    TraceRange(trc, data->numArgs, data->begin(), js_arguments_str);
    TraceManuallyBarrieredEdge(trc, &data->script, "script");
}

/* static */ size_t
ArgumentsObject::objectMovedDuringMinorGC(JSTracer* trc, JSObject* dst, JSObject* src)
{
    ArgumentsObject* ndst = &dst->as<ArgumentsObject>();
    ArgumentsObject* nsrc = &src->as<ArgumentsObject>();
    MOZ_ASSERT(ndst->data() == nsrc->data());

    Nursery& nursery = trc->runtime()->gc.nursery;

    if (!nursery.isInside(nsrc->data())) {
        nursery.removeMallocedBuffer(nsrc->data());
        return 0;
    }

    uint32_t nbytes = nsrc->data()->dataBytes;
    uint8_t* data = nsrc->zone()->pod_malloc<uint8_t>(nbytes);
    if (!data)
        CrashAtUnhandlableOOM("Failed to allocate ArgumentsObject data while tenuring.");
    ndst->initFixedSlot(DATA_SLOT, PrivateValue(data));

    mozilla::PodCopy(data, reinterpret_cast<uint8_t*>(nsrc->data()), nbytes);

    ArgumentsData* dstData = ndst->data();
    dstData->deletedBits = reinterpret_cast<size_t*>(dstData->args + dstData->numArgs);

    return nbytes;
}

/*
 * The classes below collaborate to lazily reflect and synchronize actual
 * argument values, argument count, and callee function object stored in a
 * stack frame with their corresponding property values in the frame's
 * arguments object.
 */
const Class NormalArgumentsObject::class_ = {
    "Arguments",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(NormalArgumentsObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object) |
    JSCLASS_SKIP_NURSERY_FINALIZE |
    JSCLASS_BACKGROUND_FINALIZE,
    nullptr,                 /* addProperty */
    args_delProperty,
    nullptr,                 /* getProperty */
    nullptr,                 /* setProperty */
    args_enumerate,
    args_resolve,
    nullptr,                 /* mayResolve  */
    nullptr,                 /* convert     */
    ArgumentsObject::finalize,
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    ArgumentsObject::trace
};

/*
 * Strict mode arguments is significantly less magical than non-strict mode
 * arguments, so it is represented by a different class while sharing some
 * functionality.
 */
const Class StrictArgumentsObject::class_ = {
    "Arguments",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(StrictArgumentsObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object) |
    JSCLASS_SKIP_NURSERY_FINALIZE |
    JSCLASS_BACKGROUND_FINALIZE,
    nullptr,                 /* addProperty */
    args_delProperty,
    nullptr,                 /* getProperty */
    nullptr,                 /* setProperty */
    strictargs_enumerate,
    strictargs_resolve,
    nullptr,                 /* mayResolve  */
    nullptr,                 /* convert     */
    ArgumentsObject::finalize,
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    ArgumentsObject::trace
};
