/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ParallelFunctions.h"

#include "builtin/TypedObject.h"
#include "jit/arm/Simulator-arm.h"
#include "vm/ArrayObject.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace jit;

using parallel::Spew;
using parallel::SpewOps;
using parallel::SpewBailouts;
using parallel::SpewBailoutIR;

// Load the current thread context.
ForkJoinContext *
jit::ForkJoinContextPar()
{
    return ForkJoinContext::current();
}

// NewGCThingPar() is called in place of NewGCThing() when executing
// parallel code.  It uses the ArenaLists for the current thread and
// allocates from there.
JSObject *
jit::NewGCThingPar(ForkJoinContext *cx, gc::AllocKind allocKind)
{
    JS_ASSERT(ForkJoinContext::current() == cx);
    return js::NewGCObject<NoGC>(cx, allocKind, 0, gc::TenuredHeap);
}

bool
jit::ParallelWriteGuard(ForkJoinContext *cx, JSObject *object)
{
    // Implements the most general form of the write guard, which is
    // suitable for writes to any object O. There are two cases to
    // consider and test for:
    //
    // 1. Writes to thread-local memory are safe. Thread-local memory
    //    is defined as memory allocated by the current thread.
    //    The definition of the PJS API guarantees that such memory
    //    cannot have escaped to other parallel threads.
    //
    // 2. Writes into the output buffer are safe. Some PJS operations
    //    supply an out pointer into the final target buffer. The design
    //    of the API ensures that this out pointer is always pointing
    //    at a fresh region of the buffer that is not accessible to
    //    other threads. Thus, even though this output buffer has not
    //    been created by the current thread, it is writable.
    //
    // There are some subtleties to consider:
    //
    // A. Typed objects and typed arrays are just views onto a base buffer.
    //    For the purposes of guarding parallel writes, it is not important
    //    whether the *view* is thread-local -- what matters is whether
    //    the *underlying buffer* is thread-local.
    //
    // B. With regard to the output buffer, we have to be careful
    //    because of the potential for sequential iterations to be
    //    intermingled with parallel ones. During a sequential
    //    iteration, the out pointer could escape into global
    //    variables and so forth, and thus be used during later
    //    parallel operations. However, those out pointers must be
    //    pointing to distinct regions of the final output buffer than
    //    the ones that are currently being written, so there is no
    //    harm done in letting them be read (but not written).
    //
    //    In order to be able to distinguish escaped out pointers from
    //    prior iterations and the proper out pointers from the
    //    current iteration, we always track a *target memory region*
    //    (which is a span of bytes within the output buffer) and not
    //    just the output buffer itself.

    JS_ASSERT(ForkJoinContext::current() == cx);

    if (object->is<TypedDatum>()) {
        TypedDatum &datum = object->as<TypedDatum>();

        // Note: check target region based on `datum`, not the owner.
        // This is because `datum` may point to some subregion of the
        // owner and we only care if that *subregion* is within the
        // target region, not the entire owner.
        if (IsInTargetRegion(cx, &datum))
            return true;

        // Also check whether owner is thread-local.
        TypedDatum &owner = datum.owner();
        return cx->isThreadLocal(&owner);
    }

    // For other kinds of writable objects, must be thread-local.
    return cx->isThreadLocal(object);
}

// Check that |object| (which must be a typed datum) maps
// to memory in the target region.
//
// For efficiency, we assume that all handles which the user has
// access to are either entirely within the target region or entirely
// without, but not straddling the target region nor encompassing
// it. This invariant is maintained by the PJS APIs, where the target
// region and handles are always elements of the same output array.
bool
jit::IsInTargetRegion(ForkJoinContext *cx, TypedDatum *datum)
{
    JS_ASSERT(datum->is<TypedDatum>()); // in case JIT supplies something bogus
    uint8_t *typedMem = datum->typedMem();
    return (typedMem >= cx->targetRegionStart &&
            typedMem <  cx->targetRegionEnd);
}

#ifdef DEBUG
static void
printTrace(const char *prefix, struct IonLIRTraceData *cached)
{
    fprintf(stderr, "%s / Block %3u / LIR %3u / Mode %u / LIR %s\n",
            prefix,
            cached->blockIndex, cached->lirIndex, cached->execModeInt, cached->lirOpName);
}

