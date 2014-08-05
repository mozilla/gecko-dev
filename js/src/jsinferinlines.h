/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Inline members for javascript type inference. */

#ifndef jsinferinlines_h
#define jsinferinlines_h

#include "jsinfer.h"

#include "mozilla/PodOperations.h"

#include "builtin/SymbolObject.h"
#include "vm/ArrayObject.h"
#include "vm/BooleanObject.h"
#include "vm/NumberObject.h"
#include "vm/SharedArrayObject.h"
#include "vm/StringObject.h"
#include "vm/TypedArrayObject.h"

#include "jscntxtinlines.h"

#include "jit/ExecutionMode-inl.h"

namespace js {
namespace types {

/////////////////////////////////////////////////////////////////////
// CompilerOutput & RecompileInfo
/////////////////////////////////////////////////////////////////////

inline jit::IonScript *
CompilerOutput::ion() const
{
    // Note: If type constraints are generated before compilation has finished
    // (i.e. after IonBuilder but before CodeGenerator::link) then a valid
    // CompilerOutput may not yet have an associated IonScript.
    JS_ASSERT(isValid());
    jit::IonScript *ion = jit::GetIonScript(script(), mode());
    JS_ASSERT(ion != ION_COMPILING_SCRIPT);
    return ion;
}

inline CompilerOutput*
RecompileInfo::compilerOutput(TypeZone &types) const
{
    if (!types.compilerOutputs || outputIndex >= types.compilerOutputs->length())
        return nullptr;
    return &(*types.compilerOutputs)[outputIndex];
}

inline CompilerOutput*
RecompileInfo::compilerOutput(JSContext *cx) const
{
    return compilerOutput(cx->zone()->types);
}

inline bool
RecompileInfo::shouldSweep(TypeZone &types)
{
    CompilerOutput *output = compilerOutput(types);
    if (!output || !output->isValid())
        return true;

    // Update this info for the output's new index in the zone's compiler outputs.
    outputIndex = output->sweepIndex();
    return false;
}

/////////////////////////////////////////////////////////////////////
// Types
/////////////////////////////////////////////////////////////////////

inline TypeObject *
TypeObjectKey::asTypeObjectNoBarrier()
{
    JS_ASSERT(isTypeObject());
    return (TypeObject *) this;
}

inline JSObject *
TypeObjectKey::asSingleObjectNoBarrier()
{
    JS_ASSERT(isSingleObject());
    return (JSObject *) (uintptr_t(this) & ~1);
}

inline TypeObject *
TypeObjectKey::asTypeObject()
{
    TypeObject *res = asTypeObjectNoBarrier();
    TypeObject::readBarrier(res);
    return res;
}

inline JSObject *
TypeObjectKey::asSingleObject()
{
    JSObject *res = asSingleObjectNoBarrier();
    JSObject::readBarrier(res);
    return res;
}

/* static */ inline Type
Type::ObjectType(JSObject *obj)
{
    if (obj->hasSingletonType())
        return Type(uintptr_t(obj) | 1);
    return Type(uintptr_t(obj->type()));
}

/* static */ inline Type
Type::ObjectType(TypeObject *obj)
{
    if (obj->singleton())
        return Type(uintptr_t(obj->singleton()) | 1);
    return Type(uintptr_t(obj));
}

/* static */ inline Type
Type::ObjectType(TypeObjectKey *obj)
{
    return Type(uintptr_t(obj));
}

inline Type
GetValueType(const Value &val)
{
    if (val.isDouble())
        return Type::DoubleType();
    if (val.isObject())
        return Type::ObjectType(&val.toObject());
    return Type::PrimitiveType(val.extractNonDoubleType());
}

inline Type
GetMaybeOptimizedOutValueType(const Value &val)
{
    if (val.isMagic() && val.whyMagic() == JS_OPTIMIZED_OUT)
        return Type::UnknownType();
    return GetValueType(val);
}

inline TypeFlags
PrimitiveTypeFlag(JSValueType type)
{
    switch (type) {
      case JSVAL_TYPE_UNDEFINED:
        return TYPE_FLAG_UNDEFINED;
      case JSVAL_TYPE_NULL:
        return TYPE_FLAG_NULL;
      case JSVAL_TYPE_BOOLEAN:
        return TYPE_FLAG_BOOLEAN;
      case JSVAL_TYPE_INT32:
        return TYPE_FLAG_INT32;
      case JSVAL_TYPE_DOUBLE:
        return TYPE_FLAG_DOUBLE;
      case JSVAL_TYPE_STRING:
        return TYPE_FLAG_STRING;
      case JSVAL_TYPE_SYMBOL:
        return TYPE_FLAG_SYMBOL;
      case JSVAL_TYPE_MAGIC:
        return TYPE_FLAG_LAZYARGS;
      default:
        MOZ_ASSUME_UNREACHABLE("Bad type");
    }
}

inline JSValueType
TypeFlagPrimitive(TypeFlags flags)
{
    switch (flags) {
      case TYPE_FLAG_UNDEFINED:
        return JSVAL_TYPE_UNDEFINED;
      case TYPE_FLAG_NULL:
        return JSVAL_TYPE_NULL;
      case TYPE_FLAG_BOOLEAN:
        return JSVAL_TYPE_BOOLEAN;
      case TYPE_FLAG_INT32:
        return JSVAL_TYPE_INT32;
      case TYPE_FLAG_DOUBLE:
        return JSVAL_TYPE_DOUBLE;
      case TYPE_FLAG_STRING:
        return JSVAL_TYPE_STRING;
      case TYPE_FLAG_SYMBOL:
        return JSVAL_TYPE_SYMBOL;
      case TYPE_FLAG_LAZYARGS:
        return JSVAL_TYPE_MAGIC;
      default:
        MOZ_ASSUME_UNREACHABLE("Bad type");
    }
}

/*
 * Check for numeric strings, as in js_StringIsIndex, but allow negative
 * and overflowing integers.
 */
template <class Range>
inline bool
IdIsNumericTypeId(Range cp)
{
    if (cp.length() == 0)
        return false;

    if (!JS7_ISDEC(cp[0]) && cp[0] != '-')
        return false;

    for (size_t i = 1; i < cp.length(); ++i) {
        if (!JS7_ISDEC(cp[i]))
            return false;
    }

    return true;
}

/*
 * Get the canonical representation of an id to use when doing inference.  This
 * maintains the constraint that if two different jsids map to the same property
 * in JS (e.g. 3 and "3"), they have the same type representation.
 */
inline jsid
IdToTypeId(jsid id)
{
    JS_ASSERT(!JSID_IS_EMPTY(id));

    /*
     * All integers must map to the aggregate property for index types, including
     * negative integers.
     */
    if (JSID_IS_INT(id))
        return JSID_VOID;

    if (JSID_IS_STRING(id)) {
        JSAtom *atom = JSID_TO_ATOM(id);
        JS::AutoCheckCannotGC nogc;
        bool isNumeric = atom->hasLatin1Chars()
                         ? IdIsNumericTypeId(atom->latin1Range(nogc))
                         : IdIsNumericTypeId(atom->twoByteRange(nogc));
        return isNumeric ? JSID_VOID : id;
    }

    return JSID_VOID;
}

const char * TypeIdStringImpl(jsid id);

/* Convert an id for printing during debug. */
static inline const char *
TypeIdString(jsid id)
{
#ifdef DEBUG
    return TypeIdStringImpl(id);
#else
    return "(missing)";
#endif
}

/*
 * Structure for type inference entry point functions. All functions which can
 * change type information must use this, and functions which depend on
 * intermediate types (i.e. JITs) can use this to ensure that intermediate
 * information is not collected and does not change.
 *
 * Pins inference results so that intermediate type information, TypeObjects
 * and JSScripts won't be collected during GC. Does additional sanity checking
 * that inference is not reentrant and that recompilations occur properly.
 */
struct AutoEnterAnalysis
{
    /* Prevent GC activity in the middle of analysis. */
    gc::AutoSuppressGC suppressGC;

