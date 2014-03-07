/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript API.
 */

#include "jsapi.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"

#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "json.h"
#include "jsprf.h"
#include "jsproxy.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#ifdef JS_THREADSAFE
#include "jsworkers.h"
#endif
#include "jswrapper.h"
#include "prmjtime.h"

#if ENABLE_YARR_JIT
#include "assembler/jit/ExecutableAllocator.h"
#endif
#include "builtin/Eval.h"
#include "builtin/Intl.h"
#include "builtin/MapObject.h"
#include "builtin/RegExp.h"
#ifdef ENABLE_BINARYDATA
#include "builtin/SIMD.h"
#include "builtin/TypedObject.h"
#endif
#include "frontend/BytecodeCompiler.h"
#include "frontend/FullParseHandler.h"  // for JS_BufferIsCompileableUnit
#include "frontend/Parser.h" // for JS_BufferIsCompileableUnit
#include "gc/Marking.h"
#include "jit/AsmJSLink.h"
#include "jit/JitCommon.h"
#include "js/CharacterEncoding.h"
#include "js/SliceBudget.h"
#include "js/StructuredClone.h"
#if ENABLE_INTL_API
#include "unicode/uclean.h"
#include "unicode/utypes.h"
#endif // ENABLE_INTL_API
#include "vm/DateObject.h"
#include "vm/Debugger.h"
#include "vm/ErrorObject.h"
#include "vm/Interpreter.h"
#include "vm/NumericConversions.h"
#include "vm/RegExpStatics.h"
#include "vm/Runtime.h"
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/StopIterationObject.h"
#include "vm/StringBuffer.h"
#include "vm/TypedArrayObject.h"
#include "vm/WeakMapObject.h"
#include "vm/WrapperObject.h"
#include "vm/Xdr.h"
#include "yarr/BumpPointerAllocator.h"

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/ObjectImpl-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::Maybe;
using mozilla::PodCopy;
using mozilla::PodZero;

using js::frontend::Parser;

#ifdef HAVE_VA_LIST_AS_ARRAY
#define JS_ADDRESSOF_VA_LIST(ap) ((va_list *)(ap))
#else
#define JS_ADDRESSOF_VA_LIST(ap) (&(ap))
#endif

/* Make sure that jschar is two bytes unsigned integer */
JS_STATIC_ASSERT((jschar)-1 > 0);
JS_STATIC_ASSERT(sizeof(jschar) == 2);

JS_PUBLIC_API(int64_t)
JS_Now()
{
    return PRMJ_Now();
}

JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx)
{
    return cx->runtime()->NaNValue;
}

JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    return cx->runtime()->negativeInfinityValue;
}

JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    return cx->runtime()->positiveInfinityValue;
}

JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    return STRING_TO_JSVAL(cx->runtime()->emptyString);
}

JS_PUBLIC_API(JSString *)
JS_GetEmptyString(JSRuntime *rt)
{
    JS_ASSERT(rt->hasContexts());
    return rt->emptyString;
}

namespace js {

void
AssertHeapIsIdle(JSRuntime *rt)
{
    JS_ASSERT(rt->heapState == js::Idle);
}

void
AssertHeapIsIdle(JSContext *cx)
{
    AssertHeapIsIdle(cx->runtime());
}

}

static void
AssertHeapIsIdleOrIterating(JSRuntime *rt)
{
    JS_ASSERT(!rt->isHeapCollecting());
}

static void
AssertHeapIsIdleOrIterating(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx->runtime());
}

static void
AssertHeapIsIdleOrStringIsFlat(JSContext *cx, JSString *str)
{
    /*
     * We allow some functions to be called during a GC as long as the argument
     * is a flat string, since that will not cause allocation.
     */
    JS_ASSERT_IF(cx->runtime()->isHeapBusy(), str->isFlat());
}

JS_PUBLIC_API(bool)
JS_ConvertArguments(JSContext *cx, const CallArgs &args, const char *format, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);

    va_start(ap, format);
    ok = JS_ConvertArgumentsVA(cx, args, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(bool)
JS_ConvertArgumentsVA(JSContext *cx, const CallArgs &args, const char *format, va_list ap)
{
    unsigned index = 0;
    bool required;
    char c;
    double d;
    JSString *str;
    RootedObject obj(cx);
    RootedValue val(cx);

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, args);
    required = true;
    while ((c = *format++) != '\0') {
        if (isspace(c))
            continue;
        if (c == '/') {
            required = false;
            continue;
        }
        if (index == args.length()) {
            if (required) {
                if (JSFunction *fun = ReportIfNotFunction(cx, args.calleev())) {
                    char numBuf[12];
                    JS_snprintf(numBuf, sizeof numBuf, "%u", args.length());
                    JSAutoByteString funNameBytes;
                    if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                             JSMSG_MORE_ARGS_NEEDED,
                                             name, numBuf, (args.length() == 1) ? "" : "s");
                    }
                }
                return false;
            }
            break;
        }
        MutableHandleValue arg = args[index++];
        switch (c) {
          case 'b':
            *va_arg(ap, bool *) = ToBoolean(arg);
            break;
          case 'c':
            if (!ToUint16(cx, arg, va_arg(ap, uint16_t *)))
                return false;
            break;
          case 'i':
          case 'j': // "j" was broken, you should not use it.
            if (!ToInt32(cx, arg, va_arg(ap, int32_t *)))
                return false;
            break;
          case 'u':
            if (!ToUint32(cx, arg, va_arg(ap, uint32_t *)))
                return false;
            break;
          case 'd':
            if (!ToNumber(cx, arg, va_arg(ap, double *)))
                return false;
            break;
          case 'I':
            if (!ToNumber(cx, arg, &d))
                return false;
            *va_arg(ap, double *) = ToInteger(d);
            break;
          case 'S':
          case 'W':
            str = ToString<CanGC>(cx, arg);
            if (!str)
                return false;
            arg.setString(str);
            if (c == 'W') {
                JSFlatString *flat = str->ensureFlat(cx);
                if (!flat)
                    return false;
                *va_arg(ap, const jschar **) = flat->chars();
            } else {
                *va_arg(ap, JSString **) = str;
            }
            break;
          case 'o':
            if (arg.isNullOrUndefined()) {
                obj = nullptr;
            } else {
                obj = ToObject(cx, arg);
                if (!obj)
                    return false;
            }
            arg.setObjectOrNull(obj);
            *va_arg(ap, JSObject **) = obj;
            break;
          case 'f':
            obj = ReportIfNotFunction(cx, arg);
            if (!obj)
                return false;
            arg.setObject(*obj);
            *va_arg(ap, JSFunction **) = &obj->as<JSFunction>();
            break;
          case 'v':
            *va_arg(ap, jsval *) = arg;
            break;
          case '*':
            break;
          default:
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_CHAR, format);
            return false;
        }
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_ConvertValue(JSContext *cx, HandleValue value, JSType type, MutableHandleValue vp)
{
    bool ok;
    RootedObject obj(cx);
    JSString *str;
    double d;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    switch (type) {
      case JSTYPE_VOID:
        vp.setUndefined();
        ok = true;
        break;
      case JSTYPE_OBJECT:
        if (value.isNullOrUndefined()) {
            obj.set(nullptr);
        } else {
            obj = ToObject(cx, value);
            if (!obj)
                return false;
        }
        ok = true;
        break;
      case JSTYPE_FUNCTION:
        vp.set(value);
        obj = ReportIfNotFunction(cx, vp);
        ok = (obj != nullptr);
        break;
      case JSTYPE_STRING:
        str = ToString<CanGC>(cx, value);
        ok = (str != nullptr);
        if (ok)
            vp.setString(str);
        break;
      case JSTYPE_NUMBER:
        ok = ToNumber(cx, value, &d);
        if (ok)
            vp.setDouble(d);
        break;
      case JSTYPE_BOOLEAN:
        vp.setBoolean(ToBoolean(value));
        return true;
      default: {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%d", (int)type);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_TYPE, numBuf);
        ok = false;
        break;
      }
    }
    return ok;
}

JS_PUBLIC_API(bool)
JS_ValueToObject(JSContext *cx, HandleValue value, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    if (value.isNullOrUndefined()) {
        objp.set(nullptr);
        return true;
    }
    JSObject *obj = ToObject(cx, value);
    if (!obj)
        return false;
    objp.set(obj);
    return true;
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ReportIfNotFunction(cx, value);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ReportIfNotFunction(cx, value);
}

JS_PUBLIC_API(JSString *)
JS_ValueToSource(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ValueToSource(cx, value);
}

JS_PUBLIC_API(bool)
JS_DoubleIsInt32(double d, int32_t *ip)
{
    return mozilla::NumberIsInt32(d, ip);
}

JS_PUBLIC_API(int32_t)
JS_DoubleToInt32(double d)
{
    return ToInt32(d);
}

JS_PUBLIC_API(uint32_t)
JS_DoubleToUint32(double d)
{
    return ToUint32(d);
}

JS_PUBLIC_API(JSType)
JS_TypeOfValue(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return TypeOfValue(value);
}

JS_PUBLIC_API(const char *)
JS_GetTypeName(JSContext *cx, JSType type)
{
    if ((unsigned)type >= (unsigned)JSTYPE_LIMIT)
        return nullptr;
    return TypeStrings[type];
}

JS_PUBLIC_API(bool)
JS_StrictlyEqual(JSContext *cx, jsval value1, jsval value2, bool *equal)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value1, value2);
    bool eq;
    if (!StrictlyEqual(cx, value1, value2, &eq))
        return false;
    *equal = eq;
    return true;
}

JS_PUBLIC_API(bool)
JS_LooselyEqual(JSContext *cx, HandleValue value1, HandleValue value2, bool *equal)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value1, value2);
    JS_ASSERT(equal);
    return LooselyEqual(cx, value1, value2, equal);
}

JS_PUBLIC_API(bool)
JS_SameValue(JSContext *cx, jsval value1, jsval value2, bool *same)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value1, value2);
    bool s;
    if (!SameValue(cx, value1, value2, &s))
        return false;
    *same = s;
    return true;
}

JS_PUBLIC_API(bool)
JS_IsBuiltinEvalFunction(JSFunction *fun)
{
    return IsAnyBuiltinEval(fun);
}

JS_PUBLIC_API(bool)
JS_IsBuiltinFunctionConstructor(JSFunction *fun)
{
    return fun->isBuiltinFunctionConstructor();
}

/************************************************************************/

/*
 * SpiderMonkey's initialization status is tracked here, and it controls things
 * that should happen only once across all runtimes.  It's an API requirement
 * that JS_Init (and JS_ShutDown, if called) be called in a thread-aware
 * manner, so this variable doesn't need to be atomic.
 *
 * The only reason at present for the restriction that you can't call
 * JS_Init/stuff/JS_ShutDown multiple times is the Windows PRMJ NowInit
 * initialization code, which uses PR_CallOnce to initialize the PRMJ_Now
 * subsystem.  (For reinitialization to be permitted, we'd need to "reset" the
 * called-once status -- doable, but more trouble than it's worth now.)
 * Initializing that subsystem from JS_Init eliminates the problem, but
 * initialization can take a comparatively long time (15ms or so), so we
 * really don't want to do it in JS_Init, and we really do want to do it only
 * when PRMJ_Now is eventually called.
 */
enum InitState { Uninitialized, Running, ShutDown };
static InitState jsInitState = Uninitialized;

#ifdef DEBUG
static void
CheckMessageNumbering()
{
    // Assert that the numbers associated with the error names in js.msg are
    // monotonically increasing.  It's not a compile-time check, but it's
    // better than nothing.
    int errorNumber = 0;
# define MSG_DEF(name, number, count, exception, format)                      \
    JS_ASSERT(name == errorNumber++);
# include "js.msg"
# undef MSG_DEF
}

static unsigned
MessageParameterCount(const char *format)
{
    unsigned numfmtspecs = 0;
    for (const char *fmt = format; *fmt != '\0'; fmt++) {
        if (*fmt == '{' && isdigit(fmt[1]))
            ++numfmtspecs;
    }
    return numfmtspecs;
}

static void
CheckMessageParameterCounts()
{
    // Assert that each message format has the correct number of braced
    // parameters.
# define MSG_DEF(name, number, count, exception, format)                      \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(MessageParameterCount(format) == count);                    \
    JS_END_MACRO;
# include "js.msg"
# undef MSG_DEF
}
#endif /* DEBUG */

JS_PUBLIC_API(bool)
JS_Init(void)
{
    MOZ_ASSERT(jsInitState == Uninitialized,
               "must call JS_Init once before any JSAPI operation except "
               "JS_SetICUMemoryFunctions");
    MOZ_ASSERT(!JSRuntime::hasLiveRuntimes(),
               "how do we have live runtimes before JS_Init?");

#ifdef DEBUG
    CheckMessageNumbering();
    CheckMessageParameterCounts();
#endif

    using js::TlsPerThreadData;
    if (!TlsPerThreadData.initialized() && !TlsPerThreadData.init())
        return false;

#if defined(JS_ION)
    if (!jit::InitializeIon())
        return false;
#endif

    if (!ForkJoinContext::initialize())
        return false;

#if EXPOSE_INTL_API
    UErrorCode err = U_ZERO_ERROR;
    u_init(&err);
    if (U_FAILURE(err))
        return false;
#endif // EXPOSE_INTL_API

    jsInitState = Running;
    return true;
}

JS_PUBLIC_API(void)
JS_ShutDown(void)
{
    MOZ_ASSERT(jsInitState == Running,
               "JS_ShutDown must only be called after JS_Init and can't race with it");
#ifdef DEBUG
    if (JSRuntime::hasLiveRuntimes()) {
        // Gecko is too buggy to assert this just yet.
        fprintf(stderr,
                "WARNING: YOU ARE LEAKING THE WORLD (at least one JSRuntime "
                "and everything alive inside it, that is) AT JS_ShutDown "
                "TIME.  FIX THIS!\n");
    }
#endif

#ifdef JS_THREADSAFE
    WorkerThreadState().finish();
#endif

    PRMJ_NowShutdown();

#if EXPOSE_INTL_API
    u_cleanup();
#endif // EXPOSE_INTL_API

    jsInitState = ShutDown;
}

#ifdef DEBUG
JS_FRIEND_API(bool)
JS::isGCEnabled()
{
    return !TlsPerThreadData.get()->suppressGC;
}
#else
JS_FRIEND_API(bool) JS::isGCEnabled() { return true; }
#endif

JS_PUBLIC_API(JSRuntime *)
JS_NewRuntime(uint32_t maxbytes, JSUseHelperThreads useHelperThreads, JSRuntime *parentRuntime)
{
    MOZ_ASSERT(jsInitState == Running,
               "must call JS_Init prior to creating any JSRuntimes");

    // Any parent runtime should be the topmost parent. This assert
    // isn't required for correctness, but ensuring that the parent
    // runtime is not destroyed before this one is more easily done
    // for the main runtime in the process.
    JS_ASSERT_IF(parentRuntime, !parentRuntime->parentRuntime);

    JSRuntime *rt = js_new<JSRuntime>(parentRuntime, useHelperThreads);
    if (!rt)
        return nullptr;

    if (!rt->init(maxbytes)) {
        JS_DestroyRuntime(rt);
        return nullptr;
    }

    return rt;
}

JS_PUBLIC_API(void)
JS_DestroyRuntime(JSRuntime *rt)
{
    js_delete(rt);
}

JS_PUBLIC_API(bool)
JS_SetICUMemoryFunctions(JS_ICUAllocFn allocFn, JS_ICUReallocFn reallocFn, JS_ICUFreeFn freeFn)
{
    MOZ_ASSERT(jsInitState == Uninitialized,
               "must call JS_SetICUMemoryFunctions before any other JSAPI "
               "operation (including JS_Init)");

#if EXPOSE_INTL_API
    UErrorCode status = U_ZERO_ERROR;
    u_setMemoryFunctions(/* context = */ nullptr, allocFn, reallocFn, freeFn, &status);
    return U_SUCCESS(status);
#else
    return true;
#endif
}

JS_PUBLIC_API(void *)
JS_GetRuntimePrivate(JSRuntime *rt)
{
    return rt->data;
}

JS_PUBLIC_API(void)
JS_SetRuntimePrivate(JSRuntime *rt, void *data)
{
    rt->data = data;
}

#ifdef JS_THREADSAFE
static void
StartRequest(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));

    if (rt->requestDepth) {
        rt->requestDepth++;
    } else {
        /* Indicate that a request is running. */
        rt->requestDepth = 1;
        rt->triggerActivityCallback(true);
    }
}

static void
StopRequest(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));

    JS_ASSERT(rt->requestDepth != 0);
    if (rt->requestDepth != 1) {
        rt->requestDepth--;
    } else {
        rt->conservativeGC.updateForRequestEnd();
        rt->requestDepth = 0;
        rt->triggerActivityCallback(false);
    }
}
#endif /* JS_THREADSAFE */

JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    cx->outstandingRequests++;
    StartRequest(cx);
#endif
}

JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->outstandingRequests != 0);
    cx->outstandingRequests--;
    StopRequest(cx);
#endif
}

JS_PUBLIC_API(bool)
JS_IsInRequest(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));
    return rt->requestDepth != 0;
#else
    return false;
#endif
}

JS_PUBLIC_API(void)
JS_SetContextCallback(JSRuntime *rt, JSContextCallback cxCallback, void *data)
{
    rt->cxCallback = cxCallback;
    rt->cxCallbackData = data;
}

JS_PUBLIC_API(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    return NewContext(rt, stackChunkSize);
}

JS_PUBLIC_API(void)
JS_DestroyContext(JSContext *cx)
{
    JS_ASSERT(!cx->compartment());
    DestroyContext(cx, DCM_FORCE_GC);
}

JS_PUBLIC_API(void)
JS_DestroyContextNoGC(JSContext *cx)
{
    JS_ASSERT(!cx->compartment());
    DestroyContext(cx, DCM_NO_GC);
}

JS_PUBLIC_API(void *)
JS_GetContextPrivate(JSContext *cx)
{
    return cx->data;
}

JS_PUBLIC_API(void)
JS_SetContextPrivate(JSContext *cx, void *data)
{
    cx->data = data;
}

JS_PUBLIC_API(void *)
JS_GetSecondContextPrivate(JSContext *cx)
{
    return cx->data2;
}

JS_PUBLIC_API(void)
JS_SetSecondContextPrivate(JSContext *cx, void *data)
{
    cx->data2 = data;
}

JS_PUBLIC_API(JSRuntime *)
JS_GetRuntime(JSContext *cx)
{
    return cx->runtime();
}

JS_PUBLIC_API(JSRuntime *)
JS_GetParentRuntime(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();
    return rt->parentRuntime ? rt->parentRuntime : nullptr;
}

JS_PUBLIC_API(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    JSContext *cx = *iterp;
    cx = cx ? cx->getNext() : rt->contextList.getFirst();
    *iterp = cx;
    return cx;
}

JS_PUBLIC_API(JSVersion)
JS_GetVersion(JSContext *cx)
{
    return VersionNumber(cx->findVersion());
}

JS_PUBLIC_API(void)
JS_SetVersionForCompartment(JSCompartment *compartment, JSVersion version)
{
    compartment->options().setVersion(version);
}

static const struct v2smap {
    JSVersion   version;
    const char  *string;
} v2smap[] = {
    {JSVERSION_ECMA_3,  "ECMAv3"},
    {JSVERSION_1_6,     "1.6"},
    {JSVERSION_1_7,     "1.7"},
    {JSVERSION_1_8,     "1.8"},
    {JSVERSION_ECMA_5,  "ECMAv5"},
    {JSVERSION_DEFAULT, js_default_str},
    {JSVERSION_DEFAULT, "1.0"},
    {JSVERSION_DEFAULT, "1.1"},
    {JSVERSION_DEFAULT, "1.2"},
    {JSVERSION_DEFAULT, "1.3"},
    {JSVERSION_DEFAULT, "1.4"},
    {JSVERSION_DEFAULT, "1.5"},
    {JSVERSION_UNKNOWN, nullptr},          /* must be last, nullptr is sentinel */
};

JS_PUBLIC_API(const char *)
JS_VersionToString(JSVersion version)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (v2smap[i].version == version)
            return v2smap[i].string;
    return "unknown";
}

JS_PUBLIC_API(JSVersion)
JS_StringToVersion(const char *string)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (strcmp(v2smap[i].string, string) == 0)
            return v2smap[i].version;
    return JSVERSION_UNKNOWN;
}

JS_PUBLIC_API(JS::ContextOptions &)
JS::ContextOptionsRef(JSContext *cx)
{
    return cx->options();
}

JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void)
{
    return "JavaScript-C" MOZILLA_VERSION;
}

