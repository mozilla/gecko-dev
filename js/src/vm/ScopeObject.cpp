/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ScopeObject-inl.h"

#include "mozilla/PodOperations.h"

#include "jscompartment.h"
#include "jsiter.h"

#include "vm/ArgumentsObject.h"
#include "vm/GlobalObject.h"
#include "vm/ProxyObject.h"
#include "vm/Shape.h"
#include "vm/Xdr.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace js::types;

using mozilla::PodZero;

typedef Rooted<ArgumentsObject *> RootedArgumentsObject;

/*****************************************************************************/

static JSObject *
InnermostStaticScope(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(script->containsPC(pc));
    JS_ASSERT(JOF_OPTYPE(*pc) == JOF_SCOPECOORD);

    NestedScopeObject *scope = script->getStaticScope(pc);
    if (scope)
        return scope;
    return script->functionNonDelazifying();
}

Shape *
js::ScopeCoordinateToStaticScopeShape(JSScript *script, jsbytecode *pc)
{
    StaticScopeIter<NoGC> ssi(InnermostStaticScope(script, pc));
    uint32_t hops = ScopeCoordinate(pc).hops();
    while (true) {
        JS_ASSERT(!ssi.done());
        if (ssi.hasDynamicScopeObject()) {
            if (!hops)
                break;
            hops--;
        }
        ssi++;
    }
    return ssi.scopeShape();
}

static const uint32_t SCOPE_COORDINATE_NAME_THRESHOLD = 20;

void
ScopeCoordinateNameCache::purge()
{
    shape = nullptr;
    if (map.initialized())
        map.finish();
}

PropertyName *
js::ScopeCoordinateName(ScopeCoordinateNameCache &cache, JSScript *script, jsbytecode *pc)
{
    Shape *shape = ScopeCoordinateToStaticScopeShape(script, pc);
    if (shape != cache.shape && shape->slot() >= SCOPE_COORDINATE_NAME_THRESHOLD) {
        cache.purge();
        if (cache.map.init(shape->slot())) {
            cache.shape = shape;
            Shape::Range<NoGC> r(shape);
            while (!r.empty()) {
                if (!cache.map.putNew(r.front().slot(), r.front().propid())) {
                    cache.purge();
                    break;
                }
                r.popFront();
            }
        }
    }

    jsid id;
    ScopeCoordinate sc(pc);
    if (shape == cache.shape) {
        ScopeCoordinateNameCache::Map::Ptr p = cache.map.lookup(sc.slot());
        id = p->value();
    } else {
        Shape::Range<NoGC> r(shape);
        while (r.front().slot() != sc.slot())
            r.popFront();
        id = r.front().propidRaw();
    }

    /* Beware nameless destructuring formal. */
    if (!JSID_IS_ATOM(id))
        return script->runtimeFromAnyThread()->atomState.empty;
    return JSID_TO_ATOM(id)->asPropertyName();
}

JSScript *
js::ScopeCoordinateFunctionScript(JSScript *script, jsbytecode *pc)
{
    StaticScopeIter<NoGC> ssi(InnermostStaticScope(script, pc));
    uint32_t hops = ScopeCoordinate(pc).hops();
    while (true) {
        if (ssi.hasDynamicScopeObject()) {
            if (!hops)
                break;
            hops--;
        }
        ssi++;
    }
    if (ssi.type() != StaticScopeIter<NoGC>::FUNCTION)
        return nullptr;
    return ssi.funScript();
}

/*****************************************************************************/

void
ScopeObject::setEnclosingScope(HandleObject obj)
{
    JS_ASSERT_IF(obj->is<CallObject>() || obj->is<DeclEnvObject>() || obj->is<BlockObject>(),
                 obj->isDelegate());
    setFixedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*obj));
}

/*
 * Construct a bare-bones call object given a shape, type, and slots pointer.
 * The call object must be further initialized to be usable.
 */
CallObject *
CallObject::create(JSContext *cx, HandleScript script, HandleShape shape, HandleTypeObject type, HeapSlot *slots)
{
    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    JS_ASSERT(CanBeFinalizedInBackground(kind, &CallObject::class_));
    kind = gc::GetBackgroundAllocKind(kind);

    gc::InitialHeap heap = script->treatAsRunOnce() ? gc::TenuredHeap : gc::DefaultHeap;
    JSObject *obj = JSObject::create(cx, kind, heap, shape, type, slots);
    if (!obj)
        return nullptr;

    if (script->treatAsRunOnce()) {
        RootedObject nobj(cx, obj);
        if (!JSObject::setSingletonType(cx, nobj))
            return nullptr;
        return &nobj->as<CallObject>();
    }

    return &obj->as<CallObject>();
}

/*
 * Create a CallObject for a JSScript that is not initialized to any particular
 * callsite. This object can either be initialized (with an enclosing scope and
 * callee) or used as a template for jit compilation.
 */
CallObject *
CallObject::createTemplateObject(JSContext *cx, HandleScript script, gc::InitialHeap heap)
{
    RootedShape shape(cx, script->bindings.callObjShape());
    JS_ASSERT(shape->getObjectClass() == &class_);

    RootedTypeObject type(cx, cx->getNewType(&class_, nullptr));
    if (!type)
        return nullptr;

    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    JS_ASSERT(CanBeFinalizedInBackground(kind, &class_));
    kind = gc::GetBackgroundAllocKind(kind);

    JSObject *obj = JSObject::create(cx, kind, heap, shape, type);
    if (!obj)
        return nullptr;

    return &obj->as<CallObject>();
}

/*
 * Construct a call object for the given bindings.  If this is a call object
 * for a function invocation, callee should be the function being called.
 * Otherwise it must be a call object for eval of strict mode code, and callee
 * must be null.
 */
CallObject *
CallObject::create(JSContext *cx, HandleScript script, HandleObject enclosing, HandleFunction callee)
{
    gc::InitialHeap heap = script->treatAsRunOnce() ? gc::TenuredHeap : gc::DefaultHeap;
    CallObject *callobj = CallObject::createTemplateObject(cx, script, heap);
    if (!callobj)
        return nullptr;

    callobj->as<ScopeObject>().setEnclosingScope(enclosing);
    callobj->initFixedSlot(CALLEE_SLOT, ObjectOrNullValue(callee));

    if (script->treatAsRunOnce()) {
        Rooted<CallObject*> ncallobj(cx, callobj);
        if (!JSObject::setSingletonType(cx, ncallobj))
            return nullptr;
        return ncallobj;
    }

    return callobj;
}

CallObject *
CallObject::createForFunction(JSContext *cx, HandleObject enclosing, HandleFunction callee)
{
    RootedObject scopeChain(cx, enclosing);
    JS_ASSERT(scopeChain);

    /*
     * For a named function expression Call's parent points to an environment
     * object holding function's name.
     */
    if (callee->isNamedLambda()) {
        scopeChain = DeclEnvObject::create(cx, scopeChain, callee);
        if (!scopeChain)
            return nullptr;
    }

    RootedScript script(cx, callee->nonLazyScript());
    return create(cx, script, scopeChain, callee);
}

CallObject *
CallObject::createForFunction(JSContext *cx, AbstractFramePtr frame)
{
    JS_ASSERT(frame.isNonEvalFunctionFrame());
    assertSameCompartment(cx, frame);

    RootedObject scopeChain(cx, frame.scopeChain());
    RootedFunction callee(cx, frame.callee());

    CallObject *callobj = createForFunction(cx, scopeChain, callee);
    if (!callobj)
        return nullptr;

    /* Copy in the closed-over formal arguments. */
    for (AliasedFormalIter i(frame.script()); i; i++) {
        callobj->setAliasedVar(cx, i, i->name(),
                               frame.unaliasedFormal(i.frameIndex(), DONT_CHECK_ALIASING));
    }

    return callobj;
}

CallObject *
CallObject::createForStrictEval(JSContext *cx, AbstractFramePtr frame)
{
    JS_ASSERT(frame.isStrictEvalFrame());
    JS_ASSERT_IF(frame.isStackFrame(), cx->interpreterFrame() == frame.asStackFrame());
    JS_ASSERT_IF(frame.isStackFrame(), cx->interpreterRegs().pc == frame.script()->code());

    RootedFunction callee(cx);
    RootedScript script(cx, frame.script());
    RootedObject scopeChain(cx, frame.scopeChain());
    return create(cx, script, scopeChain, callee);
}

const Class CallObject::class_ = {
    "Call",
    JSCLASS_IS_ANONYMOUS | JSCLASS_HAS_RESERVED_SLOTS(CallObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    nullptr                  /* convert: Leave it nullptr so we notice if calls ever escape */
};

const Class DeclEnvObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(DeclEnvObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

/*
 * Create a DeclEnvObject for a JSScript that is not initialized to any
 * particular callsite. This object can either be initialized (with an enclosing
 * scope and callee) or used as a template for jit compilation.
 */
DeclEnvObject *
DeclEnvObject::createTemplateObject(JSContext *cx, HandleFunction fun, gc::InitialHeap heap)
{
    JS_ASSERT(IsNurseryAllocable(FINALIZE_KIND));

    RootedTypeObject type(cx, cx->getNewType(&class_, nullptr));
    if (!type)
        return nullptr;

    RootedShape emptyDeclEnvShape(cx);
    emptyDeclEnvShape = EmptyShape::getInitialShape(cx, &class_, nullptr,
                                                    cx->global(), nullptr, FINALIZE_KIND,
                                                    BaseShape::DELEGATE);
    if (!emptyDeclEnvShape)
        return nullptr;

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, heap, emptyDeclEnvShape, type));
    if (!obj)
        return nullptr;

    // Assign a fixed slot to a property with the same name as the lambda.
    Rooted<jsid> id(cx, AtomToId(fun->atom()));
    const Class *clasp = obj->getClass();
    unsigned attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY;
    if (!JSObject::putProperty<SequentialExecution>(cx, obj, id, clasp->getProperty,
                                                    clasp->setProperty, lambdaSlot(), attrs, 0, 0))
    {
        return nullptr;
    }

    JS_ASSERT(!obj->hasDynamicSlots());
    return &obj->as<DeclEnvObject>();
}

