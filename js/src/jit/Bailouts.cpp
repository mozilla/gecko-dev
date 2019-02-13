/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Bailouts.h"

#include "jscntxt.h"

#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/JitCompartment.h"
#include "jit/JitSpewer.h"
#include "jit/Snapshots.h"
#include "vm/TraceLogging.h"

#include "jit/JitFrameIterator-inl.h"
#include "vm/Probes-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::IsInRange;

uint32_t
jit::Bailout(BailoutStack* sp, BaselineBailoutInfo** bailoutInfo)
{
    JSContext* cx = GetJSContextFromJitCode();
    MOZ_ASSERT(bailoutInfo);

    // We don't have an exit frame.
    MOZ_ASSERT(IsInRange(FAKE_JIT_TOP_FOR_BAILOUT, 0, 0x1000) &&
               IsInRange(FAKE_JIT_TOP_FOR_BAILOUT + sizeof(CommonFrameLayout), 0, 0x1000),
               "Fake jitTop pointer should be within the first page.");
    cx->runtime()->jitTop = FAKE_JIT_TOP_FOR_BAILOUT;

    JitActivationIterator jitActivations(cx->runtime());
    BailoutFrameInfo bailoutData(jitActivations, sp);
    JitFrameIterator iter(jitActivations);
    MOZ_ASSERT(!iter.ionScript()->invalidated());
    CommonFrameLayout* currentFramePtr = iter.current();

    TraceLoggerThread* logger = TraceLoggerForMainThread(cx->runtime());
    TraceLogTimestamp(logger, TraceLogger_Bailout);

    JitSpew(JitSpew_IonBailouts, "Took bailout! Snapshot offset: %d", iter.snapshotOffset());

    MOZ_ASSERT(IsBaselineEnabled(cx));

    *bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, bailoutData.activation(), iter, false, bailoutInfo,
                                           /* excInfo = */ nullptr);
    MOZ_ASSERT(retval == BAILOUT_RETURN_OK ||
               retval == BAILOUT_RETURN_FATAL_ERROR ||
               retval == BAILOUT_RETURN_OVERRECURSED);
    MOZ_ASSERT_IF(retval == BAILOUT_RETURN_OK, *bailoutInfo != nullptr);

    if (retval != BAILOUT_RETURN_OK) {
        JSScript* script = iter.script();
        probes::ExitScript(cx, script, script->functionNonDelazifying(),
                           /* popSPSFrame = */ false);

        EnsureExitFrame(iter.jsFrame());
    }

    // This condition was wrong when we entered this bailout function, but it
    // might be true now. A GC might have reclaimed all the Jit code and
    // invalidated all frames which are currently on the stack. As we are
    // already in a bailout, we could not switch to an invalidation
    // bailout. When the code of an IonScript which is on the stack is
    // invalidated (see InvalidateActivation), we remove references to it and
    // increment the reference counter for each activation that appear on the
    // stack. As the bailed frame is one of them, we have to decrement it now.
    if (iter.ionScript()->invalidated())
        iter.ionScript()->decrementInvalidationCount(cx->runtime()->defaultFreeOp());

    // NB: Commentary on how |lastProfilingFrame| is set from bailouts.
    //
    // Once we return to jitcode, any following frames might get clobbered,
    // but the current frame will not (as it will be clobbered "in-place"
    // with a baseline frame that will share the same frame prefix).
    // However, there may be multiple baseline frames unpacked from this
    // single Ion frame, which means we will need to once again reset
    // |lastProfilingFrame| to point to the correct unpacked last frame
    // in |FinishBailoutToBaseline|.
    //
    // In the case of error, the jitcode will jump immediately to an
    // exception handler, which will unwind the frames and properly set
    // the |lastProfilingFrame| to point to the frame being resumed into
    // (see |AutoResetLastProfilerFrameOnReturnFromException|).
    //
    // In both cases, we want to temporarily set the |lastProfilingFrame|
    // to the current frame being bailed out, and then fix it up later.
    if (cx->runtime()->jitRuntime()->isProfilerInstrumentationEnabled(cx->runtime()))
        cx->runtime()->jitActivation->setLastProfilingFrame(currentFramePtr);

    return retval;
}

