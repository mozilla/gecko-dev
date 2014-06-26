/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TestingFunctions.h"

#include "mozilla/Move.h"
#include "mozilla/Scoped.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsobj.h"
#ifndef JS_MORE_DETERMINISTIC
#include "jsprf.h"
#endif
#include "jswrapper.h"

#include "jit/AsmJS.h"
#include "jit/AsmJSLink.h"
#include "js/HashTable.h"
#include "js/StructuredClone.h"
#include "js/UbiNode.h"
#include "js/UbiNodeTraverse.h"
#include "js/Vector.h"
#include "vm/ForkJoin.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/ProxyObject.h"
#include "vm/SavedStacks.h"
#include "vm/TraceLogging.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace JS;

using mozilla::ArrayLength;
using mozilla::Move;
using mozilla::ScopedFreePtr;

// If fuzzingSafe is set, remove functionality that could cause problems with
// fuzzers. Set this via the environment variable MOZ_FUZZING_SAFE.
static bool fuzzingSafe = false;

static bool
GetBuildConfiguration(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject info(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!info)
        return false;

    RootedValue value(cx, BooleanValue(false));
    if (!JS_SetProperty(cx, info, "rooting-analysis", value))
        return false;

#ifdef JSGC_USE_EXACT_ROOTING
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "exact-rooting", value))
        return false;

#ifdef DEBUG
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "debug", value))
        return false;

#ifdef JS_HAS_CTYPES
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "has-ctypes", value))
        return false;

#ifdef JS_CPU_X86
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "x86", value))
        return false;

#ifdef JS_CPU_X64
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "x64", value))
        return false;

#ifdef JS_ARM_SIMULATOR
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "arm-simulator", value))
        return false;

#ifdef MOZ_ASAN
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "asan", value))
        return false;

#ifdef JS_GC_ZEAL
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "has-gczeal", value))
        return false;

#ifdef JS_THREADSAFE
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "threadsafe", value))
        return false;

#ifdef JS_MORE_DETERMINISTIC
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "more-deterministic", value))
        return false;

#ifdef MOZ_PROFILING
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "profiling", value))
        return false;

#ifdef INCLUDE_MOZILLA_DTRACE
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "dtrace", value))
        return false;

#ifdef MOZ_TRACE_JSCALLS
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "trace-jscalls-api", value))
        return false;

#ifdef JSGC_INCREMENTAL
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "incremental-gc", value))
        return false;

#ifdef JSGC_GENERATIONAL
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "generational-gc", value))
        return false;

#ifdef MOZ_VALGRIND
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "valgrind", value))
        return false;

#ifdef JS_OOM_DO_BACKTRACES
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "oom-backtraces", value))
        return false;

#ifdef ENABLE_PARALLEL_JS
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "parallelJS", value))
        return false;

#ifdef ENABLE_BINARYDATA
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "binary-data", value))
        return false;

#ifdef EXPOSE_INTL_API
    value = BooleanValue(true);
#else
    value = BooleanValue(false);
#endif
    if (!JS_SetProperty(cx, info, "intl-api", value))
        return false;

    args.rval().setObject(*info);
    return true;
}

static bool
GC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /*
     * If the first argument is 'compartment', we collect any compartments
     * previously scheduled for GC via schedulegc. If the first argument is an
     * object, we collect the object's compartment (and any other compartments
     * scheduled for GC). Otherwise, we collect all compartments.
     */
    bool compartment = false;
    if (args.length() == 1) {
        Value arg = args[0];
        if (arg.isString()) {
            if (!JS_StringEqualsAscii(cx, arg.toString(), "compartment", &compartment))
                return false;
        } else if (arg.isObject()) {
            PrepareZoneForGC(UncheckedUnwrap(&arg.toObject())->zone());
            compartment = true;
        }
    }

#ifndef JS_MORE_DETERMINISTIC
    size_t preBytes = cx->runtime()->gc.bytes;
#endif

    if (compartment)
        PrepareForDebugGC(cx->runtime());
    else
        PrepareForFullGC(cx->runtime());
    GCForReason(cx->runtime(), gcreason::API);

    char buf[256] = { '\0' };
#ifndef JS_MORE_DETERMINISTIC
    JS_snprintf(buf, sizeof(buf), "before %lu, after %lu\n",
                (unsigned long)preBytes, (unsigned long)cx->runtime()->gc.bytes);
#endif
    JSString *str = JS_NewStringCopyZ(cx, buf);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
MinorGC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#ifdef JSGC_GENERATIONAL
    if (args.get(0) == BooleanValue(true))
        cx->runtime()->gc.storeBuffer.setAboutToOverflow();

    MinorGC(cx, gcreason::API);
#endif
    args.rval().setUndefined();
    return true;
}

static const struct ParamPair {
    const char      *name;
    JSGCParamKey    param;
} paramMap[] = {
    {"maxBytes",            JSGC_MAX_BYTES },
    {"maxMallocBytes",      JSGC_MAX_MALLOC_BYTES},
    {"gcBytes",             JSGC_BYTES},
    {"gcNumber",            JSGC_NUMBER},
    {"sliceTimeBudget",     JSGC_SLICE_TIME_BUDGET},
    {"markStackLimit",      JSGC_MARK_STACK_LIMIT}
};

// Keep this in sync with above params.
#define GC_PARAMETER_ARGS_LIST "maxBytes, maxMallocBytes, gcBytes, gcNumber, sliceTimeBudget, or markStackLimit"

static bool
GCParameter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSString *str = ToString(cx, args.get(0));
    if (!str)
        return false;

    JSFlatString *flatStr = JS_FlattenString(cx, str);
    if (!flatStr)
        return false;

    size_t paramIndex = 0;
    for (;; paramIndex++) {
        if (paramIndex == ArrayLength(paramMap)) {
            JS_ReportError(cx,
                           "the first argument must be one of " GC_PARAMETER_ARGS_LIST);
            return false;
        }
        if (JS_FlatStringEqualsAscii(flatStr, paramMap[paramIndex].name))
            break;
    }
    JSGCParamKey param = paramMap[paramIndex].param;

    // Request mode.
    if (args.length() == 1) {
        uint32_t value = JS_GetGCParameter(cx->runtime(), param);
        args.rval().setNumber(value);
        return true;
    }

    if (param == JSGC_NUMBER || param == JSGC_BYTES) {
        JS_ReportError(cx, "Attempt to change read-only parameter %s",
                       paramMap[paramIndex].name);
        return false;
    }

    uint32_t value;
    if (!ToUint32(cx, args[1], &value))
        return false;

    if (!value) {
        JS_ReportError(cx, "the second argument must be convertable to uint32_t "
                           "with non-zero value");
        return false;
    }

    if (param == JSGC_MARK_STACK_LIMIT && IsIncrementalGCInProgress(cx->runtime())) {
        JS_ReportError(cx, "attempt to set markStackLimit while a GC is in progress");
        return false;
    }

    if (param == JSGC_MAX_BYTES) {
        uint32_t gcBytes = JS_GetGCParameter(cx->runtime(), JSGC_BYTES);
        if (value < gcBytes) {
            JS_ReportError(cx,
                           "attempt to set maxBytes to the value less than the current "
                           "gcBytes (%u)",
                           gcBytes);
            return false;
        }
    }

    JS_SetGCParameter(cx->runtime(), param, value);
    args.rval().setUndefined();
    return true;
}

