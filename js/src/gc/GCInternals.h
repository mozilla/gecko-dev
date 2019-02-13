/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCInternals_h
#define gc_GCInternals_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"

#include "jscntxt.h"

#include "gc/Zone.h"
#include "vm/HelperThreads.h"
#include "vm/Runtime.h"

namespace js {
namespace gc {

void
MarkPersistentRootedChains(JSTracer* trc);

class AutoCopyFreeListToArenas
{
    JSRuntime* runtime;
    ZoneSelector selector;

  public:
    AutoCopyFreeListToArenas(JSRuntime* rt, ZoneSelector selector);
    ~AutoCopyFreeListToArenas();
};

struct AutoFinishGC
{
    explicit AutoFinishGC(JSRuntime* rt);
};

/*
 * This class should be used by any code that needs to exclusive access to the
 * heap in order to trace through it...
 */
class AutoTraceSession
{
  public:
    explicit AutoTraceSession(JSRuntime* rt, JS::HeapState state = JS::HeapState::Tracing);
    ~AutoTraceSession();

  protected:
    AutoLockForExclusiveAccess lock;
    JSRuntime* runtime;

  private:
    AutoTraceSession(const AutoTraceSession&) = delete;
    void operator=(const AutoTraceSession&) = delete;

    JS::HeapState prevState;
};

struct AutoPrepareForTracing
{
    AutoFinishGC finish;
    AutoTraceSession session;
    AutoCopyFreeListToArenas copy;

    AutoPrepareForTracing(JSRuntime* rt, ZoneSelector selector);
};

class IncrementalSafety
{
    const char* reason_;

    explicit IncrementalSafety(const char* reason) : reason_(reason) {}

  public:
    static IncrementalSafety Safe() { return IncrementalSafety(nullptr); }
    static IncrementalSafety Unsafe(const char* reason) { return IncrementalSafety(reason); }

    explicit operator bool() const {
        return reason_ == nullptr;
    }

    const char* reason() {
        MOZ_ASSERT(reason_);
        return reason_;
    }
};

IncrementalSafety
IsIncrementalGCSafe(JSRuntime* rt);

#ifdef JS_GC_ZEAL

class AutoStopVerifyingBarriers
{
    GCRuntime* gc;
    bool restartPreVerifier;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoStopVerifyingBarriers(JSRuntime* rt, bool isShutdown
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : gc(&rt->gc)
    {
        restartPreVerifier = gc->endVerifyPreBarriers() && !isShutdown;
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoStopVerifyingBarriers() {
        // Nasty special case: verification runs a minor GC, which *may* nest
        // inside of an outer minor GC. This is not allowed by the
        // gc::Statistics phase tree. So we pause the "real" GC, if in fact one
        // is in progress.
        gcstats::Phase outer = gc->stats.currentPhase();
        if (outer != gcstats::PHASE_NONE)
            gc->stats.endPhase(outer);
        MOZ_ASSERT((gc->stats.currentPhase() == gcstats::PHASE_NONE) ||
                   (gc->stats.currentPhase() == gcstats::PHASE_GC_BEGIN) ||
                   (gc->stats.currentPhase() == gcstats::PHASE_GC_END));

        if (restartPreVerifier)
            gc->startVerifyPreBarriers();

        if (outer != gcstats::PHASE_NONE)
            gc->stats.beginPhase(outer);
    }
};
#else
struct AutoStopVerifyingBarriers
{
    AutoStopVerifyingBarriers(JSRuntime*, bool) {}
};
#endif /* JS_GC_ZEAL */

#ifdef JSGC_HASH_TABLE_CHECKS
void
CheckHashTablesAfterMovingGC(JSRuntime* rt);
#endif

struct MovingTracer : JS::CallbackTracer
{
    explicit MovingTracer(JSRuntime* rt) : CallbackTracer(rt, TraceWeakMapKeysValues) {}

    void onObjectEdge(JSObject** objp) override;
    void onChild(const JS::GCCellPtr& thing) override {
        MOZ_ASSERT(!RelocationOverlay::isCellForwarded(thing.asCell()));
    }

#ifdef DEBUG
    TracerKind getTracerKind() const override { return TracerKind::Moving; }
#endif
};

class AutoMaybeStartBackgroundAllocation
{
  private:
    JSRuntime* runtime;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoMaybeStartBackgroundAllocation(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
      : runtime(nullptr)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    void tryToStartBackgroundAllocation(JSRuntime* rt) {
        runtime = rt;
    }

    ~AutoMaybeStartBackgroundAllocation() {
        if (runtime)
            runtime->gc.startBackgroundAllocTaskIfIdle();
    }
};

// In debug builds, set/unset the GC sweeping flag for the current thread.
struct AutoSetThreadIsSweeping
{
#ifdef DEBUG
    explicit AutoSetThreadIsSweeping(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
      : threadData_(js::TlsPerThreadData.get())
    {
        MOZ_ASSERT(!threadData_->gcSweeping);
        threadData_->gcSweeping = true;
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoSetThreadIsSweeping() {
        MOZ_ASSERT(threadData_->gcSweeping);
        threadData_->gcSweeping = false;
    }

  private:
    PerThreadData* threadData_;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
#else
    AutoSetThreadIsSweeping() {}
#endif
};

// Structure for counting how many times objects in a particular group have
// been tenured during a minor collection.
struct TenureCount
{
    ObjectGroup* group;
    int count;
};

// Keep rough track of how many times we tenure objects in particular groups
// during minor collections, using a fixed size hash for efficiency at the cost
// of potential collisions.
struct TenureCountCache
{
    TenureCount entries[16];

    TenureCountCache() { mozilla::PodZero(this); }

    TenureCount& findEntry(ObjectGroup* group) {
        return entries[PointerHasher<ObjectGroup*, 3>::hash(group) % mozilla::ArrayLength(entries)];
    }
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_GCInternals_h */