static struct IonLIRTraceData seqTraceData;
#endif

void
jit::TraceLIR(IonLIRTraceData *current)
{
#ifdef DEBUG
    static enum { NotSet, All, Bailouts } traceMode;

    // If you set IONFLAGS=trace, this function will be invoked before every LIR.
    //
    // You can either modify it to do whatever you like, or use gdb scripting.
    // For example:
    //
    // break TraceLIR
    // commands
    // continue
    // exit

    if (traceMode == NotSet) {
        // Racy, but that's ok.
        const char *env = getenv("IONFLAGS");
        if (strstr(env, "trace-all"))
            traceMode = All;
        else
            traceMode = Bailouts;
    }

    IonLIRTraceData *cached;
    if (current->execModeInt == 0)
        cached = &seqTraceData;
    else
        cached = &ForkJoinContext::current()->traceData;

    if (current->blockIndex == 0xDEADBEEF) {
        if (current->execModeInt == 0)
            printTrace("BAILOUT", cached);
        else
            SpewBailoutIR(cached);
    }

    memcpy(cached, current, sizeof(IonLIRTraceData));

    if (traceMode == All)
        printTrace("Exec", cached);
#endif
}

bool
jit::CheckOverRecursedPar(ForkJoinContext *cx)
{
    JS_ASSERT(ForkJoinContext::current() == cx);
    int stackDummy_;

    // When an interrupt is triggered, the main thread stack limit is
    // overwritten with a sentinel value that brings us here.
    // Therefore, we must check whether this is really a stack overrun
    // and, if not, check whether an interrupt is needed.
    //
    // When not on the main thread, we don't overwrite the stack
    // limit, but we do still call into this routine if the interrupt
    // flag is set, so we still need to double check.

#ifdef JS_ARM_SIMULATOR
    if (Simulator::Current()->overRecursed()) {
        cx->bailoutRecord->setCause(ParallelBailoutOverRecursed);
        return false;
    }
#endif

    uintptr_t realStackLimit;
    if (cx->isMainThread())
        realStackLimit = GetNativeStackLimit(cx);
    else
        realStackLimit = cx->perThreadData->ionStackLimit;

    if (!JS_CHECK_STACK_SIZE(realStackLimit, &stackDummy_)) {
        cx->bailoutRecord->setCause(ParallelBailoutOverRecursed);
        return false;
    }

    return CheckInterruptPar(cx);
}

bool
jit::CheckInterruptPar(ForkJoinContext *cx)
{
    JS_ASSERT(ForkJoinContext::current() == cx);
    bool result = cx->check();
    if (!result) {
        // Do not set the cause here.  Either it was set by this
        // thread already by some code that then triggered an abort,
        // or else we are just picking up an abort from some other
        // thread.  Either way we have nothing useful to contribute so
        // we might as well leave our bailout case unset.
        return false;
    }
    return true;
}

JSObject *
jit::ExtendArrayPar(ForkJoinContext *cx, JSObject *array, uint32_t length)
{
    JSObject::EnsureDenseResult res =
        array->ensureDenseElementsPreservePackedFlag(cx, 0, length);
    if (res != JSObject::ED_OK)
        return nullptr;
    return array;
}

bool
jit::SetPropertyPar(ForkJoinContext *cx, HandleObject obj, HandlePropertyName name,
                    HandleValue value, bool strict, jsbytecode *pc)
{
    JS_ASSERT(cx->isThreadLocal(obj));

    if (*pc == JSOP_SETALIASEDVAR) {
        // See comment in jit::SetProperty.
        Shape *shape = obj->nativeLookupPure(name);
        JS_ASSERT(shape && shape->hasSlot());
        return obj->nativeSetSlotIfHasType(shape, value);
    }

    // Fail early on hooks.
    if (obj->getOps()->setProperty)
        return TP_RETRY_SEQUENTIALLY;

    RootedValue v(cx, value);
    RootedId id(cx, NameToId(name));
    return baseops::SetPropertyHelper<ParallelExecution>(cx, obj, obj, id, 0, &v, strict);
}