    FreeOp *freeOp;
    JSCompartment *compartment;
    bool oldActiveAnalysis;

    explicit AutoEnterAnalysis(ExclusiveContext *cx)
      : suppressGC(cx)
    {
        init(cx->defaultFreeOp(), cx->compartment());
    }

    AutoEnterAnalysis(FreeOp *fop, JSCompartment *comp)
      : suppressGC(comp)
    {
        init(fop, comp);
    }

    ~AutoEnterAnalysis()
    {
        compartment->activeAnalysis = oldActiveAnalysis;

        /*
         * If there are no more type inference activations on the stack,
         * process any triggered recompilations. Note that we should not be
         * invoking any scripted code while type inference is running.
         */
        if (!compartment->activeAnalysis) {
            TypeZone &types = compartment->zone()->types;
            if (types.pendingRecompiles)
                types.processPendingRecompiles(freeOp);
        }
    }

  private:
    void init(FreeOp *fop, JSCompartment *comp) {
        freeOp = fop;
        compartment = comp;
        oldActiveAnalysis = compartment->activeAnalysis;
        compartment->activeAnalysis = true;
    }
};

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

inline const Class *
GetClassForProtoKey(JSProtoKey key)
{
    switch (key) {
      case JSProto_Object:
        return &JSObject::class_;
      case JSProto_Array:
        return &ArrayObject::class_;

      case JSProto_Number:
        return &NumberObject::class_;
      case JSProto_Boolean:
        return &BooleanObject::class_;
      case JSProto_String:
        return &StringObject::class_;
      case JSProto_Symbol:
        return &SymbolObject::class_;
      case JSProto_RegExp:
        return &RegExpObject::class_;

      case JSProto_Int8Array:
      case JSProto_Uint8Array:
      case JSProto_Int16Array:
      case JSProto_Uint16Array:
      case JSProto_Int32Array:
      case JSProto_Uint32Array:
      case JSProto_Float32Array:
      case JSProto_Float64Array:
      case JSProto_Uint8ClampedArray:
        return &TypedArrayObject::classes[key - JSProto_Int8Array];

      case JSProto_ArrayBuffer:
        return &ArrayBufferObject::class_;

      case JSProto_SharedArrayBuffer:
        return &SharedArrayBufferObject::class_;

      case JSProto_DataView:
        return &DataViewObject::class_;

      default:
        MOZ_ASSUME_UNREACHABLE("Bad proto key");
    }
}

/*
 * Get the default 'new' object for a given standard class, per the currently
 * active global.
 */
inline TypeObject *
GetTypeNewObject(JSContext *cx, JSProtoKey key)
{
    RootedObject proto(cx);
    if (!GetBuiltinPrototype(cx, key, &proto))
        return nullptr;
    return cx->getNewType(GetClassForProtoKey(key), TaggedProto(proto.get()));
}

/* Get a type object for the immediate allocation site within a native. */
inline TypeObject *
GetTypeCallerInitObject(JSContext *cx, JSProtoKey key)
{
    jsbytecode *pc;
    RootedScript script(cx, cx->currentScript(&pc));
    if (script)
        return TypeScript::InitObject(cx, script, pc, key);
    return GetTypeNewObject(cx, key);
}

void MarkIteratorUnknownSlow(JSContext *cx);

void TypeMonitorCallSlow(JSContext *cx, JSObject *callee, const CallArgs &args,
                         bool constructing);

/*
 * Monitor a javascript call, either on entry to the interpreter or made
 * from within the interpreter.
 */
inline void
TypeMonitorCall(JSContext *cx, const js::CallArgs &args, bool constructing)
{
    if (args.callee().is<JSFunction>()) {
        JSFunction *fun = &args.callee().as<JSFunction>();
        if (fun->isInterpreted() && fun->nonLazyScript()->types)
            TypeMonitorCallSlow(cx, &args.callee(), args, constructing);
    }
}

inline bool
TrackPropertyTypes(ExclusiveContext *cx, JSObject *obj, jsid id)
{
    if (obj->hasLazyType() || obj->type()->unknownProperties())
        return false;

    if (obj->hasSingletonType() && !obj->type()->maybeGetProperty(id))
        return false;

    return true;
}

inline void
EnsureTrackPropertyTypes(JSContext *cx, JSObject *obj, jsid id)
{
    id = IdToTypeId(id);

    if (obj->hasSingletonType()) {
        AutoEnterAnalysis enter(cx);
        if (obj->hasLazyType() && !obj->getType(cx)) {
            CrashAtUnhandlableOOM("Could not allocate TypeObject in EnsureTrackPropertyTypes");
            return;
        }
        if (!obj->type()->unknownProperties() && !obj->type()->getProperty(cx, id)) {
            MOZ_ASSERT(obj->type()->unknownProperties());
            return;
        }
    }

    JS_ASSERT(obj->type()->unknownProperties() || TrackPropertyTypes(cx, obj, id));
}

inline bool
CanHaveEmptyPropertyTypesForOwnProperty(JSObject *obj)
{
    // Per the comment on TypeSet::propertySet, property type sets for global
    // objects may be empty for 'own' properties if the global property still
    // has its initial undefined value.
    return obj->is<GlobalObject>();
}

inline bool
HasTypePropertyId(JSObject *obj, jsid id, Type type)
{
    if (obj->hasLazyType())
        return true;

    if (obj->type()->unknownProperties())
        return true;

    if (HeapTypeSet *types = obj->type()->maybeGetProperty(IdToTypeId(id)))
        return types->hasType(type);

    return false;
}

inline bool
HasTypePropertyId(JSObject *obj, jsid id, const Value &value)
{
    return HasTypePropertyId(obj, id, GetValueType(value));
}

/* Add a possible type for a property of obj. */
inline void
AddTypePropertyId(ExclusiveContext *cx, JSObject *obj, jsid id, Type type)
{
    id = IdToTypeId(id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, type);
}

inline void
AddTypePropertyId(ExclusiveContext *cx, JSObject *obj, jsid id, const Value &value)
{
    id = IdToTypeId(id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, value);
}

inline void
AddTypePropertyId(ExclusiveContext *cx, TypeObject *obj, jsid id, Type type)
{
    if (!obj->unknownProperties())
        obj->addPropertyType(cx, id, type);
}

inline void
AddTypePropertyId(ExclusiveContext *cx, TypeObject *obj, jsid id, const Value &value)
{
    if (!obj->unknownProperties())
        obj->addPropertyType(cx, id, value);
}

/* Set one or more dynamic flags on a type object. */
inline void
MarkTypeObjectFlags(ExclusiveContext *cx, JSObject *obj, TypeObjectFlags flags)
{
    if (!obj->hasLazyType() && !obj->type()->hasAllFlags(flags))
        obj->type()->setFlags(cx, flags);
}

/*
 * Mark all properties of a type object as unknown. If markSetsUnknown is set,
 * scan the entire compartment and mark all type sets containing it as having
 * an unknown object. This is needed for correctness in dealing with mutable
 * __proto__, which can change the type of an object dynamically.
 */
inline void
MarkTypeObjectUnknownProperties(JSContext *cx, TypeObject *obj,
                                bool markSetsUnknown = false)
{
    if (!obj->unknownProperties())
        obj->markUnknown(cx);
    if (markSetsUnknown && !(obj->flags() & OBJECT_FLAG_SETS_MARKED_UNKNOWN))
        cx->compartment()->types.markSetsUnknown(cx, obj);
}

inline void
MarkTypePropertyNonData(ExclusiveContext *cx, JSObject *obj, jsid id)
{
    id = IdToTypeId(id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->markPropertyNonData(cx, id);
}

inline void
MarkTypePropertyNonWritable(ExclusiveContext *cx, JSObject *obj, jsid id)
{
    id = IdToTypeId(id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->markPropertyNonWritable(cx, id);
}

inline bool
IsTypePropertyIdMarkedNonData(JSObject *obj, jsid id)
{
    return obj->type()->isPropertyNonData(id);
}

inline bool
IsTypePropertyIdMarkedNonWritable(JSObject *obj, jsid id)
{
    return obj->type()->isPropertyNonWritable(id);
}

/* Mark a state change on a particular object. */
inline void
MarkObjectStateChange(ExclusiveContext *cx, JSObject *obj)
{
    if (!obj->hasLazyType() && !obj->type()->unknownProperties())
        obj->type()->markStateChange(cx);
}

/*
 * For an array or object which has not yet escaped and been referenced elsewhere,
 * pick a new type based on the object's current contents.
 */

inline void
FixArrayType(ExclusiveContext *cx, HandleObject obj)
{
    cx->compartment()->types.fixArrayType(cx, obj);
}

inline void
FixObjectType(ExclusiveContext *cx, HandleObject obj)
{
    cx->compartment()->types.fixObjectType(cx, obj);
}

/* Interface helpers for JSScript*. */
extern void TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc,
                              const js::Value &rval);
extern void TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc,
                              js::types::Type type);

/////////////////////////////////////////////////////////////////////
// Script interface functions
/////////////////////////////////////////////////////////////////////

/* static */ inline unsigned
TypeScript::NumTypeSets(JSScript *script)
{
    size_t num = script->nTypeSets() + 1 /* this */;
    if (JSFunction *fun = script->functionNonDelazifying())
        num += fun->nargs();
    return num;
}

/* static */ inline StackTypeSet *
TypeScript::ThisTypes(JSScript *script)
{
    return script->types->typeArray() + script->nTypeSets();
}

/*
 * Note: for non-escaping arguments, argTypes reflect only the initial type of
 * the variable (e.g. passed values for argTypes, or undefined for localTypes)
 * and not types from subsequent assignments.
 */

/* static */ inline StackTypeSet *
TypeScript::ArgTypes(JSScript *script, unsigned i)
{
    JS_ASSERT(i < script->functionNonDelazifying()->nargs());
    return script->types->typeArray() + script->nTypeSets() + 1 + i;
}

template <typename TYPESET>
/* static */ inline TYPESET *
TypeScript::BytecodeTypes(JSScript *script, jsbytecode *pc, uint32_t *bytecodeMap,
                          uint32_t *hint, TYPESET *typeArray)
{
    JS_ASSERT(js_CodeSpec[*pc].format & JOF_TYPESET);
    uint32_t offset = script->pcToOffset(pc);

    // See if this pc is the next typeset opcode after the last one looked up.
    if ((*hint + 1) < script->nTypeSets() && bytecodeMap[*hint + 1] == offset) {
        (*hint)++;
        return typeArray + *hint;
    }

    // See if this pc is the same as the last one looked up.
    if (bytecodeMap[*hint] == offset)
        return typeArray + *hint;

    // Fall back to a binary search.
    size_t bottom = 0;
    size_t top = script->nTypeSets() - 1;
    size_t mid = bottom + (top - bottom) / 2;
    while (mid < top) {
        if (bytecodeMap[mid] < offset)
            bottom = mid + 1;
        else if (bytecodeMap[mid] > offset)
            top = mid;
        else
            break;
        mid = bottom + (top - bottom) / 2;
    }

    // We should have have zeroed in on either the exact offset, unless there
    // are more JOF_TYPESET opcodes than nTypeSets in the script (as can happen
    // if the script is very long).
    JS_ASSERT(bytecodeMap[mid] == offset || mid == top);

    *hint = mid;
    return typeArray + *hint;
}

/* static */ inline StackTypeSet *
TypeScript::BytecodeTypes(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(CurrentThreadCanAccessRuntime(script->runtimeFromMainThread()));
    uint32_t *hint = script->baselineScript()->bytecodeTypeMap() + script->nTypeSets();
    return BytecodeTypes(script, pc, script->baselineScript()->bytecodeTypeMap(),
                         hint, script->types->typeArray());
}

struct AllocationSiteKey : public DefaultHasher<AllocationSiteKey> {
    JSScript *script;