DeclEnvObject *
DeclEnvObject::create(JSContext *cx, HandleObject enclosing, HandleFunction callee)
{
    RootedObject obj(cx, createTemplateObject(cx, callee, gc::DefaultHeap));
    if (!obj)
        return nullptr;

    obj->as<ScopeObject>().setEnclosingScope(enclosing);
    obj->setFixedSlot(lambdaSlot(), ObjectValue(*callee));
    return &obj->as<DeclEnvObject>();
}

WithObject *
WithObject::create(JSContext *cx, HandleObject proto, HandleObject enclosing, uint32_t depth)
{
    RootedTypeObject type(cx, cx->getNewType(&class_, proto.get()));
    if (!type)
        return nullptr;

    RootedShape shape(cx, EmptyShape::getInitialShape(cx, &class_, TaggedProto(proto),
                                                      &enclosing->global(), nullptr,
                                                      FINALIZE_KIND));
    if (!shape)
        return nullptr;

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, gc::DefaultHeap, shape, type));
    if (!obj)
        return nullptr;

    obj->as<ScopeObject>().setEnclosingScope(enclosing);
    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));

    JSObject *thisp = JSObject::thisObject(cx, proto);
    if (!thisp)
        return nullptr;

    obj->setFixedSlot(THIS_SLOT, ObjectValue(*thisp));

    return &obj->as<WithObject>();
}

static bool
with_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::lookupGeneric(cx, actual, id, objp, propp);
}

static bool
with_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static bool
with_LookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static bool
with_LookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static bool
with_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                MutableHandleValue vp)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::getGeneric(cx, actual, actual, id, vp);
}

static bool
with_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                 MutableHandleValue vp)
{
    RootedId id(cx, NameToId(name));
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static bool
with_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                MutableHandleValue vp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static bool
with_GetSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid,
                MutableHandleValue vp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static bool
with_SetGeneric(JSContext *cx, HandleObject obj, HandleId id,
                MutableHandleValue vp, bool strict)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::setGeneric(cx, actual, actual, id, vp, strict);
}

static bool
with_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                 MutableHandleValue vp, bool strict)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::setProperty(cx, actual, actual, name, vp, strict);
}

static bool
with_SetElement(JSContext *cx, HandleObject obj, uint32_t index,
                MutableHandleValue vp, bool strict)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::setElement(cx, actual, actual, index, vp, strict);
}

static bool
with_SetSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                MutableHandleValue vp, bool strict)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::setSpecial(cx, actual, actual, sid, vp, strict);
}

static bool
with_GetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::getGenericAttributes(cx, actual, id, attrsp);
}

static bool
with_SetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::setGenericAttributes(cx, actual, id, attrsp);
}

static bool
with_DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                    bool *succeeded)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::deleteProperty(cx, actual, name, succeeded);
}

static bool
with_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                   bool *succeeded)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::deleteElement(cx, actual, index, succeeded);
}

static bool
with_DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                   bool *succeeded)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::deleteSpecial(cx, actual, sid, succeeded);
}

static bool
with_Enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
               MutableHandleValue statep, MutableHandleId idp)
{
    RootedObject actual(cx, &obj->as<WithObject>().object());
    return JSObject::enumerate(cx, actual, enum_op, statep, idp);
}

static JSObject *
with_ThisObject(JSContext *cx, HandleObject obj)
{
    return &obj->as<WithObject>().withThis();
}

const Class WithObject::class_ = {
    "With",
    JSCLASS_HAS_RESERVED_SLOTS(WithObject::RESERVED_SLOTS) |
    JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,                 /* finalize */
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    nullptr,                 /* trace       */
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    {
        with_LookupGeneric,
        with_LookupProperty,
        with_LookupElement,
        with_LookupSpecial,
        nullptr,             /* defineGeneric */
        nullptr,             /* defineProperty */
        nullptr,             /* defineElement */
        nullptr,             /* defineSpecial */
        with_GetGeneric,
        with_GetProperty,
        with_GetElement,
        with_GetSpecial,
        with_SetGeneric,
        with_SetProperty,
        with_SetElement,
        with_SetSpecial,
        with_GetGenericAttributes,
        with_SetGenericAttributes,
        with_DeleteProperty,
        with_DeleteElement,
        with_DeleteSpecial,
        nullptr, nullptr,    /* watch/unwatch */
        nullptr,             /* slice */
        with_Enumerate,
        with_ThisObject,
    }
};

/*****************************************************************************/

ClonedBlockObject *
ClonedBlockObject::create(JSContext *cx, Handle<StaticBlockObject *> block, AbstractFramePtr frame)
{
    assertSameCompartment(cx, frame);
    JS_ASSERT(block->getClass() == &BlockObject::class_);

    RootedTypeObject type(cx, cx->getNewType(&BlockObject::class_, block.get()));
    if (!type)
        return nullptr;

    RootedShape shape(cx, block->lastProperty());

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, gc::TenuredHeap, shape, type));
    if (!obj)
        return nullptr;

    /* Set the parent if necessary, as for call objects. */
    if (&frame.scopeChain()->global() != obj->getParent()) {
        JS_ASSERT(obj->getParent() == nullptr);
        Rooted<GlobalObject*> global(cx, &frame.scopeChain()->global());
        if (!JSObject::setParent(cx, obj, global))
            return nullptr;
    }

    JS_ASSERT(!obj->inDictionaryMode());
    JS_ASSERT(obj->slotSpan() >= block->slotCount() + RESERVED_SLOTS);

    obj->setReservedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*frame.scopeChain()));
    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(block->stackDepth()));

    /*
     * Copy in the closed-over locals. Closed-over locals don't need
     * any fixup since the initial value is 'undefined'.
     */
    unsigned nslots = block->slotCount();
    unsigned base = frame.script()->nfixed() + block->stackDepth();
    for (unsigned i = 0; i < nslots; ++i) {
        if (block->isAliased(i))
            obj->as<ClonedBlockObject>().setVar(i, frame.unaliasedLocal(base + i));
    }

    JS_ASSERT(obj->isDelegate());

    return &obj->as<ClonedBlockObject>();
}

void
ClonedBlockObject::copyUnaliasedValues(AbstractFramePtr frame)
{
    StaticBlockObject &block = staticBlock();
    unsigned base = frame.script()->nfixed() + block.stackDepth();
    for (unsigned i = 0; i < slotCount(); ++i) {
        if (!block.isAliased(i))
            setVar(i, frame.unaliasedLocal(base + i), DONT_CHECK_ALIASING);
    }
}

StaticBlockObject *
StaticBlockObject::create(ExclusiveContext *cx)
{
    RootedTypeObject type(cx, cx->getNewType(&BlockObject::class_, nullptr));
    if (!type)
        return nullptr;

    RootedShape emptyBlockShape(cx);
    emptyBlockShape = EmptyShape::getInitialShape(cx, &BlockObject::class_, nullptr, nullptr,
                                                  nullptr, FINALIZE_KIND, BaseShape::DELEGATE);
    if (!emptyBlockShape)
        return nullptr;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, gc::TenuredHeap, emptyBlockShape, type);
    if (!obj)
        return nullptr;

    return &obj->as<StaticBlockObject>();
}

/* static */ Shape *
StaticBlockObject::addVar(ExclusiveContext *cx, Handle<StaticBlockObject*> block, HandleId id,
                          unsigned index, bool *redeclared)
{
    JS_ASSERT(JSID_IS_ATOM(id) || (JSID_IS_INT(id) && JSID_TO_INT(id) == (int)index));
    JS_ASSERT(index < VAR_INDEX_LIMIT);

    *redeclared = false;

    /* Inline JSObject::addProperty in order to trap the redefinition case. */
    Shape **spp;
    if (Shape::search(cx, block->lastProperty(), id, &spp, true)) {
        *redeclared = true;
        return nullptr;
    }

    /*
     * Don't convert this object to dictionary mode so that we can clone the
     * block's shape later.
     */
    uint32_t slot = JSSLOT_FREE(&BlockObject::class_) + index;
    return JSObject::addPropertyInternal<SequentialExecution>(
        cx, block, id,
        /* getter = */ nullptr, /* setter = */ nullptr,
        slot, JSPROP_ENUMERATE | JSPROP_PERMANENT,
        Shape::HAS_SHORTID, index, spp,
        /* allowDictionary = */ false);
}