static bool
IsProxy(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportError(cx, "the function takes exactly one argument");
        return false;
    }
    if (!args[0].isObject()) {
        args.rval().setBoolean(false);
        return true;
    }
    args.rval().setBoolean(args[0].toObject().is<ProxyObject>());
    return true;
}

static bool
IsLazyFunction(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportError(cx, "The function takes exactly one argument.");
        return false;
    }
    if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        JS_ReportError(cx, "The first argument should be a function.");
        return true;
    }
    args.rval().setBoolean(args[0].toObject().as<JSFunction>().isInterpretedLazy());
    return true;
}

static bool
IsRelazifiableFunction(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportError(cx, "The function takes exactly one argument.");
        return false;
    }
    if (!args[0].isObject() ||
        !args[0].toObject().is<JSFunction>())
    {
        JS_ReportError(cx, "The first argument should be a function.");
        return true;
    }

    JSFunction *fun = &args[0].toObject().as<JSFunction>();
    args.rval().setBoolean(fun->hasScript() && fun->nonLazyScript()->isRelazifiable());
    return true;
}

static bool
InternalConst(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        JS_ReportError(cx, "the function takes exactly one argument");
        return false;
    }

    JSString *str = ToString(cx, args[0]);
    if (!str)
        return false;
    JSFlatString *flat = JS_FlattenString(cx, str);
    if (!flat)
        return false;

    if (JS_FlatStringEqualsAscii(flat, "INCREMENTAL_MARK_STACK_BASE_CAPACITY")) {
        args.rval().setNumber(uint32_t(js::INCREMENTAL_MARK_STACK_BASE_CAPACITY));
    } else {
        JS_ReportError(cx, "unknown const name");
        return false;
    }
    return true;
}

static bool
GCPreserveCode(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 0) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    cx->runtime()->gc.setAlwaysPreserveCode();

    args.rval().setUndefined();
    return true;
}

#ifdef JS_GC_ZEAL
static bool
GCZeal(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() > 2) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Too many arguments");
        return false;
    }

    uint32_t zeal;
    if (!ToUint32(cx, args.get(0), &zeal))
        return false;

    uint32_t frequency = JS_DEFAULT_ZEAL_FREQ;
    if (args.length() >= 2) {
        if (!ToUint32(cx, args.get(1), &frequency))
            return false;
    }

    JS_SetGCZeal(cx, (uint8_t)zeal, frequency);
    args.rval().setUndefined();
    return true;
}

static bool
ScheduleGC(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (args[0].isInt32()) {
        /* Schedule a GC to happen after |arg| allocations. */
        JS_ScheduleGC(cx, args[0].toInt32());
    } else if (args[0].isObject()) {
        /* Ensure that |zone| is collected during the next GC. */
        Zone *zone = UncheckedUnwrap(&args[0].toObject())->zone();
        PrepareZoneForGC(zone);
    } else if (args[0].isString()) {
        /* This allows us to schedule atomsCompartment for GC. */
        PrepareZoneForGC(args[0].toString()->zone());
    }

    args.rval().setUndefined();
    return true;
}

static bool
SelectForGC(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /*
     * The selectedForMarking set is intended to be manually marked at slice
     * start to detect missing pre-barriers. It is invalid for nursery things
     * to be in the set, so evict the nursery before adding items.
     */
    JSRuntime *rt = cx->runtime();
    MinorGC(rt, JS::gcreason::EVICT_NURSERY);

    for (unsigned i = 0; i < args.length(); i++) {
        if (args[i].isObject()) {
            if (!rt->gc.selectForMarking(&args[i].toObject()))
                return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static bool
VerifyPreBarriers(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() > 0) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Too many arguments");
        return false;
    }

    gc::VerifyBarriers(cx->runtime(), gc::PreBarrierVerifier);
    args.rval().setUndefined();
    return true;
}

static bool
VerifyPostBarriers(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length()) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Too many arguments");
        return false;
    }
    gc::VerifyBarriers(cx->runtime(), gc::PostBarrierVerifier);
    args.rval().setUndefined();
    return true;
}

static bool
GCState(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 0) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Too many arguments");
        return false;
    }

    const char *state;
    gc::State globalState = cx->runtime()->gc.state();
    if (globalState == gc::NO_INCREMENTAL)
        state = "none";
    else if (globalState == gc::MARK)
        state = "mark";
    else if (globalState == gc::SWEEP)
        state = "sweep";
    else
        MOZ_ASSUME_UNREACHABLE("Unobserveable global GC state");

    JSString *str = JS_NewStringCopyZ(cx, state);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
DeterministicGC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    cx->runtime()->gc.setDeterministic(ToBoolean(args[0]));
    args.rval().setUndefined();
    return true;
}
#endif /* JS_GC_ZEAL */

static bool
GCSlice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() > 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    bool limit = true;
    uint32_t budget = 0;
    if (args.length() == 1) {
        if (!ToUint32(cx, args[0], &budget))
            return false;
    } else {
        limit = false;
    }

    GCDebugSlice(cx->runtime(), limit, budget);
    args.rval().setUndefined();
    return true;
}

static bool
ValidateGC(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    cx->runtime()->gc.setValidate(ToBoolean(args[0]));
    args.rval().setUndefined();
    return true;
}

static bool
FullCompartmentChecks(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    cx->runtime()->gc.setFullCompartmentChecks(ToBoolean(args[0]));
    args.rval().setUndefined();
    return true;
}

static bool
NondeterministicGetWeakMapKeys(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        RootedObject callee(cx, &args.callee());
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }
    if (!args[0].isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE,
                             "nondeterministicGetWeakMapKeys", "WeakMap",
                             InformalValueTypeName(args[0]));
        return false;
    }
    RootedObject arr(cx);
    RootedObject mapObj(cx, &args[0].toObject());
    if (!JS_NondeterministicGetWeakMapKeys(cx, mapObj, &arr))
        return false;
    if (!arr) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE,
                             "nondeterministicGetWeakMapKeys", "WeakMap",
                             args[0].toObject().getClass()->name);
        return false;
    }
    args.rval().setObject(*arr);
    return true;
}

struct JSCountHeapNode {
    void                *thing;
    JSGCTraceKind       kind;
    JSCountHeapNode     *next;
};

typedef HashSet<void *, PointerHasher<void *, 3>, SystemAllocPolicy> VisitedSet;

class CountHeapTracer
{
  public:
    CountHeapTracer(JSRuntime *rt, JSTraceCallback callback) : base(rt, callback) {}

    JSTracer            base;
    VisitedSet          visited;
    JSCountHeapNode     *traceList;
    JSCountHeapNode     *recycleList;
    bool                ok;
};

static void
CountHeapNotify(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JS_ASSERT(trc->callback == CountHeapNotify);

    CountHeapTracer *countTracer = (CountHeapTracer *)trc;
    void *thing = *thingp;

    if (!countTracer->ok)
        return;

    VisitedSet::AddPtr p = countTracer->visited.lookupForAdd(thing);
    if (p)
        return;

    if (!countTracer->visited.add(p, thing)) {
        countTracer->ok = false;
        return;
    }

    JSCountHeapNode *node = countTracer->recycleList;
    if (node) {
        countTracer->recycleList = node->next;
    } else {
        node = js_pod_malloc<JSCountHeapNode>();
        if (!node) {
            countTracer->ok = false;
            return;
        }
    }
    node->thing = thing;
    node->kind = kind;
    node->next = countTracer->traceList;
    countTracer->traceList = node;
}