JS_PUBLIC_API(void)
JS_SetDestroyCompartmentCallback(JSRuntime *rt, JSDestroyCompartmentCallback callback)
{
    rt->destroyCompartmentCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetDestroyZoneCallback(JSRuntime *rt, JSZoneCallback callback)
{
    rt->destroyZoneCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetSweepZoneCallback(JSRuntime *rt, JSZoneCallback callback)
{
    rt->sweepZoneCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetCompartmentNameCallback(JSRuntime *rt, JSCompartmentNameCallback callback)
{
    rt->compartmentNameCallback = callback;
}

JS_PUBLIC_API(void)
JS_SetWrapObjectCallbacks(JSRuntime *rt, const JSWrapObjectCallbacks *callbacks)
{
    rt->wrapObjectCallbacks = callbacks;
}

JS_PUBLIC_API(JSCompartment *)
JS_EnterCompartment(JSContext *cx, JSObject *target)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    JSCompartment *oldCompartment = cx->compartment();
    cx->enterCompartment(target->compartment());
    return oldCompartment;
}

JS_PUBLIC_API(JSCompartment *)
JS_EnterCompartmentOfScript(JSContext *cx, JSScript *target)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    GlobalObject &global = target->global();
    return JS_EnterCompartment(cx, &global);
}

JS_PUBLIC_API(void)
JS_LeaveCompartment(JSContext *cx, JSCompartment *oldCompartment)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    cx->leaveCompartment(oldCompartment);
}

JSAutoCompartment::JSAutoCompartment(JSContext *cx, JSObject *target)
  : cx_(cx),
    oldCompartment_(cx->compartment())
{
    AssertHeapIsIdleOrIterating(cx_);
    cx_->enterCompartment(target->compartment());
}

JSAutoCompartment::JSAutoCompartment(JSContext *cx, JSScript *target)
  : cx_(cx),
    oldCompartment_(cx->compartment())
{
    AssertHeapIsIdleOrIterating(cx_);
    cx_->enterCompartment(target->compartment());
}

JSAutoCompartment::~JSAutoCompartment()
{
    cx_->leaveCompartment(oldCompartment_);
}

JS_PUBLIC_API(void)
JS_SetCompartmentPrivate(JSCompartment *compartment, void *data)
{
    compartment->data = data;
}

JS_PUBLIC_API(void *)
JS_GetCompartmentPrivate(JSCompartment *compartment)
{
    return compartment->data;
}

JS_PUBLIC_API(void)
JS_SetZoneUserData(JS::Zone *zone, void *data)
{
    zone->data = data;
}

JS_PUBLIC_API(void *)
JS_GetZoneUserData(JS::Zone *zone)
{
    return zone->data;
}

JS_PUBLIC_API(bool)
JS_WrapObject(JSContext *cx, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (objp)
        JS::ExposeGCThingToActiveJS(objp, JSTRACE_OBJECT);
    return cx->compartment()->wrap(cx, objp);
}

JS_PUBLIC_API(bool)
JS_WrapValue(JSContext *cx, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JS::ExposeValueToActiveJS(vp);
    return cx->compartment()->wrap(cx, vp);
}

JS_PUBLIC_API(bool)
JS_WrapId(JSContext *cx, jsid *idp)
{
  AssertHeapIsIdle(cx);
  CHECK_REQUEST(cx);
  if (idp) {
      jsid id = *idp;
      if (JSID_IS_STRING(id))
          JS::ExposeGCThingToActiveJS(JSID_TO_STRING(id), JSTRACE_STRING);
      else if (JSID_IS_OBJECT(id))
          JS::ExposeGCThingToActiveJS(JSID_TO_OBJECT(id), JSTRACE_OBJECT);
  }
  return cx->compartment()->wrapId(cx, idp);
}

/*
 * Identity remapping. Not for casual consumers.
 *
 * Normally, an object's contents and its identity are inextricably linked.
 * Identity is determined by the address of the JSObject* in the heap, and
 * the contents are what is located at that address. Transplanting allows these
 * concepts to be separated through a combination of swapping (exchanging the
 * contents of two same-compartment objects) and remapping cross-compartment
 * identities by altering wrappers.
 *
 * The |origobj| argument should be the object whose identity needs to be
 * remapped, usually to another compartment. The contents of |origobj| are
 * destroyed.
 *
 * The |target| argument serves two purposes:
 *
 * First, |target| serves as a hint for the new identity of the object. The new
 * identity object will always be in the same compartment as |target|, but
 * if that compartment already had an object representing |origobj| (either a
 * cross-compartment wrapper for it, or |origobj| itself if the two arguments
 * are same-compartment), the existing object is used. Otherwise, |target|
 * itself is used. To avoid ambiguity, JS_TransplantObject always returns the
 * new identity.
 *
 * Second, the new identity object's contents will be those of |target|. A swap()
 * is used to make this happen if an object other than |target| is used.
 *
 * We don't have a good way to recover from failure in this function, so
 * we intentionally crash instead.
 */

JS_PUBLIC_API(JSObject *)
JS_TransplantObject(JSContext *cx, HandleObject origobj, HandleObject target)
{
    AssertHeapIsIdle(cx);
    JS_ASSERT(origobj != target);
    JS_ASSERT(!origobj->is<CrossCompartmentWrapperObject>());
    JS_ASSERT(!target->is<CrossCompartmentWrapperObject>());

    AutoMaybeTouchDeadZones agc(cx);
    AutoDisableProxyCheck adpc(cx->runtime());

    JSCompartment *destination = target->compartment();
    RootedValue origv(cx, ObjectValue(*origobj));
    RootedObject newIdentity(cx);

    if (origobj->compartment() == destination) {
        // If the original object is in the same compartment as the
        // destination, then we know that we won't find a wrapper in the
        // destination's cross compartment map and that the same
        // object will continue to work.
        if (!JSObject::swap(cx, origobj, target))
            MOZ_CRASH();
        newIdentity = origobj;
    } else if (WrapperMap::Ptr p = destination->lookupWrapper(origv)) {
        // There might already be a wrapper for the original object in
        // the new compartment. If there is, we use its identity and swap
        // in the contents of |target|.
        newIdentity = &p->value().toObject();

        // When we remove origv from the wrapper map, its wrapper, newIdentity,
        // must immediately cease to be a cross-compartment wrapper. Neuter it.
        destination->removeWrapper(p);
        NukeCrossCompartmentWrapper(cx, newIdentity);

        if (!JSObject::swap(cx, newIdentity, target))
            MOZ_CRASH();
    } else {
        // Otherwise, we use |target| for the new identity object.
        newIdentity = target;
    }

    // Now, iterate through other scopes looking for references to the
    // old object, and update the relevant cross-compartment wrappers.
    if (!RemapAllWrappersForObject(cx, origobj, newIdentity))
        MOZ_CRASH();

    // Lastly, update the original object to point to the new one.
    if (origobj->compartment() != destination) {
        RootedObject newIdentityWrapper(cx, newIdentity);
        AutoCompartment ac(cx, origobj);
        if (!JS_WrapObject(cx, &newIdentityWrapper))
            MOZ_CRASH();
        JS_ASSERT(Wrapper::wrappedObject(newIdentityWrapper) == newIdentity);
        if (!JSObject::swap(cx, origobj, newIdentityWrapper))
            MOZ_CRASH();
        origobj->compartment()->putWrapper(cx, ObjectValue(*newIdentity), origv);
    }

    // The new identity object might be one of several things. Return it to avoid
    // ambiguity.
    return newIdentity;
}

/*
 * Recompute all cross-compartment wrappers for an object, resetting state.
 * Gecko uses this to clear Xray wrappers when doing a navigation that reuses
 * the inner window and global object.
 */
JS_PUBLIC_API(bool)
JS_RefreshCrossCompartmentWrappers(JSContext *cx, HandleObject obj)
{
    return RemapAllWrappersForObject(cx, obj, obj);
}

JS_PUBLIC_API(bool)
JS_InitStandardClasses(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    cx->setDefaultCompartmentObjectIfUnset(obj);
    assertSameCompartment(cx, obj);

    Rooted<GlobalObject*> global(cx, &obj->global());
    return GlobalObject::initStandardClasses(cx, global);
}

#define CLASP(name)                 (&name##Class)
#define OCLASP(name)                (&name##Object::class_)
#define TYPED_ARRAY_CLASP(type)     (&TypedArrayObject::classes[ScalarTypeDescr::type])
#define EAGER_ATOM(name)            NAME_OFFSET(name)
#define EAGER_CLASS_ATOM(name)      NAME_OFFSET(name)

static js::Class DummyClass;
static js::Class SentinelClass;

typedef struct JSStdName {
    size_t      atomOffset;     /* offset of atom pointer in JSAtomState */
    const Class *clasp;
    bool isDummy() const { return clasp == &DummyClass; };
    bool isSentinel() const { return clasp == &SentinelClass; };
} JSStdName;

static const JSStdName*
LookupStdName(JSRuntime *rt, HandleString name, const JSStdName *table)
{
    MOZ_ASSERT(name->isAtom());
    for (unsigned i = 0; !table[i].isSentinel(); i++) {
        if (table[i].isDummy())
            continue;
        JSAtom *atom = AtomStateOffsetToName(*rt->commonNames, table[i].atomOffset);
        MOZ_ASSERT(atom);
        if (name == atom)
            return &table[i];
    }

    return nullptr;
}

/*
 * Table of standard classes, indexed by JSProtoKey. For entries where the
 * JSProtoKey does not correspond to a class with a meaningful constructor, we
 * insert a null entry into the table.
 */
#define STD_NAME_ENTRY(name, code, init, clasp) { EAGER_CLASS_ATOM(name), clasp },
#define STD_DUMMY_ENTRY(name, code, init, dummy) { 0, &DummyClass },
static const JSStdName standard_class_names[] = {
  JS_FOR_PROTOTYPES(STD_NAME_ENTRY, STD_DUMMY_ENTRY)
  { 0, &SentinelClass }
};

/*
 * Table of top-level function and constant names and the init function of the
 * corresponding standard class that sets them up.
 */
static const JSStdName builtin_property_names[] = {
    { EAGER_ATOM(eval), &JSObject::class_ },

    /* Global properties and functions defined by the Number class. */
    { EAGER_ATOM(NaN), OCLASP(Number) },
    { EAGER_ATOM(Infinity), OCLASP(Number) },
    { EAGER_ATOM(isNaN), OCLASP(Number) },
    { EAGER_ATOM(isFinite), OCLASP(Number) },
    { EAGER_ATOM(parseFloat), OCLASP(Number) },
    { EAGER_ATOM(parseInt), OCLASP(Number) },

    /* String global functions. */
    { EAGER_ATOM(escape), OCLASP(String) },
    { EAGER_ATOM(unescape), OCLASP(String) },
    { EAGER_ATOM(decodeURI), OCLASP(String) },
    { EAGER_ATOM(encodeURI), OCLASP(String) },
    { EAGER_ATOM(decodeURIComponent), OCLASP(String) },
    { EAGER_ATOM(encodeURIComponent), OCLASP(String) },
#if JS_HAS_UNEVAL
    { EAGER_ATOM(uneval), OCLASP(String) },
#endif
#ifdef ENABLE_BINARYDATA
    { EAGER_ATOM(SIMD), OCLASP(SIMD) },
    { EAGER_ATOM(TypedObject), OCLASP(TypedObjectModule) },
#endif

    { 0, &SentinelClass }
};

static const JSStdName object_prototype_names[] = {
    /* Object.prototype properties (global delegates to Object.prototype). */
    { EAGER_ATOM(proto), &JSObject::class_ },
#if JS_HAS_TOSOURCE
    { EAGER_ATOM(toSource), &JSObject::class_ },
#endif
    { EAGER_ATOM(toString), &JSObject::class_ },
    { EAGER_ATOM(toLocaleString), &JSObject::class_ },
    { EAGER_ATOM(valueOf), &JSObject::class_ },
#if JS_HAS_OBJ_WATCHPOINT
    { EAGER_ATOM(watch), &JSObject::class_ },
    { EAGER_ATOM(unwatch), &JSObject::class_ },
#endif
    { EAGER_ATOM(hasOwnProperty), &JSObject::class_ },
    { EAGER_ATOM(isPrototypeOf), &JSObject::class_ },
    { EAGER_ATOM(propertyIsEnumerable), &JSObject::class_ },
#if JS_OLD_GETTER_SETTER_METHODS
    { EAGER_ATOM(defineGetter), &JSObject::class_ },
    { EAGER_ATOM(defineSetter), &JSObject::class_ },
    { EAGER_ATOM(lookupGetter), &JSObject::class_ },
    { EAGER_ATOM(lookupSetter), &JSObject::class_ },
#endif

    { 0, &SentinelClass }
};

#undef CLASP
#undef TYPED_ARRAY_CLASP
#undef EAGER_ATOM
#undef EAGER_CLASS_ATOM
#undef EAGER_ATOM_CLASP

JS_PUBLIC_API(bool)
JS_ResolveStandardClass(JSContext *cx, HandleObject obj, HandleId id, bool *resolved)
{
    JSRuntime *rt;
    const JSStdName *stdnm;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    JS_ASSERT(obj->is<GlobalObject>());
    *resolved = false;

    rt = cx->runtime();
    if (!rt->hasContexts() || !JSID_IS_ATOM(id))
        return true;

    RootedString idstr(cx, JSID_TO_STRING(id));

    /* Check whether we're resolving 'undefined', and define it if so. */
    JSAtom *undefinedAtom = cx->names().undefined;
    if (idstr == undefinedAtom) {
        *resolved = true;
        return JSObject::defineProperty(cx, obj, undefinedAtom->asPropertyName(),
                                        UndefinedHandleValue,
                                        JS_PropertyStub, JS_StrictPropertyStub,
                                        JSPROP_PERMANENT | JSPROP_READONLY);
    }

    /* Try for class constructors/prototypes named by well-known atoms. */
    stdnm = LookupStdName(rt, idstr, standard_class_names);

    /* Try less frequently used top-level functions and constants. */
    if (!stdnm)
        stdnm = LookupStdName(rt, idstr, builtin_property_names);

    /*
     * Try even less frequently used names delegated from the global
     * object to Object.prototype, but only if the Object class hasn't
     * yet been initialized.
     */
    if (!stdnm) {
        RootedObject proto(cx);
        if (!JSObject::getProto(cx, obj, &proto))
            return false;
        if (!proto)
            stdnm = LookupStdName(rt, idstr, object_prototype_names);
    }

    if (stdnm) {
        /*
         * If this standard class is anonymous, then we don't want to resolve
         * by name.
         */
        if (stdnm->clasp->flags & JSCLASS_IS_ANONYMOUS)
            return true;

        Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
        JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(stdnm->clasp);
        if (!GlobalObject::ensureConstructor(cx, global, key))
            return false;

        *resolved = true;
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_EnumerateStandardClasses(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    MOZ_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    return GlobalObject::initStandardClasses(cx, global);
}

JS_PUBLIC_API(bool)
JS_GetClassObject(JSContext *cx, JSProtoKey key, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_GetClassObject(cx, key, objp);
}

JS_PUBLIC_API(bool)
JS_GetClassPrototype(JSContext *cx, JSProtoKey key, MutableHandleObject objp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_GetClassPrototype(cx, key, objp);
}

extern JS_PUBLIC_API(JSProtoKey)
JS_IdToProtoKey(JSContext *cx, HandleId id)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (!JSID_IS_ATOM(id))
        return JSProto_Null;
    RootedString idstr(cx, JSID_TO_STRING(id));
    const JSStdName *stdnm = LookupStdName(cx->runtime(), idstr, standard_class_names);
    if (!stdnm)
        return JSProto_Null;

    MOZ_ASSERT(MOZ_ARRAY_LENGTH(standard_class_names) == JSProto_LIMIT + 1);
    return static_cast<JSProtoKey>(stdnm - standard_class_names);
}

JS_PUBLIC_API(JSObject *)
JS_GetObjectPrototype(JSContext *cx, HandleObject forObj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, forObj);
    return forObj->global().getOrCreateObjectPrototype(cx);
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionPrototype(JSContext *cx, HandleObject forObj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, forObj);
    return forObj->global().getOrCreateFunctionPrototype(cx);
}

JS_PUBLIC_API(JSObject *)
JS_GetArrayPrototype(JSContext *cx, HandleObject forObj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, forObj);
    Rooted<GlobalObject*> global(cx, &forObj->global());
    return GlobalObject::getOrCreateArrayPrototype(cx, global);
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalForObject(JSContext *cx, JSObject *obj)
{
    AssertHeapIsIdle(cx);
    assertSameCompartment(cx, obj);
    return &obj->global();
}

extern JS_PUBLIC_API(bool)
JS_IsGlobalObject(JSObject *obj)
{
    return obj->is<GlobalObject>();
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalForCompartmentOrNull(JSContext *cx, JSCompartment *c)
{
    AssertHeapIsIdleOrIterating(cx);
    assertSameCompartment(cx, c);
    return c->maybeGlobal();
}

JS_PUBLIC_API(JSObject *)
JS::CurrentGlobalOrNull(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx);
    CHECK_REQUEST(cx);
    if (!cx->compartment())
        return nullptr;
    return cx->global();
}

JS_PUBLIC_API(jsval)
JS_ComputeThis(JSContext *cx, jsval *vp)
{
    AssertHeapIsIdle(cx);
    assertSameCompartment(cx, JSValueArray(vp, 2));
    CallReceiver call = CallReceiverFromVp(vp);
    if (!BoxNonStrictThis(cx, call))
        return JSVAL_NULL;
    return call.thisv();
}

JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return cx->malloc_(nbytes);
}

JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return cx->realloc_(p, nbytes);
}

JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p)
{
    return js_free(p);
}

JS_PUBLIC_API(void)
JS_freeop(JSFreeOp *fop, void *p)
{
    return FreeOp::get(fop)->free_(p);
}

JS_PUBLIC_API(JSFreeOp *)
JS_GetDefaultFreeOp(JSRuntime *rt)
{
    return rt->defaultFreeOp();
}

JS_PUBLIC_API(void)
JS_updateMallocCounter(JSContext *cx, size_t nbytes)
{
    return cx->runtime()->updateMallocCounter(cx->zone(), nbytes);
}

JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s)
{
    AssertHeapIsIdle(cx);
    return js_strdup(cx, s);
}

JS_PUBLIC_API(char *)
JS_strdup(JSRuntime *rt, const char *s)
{
    AssertHeapIsIdle(rt);
    size_t n = strlen(s) + 1;
    void *p = rt->malloc_(n);
    if (!p)
        return nullptr;
    return static_cast<char*>(js_memcpy(p, s, n));
}

#undef JS_AddRoot

JS_PUBLIC_API(bool)
JS_AddValueRoot(JSContext *cx, jsval *vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddValueRoot(cx, vp, nullptr);
}

JS_PUBLIC_API(bool)
JS_AddStringRoot(JSContext *cx, JSString **rp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddStringRoot(cx, rp, nullptr);
}

JS_PUBLIC_API(bool)
JS_AddObjectRoot(JSContext *cx, JSObject **rp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddObjectRoot(cx, rp, nullptr);
}

JS_PUBLIC_API(bool)
JS_AddNamedValueRoot(JSContext *cx, jsval *vp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddValueRoot(cx, vp, name);
}

JS_PUBLIC_API(bool)
JS_AddNamedValueRootRT(JSRuntime *rt, jsval *vp, const char *name)
{
    return AddValueRootRT(rt, vp, name);
}

JS_PUBLIC_API(bool)
JS_AddNamedStringRoot(JSContext *cx, JSString **rp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddStringRoot(cx, rp, name);
}

JS_PUBLIC_API(bool)
JS_AddNamedObjectRoot(JSContext *cx, JSObject **rp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddObjectRoot(cx, rp, name);
}

JS_PUBLIC_API(bool)
JS_AddNamedScriptRoot(JSContext *cx, JSScript **rp, const char *name)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return AddScriptRoot(cx, rp, name);
}

/* We allow unrooting from finalizers within the GC */

JS_PUBLIC_API(void)
JS_RemoveValueRoot(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)vp);
}

JS_PUBLIC_API(void)
JS_RemoveStringRoot(JSContext *cx, JSString **rp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)rp);
}

JS_PUBLIC_API(void)
JS_RemoveObjectRoot(JSContext *cx, JSObject **rp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)rp);
}

JS_PUBLIC_API(void)
JS_RemoveScriptRoot(JSContext *cx, JSScript **rp)
{
    CHECK_REQUEST(cx);
    RemoveRoot(cx->runtime(), (void *)rp);
}

JS_PUBLIC_API(void)
JS_RemoveValueRootRT(JSRuntime *rt, jsval *vp)
{
    RemoveRoot(rt, (void *)vp);
}

JS_PUBLIC_API(void)
JS_RemoveStringRootRT(JSRuntime *rt, JSString **rp)
{
    RemoveRoot(rt, (void *)rp);
}

JS_PUBLIC_API(void)
JS_RemoveObjectRootRT(JSRuntime *rt, JSObject **rp)
{
    RemoveRoot(rt, (void *)rp);
}

JS_PUBLIC_API(void)
JS_RemoveScriptRootRT(JSRuntime *rt, JSScript **rp)
{
    RemoveRoot(rt, (void *)rp);
}

JS_PUBLIC_API(bool)
JS_AddExtraGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data)
{
    AssertHeapIsIdle(rt);
    return !!rt->gcBlackRootTracers.append(JSRuntime::ExtraTracer(traceOp, data));
}

JS_PUBLIC_API(void)
JS_RemoveExtraGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data)
{
    AssertHeapIsIdle(rt);
    for (size_t i = 0; i < rt->gcBlackRootTracers.length(); i++) {
        JSRuntime::ExtraTracer *e = &rt->gcBlackRootTracers[i];
        if (e->op == traceOp && e->data == data) {
            rt->gcBlackRootTracers.erase(e);
            break;
        }
    }
}

#ifdef DEBUG

typedef struct JSHeapDumpNode JSHeapDumpNode;