const Class BlockObject::class_ = {
    "Block",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(BlockObject::RESERVED_SLOTS) |
    JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

template<XDRMode mode>
bool
js::XDRStaticBlockObject(XDRState<mode> *xdr, HandleObject enclosingScope,
                         StaticBlockObject **objp)
{
    /* NB: Keep this in sync with CloneStaticBlockObject. */

    JSContext *cx = xdr->cx();

    Rooted<StaticBlockObject*> obj(cx);
    uint32_t count = 0;
    uint32_t depthAndCount = 0;

    if (mode == XDR_ENCODE) {
        obj = *objp;
        uint32_t depth = obj->stackDepth();
        JS_ASSERT(depth <= UINT16_MAX);
        count = obj->slotCount();
        JS_ASSERT(count <= UINT16_MAX);
        depthAndCount = (depth << 16) | uint16_t(count);
    }

    if (mode == XDR_DECODE) {
        obj = StaticBlockObject::create(cx);
        if (!obj)
            return false;
        obj->initEnclosingNestedScope(enclosingScope);
        *objp = obj;
    }

    if (!xdr->codeUint32(&depthAndCount))
        return false;

    if (mode == XDR_DECODE) {
        uint32_t depth = uint16_t(depthAndCount >> 16);
        count = uint16_t(depthAndCount);
        obj->setStackDepth(depth);

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (unsigned i = 0; i < count; i++) {
            RootedAtom atom(cx);
            if (!XDRAtom(xdr, &atom))
                return false;

            /* The empty string indicates an int id. */
            RootedId id(cx, atom != cx->runtime()->emptyString
                            ? AtomToId(atom)
                            : INT_TO_JSID(i));

            bool redeclared;
            if (!StaticBlockObject::addVar(cx, obj, id, i, &redeclared)) {
                JS_ASSERT(!redeclared);
                return false;
            }

            uint32_t aliased;
            if (!xdr->codeUint32(&aliased))
                return false;

            JS_ASSERT(aliased == 0 || aliased == 1);
            obj->setAliased(i, !!aliased);
        }
    } else {
        AutoShapeVector shapes(cx);
        if (!shapes.growBy(count))
            return false;

        for (Shape::Range<NoGC> r(obj->lastProperty()); !r.empty(); r.popFront()) {
            Shape *shape = &r.front();
            shapes[shape->shortid()] = shape;
        }

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        RootedShape shape(cx);
        RootedId propid(cx);
        RootedAtom atom(cx);
        for (unsigned i = 0; i < count; i++) {
            shape = shapes[i];
            JS_ASSERT(shape->hasDefaultGetter());
            JS_ASSERT(unsigned(shape->shortid()) == i);

            propid = shape->propid();
            JS_ASSERT(JSID_IS_ATOM(propid) || JSID_IS_INT(propid));

            /* The empty string indicates an int id. */
            atom = JSID_IS_ATOM(propid)
                   ? JSID_TO_ATOM(propid)
                   : cx->runtime()->emptyString;
            if (!XDRAtom(xdr, &atom))
                return false;

            uint32_t aliased = obj->isAliased(i);
            if (!xdr->codeUint32(&aliased))
                return false;
        }
    }
    return true;
}

template bool
js::XDRStaticBlockObject(XDRState<XDR_ENCODE> *, HandleObject, StaticBlockObject **);

template bool
js::XDRStaticBlockObject(XDRState<XDR_DECODE> *, HandleObject, StaticBlockObject **);

static JSObject *
CloneStaticBlockObject(JSContext *cx, HandleObject enclosingScope, Handle<StaticBlockObject*> srcBlock)
{
    /* NB: Keep this in sync with XDRStaticBlockObject. */

    Rooted<StaticBlockObject*> clone(cx, StaticBlockObject::create(cx));
    if (!clone)
        return nullptr;

    clone->initEnclosingNestedScope(enclosingScope);
    clone->setStackDepth(srcBlock->stackDepth());

    /* Shape::Range is reverse order, so build a list in forward order. */
    AutoShapeVector shapes(cx);
    if (!shapes.growBy(srcBlock->slotCount()))
        return nullptr;
    for (Shape::Range<NoGC> r(srcBlock->lastProperty()); !r.empty(); r.popFront())
        shapes[r.front().shortid()] = &r.front();

    for (Shape **p = shapes.begin(); p != shapes.end(); ++p) {
        RootedId id(cx, (*p)->propid());
        unsigned i = (*p)->shortid();

        bool redeclared;
        if (!StaticBlockObject::addVar(cx, clone, id, i, &redeclared)) {
            JS_ASSERT(!redeclared);
            return nullptr;
        }

        clone->setAliased(i, srcBlock->isAliased(i));
    }

    return clone;
}

JSObject *
js::CloneNestedScopeObject(JSContext *cx, HandleObject enclosingScope, Handle<NestedScopeObject*> srcBlock)
{
    JS_ASSERT(srcBlock->is<StaticBlockObject>());
    Rooted<StaticBlockObject *> blockObj(cx, &srcBlock->as<StaticBlockObject>());
    return CloneStaticBlockObject(cx, enclosingScope, blockObj);
}

/*****************************************************************************/

// Any name atom for a function which will be added as a DeclEnv object to the
// scope chain above call objects for fun.
static inline JSAtom *
CallObjectLambdaName(JSFunction &fun)
{
    return fun.isNamedLambda() ? fun.atom() : nullptr;
}

ScopeIter::ScopeIter(const ScopeIter &si, JSContext *cx
                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx),
    frame_(si.frame_),
    cur_(cx, si.cur_),
    staticScope_(cx, si.staticScope_),
    type_(si.type_),
    hasScopeObject_(si.hasScopeObject_)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(JSObject &enclosingScope, JSContext *cx
                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx),
    frame_(NullFramePtr()),
    cur_(cx, &enclosingScope),
    staticScope_(cx, nullptr),
    type_(Type(-1))
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(AbstractFramePtr frame, jsbytecode *pc, JSContext *cx
                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx),
    frame_(frame),
    cur_(cx, frame.scopeChain()),
    staticScope_(cx, frame.script()->getStaticScope(pc))
{
    assertSameCompartment(cx, frame);
    settle();
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(const ScopeIterVal &val, JSContext *cx
                     MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx),
    frame_(val.frame_),
    cur_(cx, val.cur_),
    staticScope_(cx, val.staticScope_),
    type_(val.type_),
    hasScopeObject_(val.hasScopeObject_)
{
    assertSameCompartment(cx, val.frame_);
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeObject &
ScopeIter::scope() const
{
    JS_ASSERT(hasScopeObject());
    return cur_->as<ScopeObject>();
}

ScopeIter &
ScopeIter::operator++()
{
    JS_ASSERT(!done());
    switch (type_) {
      case Call:
        if (hasScopeObject_) {
            cur_ = &cur_->as<CallObject>().enclosingScope();
            if (CallObjectLambdaName(*frame_.fun()))
                cur_ = &cur_->as<DeclEnvObject>().enclosingScope();
        }
        frame_ = NullFramePtr();
        break;
      case Block:
        staticScope_ = staticScope_->as<StaticBlockObject>().enclosingBlock();
        if (hasScopeObject_)
            cur_ = &cur_->as<ClonedBlockObject>().enclosingScope();
        settle();
        break;
      case With:
        JS_ASSERT(hasScopeObject_);
        cur_ = &cur_->as<WithObject>().enclosingScope();
        settle();
        break;
      case StrictEvalScope:
        if (hasScopeObject_)
            cur_ = &cur_->as<CallObject>().enclosingScope();
        frame_ = NullFramePtr();
        break;
    }
    return *this;
}

void
ScopeIter::settle()
{
    /*
     * Given an iterator state (cur_, staticScope_), figure out which (potentially
     * optimized) scope the iterator should report. Thus, the result is a pair
     * (type_, hasScopeObject_) where hasScopeObject_ indicates whether the
     * scope object has been optimized away and does not exist on the scope
     * chain. Beware: while ScopeIter iterates over the scopes of a single
     * frame, the scope chain (pointed to by cur_) continues into the scopes of
     * enclosing frames. Thus, it is important not to look at cur_ until it is
     * certain that cur_ points to a scope object in the current frame. In
     * particular, there are three tricky corner cases:
     *  - non-heavyweight functions;
     *  - non-strict direct eval.
     *  - heavyweight functions observed before the prologue has finished;
     * In all cases, cur_ can already be pointing into an enclosing frame's
     * scope chain. Furthermore, in the first two cases: even if cur_ points
     * into an enclosing frame's scope chain, the current frame may still have
     * uncloned blocks. In the last case, since we haven't entered the
     * function, we simply return a ScopeIter where done() == true.
     *
     * Note: DebugScopeObject falls nicely into this plan: since they are only
     * ever introduced as the *enclosing* scope of a frame, they should never
     * show up in scope iteration and fall into the final non-scope case.
     */
    if (frame_.isNonEvalFunctionFrame() && !frame_.fun()->isHeavyweight()) {
        if (staticScope_) {
            JS_ASSERT(staticScope_->is<StaticBlockObject>());
            type_ = Block;
            hasScopeObject_ = staticScope_->as<StaticBlockObject>().needsClone();
        } else {
            type_ = Call;
            hasScopeObject_ = false;
        }
    } else if (frame_.isNonStrictDirectEvalFrame() && cur_ == frame_.evalPrevScopeChain(cx)) {
        if (staticScope_) {
            JS_ASSERT(staticScope_->is<StaticBlockObject>());
            JS_ASSERT(!staticScope_->as<StaticBlockObject>().needsClone());
            type_ = Block;
            hasScopeObject_ = false;
        } else {
            frame_ = NullFramePtr();
        }
    } else if (frame_.isNonEvalFunctionFrame() && !frame_.hasCallObj()) {
        JS_ASSERT(cur_ == frame_.fun()->environment());
        frame_ = NullFramePtr();
    } else if (frame_.isStrictEvalFrame() && !frame_.hasCallObj()) {
        JS_ASSERT(cur_ == frame_.evalPrevScopeChain(cx));
        frame_ = NullFramePtr();
    } else if (cur_->is<WithObject>()) {
        JS_ASSERT_IF(frame_.isFunctionFrame(), frame_.fun()->isHeavyweight());
        JS_ASSERT_IF(staticScope_, staticScope_->as<StaticBlockObject>().needsClone());
        JS_ASSERT_IF(staticScope_,
                     staticScope_->as<StaticBlockObject>().stackDepth() <
                     cur_->as<WithObject>().stackDepth());
        type_ = With;
        hasScopeObject_ = true;
    } else if (staticScope_) {
        type_ = Block;
        hasScopeObject_ = staticScope_->as<StaticBlockObject>().needsClone();
        JS_ASSERT_IF(hasScopeObject_, cur_->as<ClonedBlockObject>().staticBlock() == *staticScope_);
    } else if (cur_->is<CallObject>()) {
        CallObject &callobj = cur_->as<CallObject>();
        type_ = callobj.isForEval() ? StrictEvalScope : Call;
        hasScopeObject_ = true;
        JS_ASSERT_IF(type_ == Call, callobj.callee().nonLazyScript() == frame_.script());
    } else {
        JS_ASSERT(!cur_->is<ScopeObject>());
        JS_ASSERT(frame_.isGlobalFrame() || frame_.isDebuggerFrame());
        frame_ = NullFramePtr();
    }
}

/* static */ HashNumber
ScopeIterKey::hash(ScopeIterKey si)
{
    /* hasScopeObject_ is determined by the other fields. */
    return size_t(si.frame_.raw()) ^ size_t(si.cur_) ^ size_t(si.staticScope_) ^ si.type_;
}

/* static */ bool
ScopeIterKey::match(ScopeIterKey si1, ScopeIterKey si2)
{
    /* hasScopeObject_ is determined by the other fields. */
    return si1.frame_ == si2.frame_ &&
           (!si1.frame_ ||
            (si1.cur_   == si2.cur_   &&
             si1.staticScope_ == si2.staticScope_ &&
             si1.type_  == si2.type_));
}

// Live ScopeIter values may be added to DebugScopes::liveScopes, as
// ScopeIterVal instances.  They need to have write barriers when they are added
// to the hash table, but no barriers when rehashing inside GC.  It's a nasty
// hack, but the important thing is that ScopeIterKey and ScopeIterVal need to
// alias each other.
void ScopeIterVal::staticAsserts() {
    static_assert(sizeof(ScopeIterVal) == sizeof(ScopeIterKey),
                  "ScopeIterVal must be same size of ScopeIterKey");
    static_assert(offsetof(ScopeIterVal, cur_) == offsetof(ScopeIterKey, cur_),
                  "ScopeIterVal.cur_ must alias ScopeIterKey.cur_");
    static_assert(offsetof(ScopeIterVal, staticScope_) == offsetof(ScopeIterKey, staticScope_),
                  "ScopeIterVal.staticScope_ must alias ScopeIterKey.staticScope_");
}

/*****************************************************************************/

namespace {

/*
 * DebugScopeProxy is the handler for DebugScopeObject proxy objects. Having a
 * custom handler (rather than trying to reuse js::Wrapper) gives us several
 * important abilities:
 *  - We want to pass the ScopeObject as the receiver to forwarded scope
 *    property ops on aliased variables so that Call/Block/With ops do not all
 *    require a 'normalization' step.
 *  - The debug scope proxy can directly manipulate the stack frame to allow
 *    the debugger to read/write args/locals that were otherwise unaliased.
 *  - The debug scope proxy can store unaliased variables after the stack frame
 *    is popped so that they may still be read/written by the debugger.
 *  - The engine has made certain assumptions about the possible reads/writes
 *    in a scope. DebugScopeProxy allows us to prevent the debugger from
 *    breaking those assumptions.
 *  - The engine makes optimizations that are observable to the debugger. The
 *    proxy can either hide these optimizations or make the situation more
 *    clear to the debugger. An example is 'arguments'.
 */
class DebugScopeProxy : public BaseProxyHandler
{
    enum Action { SET, GET };

    /*
     * This function handles access to unaliased locals/formals. Since they are
     * unaliased, the values of these variables are not stored in the slots of
     * the normal Call/BlockObject scope objects and thus must be recovered
     * from somewhere else:
     *  + if the invocation for which the scope was created is still executing,
     *    there is a StackFrame live on the stack holding the values;
     *  + if the invocation for which the scope was created finished executing:
     *     - and there was a DebugScopeObject associated with scope, then the
     *       DebugScopes::onPop(Call|Block) handler copied out the unaliased
     *       variables:
     *        . for block scopes, the unaliased values were copied directly
     *          into the block object, since there is a slot allocated for every
     *          block binding, regardless of whether it is aliased;
     *        . for function scopes, a dense array is created in onPopCall to hold
     *          the unaliased values and attached to the DebugScopeObject;
     *     - and there was not a DebugScopeObject yet associated with the
     *       scope, then the unaliased values are lost and not recoverable.
     *
     * handleUnaliasedAccess returns 'true' if the access was unaliased and
     * completed by handleUnaliasedAccess.
     */
    bool handleUnaliasedAccess(JSContext *cx, Handle<DebugScopeObject*> debugScope, Handle<ScopeObject*> scope,
                               jsid id, Action action, MutableHandleValue vp)
    {
        JS_ASSERT(&debugScope->scope() == scope);
        ScopeIterVal *maybeLiveScope = DebugScopes::hasLiveScope(*scope);

        /* Handle unaliased formals, vars, and consts at function scope. */
        if (scope->is<CallObject>() && !scope->as<CallObject>().isForEval()) {
            CallObject &callobj = scope->as<CallObject>();
            RootedScript script(cx, callobj.callee().nonLazyScript());
            if (!script->ensureHasTypes(cx))
                return false;

            Bindings &bindings = script->bindings;
            BindingIter bi(script);
            while (bi && NameToId(bi->name()) != id)
                bi++;
            if (!bi)
                return false;

            if (bi->kind() == VARIABLE || bi->kind() == CONSTANT) {
                uint32_t i = bi.frameIndex();
                if (script->varIsAliased(i))
                    return false;

                if (maybeLiveScope) {
                    AbstractFramePtr frame = maybeLiveScope->frame();
                    if (action == GET)
                        vp.set(frame.unaliasedVar(i));
                    else
                        frame.unaliasedVar(i) = vp;
                } else if (JSObject *snapshot = debugScope->maybeSnapshot()) {
                    if (action == GET)
                        vp.set(snapshot->getDenseElement(bindings.numArgs() + i));
                    else
                        snapshot->setDenseElement(bindings.numArgs() + i, vp);
                } else {
                    /* The unaliased value has been lost to the debugger. */
                    if (action == GET)
                        vp.set(UndefinedValue());
                }
            } else {
                JS_ASSERT(bi->kind() == ARGUMENT);
                unsigned i = bi.frameIndex();
                if (script->formalIsAliased(i))
                    return false;

                if (maybeLiveScope) {
                    AbstractFramePtr frame = maybeLiveScope->frame();
                    if (script->argsObjAliasesFormals() && frame.hasArgsObj()) {
                        if (action == GET)
                            vp.set(frame.argsObj().arg(i));
                        else
                            frame.argsObj().setArg(i, vp);
                    } else {
                        if (action == GET)
                            vp.set(frame.unaliasedFormal(i, DONT_CHECK_ALIASING));
                        else
                            frame.unaliasedFormal(i, DONT_CHECK_ALIASING) = vp;
                    }
                } else if (JSObject *snapshot = debugScope->maybeSnapshot()) {
                    if (action == GET)
                        vp.set(snapshot->getDenseElement(i));
                    else
                        snapshot->setDenseElement(i, vp);
                } else {
                    /* The unaliased value has been lost to the debugger. */
                    if (action == GET)
                        vp.set(UndefinedValue());
                }

                if (action == SET)
                    TypeScript::SetArgument(cx, script, i, vp);
            }

            return true;
        }

        /* Handle unaliased let and catch bindings at block scope. */
        if (scope->is<ClonedBlockObject>()) {
            Rooted<ClonedBlockObject *> block(cx, &scope->as<ClonedBlockObject>());
            Shape *shape = block->lastProperty()->search(cx, id);
            if (!shape)
                return false;

            unsigned i = shape->shortid();
            if (block->staticBlock().isAliased(i))
                return false;

            if (maybeLiveScope) {
                AbstractFramePtr frame = maybeLiveScope->frame();
                JSScript *script = frame.script();
                uint32_t local = block->slotToLocalIndex(script->bindings, shape->slot());
                if (action == GET)
                    vp.set(frame.unaliasedLocal(local));
                else
                    frame.unaliasedLocal(local) = vp;
                JS_ASSERT(analyze::LocalSlot(script, local) >= analyze::TotalSlots(script));
            } else {
                if (action == GET)
                    vp.set(block->var(i, DONT_CHECK_ALIASING));
                else
                    block->setVar(i, vp, DONT_CHECK_ALIASING);
            }

            return true;
        }

        /* The rest of the internal scopes do not have unaliased vars. */
        JS_ASSERT(scope->is<DeclEnvObject>() || scope->is<WithObject>() ||
                  scope->as<CallObject>().isForEval());
        return false;
    }

    static bool isArguments(JSContext *cx, jsid id)
    {
        return id == NameToId(cx->names().arguments);
    }

    static bool isFunctionScope(ScopeObject &scope)
    {
        return scope.is<CallObject>() && !scope.as<CallObject>().isForEval();
    }

    /*
     * In theory, every function scope contains an 'arguments' bindings.
     * However, the engine only adds a binding if 'arguments' is used in the
     * function body. Thus, from the debugger's perspective, 'arguments' may be
     * missing from the list of bindings.
     */
    static bool isMissingArgumentsBinding(ScopeObject &scope)
    {
        return isFunctionScope(scope) &&
               !scope.as<CallObject>().callee().nonLazyScript()->argumentsHasVarBinding();
    }

    /*
     * This function creates an arguments object when the debugger requests
     * 'arguments' for a function scope where the arguments object has been
     * optimized away (either because the binding is missing altogether or
     * because !ScriptAnalysis::needsArgsObj).
     */
    static bool checkForMissingArguments(JSContext *cx, jsid id, ScopeObject &scope,
                                         ArgumentsObject **maybeArgsObj)
    {
        *maybeArgsObj = nullptr;

        if (!isArguments(cx, id) || !isFunctionScope(scope))
            return true;

        if (scope.as<CallObject>().callee().nonLazyScript()->needsArgsObj())
            return true;

        ScopeIterVal *maybeScope = DebugScopes::hasLiveScope(scope);
        if (!maybeScope) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_DEBUG_NOT_LIVE,
                                 "Debugger scope");
            return false;
        }

        *maybeArgsObj = ArgumentsObject::createUnexpected(cx, maybeScope->frame());
        return true;
    }

  public:
    static int family;
    static DebugScopeProxy singleton;

    DebugScopeProxy() : BaseProxyHandler(&family) {}

    bool isExtensible(JSContext *cx, HandleObject proxy, bool *extensible) MOZ_OVERRIDE
    {
        // always [[Extensible]], can't be made non-[[Extensible]], like most
        // proxies
        *extensible = true;
        return true;
    }

    bool preventExtensions(JSContext *cx, HandleObject proxy) MOZ_OVERRIDE
    {
        // See above.
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CHANGE_EXTENSIBILITY);
        return false;
    }

    bool getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                               MutableHandle<PropertyDescriptor> desc,
                               unsigned flags) MOZ_OVERRIDE
    {
        return getOwnPropertyDescriptor(cx, proxy, id, desc, flags);
    }

    bool getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                  MutableHandle<PropertyDescriptor> desc,
                                  unsigned flags) MOZ_OVERRIDE
    {
        Rooted<DebugScopeObject*> debugScope(cx, &proxy->as<DebugScopeObject>());
        Rooted<ScopeObject*> scope(cx, &debugScope->scope());

        RootedArgumentsObject maybeArgsObj(cx);
        if (!checkForMissingArguments(cx, id, *scope, maybeArgsObj.address()))
            return false;

        if (maybeArgsObj) {
            desc.object().set(debugScope);
            desc.setAttributes(JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT);
            desc.value().setObject(*maybeArgsObj);
            desc.setShortId(0);
            desc.setGetter(nullptr);
            desc.setSetter(nullptr);
            return true;
        }

        RootedValue v(cx);
        if (handleUnaliasedAccess(cx, debugScope, scope, id, GET, &v)) {
            desc.object().set(debugScope);
            desc.setAttributes(JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT);
            desc.value().set(v);
            desc.setShortId(0);
            desc.setGetter(nullptr);
            desc.setSetter(nullptr);
            return true;
        }

        return JS_GetOwnPropertyDescriptorById(cx, scope, id, flags, desc);
    }

    bool get(JSContext *cx, HandleObject proxy, HandleObject receiver,  HandleId id,
             MutableHandleValue vp) MOZ_OVERRIDE
    {
        Rooted<DebugScopeObject*> debugScope(cx, &proxy->as<DebugScopeObject>());
        Rooted<ScopeObject*> scope(cx, &proxy->as<DebugScopeObject>().scope());

        RootedArgumentsObject maybeArgsObj(cx);
        if (!checkForMissingArguments(cx, id, *scope, maybeArgsObj.address()))
            return false;

        if (maybeArgsObj) {
            vp.set(ObjectValue(*maybeArgsObj));
            return true;
        }

        if (handleUnaliasedAccess(cx, debugScope, scope, id, GET, vp))
            return true;

        return JSObject::getGeneric(cx, scope, scope, id, vp);
    }

    bool set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id, bool strict,
             MutableHandleValue vp) MOZ_OVERRIDE
    {
        Rooted<DebugScopeObject*> debugScope(cx, &proxy->as<DebugScopeObject>());
        Rooted<ScopeObject*> scope(cx, &proxy->as<DebugScopeObject>().scope());
        if (handleUnaliasedAccess(cx, debugScope, scope, id, SET, vp))
            return true;
        return JSObject::setGeneric(cx, scope, scope, id, vp, strict);
    }

    bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                        MutableHandle<PropertyDescriptor> desc) MOZ_OVERRIDE
    {
        Rooted<ScopeObject*> scope(cx, &proxy->as<DebugScopeObject>().scope());

        bool found;
        if (!has(cx, proxy, id, &found))
            return false;
        if (found)
            return Throw(cx, id, JSMSG_CANT_REDEFINE_PROP);

        return JS_DefinePropertyById(cx, scope, id, desc.value(), desc.getter(), desc.setter(),
                                     desc.attributes());
    }

    bool getScopePropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props,
                               unsigned flags)
    {
        Rooted<ScopeObject*> scope(cx, &proxy->as<DebugScopeObject>().scope());

        if (isMissingArgumentsBinding(*scope)) {
            if (!props.append(NameToId(cx->names().arguments)))
                return false;
        }

        if (!GetPropertyNames(cx, scope, flags, &props))
            return false;

        /*
         * Function scopes are optimized to not contain unaliased variables so
         * they must be manually appended here.
         */
        if (scope->is<CallObject>() && !scope->as<CallObject>().isForEval()) {
            RootedScript script(cx, scope->as<CallObject>().callee().nonLazyScript());
            for (BindingIter bi(script); bi; bi++) {
                if (!bi->aliased() && !props.append(NameToId(bi->name())))
                    return false;
            }
        }

        return true;
    }

    bool getOwnPropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        return getScopePropertyNames(cx, proxy, props, JSITER_OWNONLY);
    }

    bool enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        return getScopePropertyNames(cx, proxy, props, 0);
    }

    bool has(JSContext *cx, HandleObject proxy, HandleId id_, bool *bp) MOZ_OVERRIDE
    {
        RootedId id(cx, id_);
        ScopeObject &scopeObj = proxy->as<DebugScopeObject>().scope();

        if (isArguments(cx, id) && isFunctionScope(scopeObj)) {
            *bp = true;
            return true;
        }

        bool found;
        RootedObject scope(cx, &scopeObj);
        if (!JS_HasPropertyById(cx, scope, id, &found))
            return false;

        /*
         * Function scopes are optimized to not contain unaliased variables so
         * a manual search is necessary.
         */
        if (!found && scope->is<CallObject>() && !scope->as<CallObject>().isForEval()) {
            RootedScript script(cx, scope->as<CallObject>().callee().nonLazyScript());
            for (BindingIter bi(script); bi; bi++) {
                if (!bi->aliased() && NameToId(bi->name()) == id) {
                    found = true;
                    break;
                }
            }
        }

        *bp = found;
        return true;
    }

    bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE
    {
        RootedValue idval(cx, IdToValue(id));
        return js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_CANT_DELETE,
                                        JSDVG_IGNORE_STACK, idval, NullPtr(), nullptr, nullptr);
    }
};

} /* anonymous namespace */