static const struct TraceKindPair {
    const char       *name;
    int32_t           kind;
} traceKindNames[] = {
    { "all",        -1                  },
    { "object",     JSTRACE_OBJECT      },
    { "string",     JSTRACE_STRING      },
    { "symbol",     JSTRACE_SYMBOL      },
};

static bool
CountHeap(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue startValue(cx, UndefinedValue());
    if (args.length() > 0) {
        jsval v = args[0];
        if (v.isMarkable()) {
            startValue = v;
        } else if (!v.isNull()) {
            JS_ReportError(cx,
                           "the first argument is not null or a heap-allocated "
                           "thing");
            return false;
        }
    }

    RootedValue traceValue(cx);
    int32_t traceKind = -1;
    void *traceThing = nullptr;
    if (args.length() > 1) {
        JSString *str = ToString(cx, args[1]);
        if (!str)
            return false;
        JSFlatString *flatStr = JS_FlattenString(cx, str);
        if (!flatStr)
            return false;
        if (JS_FlatStringEqualsAscii(flatStr, "specific")) {
            if (args.length() < 3) {
                JS_ReportError(cx, "tracing of specific value requested "
                               "but no value provided");
                return false;
            }
            traceValue = args[2];
            if (!traceValue.isMarkable()){
                JS_ReportError(cx, "cannot trace this kind of value");
                return false;
            }
            traceThing = traceValue.toGCThing();
        } else {
            for (size_t i = 0; ;) {
                if (JS_FlatStringEqualsAscii(flatStr, traceKindNames[i].name)) {
                    traceKind = traceKindNames[i].kind;
                    break;
                }
                if (++i == ArrayLength(traceKindNames)) {
                    JSAutoByteString bytes(cx, str);
                    if (!!bytes)
                        JS_ReportError(cx, "trace kind name '%s' is unknown", bytes.ptr());
                    return false;
                }
            }
        }
    }

    CountHeapTracer countTracer(JS_GetRuntime(cx), CountHeapNotify);
    if (!countTracer.visited.init()) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    countTracer.ok = true;
    countTracer.traceList = nullptr;
    countTracer.recycleList = nullptr;

    if (startValue.isUndefined()) {
        JS_TraceRuntime(&countTracer.base);
    } else {
        JS_CallValueTracer(&countTracer.base, startValue.address(), "root");
    }

    JSCountHeapNode *node;
    size_t counter = 0;
    while ((node = countTracer.traceList) != nullptr) {
        if (traceThing == nullptr) {
            // We are looking for all nodes with a specific kind
            if (traceKind == -1 || node->kind == traceKind)
                counter++;
        } else {
            // We are looking for some specific thing
            if (node->thing == traceThing)
                counter++;
        }
        countTracer.traceList = node->next;
        node->next = countTracer.recycleList;
        countTracer.recycleList = node;
        JS_TraceChildren(&countTracer.base, node->thing, node->kind);
    }
    while ((node = countTracer.recycleList) != nullptr) {
        countTracer.recycleList = node->next;
        js_free(node);
    }
    if (!countTracer.ok) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    args.rval().setNumber(double(counter));
    return true;
}

static bool
GetSavedFrameCount(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(cx->compartment()->savedStacks().count());
    return true;
}

static bool
SaveStack(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<JSObject*> stack(cx);
    if (!JS::CaptureCurrentStack(cx, &stack))
        return false;
    args.rval().setObject(*stack);
    return true;
}

static bool
EnableTrackAllocations(JSContext *cx, unsigned argc, jsval *vp)
{
    SetObjectMetadataCallback(cx, SavedStacksMetadataCallback);
    return true;
}

static bool
DisableTrackAllocations(JSContext *cx, unsigned argc, jsval *vp)
{
    SetObjectMetadataCallback(cx, nullptr);
    return true;
}

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
static bool
OOMAfterAllocations(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportError(cx, "count argument required");
        return false;
    }

    uint32_t count;
    if (!JS::ToUint32(cx, args[0], &count))
        return false;

    OOM_maxAllocations = OOM_counter + count;
    return true;
}
#endif

static unsigned finalizeCount = 0;

static void
finalize_counter_finalize(JSFreeOp *fop, JSObject *obj)
{
    ++finalizeCount;
}

static const JSClass FinalizeCounterClass = {
    "FinalizeCounter", JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize_counter_finalize
};

static bool
MakeFinalizeObserver(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject scope(cx, JS::CurrentGlobalOrNull(cx));
    if (!scope)
        return false;

    JSObject *obj = JS_NewObjectWithGivenProto(cx, &FinalizeCounterClass, JS::NullPtr(), scope);
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
FinalizeCount(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(finalizeCount);
    return true;
}

static bool
DumpHeapComplete(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    DumpHeapNurseryBehaviour nurseryBehaviour = js::IgnoreNurseryObjects;
    FILE *dumpFile = nullptr;

    unsigned i = 0;
    if (args.length() > i) {
        Value v = args[i];
        if (v.isString()) {
            JSString *str = v.toString();
            bool same = false;
            if (!JS_StringEqualsAscii(cx, str, "collectNurseryBeforeDump", &same))
                return false;
            if (same) {
                nurseryBehaviour = js::CollectNurseryBeforeDump;
                ++i;
            }
        }
    }

    if (args.length() > i) {
        Value v = args[i];
        if (v.isString()) {
            if (!fuzzingSafe) {
                JSString *str = v.toString();
                JSAutoByteString fileNameBytes;
                if (!fileNameBytes.encodeLatin1(cx, str))
                    return false;
                const char *fileName = fileNameBytes.ptr();
                dumpFile = fopen(fileName, "w");
                if (!dumpFile) {
                    JS_ReportError(cx, "can't open %s", fileName);
                    return false;
                }
            }
            ++i;
        }
    }

    if (i != args.length()) {
        JS_ReportError(cx, "bad arguments passed to dumpHeapComplete");
        return false;
    }

    js::DumpHeapComplete(JS_GetRuntime(cx), dumpFile ? dumpFile : stdout, nurseryBehaviour);

    if (dumpFile)
        fclose(dumpFile);

    args.rval().setUndefined();
    return true;
}

static bool
Terminate(JSContext *cx, unsigned arg, jsval *vp)
{
#ifdef JS_MORE_DETERMINISTIC
    // Print a message to stderr in more-deterministic builds to help jsfunfuzz
    // find uncatchable-exception bugs.
    fprintf(stderr, "terminate called\n");
#endif

    JS_ClearPendingException(cx);
    return false;
}

#define SPS_PROFILING_STACK_MAX_SIZE 1000
static ProfileEntry SPS_PROFILING_STACK[SPS_PROFILING_STACK_MAX_SIZE];
static uint32_t SPS_PROFILING_STACK_SIZE = 0;

static bool
EnableSPSProfiling(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Disable before re-enabling; see the assertion in |SPSProfiler::setProfilingStack|.
    if (cx->runtime()->spsProfiler.installed())
        cx->runtime()->spsProfiler.enable(false);

    SetRuntimeProfilingStack(cx->runtime(), SPS_PROFILING_STACK, &SPS_PROFILING_STACK_SIZE,
                             SPS_PROFILING_STACK_MAX_SIZE);
    cx->runtime()->spsProfiler.enableSlowAssertions(false);
    cx->runtime()->spsProfiler.enable(true);

    args.rval().setUndefined();
    return true;
}

static bool
EnableSPSProfilingWithSlowAssertions(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();

    if (cx->runtime()->spsProfiler.enabled()) {
        // If profiling already enabled with slow assertions disabled,
        // this is a no-op.
        if (cx->runtime()->spsProfiler.slowAssertionsEnabled())
            return true;

        // Slow assertions are off.  Disable profiling before re-enabling
        // with slow assertions on.
        cx->runtime()->spsProfiler.enable(false);
    }

    // Disable before re-enabling; see the assertion in |SPSProfiler::setProfilingStack|.
    if (cx->runtime()->spsProfiler.installed())
        cx->runtime()->spsProfiler.enable(false);

    SetRuntimeProfilingStack(cx->runtime(), SPS_PROFILING_STACK, &SPS_PROFILING_STACK_SIZE,
                             SPS_PROFILING_STACK_MAX_SIZE);
    cx->runtime()->spsProfiler.enableSlowAssertions(true);
    cx->runtime()->spsProfiler.enable(true);

    return true;
}

static bool
DisableSPSProfiling(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (cx->runtime()->spsProfiler.installed())
        cx->runtime()->spsProfiler.enable(false);
    args.rval().setUndefined();
    return true;
}

static bool
EnableOsiPointRegisterChecks(JSContext *, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#if defined(JS_ION) && defined(CHECK_OSIPOINT_REGISTERS)
    jit::js_JitOptions.checkOsiPointRegisters = true;
#endif
    args.rval().setUndefined();
    return true;
}

static bool
DisplayName(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isObject() || !args[0].toObject().is<JSFunction>()) {
        RootedObject arg(cx, &args.callee());
        ReportUsageError(cx, arg, "Must have one function argument");
        return false;
    }

    JSFunction *fun = &args[0].toObject().as<JSFunction>();
    JSString *str = fun->displayAtom();
    args.rval().setString(str ? str : cx->runtime()->emptyString);
    return true;
}