    uint32_t offset : 24;
    JSProtoKey kind : 8;

    static const uint32_t OFFSET_LIMIT = (1 << 23);

    AllocationSiteKey() { mozilla::PodZero(this); }

    static inline uint32_t hash(AllocationSiteKey key) {
        return uint32_t(size_t(key.script->offsetToPC(key.offset)) ^ key.kind);
    }

    static inline bool match(const AllocationSiteKey &a, const AllocationSiteKey &b) {
        return a.script == b.script && a.offset == b.offset && a.kind == b.kind;
    }
};

/* Whether to use a new type object for an initializer opcode at script/pc. */
js::NewObjectKind
UseNewTypeForInitializer(JSScript *script, jsbytecode *pc, JSProtoKey key);

js::NewObjectKind
UseNewTypeForInitializer(JSScript *script, jsbytecode *pc, const Class *clasp);

/* static */ inline TypeObject *
TypeScript::InitObject(JSContext *cx, JSScript *script, jsbytecode *pc, JSProtoKey kind)
{
    JS_ASSERT(!UseNewTypeForInitializer(script, pc, kind));

    /* :XXX: Limit script->length so we don't need to check the offset up front? */
    uint32_t offset = script->pcToOffset(pc);

    if (!script->compileAndGo() || offset >= AllocationSiteKey::OFFSET_LIMIT)
        return GetTypeNewObject(cx, kind);

    AllocationSiteKey key;
    key.script = script;
    key.offset = offset;
    key.kind = kind;

    if (!cx->compartment()->types.allocationSiteTable)
        return cx->compartment()->types.addAllocationSiteTypeObject(cx, key);

    AllocationSiteTable::Ptr p = cx->compartment()->types.allocationSiteTable->lookup(key);

    if (p)
        return p->value();
    return cx->compartment()->types.addAllocationSiteTypeObject(cx, key);
}

/* Set the type to use for obj according to the site it was allocated at. */
static inline bool
SetInitializerObjectType(JSContext *cx, HandleScript script, jsbytecode *pc, HandleObject obj, NewObjectKind kind)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(obj->getClass());
    JS_ASSERT(key != JSProto_Null);
    JS_ASSERT(kind == UseNewTypeForInitializer(script, pc, key));