int DebugScopeProxy::family = 0;
DebugScopeProxy DebugScopeProxy::singleton;

/* static */ DebugScopeObject *
DebugScopeObject::create(JSContext *cx, ScopeObject &scope, HandleObject enclosing)
{
    JS_ASSERT(scope.compartment() == cx->compartment());
    RootedValue priv(cx, ObjectValue(scope));
    JSObject *obj = NewProxyObject(cx, &DebugScopeProxy::singleton, priv,
                                   nullptr /* proto */, &scope.global());
    if (!obj)
        return nullptr;

    JS_ASSERT(!enclosing->is<ScopeObject>());

    DebugScopeObject *debugScope = &obj->as<DebugScopeObject>();
    debugScope->setExtra(ENCLOSING_EXTRA, ObjectValue(*enclosing));
    debugScope->setExtra(SNAPSHOT_EXTRA, NullValue());

    return debugScope;
}

ScopeObject &
DebugScopeObject::scope() const
{
    return target()->as<ScopeObject>();
}

JSObject &
DebugScopeObject::enclosingScope() const
{
    return extra(ENCLOSING_EXTRA).toObject();
}

JSObject *
DebugScopeObject::maybeSnapshot() const
{
    JS_ASSERT(!scope().as<CallObject>().isForEval());
    return extra(SNAPSHOT_EXTRA).toObjectOrNull();
}

