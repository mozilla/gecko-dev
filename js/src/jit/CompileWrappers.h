/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CompileWrappers_h
#define jit_CompileWrappers_h

#include "jscntxt.h"

namespace js {
namespace jit {

class JitRuntime;

// During Ion compilation we need access to various bits of the current
// compartment, runtime and so forth. However, since compilation can run off
// thread while the main thread is actively mutating the VM, this access needs
// to be restricted. The classes below give the compiler an interface to access
// all necessary information in a threadsafe fashion.

class CompileRuntime
{
    JSRuntime* runtime();

  public:
    static CompileRuntime* get(JSRuntime* rt);

    bool onMainThread();

    js::PerThreadData* mainThread();

    // &runtime()->jitTop
    const void* addressOfJitTop();

    // &runtime()->jitActivation
    const void* addressOfJitActivation();

    // &runtime()->profilingActivation
    const void* addressOfProfilingActivation();

    // rt->runtime()->jitStackLimit;
    const void* addressOfJitStackLimit();

#ifdef DEBUG
    // rt->runtime()->addressOfIonBailAfter;
    const void* addressOfIonBailAfter();
#endif

    // &runtime()->activation_
    const void* addressOfActivation();

#ifdef JS_GC_ZEAL
    const void* addressOfGCZealModeBits();
#endif

    const void* addressOfInterruptUint32();

    // We have to bake JSContext* into JIT code, but this pointer shouldn't be
    // used/dereferenced on the background thread so we return it as void*.
    const void* getJSContext();

    const JitRuntime* jitRuntime();

    // Compilation does not occur off thread when the SPS profiler is enabled.
    SPSProfiler& spsProfiler();

    bool jitSupportsFloatingPoint();
    bool hadOutOfMemory();
    bool profilingScripts();

    const JSAtomState& names();
    const PropertyName* emptyString();
    const StaticStrings& staticStrings();
    const Value& NaNValue();
    const Value& positiveInfinityValue();
    const WellKnownSymbols& wellKnownSymbols();

#ifdef DEBUG
    bool isInsideNursery(gc::Cell* cell);
#endif

    // DOM callbacks must be threadsafe (and will hopefully be removed soon).
    const DOMCallbacks* DOMcallbacks();

    const Nursery& gcNursery();
    void setMinorGCShouldCancelIonCompilations();

    bool runtimeMatches(JSRuntime* rt);
};

class CompileZone
{
    Zone* zone();

  public:
    static CompileZone* get(Zone* zone);

    const void* addressOfNeedsIncrementalBarrier();

    const void* addressOfFreeList(gc::AllocKind allocKind);
};

class JitCompartment;

class CompileCompartment
{
    JSCompartment* compartment();

  public:
    static CompileCompartment* get(JSCompartment* comp);

    CompileZone* zone();
    CompileRuntime* runtime();

    const void* addressOfEnumerators();
    const void* addressOfRandomNumberGenerator();
    const void* addressOfLastCachedNativeIterator();

    const JitCompartment* jitCompartment();

    const GlobalObject* maybeGlobal();

    bool hasAllocationMetadataBuilder();

    // Mirror CompartmentOptions.
    void setSingletonsAsValues();
};

class JitCompileOptions
{
  public:
    JitCompileOptions();
    explicit JitCompileOptions(JSContext* cx);

    bool cloneSingletons() const {
        return cloneSingletons_;
    }

    bool spsSlowAssertionsEnabled() const {
        return spsSlowAssertionsEnabled_;
    }

    bool offThreadCompilationAvailable() const {
        return offThreadCompilationAvailable_;
    }

  private:
    bool cloneSingletons_;
    bool spsSlowAssertionsEnabled_;
    bool offThreadCompilationAvailable_;
};

} // namespace jit
} // namespace js

#endif // jit_CompileWrappers_h