bool
js::testingFunc_inParallelSection(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // If we were actually *in* a parallel section, then this function
    // would be inlined to TRUE in ion-generated code.
    JS_ASSERT(!InParallelSection());
    args.rval().setBoolean(false);
    return true;
}

static bool
ShellObjectMetadataCallback(JSContext *cx, JSObject **pmetadata)
{
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &JSObject::class_));
    if (!obj)
        return false;

    RootedObject stack(cx, NewDenseEmptyArray(cx));
    if (!stack)
        return false;

    static int createdIndex = 0;
    createdIndex++;

    if (!JS_DefineProperty(cx, obj, "index", createdIndex, 0,
                           JS_PropertyStub, JS_StrictPropertyStub))
    {
        return false;
    }

    if (!JS_DefineProperty(cx, obj, "stack", stack, 0,
                           JS_PropertyStub, JS_StrictPropertyStub))
    {
        return false;
    }

    int stackIndex = 0;
    RootedId id(cx);
    RootedValue callee(cx);
    for (NonBuiltinScriptFrameIter iter(cx); !iter.done(); ++iter) {
        if (iter.isFunctionFrame() && iter.compartment() == cx->compartment()) {
            id = INT_TO_JSID(stackIndex);
            RootedObject callee(cx, iter.callee());
            if (!JS_DefinePropertyById(cx, stack, id, callee, 0,
                                       JS_PropertyStub, JS_StrictPropertyStub))
            {
                return false;
            }
            stackIndex++;
        }
    }

    *pmetadata = obj;
    return true;
}

static bool
SetObjectMetadataCallback(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool enabled = args.length() ? ToBoolean(args[0]) : false;
    SetObjectMetadataCallback(cx, enabled ? ShellObjectMetadataCallback : nullptr);

    args.rval().setUndefined();
    return true;
}

static bool
SetObjectMetadata(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 2 || !args[0].isObject() || !args[1].isObject()) {
        JS_ReportError(cx, "Both arguments must be objects");
        return false;
    }

    args.rval().setUndefined();

    RootedObject obj(cx, &args[0].toObject());
    RootedObject metadata(cx, &args[1].toObject());
    return SetObjectMetadata(cx, obj, metadata);
}

static bool
GetObjectMetadata(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 || !args[0].isObject()) {
        JS_ReportError(cx, "Argument must be an object");
        return false;
    }

    args.rval().setObjectOrNull(GetObjectMetadata(&args[0].toObject()));
    return true;
}

bool
js::testingFunc_bailout(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // NOP when not in IonMonkey
    args.rval().setUndefined();
    return true;
}

bool
js::testingFunc_assertFloat32(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // NOP when not in IonMonkey
    args.rval().setUndefined();
    return true;
}

static bool
SetJitCompilerOption(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() != 2) {
        ReportUsageError(cx, callee, "Wrong number of arguments.");
        return false;
    }

    if (!args[0].isString()) {
        ReportUsageError(cx, callee, "First argument must be a String.");
        return false;
    }

    if (!args[1].isInt32()) {
        ReportUsageError(cx, callee, "Second argument must be an Int32.");
        return false;
    }

    JSFlatString *strArg = JS_FlattenString(cx, args[0].toString());

#define JIT_COMPILER_MATCH(key, string)                 \
    else if (JS_FlatStringEqualsAscii(strArg, string))  \
        opt = JSJITCOMPILER_ ## key;

    JSJitCompilerOption opt = JSJITCOMPILER_NOT_AN_OPTION;
    if (false) {}
    JIT_COMPILER_OPTIONS(JIT_COMPILER_MATCH);
#undef JIT_COMPILER_MATCH

    if (opt == JSJITCOMPILER_NOT_AN_OPTION) {
        ReportUsageError(cx, callee, "First argument does not name a valid option (see jsapi.h).");
        return false;
    }

    int32_t number = args[1].toInt32();
    if (number < 0)
        number = -1;

    JS_SetGlobalJitCompilerOption(cx->runtime(), opt, uint32_t(number));

    args.rval().setUndefined();
    return true;
}

static bool
GetJitCompilerOptions(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject info(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!info)
        return false;

    RootedValue value(cx);

#define JIT_COMPILER_MATCH(key, string)                                \
    opt = JSJITCOMPILER_ ## key;                                       \
    value.setInt32(JS_GetGlobalJitCompilerOption(cx->runtime(), opt)); \
    if (!JS_SetProperty(cx, info, string, value))                      \
        return false;

    JSJitCompilerOption opt = JSJITCOMPILER_NOT_AN_OPTION;
    JIT_COMPILER_OPTIONS(JIT_COMPILER_MATCH);
#undef JIT_COMPILER_MATCH

    args.rval().setObject(*info);

    return true;
}

static bool
SetIonCheckGraphCoherency(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#ifdef JS_ION
    jit::js_JitOptions.checkGraphConsistency = ToBoolean(args.get(0));
#endif
    args.rval().setUndefined();
    return true;
}

class CloneBufferObject : public JSObject {
    static const JSPropertySpec props_[2];
    static const size_t DATA_SLOT   = 0;
    static const size_t LENGTH_SLOT = 1;
    static const size_t NUM_SLOTS   = 2;

  public:
    static const Class class_;

    static CloneBufferObject *Create(JSContext *cx) {
        RootedObject obj(cx, JS_NewObject(cx, Jsvalify(&class_), JS::NullPtr(), JS::NullPtr()));
        if (!obj)
            return nullptr;
        obj->setReservedSlot(DATA_SLOT, PrivateValue(nullptr));
        obj->setReservedSlot(LENGTH_SLOT, Int32Value(0));

        if (!JS_DefineProperties(cx, obj, props_))
            return nullptr;

        return &obj->as<CloneBufferObject>();
    }