struct JSHeapDumpNode {
    void            *thing;
    JSGCTraceKind   kind;
    JSHeapDumpNode  *next;          /* next sibling */
    JSHeapDumpNode  *parent;        /* node with the thing that refer to thing
                                       from this node */
    char            edgeName[1];    /* name of the edge from parent->thing
                                       into thing */
};

typedef HashSet<void *, PointerHasher<void *, 3>, SystemAllocPolicy> VisitedSet;

typedef struct JSDumpingTracer {
    JSTracer            base;
    VisitedSet          visited;
    bool                ok;
    void                *startThing;
    void                *thingToFind;
    void                *thingToIgnore;
    JSHeapDumpNode      *parentNode;
    JSHeapDumpNode      **lastNodep;
    char                buffer[200];
} JSDumpingTracer;

static void
DumpNotify(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JS_ASSERT(trc->callback == DumpNotify);

    JSDumpingTracer *dtrc = (JSDumpingTracer *)trc;
    void *thing = *thingp;

    if (!dtrc->ok || thing == dtrc->thingToIgnore)
        return;

    /*
     * Check if we have already seen thing unless it is thingToFind to include
     * it to the graph each time we reach it and print all live things that
     * refer to thingToFind.
     *
     * This does not print all possible paths leading to thingToFind since
     * when a thing A refers directly or indirectly to thingToFind and A is
     * present several times in the graph, we will print only the first path
     * leading to A and thingToFind, other ways to reach A will be ignored.
     */
    if (dtrc->thingToFind != thing) {
        /*
         * The startThing check allows to avoid putting startThing into the
         * hash table before tracing startThing in JS_DumpHeap.
         */
        if (thing == dtrc->startThing)
            return;
        VisitedSet::AddPtr p = dtrc->visited.lookupForAdd(thing);
        if (p)
            return;
        if (!dtrc->visited.add(p, thing)) {
            dtrc->ok = false;
            return;
        }
    }

    const char *edgeName = JS_GetTraceEdgeName(&dtrc->base, dtrc->buffer, sizeof(dtrc->buffer));
    size_t edgeNameSize = strlen(edgeName) + 1;
    size_t bytes = offsetof(JSHeapDumpNode, edgeName) + edgeNameSize;
    JSHeapDumpNode *node = (JSHeapDumpNode *) js_malloc(bytes);
    if (!node) {
        dtrc->ok = false;
        return;
    }

    node->thing = thing;
    node->kind = kind;
    node->next = nullptr;
    node->parent = dtrc->parentNode;
    js_memcpy(node->edgeName, edgeName, edgeNameSize);

    JS_ASSERT(!*dtrc->lastNodep);
    *dtrc->lastNodep = node;
    dtrc->lastNodep = &node->next;
}

/* Dump node and the chain that leads to thing it contains. */
static bool
DumpNode(JSDumpingTracer *dtrc, FILE* fp, JSHeapDumpNode *node)
{
    JSHeapDumpNode *prev, *following;
    size_t chainLimit;
    enum { MAX_PARENTS_TO_PRINT = 10 };

    JS_GetTraceThingInfo(dtrc->buffer, sizeof dtrc->buffer,
                         &dtrc->base, node->thing, node->kind, true);
    if (fprintf(fp, "%p %-22s via ", node->thing, dtrc->buffer) < 0)
        return false;

    /*
     * We need to print the parent chain in the reverse order. To do it in
     * O(N) time where N is the chain length we first reverse the chain while
     * searching for the top and then print each node while restoring the
     * chain order.
     */
    chainLimit = MAX_PARENTS_TO_PRINT;
    prev = nullptr;
    for (;;) {
        following = node->parent;
        node->parent = prev;
        prev = node;
        node = following;
        if (!node)
            break;
        if (chainLimit == 0) {
            if (fputs("...", fp) < 0)
                return false;
            break;
        }
        --chainLimit;
    }

    node = prev;
    prev = following;
    bool ok = true;
    do {
        /* Loop must continue even when !ok to restore the parent chain. */
        if (ok) {
            if (!prev) {
                /* Print edge from some runtime root or startThing. */
                if (fputs(node->edgeName, fp) < 0)
                    ok = false;
            } else {
                JS_GetTraceThingInfo(dtrc->buffer, sizeof dtrc->buffer,
                                     &dtrc->base, prev->thing, prev->kind,
                                     false);
                if (fprintf(fp, "(%p %s).%s",
                           prev->thing, dtrc->buffer, node->edgeName) < 0) {
                    ok = false;
                }
            }
        }
        following = node->parent;
        node->parent = prev;
        prev = node;
        node = following;
    } while (node);

    return ok && putc('\n', fp) >= 0;
}

JS_PUBLIC_API(bool)
JS_DumpHeap(JSRuntime *rt, FILE *fp, void* startThing, JSGCTraceKind startKind,
            void *thingToFind, size_t maxDepth, void *thingToIgnore)
{
    if (maxDepth == 0)
        return true;

    JSDumpingTracer dtrc;
    if (!dtrc.visited.init())
        return false;
    JS_TracerInit(&dtrc.base, rt, DumpNotify);
    dtrc.ok = true;
    dtrc.startThing = startThing;
    dtrc.thingToFind = thingToFind;
    dtrc.thingToIgnore = thingToIgnore;
    dtrc.parentNode = nullptr;
    JSHeapDumpNode *node = nullptr;
    dtrc.lastNodep = &node;
    if (!startThing) {
        JS_ASSERT(startKind == JSTRACE_OBJECT);
        TraceRuntime(&dtrc.base);
    } else {
        JS_TraceChildren(&dtrc.base, startThing, startKind);
    }

    if (!node)
        return dtrc.ok;

    size_t depth = 1;
    JSHeapDumpNode *children, *next, *parent;
    bool thingToFindWasTraced = thingToFind && thingToFind == startThing;
    for (;;) {
        /*
         * Loop must continue even when !dtrc.ok to free all nodes allocated
         * so far.
         */
        if (dtrc.ok) {
            if (thingToFind == nullptr || thingToFind == node->thing)
                dtrc.ok = DumpNode(&dtrc, fp, node);

            /* Descend into children. */
            if (dtrc.ok &&
                depth < maxDepth &&
                (thingToFind != node->thing || !thingToFindWasTraced)) {
                dtrc.parentNode = node;
                children = nullptr;
                dtrc.lastNodep = &children;
                JS_TraceChildren(&dtrc.base, node->thing, node->kind);
                if (thingToFind == node->thing)
                    thingToFindWasTraced = true;
                if (children != nullptr) {
                    ++depth;
                    node = children;
                    continue;
                }
            }
        }

        /* Move to next or parents next and free the node. */
        for (;;) {
            next = node->next;
            parent = node->parent;
            js_free(node);
            node = next;
            if (node)
                break;
            if (!parent)
                return dtrc.ok;
            JS_ASSERT(depth > 1);
            --depth;
            node = parent;
        }
    }

    JS_ASSERT(depth == 1);
    return dtrc.ok;
}

#endif /* DEBUG */

extern JS_PUBLIC_API(bool)
JS_IsGCMarkingTracer(JSTracer *trc)
{
    return IS_GC_MARKING_TRACER(trc);
}

#ifdef DEBUG
extern JS_PUBLIC_API(bool)
JS_IsMarkingGray(JSTracer *trc)
{
    JS_ASSERT(JS_IsGCMarkingTracer(trc));
    return trc->callback == GCMarker::GrayCallback;
}
#endif

JS_PUBLIC_API(void)
JS_GC(JSRuntime *rt)
{
    AssertHeapIsIdle(rt);
    JS::PrepareForFullGC(rt);
    GC(rt, GC_NORMAL, JS::gcreason::API);
}

JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx)
{
    MaybeGC(cx);
}

JS_PUBLIC_API(void)
JS_SetGCCallback(JSRuntime *rt, JSGCCallback cb, void *data)
{
    AssertHeapIsIdle(rt);
    rt->gcCallback = cb;
    rt->gcCallbackData = data;
}

JS_PUBLIC_API(void)
JS_SetFinalizeCallback(JSRuntime *rt, JSFinalizeCallback cb)
{
    AssertHeapIsIdle(rt);
    rt->gcFinalizeCallback = cb;
}

JS_PUBLIC_API(bool)
JS_IsAboutToBeFinalized(JS::Heap<JSObject *> *objp)
{
    return IsObjectAboutToBeFinalized(objp->unsafeGet());
}

JS_PUBLIC_API(bool)
JS_IsAboutToBeFinalizedUnbarriered(JSObject **objp)
{
    return IsObjectAboutToBeFinalized(objp);
}

JS_PUBLIC_API(void)
JS_SetGCParameter(JSRuntime *rt, JSGCParamKey key, uint32_t value)
{
    switch (key) {
      case JSGC_MAX_BYTES: {
        JS_ASSERT(value >= rt->gcBytes);
        rt->gcMaxBytes = value;
        break;
      }
      case JSGC_MAX_MALLOC_BYTES:
        rt->setGCMaxMallocBytes(value);
        break;
      case JSGC_SLICE_TIME_BUDGET:
        rt->gcSliceBudget = SliceBudget::TimeBudget(value);
        break;
      case JSGC_MARK_STACK_LIMIT:
        js::SetMarkStackLimit(rt, value);
        break;
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
        rt->gcHighFrequencyTimeThreshold = value;
        break;
      case JSGC_HIGH_FREQUENCY_LOW_LIMIT:
        rt->gcHighFrequencyLowLimitBytes = value * 1024 * 1024;
        break;
      case JSGC_HIGH_FREQUENCY_HIGH_LIMIT:
        rt->gcHighFrequencyHighLimitBytes = value * 1024 * 1024;
        break;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX:
        rt->gcHighFrequencyHeapGrowthMax = value / 100.0;
        break;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN:
        rt->gcHighFrequencyHeapGrowthMin = value / 100.0;
        break;
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH:
        rt->gcLowFrequencyHeapGrowth = value / 100.0;
        break;
      case JSGC_DYNAMIC_HEAP_GROWTH:
        rt->gcDynamicHeapGrowth = value;
        break;
      case JSGC_DYNAMIC_MARK_SLICE:
        rt->gcDynamicMarkSlice = value;
        break;
      case JSGC_ALLOCATION_THRESHOLD:
        rt->gcAllocationThreshold = value * 1024 * 1024;
        break;
      case JSGC_DECOMMIT_THRESHOLD:
        rt->gcDecommitThreshold = value * 1024 * 1024;
        break;
      default:
        JS_ASSERT(key == JSGC_MODE);
        rt->setGCMode(JSGCMode(value));
        JS_ASSERT(rt->gcMode() == JSGC_MODE_GLOBAL ||
                  rt->gcMode() == JSGC_MODE_COMPARTMENT ||
                  rt->gcMode() == JSGC_MODE_INCREMENTAL);
        return;
    }
}

JS_PUBLIC_API(uint32_t)
JS_GetGCParameter(JSRuntime *rt, JSGCParamKey key)
{
    switch (key) {
      case JSGC_MAX_BYTES:
        return uint32_t(rt->gcMaxBytes);
      case JSGC_MAX_MALLOC_BYTES:
        return rt->gcMaxMallocBytes;
      case JSGC_BYTES:
        return uint32_t(rt->gcBytes);
      case JSGC_MODE:
        return uint32_t(rt->gcMode());
      case JSGC_UNUSED_CHUNKS:
        return uint32_t(rt->gcChunkPool.getEmptyCount());
      case JSGC_TOTAL_CHUNKS:
        return uint32_t(rt->gcChunkSet.count() + rt->gcChunkPool.getEmptyCount());
      case JSGC_SLICE_TIME_BUDGET:
        return uint32_t(rt->gcSliceBudget > 0 ? rt->gcSliceBudget / PRMJ_USEC_PER_MSEC : 0);
      case JSGC_MARK_STACK_LIMIT:
        return rt->gcMarker.maxCapacity();
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
        return rt->gcHighFrequencyTimeThreshold;
      case JSGC_HIGH_FREQUENCY_LOW_LIMIT:
        return rt->gcHighFrequencyLowLimitBytes / 1024 / 1024;
      case JSGC_HIGH_FREQUENCY_HIGH_LIMIT:
        return rt->gcHighFrequencyHighLimitBytes / 1024 / 1024;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX:
        return uint32_t(rt->gcHighFrequencyHeapGrowthMax * 100);
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN:
        return uint32_t(rt->gcHighFrequencyHeapGrowthMin * 100);
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH:
        return uint32_t(rt->gcLowFrequencyHeapGrowth * 100);
      case JSGC_DYNAMIC_HEAP_GROWTH:
        return rt->gcDynamicHeapGrowth;
      case JSGC_DYNAMIC_MARK_SLICE:
        return rt->gcDynamicMarkSlice;
      case JSGC_ALLOCATION_THRESHOLD:
        return rt->gcAllocationThreshold / 1024 / 1024;
      default:
        JS_ASSERT(key == JSGC_NUMBER);
        return uint32_t(rt->gcNumber);
    }
}

JS_PUBLIC_API(void)
JS_SetGCParameterForThread(JSContext *cx, JSGCParamKey key, uint32_t value)
{
    JS_ASSERT(key == JSGC_MAX_CODE_CACHE_BYTES);
}

JS_PUBLIC_API(uint32_t)
JS_GetGCParameterForThread(JSContext *cx, JSGCParamKey key)
{
    JS_ASSERT(key == JSGC_MAX_CODE_CACHE_BYTES);
    return 0;
}

static const size_t NumGCConfigs = 14;
struct JSGCConfig {
    JSGCParamKey key;
    uint32_t value;
};

JS_PUBLIC_API(void)
JS_SetGCParametersBasedOnAvailableMemory(JSRuntime *rt, uint32_t availMem)
{
    static const JSGCConfig minimal[NumGCConfigs] = {
        {JSGC_MAX_MALLOC_BYTES, 6 * 1024 * 1024},
        {JSGC_SLICE_TIME_BUDGET, 30},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_HIGH_FREQUENCY_HIGH_LIMIT, 40},
        {JSGC_HIGH_FREQUENCY_LOW_LIMIT, 0},
        {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX, 300},
        {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN, 120},
        {JSGC_LOW_FREQUENCY_HEAP_GROWTH, 120},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
        {JSGC_ALLOCATION_THRESHOLD, 1},
        {JSGC_DECOMMIT_THRESHOLD, 1},
        {JSGC_MODE, JSGC_MODE_INCREMENTAL}
    };

    const JSGCConfig *config = minimal;
    if (availMem > 512) {
        static const JSGCConfig nominal[NumGCConfigs] = {
            {JSGC_MAX_MALLOC_BYTES, 6 * 1024 * 1024},
            {JSGC_SLICE_TIME_BUDGET, 30},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1000},
            // This are the current default settings but this is likely inverted as
            // explained for the computation of Next_GC in Bug 863398 comment 21.
            {JSGC_HIGH_FREQUENCY_HIGH_LIMIT, 100},
            {JSGC_HIGH_FREQUENCY_LOW_LIMIT, 500},
            {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX, 300},
            {JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN, 150},
            {JSGC_LOW_FREQUENCY_HEAP_GROWTH, 150},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
            {JSGC_HIGH_FREQUENCY_TIME_LIMIT, 1500},
            {JSGC_ALLOCATION_THRESHOLD, 30},
            {JSGC_DECOMMIT_THRESHOLD, 32},
            {JSGC_MODE, JSGC_MODE_COMPARTMENT}
        };

        config = nominal;
    }

    for (size_t i = 0; i < NumGCConfigs; i++)
        JS_SetGCParameter(rt, config[i].key, config[i].value);
}


JS_PUBLIC_API(JSString *)
JS_NewExternalString(JSContext *cx, const jschar *chars, size_t length,
                     const JSStringFinalizer *fin)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSString *s = JSExternalString::new_(cx, chars, length, fin);
    return s;
}

extern JS_PUBLIC_API(bool)
JS_IsExternalString(JSString *str)
{
    return str->isExternal();
}

extern JS_PUBLIC_API(const JSStringFinalizer *)
JS_GetExternalStringFinalizer(JSString *str)
{
    return str->asExternal().externalFinalizer();
}

static void
SetNativeStackQuota(JSRuntime *rt, StackKind kind, size_t stackSize)
{
    rt->nativeStackQuota[kind] = stackSize;
    if (rt->nativeStackBase)
        RecomputeStackLimit(rt, kind);
}

void
js::RecomputeStackLimit(JSRuntime *rt, StackKind kind)
{
    size_t stackSize = rt->nativeStackQuota[kind];
#if JS_STACK_GROWTH_DIRECTION > 0
    if (stackSize == 0) {
        rt->mainThread.nativeStackLimit[kind] = UINTPTR_MAX;
    } else {
        JS_ASSERT(rt->nativeStackBase <= size_t(-1) - stackSize);
        rt->mainThread.nativeStackLimit[kind] =
          rt->nativeStackBase + stackSize - 1;
    }
#else
    if (stackSize == 0) {
        rt->mainThread.nativeStackLimit[kind] = 0;
    } else {
        JS_ASSERT(rt->nativeStackBase >= stackSize);
        rt->mainThread.nativeStackLimit[kind] =
          rt->nativeStackBase - (stackSize - 1);
    }
#endif

    // If there's no pending interrupt request set on the runtime's main thread's
    // jitStackLimit, then update it so that it reflects the new nativeStacklimit.
    //
    // Note that, for now, we use the untrusted limit for ion. This is fine,
    // because it's the most conservative limit, and if we hit it, we'll bail
    // out of ion into the interpeter, which will do a proper recursion check.
#ifdef JS_ION
    if (kind == StackForUntrustedScript) {
        JSRuntime::AutoLockForOperationCallback lock(rt);
        if (rt->mainThread.jitStackLimit != uintptr_t(-1)) {
            rt->mainThread.jitStackLimit = rt->mainThread.nativeStackLimit[kind];
#ifdef JS_ARM_SIMULATOR
            rt->mainThread.jitStackLimit = jit::Simulator::StackLimit();
#endif
        }
    }
#endif
}

JS_PUBLIC_API(void)
JS_SetNativeStackQuota(JSRuntime *rt, size_t systemCodeStackSize,
                       size_t trustedScriptStackSize,
                       size_t untrustedScriptStackSize)
{
    JS_ASSERT_IF(trustedScriptStackSize,
                 trustedScriptStackSize < systemCodeStackSize);
    if (!trustedScriptStackSize)
        trustedScriptStackSize = systemCodeStackSize;
    JS_ASSERT_IF(untrustedScriptStackSize,
                 untrustedScriptStackSize < trustedScriptStackSize);
    if (!untrustedScriptStackSize)
        untrustedScriptStackSize = trustedScriptStackSize;
    SetNativeStackQuota(rt, StackForSystemCode, systemCodeStackSize);
    SetNativeStackQuota(rt, StackForTrustedScript, trustedScriptStackSize);
    SetNativeStackQuota(rt, StackForUntrustedScript, untrustedScriptStackSize);
}

/************************************************************************/

JS_PUBLIC_API(int)
JS_IdArrayLength(JSContext *cx, JSIdArray *ida)
{
    return ida->length;
}

JS_PUBLIC_API(jsid)
JS_IdArrayGet(JSContext *cx, JSIdArray *ida, int index)
{
    JS_ASSERT(index >= 0 && index < ida->length);
    return ida->vector[index];
}

JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida)
{
    cx->runtime()->defaultFreeOp()->free_(ida);
}