void
DebugScopeObject::initSnapshot(JSObject &o)
{
    JS_ASSERT(maybeSnapshot() == nullptr);
    setExtra(SNAPSHOT_EXTRA, ObjectValue(o));
}

bool
DebugScopeObject::isForDeclarative() const
{
    ScopeObject &s = scope();
    return s.is<CallObject>() || s.is<BlockObject>() || s.is<DeclEnvObject>();
}

bool
js_IsDebugScopeSlow(ProxyObject *proxy)
{
    JS_ASSERT(proxy->hasClass(&ProxyObject::uncallableClass_));
    return proxy->handler() == &DebugScopeProxy::singleton;
}

/*****************************************************************************/

/* static */ MOZ_ALWAYS_INLINE void
DebugScopes::proxiedScopesPostWriteBarrier(JSRuntime *rt, ObjectWeakMap *map,
                                           const EncapsulatedPtr<JSObject> &key)
{
#ifdef JSGC_GENERATIONAL
    /*
     * Strip the barriers from the type before inserting into the store buffer.
     * This will automatically ensure that barriers do not fire during GC.
     *
     * Some compilers complain about instantiating the WeakMap class for
     * unbarriered type arguments, so we cast to a HashMap instead.  Because of
     * WeakMap's multiple inheritace, We need to do this in two stages, first to
     * the HashMap base class and then to the unbarriered version.
     */
    ObjectWeakMap::Base *baseHashMap = static_cast<ObjectWeakMap::Base *>(map);

    typedef HashMap<JSObject *, JSObject *> UnbarrieredMap;
    UnbarrieredMap *unbarrieredMap = reinterpret_cast<UnbarrieredMap *>(baseHashMap);

    typedef gc::HashKeyRef<UnbarrieredMap, JSObject *> Ref;
    if (key && IsInsideNursery(rt, key))
        rt->gcStoreBuffer.putGeneric(Ref(unbarrieredMap, key.get()));
#endif
}