    static CloneBufferObject *Create(JSContext *cx, JSAutoStructuredCloneBuffer *buffer) {
        Rooted<CloneBufferObject*> obj(cx, Create(cx));
        if (!obj)
            return nullptr;
        uint64_t *datap;
        size_t nbytes;
        buffer->steal(&datap, &nbytes);
        obj->setData(datap);
        obj->setNBytes(nbytes);
        return obj;
    }

    uint64_t *data() const {
        return static_cast<uint64_t*>(getReservedSlot(DATA_SLOT).toPrivate());
    }

    void setData(uint64_t *aData) {
        JS_ASSERT(!data());
        setReservedSlot(DATA_SLOT, PrivateValue(aData));
    }

    size_t nbytes() const {
        return getReservedSlot(LENGTH_SLOT).toInt32();
    }

    void setNBytes(size_t nbytes) {
        JS_ASSERT(nbytes <= UINT32_MAX);
        setReservedSlot(LENGTH_SLOT, Int32Value(nbytes));
    }

    // Discard an owned clone buffer.
    void discard() {
        if (data())
            JS_ClearStructuredClone(data(), nbytes(), nullptr, nullptr);
        setReservedSlot(DATA_SLOT, PrivateValue(nullptr));
    }

    static bool
    setCloneBuffer_impl(JSContext* cx, CallArgs args) {
        if (args.length() != 1 || !args[0].isString()) {
            JS_ReportError(cx,
                           "the first argument argument must be maxBytes, "
                           "maxMallocBytes, gcStackpoolLifespan, gcBytes or "
                           "gcNumber");
            JS_ReportError(cx, "clonebuffer setter requires a single string argument");
            return false;
        }

        if (fuzzingSafe) {
            // A manually-created clonebuffer could easily trigger a crash
            args.rval().setUndefined();
            return true;
        }

        Rooted<CloneBufferObject*> obj(cx, &args.thisv().toObject().as<CloneBufferObject>());
        obj->discard();

        char *str = JS_EncodeString(cx, args[0].toString());
        if (!str)
            return false;
        obj->setData(reinterpret_cast<uint64_t*>(str));
        obj->setNBytes(JS_GetStringLength(args[0].toString()));

        args.rval().setUndefined();
        return true;
    }

    static bool
    is(HandleValue v) {
        return v.isObject() && v.toObject().is<CloneBufferObject>();
    }

    static bool
    setCloneBuffer(JSContext* cx, unsigned int argc, JS::Value* vp) {
        CallArgs args = CallArgsFromVp(argc, vp);
        return CallNonGenericMethod<is, setCloneBuffer_impl>(cx, args);
    }

    static bool
    getCloneBuffer_impl(JSContext* cx, CallArgs args) {
        Rooted<CloneBufferObject*> obj(cx, &args.thisv().toObject().as<CloneBufferObject>());
        JS_ASSERT(args.length() == 0);

        if (!obj->data()) {
            args.rval().setUndefined();
            return true;
        }

        bool hasTransferable;
        if (!JS_StructuredCloneHasTransferables(obj->data(), obj->nbytes(), &hasTransferable))
            return false;

        if (hasTransferable) {
            JS_ReportError(cx, "cannot retrieve structured clone buffer with transferables");
            return false;
        }

        JSString *str = JS_NewStringCopyN(cx, reinterpret_cast<char*>(obj->data()), obj->nbytes());
        if (!str)
            return false;
        args.rval().setString(str);
        return true;
    }

    static bool
    getCloneBuffer(JSContext* cx, unsigned int argc, JS::Value* vp) {
        CallArgs args = CallArgsFromVp(argc, vp);
        return CallNonGenericMethod<is, getCloneBuffer_impl>(cx, args);
    }

    static void Finalize(FreeOp *fop, JSObject *obj) {
        obj->as<CloneBufferObject>().discard();
    }
};

const Class CloneBufferObject::class_ = {
    "CloneBuffer", JSCLASS_HAS_RESERVED_SLOTS(CloneBufferObject::NUM_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    Finalize,
    nullptr,                  /* call */
    nullptr,                  /* hasInstance */
    nullptr,                  /* construct */
    nullptr,                  /* trace */
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    JS_NULL_OBJECT_OPS
};

const JSPropertySpec CloneBufferObject::props_[] = {
    JS_PSGS("clonebuffer", getCloneBuffer, setCloneBuffer, 0),
    JS_PS_END
};

static bool
Serialize(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSAutoStructuredCloneBuffer clonebuf;
    if (!clonebuf.write(cx, args.get(0), args.get(1)))
        return false;

    RootedObject obj(cx, CloneBufferObject::Create(cx, &clonebuf));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Deserialize(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1 || !args[0].isObject()) {
        JS_ReportError(cx, "deserialize requires a single clonebuffer argument");
        return false;
    }

    if (!args[0].toObject().is<CloneBufferObject>()) {
        JS_ReportError(cx, "deserialize requires a clonebuffer");
        return false;
    }

    Rooted<CloneBufferObject*> obj(cx, &args[0].toObject().as<CloneBufferObject>());

    // Clone buffer was already consumed?
    if (!obj->data()) {
        JS_ReportError(cx, "deserialize given invalid clone buffer "
                       "(transferables already consumed?)");
        return false;
    }

    bool hasTransferable;
    if (!JS_StructuredCloneHasTransferables(obj->data(), obj->nbytes(), &hasTransferable))
        return false;

    RootedValue deserialized(cx);
    if (!JS_ReadStructuredClone(cx, obj->data(), obj->nbytes(),
                                JS_STRUCTURED_CLONE_VERSION, &deserialized, nullptr, nullptr)) {
        return false;
    }
    args.rval().set(deserialized);

    if (hasTransferable)
        obj->discard();

    return true;
}

static bool
Neuter(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 2) {
        JS_ReportError(cx, "wrong number of arguments to neuter()");
        return false;
    }

    RootedObject obj(cx);
    if (!JS_ValueToObject(cx, args[0], &obj))
        return false;

    if (!obj) {
        JS_ReportError(cx, "neuter must be passed an object");
        return false;
    }

    NeuterDataDisposition changeData;
    RootedString str(cx, JS::ToString(cx, args[1]));
    if (!str)
        return false;
    JSAutoByteString dataDisposition(cx, str);
    if (!dataDisposition)
        return false;
    if (strcmp(dataDisposition.ptr(), "same-data") == 0) {
        changeData = KeepData;
    } else if (strcmp(dataDisposition.ptr(), "change-data") == 0) {
        changeData = ChangeData;
    } else {
        JS_ReportError(cx, "unknown parameter 2 to neuter()");
        return false;
    }

    if (!JS_NeuterArrayBuffer(cx, obj, changeData))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
HelperThreadCount(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#ifdef JS_THREADSAFE
    args.rval().setInt32(HelperThreadState().threadCount);
#else
    args.rval().setInt32(0);
#endif
    return true;
}

static bool
TimesAccessed(JSContext *cx, unsigned argc, jsval *vp)
{
    static int32_t accessed = 0;
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(++accessed);
    return true;
}

static bool
EnableTraceLogger(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    args.rval().setBoolean(TraceLoggerEnable(logger));

    return true;
}

static bool
DisableTraceLogger(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    args.rval().setBoolean(TraceLoggerDisable(logger));

    return true;
}

#ifdef DEBUG
static bool
DumpObject(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx);
    if (!JS_ConvertArguments(cx, args, "o", obj.address()))
        return false;

    js_DumpObject(obj);

    args.rval().setUndefined();
    return true;
}
#endif