JS_PUBLIC_API(bool)
JS_ValueToId(JSContext *cx, jsval valueArg, MutableHandleId idp)
{
    RootedValue value(cx, valueArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    return ValueToId<CanGC>(cx, value, idp);
}

JS_PUBLIC_API(bool)
JS_IdToValue(JSContext *cx, jsid id, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    vp.set(IdToValue(id));
    assertSameCompartment(cx, vp);
    return true;
}

JS_PUBLIC_API(bool)
JS_DefaultValue(JSContext *cx, HandleObject obj, JSType hint, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JS_ASSERT(obj != nullptr);
    JS_ASSERT(hint == JSTYPE_VOID || hint == JSTYPE_STRING || hint == JSTYPE_NUMBER);
    return JSObject::defaultValue(cx, obj, hint, vp);
}

JS_PUBLIC_API(bool)
JS_PropertyStub(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_StrictPropertyStub(JSContext *cx, HandleObject obj, HandleId id, bool strict, MutableHandleValue vp)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_DeletePropertyStub(JSContext *cx, HandleObject obj, HandleId id, bool *succeeded)
{
    *succeeded = true;
    return true;
}

JS_PUBLIC_API(bool)
JS_EnumerateStub(JSContext *cx, HandleObject obj)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_ResolveStub(JSContext *cx, HandleObject obj, HandleId id)
{
    return true;
}

JS_PUBLIC_API(bool)
JS_ConvertStub(JSContext *cx, HandleObject obj, JSType type, MutableHandleValue vp)
{
    JS_ASSERT(type != JSTYPE_OBJECT && type != JSTYPE_FUNCTION);
    JS_ASSERT(obj);
    return DefaultValue(cx, obj, type, vp);
}

JS_PUBLIC_API(JSObject *)
JS_InitClass(JSContext *cx, HandleObject obj, HandleObject parent_proto,
             const JSClass *clasp, JSNative constructor, unsigned nargs,
             const JSPropertySpec *ps, const JSFunctionSpec *fs,
             const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, parent_proto);
    return js_InitClass(cx, obj, parent_proto, Valueify(clasp), constructor,
                        nargs, ps, fs, static_ps, static_fs);
}

JS_PUBLIC_API(bool)
JS_LinkConstructorAndPrototype(JSContext *cx, HandleObject ctor, HandleObject proto)
{
    return LinkConstructorAndPrototype(cx, ctor, proto);
}

JS_PUBLIC_API(const JSClass *)
JS_GetClass(JSObject *obj)
{
    return obj->getJSClass();
}

JS_PUBLIC_API(bool)
JS_InstanceOf(JSContext *cx, HandleObject obj, const JSClass *clasp, jsval *argv)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
#ifdef DEBUG
    if (argv) {
        assertSameCompartment(cx, obj);
        assertSameCompartment(cx, JSValueArray(argv - 2, 2));
    }
#endif
    if (!obj || obj->getJSClass() != clasp) {
        if (argv)
            ReportIncompatibleMethod(cx, CallReceiverFromArgv(argv), Valueify(clasp));
        return false;
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_HasInstance(JSContext *cx, HandleObject obj, HandleValue value, bool *bp)
{
    AssertHeapIsIdle(cx);
    assertSameCompartment(cx, obj, value);
    return HasInstance(cx, obj, value, bp);
}

JS_PUBLIC_API(void *)
JS_GetPrivate(JSObject *obj)
{
    /* This function can be called by a finalizer. */
    return obj->getPrivate();
}

JS_PUBLIC_API(void)
JS_SetPrivate(JSObject *obj, void *data)
{
    /* This function can be called by a finalizer. */
    obj->setPrivate(data);
}

JS_PUBLIC_API(void *)
JS_GetInstancePrivate(JSContext *cx, HandleObject obj, const JSClass *clasp, jsval *argv)
{
    if (!JS_InstanceOf(cx, obj, clasp, argv))
        return nullptr;
    return obj->getPrivate();
}

JS_PUBLIC_API(bool)
JS_GetPrototype(JSContext *cx, JS::Handle<JSObject*> obj, JS::MutableHandle<JSObject*> protop)
{
    return JSObject::getProto(cx, obj, protop);
}

JS_PUBLIC_API(bool)
JS_SetPrototype(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<JSObject*> proto)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, proto);

    bool succeeded;
    if (!JSObject::setProto(cx, obj, proto, &succeeded))
        return false;

    if (!succeeded) {
        RootedValue val(cx, ObjectValue(*obj));
        js_ReportValueError(cx, JSMSG_SETPROTOTYPEOF_FAIL, JSDVG_IGNORE_STACK, val, js::NullPtr());
        return false;
    }

    return true;
}

JS_PUBLIC_API(JSObject *)
JS_GetParent(JSObject *obj)
{
    JS_ASSERT(!obj->is<ScopeObject>());
    return obj->getParent();
}

JS_PUBLIC_API(bool)
JS_SetParent(JSContext *cx, JSObject *objArg, JSObject *parentArg)
{
    RootedObject obj(cx, objArg);
    RootedObject parent(cx, parentArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JS_ASSERT(!obj->is<ScopeObject>());
    JS_ASSERT(parent || !obj->getParent());
    assertSameCompartment(cx, obj, parent);

    return JSObject::setParent(cx, obj, parent);
}

JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, HandleObject proto)
{
    RootedValue cval(cx);

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto);
    {
        JSAutoResolveFlags rf(cx, 0);

        if (!JSObject::getProperty(cx, proto, proto, cx->names().constructor, &cval))
            return nullptr;
    }
    if (!IsFunctionObject(cval)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                             proto->getClass()->name);
        return nullptr;
    }
    return &cval.toObject();
}

JS_PUBLIC_API(bool)
JS_GetObjectId(JSContext *cx, HandleObject obj, MutableHandleId idp)
{
    AssertHeapIsIdle(cx);
    assertSameCompartment(cx, obj);

#ifdef JSGC_GENERATIONAL
    // Ensure that the object is tenured before returning it.
    if (IsInsideNursery(cx->runtime(), obj)) {
        MinorGC(cx, JS::gcreason::EVICT_NURSERY);
        MOZ_ASSERT(!IsInsideNursery(cx->runtime(), obj));
    }
#endif

    idp.set(OBJECT_TO_JSID(obj));
    return true;
}

namespace {

class AutoHoldZone
{
  public:
    explicit AutoHoldZone(Zone *zone
                          MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : holdp(&zone->hold)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        *holdp = true;
    }

    ~AutoHoldZone() {
        *holdp = false;
    }

  private:
    bool *holdp;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* anonymous namespace */

bool
JS::CompartmentOptions::baseline(JSContext *cx) const
{
    return baselineOverride_.get(cx->options().baseline());
}

bool
JS::CompartmentOptions::typeInference(const ExclusiveContext *cx) const
{
    /* Unlike the other options that can be overriden on a per compartment
     * basis, the default value for the typeInference option is stored on the
     * compartment's type zone, rather than the current JSContext. Type zones
     * copy this default value over from the current JSContext when they are
     * created.
     */
    return typeInferenceOverride_.get(cx->compartment()->zone()->types.inferenceEnabled);
}

bool
JS::CompartmentOptions::ion(JSContext *cx) const
{
    return ionOverride_.get(cx->options().ion());
}

bool
JS::CompartmentOptions::asmJS(JSContext *cx) const
{
    return asmJSOverride_.get(cx->options().asmJS());
}

bool
JS::CompartmentOptions::cloneSingletons(JSContext *cx) const
{
    return cloneSingletonsOverride_.get(cx->options().cloneSingletons());
}

JS::CompartmentOptions &
JS::CompartmentOptions::setZone(ZoneSpecifier spec)
{
    zone_.spec = spec;
    return *this;
}

JS::CompartmentOptions &
JS::CompartmentOptions::setSameZoneAs(JSObject *obj)
{
    zone_.pointer = static_cast<void *>(obj->zone());
    return *this;
}

JS::CompartmentOptions &
JS::CompartmentOptionsRef(JSCompartment *compartment)
{
    return compartment->options();
}

JS::CompartmentOptions &
JS::CompartmentOptionsRef(JSContext *cx)
{
    return cx->compartment()->options();
}

JS_PUBLIC_API(JSObject *)
JS_NewGlobalObject(JSContext *cx, const JSClass *clasp, JSPrincipals *principals,
                   JS::OnNewGlobalHookOption hookOption,
                   const JS::CompartmentOptions &options /* = JS::CompartmentOptions() */)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    JS_ASSERT(!cx->isExceptionPending());

    JSRuntime *rt = cx->runtime();

    Zone *zone;
    if (options.zoneSpecifier() == JS::SystemZone)
        zone = rt->systemZone;
    else if (options.zoneSpecifier() == JS::FreshZone)
        zone = nullptr;
    else
        zone = static_cast<Zone *>(options.zonePointer());

    JSCompartment *compartment = NewCompartment(cx, zone, principals, options);
    if (!compartment)
        return nullptr;

    // Lazily create the system zone.
    if (!rt->systemZone && options.zoneSpecifier() == JS::SystemZone) {
        rt->systemZone = compartment->zone();
        rt->systemZone->isSystem = true;
    }

    AutoHoldZone hold(compartment->zone());

    Rooted<GlobalObject *> global(cx);
    {
        AutoCompartment ac(cx, compartment);
        global = GlobalObject::create(cx, Valueify(clasp));
    }

    if (!global)
        return nullptr;

    if (hookOption == JS::FireOnNewGlobalHook)
        JS_FireOnNewGlobalObject(cx, global);

    return global;
}

JS_PUBLIC_API(void)
JS_FireOnNewGlobalObject(JSContext *cx, JS::HandleObject global)
{
    // This hook is infallible, because we don't really want arbitrary script
    // to be able to throw errors during delicate global creation routines.
    // This infallibility will eat OOM and slow script, but if that happens
    // we'll likely run up into them again soon in a fallible context.
    Rooted<js::GlobalObject*> globalObject(cx, &global->as<GlobalObject>());
    Debugger::onNewGlobalObject(cx, globalObject);
}

JS_PUBLIC_API(JSObject *)
JS_NewObject(JSContext *cx, const JSClass *jsclasp, HandleObject proto, HandleObject parent)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent);

    const Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &JSObject::class_;    /* default class is Object */

    JS_ASSERT(clasp != &JSFunction::class_);
    JS_ASSERT(!(clasp->flags & JSCLASS_IS_GLOBAL));

    JSObject *obj = NewObjectWithClassProto(cx, clasp, proto, parent);
    JS_ASSERT_IF(obj, obj->getParent());
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectWithGivenProto(JSContext *cx, const JSClass *jsclasp, HandleObject proto, HandleObject parent)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent);

    const Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &JSObject::class_;    /* default class is Object */

    JS_ASSERT(clasp != &JSFunction::class_);
    JS_ASSERT(!(clasp->flags & JSCLASS_IS_GLOBAL));

    JSObject *obj = NewObjectWithGivenProto(cx, clasp, proto, parent);
    if (obj)
        MarkTypeObjectUnknownProperties(cx, obj->type());
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectForConstructor(JSContext *cx, const JSClass *clasp, const jsval *vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, *vp);

    RootedObject obj(cx, JSVAL_TO_OBJECT(*vp));
    return CreateThis(cx, Valueify(clasp), obj);
}

JS_PUBLIC_API(bool)
JS_IsExtensible(JSContext *cx, HandleObject obj, bool *extensible)
{
    return JSObject::isExtensible(cx, obj, extensible);
}

JS_PUBLIC_API(bool)
JS_IsNative(JSObject *obj)
{
    return obj->isNative();
}

JS_PUBLIC_API(JSRuntime *)
JS_GetObjectRuntime(JSObject *obj)
{
    return obj->compartment()->runtimeFromMainThread();
}

JS_PUBLIC_API(bool)
JS_FreezeObject(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return JSObject::freeze(cx, obj);
}

JS_PUBLIC_API(bool)
JS_DeepFreezeObject(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    /* Assume that non-extensible objects are already deep-frozen, to avoid divergence. */
    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return false;
    if (!extensible)
        return true;

    if (!JSObject::freeze(cx, obj))
        return false;

    /* Walk slots in obj and if any value is a non-null object, seal it. */
    for (uint32_t i = 0, n = obj->slotSpan(); i < n; ++i) {
        const Value &v = obj->getSlot(i);
        if (v.isPrimitive())
            continue;
        RootedObject obj(cx, &v.toObject());
        if (!JS_DeepFreezeObject(cx, obj))
            return false;
    }

    return true;
}

static bool
LookupPropertyById(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    JSAutoResolveFlags rf(cx, flags);
    return JSObject::lookupGeneric(cx, obj, id, objp, propp);
}

#define AUTO_NAMELEN(s,n)   (((n) == (size_t)-1) ? js_strlen(s) : (n))

static bool
LookupResult(JSContext *cx, HandleObject obj, HandleObject obj2, HandleId id,
             HandleShape shape, MutableHandleValue vp)
{
    if (!shape) {
        /* XXX bad API: no way to tell "not defined" from "void value" */
        vp.setUndefined();
        return true;
    }

    if (!obj2->isNative()) {
        if (obj2->is<ProxyObject>()) {
            Rooted<PropertyDescriptor> desc(cx);
            if (!Proxy::getPropertyDescriptor(cx, obj2, id, &desc, 0))
                return false;
            if (!desc.isShared()) {
                vp.set(desc.value());
                return true;
            }
        }
    } else if (IsImplicitDenseOrTypedArrayElement(shape)) {
        vp.set(obj2->getDenseOrTypedArrayElement(JSID_TO_INT(id)));
        return true;
    } else {
        /* Peek at the native property's slot value, without doing a Get. */
        if (shape->hasSlot()) {
            vp.set(obj2->nativeGetSlot(shape->slot()));
            return true;
        }
    }

    /* XXX bad API: no way to return "defined but value unknown" */
    vp.setBoolean(true);
    return true;
}

JS_PUBLIC_API(bool)
JS_LookupPropertyById(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    RootedObject obj2(cx);
    RootedShape prop(cx);

    return LookupPropertyById(cx, obj, id, 0, &obj2, &prop) &&
           LookupResult(cx, obj, obj2, id, prop, vp);
}

JS_PUBLIC_API(bool)
JS_LookupElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleValue vp)
{
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_LookupPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_LookupProperty(JSContext *cx, HandleObject objArg, const char *name, MutableHandleValue vp)
{
    RootedObject obj(cx, objArg);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    return JS_LookupPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_LookupUCProperty(JSContext *cx, HandleObject objArg, const jschar *name, size_t namelen,
                    MutableHandleValue vp)
{
    RootedObject obj(cx, objArg);
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    return JS_LookupPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_LookupPropertyWithFlagsById(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                               MutableHandleObject objp, MutableHandleValue vp)
{
    RootedShape prop(cx);

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    if (!(obj->isNative()
          ? LookupPropertyWithFlags(cx, obj, id, flags, objp, &prop)
          : JSObject::lookupGeneric(cx, obj, id, objp, &prop)))
        return false;

    return LookupResult(cx, obj, objp, id, prop, vp);
}

JS_PUBLIC_API(bool)
JS_LookupPropertyWithFlags(JSContext *cx, HandleObject obj, const char *name, unsigned flags,
                           MutableHandleValue vp)
{
    RootedObject obj2(cx);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    return JS_LookupPropertyWithFlagsById(cx, obj, id, flags, &obj2, vp);
}

JS_PUBLIC_API(bool)
JS_HasPropertyById(JSContext *cx, HandleObject obj, HandleId id, bool *foundp)
{
    RootedObject obj2(cx);
    RootedShape prop(cx);
    bool ok = LookupPropertyById(cx, obj, id, 0, &obj2, &prop);
    *foundp = (prop != nullptr);
    return ok;
}

JS_PUBLIC_API(bool)
JS_HasElement(JSContext *cx, HandleObject obj, uint32_t index, bool *foundp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_HasPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_HasProperty(JSContext *cx, HandleObject obj, const char *name, bool *foundp)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_HasPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_HasUCProperty(JSContext *cx, HandleObject obj, const jschar *name, size_t namelen, bool *foundp)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_HasPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnPropertyById(JSContext *cx, HandleObject obj, HandleId id, bool *foundp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    if (!obj->isNative()) {
        RootedObject obj2(cx);
        RootedShape prop(cx);

        if (!LookupPropertyById(cx, obj, id, 0, &obj2, &prop))
            return false;
        *foundp = (obj == obj2);
        return true;
    }

    // Check for an existing native property on the objct. Be careful not to
    // call any lookup or resolve hooks.

    if (JSID_IS_INT(id)) {
        uint32_t index = JSID_TO_INT(id);

        if (obj->containsDenseElement(index)) {
            *foundp = true;
            return true;
        }

        if (obj->is<TypedArrayObject>() && index < obj->as<TypedArrayObject>().length()) {
            *foundp = true;
            return true;
        }
    }

    *foundp = obj->nativeContains(cx, id);
    return true;
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnElement(JSContext *cx, HandleObject obj, uint32_t index, bool *foundp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_AlreadyHasOwnPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnProperty(JSContext *cx, HandleObject obj, const char *name, bool *foundp)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_AlreadyHasOwnPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(bool)
JS_AlreadyHasOwnUCProperty(JSContext *cx, HandleObject obj, const jschar *name, size_t namelen,
                           bool *foundp)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_AlreadyHasOwnPropertyById(cx, obj, id, foundp);
}

/* Wrapper functions to create wrappers with no corresponding JSJitInfo from API
 * function arguments.
 */
static JSPropertyOpWrapper
GetterWrapper(JSPropertyOp getter)
{
    JSPropertyOpWrapper ret;
    ret.op = getter;
    ret.info = nullptr;
    return ret;
}

static JSStrictPropertyOpWrapper
SetterWrapper(JSStrictPropertyOp setter)
{
    JSStrictPropertyOpWrapper ret;
    ret.op = setter;
    ret.info = nullptr;
    return ret;
}

static bool
DefinePropertyById(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                   const JSPropertyOpWrapper &get, const JSStrictPropertyOpWrapper &set,
                   unsigned attrs, unsigned flags)
{
    PropertyOp getter = get.op;
    StrictPropertyOp setter = set.op;
    /*
     * JSPROP_READONLY has no meaning when accessors are involved. Ideally we'd
     * throw if this happens, but we've accepted it for long enough that it's
     * not worth trying to make callers change their ways. Just flip it off on
     * its way through the API layer so that we can enforce this internally.
     */
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER))
        attrs &= ~JSPROP_READONLY;

    /*
     * When we use DefineProperty, we need full scriptable Function objects rather
     * than JSNatives. However, we might be pulling this property descriptor off
     * of something with JSNative property descriptors. If we are, wrap them in
     * JS Function objects.
     */
    if (attrs & JSPROP_NATIVE_ACCESSORS) {
        JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
        JSFunction::Flags zeroFlags = JSAPIToJSFunctionFlags(0);
        // We can't just use JS_NewFunctionById here because it
        // assumes a string id.
        RootedAtom atom(cx, JSID_IS_ATOM(id) ? JSID_TO_ATOM(id) : nullptr);
        attrs &= ~JSPROP_NATIVE_ACCESSORS;
        if (getter) {
            RootedObject global(cx, (JSObject*) &obj->global());
            JSFunction *getobj = NewFunction(cx, NullPtr(), (Native) getter, 0,
                                             zeroFlags, global, atom);
            if (!getobj)
                return false;

            if (get.info)
                getobj->setJitInfo(get.info);

            getter = JS_DATA_TO_FUNC_PTR(PropertyOp, getobj);
            attrs |= JSPROP_GETTER;
        }
        if (setter) {
            // Root just the getter, since the setter is not yet a JSObject.
            AutoRooterGetterSetter getRoot(cx, JSPROP_GETTER, &getter, nullptr);
            RootedObject global(cx, (JSObject*) &obj->global());
            JSFunction *setobj = NewFunction(cx, NullPtr(), (Native) setter, 1,
                                             zeroFlags, global, atom);
            if (!setobj)
                return false;

            if (set.info)
                setobj->setJitInfo(set.info);

            setter = JS_DATA_TO_FUNC_PTR(StrictPropertyOp, setobj);
            attrs |= JSPROP_SETTER;
        }
    }


    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id, value,
                            (attrs & JSPROP_GETTER)
                            ? JS_FUNC_TO_DATA_PTR(JSObject *, getter)
                            : nullptr,
                            (attrs & JSPROP_SETTER)
                            ? JS_FUNC_TO_DATA_PTR(JSObject *, setter)
                            : nullptr);

    JSAutoResolveFlags rf(cx, 0);
    if (flags != 0 && obj->isNative())
        return DefineNativeProperty(cx, obj, id, value, getter, setter, attrs, flags);
    return JSObject::defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

JS_PUBLIC_API(bool)
JS_DefinePropertyById(JSContext *cx, JSObject *objArg, jsid idArg, jsval valueArg,
                      JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    RootedValue value(cx, valueArg);
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineElement(JSContext *cx, JSObject *objArg, uint32_t index, jsval valueArg,
                 JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    RootedObject obj(cx, objArg);
    RootedValue value(cx, valueArg);
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, 0);
}

static bool
DefineProperty(JSContext *cx, HandleObject obj, const char *name, HandleValue value,
               const JSPropertyOpWrapper &getter, const JSStrictPropertyOpWrapper &setter,
               unsigned attrs, unsigned flags)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, const_cast<JSPropertyOp *>(&getter.op),
                                  const_cast<JSStrictPropertyOp *>(&setter.op));

    RootedId id(cx);
    if (attrs & JSPROP_INDEX) {
        id.set(INT_TO_JSID(intptr_t(name)));
        attrs &= ~JSPROP_INDEX;
    } else {
        JSAtom *atom = Atomize(cx, name, strlen(name));
        if (!atom)
            return false;
        id = AtomToId(atom);
    }

    return DefinePropertyById(cx, obj, id, value, getter, setter, attrs, flags);
}


static bool
DefineSelfHostedProperty(JSContext *cx,
                         HandleObject obj,
                         const char *name,
                         const char *getterName,
                         const char *setterName,
                         unsigned attrs,
                         unsigned flags)
{
    RootedAtom nameAtom(cx, Atomize(cx, name, strlen(name)));
    if (!nameAtom)
        return false;

    RootedAtom getterNameAtom(cx, Atomize(cx, getterName, strlen(getterName)));
    if (!getterNameAtom)
        return false;

    RootedValue getterValue(cx);
    if (!cx->global()->getSelfHostedFunction(cx, getterNameAtom, nameAtom,
                                             0, &getterValue))
    {
        return false;
    }
    JS_ASSERT(getterValue.isObject() && getterValue.toObject().is<JSFunction>());
    RootedFunction getterFunc(cx, &getterValue.toObject().as<JSFunction>());
    JSPropertyOp getterOp = JS_DATA_TO_FUNC_PTR(PropertyOp, getterFunc.get());

    RootedFunction setterFunc(cx);
    if (setterName) {
        RootedAtom setterNameAtom(cx, Atomize(cx, setterName, strlen(setterName)));
        if (!setterNameAtom)
            return false;

        RootedValue setterValue(cx);
        if (!cx->global()->getSelfHostedFunction(cx, setterNameAtom, nameAtom,
                                                 0, &setterValue))
        {
            return false;
        }
        JS_ASSERT(setterValue.isObject() && setterValue.toObject().is<JSFunction>());
        setterFunc = &getterValue.toObject().as<JSFunction>();
    }
    JSStrictPropertyOp setterOp = JS_DATA_TO_FUNC_PTR(StrictPropertyOp, setterFunc.get());

    return DefineProperty(cx, obj, name, JS::UndefinedHandleValue,
                          GetterWrapper(getterOp), SetterWrapper(setterOp),
                          attrs, flags);
}

JS_PUBLIC_API(bool)
JS_DefineProperty(JSContext *cx, JSObject *objArg, const char *name, jsval valueArg,
                  PropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    RootedObject obj(cx, objArg);
    RootedValue value(cx, valueArg);
    return DefineProperty(cx, obj, name, value, GetterWrapper(getter), SetterWrapper(setter),
                          attrs, 0);
}

static bool
DefineUCProperty(JSContext *cx, HandleObject obj, const jschar *name, size_t namelen,
                 const Value &value_, PropertyOp getter, StrictPropertyOp setter, unsigned attrs,
                 unsigned flags)
{
    RootedValue value(cx, value_);
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return DefinePropertyById(cx, obj, id, value, GetterWrapper(getter), SetterWrapper(setter),
                              attrs, flags);
}

JS_PUBLIC_API(bool)
JS_DefineUCProperty(JSContext *cx, JSObject *objArg, const jschar *name, size_t namelen,
                    jsval valueArg, JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    RootedObject obj(cx, objArg);
    RootedValue value(cx, valueArg);
    return DefineUCProperty(cx, obj, name, namelen, value, getter, setter, attrs, 0);
}

JS_PUBLIC_API(bool)
JS_DefineOwnProperty(JSContext *cx, JSObject *objArg, jsid idArg, jsval descriptorArg, bool *bp)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    RootedValue descriptor(cx, descriptorArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id, descriptor);

    return DefineOwnProperty(cx, obj, id, descriptor, bp);
}

