/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MIRGenerator_h
#define jit_MIRGenerator_h

// This file declares the data structures used to build a control-flow graph
// containing MIR.

#include "mozilla/Atomics.h"

#include <stdarg.h>

#include "jscntxt.h"
#include "jscompartment.h"

#include "jit/CompileInfo.h"
#include "jit/IonAllocPolicy.h"
#include "jit/JitCompartment.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/RegisterSets.h"

namespace js {
namespace jit {

class MBasicBlock;
class MIRGraph;
class MStart;
class OptimizationInfo;

class MIRGenerator
{
  public:
    MIRGenerator(CompileCompartment *compartment, const JitCompileOptions &options,
                 TempAllocator *alloc, MIRGraph *graph,
                 CompileInfo *info, const OptimizationInfo *optimizationInfo);

    TempAllocator &alloc() {
        return *alloc_;
    }
    MIRGraph &graph() {
        return *graph_;
    }
    bool ensureBallast() {
        return alloc().ensureBallast();
    }
    const JitRuntime *jitRuntime() const {
        return GetIonContext()->runtime->jitRuntime();
    }
    CompileInfo &info() {
        return *info_;
    }
    const OptimizationInfo &optimizationInfo() const {
        return *optimizationInfo_;
    }

    template <typename T>
    T * allocate(size_t count = 1) {
        if (count & mozilla::tl::MulOverflowMask<sizeof(T)>::value)
            return nullptr;
        return reinterpret_cast<T *>(alloc().allocate(sizeof(T) * count));
    }

    // Set an error state and prints a message. Returns false so errors can be
    // propagated up.
    bool abort(const char *message, ...);
    bool abortFmt(const char *message, va_list ap);

    bool errored() const {
        return error_;
    }

    bool instrumentedProfiling() {
        return GetIonContext()->runtime->spsProfiler().enabled();
    }

    // Whether the main thread is trying to cancel this build.
    bool shouldCancel(const char *why) {
        return cancelBuild_;
    }
    void cancel() {
        cancelBuild_ = true;
    }

    void disable() {
        abortReason_ = AbortReason_Disable;
    }
    AbortReason abortReason() {
        return abortReason_;
    }

    bool compilingAsmJS() const {
        return info_->compilingAsmJS();
    }

    uint32_t maxAsmJSStackArgBytes() const {
        JS_ASSERT(compilingAsmJS());
        return maxAsmJSStackArgBytes_;
    }
    uint32_t resetAsmJSMaxStackArgBytes() {
        JS_ASSERT(compilingAsmJS());
        uint32_t old = maxAsmJSStackArgBytes_;
        maxAsmJSStackArgBytes_ = 0;
        return old;
    }
    void setAsmJSMaxStackArgBytes(uint32_t n) {
        JS_ASSERT(compilingAsmJS());
        maxAsmJSStackArgBytes_ = n;
    }
    void setPerformsCall() {
        performsCall_ = true;
    }
    bool performsCall() const {
        return performsCall_;
    }
    void setNeedsInitialStackAlignment() {
        needsInitialStackAlignment_ = true;
    }
    bool needsInitialStackAlignment() const {
        JS_ASSERT(compilingAsmJS());
        return needsInitialStackAlignment_;
    }
    void setPerformsAsmJSCall() {
        JS_ASSERT(compilingAsmJS());
        setPerformsCall();
        setNeedsInitialStackAlignment();
    }
    void noteMinAsmJSHeapLength(uint32_t len) {
        minAsmJSHeapLength_ = len;
    }
    uint32_t minAsmJSHeapLength() const {
        return minAsmJSHeapLength_;
    }

    bool modifiesFrameArguments() const {
        return modifiesFrameArguments_;
    }

  public:
    CompileCompartment *compartment;

  protected:
    CompileInfo *info_;
    const OptimizationInfo *optimizationInfo_;
    TempAllocator *alloc_;
    JSFunction *fun_;
    uint32_t nslots_;
    MIRGraph *graph_;
    AbortReason abortReason_;
    bool error_;
    mozilla::Atomic<bool, mozilla::Relaxed> cancelBuild_;

    uint32_t maxAsmJSStackArgBytes_;
    bool performsCall_;
    bool needsInitialStackAlignment_;
    uint32_t minAsmJSHeapLength_;

    // Keep track of whether frame arguments are modified during execution.
    // RegAlloc needs to know this as spilling values back to their register
    // slots is not compatible with that.
    bool modifiesFrameArguments_;

#if defined(JS_ION_PERF)
    AsmJSPerfSpewer asmJSPerfSpewer_;

  public:
    AsmJSPerfSpewer &perfSpewer() { return asmJSPerfSpewer_; }
#endif

  public:
    const JitCompileOptions options;
};

} // namespace jit
} // namespace js

#endif /* jit_MIRGenerator_h */