static bool
ReportOutOfMemory(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ReportOutOfMemory(cx);
    cx->clearPendingException();
    args.rval().setUndefined();
    return true;
}

static bool
ReportLargeAllocationFailure(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    void *buf = cx->runtime()->onOutOfMemoryCanGC(NULL, JSRuntime::LARGE_ALLOCATION);
    js_free(buf);
    args.rval().setUndefined();
    return true;
}

namespace heaptools {

// An edge to a node from its predecessor in a path through the graph.
class BackEdge {
    // The node from which this edge starts.
    JS::ubi::Node predecessor_;

    // The name of this edge. We own this storage.
    ScopedFreePtr<jschar> name_;

  public:
    BackEdge() : name_(nullptr) { }
    // Construct an initialized back edge. Take ownership of |name|.
    BackEdge(JS::ubi::Node predecessor, jschar *name)
        : predecessor_(predecessor), name_(name) { }
    BackEdge(BackEdge &&rhs) : predecessor_(rhs.predecessor_), name_(rhs.name_.forget()) { }
    BackEdge &operator=(BackEdge &&rhs) {
        MOZ_ASSERT(&rhs != this);
        this->~BackEdge();
        new(this) BackEdge(Move(rhs));
        return *this;
    }

    jschar *forgetName() { return name_.forget(); }
    JS::ubi::Node predecessor() const { return predecessor_; }

  private:
    // No copy constructor or copying assignment.
    BackEdge(const BackEdge &) MOZ_DELETE;
    BackEdge &operator=(const BackEdge &) MOZ_DELETE;
};

// A path-finding handler class for use with JS::ubi::BreadthFirst.
struct FindPathHandler {
    typedef BackEdge NodeData;
    typedef JS::ubi::BreadthFirst<FindPathHandler> Traversal;

    FindPathHandler(JS::ubi::Node start, JS::ubi::Node target,
                    AutoValueVector &nodes, Vector<ScopedFreePtr<jschar> > &edges)
      : start(start), target(target), foundPath(false),
        nodes(nodes), edges(edges) { }

    bool
    operator()(Traversal &traversal, JS::ubi::Node origin, const JS::ubi::Edge &edge,
               BackEdge *backEdge, bool first)
    {
        // We take care of each node the first time we visit it, so there's
        // nothing to be done on subsequent visits.
        if (!first)
            return true;

        // Record how we reached this node. This is the last edge on a
        // shortest path to this node.
        jschar *edgeName = js_strdup(traversal.cx, edge.name);
        if (!edgeName)
            return false;
        *backEdge = mozilla::Move(BackEdge(origin, edgeName));

        // Have we reached our final target node?
        if (edge.referent == target) {
            // Record the path that got us here, which must be a shortest path.
            if (!recordPath(traversal))
                return false;
            foundPath = true;
            traversal.stop();
        }

        return true;
    }

    // We've found a path to our target. Walk the backlinks to produce the
    // (reversed) path, saving the path in |nodes| and |edges|. |nodes| is
    // rooted, so it can hold the path's nodes as we leave the scope of
    // the AutoCheckCannotGC.
    bool recordPath(Traversal &traversal) {
        JS::ubi::Node here = target;

        do {
            Traversal::NodeMap::Ptr p = traversal.visited.lookup(here);
            MOZ_ASSERT(p);
            JS::ubi::Node predecessor = p->value().predecessor();
            if (!nodes.append(predecessor.exposeToJS()) ||
                !edges.append(p->value().forgetName()))
                return false;
            here = predecessor;
        } while (here != start);

        return true;
    }

    // The node we're starting from.
    JS::ubi::Node start;

    // The node we're looking for.
    JS::ubi::Node target;

    // True if we found a path to target, false if we didn't.
    bool foundPath;

    // The nodes and edges of the path --- should we find one. The path is
    // stored in reverse order, because that's how it's easiest for us to
    // construct it:
    // - edges[i] is the name of the edge from nodes[i] to nodes[i-1].
    // - edges[0] is the name of the edge from nodes[0] to the target.
    // - The last node, nodes[n-1], is the start node.
    AutoValueVector &nodes;
    Vector<ScopedFreePtr<jschar> > &edges;
};

} // namespace heaptools

static bool
FindPath(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "findPath", "1", "");
        return false;
    }

    // We don't ToString non-objects given as 'start' or 'target'. We can't
    // see edges to non-string primitive values, and it doesn't make much
    // sense to ask for paths to or from a freshly allocated string, so
    // if a non-string primitive appears here it's probably a mistake.
    if (!args[0].isObject() && !args[0].isString()) {
        js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_UNEXPECTED_TYPE,
                                 JSDVG_SEARCH_STACK, args[0], JS::NullPtr(),
                                 "neither an object nor a string", NULL);
        return false;
    }

    if (!args[1].isObject() && !args[1].isString()) {
        js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_UNEXPECTED_TYPE,
                                 JSDVG_SEARCH_STACK, args[0], JS::NullPtr(),
                                 "neither an object nor a string", NULL);
        return false;
    }

    AutoValueVector nodes(cx);
    Vector<ScopedFreePtr<jschar> > edges(cx);

    {
        // We can't tolerate the GC moving things around while we're searching
        // the heap. Check that nothing we do causes a GC.
        JS::AutoCheckCannotGC autoCannotGC;

        JS::ubi::Node start(args[0]), target(args[1]);

        heaptools::FindPathHandler handler(start, target, nodes, edges);
        heaptools::FindPathHandler::Traversal traversal(cx, handler, autoCannotGC);
        if (!traversal.init() || !traversal.addStart(start))
            return false;

        if (!traversal.traverse())
            return false;

        if (!handler.foundPath) {
            // We didn't find any paths from the start to the target.
            args.rval().setUndefined();
            return true;
        }
    }

    // |nodes| and |edges| contain the path from |start| to |target|, reversed.
    // Construct a JavaScript array describing the path from the start to the
    // target. Each element has the form:
    //
    //   { node: <object or string>, edge: <string describing outgoing edge from node> }
    //
    // or, if the node is some internal thing, that isn't a proper
    // JavaScript value:
    //
    //   { node: undefined, edge: <string> }
    size_t length = nodes.length();
    RootedObject result(cx, NewDenseAllocatedArray(cx, length));
    if (!result)
        return false;
    result->ensureDenseInitializedLength(cx, 0, length);

    // Walk |nodes| and |edges| in the stored order, and construct the result
    // array in start-to-target order.
    for (size_t i = 0; i < length; i++) {
        // Build an object describing the node and edge.
        RootedObject obj(cx, NewBuiltinClassInstance<JSObject>(cx));
        if (!obj)
            return false;

        if (!JS_DefineProperty(cx, obj, "node", nodes[i],
                               JSPROP_ENUMERATE, nullptr, nullptr))
            return false;

        RootedString edge(cx, NewString<CanGC>(cx, edges[i].get(), js_strlen(edges[i])));
        if (!edge)
            return false;
        edges[i].forget();
        RootedValue edgeString(cx, StringValue(edge));
        if (!JS_DefineProperty(cx, obj, "edge", edgeString,
                               JSPROP_ENUMERATE, nullptr, nullptr))
            return false;

        result->setDenseElement(length - i - 1, ObjectValue(*obj));
    }

    args.rval().setObject(*result);
    return true;
}