JS_PUBLIC_API(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *objArg, const char *name, const JSClass *jsclasp,
                JSObject *protoArg, unsigned attrs)
{
    RootedObject obj(cx, objArg);
    RootedObject proto(cx, protoArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, proto);

    const Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &JSObject::class_;    /* default class is Object */

    RootedObject nobj(cx, NewObjectWithClassProto(cx, clasp, proto, obj));
    if (!nobj)
        return nullptr;

    RootedValue nobjValue(cx, ObjectValue(*nobj));
    if (!DefineProperty(cx, obj, name, nobjValue, GetterWrapper(nullptr), SetterWrapper(nullptr),
                        attrs, 0)) {
        return nullptr;
    }

    return nobj;
}

JS_PUBLIC_API(bool)
JS_DefineConstDoubles(JSContext *cx, JSObject *objArg, const JSConstDoubleSpec *cds)
{
    RootedObject obj(cx, objArg);
    bool ok;
    unsigned attrs;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSPropertyOpWrapper noget = GetterWrapper(nullptr);
    JSStrictPropertyOpWrapper noset = SetterWrapper(nullptr);
    for (ok = true; cds->name; cds++) {
        RootedValue value(cx, DoubleValue(cds->dval));
        attrs = cds->flags;
        if (!attrs)
            attrs = JSPROP_READONLY | JSPROP_PERMANENT;
        ok = DefineProperty(cx, obj, cds->name, value, noget, noset, attrs, 0);
        if (!ok)
            break;
    }
    return ok;
}

JS_PUBLIC_API(bool)
JS_DefineProperties(JSContext *cx, JSObject *objArg, const JSPropertySpec *ps)
{
    RootedObject obj(cx, objArg);
    bool ok;
    for (ok = true; ps->name; ps++) {
        if (ps->flags & JSPROP_NATIVE_ACCESSORS) {
            // If you declare native accessors, then you should have a native
            // getter.
            JS_ASSERT(ps->getter.propertyOp.op);
            // If you do not have a self-hosted getter, you should not have a
            // self-hosted setter. This is the closest approximation to that
            // assertion we can have with our setup.
            JS_ASSERT_IF(ps->setter.propertyOp.info, ps->setter.propertyOp.op);

            ok = DefineProperty(cx, obj, ps->name, JS::UndefinedHandleValue,
                                ps->getter.propertyOp, ps->setter.propertyOp, ps->flags, 0);
        } else {
            // If you have self-hosted getter/setter, you can't have a
            // native one.
            JS_ASSERT(!ps->getter.propertyOp.op && !ps->setter.propertyOp.op);
            JS_ASSERT(ps->flags & JSPROP_GETTER);
            /*
             * During creation of the self-hosting global, we ignore all
             * self-hosted properties, as that means we're currently setting up
             * the global object that the self-hosted code is then compiled
             * in. That means that Self-hosted properties can't be used in the
             * self-hosting global itself, right now.
             */
            if (cx->runtime()->isSelfHostingGlobal(cx->global()))
                continue;

            ok = DefineSelfHostedProperty(cx, obj, ps->name,
                                          ps->getter.selfHosted.funname,
                                          ps->setter.selfHosted.funname,
                                          ps->flags, 0);
        }
        if (!ok)
            break;
    }
    return ok;
}

static bool
GetPropertyDescriptorById(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                          bool own, MutableHandle<PropertyDescriptor> desc)
{
    RootedObject obj2(cx);
    RootedShape shape(cx);

    if (!LookupPropertyById(cx, obj, id, flags, &obj2, &shape))
        return false;

    desc.clear();
    if (!shape || (own && obj != obj2))
        return true;

    desc.object().set(obj2);
    if (obj2->isNative()) {
        if (IsImplicitDenseOrTypedArrayElement(shape)) {
            desc.setEnumerable();
            desc.value().set(obj2->getDenseOrTypedArrayElement(JSID_TO_INT(id)));
        } else {
            desc.setAttributes(shape->attributes());
            desc.setGetter(shape->getter());
            desc.setSetter(shape->setter());
            JS_ASSERT(desc.value().isUndefined());
            if (shape->hasSlot())
                desc.value().set(obj2->nativeGetSlot(shape->slot()));
        }
    } else {
        if (obj2->is<ProxyObject>()) {
            JSAutoResolveFlags rf(cx, flags);
            return own
                   ? Proxy::getOwnPropertyDescriptor(cx, obj2, id, desc, 0)
                   : Proxy::getPropertyDescriptor(cx, obj2, id, desc, 0);
        }
        if (!JSObject::getGenericAttributes(cx, obj2, id, &desc.attributesRef()))
            return false;
        JS_ASSERT(desc.getter() == nullptr);
        JS_ASSERT(desc.setter() == nullptr);
        JS_ASSERT(desc.value().isUndefined());
    }
    return true;
}

JS_PUBLIC_API(bool)
JS_GetOwnPropertyDescriptorById(JSContext *cx, JSObject *objArg, jsid idArg, unsigned flags,
                                MutableHandle<JSPropertyDescriptor> desc)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return GetPropertyDescriptorById(cx, obj, id, flags, true, desc);
}

JS_PUBLIC_API(bool)
JS_GetOwnPropertyDescriptor(JSContext *cx, JSObject *objArg, const char *name, unsigned flags,
                            MutableHandle<JSPropertyDescriptor> desc)
{
    RootedObject obj(cx, objArg);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    return atom && JS_GetOwnPropertyDescriptorById(cx, obj, AtomToId(atom), flags, desc);
}

JS_PUBLIC_API(bool)
JS_GetPropertyDescriptorById(JSContext *cx, JSObject *objArg, jsid idArg, unsigned flags,
                             MutableHandle<JSPropertyDescriptor> desc)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    return GetPropertyDescriptorById(cx, obj, id, flags, false, desc);
}

JS_PUBLIC_API(bool)
JS_GetPropertyDescriptor(JSContext *cx, JSObject *objArg, const char *name, unsigned flags,
                         MutableHandle<JSPropertyDescriptor> desc)
{
    RootedObject obj(cx, objArg);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    return atom && JS_GetPropertyDescriptorById(cx, obj, AtomToId(atom), flags, desc);
}

JS_PUBLIC_API(bool)
JS_GetPropertyById(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    return JS_ForwardGetPropertyTo(cx, obj, id, obj, vp);
}

JS_PUBLIC_API(bool)
JS_ForwardGetPropertyTo(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::HandleObject onBehalfOf,
                        JS::MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    assertSameCompartment(cx, onBehalfOf);
    JSAutoResolveFlags rf(cx, 0);

    return JSObject::getGeneric(cx, obj, onBehalfOf, id, vp);
}

JS_PUBLIC_API(bool)
JS_GetElement(JSContext *cx, HandleObject objArg, uint32_t index, MutableHandleValue vp)
{
    return JS_ForwardGetElementTo(cx, objArg, index, objArg, vp);
}

JS_PUBLIC_API(bool)
JS_ForwardGetElementTo(JSContext *cx, JSObject *objArg, uint32_t index, JSObject *onBehalfOfArg,
                       MutableHandleValue vp)
{
    RootedObject obj(cx, objArg);
    RootedObject onBehalfOf(cx, onBehalfOfArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAutoResolveFlags rf(cx, 0);

    return JSObject::getElement(cx, obj, onBehalfOf, index, vp);
}

JS_PUBLIC_API(bool)
JS_GetProperty(JSContext *cx, HandleObject obj, const char *name, MutableHandleValue vp)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_GetPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_GetUCProperty(JSContext *cx, HandleObject obj, const jschar *name, size_t namelen,
                 MutableHandleValue vp)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_GetPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(bool)
JS_SetPropertyById(JSContext *cx, HandleObject obj, HandleId id, HandleValue v)
{
    RootedValue value(cx, v);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    JSAutoResolveFlags rf(cx, JSRESOLVE_ASSIGNING);

    return JSObject::setGeneric(cx, obj, obj, id, &value, false);
}

static bool
SetElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, vp);
    JSAutoResolveFlags rf(cx, JSRESOLVE_ASSIGNING);

    return JSObject::setElement(cx, obj, obj, index, vp, false);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v)
{
    RootedValue value(cx, v);
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, HandleObject v)
{
    RootedValue value(cx, ObjectOrNullValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, HandleString v)
{
    RootedValue value(cx, StringValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, int32_t v)
{
    RootedValue value(cx, NumberValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, uint32_t v)
{
    RootedValue value(cx, NumberValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetElement(JSContext *cx, HandleObject obj, uint32_t index, double v)
{
    RootedValue value(cx, NumberValue(v));
    return SetElement(cx, obj, index, &value);
}

JS_PUBLIC_API(bool)
JS_SetProperty(JSContext *cx, HandleObject obj, const char *name, HandleValue v)
{
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_SetPropertyById(cx, obj, id, v);
}

JS_PUBLIC_API(bool)
JS_SetUCProperty(JSContext *cx, HandleObject obj, const jschar *name, size_t namelen,
                 HandleValue v)
{
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));
    return JS_SetPropertyById(cx, obj, id, v);
}

JS_PUBLIC_API(bool)
JS_DeletePropertyById2(JSContext *cx, HandleObject obj, HandleId id, bool *result)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    JSAutoResolveFlags rf(cx, 0);

    if (JSID_IS_SPECIAL(id)) {
        Rooted<SpecialId> sid(cx, JSID_TO_SPECIALID(id));
        return JSObject::deleteSpecial(cx, obj, sid, result);
    }
    return JSObject::deleteByValue(cx, obj, IdToValue(id), result);
}

JS_PUBLIC_API(bool)
JS_DeleteElement2(JSContext *cx, HandleObject obj, uint32_t index, bool *result)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAutoResolveFlags rf(cx, 0);

    return JSObject::deleteElement(cx, obj, index, result);
}

JS_PUBLIC_API(bool)
JS_DeleteProperty2(JSContext *cx, HandleObject obj, const char *name, bool *result)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAutoResolveFlags rf(cx, 0);

    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    return JSObject::deleteByValue(cx, obj, StringValue(atom), result);
}

JS_PUBLIC_API(bool)
JS_DeleteUCProperty2(JSContext *cx, HandleObject obj, const jschar *name, size_t namelen,
                     bool *result)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAutoResolveFlags rf(cx, 0);

    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return false;
    return JSObject::deleteByValue(cx, obj, StringValue(atom), result);
}

JS_PUBLIC_API(bool)
JS_DeletePropertyById(JSContext *cx, HandleObject obj, HandleId id)
{
    bool junk;
    return JS_DeletePropertyById2(cx, obj, id, &junk);
}

JS_PUBLIC_API(bool)
JS_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index)
{
    bool junk;
    return JS_DeleteElement2(cx, obj, index, &junk);
}

JS_PUBLIC_API(bool)
JS_DeleteProperty(JSContext *cx, HandleObject obj, const char *name)
{
    bool junk;
    return JS_DeleteProperty2(cx, obj, name, &junk);
}

static Shape *
LastConfigurableShape(JSObject *obj)
{
    for (Shape::Range<NoGC> r(obj->lastProperty()); !r.empty(); r.popFront()) {
        Shape *shape = &r.front();
        if (shape->configurable())
            return shape;
    }
    return nullptr;
}

JS_PUBLIC_API(void)
JS_ClearNonGlobalObject(JSContext *cx, JSObject *objArg)
{
    RootedObject obj(cx, objArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    JS_ASSERT(!obj->is<GlobalObject>());

    if (!obj->isNative())
        return;

    /* Remove all configurable properties from obj. */
    RootedShape shape(cx);
    while ((shape = LastConfigurableShape(obj))) {
        if (!obj->removeProperty(cx, shape->propid()))
            return;
    }

    /* Set all remaining writable plain data properties to undefined. */
    for (Shape::Range<NoGC> r(obj->lastProperty()); !r.empty(); r.popFront()) {
        Shape *shape = &r.front();
        if (shape->isDataDescriptor() &&
            shape->writable() &&
            shape->hasDefaultSetter() &&
            shape->hasSlot())
        {
            obj->nativeSetSlot(shape->slot(), UndefinedValue());
        }
    }
}

JS_PUBLIC_API(void)
JS_SetAllNonReservedSlotsToUndefined(JSContext *cx, JSObject *objArg)
{
    RootedObject obj(cx, objArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    if (!obj->isNative())
        return;

    const Class *clasp = obj->getClass();
    unsigned numReserved = JSCLASS_RESERVED_SLOTS(clasp);
    unsigned numSlots = obj->slotSpan();
    for (unsigned i = numReserved; i < numSlots; i++)
        obj->setSlot(i, UndefinedValue());
}

JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, JSObject *objArg)
{
    RootedObject obj(cx, objArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    AutoIdVector props(cx);
    JSIdArray *ida;
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &props) || !VectorToIdArray(cx, props, &ida))
        return nullptr;
    return ida;
}

/*
 * XXX reverse iterator for properties, unreverse and meld with jsinterp.c's
 *     prop_iterator_class somehow...
 * + preserve the obj->enumerate API while optimizing the native object case
 * + native case here uses a JSShape *, but that iterates in reverse!
 * + so we make non-native match, by reverse-iterating after JS_Enumerating
 */
static const uint32_t JSSLOT_ITER_INDEX = 0;

static void
prop_iter_finalize(FreeOp *fop, JSObject *obj)
{
    void *pdata = obj->getPrivate();
    if (!pdata)
        return;

    if (obj->getSlot(JSSLOT_ITER_INDEX).toInt32() >= 0) {
        /* Non-native case: destroy the ida enumerated when obj was created. */
        JSIdArray *ida = (JSIdArray *) pdata;
        fop->free_(ida);
    }
}

static void
prop_iter_trace(JSTracer *trc, JSObject *obj)
{
    void *pdata = obj->getPrivate();
    if (!pdata)
        return;

    if (obj->getSlot(JSSLOT_ITER_INDEX).toInt32() < 0) {
        /*
         * Native case: just mark the next property to visit. We don't need a
         * barrier here because the pointer is updated via setPrivate, which
         * always takes a barrier.
         */
        Shape *tmp = static_cast<Shape *>(pdata);
        MarkShapeUnbarriered(trc, &tmp, "prop iter shape");
        obj->setPrivateUnbarriered(tmp);
    } else {
        /* Non-native case: mark each id in the JSIdArray private. */
        JSIdArray *ida = (JSIdArray *) pdata;
        MarkIdRange(trc, ida->length, ida->vector, "prop iter");
    }
}

static const Class prop_iter_class = {
    "PropertyIterator",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    prop_iter_finalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    prop_iter_trace
};

JS_PUBLIC_API(JSObject *)
JS_NewPropertyIterator(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    RootedObject iterobj(cx, NewObjectWithClassProto(cx, &prop_iter_class, nullptr, obj));
    if (!iterobj)
        return nullptr;

    int index;
    if (obj->isNative()) {
        /* Native case: start with the last property in obj. */
        iterobj->setPrivateGCThing(obj->lastProperty());
        index = -1;
    } else {
        /* Non-native case: enumerate a JSIdArray and keep it via private. */
        JSIdArray *ida = JS_Enumerate(cx, obj);
        if (!ida)
            return nullptr;
        iterobj->setPrivate((void *)ida);
        index = ida->length;
    }

    /* iterobj cannot escape to other threads here. */
    iterobj->setSlot(JSSLOT_ITER_INDEX, Int32Value(index));
    return iterobj;
}

JS_PUBLIC_API(bool)
JS_NextProperty(JSContext *cx, HandleObject iterobj, jsid *idp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, iterobj);
    int32_t i = iterobj->getSlot(JSSLOT_ITER_INDEX).toInt32();
    if (i < 0) {
        /* Native case: private data is a property tree node pointer. */
        JS_ASSERT(iterobj->getParent()->isNative());
        Shape *shape = static_cast<Shape *>(iterobj->getPrivate());

        while (shape->previous() && !shape->enumerable())
            shape = shape->previous();

        if (!shape->previous()) {
            JS_ASSERT(shape->isEmptyShape());
            *idp = JSID_VOID;
        } else {
            iterobj->setPrivateGCThing(const_cast<Shape *>(shape->previous().get()));
            *idp = shape->propid();
        }
    } else {
        /* Non-native case: use the ida enumerated when iterobj was created. */
        JSIdArray *ida = (JSIdArray *) iterobj->getPrivate();
        JS_ASSERT(i <= ida->length);
        STATIC_ASSUME(i <= ida->length);
        if (i == 0) {
            *idp = JSID_VOID;
        } else {
            *idp = ida->vector[--i];
            iterobj->setSlot(JSSLOT_ITER_INDEX, Int32Value(i));
        }
    }
    return true;
}

JS_PUBLIC_API(jsval)
JS_GetReservedSlot(JSObject *obj, uint32_t index)
{
    return obj->getReservedSlot(index);
}

JS_PUBLIC_API(void)
JS_SetReservedSlot(JSObject *obj, uint32_t index, Value value)
{
    obj->setReservedSlot(index, value);
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, const JS::HandleValueArray& contents)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    assertSameCompartment(cx, contents);
    return NewDenseCopiedArray(cx, contents.length(), contents.begin());
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, size_t length)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return NewDenseAllocatedArray(cx, length);
}

JS_PUBLIC_API(bool)
JS_IsArrayObject(JSContext *cx, JS::HandleObject obj)
{
    assertSameCompartment(cx, obj);
    return ObjectClassIs(obj, ESClass_Array, cx);
}

JS_PUBLIC_API(bool)
JS_IsArrayObject(JSContext *cx, JS::HandleValue value)
{
    if (!value.isObject())
        return false;
    RootedObject obj(cx, &value.toObject());
    return JS_IsArrayObject(cx, obj);
}

JS_PUBLIC_API(bool)
JS_GetArrayLength(JSContext *cx, HandleObject obj, uint32_t *lengthp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return GetLengthProperty(cx, obj, lengthp);
}

JS_PUBLIC_API(bool)
JS_SetArrayLength(JSContext *cx, HandleObject obj, uint32_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return SetLengthProperty(cx, obj, length);
}

JS_PUBLIC_API(void)
JS_HoldPrincipals(JSPrincipals *principals)
{
    ++principals->refcount;
}

JS_PUBLIC_API(void)
JS_DropPrincipals(JSRuntime *rt, JSPrincipals *principals)
{
    int rc = --principals->refcount;
    if (rc == 0)
        rt->destroyPrincipals(principals);
}

JS_PUBLIC_API(void)
JS_SetSecurityCallbacks(JSRuntime *rt, const JSSecurityCallbacks *scb)
{
    JS_ASSERT(scb != &NullSecurityCallbacks);
    rt->securityCallbacks = scb ? scb : &NullSecurityCallbacks;
}

JS_PUBLIC_API(const JSSecurityCallbacks *)
JS_GetSecurityCallbacks(JSRuntime *rt)
{
    return (rt->securityCallbacks != &NullSecurityCallbacks) ? rt->securityCallbacks : nullptr;
}

JS_PUBLIC_API(void)
JS_SetTrustedPrincipals(JSRuntime *rt, const JSPrincipals *prin)
{
    rt->setTrustedPrincipals(prin);
}