    if (kind == SingletonObject) {
        JS_ASSERT(obj->hasSingletonType());

        /*
         * Inference does not account for types of run-once initializer
         * objects, as these may not be created until after the script
         * has been analyzed.
         */
        TypeScript::Monitor(cx, script, pc, ObjectValue(*obj));
    } else {
        types::TypeObject *type = TypeScript::InitObject(cx, script, pc, key);
        if (!type)
            return false;
        obj->uninlinedSetType(type);
    }

    return true;
}

/* static */ inline void
TypeScript::Monitor(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    TypeMonitorResult(cx, script, pc, rval);
}

/* static */ inline void
TypeScript::Monitor(JSContext *cx, const js::Value &rval)
{
    jsbytecode *pc;
    RootedScript script(cx, cx->currentScript(&pc));
    Monitor(cx, script, pc, rval);
}

/* static */ inline void
TypeScript::MonitorAssign(JSContext *cx, HandleObject obj, jsid id)
{
    if (!obj->hasSingletonType()) {
        /*
         * Mark as unknown any object which has had dynamic assignments to
         * non-integer properties at SETELEM opcodes. This avoids making large
         * numbers of type properties for hashmap-style objects. We don't need
         * to do this for objects with singleton type, because type properties
         * are only constructed for them when analyzed scripts depend on those
         * specific properties.
         */
        uint32_t i;
        if (js_IdIsIndex(id, &i))
            return;

        // But if we don't have too many properties yet, don't do anything.  The
        // idea here is that normal object initialization should not trigger
        // deoptimization in most cases, while actual usage as a hashmap should.
        TypeObject* type = obj->type();
        if (type->getPropertyCount() < 128)
            return;
        MarkTypeObjectUnknownProperties(cx, type);
    }
}

/* static */ inline void
TypeScript::SetThis(JSContext *cx, JSScript *script, Type type)
{
    if (!script->types)
        return;

    if (!ThisTypes(script)->hasType(type)) {
        AutoEnterAnalysis enter(cx);

        InferSpew(ISpewOps, "externalType: setThis #%u: %s",
                  script->id(), TypeString(type));
        ThisTypes(script)->addType(cx, type);
    }
}

/* static */ inline void
TypeScript::SetThis(JSContext *cx, JSScript *script, const js::Value &value)
{
    SetThis(cx, script, GetValueType(value));
}

/* static */ inline void
TypeScript::SetArgument(JSContext *cx, JSScript *script, unsigned arg, Type type)
{
    if (!script->types)
        return;

    if (!ArgTypes(script, arg)->hasType(type)) {
        AutoEnterAnalysis enter(cx);

        InferSpew(ISpewOps, "externalType: setArg #%u %u: %s",
                  script->id(), arg, TypeString(type));
        ArgTypes(script, arg)->addType(cx, type);
    }
}

/* static */ inline void
TypeScript::SetArgument(JSContext *cx, JSScript *script, unsigned arg, const js::Value &value)
{
    Type type = GetValueType(value);
    SetArgument(cx, script, arg, type);
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

inline JSCompartment *
TypeCompartment::compartment()
{
    return (JSCompartment *)((char *)this - offsetof(JSCompartment, types));
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

/*
 * The sets of objects and scripts in a type set grow monotonically, are usually
 * empty, almost always small, and sometimes big.  For empty or singleton sets,
 * the pointer refers directly to the value.  For sets fitting into SET_ARRAY_SIZE,
 * an array of this length is used to store the elements.  For larger sets, a hash
 * table filled to 25%-50% of capacity is used, with collisions resolved by linear
 * probing.  TODO: replace these with jshashtables.
 */
const unsigned SET_ARRAY_SIZE = 8;
const unsigned SET_CAPACITY_OVERFLOW = 1u << 30;

/* Get the capacity of a set with the given element count. */
static inline unsigned
HashSetCapacity(unsigned count)
{
    JS_ASSERT(count >= 2);
    JS_ASSERT(count < SET_CAPACITY_OVERFLOW);

    if (count <= SET_ARRAY_SIZE)
        return SET_ARRAY_SIZE;

    return 1u << (mozilla::FloorLog2(count) + 2);
}

/* Compute the FNV hash for the low 32 bits of v. */
template <class T, class KEY>
static inline uint32_t
HashKey(T v)
{
    uint32_t nv = KEY::keyBits(v);

    uint32_t hash = 84696351 ^ (nv & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 8) & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 16) & 0xff);
    return (hash * 16777619) ^ ((nv >> 24) & 0xff);
}

/*
 * Insert space for an element into the specified set and grow its capacity if needed.
 * returned value is an existing or new entry (nullptr if new).
 */
template <class T, class U, class KEY>
static U **
HashSetInsertTry(LifoAlloc &alloc, U **&values, unsigned &count, T key)
{
    unsigned capacity = HashSetCapacity(count);
    unsigned insertpos = HashKey<T,KEY>(key) & (capacity - 1);

    /* Whether we are converting from a fixed array to hashtable. */
    bool converting = (count == SET_ARRAY_SIZE);

    if (!converting) {
        while (values[insertpos] != nullptr) {
            if (KEY::getKey(values[insertpos]) == key)
                return &values[insertpos];
            insertpos = (insertpos + 1) & (capacity - 1);
        }
    }

    if (count >= SET_CAPACITY_OVERFLOW)
        return nullptr;

    count++;
    unsigned newCapacity = HashSetCapacity(count);

    if (newCapacity == capacity) {
        JS_ASSERT(!converting);
        return &values[insertpos];
    }

    U **newValues = alloc.newArray<U*>(newCapacity);
    if (!newValues)
        return nullptr;
    mozilla::PodZero(newValues, newCapacity);

    for (unsigned i = 0; i < capacity; i++) {
        if (values[i]) {
            unsigned pos = HashKey<T,KEY>(KEY::getKey(values[i])) & (newCapacity - 1);
            while (newValues[pos] != nullptr)
                pos = (pos + 1) & (newCapacity - 1);
            newValues[pos] = values[i];
        }
    }

    values = newValues;

    insertpos = HashKey<T,KEY>(key) & (newCapacity - 1);
    while (values[insertpos] != nullptr)
        insertpos = (insertpos + 1) & (newCapacity - 1);
    return &values[insertpos];
}

/*
 * Insert an element into the specified set if it is not already there, returning
 * an entry which is nullptr if the element was not there.
 */
template <class T, class U, class KEY>
static inline U **
HashSetInsert(LifoAlloc &alloc, U **&values, unsigned &count, T key)
{
    if (count == 0) {
        JS_ASSERT(values == nullptr);
        count++;
        return (U **) &values;
    }

    if (count == 1) {
        U *oldData = (U*) values;
        if (KEY::getKey(oldData) == key)
            return (U **) &values;

        values = alloc.newArray<U*>(SET_ARRAY_SIZE);
        if (!values) {
            values = (U **) oldData;
            return nullptr;
        }
        mozilla::PodZero(values, SET_ARRAY_SIZE);
        count++;

        values[0] = oldData;
        return &values[1];
    }

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return &values[i];
        }