static const JSFunctionSpecWithHelp TestingFunctions[] = {
    JS_FN_HELP("gc", ::GC, 0, 0,
"gc([obj] | 'compartment')",
"  Run the garbage collector. When obj is given, GC only its compartment.\n"
"  If 'compartment' is given, GC any compartments that were scheduled for\n"
"  GC via schedulegc."),

    JS_FN_HELP("minorgc", ::MinorGC, 0, 0,
"minorgc([aboutToOverflow])",
"  Run a minor collector on the Nursery. When aboutToOverflow is true, marks\n"
"  the store buffer as about-to-overflow before collecting."),

    JS_FN_HELP("gcparam", GCParameter, 2, 0,
"gcparam(name [, value])",
"  Wrapper for JS_[GS]etGCParameter. The name is one of " GC_PARAMETER_ARGS_LIST),

    JS_FN_HELP("getBuildConfiguration", GetBuildConfiguration, 0, 0,
"getBuildConfiguration()",
"  Return an object describing some of the configuration options SpiderMonkey\n"
"  was built with."),

    JS_FN_HELP("countHeap", CountHeap, 0, 0,
"countHeap([start[, kind[, thing]]])",
"  Count the number of live GC things in the heap or things reachable from\n"
"  start when it is given and is not null. kind is either 'all' (default) to\n"
"  count all things or one of 'object', 'double', 'string', 'function'\n"
"  to count only things of that kind. If kind is the string 'specific',\n"
"  then you can provide an extra argument with some specific traceable\n"
"  thing to count.\n"),

    JS_FN_HELP("getSavedFrameCount", GetSavedFrameCount, 0, 0,
"getSavedFrameCount()",
"  Return the number of SavedFrame instances stored in this compartment's\n"
"  SavedStacks cache."),

    JS_FN_HELP("saveStack", SaveStack, 0, 0,
"saveStack()",
"  Capture a stack.\n"),

    JS_FN_HELP("enableTrackAllocations", EnableTrackAllocations, 0, 0,
"enableTrackAllocations()",
"  Start capturing the JS stack at every allocation. Note that this sets an "
"  object metadata callback that will override any other object metadata "
"  callback that may be set."),

    JS_FN_HELP("disableTrackAllocations", DisableTrackAllocations, 0, 0,
"disableTrackAllocations()",
"  Stop capturing the JS stack at every allocation."),

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
    JS_FN_HELP("oomAfterAllocations", OOMAfterAllocations, 1, 0,
"oomAfterAllocations(count)",
"  After 'count' js_malloc memory allocations, fail every following allocation\n"
"  (return NULL)."),
#endif

    JS_FN_HELP("makeFinalizeObserver", MakeFinalizeObserver, 0, 0,
"makeFinalizeObserver()",
"  Get a special object whose finalization increases the counter returned\n"
"  by the finalizeCount function."),

    JS_FN_HELP("finalizeCount", FinalizeCount, 0, 0,
"finalizeCount()",
"  Return the current value of the finalization counter that is incremented\n"
"  each time an object returned by the makeFinalizeObserver is finalized."),

    JS_FN_HELP("gcPreserveCode", GCPreserveCode, 0, 0,
"gcPreserveCode()",
"  Preserve JIT code during garbage collections."),

#ifdef JS_GC_ZEAL
    JS_FN_HELP("gczeal", GCZeal, 2, 0,
"gczeal(level, [period])",
"  Specifies how zealous the garbage collector should be. Values for level:\n"
"    0: Normal amount of collection\n"
"    1: Collect when roots are added or removed\n"
"    2: Collect when memory is allocated\n"
"    3: Collect when the window paints (browser only)\n"
"    4: Verify pre write barriers between instructions\n"
"    5: Verify pre write barriers between paints\n"
"    6: Verify stack rooting\n"
"    7: Collect the nursery every N nursery allocations\n"
"    8: Incremental GC in two slices: 1) mark roots 2) finish collection\n"
"    9: Incremental GC in two slices: 1) mark all 2) new marking and finish\n"
"   10: Incremental GC in multiple slices\n"
"   11: Verify post write barriers between instructions\n"
"   12: Verify post write barriers between paints\n"
"   13: Check internal hashtables on minor GC\n"
"  Period specifies that collection happens every n allocations.\n"),

    JS_FN_HELP("schedulegc", ScheduleGC, 1, 0,
"schedulegc(num | obj)",
"  If num is given, schedule a GC after num allocations.\n"
"  If obj is given, schedule a GC of obj's compartment."),

    JS_FN_HELP("selectforgc", SelectForGC, 0, 0,
"selectforgc(obj1, obj2, ...)",
"  Schedule the given objects to be marked in the next GC slice."),

    JS_FN_HELP("verifyprebarriers", VerifyPreBarriers, 0, 0,
"verifyprebarriers()",
"  Start or end a run of the pre-write barrier verifier."),

    JS_FN_HELP("verifypostbarriers", VerifyPostBarriers, 0, 0,
"verifypostbarriers()",
"  Start or end a run of the post-write barrier verifier."),

    JS_FN_HELP("gcstate", GCState, 0, 0,
"gcstate()",
"  Report the global GC state."),

    JS_FN_HELP("deterministicgc", DeterministicGC, 1, 0,
"deterministicgc(true|false)",
"  If true, only allow determinstic GCs to run."),
#endif

    JS_FN_HELP("gcslice", GCSlice, 1, 0,
"gcslice(n)",
"  Run an incremental GC slice that marks about n objects."),

    JS_FN_HELP("validategc", ValidateGC, 1, 0,
"validategc(true|false)",
"  If true, a separate validation step is performed after an incremental GC."),

    JS_FN_HELP("fullcompartmentchecks", FullCompartmentChecks, 1, 0,
"fullcompartmentchecks(true|false)",
"  If true, check for compartment mismatches before every GC."),

    JS_FN_HELP("nondeterministicGetWeakMapKeys", NondeterministicGetWeakMapKeys, 1, 0,
"nondeterministicGetWeakMapKeys(weakmap)",
"  Return an array of the keys in the given WeakMap."),

    JS_FN_HELP("internalConst", InternalConst, 1, 0,
"internalConst(name)",
"  Query an internal constant for the engine. See InternalConst source for\n"
"  the list of constant names."),

    JS_FN_HELP("isProxy", IsProxy, 1, 0,
"isProxy(obj)",
"  If true, obj is a proxy of some sort"),

    JS_FN_HELP("dumpHeapComplete", DumpHeapComplete, 1, 0,
"dumpHeapComplete(['collectNurseryBeforeDump'], [filename])",
"  Dump reachable and unreachable objects to the named file, or to stdout.  If\n"
"  'collectNurseryBeforeDump' is specified, a minor GC is performed first,\n"
"  otherwise objects in the nursery are ignored."),

    JS_FN_HELP("terminate", Terminate, 0, 0,
"terminate()",
"  Terminate JavaScript execution, as if we had run out of\n"
"  memory or been terminated by the slow script dialog."),

    JS_FN_HELP("enableSPSProfiling", EnableSPSProfiling, 0, 0,
"enableSPSProfiling()",
"  Enables SPS instrumentation and corresponding assertions, with slow\n"
"  assertions disabled.\n"),

    JS_FN_HELP("enableSPSProfilingWithSlowAssertions", EnableSPSProfilingWithSlowAssertions, 0, 0,
"enableSPSProfilingWithSlowAssertions()",
"  Enables SPS instrumentation and corresponding assertions, with slow\n"
"  assertions enabled.\n"),

    JS_FN_HELP("disableSPSProfiling", DisableSPSProfiling, 0, 0,
"disableSPSProfiling()",
"  Disables SPS instrumentation"),

    JS_FN_HELP("enableOsiPointRegisterChecks", EnableOsiPointRegisterChecks, 0, 0,
"enableOsiPointRegisterChecks()",
"Emit extra code to verify live regs at the start of a VM call are not\n"
"modified before its OsiPoint."),

    JS_FN_HELP("displayName", DisplayName, 1, 0,
"displayName(fn)",
"  Gets the display name for a function, which can possibly be a guessed or\n"
"  inferred name based on where the function was defined. This can be\n"
"  different from the 'name' property on the function."),

    JS_FN_HELP("isAsmJSCompilationAvailable", IsAsmJSCompilationAvailable, 0, 0,
"isAsmJSCompilationAvailable",
"  Returns whether asm.js compilation is currently available or whether it is disabled\n"
"  (e.g., by the debugger)."),

    JS_FN_HELP("getJitCompilerOptions", GetJitCompilerOptions, 0, 0,
"getCompilerOptions()",
"Return an object describing some of the JIT compiler options.\n"),

    JS_FN_HELP("isAsmJSModule", IsAsmJSModule, 1, 0,
"isAsmJSModule(fn)",
"  Returns whether the given value is a function containing \"use asm\" that has been\n"
"  validated according to the asm.js spec."),

    JS_FN_HELP("isAsmJSModuleLoadedFromCache", IsAsmJSModuleLoadedFromCache, 1, 0,
"isAsmJSModuleLoadedFromCache(fn)",
"  Return whether the given asm.js module function has been loaded directly\n"
"  from the cache. This function throws an error if fn is not a validated asm.js\n"
"  module."),

    JS_FN_HELP("isAsmJSFunction", IsAsmJSFunction, 1, 0,
"isAsmJSFunction(fn)",
"  Returns whether the given value is a nested function in an asm.js module that has been\n"
"  both compile- and link-time validated."),

    JS_FN_HELP("isLazyFunction", IsLazyFunction, 1, 0,
"isLazyFunction(fun)",
"  True if fun is a lazy JSFunction."),

    JS_FN_HELP("isRelazifiableFunction", IsRelazifiableFunction, 1, 0,
"isRelazifiableFunction(fun)",
"  Ture if fun is a JSFunction with a relazifiable JSScript."),

    JS_FN_HELP("inParallelSection", testingFunc_inParallelSection, 0, 0,
"inParallelSection()",
"  True if this code is executing within a parallel section."),

    JS_FN_HELP("setObjectMetadataCallback", SetObjectMetadataCallback, 1, 0,
"setObjectMetadataCallback(fn)",
"  Specify function to supply metadata for all newly created objects."),

    JS_FN_HELP("setObjectMetadata", SetObjectMetadata, 2, 0,
"setObjectMetadata(obj, metadataObj)",
"  Change the metadata for an object."),

    JS_FN_HELP("getObjectMetadata", GetObjectMetadata, 1, 0,
"getObjectMetadata(obj)",
"  Get the metadata for an object."),

    JS_FN_HELP("bailout", testingFunc_bailout, 0, 0,
"bailout()",
"  Force a bailout out of ionmonkey (if running in ionmonkey)."),

    JS_FN_HELP("setJitCompilerOption", SetJitCompilerOption, 2, 0,
"setCompilerOption(<option>, <number>)",
"  Set a compiler option indexed in JSCompileOption enum to a number.\n"),

    JS_FN_HELP("setIonCheckGraphCoherency", SetIonCheckGraphCoherency, 1, 0,
"setIonCheckGraphCoherency(bool)",
"  Set whether Ion should perform graph consistency (DEBUG-only) assertions. These assertions\n"
"  are valuable and should be generally enabled, however they can be very expensive for large\n"
"  (asm.js) programs."),

    JS_FN_HELP("serialize", Serialize, 1, 0,
"serialize(data, [transferables])",
"  Serialize 'data' using JS_WriteStructuredClone. Returns a structured\n"
"  clone buffer object."),

    JS_FN_HELP("deserialize", Deserialize, 1, 0,
"deserialize(clonebuffer)",
"  Deserialize data generated by serialize."),

    JS_FN_HELP("neuter", Neuter, 1, 0,
"neuter(buffer, \"change-data\"|\"same-data\")",
"  Neuter the given ArrayBuffer object as if it had been transferred to a\n"
"  WebWorker. \"change-data\" will update the internal data pointer.\n"
"  \"same-data\" will leave it set to its original value, to mimic eg\n"
"  asm.js ArrayBuffer neutering."),

    JS_FN_HELP("helperThreadCount", HelperThreadCount, 0, 0,
"helperThreadCount()",
"  Returns the number of helper threads available for off-main-thread tasks."),

    JS_FN_HELP("startTraceLogger", EnableTraceLogger, 0, 0,
"startTraceLogger()",
"  Start logging the mainThread.\n"
"  Note: tracelogging starts automatically. Disable it by setting environment variable\n"
"  TLOPTIONS=disableMainThread"),

    JS_FN_HELP("stopTraceLogger", DisableTraceLogger, 0, 0,
"stopTraceLogger()",
"  Stop logging the mainThread."),

    JS_FN_HELP("reportOutOfMemory", ReportOutOfMemory, 0, 0,
"reportOutOfMemory()",
"  Report OOM, then clear the exception and return undefined. For crash testing."),

    JS_FN_HELP("reportLargeAllocationFailure", ReportLargeAllocationFailure, 0, 0,
"reportLargeAllocationFailure()",
"  Call the large allocation failure callback, as though a large malloc call failed,\n"
"  then return undefined. In Gecko, this sends a memory pressure notification, which\n"
"  can free up some memory."),

    JS_FN_HELP("findPath", FindPath, 2, 0,
"findPath(start, target)",
"  Return an array describing one of the shortest paths of GC heap edges from\n"
"  |start| to |target|, or |undefined| if |target| is unreachable from |start|.\n"
"  Each element of the array is either of the form:\n"
"    { node: <object or string>, edge: <string describing edge from node> }\n"
"  if the node is a JavaScript object or value; or of the form:\n"
"    { type: <string describing node>, edge: <string describing edge> }\n"
"  if the node is some internal thing that is not a proper JavaScript value\n"
"  (like a shape or a scope chain element). The destination of the i'th array\n"
"  element's edge is the node of the i+1'th array element; the destination of\n"
"  the last array element is implicitly |target|.\n"),

#ifdef DEBUG
    JS_FN_HELP("dumpObject", DumpObject, 1, 0,
"dumpObject()",
"  Dump an internal representation of an object."),
#endif

    JS_FS_HELP_END
};

static const JSPropertySpec TestingProperties[] = {
    JS_PSG("timesAccessed", TimesAccessed, 0),
    JS_PS_END
};

bool
js::DefineTestingFunctions(JSContext *cx, HandleObject obj, bool fuzzingSafe_)
{
    fuzzingSafe = fuzzingSafe_;
    if (getenv("MOZ_FUZZING_SAFE") && getenv("MOZ_FUZZING_SAFE")[0] != '0')
        fuzzingSafe = true;

    if (!JS_DefineProperties(cx, obj, TestingProperties))
        return false;

    return JS_DefineFunctionsWithHelp(cx, obj, TestingFunctions);
}