extern JS_PUBLIC_API(void)
JS_InitDestroyPrincipalsCallback(JSRuntime *rt, JSDestroyPrincipalsOp destroyPrincipals)
{
    JS_ASSERT(destroyPrincipals);
    JS_ASSERT(!rt->destroyPrincipals);
    rt->destroyPrincipals = destroyPrincipals;
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
               HandleObject parent, const char *name)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom atom(cx);
    if (name) {
        atom = Atomize(cx, name, strlen(name));
        if (!atom)
            return nullptr;
    }

    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, atom);
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunctionById(JSContext *cx, JSNative native, unsigned nargs, unsigned flags,
                   HandleObject parent, HandleId id)
{
    JS_ASSERT(JSID_IS_STRING(id));
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    JS_ASSERT(native);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    RootedAtom name(cx, JSID_TO_ATOM(id));
    JSFunction::Flags funFlags = JSAPIToJSFunctionFlags(flags);
    return NewFunction(cx, NullPtr(), native, nargs, funFlags, parent, name);
}

JS_PUBLIC_API(JSFunction *)
JS::GetSelfHostedFunction(JSContext *cx, const char *selfHostedName, HandleId id, unsigned nargs)
{
    JS_ASSERT(JSID_IS_STRING(id));
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RootedAtom name(cx, JSID_TO_ATOM(id));
    RootedAtom shName(cx, Atomize(cx, selfHostedName, strlen(selfHostedName)));
    if (!shName)
        return nullptr;
    RootedValue funVal(cx);
    if (!cx->global()->getSelfHostedFunction(cx, shName, name, nargs, &funVal))
        return nullptr;
    return &funVal.toObject().as<JSFunction>();
}

JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, HandleObject funobj, HandleObject parentArg)
{
    RootedObject parent(cx, parentArg);

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);
    // Note that funobj can be in a different compartment.

    if (!parent)
        parent = cx->global();

    if (!funobj->is<JSFunction>()) {
        AutoCompartment ac(cx, funobj);
        RootedValue v(cx, ObjectValue(*funobj));
        ReportIsNotFunction(cx, v);
        return nullptr;
    }

    RootedFunction fun(cx, &funobj->as<JSFunction>());
    if (fun->isInterpretedLazy()) {
        AutoCompartment ac(cx, funobj);
        if (!fun->getOrCreateScript(cx))
            return nullptr;
    }
    /*
     * If a function was compiled to be lexically nested inside some other
     * script, we cannot clone it without breaking the compiler's assumptions.
     */
    if (fun->isInterpreted() && (fun->nonLazyScript()->enclosingStaticScope() ||
        (fun->nonLazyScript()->compileAndGo() && !parent->is<GlobalObject>())))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_CLONE_FUNOBJ_SCOPE);
        return nullptr;
    }

    if (fun->isBoundFunction()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CLONE_OBJECT);
        return nullptr;
    }

    if (fun->isNative() && IsAsmJSModuleNative(fun->native())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CLONE_OBJECT);
        return nullptr;
    }

    return CloneFunctionObject(cx, fun, parent, fun->getAllocKind());
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    return fun;
}

JS_PUBLIC_API(JSString *)
JS_GetFunctionId(JSFunction *fun)
{
    return fun->atom();
}

JS_PUBLIC_API(JSString *)
JS_GetFunctionDisplayId(JSFunction *fun)
{
    return fun->displayAtom();
}

JS_PUBLIC_API(uint16_t)
JS_GetFunctionArity(JSFunction *fun)
{
    return fun->nargs();
}

JS_PUBLIC_API(bool)
JS_ObjectIsFunction(JSContext *cx, JSObject *obj)
{
    return obj->is<JSFunction>();
}

JS_PUBLIC_API(bool)
JS_ObjectIsCallable(JSContext *cx, JSObject *obj)
{
    return obj->isCallable();
}

JS_PUBLIC_API(bool)
JS_IsNativeFunction(JSObject *funobj, JSNative call)
{
    if (!funobj->is<JSFunction>())
        return false;
    JSFunction *fun = &funobj->as<JSFunction>();
    return fun->isNative() && fun->native() == call;
}

extern JS_PUBLIC_API(bool)
JS_IsConstructor(JSFunction *fun)
{
    return fun->isNativeConstructor() || fun->isInterpretedConstructor();
}

JS_PUBLIC_API(JSObject*)
JS_BindCallable(JSContext *cx, HandleObject target, HandleObject newThis)
{
    RootedValue thisArg(cx, ObjectValue(*newThis));
    return js_fun_bind(cx, target, thisArg, nullptr, 0);
}

static bool
js_generic_native_method_dispatcher(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    const JSFunctionSpec *fs = (JSFunctionSpec *)
        vp->toObject().as<JSFunction>().getExtendedSlot(0).toPrivate();
    JS_ASSERT((fs->flags & JSFUN_GENERIC_NATIVE) != 0);

    if (argc < 1) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }

    /*
     * Copy all actual (argc) arguments down over our |this| parameter, vp[1],
     * which is almost always the class constructor object, e.g. Array.  Then
     * call the corresponding prototype native method with our first argument
     * passed as |this|.
     */
    memmove(vp + 1, vp + 2, argc * sizeof(jsval));

    /* Clear the last parameter in case too few arguments were passed. */
    vp[2 + --argc].setUndefined();

    return fs->call.op(cx, argc, vp);
}

JS_PUBLIC_API(bool)
JS_DefineFunctions(JSContext *cx, HandleObject obj, const JSFunctionSpec *fs)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    RootedObject ctor(cx);

    for (; fs->name; fs++) {
        RootedAtom atom(cx);
        // If the name starts with "@@", it must be a well-known symbol.
        if (fs->name[0] != '@' || fs->name[1] != '@')
            atom = Atomize(cx, fs->name, strlen(fs->name));
        else if (strcmp(fs->name, "@@iterator") == 0)
            // FIXME: This atom should be a symbol: bug 918828.
            atom = cx->names().std_iterator;
        else
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_SYMBOL, fs->name);
        if (!atom)
            return false;

        Rooted<jsid> id(cx, AtomToId(atom));

        /*
         * Define a generic arity N+1 static method for the arity N prototype
         * method if flags contains JSFUN_GENERIC_NATIVE.
         */
        unsigned flags = fs->flags;
        if (flags & JSFUN_GENERIC_NATIVE) {
            if (!ctor) {
                ctor = JS_GetConstructor(cx, obj);
                if (!ctor)
                    return false;
            }

            flags &= ~JSFUN_GENERIC_NATIVE;
            JSFunction *fun = DefineFunction(cx, ctor, id,
                                             js_generic_native_method_dispatcher,
                                             fs->nargs + 1, flags,
                                             JSFunction::ExtendedFinalizeKind);
            if (!fun)
                return false;

            /*
             * As jsapi.h notes, fs must point to storage that lives as long
             * as fun->object lives.
             */
            fun->setExtendedSlot(0, PrivateValue(const_cast<JSFunctionSpec*>(fs)));
        }

        /*
         * Delay cloning self-hosted functions until they are called. This is
         * achieved by passing DefineFunction a nullptr JSNative which
         * produces an interpreted JSFunction where !hasScript. Interpreted
         * call paths then call InitializeLazyFunctionScript if !hasScript.
         */
        if (fs->selfHostedName) {
            JS_ASSERT(!fs->call.op);
            JS_ASSERT(!fs->call.info);
            /*
             * During creation of the self-hosting global, we ignore all
             * self-hosted functions, as that means we're currently setting up
             * the global object that the self-hosted code is then compiled
             * in. Self-hosted functions can access each other via their names,
             * but not via the builtin classes they get installed into.
             */
            if (cx->runtime()->isSelfHostingGlobal(cx->global()))
                continue;

            RootedAtom shName(cx, Atomize(cx, fs->selfHostedName, strlen(fs->selfHostedName)));
            if (!shName)
                return false;
            RootedValue funVal(cx);
            if (!cx->global()->getSelfHostedFunction(cx, shName, atom, fs->nargs, &funVal))
                return false;
            if (!JSObject::defineGeneric(cx, obj, id, funVal, nullptr, nullptr, flags))
                return false;
        } else {
            JSFunction *fun = DefineFunction(cx, obj, id, fs->call.op, fs->nargs, flags);
            if (!fun)
                return false;
            if (fs->call.info)
                fun->setJitInfo(fs->call.info);
        }
    }
    return true;
}

JS_PUBLIC_API(JSFunction *)
JS_DefineFunction(JSContext *cx, HandleObject obj, const char *name, JSNative call,
                  unsigned nargs, unsigned attrs)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return nullptr;
    Rooted<jsid> id(cx, AtomToId(atom));
    return DefineFunction(cx, obj, id, call, nargs, attrs);
}

JS_PUBLIC_API(JSFunction *)
JS_DefineUCFunction(JSContext *cx, HandleObject obj,
                    const jschar *name, size_t namelen, JSNative call,
                    unsigned nargs, unsigned attrs)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return nullptr;
    Rooted<jsid> id(cx, AtomToId(atom));
    return DefineFunction(cx, obj, id, call, nargs, attrs);
}

extern JS_PUBLIC_API(JSFunction *)
JS_DefineFunctionById(JSContext *cx, HandleObject obj, HandleId id, JSNative call,
                      unsigned nargs, unsigned attrs)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return DefineFunction(cx, obj, id, call, nargs, attrs);
}

struct AutoLastFrameCheck
{
    AutoLastFrameCheck(JSContext *cx
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx)
    {
        JS_ASSERT(cx);
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoLastFrameCheck() {
        if (cx->isExceptionPending() &&
            !JS_IsRunning(cx) &&
            !cx->options().dontReportUncaught()) {
            js_ReportUncaughtException(cx);
        }
    }

  private:
    JSContext *cx;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* Use the fastest available getc. */
#if defined(HAVE_GETC_UNLOCKED)
# define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
# define fast_getc _getc_nolock
#else
# define fast_getc getc
#endif

typedef js::Vector<char, 8, TempAllocPolicy> FileContents;

static bool
ReadCompleteFile(JSContext *cx, FILE *fp, FileContents &buffer)
{
    /* Get the complete length of the file, if possible. */
    struct stat st;
    int ok = fstat(fileno(fp), &st);
    if (ok != 0)
        return false;
    if (st.st_size > 0) {
        if (!buffer.reserve(st.st_size))
            return false;
    }

    // Read in the whole file. Note that we can't assume the data's length
    // is actually st.st_size, because 1) some files lie about their size
    // (/dev/zero and /dev/random), and 2) reading files in text mode on
    // Windows collapses "\r\n" pairs to single \n characters.
    for (;;) {
        int c = fast_getc(fp);
        if (c == EOF)
            break;
        if (!buffer.append(c))
            return false;
    }

    return true;
}

namespace {

class AutoFile
{
    FILE *fp_;
  public:
    AutoFile()
      : fp_(nullptr)
    {}
    ~AutoFile()
    {
        if (fp_ && fp_ != stdin)
            fclose(fp_);
    }
    FILE *fp() const { return fp_; }
    bool open(JSContext *cx, const char *filename);
    bool readAll(JSContext *cx, FileContents &buffer)
    {
        JS_ASSERT(fp_);
        return ReadCompleteFile(cx, fp_, buffer);
    }
};

} /* anonymous namespace */

/*
 * Open a source file for reading. Supports "-" and nullptr to mean stdin. The
 * return value must be fclosed unless it is stdin.
 */
bool
AutoFile::open(JSContext *cx, const char *filename)
{
    if (!filename || strcmp(filename, "-") == 0) {
        fp_ = stdin;
    } else {
        fp_ = fopen(filename, "r");
        if (!fp_) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_OPEN,
                                 filename, "No such file or directory");
            return false;
        }
    }
    return true;
}

JSObject * const JS::ReadOnlyCompileOptions::nullObjectPtr = nullptr;

void
JS::ReadOnlyCompileOptions::copyPODOptions(const ReadOnlyCompileOptions &rhs)
{
    version = rhs.version;
    versionSet = rhs.versionSet;
    utf8 = rhs.utf8;
    lineno = rhs.lineno;
    column = rhs.column;
    compileAndGo = rhs.compileAndGo;
    forEval = rhs.forEval;
    noScriptRval = rhs.noScriptRval;
    selfHostingMode = rhs.selfHostingMode;
    canLazilyParse = rhs.canLazilyParse;
    strictOption = rhs.strictOption;
    extraWarningsOption = rhs.extraWarningsOption;
    werrorOption = rhs.werrorOption;
    asmJSOption = rhs.asmJSOption;
    sourcePolicy = rhs.sourcePolicy;
    introductionType = rhs.introductionType;
    introductionLineno = rhs.introductionLineno;
    introductionOffset = rhs.introductionOffset;
    hasIntroductionInfo = rhs.hasIntroductionInfo;
}

JSPrincipals *
JS::ReadOnlyCompileOptions::originPrincipals(ExclusiveContext *cx) const
{
    return NormalizeOriginPrincipals(cx->compartment()->principals, originPrincipals_);
}

JS::OwningCompileOptions::OwningCompileOptions(JSContext *cx)
    : ReadOnlyCompileOptions(),
      runtime(GetRuntime(cx)),
      elementRoot(cx),
      elementAttributeNameRoot(cx),
      introductionScriptRoot(cx)
{
}

JS::OwningCompileOptions::~OwningCompileOptions()
{
    if (originPrincipals_)
        JS_DropPrincipals(runtime, originPrincipals_);

    // OwningCompileOptions always owns these, so these casts are okay.
    js_free(const_cast<char *>(filename_));
    js_free(const_cast<jschar *>(sourceMapURL_));
    js_free(const_cast<char *>(introducerFilename_));
}

bool
JS::OwningCompileOptions::copy(JSContext *cx, const ReadOnlyCompileOptions &rhs)
{
    copyPODOptions(rhs);

    setOriginPrincipals(rhs.originPrincipals(cx));
    setElement(rhs.element());
    setElementAttributeName(rhs.elementAttributeName());
    setIntroductionScript(rhs.introductionScript());

    return (setFileAndLine(cx, rhs.filename(), rhs.lineno) &&
            setSourceMapURL(cx, rhs.sourceMapURL()) &&
            setIntroducerFilename(cx, rhs.introducerFilename()));
}

bool
JS::OwningCompileOptions::setFile(JSContext *cx, const char *f)
{
    char *copy = nullptr;
    if (f) {
        copy = JS_strdup(cx, f);
        if (!copy)
            return false;
    }

    // OwningCompileOptions always owns filename_, so this cast is okay.
    js_free(const_cast<char *>(filename_));

    filename_ = copy;
    return true;
}

bool
JS::OwningCompileOptions::setFileAndLine(JSContext *cx, const char *f, unsigned l)
{
    if (!setFile(cx, f))
        return false;

    lineno = l;
    return true;
}

bool
JS::OwningCompileOptions::setSourceMapURL(JSContext *cx, const jschar *s)
{
    jschar *copy = nullptr;
    if (s) {
        copy = js_strdup(cx, s);
        if (!copy)
            return false;
    }

    // OwningCompileOptions always owns sourceMapURL_, so this cast is okay.
    js_free(const_cast<jschar *>(sourceMapURL_));

    sourceMapURL_ = copy;
    return true;
}

bool
JS::OwningCompileOptions::setIntroducerFilename(JSContext *cx, const char *s)
{
    char *copy = nullptr;
    if (s) {
        copy = JS_strdup(cx, s);
        if (!copy)
            return false;
    }

    // OwningCompileOptions always owns introducerFilename_, so this cast is okay.
    js_free(const_cast<char *>(introducerFilename_));

    introducerFilename_ = copy;
    return true;
}

bool
JS::OwningCompileOptions::wrap(JSContext *cx, JSCompartment *compartment)
{
    if (!compartment->wrap(cx, &elementRoot))
        return false;
    if (elementAttributeNameRoot) {
        if (!compartment->wrap(cx, elementAttributeNameRoot.address()))
            return false;
    }

    // There is no equivalent of cross-compartment wrappers for scripts. If
    // the introduction script would be in a different compartment from the
    // compiled code, we would be creating a cross-compartment script
    // reference, which would be bogus. In that case, just don't bother to
    // retain the introduction script.
    if (introductionScriptRoot) {
        if (introductionScriptRoot->compartment() != compartment)
            introductionScriptRoot = nullptr;
    }

    return true;
}

JS::CompileOptions::CompileOptions(JSContext *cx, JSVersion version)
    : ReadOnlyCompileOptions(), elementRoot(cx), elementAttributeNameRoot(cx),
      introductionScriptRoot(cx)
{
    this->version = (version != JSVERSION_UNKNOWN) ? version : cx->findVersion();

    compileAndGo = false;
    noScriptRval = cx->options().noScriptRval();
    strictOption = cx->options().strictMode();
    extraWarningsOption = cx->options().extraWarnings();
    werrorOption = cx->options().werror();
    asmJSOption = cx->options().asmJS();
}

bool
JS::CompileOptions::wrap(JSContext *cx, JSCompartment *compartment)
{
    if (!compartment->wrap(cx, &elementRoot))
        return false;
    if (elementAttributeNameRoot) {
        if (!compartment->wrap(cx, elementAttributeNameRoot.address()))
            return false;
    }

    // There is no equivalent of cross-compartment wrappers for scripts. If
    // the introduction script would be in a different compartment from the
    // compiled code, we would be creating a cross-compartment script
    // reference, which would be bogus. In that case, just don't bother to
    // retain the introduction script.
    if (introductionScriptRoot) {
        if (introductionScriptRoot->compartment() != compartment)
            introductionScriptRoot = nullptr;
    }

    return true;
}

JSScript *
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
            const jschar *chars, size_t length)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    AutoLastFrameCheck lfc(cx);

    return frontend::CompileScript(cx, &cx->tempLifoAlloc(), obj, NullPtr(), options, chars, length);
}

JSScript *
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
            const char *bytes, size_t length)
{
    jschar *chars;
    if (options.utf8)
        chars = UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get();
    else
        chars = InflateString(cx, bytes, &length);
    if (!chars)
        return nullptr;

    JSScript *script = Compile(cx, obj, options, chars, length);
    js_free(chars);
    return script;
}

JSScript *
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options, FILE *fp)
{
    FileContents buffer(cx);
    if (!ReadCompleteFile(cx, fp, buffer))
        return nullptr;

    JSScript *script = Compile(cx, obj, options, buffer.begin(), buffer.length());
    return script;
}

JSScript *
JS::Compile(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg, const char *filename)
{
    AutoFile file;
    if (!file.open(cx, filename))
        return nullptr;
    CompileOptions options(cx, optionsArg);
    options.setFileAndLine(filename, 1);
    JSScript *script = Compile(cx, obj, options, file.fp());
    return script;
}

JS_PUBLIC_API(bool)
JS::CanCompileOffThread(JSContext *cx, const ReadOnlyCompileOptions &options, size_t length)
{
    static const size_t TINY_LENGTH = 1000;
    static const size_t HUGE_LENGTH = 100 * 1000;

    // These are heuristics which the caller may choose to ignore (e.g., for
    // testing purposes).
    if (!options.forceAsync) {
        // Compiling off the main thread inolves creating a new Zone and other
        // significant overheads.  Don't bother if the script is tiny.
        if (length < TINY_LENGTH)
            return false;

#ifdef JS_THREADSAFE
        // If the parsing task would have to wait for GC to complete, it'll probably
        // be faster to just start it synchronously on the main thread unless the
        // script is huge.
        if (OffThreadParsingMustWaitForGC(cx->runtime()) && length < HUGE_LENGTH)
            return false;
#endif // JS_THREADSAFE
    }

    return cx->runtime()->canUseParallelParsing();
}

JS_PUBLIC_API(bool)
JS::CompileOffThread(JSContext *cx, Handle<JSObject*> obj, const ReadOnlyCompileOptions &options,
                     const jschar *chars, size_t length,
                     OffThreadCompileCallback callback, void *callbackData)
{
    JS_ASSERT(CanCompileOffThread(cx, options, length));
    return StartOffThreadParseScript(cx, options, chars, length, obj, callback, callbackData);
}

JS_PUBLIC_API(JSScript *)
JS::FinishOffThreadScript(JSContext *maybecx, JSRuntime *rt, void *token)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));

    Maybe<AutoLastFrameCheck> lfc;
    if (maybecx)
        lfc.construct(maybecx);

    return WorkerThreadState().finishParseTask(maybecx, rt, token);
#else
    MOZ_ASSUME_UNREACHABLE("Off thread compilation is not available.");
#endif
}