        if (count < SET_ARRAY_SIZE) {
            count++;
            return &values[count - 1];
        }
    }

    return HashSetInsertTry<T,U,KEY>(alloc, values, count, key);
}

/* Lookup an entry in a hash set, return nullptr if it does not exist. */
template <class T, class U, class KEY>
static inline U *
HashSetLookup(U **values, unsigned count, T key)
{
    if (count == 0)
        return nullptr;

    if (count == 1)
        return (KEY::getKey((U *) values) == key) ? (U *) values : nullptr;

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return values[i];
        }
        return nullptr;
    }

    unsigned capacity = HashSetCapacity(count);
    unsigned pos = HashKey<T,KEY>(key) & (capacity - 1);

    while (values[pos] != nullptr) {
        if (KEY::getKey(values[pos]) == key)
            return values[pos];
        pos = (pos + 1) & (capacity - 1);
    }

    return nullptr;
}

inline TypeObjectKey *
Type::objectKey() const
{
    JS_ASSERT(isObject());
    return (TypeObjectKey *) data;
}

inline JSObject *
Type::singleObject() const
{
    return objectKey()->asSingleObject();
}

inline TypeObject *
Type::typeObject() const
{
    return objectKey()->asTypeObject();
}

inline JSObject *
Type::singleObjectNoBarrier() const
{
    return objectKey()->asSingleObjectNoBarrier();
}

