/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsfriendapi_h
#define jsfriendapi_h

#include "mozilla/Casting.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TypedEnum.h"

#include "jsbytecode.h"
#include "jspubtd.h"

#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"

/*
 * This macro checks if the stack pointer has exceeded a given limit. If
 * |tolerance| is non-zero, it returns true only if the stack pointer has
 * exceeded the limit by more than |tolerance| bytes.
 */
#if JS_STACK_GROWTH_DIRECTION > 0
# define JS_CHECK_STACK_SIZE_WITH_TOLERANCE(limit, sp, tolerance)  \
    ((uintptr_t)(sp) < (limit)+(tolerance))
#else
# define JS_CHECK_STACK_SIZE_WITH_TOLERANCE(limit, sp, tolerance)  \
    ((uintptr_t)(sp) > (limit)-(tolerance))
#endif

#define JS_CHECK_STACK_SIZE(limit, lval) JS_CHECK_STACK_SIZE_WITH_TOLERANCE(limit, lval, 0)

class JSAtom;
struct JSErrorFormatString;
class JSLinearString;
struct JSJitInfo;
class JSErrorReport;

namespace JS {
template <class T>
class Heap;
} /* namespace JS */

extern JS_FRIEND_API(void)
JS_SetGrayGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data);

extern JS_FRIEND_API(JSString *)
JS_GetAnonymousString(JSRuntime *rt);

extern JS_FRIEND_API(JSObject *)
JS_FindCompilationScope(JSContext *cx, JS::HandleObject obj);

extern JS_FRIEND_API(JSFunction *)
JS_GetObjectFunction(JSObject *obj);

extern JS_FRIEND_API(bool)
JS_SplicePrototype(JSContext *cx, JS::HandleObject obj, JS::HandleObject proto);

extern JS_FRIEND_API(JSObject *)
JS_NewObjectWithUniqueType(JSContext *cx, const JSClass *clasp, JS::HandleObject proto,
                           JS::HandleObject parent);

extern JS_FRIEND_API(uint32_t)
JS_ObjectCountDynamicSlots(JS::HandleObject obj);

extern JS_FRIEND_API(size_t)
JS_SetProtoCalled(JSContext *cx);

extern JS_FRIEND_API(size_t)
JS_GetCustomIteratorCount(JSContext *cx);

extern JS_FRIEND_API(bool)
JS_NondeterministicGetWeakMapKeys(JSContext *cx, JS::HandleObject obj, JS::MutableHandleObject ret);

/*
 * Determine whether the given object is backed by a DeadObjectProxy.
 *
 * Such objects hold no other objects (they have no outgoing reference edges)
 * and will throw if you touch them (e.g. by reading/writing a property).
 */
extern JS_FRIEND_API(bool)
JS_IsDeadWrapper(JSObject *obj);

/*
 * Used by the cycle collector to trace through the shape and all
 * shapes it reaches, marking all non-shape children found in the
 * process. Uses bounded stack space.
 */
extern JS_FRIEND_API(void)
JS_TraceShapeCycleCollectorChildren(JSTracer *trc, void *shape);

enum {
    JS_TELEMETRY_GC_REASON,
    JS_TELEMETRY_GC_IS_COMPARTMENTAL,
    JS_TELEMETRY_GC_MS,
    JS_TELEMETRY_GC_MAX_PAUSE_MS,
    JS_TELEMETRY_GC_MARK_MS,
    JS_TELEMETRY_GC_SWEEP_MS,
    JS_TELEMETRY_GC_MARK_ROOTS_MS,
    JS_TELEMETRY_GC_MARK_GRAY_MS,
    JS_TELEMETRY_GC_SLICE_MS,
    JS_TELEMETRY_GC_MMU_50,
    JS_TELEMETRY_GC_RESET,
    JS_TELEMETRY_GC_INCREMENTAL_DISABLED,
    JS_TELEMETRY_GC_NON_INCREMENTAL,
    JS_TELEMETRY_GC_SCC_SWEEP_TOTAL_MS,
    JS_TELEMETRY_GC_SCC_SWEEP_MAX_PAUSE_MS
};

typedef void
(* JSAccumulateTelemetryDataCallback)(int id, uint32_t sample);

extern JS_FRIEND_API(void)
JS_SetAccumulateTelemetryCallback(JSRuntime *rt, JSAccumulateTelemetryDataCallback callback);

extern JS_FRIEND_API(JSPrincipals *)
JS_GetCompartmentPrincipals(JSCompartment *compartment);

extern JS_FRIEND_API(void)
JS_SetCompartmentPrincipals(JSCompartment *compartment, JSPrincipals *principals);

extern JS_FRIEND_API(JSPrincipals *)
JS_GetScriptPrincipals(JSScript *script);

extern JS_FRIEND_API(JSPrincipals *)
JS_GetScriptOriginPrincipals(JSScript *script);

/* Safe to call with input obj == nullptr. Returns non-nullptr iff obj != nullptr. */
extern JS_FRIEND_API(JSObject *)
JS_ObjectToInnerObject(JSContext *cx, JS::HandleObject obj);

/* Requires obj != nullptr. */
extern JS_FRIEND_API(JSObject *)
JS_ObjectToOuterObject(JSContext *cx, JS::HandleObject obj);

extern JS_FRIEND_API(JSObject *)
JS_CloneObject(JSContext *cx, JS::HandleObject obj, JS::HandleObject proto,
               JS::HandleObject parent);

extern JS_FRIEND_API(JSString *)
JS_BasicObjectToString(JSContext *cx, JS::HandleObject obj);

extern JS_FRIEND_API(bool)
js_GetterOnlyPropertyStub(JSContext *cx, JS::HandleObject obj, JS::HandleId id, bool strict,
                          JS::MutableHandleValue vp);

JS_FRIEND_API(void)
js_ReportOverRecursed(JSContext *maybecx);

JS_FRIEND_API(bool)
js_ObjectClassIs(JSContext *cx, JS::HandleObject obj, js::ESClassValue classValue);

JS_FRIEND_API(const char *)
js_ObjectClassName(JSContext *cx, JS::HandleObject obj);

namespace js {

JS_FRIEND_API(bool)
AddRawValueRoot(JSContext *cx, JS::Value *vp, const char *name);

JS_FRIEND_API(void)
RemoveRawValueRoot(JSContext *cx, JS::Value *vp);

} /* namespace js */

#ifdef JS_DEBUG

/*
 * Routines to print out values during debugging.  These are FRIEND_API to help
 * the debugger find them and to support temporarily hacking js_Dump* calls
 * into other code.
 */

extern JS_FRIEND_API(void)
js_DumpString(JSString *str);

extern JS_FRIEND_API(void)
js_DumpAtom(JSAtom *atom);

extern JS_FRIEND_API(void)
js_DumpObject(JSObject *obj);

extern JS_FRIEND_API(void)
js_DumpChars(const jschar *s, size_t n);
#endif

/*
 * Copies all own properties from |obj| to |target|. |obj| must be a "native"
 * object (that is to say, normal-ish - not an Array or a Proxy).
 *
 * This function immediately enters a compartment, and does not impose any
 * restrictions on the compartment of |cx|.
 */
extern JS_FRIEND_API(bool)
JS_CopyPropertiesFrom(JSContext *cx, JS::HandleObject target, JS::HandleObject obj);

/*
 * Single-property version of the above. This function asserts that an |own|
 * property of the given name exists on |obj|.
 *
 * On entry, |cx| must be same-compartment with |obj|.
 */
extern JS_FRIEND_API(bool)
JS_CopyPropertyFrom(JSContext *cx, JS::HandleId id, JS::HandleObject target,
                    JS::HandleObject obj);

extern JS_FRIEND_API(bool)
JS_WrapPropertyDescriptor(JSContext *cx, JS::MutableHandle<JSPropertyDescriptor> desc);

extern JS_FRIEND_API(bool)
JS_WrapAutoIdVector(JSContext *cx, JS::AutoIdVector &props);

extern JS_FRIEND_API(bool)
JS_EnumerateState(JSContext *cx, JS::HandleObject obj, JSIterateOp enum_op,
                  JS::MutableHandleValue statep, JS::MutableHandleId idp);

struct JSFunctionSpecWithHelp {
    const char      *name;
    JSNative        call;
    uint16_t        nargs;
    uint16_t        flags;
    const char      *usage;
    const char      *help;
};

#define JS_FN_HELP(name,call,nargs,flags,usage,help)                          \
    {name, call, nargs, (flags) | JSPROP_ENUMERATE | JSFUN_STUB_GSOPS, usage, help}
#define JS_FS_HELP_END                                                        \
    {nullptr, nullptr, 0, 0, nullptr, nullptr}

extern JS_FRIEND_API(bool)
JS_DefineFunctionsWithHelp(JSContext *cx, JS::HandleObject obj, const JSFunctionSpecWithHelp *fs);

namespace js {

/*
 * Helper Macros for creating JSClasses that function as proxies.
 *
 * NB: The macro invocation must be surrounded by braces, so as to
 *     allow for potention JSClass extensions.
 */
#define PROXY_MAKE_EXT(outerObject, innerObject, iteratorObject,        \
                       isWrappedNative)                                 \
    {                                                                   \
        outerObject,                                                    \
        innerObject,                                                    \
        iteratorObject,                                                 \
        isWrappedNative,                                                \
        js::proxy_WeakmapKeyDelegate                                    \
    }