uint32_t
jit::InvalidationBailout(InvalidationBailoutStack* sp, size_t* frameSizeOut,
                         BaselineBailoutInfo** bailoutInfo)
{
    sp->checkInvariants();

    JSContext* cx = GetJSContextFromJitCode();

    // We don't have an exit frame.
    cx->runtime()->jitTop = FAKE_JIT_TOP_FOR_BAILOUT;

    JitActivationIterator jitActivations(cx->runtime());
    BailoutFrameInfo bailoutData(jitActivations, sp);
    JitFrameIterator iter(jitActivations);
    CommonFrameLayout* currentFramePtr = iter.current();

    TraceLoggerThread* logger = TraceLoggerForMainThread(cx->runtime());
    TraceLogTimestamp(logger, TraceLogger_Invalidation);

    JitSpew(JitSpew_IonBailouts, "Took invalidation bailout! Snapshot offset: %d", iter.snapshotOffset());

    // Note: the frame size must be computed before we return from this function.
    *frameSizeOut = iter.frameSize();

    MOZ_ASSERT(IsBaselineEnabled(cx));

    *bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, bailoutData.activation(), iter, true, bailoutInfo,
                                           /* excInfo = */ nullptr);
    MOZ_ASSERT(retval == BAILOUT_RETURN_OK ||
               retval == BAILOUT_RETURN_FATAL_ERROR ||
               retval == BAILOUT_RETURN_OVERRECURSED);
    MOZ_ASSERT_IF(retval == BAILOUT_RETURN_OK, *bailoutInfo != nullptr);

    if (retval != BAILOUT_RETURN_OK) {
        // If the bailout failed, then bailout trampoline will pop the
        // current frame and jump straight to exception handling code when
        // this function returns.  Any SPS entry pushed for this frame will
        // be silently forgotten.
        //
        // We call ExitScript here to ensure that if the ionScript had SPS
        // instrumentation, then the SPS entry for it is popped.
        //
        // However, if the bailout was during argument check, then a
        // pseudostack frame would not have been pushed in the first
        // place, so don't pop anything in that case.
        JSScript* script = iter.script();
        probes::ExitScript(cx, script, script->functionNonDelazifying(),
                           /* popSPSFrame = */ false);

        JitFrameLayout* frame = iter.jsFrame();
        JitSpew(JitSpew_IonInvalidate, "Bailout failed (%s): converting to exit frame",
                (retval == BAILOUT_RETURN_FATAL_ERROR) ? "Fatal Error" : "Over Recursion");
        JitSpew(JitSpew_IonInvalidate, "   orig calleeToken %p", (void*) frame->calleeToken());
        JitSpew(JitSpew_IonInvalidate, "   orig frameSize %u", unsigned(frame->prevFrameLocalSize()));
        JitSpew(JitSpew_IonInvalidate, "   orig ra %p", (void*) frame->returnAddress());

        frame->replaceCalleeToken(nullptr);
        EnsureExitFrame(frame);

        JitSpew(JitSpew_IonInvalidate, "   new  calleeToken %p", (void*) frame->calleeToken());
        JitSpew(JitSpew_IonInvalidate, "   new  frameSize %u", unsigned(frame->prevFrameLocalSize()));
        JitSpew(JitSpew_IonInvalidate, "   new  ra %p", (void*) frame->returnAddress());
    }

    iter.ionScript()->decrementInvalidationCount(cx->runtime()->defaultFreeOp());

    // Make the frame being bailed out the top profiled frame.
    if (cx->runtime()->jitRuntime()->isProfilerInstrumentationEnabled(cx->runtime()))
        cx->runtime()->jitActivation->setLastProfilingFrame(currentFramePtr);

    return retval;
}

BailoutFrameInfo::BailoutFrameInfo(const JitActivationIterator& activations,
                                   const JitFrameIterator& frame)
  : machine_(frame.machineState())
{
    framePointer_ = (uint8_t*) frame.fp();
    topFrameSize_ = frame.frameSize();
    topIonScript_ = frame.ionScript();
    attachOnJitActivation(activations);

    const OsiIndex* osiIndex = frame.osiIndex();
    snapshotOffset_ = osiIndex->snapshotOffset();
}