inline TypeObject *
Type::typeObjectNoBarrier() const
{
    return objectKey()->asTypeObjectNoBarrier();
}

inline bool
TypeSet::hasType(Type type) const
{
    if (unknown())
        return true;

    if (type.isUnknown()) {
        return false;
    } else if (type.isPrimitive()) {
        return !!(flags & PrimitiveTypeFlag(type.primitive()));
    } else if (type.isAnyObject()) {
        return !!(flags & TYPE_FLAG_ANYOBJECT);
    } else {
        return !!(flags & TYPE_FLAG_ANYOBJECT) ||
            HashSetLookup<TypeObjectKey*,TypeObjectKey,TypeObjectKey>
            (objectSet, baseObjectCount(), type.objectKey()) != nullptr;
    }
}

inline void
TypeSet::setBaseObjectCount(uint32_t count)
{
    JS_ASSERT(count <= TYPE_FLAG_OBJECT_COUNT_LIMIT);
    flags = (flags & ~TYPE_FLAG_OBJECT_COUNT_MASK)
          | (count << TYPE_FLAG_OBJECT_COUNT_SHIFT);
}

inline void
HeapTypeSet::newPropertyState(ExclusiveContext *cxArg)
{
    /* Propagate the change to all constraints. */
    if (JSContext *cx = cxArg->maybeJSContext()) {
        TypeConstraint *constraint = constraintList;
        while (constraint) {
            constraint->newPropertyState(cx, this);
            constraint = constraint->next;
        }
    } else {
        JS_ASSERT(!constraintList);
    }
}