bool
jit::SetElementPar(ForkJoinContext *cx, HandleObject obj, HandleValue index, HandleValue value,
                   bool strict)
{
    RootedId id(cx);
    if (!ValueToIdPure(index, id.address()))
        return false;

    // SetObjectElementOperation, the sequential version, has several checks
    // for certain deoptimizing behaviors, such as marking having written to
    // holes and non-indexed element accesses. We don't do that here, as we
    // can't modify any TI state anyways. If we need to add a new type, we
    // would bail out.
    RootedValue v(cx, value);
    return baseops::SetPropertyHelper<ParallelExecution>(cx, obj, obj, id, 0, &v, strict);
}

JSString *
jit::ConcatStringsPar(ForkJoinContext *cx, HandleString left, HandleString right)
{
    return ConcatStrings<NoGC>(cx, left, right);
}

JSFlatString *
jit::IntToStringPar(ForkJoinContext *cx, int i)
{
    return Int32ToString<NoGC>(cx, i);
}

JSString *
jit::DoubleToStringPar(ForkJoinContext *cx, double d)
{
    return NumberToString<NoGC>(cx, d);
}

JSString *
jit::PrimitiveToStringPar(ForkJoinContext *cx, HandleValue input)
{
    // All other cases are handled in assembly.
    JS_ASSERT(input.isDouble() || input.isInt32());

    if (input.isInt32())
        return Int32ToString<NoGC>(cx, input.toInt32());

    return NumberToString<NoGC>(cx, input.toDouble());
}

bool
jit::StringToNumberPar(ForkJoinContext *cx, JSString *str, double *out)
{
    return StringToNumber(cx, str, out);
}

#define PAR_RELATIONAL_OP(OP, EXPECTED)                                         \
do {                                                                            \
    /* Optimize for two int-tagged operands (typical loop control). */          \
    if (lhs.isInt32() && rhs.isInt32()) {                                       \
        *res = (lhs.toInt32() OP rhs.toInt32()) == EXPECTED;                    \
    } else if (lhs.isNumber() && rhs.isNumber()) {                              \
        double l = lhs.toNumber(), r = rhs.toNumber();                          \
        *res = (l OP r) == EXPECTED;                                            \
    } else if (lhs.isBoolean() && rhs.isBoolean()) {                            \
        bool l = lhs.toBoolean();                                               \
        bool r = rhs.toBoolean();                                               \
        *res = (l OP r) == EXPECTED;                                            \
    } else if (lhs.isBoolean() && rhs.isNumber()) {                             \
        bool l = lhs.toBoolean();                                               \
        double r = rhs.toNumber();                                              \
        *res = (l OP r) == EXPECTED;                                            \
    } else if (lhs.isNumber() && rhs.isBoolean()) {                             \
        double l = lhs.toNumber();                                              \
        bool r = rhs.toBoolean();                                               \
        *res = (l OP r) == EXPECTED;                                            \
    } else {                                                                    \
        int32_t vsZero;                                                         \
        if (!CompareMaybeStringsPar(cx, lhs, rhs, &vsZero))                  \
            return false;                                                       \
        *res = (vsZero OP 0) == EXPECTED;                                       \
    }                                                                           \
    return true;                                                                \
} while(0)

static bool
CompareStringsPar(ForkJoinContext *cx, JSString *left, JSString *right, int32_t *res)
{
    ScopedThreadSafeStringInspector leftInspector(left);
    ScopedThreadSafeStringInspector rightInspector(right);
    if (!leftInspector.ensureChars(cx) || !rightInspector.ensureChars(cx))
        return false;

    *res = CompareChars(leftInspector.chars(), left->length(),
                        rightInspector.chars(), right->length());
    return true;
}

static bool
CompareMaybeStringsPar(ForkJoinContext *cx, HandleValue v1, HandleValue v2, int32_t *res)
{
    if (!v1.isString())
        return false;
    if (!v2.isString())
        return false;
    return CompareStringsPar(cx, v1.toString(), v2.toString(), res);
}

template<bool Equal>
bool
LooselyEqualImplPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    PAR_RELATIONAL_OP(==, Equal);
}

bool
js::jit::LooselyEqualPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return LooselyEqualImplPar<true>(cx, lhs, rhs, res);
}

bool
js::jit::LooselyUnequalPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return LooselyEqualImplPar<false>(cx, lhs, rhs, res);
}