#define PROXY_CLASS_WITH_EXT(name, extraSlots, flags, callOp, constructOp, ext)         \
    {                                                                                   \
        name,                                                                           \
        js::Class::NON_NATIVE |                                                         \
            JSCLASS_IS_PROXY |                                                          \
            JSCLASS_IMPLEMENTS_BARRIERS |                                               \
            JSCLASS_HAS_RESERVED_SLOTS(js::PROXY_MINIMUM_SLOTS + (extraSlots)) |        \
            flags,                                                                      \
        JS_PropertyStub,         /* addProperty */                                      \
        JS_DeletePropertyStub,   /* delProperty */                                      \
        JS_PropertyStub,         /* getProperty */                                      \
        JS_StrictPropertyStub,   /* setProperty */                                      \
        JS_EnumerateStub,                                                               \
        JS_ResolveStub,                                                                 \
        js::proxy_Convert,                                                              \
        js::proxy_Finalize,      /* finalize    */                                      \
        callOp,                  /* call        */                                      \
        js::proxy_HasInstance,   /* hasInstance */                                      \
        constructOp,             /* construct   */                                      \
        js::proxy_Trace,         /* trace       */                                      \
        JS_NULL_CLASS_SPEC,                                                             \
        ext,                                                                            \
        {                                                                               \
            js::proxy_LookupGeneric,                                                    \
            js::proxy_LookupProperty,                                                   \
            js::proxy_LookupElement,                                                    \
            js::proxy_DefineGeneric,                                                    \
            js::proxy_DefineProperty,                                                   \
            js::proxy_DefineElement,                                                    \
            js::proxy_GetGeneric,                                                       \
            js::proxy_GetProperty,                                                      \
            js::proxy_GetElement,                                                       \
            js::proxy_SetGeneric,                                                       \
            js::proxy_SetProperty,                                                      \
            js::proxy_SetElement,                                                       \
            js::proxy_GetGenericAttributes,                                             \
            js::proxy_SetGenericAttributes,                                             \
            js::proxy_DeleteGeneric,                                                    \
            js::proxy_Watch, js::proxy_Unwatch,                                         \
            js::proxy_Slice,                                                            \
            nullptr,             /* enumerate       */                                  \
            nullptr,             /* thisObject      */                                  \
        }                                                                               \
    }

#define PROXY_CLASS_DEF(name, extraSlots, flags, callOp, constructOp)   \
  PROXY_CLASS_WITH_EXT(name, extraSlots, flags, callOp, constructOp,    \
                       PROXY_MAKE_EXT(                                  \
                         nullptr, /* outerObject */                     \
                         nullptr, /* innerObject */                     \
                         nullptr, /* iteratorObject */                  \
                         false    /* isWrappedNative */                 \
                       ))

/*
 * Proxy stubs, similar to JS_*Stub, for embedder proxy class definitions.
 *
 * NB: Should not be called directly.
 */

extern JS_FRIEND_API(bool)
proxy_LookupGeneric(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::MutableHandleObject objp,
                    JS::MutableHandle<Shape*> propp);
extern JS_FRIEND_API(bool)
proxy_LookupProperty(JSContext *cx, JS::HandleObject obj, JS::Handle<PropertyName*> name,
                     JS::MutableHandleObject objp, JS::MutableHandle<Shape*> propp);
extern JS_FRIEND_API(bool)
proxy_LookupElement(JSContext *cx, JS::HandleObject obj, uint32_t index, JS::MutableHandleObject objp,
                    JS::MutableHandle<Shape*> propp);
extern JS_FRIEND_API(bool)
proxy_DefineGeneric(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::HandleValue value,
                    JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs);
extern JS_FRIEND_API(bool)
proxy_DefineProperty(JSContext *cx, JS::HandleObject obj, JS::Handle<PropertyName*> name,
                     JS::HandleValue value, JSPropertyOp getter, JSStrictPropertyOp setter,
                     unsigned attrs);
extern JS_FRIEND_API(bool)
proxy_DefineElement(JSContext *cx, JS::HandleObject obj, uint32_t index, JS::HandleValue value,
                    JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs);
extern JS_FRIEND_API(bool)
proxy_GetGeneric(JSContext *cx, JS::HandleObject obj, JS::HandleObject receiver, JS::HandleId id,
                 JS::MutableHandleValue vp);
extern JS_FRIEND_API(bool)
proxy_GetProperty(JSContext *cx, JS::HandleObject obj, JS::HandleObject receiver,
                  JS::Handle<PropertyName*> name, JS::MutableHandleValue vp);
extern JS_FRIEND_API(bool)
proxy_GetElement(JSContext *cx, JS::HandleObject obj, JS::HandleObject receiver, uint32_t index,
                 JS::MutableHandleValue vp);
extern JS_FRIEND_API(bool)
proxy_SetGeneric(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                 JS::MutableHandleValue bp, bool strict);
extern JS_FRIEND_API(bool)
proxy_SetProperty(JSContext *cx, JS::HandleObject obj, JS::Handle<PropertyName*> name,
                  JS::MutableHandleValue bp, bool strict);
extern JS_FRIEND_API(bool)
proxy_SetElement(JSContext *cx, JS::HandleObject obj, uint32_t index, JS::MutableHandleValue vp,
                 bool strict);
extern JS_FRIEND_API(bool)
proxy_GetGenericAttributes(JSContext *cx, JS::HandleObject obj, JS::HandleId id, unsigned *attrsp);
extern JS_FRIEND_API(bool)
proxy_SetGenericAttributes(JSContext *cx, JS::HandleObject obj, JS::HandleId id, unsigned *attrsp);
extern JS_FRIEND_API(bool)
proxy_DeleteGeneric(JSContext *cx, JS::HandleObject obj, JS::HandleId id, bool *succeeded);

extern JS_FRIEND_API(void)
proxy_Trace(JSTracer *trc, JSObject *obj);
extern JS_FRIEND_API(JSObject *)
proxy_WeakmapKeyDelegate(JSObject *obj);
extern JS_FRIEND_API(bool)
proxy_Convert(JSContext *cx, JS::HandleObject proxy, JSType hint, JS::MutableHandleValue vp);
extern JS_FRIEND_API(void)
proxy_Finalize(FreeOp *fop, JSObject *obj);
extern JS_FRIEND_API(bool)
proxy_HasInstance(JSContext *cx, JS::HandleObject proxy, JS::MutableHandleValue v, bool *bp);
extern JS_FRIEND_API(bool)
proxy_Call(JSContext *cx, unsigned argc, JS::Value *vp);
extern JS_FRIEND_API(bool)
proxy_Construct(JSContext *cx, unsigned argc, JS::Value *vp);
extern JS_FRIEND_API(JSObject *)
proxy_innerObject(JSObject *obj);
extern JS_FRIEND_API(bool)
proxy_Watch(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::HandleObject callable);
extern JS_FRIEND_API(bool)
proxy_Unwatch(JSContext *cx, JS::HandleObject obj, JS::HandleId id);
extern JS_FRIEND_API(bool)
proxy_Slice(JSContext *cx, JS::HandleObject proxy, uint32_t begin, uint32_t end,
            JS::HandleObject result);

/*
 * A class of objects that return source code on demand.
 *
 * When code is compiled with setSourceIsLazy(true), SpiderMonkey doesn't
 * retain the source code (and doesn't do lazy bytecode generation). If we ever
 * need the source code, say, in response to a call to Function.prototype.
 * toSource or Debugger.Source.prototype.text, then we call the 'load' member
 * function of the instance of this class that has hopefully been registered
 * with the runtime, passing the code's URL, and hope that it will be able to
 * find the source.
 */
class SourceHook {
  public:
    virtual ~SourceHook() { }

    /*
     * Set |*src| and |*length| to refer to the source code for |filename|.
     * On success, the caller owns the buffer to which |*src| points, and
     * should use JS_free to free it.
     */
    virtual bool load(JSContext *cx, const char *filename, jschar **src, size_t *length) = 0;
};

/*
 * Have |rt| use |hook| to retrieve lazily-retrieved source code. See the
 * comments for SourceHook. The runtime takes ownership of the hook, and
 * will delete it when the runtime itself is deleted, or when a new hook is
 * set.
 */
extern JS_FRIEND_API(void)
SetSourceHook(JSRuntime *rt, SourceHook *hook);

/* Remove |rt|'s source hook, and return it. The caller now owns the hook. */
extern JS_FRIEND_API(SourceHook *)
ForgetSourceHook(JSRuntime *rt);

extern JS_FRIEND_API(JS::Zone *)
GetCompartmentZone(JSCompartment *comp);

typedef bool
(* PreserveWrapperCallback)(JSContext *cx, JSObject *obj);

typedef enum  {
    CollectNurseryBeforeDump,
    IgnoreNurseryObjects
} DumpHeapNurseryBehaviour;

 /*
  * Dump the complete object graph of heap-allocated things.
  * fp is the file for the dump output.
  */
extern JS_FRIEND_API(void)
DumpHeapComplete(JSRuntime *rt, FILE *fp, DumpHeapNurseryBehaviour nurseryBehaviour);

#ifdef JS_OLD_GETTER_SETTER_METHODS
JS_FRIEND_API(bool) obj_defineGetter(JSContext *cx, unsigned argc, JS::Value *vp);
JS_FRIEND_API(bool) obj_defineSetter(JSContext *cx, unsigned argc, JS::Value *vp);
#endif

extern JS_FRIEND_API(bool)
IsSystemCompartment(JSCompartment *comp);

extern JS_FRIEND_API(bool)
IsSystemZone(JS::Zone *zone);

extern JS_FRIEND_API(bool)
IsAtomsCompartment(JSCompartment *comp);

/*
 * Check whether it is OK to assign an undeclared variable with the name
 * |propname| at the current location in script.  It is not an error if there is
 * no current script location, or if that location is not an assignment to an
 * undeclared variable.  Reports an error if one needs to be reported (and,
 * particularly, always reports when it returns false).
 */
extern JS_FRIEND_API(bool)
ReportIfUndeclaredVarAssignment(JSContext *cx, JS::HandleString propname);

/*
 * Returns whether we're in a non-strict property set (in that we're in a
 * non-strict script and the bytecode we're on is a property set).  The return
 * value does NOT indicate any sort of exception was thrown: it's just a
 * boolean.
 */
extern JS_FRIEND_API(bool)
IsInNonStrictPropertySet(JSContext *cx);

struct WeakMapTracer;

/*
 * Weak map tracer callback, called once for every binding of every
 * weak map that was live at the time of the last garbage collection.
 *
 * m will be nullptr if the weak map is not contained in a JS Object.
 */