#ifdef JSGC_GENERATIONAL
class DebugScopes::MissingScopesRef : public gc::BufferableRef
{
    MissingScopeMap *map;
    ScopeIterKey key;

  public:
    MissingScopesRef(MissingScopeMap *m, const ScopeIterKey &k) : map(m), key(k) {}

    void mark(JSTracer *trc) {
        ScopeIterKey prior = key;
        MissingScopeMap::Ptr p = map->lookup(key);
        if (!p)
            return;
        JS_SET_TRACING_LOCATION(trc, &const_cast<ScopeIterKey &>(p->key()).enclosingScope());
        Mark(trc, &key.enclosingScope(), "MissingScopesRef");
        map->rekeyIfMoved(prior, key);
    }
};
#endif

/* static */ MOZ_ALWAYS_INLINE void
DebugScopes::missingScopesPostWriteBarrier(JSRuntime *rt, MissingScopeMap *map,
                                           const ScopeIterKey &key)
{
#ifdef JSGC_GENERATIONAL
    if (key.enclosingScope() && IsInsideNursery(rt, key.enclosingScope()))
        rt->gcStoreBuffer.putGeneric(MissingScopesRef(map, key));
#endif
}

/* static */ MOZ_ALWAYS_INLINE void
DebugScopes::liveScopesPostWriteBarrier(JSRuntime *rt, LiveScopeMap *map, ScopeObject *key)
{
#ifdef JSGC_GENERATIONAL
    // As above.  Otherwise, barriers could fire during GC when moving the
    // value.
    typedef HashMap<ScopeObject *,
                    ScopeIterKey,
                    DefaultHasher<ScopeObject *>,
                    RuntimeAllocPolicy> UnbarrieredLiveScopeMap;
    typedef gc::HashKeyRef<UnbarrieredLiveScopeMap, ScopeObject *> Ref;
    if (key && IsInsideNursery(rt, key))
        rt->gcStoreBuffer.putGeneric(Ref(reinterpret_cast<UnbarrieredLiveScopeMap *>(map), key));
#endif
}

DebugScopes::DebugScopes(JSContext *cx)
 : proxiedScopes(cx),
   missingScopes(cx->runtime()),
   liveScopes(cx->runtime())
{}

DebugScopes::~DebugScopes()
{
    JS_ASSERT(missingScopes.empty());
    WeakMapBase::removeWeakMapFromList(&proxiedScopes);
}

bool
DebugScopes::init()
{
    if (!liveScopes.init() ||
        !proxiedScopes.init() ||
        !missingScopes.init())
    {
        return false;
    }
    return true;
}

void
DebugScopes::mark(JSTracer *trc)
{
    proxiedScopes.trace(trc);
}

void
DebugScopes::sweep(JSRuntime *rt)
{
    /*
     * missingScopes points to debug scopes weakly so that debug scopes can be
     * released more eagerly.
     */
    for (MissingScopeMap::Enum e(missingScopes); !e.empty(); e.popFront()) {
        DebugScopeObject **debugScope = e.front().value().unsafeGet();
        if (IsObjectAboutToBeFinalized(debugScope)) {
            /*
             * Note that onPopCall and onPopBlock rely on missingScopes to find
             * scope objects that we synthesized for the debugger's sake, and
             * clean up the synthetic scope objects' entries in liveScopes. So
             * if we remove an entry frcom missingScopes here, we must also
             * remove the corresponding liveScopes entry.
             *
             * Since the DebugScopeObject is the only thing using its scope
             * object, and the DSO is about to be finalized, you might assume
             * that the synthetic SO is also about to be finalized too, and thus
             * the loop below will take care of things. But complex GC behavior
             * means that marks are only conservative approximations of
             * liveness; we should assume that anything could be marked.
             *
             * Thus, we must explicitly remove the entries from both liveScopes
             * and missingScopes here.
             */
            liveScopes.remove(&(*debugScope)->scope());
            e.removeFront();
        }
    }

    for (LiveScopeMap::Enum e(liveScopes); !e.empty(); e.popFront()) {
        ScopeObject *scope = e.front().key();

        /*
         * Scopes can be finalized when a debugger-synthesized ScopeObject is
         * no longer reachable via its DebugScopeObject.
         */
        if (IsObjectAboutToBeFinalized(&scope)) {
            e.removeFront();
            continue;
        }
    }
}

#if defined(JSGC_GENERATIONAL) && defined(JS_GC_ZEAL)
void
DebugScopes::checkHashTablesAfterMovingGC(JSRuntime *runtime)
{
    /*
     * This is called at the end of StoreBuffer::mark() to check that our
     * postbarriers have worked and that no hashtable keys (or values) are left
     * pointing into the nursery.
     */
    JS::shadow::Runtime *rt = JS::shadow::Runtime::asShadowRuntime(runtime);
    for (ObjectWeakMap::Range r = proxiedScopes.all(); !r.empty(); r.popFront()) {
        JS_ASSERT(!IsInsideNursery(rt, r.front().key().get()));
        JS_ASSERT(!IsInsideNursery(rt, r.front().value().get()));
    }
    for (MissingScopeMap::Range r = missingScopes.all(); !r.empty(); r.popFront()) {
        JS_ASSERT(!IsInsideNursery(rt, r.front().key().cur()));
        JS_ASSERT(!IsInsideNursery(rt, r.front().key().staticScope()));
        JS_ASSERT(!IsInsideNursery(rt, r.front().value().get()));
    }
    for (LiveScopeMap::Range r = liveScopes.all(); !r.empty(); r.popFront()) {
        JS_ASSERT(!IsInsideNursery(rt, r.front().key()));
        JS_ASSERT(!IsInsideNursery(rt, r.front().value().cur_.get()));
        JS_ASSERT(!IsInsideNursery(rt, r.front().value().staticScope_.get()));
    }
}
#endif

/*
 * Unfortunately, GetDebugScopeForFrame needs to work even outside debug mode
 * (in particular, JS_GetFrameScopeChain does not require debug mode). Since
 * DebugScopes::onPop* are only called in debug mode, this means we cannot
 * use any of the maps in DebugScopes. This will produce debug scope chains
 * that do not obey the debugger invariants but that is just fine.
 */
static bool
CanUseDebugScopeMaps(JSContext *cx)
{
    return cx->compartment()->debugMode();
}

DebugScopes *
DebugScopes::ensureCompartmentData(JSContext *cx)
{
    JSCompartment *c = cx->compartment();
    if (c->debugScopes)
        return c->debugScopes;

    c->debugScopes = cx->runtime()->new_<DebugScopes>(cx);
    if (c->debugScopes && c->debugScopes->init())
        return c->debugScopes;

    js_ReportOutOfMemory(cx);
    return nullptr;
}

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, ScopeObject &scope)
{
    DebugScopes *scopes = scope.compartment()->debugScopes;
    if (!scopes)
        return nullptr;

    if (ObjectWeakMap::Ptr p = scopes->proxiedScopes.lookup(&scope)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return &p->value()->as<DebugScopeObject>();
    }

    return nullptr;
}