template<bool Equal>
bool
StrictlyEqualImplPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    if (lhs.isNumber()) {
        if (rhs.isNumber()) {
            *res = (lhs.toNumber() == rhs.toNumber()) == Equal;
            return true;
        }
    } else if (lhs.isBoolean()) {
        if (rhs.isBoolean()) {
            *res = (lhs.toBoolean() == rhs.toBoolean()) == Equal;
            return true;
        }
    } else if (lhs.isNull()) {
        if (rhs.isNull()) {
            *res = Equal;
            return true;
        }
    } else if (lhs.isUndefined()) {
        if (rhs.isUndefined()) {
            *res = Equal;
            return true;
        }
    } else if (lhs.isObject()) {
        if (rhs.isObject()) {
            *res = (lhs.toObjectOrNull() == rhs.toObjectOrNull()) == Equal;
            return true;
        }
    } else if (lhs.isString()) {
        if (rhs.isString())
            return LooselyEqualImplPar<Equal>(cx, lhs, rhs, res);
    }

    *res = false;
    return true;
}

bool
js::jit::StrictlyEqualPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return StrictlyEqualImplPar<true>(cx, lhs, rhs, res);
}

bool
js::jit::StrictlyUnequalPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    return StrictlyEqualImplPar<false>(cx, lhs, rhs, res);
}

bool
js::jit::LessThanPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    PAR_RELATIONAL_OP(<, true);
}

bool
js::jit::LessThanOrEqualPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    PAR_RELATIONAL_OP(<=, true);
}

bool
js::jit::GreaterThanPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    PAR_RELATIONAL_OP(>, true);
}

bool
js::jit::GreaterThanOrEqualPar(ForkJoinContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res)
{
    PAR_RELATIONAL_OP(>=, true);
}

template<bool Equal>
bool
StringsEqualImplPar(ForkJoinContext *cx, HandleString lhs, HandleString rhs, bool *res)
{
    int32_t vsZero;
    bool ret = CompareStringsPar(cx, lhs, rhs, &vsZero);
    if (ret != true)
        return ret;
    *res = (vsZero == 0) == Equal;
    return true;
}

bool
js::jit::StringsEqualPar(ForkJoinContext *cx, HandleString v1, HandleString v2, bool *res)
{
    return StringsEqualImplPar<true>(cx, v1, v2, res);
}

bool
js::jit::StringsUnequalPar(ForkJoinContext *cx, HandleString v1, HandleString v2, bool *res)
{
    return StringsEqualImplPar<false>(cx, v1, v2, res);
}

bool
jit::BitNotPar(ForkJoinContext *cx, HandleValue in, int32_t *out)
{
    if (in.isObject())
        return false;
    int i;
    if (!NonObjectToInt32(cx, in, &i))
        return false;
    *out = ~i;
    return true;
}

#define BIT_OP(OP)                                                      \
    JS_BEGIN_MACRO                                                      \
    int32_t left, right;                                                \
    if (lhs.isObject() || rhs.isObject())                               \
        return TP_RETRY_SEQUENTIALLY;                                   \
    if (!NonObjectToInt32(cx, lhs, &left) ||                         \
        !NonObjectToInt32(cx, rhs, &right))                          \
    {                                                                   \
        return false;                                                   \
    }                                                                   \
    *out = (OP);                                                        \
    return true;                                                        \
    JS_END_MACRO

bool
jit::BitXorPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out)
{
    BIT_OP(left ^ right);
}

bool
jit::BitOrPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out)
{
    BIT_OP(left | right);
}

bool
jit::BitAndPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out)
{
    BIT_OP(left & right);
}

bool
jit::BitLshPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out)
{
    BIT_OP(left << (right & 31));
}

bool
jit::BitRshPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out)
{
    BIT_OP(left >> (right & 31));
}

#undef BIT_OP

bool
jit::UrshValuesPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs,
                   Value *out)
{
    uint32_t left;
    int32_t right;
    if (lhs.isObject() || rhs.isObject())
        return false;
    if (!NonObjectToUint32(cx, lhs, &left) || !NonObjectToInt32(cx, rhs, &right))
        return false;
    left >>= right & 31;
    out->setNumber(uint32_t(left));
    return true;
}