typedef void
(* WeakMapTraceCallback)(WeakMapTracer *trc, JSObject *m,
                         void *k, JSGCTraceKind kkind,
                         void *v, JSGCTraceKind vkind);

struct WeakMapTracer {
    JSRuntime            *runtime;
    WeakMapTraceCallback callback;

    WeakMapTracer(JSRuntime *rt, WeakMapTraceCallback cb)
        : runtime(rt), callback(cb) {}
};

extern JS_FRIEND_API(void)
TraceWeakMaps(WeakMapTracer *trc);

extern JS_FRIEND_API(bool)
AreGCGrayBitsValid(JSRuntime *rt);

extern JS_FRIEND_API(bool)
ZoneGlobalsAreAllGray(JS::Zone *zone);

typedef void
(*GCThingCallback)(void *closure, void *gcthing);

extern JS_FRIEND_API(void)
VisitGrayWrapperTargets(JS::Zone *zone, GCThingCallback callback, void *closure);

extern JS_FRIEND_API(JSObject *)
GetWeakmapKeyDelegate(JSObject *key);

JS_FRIEND_API(JSGCTraceKind)
GCThingTraceKind(void *thing);

/*
 * Invoke cellCallback on every gray JS_OBJECT in the given zone.
 */
extern JS_FRIEND_API(void)
IterateGrayObjects(JS::Zone *zone, GCThingCallback cellCallback, void *data);

#ifdef JS_HAS_CTYPES
extern JS_FRIEND_API(size_t)
SizeOfDataIfCDataObject(mozilla::MallocSizeOf mallocSizeOf, JSObject *obj);
#endif

extern JS_FRIEND_API(JSCompartment *)
GetAnyCompartmentInZone(JS::Zone *zone);

/*
 * Shadow declarations of JS internal structures, for access by inline access
 * functions below. Do not use these structures in any other way. When adding
 * new fields for access by inline methods, make sure to add static asserts to
 * the original header file to ensure that offsets are consistent.
 */
namespace shadow {

struct TypeObject {
    const Class *clasp;
    JSObject    *proto;
};

struct BaseShape {
    const js::Class *clasp_;
    JSObject *parent;
    JSObject *_1;
    JSCompartment *compartment;
};

class Shape {
public:
    shadow::BaseShape *base;
    jsid              _1;
    uint32_t          slotInfo;

    static const uint32_t FIXED_SLOTS_SHIFT = 27;
};

struct Object {
    shadow::Shape      *shape;
    shadow::TypeObject *type;
    JS::Value          *slots;
    JS::Value          *_1;

    size_t numFixedSlots() const { return shape->slotInfo >> Shape::FIXED_SLOTS_SHIFT; }
    JS::Value *fixedSlots() const {
        return (JS::Value *)(uintptr_t(this) + sizeof(shadow::Object));
    }

    JS::Value &slotRef(size_t slot) const {
        size_t nfixed = numFixedSlots();
        if (slot < nfixed)
            return fixedSlots()[slot];
        return slots[slot - nfixed];
    }

    // Reserved slots with index < MAX_FIXED_SLOTS are guaranteed to
    // be fixed slots.
    static const uint32_t MAX_FIXED_SLOTS = 16;
};

struct Function {
    Object base;
    uint16_t nargs;
    uint16_t flags;
    /* Used only for natives */
    JSNative native;
    const JSJitInfo *jitinfo;
    void *_1;
};

struct Atom {
    static const uint32_t INLINE_CHARS_BIT = JS_BIT(2);
    static const uint32_t LATIN1_CHARS_BIT = JS_BIT(6);
    uint32_t flags;
    uint32_t length;
    union {
        const JS::Latin1Char *nonInlineCharsLatin1;
        const jschar *nonInlineCharsTwoByte;
        JS::Latin1Char inlineStorageLatin1[1];
        jschar inlineStorageTwoByte[1];
    };
};

} /* namespace shadow */

// This is equal to |&JSObject::class_|.  Use it in places where you don't want
// to #include jsobj.h.
extern JS_FRIEND_DATA(const js::Class* const) ObjectClassPtr;

inline const js::Class *
GetObjectClass(JSObject *obj)
{
    return reinterpret_cast<const shadow::Object*>(obj)->type->clasp;
}

inline const JSClass *
GetObjectJSClass(JSObject *obj)
{
    return js::Jsvalify(GetObjectClass(obj));
}

inline bool
IsInnerObject(JSObject *obj) {
    return !!GetObjectClass(obj)->ext.outerObject;
}

inline bool
IsOuterObject(JSObject *obj) {
    return !!GetObjectClass(obj)->ext.innerObject;
}

JS_FRIEND_API(bool)
IsFunctionObject(JSObject *obj);

JS_FRIEND_API(bool)
IsScopeObject(JSObject *obj);

JS_FRIEND_API(bool)
IsCallObject(JSObject *obj);

inline JSObject *
GetObjectParent(JSObject *obj)
{
    JS_ASSERT(!IsScopeObject(obj));
    return reinterpret_cast<shadow::Object*>(obj)->shape->base->parent;
}

static MOZ_ALWAYS_INLINE JSCompartment *
GetObjectCompartment(JSObject *obj)
{
    return reinterpret_cast<shadow::Object*>(obj)->shape->base->compartment;
}

JS_FRIEND_API(JSObject *)
GetObjectParentMaybeScope(JSObject *obj);

JS_FRIEND_API(JSObject *)
GetGlobalForObjectCrossCompartment(JSObject *obj);

// Sidestep the activeContext checking implicitly performed in
// JS_SetPendingException.
JS_FRIEND_API(void)
SetPendingExceptionCrossContext(JSContext *cx, JS::HandleValue v);

JS_FRIEND_API(void)
AssertSameCompartment(JSContext *cx, JSObject *obj);

#ifdef JS_DEBUG
JS_FRIEND_API(void)
AssertSameCompartment(JSObject *objA, JSObject *objB);
#else
inline void AssertSameCompartment(JSObject *objA, JSObject *objB) {}
#endif

// For legacy consumers only. This whole concept is going away soon.
JS_FRIEND_API(JSObject *)
DefaultObjectForContextOrNull(JSContext *cx);

JS_FRIEND_API(void)
SetDefaultObjectForContext(JSContext *cx, JSObject *obj);

JS_FRIEND_API(void)
NotifyAnimationActivity(JSObject *obj);

/*
 * Return the outermost enclosing function (script) of the scripted caller.
 * This function returns nullptr in several cases:
 *  - no script is running on the context
 *  - the caller is in global or eval code
 * In particular, this function will "stop" its outermost search at eval() and
 * thus it will really return the outermost enclosing function *since the
 * innermost eval*.
 */
JS_FRIEND_API(JSScript *)
GetOutermostEnclosingFunctionOfScriptedCaller(JSContext *cx);

JS_FRIEND_API(JSFunction *)
DefineFunctionWithReserved(JSContext *cx, JSObject *obj, const char *name, JSNative call,
                           unsigned nargs, unsigned attrs);

JS_FRIEND_API(JSFunction *)
NewFunctionWithReserved(JSContext *cx, JSNative call, unsigned nargs, unsigned flags,
                        JSObject *parent, const char *name);

JS_FRIEND_API(JSFunction *)
NewFunctionByIdWithReserved(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
                            JSObject *parent, jsid id);

JS_FRIEND_API(JSObject *)
InitClassWithReserved(JSContext *cx, JSObject *obj, JSObject *parent_proto,
                      const JSClass *clasp, JSNative constructor, unsigned nargs,
                      const JSPropertySpec *ps, const JSFunctionSpec *fs,
                      const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs);

JS_FRIEND_API(const JS::Value &)
GetFunctionNativeReserved(JSObject *fun, size_t which);

JS_FRIEND_API(void)
SetFunctionNativeReserved(JSObject *fun, size_t which, const JS::Value &val);

JS_FRIEND_API(bool)
GetObjectProto(JSContext *cx, JS::HandleObject obj, JS::MutableHandleObject proto);

JS_FRIEND_API(bool)
GetOriginalEval(JSContext *cx, JS::HandleObject scope,
                JS::MutableHandleObject eval);

inline void *
GetObjectPrivate(JSObject *obj)
{
    const shadow::Object *nobj = reinterpret_cast<const shadow::Object*>(obj);
    void **addr = reinterpret_cast<void**>(&nobj->fixedSlots()[nobj->numFixedSlots()]);
    return *addr;
}

/*
 * Get a slot that is both reserved for object's clasp *and* is fixed (fits
 * within the maximum capacity for the object's fixed slots).
 */
inline const JS::Value &
GetReservedSlot(JSObject *obj, size_t slot)
{
    JS_ASSERT(slot < JSCLASS_RESERVED_SLOTS(GetObjectClass(obj)));
    return reinterpret_cast<const shadow::Object *>(obj)->slotRef(slot);
}

JS_FRIEND_API(void)
SetReservedSlotWithBarrier(JSObject *obj, size_t slot, const JS::Value &value);

inline void
SetReservedSlot(JSObject *obj, size_t slot, const JS::Value &value)
{
    JS_ASSERT(slot < JSCLASS_RESERVED_SLOTS(GetObjectClass(obj)));
    shadow::Object *sobj = reinterpret_cast<shadow::Object *>(obj);
    if (sobj->slotRef(slot).isMarkable()
#ifdef JSGC_GENERATIONAL
        || value.isMarkable()
#endif
       )
    {
        SetReservedSlotWithBarrier(obj, slot, value);
    } else {
        sobj->slotRef(slot) = value;
    }
}

JS_FRIEND_API(uint32_t)
GetObjectSlotSpan(JSObject *obj);

inline const JS::Value &
GetObjectSlot(JSObject *obj, size_t slot)
{
    JS_ASSERT(slot < GetObjectSlotSpan(obj));
    return reinterpret_cast<const shadow::Object *>(obj)->slotRef(slot);
}