JS_PUBLIC_API(JSScript *)
JS_CompileScript(JSContext *cx, JS::HandleObject obj, const char *ascii,
                 size_t length, const JS::CompileOptions &options)
{
    return Compile(cx, obj, options, ascii, length);
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScript(JSContext *cx, JS::HandleObject obj, const jschar *chars,
                   size_t length, const JS::CompileOptions &options)
{
    return Compile(cx, obj, options, chars, length);
}

JS_PUBLIC_API(bool)
JS_BufferIsCompilableUnit(JSContext *cx, HandleObject obj, const char *utf8, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    cx->clearPendingException();

    jschar *chars = JS::UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(utf8, length), &length).get();
    if (!chars)
        return true;

    // Return true on any out-of-memory error or non-EOF-related syntax error, so our
    // caller doesn't try to collect more buffered source.
    bool result = true;

    CompileOptions options(cx);
    options.setCompileAndGo(false);
    Parser<frontend::FullParseHandler> parser(cx, &cx->tempLifoAlloc(),
                                              options, chars, length,
                                              /* foldConstants = */ true, nullptr, nullptr);
    JSErrorReporter older = JS_SetErrorReporter(cx, nullptr);
    if (!parser.parse(obj)) {
        // We ran into an error. If it was because we ran out of source, we
        // return false so our caller knows to try to collect more buffered
        // source.
        if (parser.isUnexpectedEOF())
            result = false;

        cx->clearPendingException();
    }
    JS_SetErrorReporter(cx, older);

    js_free(chars);
    return result;
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalFromScript(JSScript *script)
{
    JS_ASSERT(!script->isCachedEval());
    return &script->global();
}

JS_PUBLIC_API(JSFunction *)
JS::CompileFunction(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
                    const char *name, unsigned nargs, const char *const *argnames,
                    const jschar *chars, size_t length)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    AutoLastFrameCheck lfc(cx);

    RootedAtom funAtom(cx);
    if (name) {
        funAtom = Atomize(cx, name, strlen(name));
        if (!funAtom)
            return nullptr;
    }

    AutoNameVector formals(cx);
    for (unsigned i = 0; i < nargs; i++) {
        RootedAtom argAtom(cx, Atomize(cx, argnames[i], strlen(argnames[i])));
        if (!argAtom || !formals.append(argAtom->asPropertyName()))
            return nullptr;
    }

    RootedFunction fun(cx, NewFunction(cx, NullPtr(), nullptr, 0, JSFunction::INTERPRETED, obj,
                                       funAtom, JSFunction::FinalizeKind, TenuredObject));
    if (!fun)
        return nullptr;

    if (!frontend::CompileFunctionBody(cx, &fun, options, formals, chars, length))
        return nullptr;

    if (obj && funAtom) {
        Rooted<jsid> id(cx, AtomToId(funAtom));
        RootedValue value(cx, ObjectValue(*fun));
        if (!JSObject::defineGeneric(cx, obj, id, value, nullptr, nullptr, JSPROP_ENUMERATE))
            return nullptr;
    }

    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS::CompileFunction(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
                    const char *name, unsigned nargs, const char *const *argnames,
                    const char *bytes, size_t length)
{
    jschar *chars;
    if (options.utf8)
        chars = UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get();
    else
        chars = InflateString(cx, bytes, &length);
    if (!chars)
        return nullptr;

    JSFunction *fun = CompileFunction(cx, obj, options, name, nargs, argnames, chars, length);
    js_free(chars);
    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JS::HandleObject obj, const char *name,
                     unsigned nargs, const char *const *argnames,
                     const jschar *chars, size_t length,
                     const CompileOptions &options)
{
    return CompileFunction(cx, obj, options, name, nargs, argnames, chars, length);
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunction(JSContext *cx, JS::HandleObject obj, const char *name,
                   unsigned nargs, const char *const *argnames,
                   const char *ascii, size_t length,
                   const JS::CompileOptions &options)
{
    return CompileFunction(cx, obj, options, name, nargs, argnames, ascii, length);
}

JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, HandleScript script, const char *name, unsigned indent)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    script->ensureNonLazyCanonicalFunction(cx);
    RootedFunction fun(cx, script->functionNonDelazifying());
    if (fun)
        return JS_DecompileFunction(cx, fun, indent);
    bool haveSource = script->scriptSource()->hasSourceData();
    if (!haveSource && !JSScript::loadSource(cx, script->scriptSource(), &haveSource))
        return nullptr;
    return haveSource ? script->sourceData(cx) : js_NewStringCopyZ<CanGC>(cx, "[no source]");
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, HandleFunction fun, unsigned indent)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, fun);
    return FunctionToString(cx, fun, false, !(indent & JS_DONT_PRETTY_PRINT));
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, HandleFunction fun, unsigned indent)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, fun);
    return FunctionToString(cx, fun, true, !(indent & JS_DONT_PRETTY_PRINT));
}

MOZ_NEVER_INLINE JS_PUBLIC_API(bool)
JS_ExecuteScript(JSContext *cx, JSObject *objArg, JSScript *scriptArg, jsval *rval)
{
    RootedObject obj(cx, objArg);
    RootedScript script(cx, scriptArg);

    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    if (cx->compartment() != obj->compartment())
        *(volatile int *) 0 = 0xf0;
    AutoLastFrameCheck lfc(cx);

    /*
     * Mozilla caches pre-compiled scripts (e.g., in the XUL prototype cache)
     * and runs them against multiple globals. With a compartment per global,
     * this requires cloning the pre-compiled script into each new global.
     * Since each script gets run once, there is no point in trying to cache
     * this clone. Ideally, this would be handled at some pinch point in
     * mozilla, but there doesn't seem to be one, so we handle it here.
     */
    if (script->compartment() != obj->compartment()) {
        script = CloneScript(cx, NullPtr(), NullPtr(), script);
        if (!script.get())
            return false;
    } else {
        script = scriptArg;
    }

    return Execute(cx, script, *obj, rval);
}

JS_PUBLIC_API(bool)
JS_ExecuteScriptVersion(JSContext *cx, JSObject *objArg, JSScript *script, jsval *rval,
                        JSVersion version)
{
    RootedObject obj(cx, objArg);
    return JS_ExecuteScript(cx, obj, script, rval);
}

static const unsigned LARGE_SCRIPT_LENGTH = 500*1024;

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             const jschar *chars, size_t length, jsval *rval)
{
    CompileOptions options(cx, optionsArg);
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    AutoLastFrameCheck lfc(cx);

    options.setCompileAndGo(obj->is<GlobalObject>());
    options.setNoScriptRval(!rval);
    SourceCompressionTask sct(cx);
    RootedScript script(cx, frontend::CompileScript(cx, &cx->tempLifoAlloc(),
                                                    obj, NullPtr(), options,
                                                    chars, length, nullptr, 0, &sct));
    if (!script)
        return false;

    JS_ASSERT(script->getVersion() == options.version);

    bool result = Execute(cx, script, *obj, rval);
    if (!sct.complete())
        result = false;

    // After evaluation, the compiled script will not be run again.
    // script->ensureRanAnalysis allocated 1 analyze::Bytecode for every opcode
    // which for large scripts means significant memory. Perform a GC eagerly
    // to clear out this analysis data before anything happens to inhibit the
    // flushing of this memory (such as setting requestAnimationFrame).
    if (script->length() > LARGE_SCRIPT_LENGTH) {
        script = nullptr;
        PrepareZoneForGC(cx->zone());
        GC(cx->runtime(), GC_NORMAL, gcreason::FINISH_LARGE_EVALUTE);
    }

    return result;
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &options,
             const char *bytes, size_t length, jsval *rval)
{
    jschar *chars;
    if (options.utf8)
        chars = UTF8CharsToNewTwoByteCharsZ(cx, UTF8Chars(bytes, length), &length).get();
    else
        chars = InflateString(cx, bytes, &length);
    if (!chars)
        return false;

    bool ok = Evaluate(cx, obj, options, chars, length, rval);
    js_free(chars);
    return ok;
}

extern JS_PUBLIC_API(bool)
JS::Evaluate(JSContext *cx, HandleObject obj, const ReadOnlyCompileOptions &optionsArg,
             const char *filename, jsval *rval)
{
    FileContents buffer(cx);
    {
        AutoFile file;
        if (!file.open(cx, filename) || !file.readAll(cx, buffer))
            return false;
    }

    CompileOptions options(cx, optionsArg);
    options.setFileAndLine(filename, 1);
    return Evaluate(cx, obj, options, buffer.begin(), buffer.length(), rval);
}

JS_PUBLIC_API(bool)
JS_EvaluateUCScript(JSContext *cx, HandleObject obj, const jschar *chars, unsigned length,
                    const char *filename, unsigned lineno, MutableHandleValue rval)
{
    CompileOptions options(cx);
    options.setFileAndLine(filename, lineno);

    return Evaluate(cx, obj, options, chars, length, rval.address());
}

JS_PUBLIC_API(bool)
JS_EvaluateScript(JSContext *cx, JSObject *objArg, const char *bytes, unsigned nbytes,
                  const char *filename, unsigned lineno, jsval *rval)
{
    RootedObject obj(cx, objArg);
    CompileOptions options(cx);
    options.setFileAndLine(filename, lineno);

    return Evaluate(cx, obj, options, bytes, nbytes, rval);
}

JS_PUBLIC_API(bool)
JS_CallFunction(JSContext *cx, HandleObject obj, HandleFunction fun, const HandleValueArray& args,
                MutableHandleValue rval)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, fun, args);
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, ObjectOrNullValue(obj), ObjectValue(*fun), args.length(), args.begin(), rval);
}

JS_PUBLIC_API(bool)
JS_CallFunctionName(JSContext *cx, HandleObject obj, const char *name, const HandleValueArray& args,
                    MutableHandleValue rval)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, args);
    AutoLastFrameCheck lfc(cx);

    JSAtom *atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;

    RootedValue v(cx);
    RootedId id(cx, AtomToId(atom));
    if (!JSObject::getGeneric(cx, obj, obj, id, &v))
        return false;

    return Invoke(cx, ObjectOrNullValue(obj), v, args.length(), args.begin(), rval);
}

JS_PUBLIC_API(bool)
JS_CallFunctionValue(JSContext *cx, HandleObject obj, HandleValue fval, const HandleValueArray& args,
                     MutableHandleValue rval)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, fval, args);
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, ObjectOrNullValue(obj), fval, args.length(), args.begin(), rval);
}

JS_PUBLIC_API(bool)
JS::Call(JSContext *cx, HandleValue thisv, HandleValue fval, const JS::HandleValueArray& args,
         MutableHandleValue rval)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, thisv, fval, args);
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, thisv, fval, args.length(), args.begin(), rval);
}

JS_PUBLIC_API(JSObject *)
JS_New(JSContext *cx, JSObject *ctorArg, unsigned argc, jsval *argv)
{
    RootedObject ctor(cx, ctorArg);
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, ctor, JSValueArray(argv, argc));
    AutoLastFrameCheck lfc(cx);

    // This is not a simple variation of JS_CallFunctionValue because JSOP_NEW
    // is not a simple variation of JSOP_CALL. We have to determine what class
    // of object to create, create it, and clamp the return value to an object,
    // among other details. InvokeConstructor does the hard work.
    InvokeArgs args(cx);
    if (!args.init(argc))
        return nullptr;

    args.setCallee(ObjectValue(*ctor));
    args.setThis(NullValue());
    PodCopy(args.array(), argv, argc);

    if (!InvokeConstructor(cx, args))
        return nullptr;

    if (!args.rval().isObject()) {
        /*
         * Although constructors may return primitives (via proxies), this
         * API is asking for an object, so we report an error.
         */
        JSAutoByteString bytes;
        if (js_ValueToPrintable(cx, args.rval(), &bytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_NEW_RESULT,
                                 bytes.ptr());
        }
        return nullptr;
    }

    return &args.rval().toObject();
}

JS_PUBLIC_API(JSOperationCallback)
JS_SetOperationCallback(JSRuntime *rt, JSOperationCallback callback)
{
    JSOperationCallback old = rt->operationCallback;
    rt->operationCallback = callback;
    return old;
}

JS_PUBLIC_API(JSOperationCallback)
JS_GetOperationCallback(JSRuntime *rt)
{
    return rt->operationCallback;
}

JS_PUBLIC_API(void)
JS_TriggerOperationCallback(JSRuntime *rt)
{
    rt->triggerOperationCallback(JSRuntime::TriggerCallbackAnyThread);
}

JS_PUBLIC_API(bool)
JS_IsRunning(JSContext *cx)
{
    return cx->currentlyRunning();
}

JS_PUBLIC_API(bool)
JS_SaveFrameChain(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx);
    CHECK_REQUEST(cx);
    return cx->saveFrameChain();
}

JS_PUBLIC_API(void)
JS_RestoreFrameChain(JSContext *cx)
{
    AssertHeapIsIdleOrIterating(cx);
    CHECK_REQUEST(cx);
    cx->restoreFrameChain();
}

#ifdef MOZ_TRACE_JSCALLS
JS_PUBLIC_API(void)
JS_SetFunctionCallback(JSContext *cx, JSFunctionCallback fcb)
{
    cx->functionCallback = fcb;
}

JS_PUBLIC_API(JSFunctionCallback)
JS_GetFunctionCallback(JSContext *cx)
{
    return cx->functionCallback;
}
#endif

/************************************************************************/
JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!n)
        return cx->names().empty;
    return js_NewStringCopyN<CanGC>(cx, s, n);
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s)
{
    size_t n;
    jschar *js;
    JSString *str;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!s || !*s)
        return cx->runtime()->emptyString;
    n = strlen(s);
    js = InflateString(cx, s, &n);
    if (!js)
        return nullptr;
    str = js_NewString<CanGC>(cx, js, n);
    if (!str)
        js_free(js);
    return str;
}

JS_PUBLIC_API(bool)
JS_StringHasBeenInterned(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (!str->isAtom())
        return false;

    return AtomIsInterned(cx, &str->asAtom());
}

JS_PUBLIC_API(jsid)
INTERNED_STRING_TO_JSID(JSContext *cx, JSString *str)
{
    JS_ASSERT(str);
    JS_ASSERT(((size_t)str & JSID_TYPE_MASK) == 0);
    JS_ASSERT_IF(cx, JS_StringHasBeenInterned(cx, str));
    return AtomToId(&str->asAtom());
}

JS_PUBLIC_API(JSString *)
JS_InternJSString(JSContext *cx, HandleString str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSAtom *atom = AtomizeString(cx, str, InternAtom);
    JS_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    return JS_InternStringN(cx, s, strlen(s));
}

JS_PUBLIC_API(JSString *)
JS_InternStringN(JSContext *cx, const char *s, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSAtom *atom = Atomize(cx, s, length, InternAtom);
    JS_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_NewString<CanGC>(cx, chars, length);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!n)
        return cx->names().empty;
    return js_NewStringCopyN<CanGC>(cx, s, n);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!s)
        return cx->runtime()->emptyString;
    return js_NewStringCopyZ<CanGC>(cx, s);
}

JS_PUBLIC_API(JSString *)
JS_InternUCStringN(JSContext *cx, const jschar *s, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JSAtom *atom = AtomizeChars(cx, s, length, InternAtom);
    JS_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s)
{
    return JS_InternUCStringN(cx, s, js_strlen(s));
}

JS_PUBLIC_API(size_t)
JS_GetStringLength(JSString *str)
{
    return str->length();
}

JS_PUBLIC_API(const jschar *)
JS_GetStringCharsZ(JSContext *cx, JSString *str)
{
    size_t dummy;
    return JS_GetStringCharsZAndLength(cx, str, &dummy);
}

JS_PUBLIC_API(const jschar *)
JS_GetStringCharsZAndLength(JSContext *cx, JSString *str, size_t *plength)
{
    /*
     * Don't require |cx->compartment()| to be |str|'s compartment. We don't need
     * it, and it's annoying for callers.
     */
    JS_ASSERT(plength);
    AssertHeapIsIdleOrStringIsFlat(cx, str);
    CHECK_REQUEST(cx);
    JSFlatString *flat = str->ensureFlat(cx);
    if (!flat)
        return nullptr;
    *plength = flat->length();
    return flat->chars();
}

JS_PUBLIC_API(const jschar *)
JS_GetStringCharsAndLength(JSContext *cx, JSString *str, size_t *plength)
{
    JS_ASSERT(plength);
    AssertHeapIsIdleOrStringIsFlat(cx, str);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;
    *plength = linear->length();
    return linear->chars();
}

JS_PUBLIC_API(const jschar *)
JS_GetInternedStringChars(JSString *str)
{
    JS_ASSERT(str->isAtom());
    JSFlatString *flat = str->ensureFlat(nullptr);
    if (!flat)
        return nullptr;
    return flat->chars();
}

JS_PUBLIC_API(const jschar *)
JS_GetInternedStringCharsAndLength(JSString *str, size_t *plength)
{
    JS_ASSERT(str->isAtom());
    JS_ASSERT(plength);
    JSFlatString *flat = str->ensureFlat(nullptr);
    if (!flat)
        return nullptr;
    *plength = flat->length();
    return flat->chars();
}

extern JS_PUBLIC_API(JSFlatString *)
JS_FlattenString(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    JSFlatString *flat = str->ensureFlat(cx);
    if (!flat)
        return nullptr;
    return flat;
}

extern JS_PUBLIC_API(const jschar *)
JS_GetFlatStringChars(JSFlatString *str)
{
    return str->chars();
}

JS_PUBLIC_API(bool)
JS_CompareStrings(JSContext *cx, JSString *str1, JSString *str2, int32_t *result)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return CompareStrings(cx, str1, str2, result);
}

JS_PUBLIC_API(bool)
JS_StringEqualsAscii(JSContext *cx, JSString *str, const char *asciiBytes, bool *match)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    JSLinearString *linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return false;
    *match = StringEqualsAscii(linearStr, asciiBytes);
    return true;
}

JS_PUBLIC_API(bool)
JS_FlatStringEqualsAscii(JSFlatString *str, const char *asciiBytes)
{
    return StringEqualsAscii(str, asciiBytes);
}

JS_PUBLIC_API(size_t)
JS_PutEscapedFlatString(char *buffer, size_t size, JSFlatString *str, char quote)
{
    return PutEscapedString(buffer, size, str, quote);
}

JS_PUBLIC_API(size_t)
JS_PutEscapedString(JSContext *cx, char *buffer, size_t size, JSString *str, char quote)
{
    AssertHeapIsIdle(cx);
    JSLinearString *linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return size_t(-1);
    return PutEscapedString(buffer, size, linearStr, quote);
}

JS_PUBLIC_API(bool)
JS_FileEscapedString(FILE *fp, JSString *str, char quote)
{
    JSLinearString *linearStr = str->ensureLinear(nullptr);
    return linearStr && FileEscapedString(fp, linearStr, quote);
}

JS_PUBLIC_API(JSString *)
JS_NewDependentString(JSContext *cx, HandleString str, size_t start, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_NewDependentString(cx, str, start, length);
}

JS_PUBLIC_API(JSString *)
JS_ConcatStrings(JSContext *cx, HandleString left, HandleString right)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return ConcatStrings<CanGC>(cx, left, right);
}

JS_PUBLIC_API(bool)
JS_DecodeBytes(JSContext *cx, const char *src, size_t srclen, jschar *dst, size_t *dstlenp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (!dst) {
        *dstlenp = srclen;
        return true;
    }

    size_t dstlen = *dstlenp;

    if (srclen > dstlen) {
        InflateStringToBuffer(src, dstlen, dst);

        AutoSuppressGC suppress(cx);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BUFFER_TOO_SMALL);
        return false;
    }

    InflateStringToBuffer(src, srclen, dst);
    *dstlenp = srclen;
    return true;
}

JS_PUBLIC_API(char *)
JS_EncodeString(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;

    return LossyTwoByteCharsToNewLatin1CharsZ(cx, linear->range()).c_str();
}

JS_PUBLIC_API(char *)
JS_EncodeStringToUTF8(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;

    return TwoByteCharsToNewUTF8CharsZ(cx, linear->range()).c_str();
}

JS_PUBLIC_API(size_t)
JS_GetStringEncodingLength(JSContext *cx, JSString *str)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    const jschar *chars = str->getChars(cx);
    if (!chars)
        return size_t(-1);
    return str->length();
}

JS_PUBLIC_API(size_t)
JS_EncodeStringToBuffer(JSContext *cx, JSString *str, char *buffer, size_t length)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    /*
     * FIXME bug 612141 - fix DeflateStringToBuffer interface so the result
     * would allow to distinguish between insufficient buffer and encoding
     * error.
     */
    size_t writtenLength = length;
    const jschar *chars = str->getChars(nullptr);
    if (!chars)
        return size_t(-1);
    if (DeflateStringToBuffer(nullptr, chars, str->length(), buffer, &writtenLength)) {
        JS_ASSERT(writtenLength <= length);
        return writtenLength;
    }
    JS_ASSERT(writtenLength <= length);
    size_t necessaryLength = str->length();
    if (necessaryLength == size_t(-1))
        return size_t(-1);
    JS_ASSERT(writtenLength == length); // C strings are NOT encoded.
    return necessaryLength;
}

JS_PUBLIC_API(bool)
JS_Stringify(JSContext *cx, MutableHandleValue vp, HandleObject replacer,
             HandleValue space, JSONWriteCallback callback, void *data)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, replacer, space);
    StringBuffer sb(cx);
    if (!js_Stringify(cx, vp, replacer, space, sb))
        return false;
    if (sb.empty()) {
        HandlePropertyName null = cx->names().null;
        return callback(null->chars(), null->length(), data);
    }
    return callback(sb.begin(), sb.length(), data);
}

JS_PUBLIC_API(bool)
JS_ParseJSON(JSContext *cx, const jschar *chars, uint32_t len, JS::MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RootedValue reviver(cx, NullValue());
    return ParseJSONWithReviver(cx, ConstTwoByteChars(chars, len), len, reviver, vp);
}