inline void
HeapTypeSet::setNonDataPropertyIgnoringConstraints()
{
    flags |= TYPE_FLAG_NON_DATA_PROPERTY;
}

inline void
HeapTypeSet::setNonDataProperty(ExclusiveContext *cx)
{
    if (flags & TYPE_FLAG_NON_DATA_PROPERTY)
        return;

    setNonDataPropertyIgnoringConstraints();
    newPropertyState(cx);
}

inline void
HeapTypeSet::setNonWritableProperty(ExclusiveContext *cx)
{
    if (flags & TYPE_FLAG_NON_WRITABLE_PROPERTY)
        return;

    flags |= TYPE_FLAG_NON_WRITABLE_PROPERTY;
    newPropertyState(cx);
}

inline unsigned
TypeSet::getObjectCount() const
{
    JS_ASSERT(!unknownObject());
    uint32_t count = baseObjectCount();
    if (count > SET_ARRAY_SIZE)
        return HashSetCapacity(count);
    return count;
}

inline TypeObjectKey *
TypeSet::getObject(unsigned i) const
{
    JS_ASSERT(i < getObjectCount());
    if (baseObjectCount() == 1) {
        JS_ASSERT(i == 0);
        return (TypeObjectKey *) objectSet;
    }
    return objectSet[i];
}

inline JSObject *
TypeSet::getSingleObject(unsigned i) const
{
    TypeObjectKey *key = getObject(i);
    return (key && key->isSingleObject()) ? key->asSingleObject() : nullptr;
}

inline TypeObject *
TypeSet::getTypeObject(unsigned i) const
{
    TypeObjectKey *key = getObject(i);
    return (key && key->isTypeObject()) ? key->asTypeObject() : nullptr;
}

inline JSObject *
TypeSet::getSingleObjectNoBarrier(unsigned i) const
{
    TypeObjectKey *key = getObject(i);
    return (key && key->isSingleObject()) ? key->asSingleObjectNoBarrier() : nullptr;
}

inline TypeObject *
TypeSet::getTypeObjectNoBarrier(unsigned i) const
{
    TypeObjectKey *key = getObject(i);
    return (key && key->isTypeObject()) ? key->asTypeObjectNoBarrier() : nullptr;
}