bool
DebugScopes::addDebugScope(JSContext *cx, ScopeObject &scope, DebugScopeObject &debugScope)
{
    JS_ASSERT(cx->compartment() == scope.compartment());
    JS_ASSERT(cx->compartment() == debugScope.compartment());

    if (!CanUseDebugScopeMaps(cx))
        return true;

    DebugScopes *scopes = ensureCompartmentData(cx);
    if (!scopes)
        return false;

    JS_ASSERT(!scopes->proxiedScopes.has(&scope));
    if (!scopes->proxiedScopes.put(&scope, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    proxiedScopesPostWriteBarrier(cx->runtime(), &scopes->proxiedScopes, &scope);
    return true;
}

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, const ScopeIter &si)
{
    JS_ASSERT(!si.hasScopeObject());

    DebugScopes *scopes = cx->compartment()->debugScopes;
    if (!scopes)
        return nullptr;

    if (MissingScopeMap::Ptr p = scopes->missingScopes.lookup(si)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return p->value();
    }
    return nullptr;
}

bool
DebugScopes::addDebugScope(JSContext *cx, const ScopeIter &si, DebugScopeObject &debugScope)
{
    JS_ASSERT(!si.hasScopeObject());
    JS_ASSERT(cx->compartment() == debugScope.compartment());
    JS_ASSERT_IF(si.frame().isFunctionFrame(), !si.frame().callee()->isGenerator());

    if (!CanUseDebugScopeMaps(cx))
        return true;

    DebugScopes *scopes = ensureCompartmentData(cx);
    if (!scopes)
        return false;

    JS_ASSERT(!scopes->missingScopes.has(si));
    if (!scopes->missingScopes.put(si, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    missingScopesPostWriteBarrier(cx->runtime(), &scopes->missingScopes, si);

    JS_ASSERT(!scopes->liveScopes.has(&debugScope.scope()));
    if (!scopes->liveScopes.put(&debugScope.scope(), si)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    liveScopesPostWriteBarrier(cx->runtime(), &scopes->liveScopes, &debugScope.scope());

    return true;
}

void
DebugScopes::onPopCall(AbstractFramePtr frame, JSContext *cx)
{
    JS_ASSERT(!frame.isYielding());
    assertSameCompartment(cx, frame);

    DebugScopes *scopes = cx->compartment()->debugScopes;
    if (!scopes)
        return;

    Rooted<DebugScopeObject*> debugScope(cx, nullptr);

    if (frame.fun()->isHeavyweight()) {
        /*
         * The StackFrame may be observed before the prologue has created the
         * CallObject. See ScopeIter::settle.
         */
        if (!frame.hasCallObj())
            return;

        CallObject &callobj = frame.scopeChain()->as<CallObject>();
        scopes->liveScopes.remove(&callobj);
        if (ObjectWeakMap::Ptr p = scopes->proxiedScopes.lookup(&callobj))
            debugScope = &p->value()->as<DebugScopeObject>();
    } else {
        ScopeIter si(frame, frame.script()->main(), cx);
        if (MissingScopeMap::Ptr p = scopes->missingScopes.lookup(si)) {
            debugScope = p->value();
            scopes->liveScopes.remove(&debugScope->scope().as<CallObject>());
            scopes->missingScopes.remove(p);
        }
    }

    /*
     * When the StackFrame is popped, the values of unaliased variables
     * are lost. If there is any debug scope referring to this scope, save a
     * copy of the unaliased variables' values in an array for later debugger
     * access via DebugScopeProxy::handleUnaliasedAccess.
     *
     * Note: since it is simplest for this function to be infallible, failure
     * in this code will be silently ignored. This does not break any
     * invariants since DebugScopeObject::maybeSnapshot can already be nullptr.
     */
    if (debugScope) {
        /*
         * Copy all frame values into the snapshot, regardless of
         * aliasing. This unnecessarily includes aliased variables
         * but it simplifies later indexing logic.
         */
        AutoValueVector vec(cx);
        if (!frame.copyRawFrameSlots(&vec) || vec.length() == 0)
            return;

        /*
         * Copy in formals that are not aliased via the scope chain
         * but are aliased via the arguments object.
         */
        RootedScript script(cx, frame.script());
        if (script->analyzedArgsUsage() && script->needsArgsObj() && frame.hasArgsObj()) {
            for (unsigned i = 0; i < frame.numFormalArgs(); ++i) {
                if (script->formalLivesInArgumentsObject(i))
                    vec[i] = frame.argsObj().arg(i);
            }
        }

        /*
         * Use a dense array as storage (since proxies do not have trace
         * hooks). This array must not escape into the wild.
         */
        RootedObject snapshot(cx, NewDenseCopiedArray(cx, vec.length(), vec.begin()));
        if (!snapshot) {
            cx->clearPendingException();
            return;
        }

        debugScope->initSnapshot(*snapshot);
    }
}

void
DebugScopes::onPopBlock(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc)
{
    assertSameCompartment(cx, frame);

    DebugScopes *scopes = cx->compartment()->debugScopes;
    if (!scopes)
        return;

    ScopeIter si(frame, pc, cx);
    onPopBlock(cx, si);
}

void
DebugScopes::onPopBlock(JSContext *cx, const ScopeIter &si)
{
    DebugScopes *scopes = cx->compartment()->debugScopes;
    if (!scopes)
        return;

    JS_ASSERT(si.type() == ScopeIter::Block);

    if (si.staticBlock().needsClone()) {
        ClonedBlockObject &clone = si.scope().as<ClonedBlockObject>();
        clone.copyUnaliasedValues(si.frame());
        scopes->liveScopes.remove(&clone);
    } else {
        if (MissingScopeMap::Ptr p = scopes->missingScopes.lookup(si)) {
            ClonedBlockObject &clone = p->value()->scope().as<ClonedBlockObject>();
            clone.copyUnaliasedValues(si.frame());
            scopes->liveScopes.remove(&clone);
            scopes->missingScopes.remove(p);
        }
    }
}

void
DebugScopes::onPopWith(AbstractFramePtr frame)
{
    DebugScopes *scopes = frame.compartment()->debugScopes;
    if (scopes)
        scopes->liveScopes.remove(&frame.scopeChain()->as<WithObject>());
}

void
DebugScopes::onPopStrictEvalScope(AbstractFramePtr frame)
{
    DebugScopes *scopes = frame.compartment()->debugScopes;
    if (!scopes)
        return;

    /*
     * The StackFrame may be observed before the prologue has created the
     * CallObject. See ScopeIter::settle.
     */
    if (frame.hasCallObj())
        scopes->liveScopes.remove(&frame.scopeChain()->as<CallObject>());
}

void
DebugScopes::onCompartmentLeaveDebugMode(JSCompartment *c)
{
    DebugScopes *scopes = c->debugScopes;
    if (scopes) {
        scopes->proxiedScopes.clear();
        scopes->missingScopes.clear();
        scopes->liveScopes.clear();
    }
}

bool
DebugScopes::updateLiveScopes(JSContext *cx)
{
    JS_CHECK_RECURSION(cx, return false);

    /*
     * Note that we must always update the top frame's scope objects' entries
     * in liveScopes because we can't be sure code hasn't run in that frame to
     * change the scope chain since we were last called. The fp->prevUpToDate()
     * flag indicates whether the scopes of frames older than fp are already
     * included in liveScopes. It might seem simpler to have fp instead carry a
     * flag indicating whether fp itself is accurately described, but then we
     * would need to clear that flag whenever fp ran code. By storing the 'up
     * to date' bit for fp->prev() in fp, simply popping fp effectively clears
     * the flag for us, at exactly the time when execution resumes fp->prev().
     */
    for (AllFramesIter i(cx); !i.done(); ++i) {
        /*
         * Debug-mode currently disables Ion compilation in the compartment of
         * the debuggee.
         */
        if (i.isIon())
            continue;

        AbstractFramePtr frame = i.abstractFramePtr();
        if (frame.scopeChain()->compartment() != cx->compartment())
            continue;

        if (frame.isFunctionFrame() && frame.callee()->isGenerator())
            continue;

        for (ScopeIter si(frame, i.pc(), cx); !si.done(); ++si) {
            if (si.hasScopeObject()) {
                JS_ASSERT(si.scope().compartment() == cx->compartment());
                DebugScopes *scopes = ensureCompartmentData(cx);
                if (!scopes)
                    return false;
                if (!scopes->liveScopes.put(&si.scope(), si))
                    return false;
                liveScopesPostWriteBarrier(cx->runtime(), &scopes->liveScopes, &si.scope());
            }
        }

        if (frame.prevUpToDate())
            return true;
        JS_ASSERT(frame.scopeChain()->compartment()->debugMode());
        frame.setPrevUpToDate();
    }

    return true;
}

ScopeIterVal*
DebugScopes::hasLiveScope(ScopeObject &scope)
{
    DebugScopes *scopes = scope.compartment()->debugScopes;
    if (!scopes)
        return nullptr;

    if (LiveScopeMap::Ptr p = scopes->liveScopes.lookup(&scope))
        return &p->value();

    return nullptr;
}

/*****************************************************************************/

static JSObject *
GetDebugScope(JSContext *cx, const ScopeIter &si);

static DebugScopeObject *
GetDebugScopeForScope(JSContext *cx, Handle<ScopeObject*> scope, const ScopeIter &enclosing)
{
    if (DebugScopeObject *debugScope = DebugScopes::hasDebugScope(cx, *scope))
        return debugScope;

    RootedObject enclosingDebug(cx, GetDebugScope(cx, enclosing));
    if (!enclosingDebug)
        return nullptr;

    JSObject &maybeDecl = scope->enclosingScope();
    if (maybeDecl.is<DeclEnvObject>()) {
        JS_ASSERT(CallObjectLambdaName(scope->as<CallObject>().callee()));
        enclosingDebug = DebugScopeObject::create(cx, maybeDecl.as<DeclEnvObject>(), enclosingDebug);
        if (!enclosingDebug)
            return nullptr;
    }

    DebugScopeObject *debugScope = DebugScopeObject::create(cx, *scope, enclosingDebug);
    if (!debugScope)
        return nullptr;

    if (!DebugScopes::addDebugScope(cx, *scope, *debugScope))
        return nullptr;

    return debugScope;
}

static DebugScopeObject *
GetDebugScopeForMissing(JSContext *cx, const ScopeIter &si)
{
    if (DebugScopeObject *debugScope = DebugScopes::hasDebugScope(cx, si))
        return debugScope;

    ScopeIter copy(si, cx);
    RootedObject enclosingDebug(cx, GetDebugScope(cx, ++copy));
    if (!enclosingDebug)
        return nullptr;

    /*
     * Create the missing scope object. For block objects, this takes care of
     * storing variable values after the StackFrame has been popped. For call
     * objects, we only use the pretend call object to access callee, bindings
     * and to receive dynamically added properties. Together, this provides the
     * nice invariant that every DebugScopeObject has a ScopeObject.
     *
     * Note: to preserve scopeChain depth invariants, these lazily-reified
     * scopes must not be put on the frame's scope chain; instead, they are
     * maintained via DebugScopes hooks.
     */
    DebugScopeObject *debugScope = nullptr;
    switch (si.type()) {
      case ScopeIter::Call: {
        // Generators should always reify their scopes.
        JS_ASSERT(!si.frame().callee()->isGenerator());
        Rooted<CallObject*> callobj(cx, CallObject::createForFunction(cx, si.frame()));
        if (!callobj)
            return nullptr;

        if (callobj->enclosingScope().is<DeclEnvObject>()) {
            JS_ASSERT(CallObjectLambdaName(callobj->callee()));
            DeclEnvObject &declenv = callobj->enclosingScope().as<DeclEnvObject>();
            enclosingDebug = DebugScopeObject::create(cx, declenv, enclosingDebug);
            if (!enclosingDebug)
                return nullptr;
        }

        debugScope = DebugScopeObject::create(cx, *callobj, enclosingDebug);
        break;
      }
      case ScopeIter::Block: {
        // Generators should always reify their scopes.
        JS_ASSERT_IF(si.frame().isFunctionFrame(), !si.frame().callee()->isGenerator());
        Rooted<StaticBlockObject *> staticBlock(cx, &si.staticBlock());
        ClonedBlockObject *block = ClonedBlockObject::create(cx, staticBlock, si.frame());
        if (!block)
            return nullptr;

        debugScope = DebugScopeObject::create(cx, *block, enclosingDebug);
        break;
      }
      case ScopeIter::With:
      case ScopeIter::StrictEvalScope:
        MOZ_ASSUME_UNREACHABLE("should already have a scope");
    }
    if (!debugScope)
        return nullptr;

    if (!DebugScopes::addDebugScope(cx, si, *debugScope))
        return nullptr;

    return debugScope;
}

static JSObject *
GetDebugScope(JSContext *cx, JSObject &obj)
{
    /*
     * As an engine invariant (maintained internally and asserted by Execute),
     * ScopeObjects and non-ScopeObjects cannot be interleaved on the scope
     * chain; every scope chain must start with zero or more ScopeObjects and
     * terminate with one or more non-ScopeObjects (viz., GlobalObject).
     */
    if (!obj.is<ScopeObject>()) {
#ifdef DEBUG
        JSObject *o = &obj;
        while ((o = o->enclosingScope()))
            JS_ASSERT(!o->is<ScopeObject>());
#endif
        return &obj;
    }

    Rooted<ScopeObject*> scope(cx, &obj.as<ScopeObject>());
    if (ScopeIterVal *maybeLiveScope = DebugScopes::hasLiveScope(*scope)) {
        ScopeIter si(*maybeLiveScope, cx);
        return GetDebugScope(cx, si);
    }
    ScopeIter si(scope->enclosingScope(), cx);
    return GetDebugScopeForScope(cx, scope, si);
}

static JSObject *
GetDebugScope(JSContext *cx, const ScopeIter &si)
{
    JS_CHECK_RECURSION(cx, return nullptr);

    if (si.done())
        return GetDebugScope(cx, si.enclosingScope());

    if (!si.hasScopeObject())
        return GetDebugScopeForMissing(cx, si);

    Rooted<ScopeObject*> scope(cx, &si.scope());

    ScopeIter copy(si, cx);
    return GetDebugScopeForScope(cx, scope, ++copy);
}

JSObject *
js::GetDebugScopeForFunction(JSContext *cx, HandleFunction fun)
{
    assertSameCompartment(cx, fun);
    JS_ASSERT(cx->compartment()->debugMode());
    if (!DebugScopes::updateLiveScopes(cx))
        return nullptr;
    return GetDebugScope(cx, *fun->environment());
}

JSObject *
js::GetDebugScopeForFrame(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc)
{
    assertSameCompartment(cx, frame);
    if (CanUseDebugScopeMaps(cx) && !DebugScopes::updateLiveScopes(cx))
        return nullptr;
    ScopeIter si(frame, pc, cx);
    return GetDebugScope(cx, si);
}

#ifdef DEBUG

typedef HashSet<PropertyName *> PropertyNameSet;

static bool
RemoveReferencedNames(JSContext *cx, HandleScript script, PropertyNameSet &remainingNames)
{
    // Remove from remainingNames --- the closure variables in some outer
    // script --- any free variables in this script. This analysis isn't perfect:
    //
    // - It will not account for free variables in an inner script which are
    //   actually accessing some name in an intermediate script between the
    //   inner and outer scripts. This can cause remainingNames to be an
    //   underapproximation.
    //
    // - It will not account for new names introduced via eval. This can cause
    //   remainingNames to be an overapproximation. This would be easy to fix
    //   but is nice to have as the eval will probably not access these
    //   these names and putting eval in an inner script is bad news if you
    //   care about entraining variables unnecessarily.

    for (jsbytecode *pc = script->code(); pc != script->codeEnd(); pc += GetBytecodeLength(pc)) {
        PropertyName *name;

        switch (JSOp(*pc)) {
          case JSOP_NAME:
          case JSOP_CALLNAME:
          case JSOP_SETNAME:
            name = script->getName(pc);
            break;

          case JSOP_GETALIASEDVAR:
          case JSOP_CALLALIASEDVAR:
          case JSOP_SETALIASEDVAR:
            name = ScopeCoordinateName(cx->runtime()->scopeCoordinateNameCache, script, pc);
            break;

          default:
            name = nullptr;
            break;
        }

        if (name)
            remainingNames.remove(name);
    }

    if (script->hasObjects()) {
        ObjectArray *objects = script->objects();
        for (size_t i = 0; i < objects->length; i++) {
            JSObject *obj = objects->vector[i];
            if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpreted()) {
                JSFunction *fun = &obj->as<JSFunction>();
                RootedScript innerScript(cx, fun->getOrCreateScript(cx));
                if (!innerScript)
                    return false;

                if (!RemoveReferencedNames(cx, innerScript, remainingNames))
                    return false;
            }
        }
    }

    return true;
}

static bool
AnalyzeEntrainedVariablesInScript(JSContext *cx, HandleScript script, HandleScript innerScript)
{
    PropertyNameSet remainingNames(cx);
    if (!remainingNames.init())
        return false;

    for (BindingIter bi(script); bi; bi++) {
        if (bi->aliased()) {
            PropertyNameSet::AddPtr p = remainingNames.lookupForAdd(bi->name());
            if (!p && !remainingNames.add(p, bi->name()))
                return false;
        }
    }

    if (!RemoveReferencedNames(cx, innerScript, remainingNames))
        return false;

    if (!remainingNames.empty()) {
        Sprinter buf(cx);
        if (!buf.init())
            return false;

        buf.printf("Script ");

        if (JSAtom *name = script->functionNonDelazifying()->displayAtom()) {
            buf.putString(name);
            buf.printf(" ");
        }

        buf.printf("(%s:%d) has variables entrained by ", script->filename(), script->lineno());

        if (JSAtom *name = innerScript->functionNonDelazifying()->displayAtom()) {
            buf.putString(name);
            buf.printf(" ");
        }

        buf.printf("(%s:%d) ::", innerScript->filename(), innerScript->lineno());

        for (PropertyNameSet::Range r = remainingNames.all(); !r.empty(); r.popFront()) {
            buf.printf(" ");
            buf.putString(r.front());
        }

        printf("%s\n", buf.string());
    }

    if (innerScript->hasObjects()) {
        ObjectArray *objects = innerScript->objects();
        for (size_t i = 0; i < objects->length; i++) {
            JSObject *obj = objects->vector[i];
            if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpreted()) {
                JSFunction *fun = &obj->as<JSFunction>();
                RootedScript innerInnerScript(cx, fun->getOrCreateScript(cx));
                if (!innerInnerScript ||
                    !AnalyzeEntrainedVariablesInScript(cx, script, innerInnerScript))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

// Look for local variables in script or any other script inner to it, which are
// part of the script's call object and are unnecessarily entrained by their own
// inner scripts which do not refer to those variables. An example is:
//
// function foo() {
//   var a, b;
//   function bar() { return a; }
//   function baz() { return b; }
// }
//
// |bar| unnecessarily entrains |b|, and |baz| unnecessarily entrains |a|.
bool
js::AnalyzeEntrainedVariables(JSContext *cx, HandleScript script)
{
    if (!script->hasObjects())
        return true;

    ObjectArray *objects = script->objects();
    for (size_t i = 0; i < objects->length; i++) {
        JSObject *obj = objects->vector[i];
        if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpreted()) {
            JSFunction *fun = &obj->as<JSFunction>();
            RootedScript innerScript(cx, fun->getOrCreateScript(cx));
            if (!innerScript)
                return false;

            if (script->functionDelazifying() && script->functionDelazifying()->isHeavyweight()) {
                if (!AnalyzeEntrainedVariablesInScript(cx, script, innerScript))
                    return false;
            }

            if (!AnalyzeEntrainedVariables(cx, innerScript))
                return false;
        }
    }

    return true;
}

#endif