void
jit::AbortPar(ParallelBailoutCause cause, JSScript *outermostScript, JSScript *currentScript,
              jsbytecode *bytecode)
{
    // Spew before asserts to help with diagnosing failures.
    Spew(SpewBailouts,
         "Parallel abort with cause %d in %p:%s:%d "
         "(%p:%s:%d at line %d)",
         cause,
         outermostScript, outermostScript->filename(), outermostScript->lineno(),
         currentScript, currentScript->filename(), currentScript->lineno(),
         (currentScript ? PCToLineNumber(currentScript, bytecode) : 0));

    JS_ASSERT(InParallelSection());
    JS_ASSERT(outermostScript != nullptr);
    JS_ASSERT(currentScript != nullptr);
    JS_ASSERT(outermostScript->hasParallelIonScript());

    ForkJoinContext *cx = ForkJoinContext::current();

    JS_ASSERT(cx->bailoutRecord->depth == 0);
    cx->bailoutRecord->setCause(cause, outermostScript, currentScript, bytecode);
}

void
jit::PropagateAbortPar(JSScript *outermostScript, JSScript *currentScript)
{
    Spew(SpewBailouts,
         "Propagate parallel abort via %p:%s:%d (%p:%s:%d)",
         outermostScript, outermostScript->filename(), outermostScript->lineno(),
         currentScript, currentScript->filename(), currentScript->lineno());

    JS_ASSERT(InParallelSection());
    JS_ASSERT(outermostScript->hasParallelIonScript());

    outermostScript->parallelIonScript()->setHasUncompiledCallTarget();

    ForkJoinContext *cx = ForkJoinContext::current();
    if (currentScript)
        cx->bailoutRecord->addTrace(currentScript, nullptr);
}

void
jit::CallToUncompiledScriptPar(JSObject *obj)
{
    JS_ASSERT(InParallelSection());

#ifdef DEBUG
    static const int max_bound_function_unrolling = 5;

    if (!obj->is<JSFunction>()) {
        Spew(SpewBailouts, "Call to non-function");
        return;
    }

    JSFunction *func = &obj->as<JSFunction>();
    if (func->hasScript()) {
        JSScript *script = func->nonLazyScript();
        Spew(SpewBailouts, "Call to uncompiled script: %p:%s:%d",
             script, script->filename(), script->lineno());
    } else if (func->isInterpretedLazy()) {
        Spew(SpewBailouts, "Call to uncompiled lazy script");
    } else if (func->isBoundFunction()) {
        int depth = 0;
        JSFunction *target = &func->getBoundFunctionTarget()->as<JSFunction>();
        while (depth < max_bound_function_unrolling) {
            if (target->hasScript())
                break;
            if (target->isBoundFunction())
                target = &target->getBoundFunctionTarget()->as<JSFunction>();
            depth--;
        }
        if (target->hasScript()) {
            JSScript *script = target->nonLazyScript();
            Spew(SpewBailouts, "Call to bound function leading (depth: %d) to script: %p:%s:%d",
                 depth, script, script->filename(), script->lineno());
        } else {
            Spew(SpewBailouts, "Call to bound function (excessive depth: %d)", depth);
        }
    } else {
        JS_ASSERT(func->isNative());
        Spew(SpewBailouts, "Call to native function");
    }
#endif
}

JSObject *
jit::InitRestParameterPar(ForkJoinContext *cx, uint32_t length, Value *rest,
                          HandleObject templateObj, HandleObject res)
{
    // In parallel execution, we should always have succeeded in allocation
    // before this point. We can do the allocation here like in the sequential
    // path, but duplicating the initGCThing logic is too tedious.
    JS_ASSERT(res);
    JS_ASSERT(res->is<ArrayObject>());
    JS_ASSERT(!res->getDenseInitializedLength());
    JS_ASSERT(res->type() == templateObj->type());

    if (length > 0) {
        JSObject::EnsureDenseResult edr =
            res->ensureDenseElementsPreservePackedFlag(cx, 0, length);
        if (edr != JSObject::ED_OK)
            return nullptr;
        res->initDenseElementsUnbarriered(0, rest, length);
        res->as<ArrayObject>().setLengthInt32(length);
    }

    return res;
}