inline const jschar *
GetAtomChars(JSAtom *atom)
{
    using shadow::Atom;
    Atom *atom_ = reinterpret_cast<Atom *>(atom);
    JS_ASSERT(!(atom_->flags & Atom::LATIN1_CHARS_BIT));
    if (atom_->flags & Atom::INLINE_CHARS_BIT) {
        char *p = reinterpret_cast<char *>(atom);
        return reinterpret_cast<const jschar *>(p + offsetof(Atom, inlineStorageTwoByte));
    }
    return atom_->nonInlineCharsTwoByte;
}

inline size_t
GetAtomLength(JSAtom *atom)
{
    return reinterpret_cast<shadow::Atom*>(atom)->length;
}

inline JSLinearString *
AtomToLinearString(JSAtom *atom)
{
    return reinterpret_cast<JSLinearString *>(atom);
}

JS_FRIEND_API(bool)
GetPropertyNames(JSContext *cx, JSObject *obj, unsigned flags, JS::AutoIdVector *props);

JS_FRIEND_API(bool)
AppendUnique(JSContext *cx, JS::AutoIdVector &base, JS::AutoIdVector &others);

JS_FRIEND_API(bool)
GetGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, JS::Value *vp);

JS_FRIEND_API(bool)
StringIsArrayIndex(JSLinearString *str, uint32_t *indexp);

JS_FRIEND_API(void)
SetPreserveWrapperCallback(JSRuntime *rt, PreserveWrapperCallback callback);

JS_FRIEND_API(bool)
IsObjectInContextCompartment(JSObject *obj, const JSContext *cx);

/*
 * NB: these flag bits are encoded into the bytecode stream in the immediate
 * operand of JSOP_ITER, so don't change them without advancing vm/Xdr.h's
 * XDR_BYTECODE_VERSION.
 */
#define JSITER_ENUMERATE  0x1   /* for-in compatible hidden default iterator */
#define JSITER_FOREACH    0x2   /* return [key, value] pair rather than key */
#define JSITER_KEYVALUE   0x4   /* destructuring for-in wants [key, value] */
#define JSITER_OWNONLY    0x8   /* iterate over obj's own properties only */
#define JSITER_HIDDEN     0x10  /* also enumerate non-enumerable properties */

JS_FRIEND_API(bool)
RunningWithTrustedPrincipals(JSContext *cx);

inline uintptr_t
GetNativeStackLimit(JSContext *cx)
{
    StackKind kind = RunningWithTrustedPrincipals(cx) ? StackForTrustedScript
                                                      : StackForUntrustedScript;
    PerThreadDataFriendFields *mainThread =
      PerThreadDataFriendFields::getMainThread(GetRuntime(cx));
    return mainThread->nativeStackLimit[kind];
}

/*
 * These macros report a stack overflow and run |onerror| if we are close to
 * using up the C stack. The JS_CHECK_CHROME_RECURSION variant gives us a little
 * extra space so that we can ensure that crucial code is able to run.
 */

#define JS_CHECK_RECURSION(cx, onerror)                                         \
    JS_BEGIN_MACRO                                                              \
        int stackDummy_;                                                        \
        if (!JS_CHECK_STACK_SIZE(js::GetNativeStackLimit(cx), &stackDummy_)) {  \
            js_ReportOverRecursed(cx);                                          \
            onerror;                                                            \
        }                                                                       \
    JS_END_MACRO

#define JS_CHECK_RECURSION_DONT_REPORT(cx, onerror)                             \
    JS_BEGIN_MACRO                                                              \
        int stackDummy_;                                                        \
        if (!JS_CHECK_STACK_SIZE(js::GetNativeStackLimit(cx), &stackDummy_)) {  \
            onerror;                                                            \
        }                                                                       \
    JS_END_MACRO

#define JS_CHECK_RECURSION_WITH_SP_DONT_REPORT(cx, sp, onerror)                 \
    JS_BEGIN_MACRO                                                              \
        if (!JS_CHECK_STACK_SIZE(js::GetNativeStackLimit(cx), sp)) {            \
            onerror;                                                            \
        }                                                                       \
    JS_END_MACRO

#define JS_CHECK_RECURSION_WITH_SP(cx, sp, onerror)                             \
    JS_BEGIN_MACRO                                                              \
        if (!JS_CHECK_STACK_SIZE(js::GetNativeStackLimit(cx), sp)) {            \
            js_ReportOverRecursed(cx);                                          \
            onerror;                                                            \
        }                                                                       \
    JS_END_MACRO

#define JS_CHECK_CHROME_RECURSION(cx, onerror)                                  \
    JS_BEGIN_MACRO                                                              \
        int stackDummy_;                                                        \
        if (!JS_CHECK_STACK_SIZE_WITH_TOLERANCE(js::GetNativeStackLimit(cx),    \
                                                &stackDummy_,                   \
                                                1024 * sizeof(size_t)))         \
        {                                                                       \
            js_ReportOverRecursed(cx);                                          \
            onerror;                                                            \
        }                                                                       \
    JS_END_MACRO

JS_FRIEND_API(void)
StartPCCountProfiling(JSContext *cx);

JS_FRIEND_API(void)
StopPCCountProfiling(JSContext *cx);

JS_FRIEND_API(void)
PurgePCCounts(JSContext *cx);

JS_FRIEND_API(size_t)
GetPCCountScriptCount(JSContext *cx);

JS_FRIEND_API(JSString *)
GetPCCountScriptSummary(JSContext *cx, size_t script);

JS_FRIEND_API(JSString *)
GetPCCountScriptContents(JSContext *cx, size_t script);

#ifdef JS_THREADSAFE
JS_FRIEND_API(bool)
ContextHasOutstandingRequests(const JSContext *cx);
#endif

typedef void
(* ActivityCallback)(void *arg, bool active);

/*
 * Sets a callback that is run whenever the runtime goes idle - the
 * last active request ceases - and begins activity - when it was
 * idle and a request begins.
 */
JS_FRIEND_API(void)
SetActivityCallback(JSRuntime *rt, ActivityCallback cb, void *arg);

extern JS_FRIEND_API(const JSStructuredCloneCallbacks *)
GetContextStructuredCloneCallbacks(JSContext *cx);

extern JS_FRIEND_API(bool)
IsContextRunningJS(JSContext *cx);

typedef bool
(* DOMInstanceClassHasProtoAtDepth)(const Class *instanceClass,
                                    uint32_t protoID, uint32_t depth);
struct JSDOMCallbacks {
    DOMInstanceClassHasProtoAtDepth instanceClassMatchesProto;
};
typedef struct JSDOMCallbacks DOMCallbacks;

extern JS_FRIEND_API(void)
SetDOMCallbacks(JSRuntime *rt, const DOMCallbacks *callbacks);

extern JS_FRIEND_API(const DOMCallbacks *)
GetDOMCallbacks(JSRuntime *rt);

extern JS_FRIEND_API(JSObject *)
GetTestingFunctions(JSContext *cx);

/*
 * Helper to convert FreeOp to JSFreeOp when the definition of FreeOp is not
 * available and the compiler does not know that FreeOp inherits from
 * JSFreeOp.
 */
inline JSFreeOp *
CastToJSFreeOp(FreeOp *fop)
{
    return reinterpret_cast<JSFreeOp *>(fop);
}

/* Implemented in jsexn.cpp. */

/*
 * Get an error type name from a JSExnType constant.
 * Returns nullptr for invalid arguments and JSEXN_INTERNALERR
 */
extern JS_FRIEND_API(const jschar*)
GetErrorTypeName(JSRuntime* rt, int16_t exnType);

#ifdef JS_DEBUG
extern JS_FRIEND_API(unsigned)
GetEnterCompartmentDepth(JSContext* cx);
#endif

/* Implemented in jswrapper.cpp. */
typedef enum NukeReferencesToWindow {
    NukeWindowReferences,
    DontNukeWindowReferences
} NukeReferencesToWindow;

/*
 * These filters are designed to be ephemeral stack classes, and thus don't
 * do any rooting or holding of their members.
 */
struct CompartmentFilter {
    virtual bool match(JSCompartment *c) const = 0;
};

struct AllCompartments : public CompartmentFilter {
    virtual bool match(JSCompartment *c) const { return true; }
};

struct ContentCompartmentsOnly : public CompartmentFilter {
    virtual bool match(JSCompartment *c) const {
        return !IsSystemCompartment(c);
    }
};

struct ChromeCompartmentsOnly : public CompartmentFilter {
    virtual bool match(JSCompartment *c) const {
        return IsSystemCompartment(c);
    }
};

struct SingleCompartment : public CompartmentFilter {
    JSCompartment *ours;
    explicit SingleCompartment(JSCompartment *c) : ours(c) {}
    virtual bool match(JSCompartment *c) const { return c == ours; }
};

struct CompartmentsWithPrincipals : public CompartmentFilter {
    JSPrincipals *principals;
    explicit CompartmentsWithPrincipals(JSPrincipals *p) : principals(p) {}
    virtual bool match(JSCompartment *c) const {
        return JS_GetCompartmentPrincipals(c) == principals;
    }
};

extern JS_FRIEND_API(bool)
NukeCrossCompartmentWrappers(JSContext* cx,
                             const CompartmentFilter& sourceFilter,
                             const CompartmentFilter& targetFilter,
                             NukeReferencesToWindow nukeReferencesToWindow);

/* Specify information about DOMProxy proxies in the DOM, for use by ICs. */

/*
 * The DOMProxyShadowsCheck function will be called to check if the property for
 * id should be gotten from the prototype, or if there is an own property that
 * shadows it.
 * If DoesntShadow is returned then the slot at listBaseExpandoSlot should
 * either be undefined or point to an expando object that would contain the own
 * property.
 * If DoesntShadowUnique is returned then the slot at listBaseExpandoSlot should
 * contain a private pointer to a ExpandoAndGeneration, which contains a
 * JS::Value that should either be undefined or point to an expando object, and
 * a uint32 value. If that value changes then the IC for getting a property will
 * be invalidated.
 */

struct ExpandoAndGeneration {
  ExpandoAndGeneration()
    : expando(JS::UndefinedValue()),
      generation(0)
  {}

  void Unlink()
  {
      ++generation;
      expando.setUndefined();
  }