JS_PUBLIC_API(bool)
JS_ParseJSONWithReviver(JSContext *cx, const jschar *chars, uint32_t len, HandleValue reviver, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return ParseJSONWithReviver(cx, ConstTwoByteChars(chars, len), len, reviver, vp);
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_ReportError(JSContext *cx, const char *format, ...)
{
    va_list ap;

    AssertHeapIsIdle(cx);
    va_start(ap, format);
    js_ReportErrorVA(cx, JSREPORT_ERROR, format, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumber(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const unsigned errorNumber, ...)
{
    va_list ap;
    va_start(ap, errorNumber);
    JS_ReportErrorNumberVA(cx, errorCallback, userRef, errorNumber, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberVA(JSContext *cx, JSErrorCallback errorCallback,
                       void *userRef, const unsigned errorNumber,
                       va_list ap)
{
    AssertHeapIsIdle(cx);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
                           errorNumber, ArgumentsAreASCII, ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUC(JSContext *cx, JSErrorCallback errorCallback,
                       void *userRef, const unsigned errorNumber, ...)
{
    va_list ap;

    AssertHeapIsIdle(cx);
    va_start(ap, errorNumber);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
                           errorNumber, ArgumentsAreUnicode, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUCArray(JSContext *cx, JSErrorCallback errorCallback,
                            void *userRef, const unsigned errorNumber,
                            const jschar **args)
{
    AssertHeapIsIdle(cx);
    js_ReportErrorNumberUCArray(cx, JSREPORT_ERROR, errorCallback, userRef,
                                errorNumber, args);
}

JS_PUBLIC_API(bool)
JS_ReportWarning(JSContext *cx, const char *format, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);
    va_start(ap, format);
    ok = js_ReportErrorVA(cx, JSREPORT_WARNING, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(bool)
JS_ReportErrorFlagsAndNumber(JSContext *cx, unsigned flags,
                             JSErrorCallback errorCallback, void *userRef,
                             const unsigned errorNumber, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);
    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, ArgumentsAreASCII, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(bool)
JS_ReportErrorFlagsAndNumberUC(JSContext *cx, unsigned flags,
                               JSErrorCallback errorCallback, void *userRef,
                               const unsigned errorNumber, ...)
{
    va_list ap;
    bool ok;

    AssertHeapIsIdle(cx);
    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, ArgumentsAreUnicode, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(void)
JS_ReportOutOfMemory(JSContext *cx)
{
    js_ReportOutOfMemory(cx);
}

JS_PUBLIC_API(void)
JS_ReportAllocationOverflow(JSContext *cx)
{
    js_ReportAllocationOverflow(cx);
}

JS_PUBLIC_API(JSErrorReporter)
JS_GetErrorReporter(JSContext *cx)
{
    return cx->errorReporter;
}

JS_PUBLIC_API(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er)
{
    JSErrorReporter older;

    older = cx->errorReporter;
    cx->errorReporter = er;
    return older;
}

/************************************************************************/

/*
 * Dates.
 */
JS_PUBLIC_API(JSObject *)
JS_NewDateObject(JSContext *cx, int year, int mon, int mday, int hour, int min, int sec)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_NewDateObject(cx, year, mon, mday, hour, min, sec);
}

JS_PUBLIC_API(JSObject *)
JS_NewDateObjectMsec(JSContext *cx, double msec)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return js_NewDateObjectMsec(cx, msec);
}

JS_PUBLIC_API(bool)
JS_ObjectIsDate(JSContext *cx, HandleObject obj)
{
    assertSameCompartment(cx, obj);
    return ObjectClassIs(obj, ESClass_Date, cx);
}

JS_PUBLIC_API(void)
JS_ClearDateCaches(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    cx->runtime()->dateTimeInfo.updateTimeZoneAdjustment();
}

/************************************************************************/

/*
 * Regular Expressions.
 */
JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, HandleObject obj, char *bytes, size_t length, unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    jschar *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return nullptr;

    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics();
    RegExpObject *reobj = RegExpObject::create(cx, res, chars, length,
                                               RegExpFlag(flags), nullptr);
    js_free(chars);
    return reobj;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, HandleObject obj, jschar *chars, size_t length,
                     unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics();
    return RegExpObject::create(cx, res, chars, length,
                                RegExpFlag(flags), nullptr);
}

JS_PUBLIC_API(void)
JS_SetRegExpInput(JSContext *cx, HandleObject obj, HandleString input, bool multiline)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, input);

    obj->as<GlobalObject>().getRegExpStatics()->reset(cx, input, !!multiline);
}

JS_PUBLIC_API(void)
JS_ClearRegExpStatics(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    JS_ASSERT(obj);

    obj->as<GlobalObject>().getRegExpStatics()->clear();
}

JS_PUBLIC_API(bool)
JS_ExecuteRegExp(JSContext *cx, HandleObject obj, HandleObject reobj, jschar *chars,
                 size_t length, size_t *indexp, bool test, MutableHandleValue rval)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    RegExpStatics *res = obj->as<GlobalObject>().getRegExpStatics();

    return ExecuteRegExpLegacy(cx, res, reobj->as<RegExpObject>(), NullPtr(), chars, length, indexp,
                               test, rval);
}

JS_PUBLIC_API(JSObject *)
JS_NewRegExpObjectNoStatics(JSContext *cx, char *bytes, size_t length, unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    jschar *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return nullptr;
    RegExpObject *reobj = RegExpObject::createNoStatics(cx, chars, length,
                                                        RegExpFlag(flags), nullptr);
    js_free(chars);
    return reobj;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObjectNoStatics(JSContext *cx, jschar *chars, size_t length, unsigned flags)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    return RegExpObject::createNoStatics(cx, chars, length,
                                         RegExpFlag(flags), nullptr);
}

JS_PUBLIC_API(bool)
JS_ExecuteRegExpNoStatics(JSContext *cx, HandleObject obj, jschar *chars, size_t length,
                          size_t *indexp, bool test, MutableHandleValue rval)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return ExecuteRegExpLegacy(cx, nullptr, obj->as<RegExpObject>(), NullPtr(), chars, length,
                               indexp, test, rval);
}

JS_PUBLIC_API(bool)
JS_ObjectIsRegExp(JSContext *cx, HandleObject obj)
{
    assertSameCompartment(cx, obj);
    return ObjectClassIs(obj, ESClass_RegExp, cx);
}

JS_PUBLIC_API(unsigned)
JS_GetRegExpFlags(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return obj->as<RegExpObject>().getFlags();
}

JS_PUBLIC_API(JSString *)
JS_GetRegExpSource(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    return obj->as<RegExpObject>().getSource();
}

/************************************************************************/

JS_PUBLIC_API(bool)
JS_SetDefaultLocale(JSRuntime *rt, const char *locale)
{
    AssertHeapIsIdle(rt);
    return rt->setDefaultLocale(locale);
}

JS_PUBLIC_API(void)
JS_ResetDefaultLocale(JSRuntime *rt)
{
    AssertHeapIsIdle(rt);
    rt->resetDefaultLocale();
}

JS_PUBLIC_API(void)
JS_SetLocaleCallbacks(JSRuntime *rt, JSLocaleCallbacks *callbacks)
{
    AssertHeapIsIdle(rt);
    rt->localeCallbacks = callbacks;
}

JS_PUBLIC_API(JSLocaleCallbacks *)
JS_GetLocaleCallbacks(JSRuntime *rt)
{
    /* This function can be called by a finalizer. */
    return rt->localeCallbacks;
}

/************************************************************************/

JS_PUBLIC_API(bool)
JS_IsExceptionPending(JSContext *cx)
{
    /* This function can be called by a finalizer. */
    return (bool) cx->isExceptionPending();
}

JS_PUBLIC_API(bool)
JS_GetPendingException(JSContext *cx, MutableHandleValue vp)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (!cx->isExceptionPending())
        return false;
    return cx->getPendingException(vp);
}

JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
    cx->setPendingException(value);
}

JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    cx->clearPendingException();
}

JS_PUBLIC_API(bool)
JS_ReportPendingException(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    // This can only fail due to oom.
    bool ok = js_ReportUncaughtException(cx);
    JS_ASSERT(!cx->isExceptionPending());
    return ok;
}

JS::AutoSaveExceptionState::AutoSaveExceptionState(JSContext *cx)
    : context(cx), wasThrowing(cx->throwing), exceptionValue(cx)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (wasThrowing) {
        exceptionValue = cx->unwrappedException_;
        cx->clearPendingException();
    }
}

void
JS::AutoSaveExceptionState::restore()
{
    context->throwing = wasThrowing;
    context->unwrappedException_ = exceptionValue;
    drop();
}

JS::AutoSaveExceptionState::~AutoSaveExceptionState()
{
    if (wasThrowing && !context->isExceptionPending()) {
        context->throwing = true;
        context->unwrappedException_ = exceptionValue;
    }
}

struct JSExceptionState {
    bool throwing;
    jsval exception;
};

JS_PUBLIC_API(JSExceptionState *)
JS_SaveExceptionState(JSContext *cx)
{
    JSExceptionState *state;

    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    state = cx->pod_malloc<JSExceptionState>();
    if (state) {
        state->throwing =
            JS_GetPendingException(cx, MutableHandleValue::fromMarkedLocation(&state->exception));
        if (state->throwing && JSVAL_IS_GCTHING(state->exception))
            AddValueRoot(cx, &state->exception, "JSExceptionState.exception");
    }
    return state;
}

JS_PUBLIC_API(void)
JS_RestoreExceptionState(JSContext *cx, JSExceptionState *state)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing)
            JS_SetPendingException(cx, HandleValue::fromMarkedLocation(&state->exception));
        else
            JS_ClearPendingException(cx);
        JS_DropExceptionState(cx, state);
    }
}

JS_PUBLIC_API(void)
JS_DropExceptionState(JSContext *cx, JSExceptionState *state)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing && JSVAL_IS_GCTHING(state->exception)) {
            assertSameCompartment(cx, state->exception);
            JS_RemoveValueRoot(cx, &state->exception);
        }
        js_free(state);
    }
}

JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, HandleObject obj)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return js_ErrorFromException(cx, obj);
}

JS_PUBLIC_API(bool)
JS_ThrowStopIteration(JSContext *cx)
{
    AssertHeapIsIdle(cx);
    return js_ThrowStopIteration(cx);
}

JS_PUBLIC_API(bool)
JS_IsStopIteration(jsval v)
{
    return v.isObject() && v.toObject().is<StopIterationObject>();
}

JS_PUBLIC_API(intptr_t)
JS_GetCurrentThread()
{
#ifdef JS_THREADSAFE
    return reinterpret_cast<intptr_t>(PR_GetCurrentThread());
#else
    return 0;
#endif
}

extern MOZ_NEVER_INLINE JS_PUBLIC_API(void)
JS_AbortIfWrongThread(JSRuntime *rt)
{
    if (!CurrentThreadCanAccessRuntime(rt))
        MOZ_CRASH();
    if (!js::TlsPerThreadData.get()->associatedWith(rt))
        MOZ_CRASH();
}

#ifdef JS_GC_ZEAL
JS_PUBLIC_API(void)
JS_SetGCZeal(JSContext *cx, uint8_t zeal, uint32_t frequency)
{
    SetGCZeal(cx->runtime(), zeal, frequency);
}

JS_PUBLIC_API(void)
JS_ScheduleGC(JSContext *cx, uint32_t count)
{
    cx->runtime()->gcNextScheduled = count;
}
#endif

JS_PUBLIC_API(void)
JS_SetParallelParsingEnabled(JSContext *cx, bool enabled)
{
#ifdef JS_ION
    cx->runtime()->setParallelParsingEnabled(enabled);
#endif
}

JS_PUBLIC_API(void)
JS_SetParallelIonCompilationEnabled(JSContext *cx, bool enabled)
{
#ifdef JS_ION
    cx->runtime()->setParallelIonCompilationEnabled(enabled);
#endif
}

JS_PUBLIC_API(void)
JS_SetGlobalJitCompilerOption(JSContext *cx, JSJitCompilerOption opt, uint32_t value)
{
#ifdef JS_ION

    switch (opt) {
      case JSJITCOMPILER_BASELINE_USECOUNT_TRIGGER:
        if (value == uint32_t(-1)) {
            jit::JitOptions defaultValues;
            value = defaultValues.baselineUsesBeforeCompile;
        }
        jit::js_JitOptions.baselineUsesBeforeCompile = value;
        break;
      case JSJITCOMPILER_ION_USECOUNT_TRIGGER:
        if (value == uint32_t(-1)) {
            jit::js_JitOptions.resetUsesBeforeCompile();
            break;
        }
        jit::js_JitOptions.setUsesBeforeCompile(value);
        if (value == 0)
            jit::js_JitOptions.setEagerCompilation();
        break;
      case JSJITCOMPILER_ION_ENABLE:
        if (value == 1) {
            JS::ContextOptionsRef(cx).setIon(true);
            IonSpew(js::jit::IonSpew_Scripts, "Enable ion");
        } else if (value == 0) {
            JS::ContextOptionsRef(cx).setIon(false);
            IonSpew(js::jit::IonSpew_Scripts, "Disable ion");
        }
        break;
      case JSJITCOMPILER_BASELINE_ENABLE:
        if (value == 1) {
            JS::ContextOptionsRef(cx).setBaseline(true);
            IonSpew(js::jit::IonSpew_BaselineScripts, "Enable baseline");
        } else if (value == 0) {
            JS::ContextOptionsRef(cx).setBaseline(false);
            IonSpew(js::jit::IonSpew_BaselineScripts, "Disable baseline");
        }
        break;
      default:
        break;
    }
#endif
}

JS_PUBLIC_API(int)
JS_GetGlobalJitCompilerOption(JSContext *cx, JSJitCompilerOption opt)
{
#ifdef JS_ION
    switch (opt) {
      case JSJITCOMPILER_BASELINE_USECOUNT_TRIGGER:
        return jit::js_JitOptions.baselineUsesBeforeCompile;
      case JSJITCOMPILER_ION_USECOUNT_TRIGGER:
        return jit::js_JitOptions.forcedDefaultIonUsesBeforeCompile;
      case JSJITCOMPILER_ION_ENABLE:
        return JS::ContextOptionsRef(cx).ion();
      case JSJITCOMPILER_BASELINE_ENABLE:
        return JS::ContextOptionsRef(cx).baseline();
      default:
        break;
    }
#endif
    return 0;
}

/************************************************************************/

#if !defined(STATIC_EXPORTABLE_JS_API) && !defined(STATIC_JS_API) && defined(XP_WIN)

#include "jswin.h"

/*
 * Initialization routine for the JS DLL.
 */
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    return TRUE;
}

#endif

JS_PUBLIC_API(bool)
JS_IndexToId(JSContext *cx, uint32_t index, MutableHandleId id)
{
    return IndexToId(cx, index, id);
}

JS_PUBLIC_API(bool)
JS_CharsToId(JSContext* cx, JS::TwoByteChars chars, MutableHandleId idp)
{
    RootedAtom atom(cx, AtomizeChars(cx, chars.start().get(), chars.length()));
    if (!atom)
        return false;
#ifdef DEBUG
    uint32_t dummy;
    MOZ_ASSERT(!atom->isIndex(&dummy), "API misuse: |chars| must not encode an index");
#endif
    idp.set(AtomToId(atom));
    return true;
}

JS_PUBLIC_API(bool)
JS_IsIdentifier(JSContext *cx, HandleString str, bool *isIdentifier)
{
    assertSameCompartment(cx, str);

    JSLinearString* linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return false;

    *isIdentifier = js::frontend::IsIdentifier(linearStr);
    return true;
}

namespace JS {

void
AutoFilename::reset(void *newScriptSource)
{
    if (newScriptSource)
        reinterpret_cast<ScriptSource*>(newScriptSource)->incref();
    if (scriptSource_)
        reinterpret_cast<ScriptSource*>(scriptSource_)->decref();
    scriptSource_ = newScriptSource;
}

const char *
AutoFilename::get() const
{
    JS_ASSERT(scriptSource_);
    return reinterpret_cast<ScriptSource*>(scriptSource_)->filename();
}

JS_PUBLIC_API(bool)
DescribeScriptedCaller(JSContext *cx, AutoFilename *filename, unsigned *lineno)
{
    if (lineno)
        *lineno = 0;

    NonBuiltinFrameIter i(cx);
    if (i.done())
        return false;

    // If the caller is hidden, the embedding wants us to return false here so
    // that it can check its own stack (see HideScriptedCaller).
    if (i.activation()->scriptedCallerIsHidden())
        return false;

    if (filename)
        filename->reset(i.scriptSource());
    if (lineno)
        *lineno = i.computeLine();
    return true;
}

JS_PUBLIC_API(JSObject *)
GetScriptedCallerGlobal(JSContext *cx)
{
    NonBuiltinFrameIter i(cx);
    if (i.done())
        return nullptr;

    // If the caller is hidden, the embedding wants us to return null here so
    // that it can check its own stack (see HideScriptedCaller).
    if (i.activation()->scriptedCallerIsHidden())
        return nullptr;

    GlobalObject *global = i.activation()->compartment()->maybeGlobal();

    // Noone should be running code in the atoms compartment or running code in
    // a compartment without any live objects, so there should definitely be a
    // live global.
    JS_ASSERT(global);

    return global;
}

JS_PUBLIC_API(void)
HideScriptedCaller(JSContext *cx)
{
    MOZ_ASSERT(cx);

    // If there's no accessible activation on the stack, we'll return null from
    // DescribeScriptedCaller anyway, so there's no need to annotate anything.
    Activation *act = cx->runtime()->mainThread.activation();
    if (!act)
        return;
    act->hideScriptedCaller();
}

JS_PUBLIC_API(void)
UnhideScriptedCaller(JSContext *cx)
{
    Activation *act = cx->runtime()->mainThread.activation();
    if (!act)
        return;
    act->unhideScriptedCaller();
}

} /* namespace JS */

#ifdef JS_THREADSAFE
static PRStatus
CallOnce(void *func)
{
    JSInitCallback init = JS_DATA_TO_FUNC_PTR(JSInitCallback, func);
    return init() ? PR_SUCCESS : PR_FAILURE;
}
#endif

JS_PUBLIC_API(bool)
JS_CallOnce(JSCallOnceType *once, JSInitCallback func)
{
#ifdef JS_THREADSAFE
    return PR_CallOnceWithArg(once, CallOnce, JS_FUNC_TO_DATA_PTR(void *, func)) == PR_SUCCESS;
#else
    if (!*once) {
        *once = true;
        return func();
    } else {
        return true;
    }
#endif
}

AutoGCRooter::AutoGCRooter(JSContext *cx, ptrdiff_t tag)
  : down(ContextFriendFields::get(cx)->autoGCRooters),
    tag_(tag),
    stackTop(&ContextFriendFields::get(cx)->autoGCRooters)
{
    JS_ASSERT(this != *stackTop);
    *stackTop = this;
}

AutoGCRooter::AutoGCRooter(ContextFriendFields *cx, ptrdiff_t tag)
  : down(cx->autoGCRooters),
    tag_(tag),
    stackTop(&cx->autoGCRooters)
{
    JS_ASSERT(this != *stackTop);
    *stackTop = this;
}

#ifdef DEBUG
JS_PUBLIC_API(void)
JS::AssertArgumentsAreSane(JSContext *cx, HandleValue value)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);
}
#endif /* DEBUG */

JS_PUBLIC_API(void *)
JS_EncodeScript(JSContext *cx, HandleScript scriptArg, uint32_t *lengthp)
{
    XDREncoder encoder(cx);
    RootedScript script(cx, scriptArg);
    if (!encoder.codeScript(&script))
        return nullptr;
    return encoder.forgetData(lengthp);
}

JS_PUBLIC_API(void *)
JS_EncodeInterpretedFunction(JSContext *cx, HandleObject funobjArg, uint32_t *lengthp)
{
    XDREncoder encoder(cx);
    RootedObject funobj(cx, funobjArg);
    if (!encoder.codeFunction(&funobj))
        return nullptr;
    return encoder.forgetData(lengthp);
}

JS_PUBLIC_API(JSScript *)
JS_DecodeScript(JSContext *cx, const void *data, uint32_t length,
                JSPrincipals *principals, JSPrincipals *originPrincipals)
{
    XDRDecoder decoder(cx, data, length, principals, originPrincipals);
    RootedScript script(cx);
    if (!decoder.codeScript(&script))
        return nullptr;
    return script;
}

JS_PUBLIC_API(JSObject *)
JS_DecodeInterpretedFunction(JSContext *cx, const void *data, uint32_t length,
                             JSPrincipals *principals, JSPrincipals *originPrincipals)
{
    XDRDecoder decoder(cx, data, length, principals, originPrincipals);
    RootedObject funobj(cx);
    if (!decoder.codeFunction(&funobj))
        return nullptr;
    return funobj;
}

JS_PUBLIC_API(bool)
JS_PreventExtensions(JSContext *cx, JS::HandleObject obj)
{
    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return false;
    if (!extensible)
        return true;
    return JSObject::preventExtensions(cx, obj);
}

JS_PUBLIC_API(void)
JS::SetAsmJSCacheOps(JSRuntime *rt, const JS::AsmJSCacheOps *ops)
{
    rt->asmJSCacheOps = *ops;
}

char *
JSAutoByteString::encodeLatin1(ExclusiveContext *cx, JSString *str)
{
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return nullptr;

    mBytes = LossyTwoByteCharsToNewLatin1CharsZ(cx, linear->range()).c_str();
    return mBytes;
}

JS_PUBLIC_API(void)
JS::SetLargeAllocationFailureCallback(JSRuntime *rt, JS::LargeAllocationFailureCallback lafc)
{
    rt->largeAllocationFailureCallback = lafc;
}

JS_PUBLIC_API(void)
JS::SetOutOfMemoryCallback(JSRuntime *rt, OutOfMemoryCallback cb)
{
    rt->oomCallback = cb;
}