uint32_t
jit::ExceptionHandlerBailout(JSContext* cx, const InlineFrameIterator& frame,
                             ResumeFromException* rfe,
                             const ExceptionBailoutInfo& excInfo,
                             bool* overrecursed)
{
    // We can be propagating debug mode exceptions without there being an
    // actual exception pending. For instance, when we return false from an
    // operation callback like a timeout handler.
    MOZ_ASSERT_IF(!excInfo.propagatingIonExceptionForDebugMode(), cx->isExceptionPending());

    cx->runtime()->jitTop = FAKE_JIT_TOP_FOR_BAILOUT;
    gc::AutoSuppressGC suppress(cx);

    JitActivationIterator jitActivations(cx->runtime());
    BailoutFrameInfo bailoutData(jitActivations, frame.frame());
    JitFrameIterator iter(jitActivations);
    CommonFrameLayout* currentFramePtr = iter.current();

    BaselineBailoutInfo* bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, bailoutData.activation(), iter, true,
                                           &bailoutInfo, &excInfo);

    if (retval == BAILOUT_RETURN_OK) {
        MOZ_ASSERT(bailoutInfo);

        // Overwrite the kind so HandleException after the bailout returns
        // false, jumping directly to the exception tail.
        if (excInfo.propagatingIonExceptionForDebugMode())
            bailoutInfo->bailoutKind = Bailout_IonExceptionDebugMode;

        rfe->kind = ResumeFromException::RESUME_BAILOUT;
        rfe->target = cx->runtime()->jitRuntime()->getBailoutTail()->raw();
        rfe->bailoutInfo = bailoutInfo;
    } else {
        // Bailout failed. If there was a fatal error, clear the
        // exception to turn this into an uncatchable error. If the
        // overrecursion check failed, continue popping all inline
        // frames and have the caller report an overrecursion error.
        MOZ_ASSERT(!bailoutInfo);

        if (!excInfo.propagatingIonExceptionForDebugMode())
            cx->clearPendingException();

        if (retval == BAILOUT_RETURN_OVERRECURSED)
            *overrecursed = true;
        else
            MOZ_ASSERT(retval == BAILOUT_RETURN_FATAL_ERROR);
    }

    // Make the frame being bailed out the top profiled frame.
    if (cx->runtime()->jitRuntime()->isProfilerInstrumentationEnabled(cx->runtime()))
        cx->runtime()->jitActivation->setLastProfilingFrame(currentFramePtr);

    return retval;
}

// Initialize the decl env Object, call object, and any arguments obj of the current frame.
bool
jit::EnsureHasScopeObjects(JSContext* cx, AbstractFramePtr fp)
{
    if (fp.isFunctionFrame() &&
        fp.fun()->isHeavyweight() &&
        !fp.hasCallObj())
    {
        return fp.initFunctionScopeObjects(cx);
    }
    return true;
}

bool
jit::CheckFrequentBailouts(JSContext* cx, JSScript* script)
{
    if (script->hasIonScript()) {
        // Invalidate if this script keeps bailing out without invalidation. Next time
        // we compile this script LICM will be disabled.
        IonScript* ionScript = script->ionScript();

        if (ionScript->numBailouts() >= js_JitOptions.frequentBailoutThreshold &&
            !script->hadFrequentBailouts())
        {
            script->setHadFrequentBailouts();

            JitSpew(JitSpew_IonInvalidate, "Invalidating due to too many bailouts");

            if (!Invalidate(cx, script))
                return false;
        }
    }

    return true;
}

void
BailoutFrameInfo::attachOnJitActivation(const JitActivationIterator& jitActivations)
{
    MOZ_ASSERT(jitActivations.jitTop() == FAKE_JIT_TOP_FOR_BAILOUT);
    activation_ = jitActivations->asJit();
    activation_->setBailoutData(this);
}

BailoutFrameInfo::~BailoutFrameInfo()
{
    activation_->cleanBailoutData();
}