  static size_t offsetOfExpando()
  {
      return offsetof(ExpandoAndGeneration, expando);
  }

  static size_t offsetOfGeneration()
  {
      return offsetof(ExpandoAndGeneration, generation);
  }

  JS::Heap<JS::Value> expando;
  uint32_t generation;
};

typedef enum DOMProxyShadowsResult {
  ShadowCheckFailed,
  Shadows,
  DoesntShadow,
  DoesntShadowUnique
} DOMProxyShadowsResult;
typedef DOMProxyShadowsResult
(* DOMProxyShadowsCheck)(JSContext* cx, JS::HandleObject object, JS::HandleId id);
JS_FRIEND_API(void)
SetDOMProxyInformation(const void *domProxyHandlerFamily, uint32_t domProxyExpandoSlot,
                       DOMProxyShadowsCheck domProxyShadowsCheck);

const void *GetDOMProxyHandlerFamily();
uint32_t GetDOMProxyExpandoSlot();
DOMProxyShadowsCheck GetDOMProxyShadowsCheck();

} /* namespace js */

/* Implemented in jsdate.cpp. */

/*
 * Detect whether the internal date value is NaN.  (Because failure is
 * out-of-band for js_DateGet*)
 */
extern JS_FRIEND_API(bool)
js_DateIsValid(JSObject* obj);

extern JS_FRIEND_API(double)
js_DateGetMsecSinceEpoch(JSObject *obj);

/* Implemented in jscntxt.cpp. */

/*
 * Report an exception, which is currently realized as a printf-style format
 * string and its arguments.
 */
typedef enum JSErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "js.msg"
#undef MSG_DEF
    JSErr_Limit
} JSErrNum;

extern JS_FRIEND_API(const JSErrorFormatString *)
js_GetErrorMessage(void *userRef, const char *locale, const unsigned errorNumber);

namespace js {

// Creates a string of the form |ErrorType: ErrorMessage| for a JSErrorReport,
// which generally matches the toString() behavior of an ErrorObject.
extern JS_FRIEND_API(JSString *)
ErrorReportToString(JSContext *cx, JSErrorReport *reportp);

} /* namespace js */


/* Implemented in jsclone.cpp. */

extern JS_FRIEND_API(uint64_t)
js_GetSCOffset(JSStructuredCloneWriter* writer);

/* Typed Array functions, implemented in jstypedarray.cpp */

namespace js {
namespace ArrayBufferView {

enum ViewType {
    TYPE_INT8 = 0,
    TYPE_UINT8,
    TYPE_INT16,
    TYPE_UINT16,
    TYPE_INT32,
    TYPE_UINT32,
    TYPE_FLOAT32,
    TYPE_FLOAT64,

    /*
     * Special type that is a uint8_t, but assignments are clamped to [0, 256).
     * Treat the raw data type as a uint8_t.
     */
    TYPE_UINT8_CLAMPED,

    /*
     * Type returned for a DataView. Note that there is no single element type
     * in this case.
     */
    TYPE_DATAVIEW,

    TYPE_MAX
};

} /* namespace ArrayBufferView */

} /* namespace js */

typedef js::ArrayBufferView::ViewType JSArrayBufferViewType;

/*
 * Create a new typed array with nelements elements.
 *
 * These functions (except the WithBuffer variants) fill in the array with zeros.
 */

extern JS_FRIEND_API(JSObject *)
JS_NewInt8Array(JSContext *cx, uint32_t nelements);
extern JS_FRIEND_API(JSObject *)
JS_NewUint8Array(JSContext *cx, uint32_t nelements);
extern JS_FRIEND_API(JSObject *)
JS_NewUint8ClampedArray(JSContext *cx, uint32_t nelements);

extern JS_FRIEND_API(JSObject *)
JS_NewInt16Array(JSContext *cx, uint32_t nelements);
extern JS_FRIEND_API(JSObject *)
JS_NewUint16Array(JSContext *cx, uint32_t nelements);
extern JS_FRIEND_API(JSObject *)
JS_NewInt32Array(JSContext *cx, uint32_t nelements);
extern JS_FRIEND_API(JSObject *)
JS_NewUint32Array(JSContext *cx, uint32_t nelements);
extern JS_FRIEND_API(JSObject *)
JS_NewFloat32Array(JSContext *cx, uint32_t nelements);
extern JS_FRIEND_API(JSObject *)
JS_NewFloat64Array(JSContext *cx, uint32_t nelements);

/*
 * Create a new typed array and copy in values from the given object. The
 * object is used as if it were an array; that is, the new array (if
 * successfully created) will have length given by array.length, and its
 * elements will be those specified by array[0], array[1], and so on, after
 * conversion to the typed array element type.
 */

extern JS_FRIEND_API(JSObject *)
JS_NewInt8ArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewUint8ArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewUint8ClampedArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewInt16ArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewUint16ArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewInt32ArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewUint32ArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewFloat32ArrayFromArray(JSContext *cx, JS::HandleObject array);
extern JS_FRIEND_API(JSObject *)
JS_NewFloat64ArrayFromArray(JSContext *cx, JS::HandleObject array);

/*
 * Create a new typed array using the given ArrayBuffer for storage.  The
 * length value is optional; if -1 is passed, enough elements to use up the
 * remainder of the byte array is used as the default value.
 */

extern JS_FRIEND_API(JSObject *)
JS_NewInt8ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                          uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewUint8ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                           uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewUint8ClampedArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                                  uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewInt16ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                           uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewUint16ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                            uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewInt32ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                           uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewUint32ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                            uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewFloat32ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                             uint32_t byteOffset, int32_t length);
extern JS_FRIEND_API(JSObject *)
JS_NewFloat64ArrayWithBuffer(JSContext *cx, JS::HandleObject arrayBuffer,
                             uint32_t byteOffset, int32_t length);

/*
 * Create a new ArrayBuffer with the given byte length.
 */
extern JS_FRIEND_API(JSObject *)
JS_NewArrayBuffer(JSContext *cx, uint32_t nbytes);

/*
 * Check whether obj supports JS_GetTypedArray* APIs. Note that this may return
 * false if a security wrapper is encountered that denies the unwrapping. If
 * this test or one of the JS_Is*Array tests succeeds, then it is safe to call
 * the various accessor JSAPI calls defined below.
 */
extern JS_FRIEND_API(bool)
JS_IsTypedArrayObject(JSObject *obj);

/*
 * Check whether obj supports JS_GetArrayBufferView* APIs. Note that this may
 * return false if a security wrapper is encountered that denies the
 * unwrapping. If this test or one of the more specific tests succeeds, then it
 * is safe to call the various ArrayBufferView accessor JSAPI calls defined
 * below.
 */
extern JS_FRIEND_API(bool)
JS_IsArrayBufferViewObject(JSObject *obj);

/*
 * Test for specific typed array types (ArrayBufferView subtypes)
 */