inline const Class *
TypeSet::getObjectClass(unsigned i) const
{
    if (JSObject *object = getSingleObject(i))
        return object->getClass();
    if (TypeObject *object = getTypeObject(i))
        return object->clasp();
    return nullptr;
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline TypeObject::TypeObject(const Class *clasp, TaggedProto proto, TypeObjectFlags initialFlags)
{
    mozilla::PodZero(this);

    /* Inner objects may not appear on prototype chains. */
    JS_ASSERT_IF(proto.isObject(), !proto.toObject()->getClass()->ext.outerObject);

    this->clasp_ = clasp;
    this->proto_ = proto.raw();
    this->flags_ = initialFlags;

    InferSpew(ISpewOps, "newObject: %s", TypeObjectString(this));
}

inline uint32_t
TypeObject::basePropertyCount() const
{
    return (flags() & OBJECT_FLAG_PROPERTY_COUNT_MASK) >> OBJECT_FLAG_PROPERTY_COUNT_SHIFT;
}

inline void
TypeObject::setBasePropertyCount(uint32_t count)
{
    // Note: Callers must ensure they are performing threadsafe operations.
    JS_ASSERT(count <= OBJECT_FLAG_PROPERTY_COUNT_LIMIT);
    flags_ = (flags() & ~OBJECT_FLAG_PROPERTY_COUNT_MASK)
           | (count << OBJECT_FLAG_PROPERTY_COUNT_SHIFT);
}

inline HeapTypeSet *
TypeObject::getProperty(ExclusiveContext *cx, jsid id)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);

    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == IdToTypeId(id));
    JS_ASSERT(!unknownProperties());

    if (HeapTypeSet *types = maybeGetProperty(id))
        return types;

    Property *base = cx->typeLifoAlloc().new_<Property>(id);
    if (!base) {
        markUnknown(cx);
        return nullptr;
    }

    uint32_t propertyCount = basePropertyCount();
    Property **pprop = HashSetInsert<jsid,Property,Property>
        (cx->typeLifoAlloc(), propertySet, propertyCount, id);
    if (!pprop) {
        markUnknown(cx);
        return nullptr;
    }

    JS_ASSERT(!*pprop);

    setBasePropertyCount(propertyCount);
    *pprop = base;

    updateNewPropertyTypes(cx, id, &base->types);

    if (propertyCount == OBJECT_FLAG_PROPERTY_COUNT_LIMIT) {
        // We hit the maximum number of properties the object can have, mark
        // the object unknown so that new properties will not be added in the
        // future.
        markUnknown(cx);
    }

    return &base->types;
}

inline HeapTypeSet *
TypeObject::maybeGetProperty(jsid id)
{
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == IdToTypeId(id));
    JS_ASSERT(!unknownProperties());

    Property *prop = HashSetLookup<jsid,Property,Property>
        (propertySet, basePropertyCount(), id);

    return prop ? &prop->types : nullptr;
}

inline unsigned
TypeObject::getPropertyCount()
{
    uint32_t count = basePropertyCount();
    if (count > SET_ARRAY_SIZE)
        return HashSetCapacity(count);
    return count;
}

inline Property *
TypeObject::getProperty(unsigned i)
{
    JS_ASSERT(i < getPropertyCount());
    if (basePropertyCount() == 1) {
        JS_ASSERT(i == 0);
        return (Property *) propertySet;
    }
    return propertySet[i];
}

inline void
TypeObjectAddendum::writeBarrierPre(TypeObjectAddendum *type)
{
#ifdef JSGC_INCREMENTAL
    if (!type)
        return;

    switch (type->kind) {
      case NewScript:
        return TypeNewScript::writeBarrierPre(type->asNewScript());
    }
#endif
}

inline void
TypeNewScript::writeBarrierPre(TypeNewScript *newScript)
{
#ifdef JSGC_INCREMENTAL
    if (!newScript || !newScript->fun->runtimeFromAnyThread()->needsIncrementalBarrier())
        return;

    JS::Zone *zone = newScript->fun->zoneFromAnyThread();
    if (zone->needsIncrementalBarrier()) {
        MarkObject(zone->barrierTracer(), &newScript->fun, "write barrier");
        MarkObject(zone->barrierTracer(), &newScript->templateObject, "write barrier");
    }
#endif
}

} } /* namespace js::types */

inline bool
JSScript::ensureHasTypes(JSContext *cx)
{
    return types || makeTypes(cx);
}

namespace js {

template <>
struct GCMethods<const types::Type>
{
    static types::Type initial() { return types::Type::UnknownType(); }
    static bool poisoned(const types::Type &v) {
        return (v.isTypeObject() && IsPoisonedPtr(v.typeObject()))
            || (v.isSingleObject() && IsPoisonedPtr(v.singleObject()));
    }
};

template <>
struct GCMethods<types::Type>
{
    static types::Type initial() { return types::Type::UnknownType(); }
    static bool poisoned(const types::Type &v) {
        return (v.isTypeObject() && IsPoisonedPtr(v.typeObject()))
            || (v.isSingleObject() && IsPoisonedPtr(v.singleObject()));
    }
};

} // namespace js

namespace JS {
template<> class AnchorPermitted<js::types::TypeObject *> { };
}  // namespace JS

#endif /* jsinferinlines_h */
