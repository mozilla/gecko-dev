/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Ion.h"

#include "jscompartmentinlines.h"

using namespace js;
using namespace js::jit;

JSRuntime*
CompileRuntime::runtime()
{
    return reinterpret_cast<JSRuntime*>(this);
}

/* static */ CompileRuntime*
CompileRuntime::get(JSRuntime* rt)
{
    return reinterpret_cast<CompileRuntime*>(rt);
}

bool
CompileRuntime::onMainThread()
{
    return js::CurrentThreadCanAccessRuntime(runtime());
}

js::PerThreadData*
CompileRuntime::mainThread()
{
    MOZ_ASSERT(onMainThread());
    return &runtime()->mainThread;
}

const void*
CompileRuntime::addressOfJitTop()
{
    return &runtime()->jitTop;
}

const void*
CompileRuntime::addressOfJitActivation()
{
    return &runtime()->jitActivation;
}

const void*
CompileRuntime::addressOfProfilingActivation()
{
    return (const void*) &runtime()->profilingActivation_;
}

const void*
CompileRuntime::addressOfJitStackLimit()
{
    return runtime()->addressOfJitStackLimit();
}

#ifdef DEBUG
const void*
CompileRuntime::addressOfIonBailAfter()
{
    return runtime()->addressOfIonBailAfter();
}
#endif

const void*
CompileRuntime::addressOfActivation()
{
    return runtime()->addressOfActivation();
}

#ifdef JS_GC_ZEAL
const void*
CompileRuntime::addressOfGCZealModeBits()
{
    return runtime()->gc.addressOfZealModeBits();
}
#endif

const void*
CompileRuntime::addressOfInterruptUint32()
{
    return runtime()->addressOfInterruptUint32();
}

const void*
CompileRuntime::getJSContext()
{
    return runtime()->unsafeContextFromAnyThread();
}

const JitRuntime*
CompileRuntime::jitRuntime()
{
    return runtime()->jitRuntime();
}

SPSProfiler&
CompileRuntime::spsProfiler()
{
    return runtime()->spsProfiler;
}

bool
CompileRuntime::jitSupportsFloatingPoint()
{
    return runtime()->jitSupportsFloatingPoint;
}

bool
CompileRuntime::hadOutOfMemory()
{
    return runtime()->hadOutOfMemory;
}

bool
CompileRuntime::profilingScripts()
{
    return runtime()->profilingScripts;
}

const JSAtomState&
CompileRuntime::names()
{
    return *runtime()->commonNames;
}

const PropertyName*
CompileRuntime::emptyString()
{
    return runtime()->emptyString;
}

const StaticStrings&
CompileRuntime::staticStrings()
{
    return *runtime()->staticStrings;
}

const Value&
CompileRuntime::NaNValue()
{
    return runtime()->NaNValue;
}

const Value&
CompileRuntime::positiveInfinityValue()
{
    return runtime()->positiveInfinityValue;
}

const WellKnownSymbols&
CompileRuntime::wellKnownSymbols()
{
    MOZ_ASSERT(onMainThread());
    return *runtime()->wellKnownSymbols;
}

#ifdef DEBUG
bool
CompileRuntime::isInsideNursery(gc::Cell* cell)
{
    return UninlinedIsInsideNursery(cell);
}
#endif

const DOMCallbacks*
CompileRuntime::DOMcallbacks()
{
    return runtime()->DOMcallbacks;
}

const Nursery&
CompileRuntime::gcNursery()
{
    return runtime()->gc.nursery;
}

void
CompileRuntime::setMinorGCShouldCancelIonCompilations()
{
    MOZ_ASSERT(onMainThread());
    runtime()->gc.storeBuffer.setShouldCancelIonCompilations();
}

bool
CompileRuntime::runtimeMatches(JSRuntime* rt)
{
    return rt == runtime();
}

Zone*
CompileZone::zone()
{
    return reinterpret_cast<Zone*>(this);
}

/* static */ CompileZone*
CompileZone::get(Zone* zone)
{
    return reinterpret_cast<CompileZone*>(zone);
}

const void*
CompileZone::addressOfNeedsIncrementalBarrier()
{
    return zone()->addressOfNeedsIncrementalBarrier();
}

const void*
CompileZone::addressOfFreeList(gc::AllocKind allocKind)
{
    return zone()->arenas.addressOfFreeList(allocKind);
}

JSCompartment*
CompileCompartment::compartment()
{
    return reinterpret_cast<JSCompartment*>(this);
}

/* static */ CompileCompartment*
CompileCompartment::get(JSCompartment* comp)
{
    return reinterpret_cast<CompileCompartment*>(comp);
}

CompileZone*
CompileCompartment::zone()
{
    return CompileZone::get(compartment()->zone());
}

CompileRuntime*
CompileCompartment::runtime()
{
    return CompileRuntime::get(compartment()->runtimeFromAnyThread());
}

const void*
CompileCompartment::addressOfEnumerators()
{
    return &compartment()->enumerators;
}

const void*
CompileCompartment::addressOfLastCachedNativeIterator()
{
    return &compartment()->lastCachedNativeIterator;
}

const void*
CompileCompartment::addressOfRandomNumberGenerator()
{
    return compartment()->randomNumberGenerator.ptr();
}

const JitCompartment*
CompileCompartment::jitCompartment()
{
    return compartment()->jitCompartment();
}

const GlobalObject*
CompileCompartment::maybeGlobal()
{
    // This uses unsafeUnbarrieredMaybeGlobal() so as not to trigger the read
    // barrier on the global from off the main thread.  This is safe because we
    // abort Ion compilation when we GC.
    return compartment()->unsafeUnbarrieredMaybeGlobal();
}

bool
CompileCompartment::hasAllocationMetadataBuilder()
{
    return compartment()->hasAllocationMetadataBuilder();
}

// Note: This function is thread-safe because setSingletonAsValue sets a boolean
// variable to false, and this boolean variable has no way to be resetted to
// true. So even if there is a concurrent write, this concurrent write will
// always have the same value.  If there is a concurrent read, then we will
// clone a singleton instead of using the value which is baked in the JSScript,
// and this would be an unfortunate allocation, but this will not change the
// semantics of the JavaScript code which is executed.
void
CompileCompartment::setSingletonsAsValues()
{
    compartment()->behaviors().setSingletonsAsValues();
}

JitCompileOptions::JitCompileOptions()
  : cloneSingletons_(false),
    spsSlowAssertionsEnabled_(false),
    offThreadCompilationAvailable_(false)
{
}

JitCompileOptions::JitCompileOptions(JSContext* cx)
{
    cloneSingletons_ = cx->compartment()->creationOptions().cloneSingletons();
    spsSlowAssertionsEnabled_ = cx->runtime()->spsProfiler.enabled() &&
                                cx->runtime()->spsProfiler.slowAssertionsEnabled();
    offThreadCompilationAvailable_ = OffThreadCompilationAvailable(cx);
}