extern JS_FRIEND_API(bool)
JS_IsInt8Array(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsUint8Array(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsUint8ClampedArray(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsInt16Array(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsUint16Array(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsInt32Array(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsUint32Array(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsFloat32Array(JSObject *obj);
extern JS_FRIEND_API(bool)
JS_IsFloat64Array(JSObject *obj);

/*
 * Test for specific typed array types (ArrayBufferView subtypes) and return
 * the unwrapped object if so, else nullptr.  Never throws.
 */

namespace js {

extern JS_FRIEND_API(JSObject *)
UnwrapInt8Array(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapUint8Array(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapUint8ClampedArray(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapInt16Array(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapUint16Array(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapInt32Array(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapUint32Array(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapFloat32Array(JSObject *obj);
extern JS_FRIEND_API(JSObject *)
UnwrapFloat64Array(JSObject *obj);

extern JS_FRIEND_API(JSObject *)
UnwrapArrayBuffer(JSObject *obj);

extern JS_FRIEND_API(JSObject *)
UnwrapArrayBufferView(JSObject *obj);

namespace detail {

extern JS_FRIEND_DATA(const Class* const) Int8ArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Uint8ArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Uint8ClampedArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Int16ArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Uint16ArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Int32ArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Uint32ArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Float32ArrayClassPtr;
extern JS_FRIEND_DATA(const Class* const) Float64ArrayClassPtr;

const size_t TypedArrayLengthSlot = 1;

} // namespace detail

/*
 * Test for specific typed array types (ArrayBufferView subtypes) and return
 * the unwrapped object if so, else nullptr.  Never throws.
 */

#define JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Type, type) \
inline void \
Get ## Type ## ArrayLengthAndData(JSObject *obj, uint32_t *length, type **data) \
{ \
    JS_ASSERT(GetObjectClass(obj) == detail::Type ## ArrayClassPtr); \
    const JS::Value &slot = GetReservedSlot(obj, detail::TypedArrayLengthSlot); \
    *length = mozilla::SafeCast<uint32_t>(slot.toInt32()); \
    *data = static_cast<type*>(GetObjectPrivate(obj)); \
}

JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Int8, int8_t)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Uint8, uint8_t)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Uint8Clamped, uint8_t)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Int16, int16_t)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Uint16, uint16_t)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Int32, int32_t)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Uint32, uint32_t)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Float32, float)
JS_DEFINE_DATA_AND_LENGTH_ACCESSOR(Float64, double)

#undef JS_DEFINE_DATA_AND_LENGTH_ACCESSOR

// This one isn't inlined because it's rather tricky (by dint of having to deal
// with a dozen-plus classes and varying slot layouts.
extern JS_FRIEND_API(void)
GetArrayBufferViewLengthAndData(JSObject *obj, uint32_t *length, uint8_t **data);

// This one isn't inlined because there are a bunch of different ArrayBuffer
// classes that would have to be individually handled here.
extern JS_FRIEND_API(void)
GetArrayBufferLengthAndData(JSObject *obj, uint32_t *length, uint8_t **data);

} // namespace js

/*
 * Unwrap Typed arrays all at once. Return nullptr without throwing if the
 * object cannot be viewed as the correct typed array, or the typed array
 * object on success, filling both outparameters.
 */
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsInt8Array(JSObject *obj, uint32_t *length, int8_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsUint8Array(JSObject *obj, uint32_t *length, uint8_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsUint8ClampedArray(JSObject *obj, uint32_t *length, uint8_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsInt16Array(JSObject *obj, uint32_t *length, int16_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsUint16Array(JSObject *obj, uint32_t *length, uint16_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsInt32Array(JSObject *obj, uint32_t *length, int32_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsUint32Array(JSObject *obj, uint32_t *length, uint32_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsFloat32Array(JSObject *obj, uint32_t *length, float **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsFloat64Array(JSObject *obj, uint32_t *length, double **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsArrayBufferView(JSObject *obj, uint32_t *length, uint8_t **data);
extern JS_FRIEND_API(JSObject *)
JS_GetObjectAsArrayBuffer(JSObject *obj, uint32_t *length, uint8_t **data);

/*
 * Get the type of elements in a typed array, or TYPE_DATAVIEW if a DataView.
 *
 * |obj| must have passed a JS_IsArrayBufferView/JS_Is*Array test, or somehow
 * be known that it would pass such a test: it is an ArrayBufferView or a
 * wrapper of an ArrayBufferView, and the unwrapping will succeed.
 */
extern JS_FRIEND_API(JSArrayBufferViewType)
JS_GetArrayBufferViewType(JSObject *obj);

/*
 * Check whether obj supports the JS_GetArrayBuffer* APIs. Note that this may
 * return false if a security wrapper is encountered that denies the
 * unwrapping. If this test succeeds, then it is safe to call the various
 * accessor JSAPI calls defined below.
 */
extern JS_FRIEND_API(bool)
JS_IsArrayBufferObject(JSObject *obj);

/*
 * Return the available byte length of an array buffer.
 *
 * |obj| must have passed a JS_IsArrayBufferObject test, or somehow be known
 * that it would pass such a test: it is an ArrayBuffer or a wrapper of an
 * ArrayBuffer, and the unwrapping will succeed.
 */
extern JS_FRIEND_API(uint32_t)
JS_GetArrayBufferByteLength(JSObject *obj);

/*
 * Check whether the obj is ArrayBufferObject and memory mapped. Note that this
 * may return false if a security wrapper is encountered that denies the
 * unwrapping.
 */
extern JS_FRIEND_API(bool)
JS_IsMappedArrayBufferObject(JSObject *obj);

/*
 * Return the number of elements in a typed array.
 *
 * |obj| must have passed a JS_IsTypedArrayObject/JS_Is*Array test, or somehow
 * be known that it would pass such a test: it is a typed array or a wrapper of
 * a typed array, and the unwrapping will succeed.
 */
extern JS_FRIEND_API(uint32_t)
JS_GetTypedArrayLength(JSObject *obj);

/*
 * Return the byte offset from the start of an array buffer to the start of a
 * typed array view.
 *
 * |obj| must have passed a JS_IsTypedArrayObject/JS_Is*Array test, or somehow
 * be known that it would pass such a test: it is a typed array or a wrapper of
 * a typed array, and the unwrapping will succeed.
 */
extern JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteOffset(JSObject *obj);

/*
 * Return the byte length of a typed array.
 *
 * |obj| must have passed a JS_IsTypedArrayObject/JS_Is*Array test, or somehow
 * be known that it would pass such a test: it is a typed array or a wrapper of
 * a typed array, and the unwrapping will succeed.
 */
extern JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteLength(JSObject *obj);

/*
 * Check whether obj supports JS_ArrayBufferView* APIs. Note that this may
 * return false if a security wrapper is encountered that denies the
 * unwrapping.
 */
extern JS_FRIEND_API(bool)
JS_IsArrayBufferViewObject(JSObject *obj);

/*
 * More generic name for JS_GetTypedArrayByteLength to cover DataViews as well
 */
extern JS_FRIEND_API(uint32_t)
JS_GetArrayBufferViewByteLength(JSObject *obj);

/*
 * Return a pointer to the start of the data referenced by a typed array. The
 * data is still owned by the typed array, and should not be modified on
 * another thread. Furthermore, the pointer can become invalid on GC (if the
 * data is small and fits inside the array's GC header), so callers must take
 * care not to hold on across anything that could GC.
 *
 * |obj| must have passed a JS_Is*Array test, or somehow be known that it would
 * pass such a test: it is a typed array or a wrapper of a typed array, and the
 * unwrapping will succeed.
 */

extern JS_FRIEND_API(uint8_t *)
JS_GetArrayBufferData(JSObject *obj);
extern JS_FRIEND_API(int8_t *)
JS_GetInt8ArrayData(JSObject *obj);
extern JS_FRIEND_API(uint8_t *)
JS_GetUint8ArrayData(JSObject *obj);
extern JS_FRIEND_API(uint8_t *)
JS_GetUint8ClampedArrayData(JSObject *obj);
extern JS_FRIEND_API(int16_t *)
JS_GetInt16ArrayData(JSObject *obj);
extern JS_FRIEND_API(uint16_t *)
JS_GetUint16ArrayData(JSObject *obj);
extern JS_FRIEND_API(int32_t *)
JS_GetInt32ArrayData(JSObject *obj);
extern JS_FRIEND_API(uint32_t *)
JS_GetUint32ArrayData(JSObject *obj);
extern JS_FRIEND_API(float *)
JS_GetFloat32ArrayData(JSObject *obj);
extern JS_FRIEND_API(double *)
JS_GetFloat64ArrayData(JSObject *obj);

/*
 * Stable versions of the above functions where the buffer remains valid as long
 * as the object is live.
 */
extern JS_FRIEND_API(uint8_t *)
JS_GetStableArrayBufferData(JSContext *cx, JS::HandleObject obj);

/*
 * Same as above, but for any kind of ArrayBufferView. Prefer the type-specific
 * versions when possible.
 */
extern JS_FRIEND_API(void *)
JS_GetArrayBufferViewData(JSObject *obj);

/*
 * Return the ArrayBuffer underlying an ArrayBufferView. If the buffer has been
 * neutered, this will still return the neutered buffer. |obj| must be an
 * object that would return true for JS_IsArrayBufferViewObject().
 */
extern JS_FRIEND_API(JSObject *)
JS_GetArrayBufferViewBuffer(JSContext *cx, JS::HandleObject obj);

typedef enum {
    ChangeData,
    KeepData
} NeuterDataDisposition;

/*
 * Set an ArrayBuffer's length to 0 and neuter all of its views.
 *
 * The |changeData| argument is a hint to inform internal behavior with respect
 * to the internal pointer to the ArrayBuffer's data after being neutered.
 * There is no guarantee it will be respected.  But if it is respected, the
 * ArrayBuffer's internal data pointer will, or will not, have changed
 * accordingly.
 */
extern JS_FRIEND_API(bool)
JS_NeuterArrayBuffer(JSContext *cx, JS::HandleObject obj,
                     NeuterDataDisposition changeData);

/*
 * Check whether the obj is ArrayBufferObject and neutered. Note that this
 * may return false if a security wrapper is encountered that denies the
 * unwrapping.
 */
extern JS_FRIEND_API(bool)
JS_IsNeuteredArrayBufferObject(JSObject *obj);

/*
 * Check whether obj supports JS_GetDataView* APIs.
 */
JS_FRIEND_API(bool)
JS_IsDataViewObject(JSObject *obj);

/*
 * Return the byte offset of a data view into its array buffer. |obj| must be a
 * DataView.
 *
 * |obj| must have passed a JS_IsDataViewObject test, or somehow be known that
 * it would pass such a test: it is a data view or a wrapper of a data view,
 * and the unwrapping will succeed.
 */
JS_FRIEND_API(uint32_t)
JS_GetDataViewByteOffset(JSObject *obj);

/*
 * Return the byte length of a data view.
 *
 * |obj| must have passed a JS_IsDataViewObject test, or somehow be known that
 * it would pass such a test: it is a data view or a wrapper of a data view,
 * and the unwrapping will succeed. If cx is nullptr, then DEBUG builds may be
 * unable to assert when unwrapping should be disallowed.
 */
JS_FRIEND_API(uint32_t)
JS_GetDataViewByteLength(JSObject *obj);

/*
 * Return a pointer to the beginning of the data referenced by a DataView.
 *
 * |obj| must have passed a JS_IsDataViewObject test, or somehow be known that
 * it would pass such a test: it is a data view or a wrapper of a data view,
 * and the unwrapping will succeed. If cx is nullptr, then DEBUG builds may be
 * unable to assert when unwrapping should be disallowed.
 */
JS_FRIEND_API(void *)
JS_GetDataViewData(JSObject *obj);

namespace js {

/*
 * Add a watchpoint -- in the Object.prototype.watch sense -- to |obj| for the
 * property |id|, using the callable object |callable| as the function to be
 * called for notifications.
 *
 * This is an internal function exposed -- temporarily -- only so that DOM
 * proxies can be watchable.  Don't use it!  We'll soon kill off the
 * Object.prototype.{,un}watch functions, at which point this will go too.
 */
extern JS_FRIEND_API(bool)
WatchGuts(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::HandleObject callable);

/*
 * Remove a watchpoint -- in the Object.prototype.watch sense -- from |obj| for
 * the property |id|.
 *
 * This is an internal function exposed -- temporarily -- only so that DOM
 * proxies can be watchable.  Don't use it!  We'll soon kill off the
 * Object.prototype.{,un}watch functions, at which point this will go too.
 */
extern JS_FRIEND_API(bool)
UnwatchGuts(JSContext *cx, JS::HandleObject obj, JS::HandleId id);

} // namespace js

/*
 * A class, expected to be passed by value, which represents the CallArgs for a
 * JSJitGetterOp.
 */
class JSJitGetterCallArgs : protected JS::MutableHandleValue
{
  public:
    explicit JSJitGetterCallArgs(const JS::CallArgs& args)
      : JS::MutableHandleValue(args.rval())
    {}

    explicit JSJitGetterCallArgs(JS::RootedValue* rooted)
      : JS::MutableHandleValue(rooted)
    {}

    JS::MutableHandleValue rval() {
        return *this;
    }
};

/*
 * A class, expected to be passed by value, which represents the CallArgs for a
 * JSJitSetterOp.
 */
class JSJitSetterCallArgs : protected JS::MutableHandleValue
{
  public:
    explicit JSJitSetterCallArgs(const JS::CallArgs& args)
      : JS::MutableHandleValue(args[0])
    {}

    JS::MutableHandleValue operator[](unsigned i) {
        MOZ_ASSERT(i == 0);
        return *this;
    }

    unsigned length() const { return 1; }

    // Add get() or maybe hasDefined() as needed
};

struct JSJitMethodCallArgsTraits;

/*
 * A class, expected to be passed by reference, which represents the CallArgs
 * for a JSJitMethodOp.
 */
class JSJitMethodCallArgs : protected JS::detail::CallArgsBase<JS::detail::NoUsedRval>
{
  private:
    typedef JS::detail::CallArgsBase<JS::detail::NoUsedRval> Base;
    friend struct JSJitMethodCallArgsTraits;

  public:
    explicit JSJitMethodCallArgs(const JS::CallArgs& args) {
        argv_ = args.array();
        argc_ = args.length();
    }

    JS::MutableHandleValue rval() const {
        return Base::rval();
    }

    unsigned length() const { return Base::length(); }

    JS::MutableHandleValue operator[](unsigned i) const {
        return Base::operator[](i);
    }

    bool hasDefined(unsigned i) const {
        return Base::hasDefined(i);
    }

    JSObject &callee() const {
        // We can't use Base::callee() because that will try to poke at
        // this->usedRval_, which we don't have.
        return argv_[-2].toObject();
    }

    // Add get() as needed
};

struct JSJitMethodCallArgsTraits
{
    static const size_t offsetOfArgv = offsetof(JSJitMethodCallArgs, argv_);
    static const size_t offsetOfArgc = offsetof(JSJitMethodCallArgs, argc_);
};

/*
 * This struct contains metadata passed from the DOM to the JS Engine for JIT
 * optimizations on DOM property accessors. Eventually, this should be made
 * available to general JSAPI users, but we are not currently ready to do so.
 */
typedef bool
(* JSJitGetterOp)(JSContext *cx, JS::HandleObject thisObj,
                  void *specializedThis, JSJitGetterCallArgs args);
typedef bool
(* JSJitSetterOp)(JSContext *cx, JS::HandleObject thisObj,
                  void *specializedThis, JSJitSetterCallArgs args);
typedef bool
(* JSJitMethodOp)(JSContext *cx, JS::HandleObject thisObj,
                  void *specializedThis, const JSJitMethodCallArgs& args);

struct JSJitInfo {
    enum OpType {
        Getter,
        Setter,
        Method,
        ParallelNative,
        StaticMethod,
        // Must be last
        OpTypeCount
    };

    enum ArgType {
        // Basic types
        String = (1 << 0),
        Integer = (1 << 1), // Only 32-bit or less
        Double = (1 << 2), // Maybe we want to add Float sometime too
        Boolean = (1 << 3),
        Object = (1 << 4),
        Null = (1 << 5),

        // And derived types
        Numeric = Integer | Double,
        // Should "Primitive" use the WebIDL definition, which
        // excludes string and null, or the typical JS one that includes them?
        Primitive = Numeric | Boolean | Null | String,
        ObjectOrNull = Object | Null,
        Any = ObjectOrNull | Primitive,

        // Our sentinel value.
        ArgTypeListEnd = (1 << 31)
    };

    static_assert(Any & String, "Any must include String.");
    static_assert(Any & Integer, "Any must include Integer.");
    static_assert(Any & Double, "Any must include Double.");
    static_assert(Any & Boolean, "Any must include Boolean.");
    static_assert(Any & Object, "Any must include Object.");
    static_assert(Any & Null, "Any must include Null.");

    enum AliasSet {
        // An enum that describes what this getter/setter/method aliases.  This
        // determines what things can be hoisted past this call, and if this
        // call is movable what it can be hoisted past.

        // Alias nothing: a constant value, getting it can't affect any other
        // values, nothing can affect it.
        AliasNone,

        // Alias things that can modify the DOM but nothing else.  Doing the
        // call can't affect the behavior of any other function.
        AliasDOMSets,

        // Alias the world.  Calling this can change arbitrary values anywhere
        // in the system.  Most things fall in this bucket.
        AliasEverything,

        // Must be last.
        AliasSetCount
    };

    bool hasParallelNative() const
    {
        return type() == ParallelNative;
    }

    bool needsOuterizedThisObject() const
    {
        return type() != Getter && type() != Setter;
    }

    bool isTypedMethodJitInfo() const
    {
        return isTypedMethod;
    }

    OpType type() const
    {
        return OpType(type_);
    }

    AliasSet aliasSet() const
    {
        return AliasSet(aliasSet_);
    }

    JSValueType returnType() const
    {
        return JSValueType(returnType_);
    }

    union {
        JSJitGetterOp getter;
        JSJitSetterOp setter;
        JSJitMethodOp method;
        /* An alternative native that's safe to call in parallel mode. */
        JSParallelNative parallelNative;
        /* A DOM static method, used for Promise wrappers */
        JSNative staticMethod;
    };

    uint16_t protoID;
    uint16_t depth;

    // These fields are carefully packed to take up 4 bytes.  If you need more
    // bits for whatever reason, please see if you can steal bits from existing
    // fields before adding more members to this structure.

#define JITINFO_OP_TYPE_BITS 4
#define JITINFO_ALIAS_SET_BITS 4
#define JITINFO_RETURN_TYPE_BITS 8

    // The OpType that says what sort of function we are.
    uint32_t type_ : JITINFO_OP_TYPE_BITS;

    // The alias set for this op.  This is a _minimal_ alias set; in
    // particular for a method it does not include whatever argument
    // conversions might do.  That's covered by argTypes and runtime
    // analysis of the actual argument types being passed in.
    uint32_t aliasSet_ : JITINFO_ALIAS_SET_BITS;

    // The return type tag.  Might be JSVAL_TYPE_UNKNOWN.
    uint32_t returnType_ : JITINFO_RETURN_TYPE_BITS;

    static_assert(OpTypeCount <= (1 << JITINFO_OP_TYPE_BITS),
                  "Not enough space for OpType");
    static_assert(AliasSetCount <= (1 << JITINFO_ALIAS_SET_BITS),
                  "Not enough space for AliasSet");
    static_assert((sizeof(JSValueType) * 8) <= JITINFO_RETURN_TYPE_BITS,
                  "Not enough space for JSValueType");

#undef JITINFO_RETURN_TYPE_BITS
#undef JITINFO_ALIAS_SET_BITS
#undef JITINFO_OP_TYPE_BITS

    uint32_t isInfallible : 1; /* Is op fallible? False in setters. */
    uint32_t isMovable : 1;    /* Is op movable?  To be movable the op must
                                  not AliasEverything, but even that might
                                  not be enough (e.g. in cases when it can
                                  throw). */
    // XXXbz should we have a JSValueType for the type of the member?
    uint32_t isAlwaysInSlot : 1; /* True if this is a getter that can always
                                    get the value from a slot of the "this"
                                    object. */
    uint32_t isLazilyCachedInSlot : 1; /* True if this is a getter that can
                                          sometimes (if the slot doesn't contain
                                          UndefinedValue()) get the value from a
                                          slot of the "this" object. */
    uint32_t isTypedMethod : 1; /* True if this is an instance of
                                   JSTypedMethodJitInfo. */
    uint32_t slotIndex : 11;   /* If isAlwaysInSlot or isSometimesInSlot is
                                  true, the index of the slot to get the value
                                  from.  Otherwise 0. */
};

static_assert(sizeof(JSJitInfo) == (sizeof(void*) + 2 * sizeof(uint32_t)),
              "There are several thousand instances of JSJitInfo stored in "
              "a binary. Please don't increase its space requirements without "
              "verifying that there is no other way forward (better packing, "
              "smaller datatypes for fields, subclassing, etc.).");

struct JSTypedMethodJitInfo
{
    // We use C-style inheritance here, rather than C++ style inheritance
    // because not all compilers support brace-initialization for non-aggregate
    // classes. Using C++ style inheritance and constructors instead of
    // brace-initialization would also force the creation of static
    // constructors (on some compilers) when JSJitInfo and JSTypedMethodJitInfo
    // structures are declared. Since there can be several thousand of these
    // structures present and we want to have roughly equivalent performance
    // across a range of compilers, we do things manually.
    JSJitInfo base;

    const JSJitInfo::ArgType* const argTypes; /* For a method, a list of sets of
                                                 types that the function
                                                 expects.  This can be used,
                                                 for example, to figure out
                                                 when argument coercions can
                                                 have side-effects. */
};

namespace JS {
namespace detail {

/* NEVER DEFINED, DON'T USE.  For use by JS_CAST_PARALLEL_NATIVE_TO only. */
inline int CheckIsParallelNative(JSParallelNative parallelNative);

} // namespace detail
} // namespace JS

#define JS_CAST_PARALLEL_NATIVE_TO(v, To) \
    (static_cast<void>(sizeof(JS::detail::CheckIsParallelNative(v))), \
     reinterpret_cast<To>(v))

/*
 * You may ask yourself: why do we define a wrapper around a wrapper here?
 * The answer is that some compilers don't understand initializing a union
 * as we do below with a construct like:
 *
 * reinterpret_cast<JSJitGetterOp>(JSParallelNativeThreadSafeWrapper<op>)
 *
 * (We need the reinterpret_cast because we must initialize the union with
 * a datum of the type of the union's first member.)
 *
 * Presumably this has something to do with template instantiation.
 * Initializing with a normal function pointer seems to work fine. Hence
 * the ugliness that you see before you.
 */
#define JS_JITINFO_NATIVE_PARALLEL(infoName, parallelOp)                \
    const JSJitInfo infoName =                                          \
        {{JS_CAST_PARALLEL_NATIVE_TO(parallelOp, JSJitGetterOp)},0,0,JSJitInfo::ParallelNative,JSJitInfo::AliasEverything,JSVAL_TYPE_MISSING,false,false,false,false,false,0}

#define JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(infoName, wrapperName, serialOp) \
    bool wrapperName##_ParallelNativeThreadSafeWrapper(js::ForkJoinContext *cx, unsigned argc, \
                                                       JS::Value *vp)   \
    {                                                                   \
        return JSParallelNativeThreadSafeWrapper<serialOp>(cx, argc, vp); \
    }                                                                   \
    JS_JITINFO_NATIVE_PARALLEL(infoName, wrapperName##_ParallelNativeThreadSafeWrapper)

static MOZ_ALWAYS_INLINE const JSJitInfo *
FUNCTION_VALUE_TO_JITINFO(const JS::Value& v)
{
    JS_ASSERT(js::GetObjectClass(&v.toObject()) == js::FunctionClassPtr);
    return reinterpret_cast<js::shadow::Function *>(&v.toObject())->jitinfo;
}

/* Statically asserted in jsfun.h. */
static const unsigned JS_FUNCTION_INTERPRETED_BIT = 0x1;

static MOZ_ALWAYS_INLINE void
SET_JITINFO(JSFunction * func, const JSJitInfo *info)
{
    js::shadow::Function *fun = reinterpret_cast<js::shadow::Function *>(func);
    JS_ASSERT(!(fun->flags & JS_FUNCTION_INTERPRETED_BIT));
    fun->jitinfo = info;
}

/*
 * Engine-internal extensions of jsid.  This code is here only until we
 * eliminate Gecko's dependencies on it!
 */

static MOZ_ALWAYS_INLINE jsid
JSID_FROM_BITS(size_t bits)
{
    jsid id;
    JSID_BITS(id) = bits;
    return id;
}

namespace js {
namespace detail {
bool IdMatchesAtom(jsid id, JSAtom *atom);
}
}

/*
 * Must not be used on atoms that are representable as integer jsids.
 * Prefer NameToId or AtomToId over this function:
 *
 * A PropertyName is an atom that does not contain an integer in the range
 * [0, UINT32_MAX]. However, jsid can only hold an integer in the range
 * [0, JSID_INT_MAX] (where JSID_INT_MAX == 2^31-1).  Thus, for the range of
 * integers (JSID_INT_MAX, UINT32_MAX], to represent as a jsid 'id', it must be
 * the case JSID_IS_ATOM(id) and !JSID_TO_ATOM(id)->isPropertyName().  In most
 * cases when creating a jsid, code does not have to care about this corner
 * case because:
 *
 * - When given an arbitrary JSAtom*, AtomToId must be used, which checks for
 *   integer atoms representable as integer jsids, and does this conversion.
 *
 * - When given a PropertyName*, NameToId can be used which which does not need
 *   to do any dynamic checks.
 *
 * Thus, it is only the rare third case which needs this function, which
 * handles any JSAtom* that is known not to be representable with an int jsid.
 */
static MOZ_ALWAYS_INLINE jsid
NON_INTEGER_ATOM_TO_JSID(JSAtom *atom)
{
    JS_ASSERT(((size_t)atom & 0x7) == 0);
    jsid id = JSID_FROM_BITS((size_t)atom);
    JS_ASSERT(js::detail::IdMatchesAtom(id, atom));
    return id;
}

/* All strings stored in jsids are atomized, but are not necessarily property names. */
static MOZ_ALWAYS_INLINE bool
JSID_IS_ATOM(jsid id)
{
    return JSID_IS_STRING(id);
}

static MOZ_ALWAYS_INLINE bool
JSID_IS_ATOM(jsid id, JSAtom *atom)
{
    return id == JSID_FROM_BITS((size_t)atom);
}

static MOZ_ALWAYS_INLINE JSAtom *
JSID_TO_ATOM(jsid id)
{
    return (JSAtom *)JSID_TO_STRING(id);
}

JS_STATIC_ASSERT(sizeof(jsid) == sizeof(void*));

namespace js {

static MOZ_ALWAYS_INLINE JS::Value
IdToValue(jsid id)
{
    if (JSID_IS_STRING(id))
        return JS::StringValue(JSID_TO_STRING(id));
    if (MOZ_LIKELY(JSID_IS_INT(id)))
        return JS::Int32Value(JSID_TO_INT(id));
    if (MOZ_LIKELY(JSID_IS_OBJECT(id)))
        return JS::ObjectValue(*JSID_TO_OBJECT(id));
    JS_ASSERT(JSID_IS_VOID(id));
    return JS::UndefinedValue();
}

extern JS_FRIEND_API(bool)
IsTypedArrayThisCheck(JS::IsAcceptableThis test);

/*
 * If the embedder has registered a default JSContext callback, returns the
 * result of the callback. Otherwise, asserts that |rt| has exactly one
 * JSContext associated with it, and returns that context.
 */
extern JS_FRIEND_API(JSContext *)
DefaultJSContext(JSRuntime *rt);

typedef JSContext*
(* DefaultJSContextCallback)(JSRuntime *rt);

JS_FRIEND_API(void)
SetDefaultJSContextCallback(JSRuntime *rt, DefaultJSContextCallback cb);

/*
 * To help embedders enforce their invariants, we allow them to specify in
 * advance which JSContext should be passed to JSAPI calls. If this is set
 * to a non-null value, the assertSameCompartment machinery does double-
 * duty (in debug builds) to verify that it matches the cx being used.
 */
#ifdef DEBUG
JS_FRIEND_API(void)
Debug_SetActiveJSContext(JSRuntime *rt, JSContext *cx);
#else
inline void
Debug_SetActiveJSContext(JSRuntime *rt, JSContext *cx) {};
#endif


enum CTypesActivityType {
    CTYPES_CALL_BEGIN,
    CTYPES_CALL_END,
    CTYPES_CALLBACK_BEGIN,
    CTYPES_CALLBACK_END
};

typedef void
(* CTypesActivityCallback)(JSContext *cx, CTypesActivityType type);

/*
 * Sets a callback that is run whenever js-ctypes is about to be used when
 * calling into C.
 */
JS_FRIEND_API(void)
SetCTypesActivityCallback(JSRuntime *rt, CTypesActivityCallback cb);

class JS_FRIEND_API(AutoCTypesActivityCallback) {
  private:
    JSContext *cx;
    CTypesActivityCallback callback;
    CTypesActivityType endType;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoCTypesActivityCallback(JSContext *cx, CTypesActivityType beginType,
                               CTypesActivityType endType
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoCTypesActivityCallback() {
        DoEndCallback();
    }
    void DoEndCallback() {
        if (callback) {
            callback(cx, endType);
            callback = nullptr;
        }
    }
};

typedef bool
(* ObjectMetadataCallback)(JSContext *cx, JSObject **pmetadata);

/*
 * Specify a callback to invoke when creating each JS object in the current
 * compartment, which may return a metadata object to associate with the
 * object. Objects with different metadata have different shape hierarchies,
 * so for efficiency, objects should generally try to share metadata objects.
 */
JS_FRIEND_API(void)
SetObjectMetadataCallback(JSContext *cx, ObjectMetadataCallback callback);

/* Manipulate the metadata associated with an object. */

JS_FRIEND_API(bool)
SetObjectMetadata(JSContext *cx, JS::HandleObject obj, JS::HandleObject metadata);

JS_FRIEND_API(JSObject *)
GetObjectMetadata(JSObject *obj);

JS_FRIEND_API(void)
UnsafeDefineElement(JSContext *cx, JS::HandleObject obj, uint32_t index, JS::HandleValue value);

JS_FRIEND_API(bool)
SliceSlowly(JSContext* cx, JS::HandleObject obj, JS::HandleObject receiver,
            uint32_t begin, uint32_t end, JS::HandleObject result);

/* ES5 8.12.8. */
extern JS_FRIEND_API(bool)
DefaultValue(JSContext *cx, JS::HandleObject obj, JSType hint, JS::MutableHandleValue vp);

/*
 * Helper function. To approximate a call to the [[DefineOwnProperty]] internal
 * method described in ES5, first call this, then call JS_DefinePropertyById.
 *
 * JS_DefinePropertyById by itself does not enforce the invariants on
 * non-configurable properties when obj->isNative(). This function performs the
 * relevant checks (specified in ES5 8.12.9 [[DefineOwnProperty]] steps 1-11),
 * but only if obj is native.
 *
 * The reason for the messiness here is that ES5 uses [[DefineOwnProperty]] as
 * a sort of extension point, but there is no hook in js::Class,
 * js::ProxyHandler, or the JSAPI with precisely the right semantics for it.
 */
extern JS_FRIEND_API(bool)
CheckDefineProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::HandleValue value,
                    unsigned attrs,
                    JSPropertyOp getter = nullptr, JSStrictPropertyOp setter = nullptr);

} /* namespace js */

extern JS_FRIEND_API(bool)
js_DefineOwnProperty(JSContext *cx, JSObject *objArg, jsid idArg,
                     JS::Handle<JSPropertyDescriptor> descriptor, bool *bp);

extern JS_FRIEND_API(bool)
js_ReportIsNotFunction(JSContext *cx, JS::HandleValue v);

#ifdef JSGC_GENERATIONAL
extern JS_FRIEND_API(void)
JS_StoreObjectPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, JSObject *key, void *data),
                                  JSObject *key, void *data);

extern JS_FRIEND_API(void)
JS_StoreStringPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, JSString *key, void *data),
                                  JSString *key, void *data);
#else
inline void
JS_StoreObjectPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, JSObject *key, void *data),
                                  JSObject *key, void *data) {}

inline void
JS_StoreStringPostBarrierCallback(JSContext* cx,
                                  void (*callback)(JSTracer *trc, JSString *key, void *data),
                                  JSString *key, void *data) {}
#endif /* JSGC_GENERATIONAL */

#endif /* jsfriendapi_h */
