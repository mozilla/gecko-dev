/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#endif

#include "jscntxt.h"
#include "jsgc.h"
#include "jsprf.h"
#include "jstypes.h"
#include "jswatchpoint.h"

#include "builtin/MapObject.h"
#include "frontend/BytecodeCompiler.h"
#include "gc/GCInternals.h"
#include "gc/Marking.h"
#include "jit/MacroAssembler.h"
#include "js/HashTable.h"
#include "vm/Debugger.h"
#include "vm/JSONParser.h"
#include "vm/WeakMapObject.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayEnd;

using JS::AutoGCRooter;

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

// Note: the following two functions cannot be static as long as we are using
// GCC 4.4, since it requires template function parameters to have external
// linkage.

void
MarkBindingsRoot(JSTracer* trc, Bindings* bindings, const char* name)
{
    bindings->trace(trc);
}

void
MarkPropertyDescriptorRoot(JSTracer* trc, JSPropertyDescriptor* pd, const char* name)
{
    pd->trace(trc);
}

template <class T>
static inline bool
IgnoreExactRoot(T* thingp)
{
    return false;
}

template <class T>
inline bool
IgnoreExactRoot(T** thingp)
{
    return IsNullTaggedPointer(*thingp);
}

template <>
inline bool
IgnoreExactRoot(JSObject** thingp)
{
    return IsNullTaggedPointer(*thingp) || *thingp == TaggedProto::LazyProto;
}

template <class T, void MarkFunc(JSTracer* trc, T* ref, const char* name), class Source>
static inline void
MarkExactStackRootList(JSTracer* trc, Source* s, const char* name)
{
    Rooted<T>* rooter = s->template gcRooters<T>();
    while (rooter) {
        T* addr = rooter->address();
        if (!IgnoreExactRoot(addr))
            MarkFunc(trc, addr, name);
        rooter = rooter->previous();
    }
}

template<class T>
static void
MarkExactStackRootsAcrossTypes(T context, JSTracer* trc)
{
    MarkExactStackRootList<JSObject*, TraceRoot>(trc, context, "exact-object");
    MarkExactStackRootList<Shape*, TraceRoot>(trc, context, "exact-shape");
    MarkExactStackRootList<BaseShape*, TraceRoot>(trc, context, "exact-baseshape");
    MarkExactStackRootList<ObjectGroup*, TraceRoot>(
        trc, context, "exact-objectgroup");
    MarkExactStackRootList<JSString*, TraceRoot>(trc, context, "exact-string");
    MarkExactStackRootList<JS::Symbol*, TraceRoot>(trc, context, "exact-symbol");
    MarkExactStackRootList<jit::JitCode*, TraceRoot>(trc, context, "exact-jitcode");
    MarkExactStackRootList<JSScript*, TraceRoot>(trc, context, "exact-script");
    MarkExactStackRootList<LazyScript*, TraceRoot>(trc, context, "exact-lazy-script");
    MarkExactStackRootList<jsid, TraceRoot>(trc, context, "exact-id");
    MarkExactStackRootList<Value, TraceRoot>(trc, context, "exact-value");
    MarkExactStackRootList<TypeSet::Type, TypeSet::MarkTypeRoot>(trc, context, "TypeSet::Type");
    MarkExactStackRootList<Bindings, MarkBindingsRoot>(trc, context, "Bindings");
    MarkExactStackRootList<JSPropertyDescriptor, MarkPropertyDescriptorRoot>(
        trc, context, "JSPropertyDescriptor");
}

static void
MarkExactStackRoots(JSRuntime* rt, JSTracer* trc)
{
    for (ContextIter cx(rt); !cx.done(); cx.next())
        MarkExactStackRootsAcrossTypes<JSContext*>(cx.get(), trc);
    MarkExactStackRootsAcrossTypes<PerThreadData*>(&rt->mainThread, trc);
}

void
JS::AutoIdArray::trace(JSTracer* trc)
{
    MOZ_ASSERT(tag_ == IDARRAY);
    TraceRange(trc, idArray->length, idArray->begin(), "JSAutoIdArray.idArray");
}

inline void
AutoGCRooter::trace(JSTracer* trc)
{
    switch (tag_) {
      case PARSER:
        frontend::MarkParser(trc, this);
        return;

      case IDARRAY: {
        JSIdArray* ida = static_cast<AutoIdArray*>(this)->idArray;
        TraceRange(trc, ida->length, ida->begin(), "JS::AutoIdArray.idArray");
        return;
      }

      case DESCVECTOR: {
        AutoPropertyDescriptorVector::VectorImpl& descriptors =
            static_cast<AutoPropertyDescriptorVector*>(this)->vector;
        for (size_t i = 0, len = descriptors.length(); i < len; i++)
            descriptors[i].trace(trc);
        return;
      }

      case VALVECTOR: {
        AutoValueVector::VectorImpl& vector = static_cast<AutoValueVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "JS::AutoValueVector.vector");
        return;
      }

      case IDVECTOR: {
        AutoIdVector::VectorImpl& vector = static_cast<AutoIdVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "JS::AutoIdVector.vector");
        return;
      }

      case IDVALVECTOR: {
        AutoIdValueVector::VectorImpl& vector = static_cast<AutoIdValueVector*>(this)->vector;
        for (size_t i = 0; i < vector.length(); i++) {
            TraceRoot(trc, &vector[i].id, "js::AutoIdValueVector id");
            TraceRoot(trc, &vector[i].value, "js::AutoIdValueVector value");
        }
        return;
      }

      case SHAPEVECTOR: {
        AutoShapeVector::VectorImpl& vector = static_cast<js::AutoShapeVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), const_cast<Shape**>(vector.begin()),
                       "js::AutoShapeVector.vector");
        return;
      }

      case OBJVECTOR: {
        AutoObjectVector::VectorImpl& vector = static_cast<AutoObjectVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "JS::AutoObjectVector.vector");
        return;
      }

      case STRINGVECTOR: {
        AutoStringVector::VectorImpl& vector = static_cast<AutoStringVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "js::AutoStringVector.vector");
        return;
      }

      case NAMEVECTOR: {
        AutoNameVector::VectorImpl& vector = static_cast<AutoNameVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "js::AutoNameVector.vector");
        return;
      }

      case VALARRAY: {
        /*
         * We don't know the template size parameter, but we can safely treat it
         * as an AutoValueArray<1> because the length is stored separately.
         */
        AutoValueArray<1>* array = static_cast<AutoValueArray<1>*>(this);
        TraceRootRange(trc, array->length(), array->begin(), "js::AutoValueArray");
        return;
      }

      case SCRIPTVECTOR: {
        AutoScriptVector::VectorImpl& vector = static_cast<AutoScriptVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "js::AutoScriptVector.vector");
        return;
      }

      case OBJU32HASHMAP: {
        AutoObjectUnsigned32HashMap* self = static_cast<AutoObjectUnsigned32HashMap*>(this);
        AutoObjectUnsigned32HashMap::HashMapImpl& map = self->map;
        for (AutoObjectUnsigned32HashMap::Enum e(map); !e.empty(); e.popFront()) {
            JSObject* key = e.front().key();
            TraceRoot(trc, &key, "AutoObjectUnsignedHashMap key");
            if (key != e.front().key())
                e.rekeyFront(key);
        }
        return;
      }

      case HASHABLEVALUE: {
        AutoHashableValueRooter* rooter = static_cast<AutoHashableValueRooter*>(this);
        rooter->trace(trc);
        return;
      }

      case IONMASM: {
        static_cast<js::jit::MacroAssembler::AutoRooter*>(this)->masm()->trace(trc);
        return;
      }

      case WRAPPER: {
        /*
         * We need to use TraceManuallyBarrieredEdge here because we mark
         * wrapper roots in every slice. This is because of some rule-breaking
         * in RemapAllWrappersForObject; see comment there.
         */
        TraceManuallyBarrieredEdge(trc, &static_cast<AutoWrapperRooter*>(this)->value.get(),
                                   "JS::AutoWrapperRooter.value");
        return;
      }

      case WRAPVECTOR: {
        AutoWrapperVector::VectorImpl& vector = static_cast<AutoWrapperVector*>(this)->vector;
        /*
         * We need to use TraceManuallyBarrieredEdge here because we mark
         * wrapper roots in every slice. This is because of some rule-breaking
         * in RemapAllWrappersForObject; see comment there.
         */
        for (WrapperValue* p = vector.begin(); p < vector.end(); p++)
            TraceManuallyBarrieredEdge(trc, &p->get(), "js::AutoWrapperVector.vector");
        return;
      }

      case JSONPARSER:
        static_cast<js::JSONParserBase*>(this)->trace(trc);
        return;

      case CUSTOM:
        static_cast<JS::CustomAutoRooter*>(this)->trace(trc);
        return;
    }

    MOZ_ASSERT(tag_ >= 0);
    if (Value* vp = static_cast<AutoArrayRooter*>(this)->array)
        TraceRootRange(trc, tag_, vp, "JS::AutoArrayRooter.array");
}

/* static */ void
AutoGCRooter::traceAll(JSTracer* trc)
{
    for (ContextIter cx(trc->runtime()); !cx.done(); cx.next())
        traceAllInContext(&*cx, trc);
}

/* static */ void
AutoGCRooter::traceAllWrappers(JSTracer* trc)
{
    for (ContextIter cx(trc->runtime()); !cx.done(); cx.next()) {
        for (AutoGCRooter* gcr = cx->autoGCRooters; gcr; gcr = gcr->down) {
            if (gcr->tag_ == WRAPVECTOR || gcr->tag_ == WRAPPER)
                gcr->trace(trc);
        }
    }
}

void
AutoHashableValueRooter::trace(JSTracer* trc)
{
    TraceRoot(trc, reinterpret_cast<Value*>(&value), "AutoHashableValueRooter");
}

void
StackShape::trace(JSTracer* trc)
{
    if (base)
        TraceRoot(trc, &base, "StackShape base");

    TraceRoot(trc, (jsid*) &propid, "StackShape id");

    if ((attrs & JSPROP_GETTER) && rawGetter)
        TraceRoot(trc, (JSObject**)&rawGetter, "StackShape getter");

    if ((attrs & JSPROP_SETTER) && rawSetter)
        TraceRoot(trc, (JSObject**)&rawSetter, "StackShape setter");
}

void
JSPropertyDescriptor::trace(JSTracer* trc)
{
    if (obj)
        TraceRoot(trc, &obj, "Descriptor::obj");
    TraceRoot(trc, &value, "Descriptor::value");
    if ((attrs & JSPROP_GETTER) && getter) {
        JSObject* tmp = JS_FUNC_TO_DATA_PTR(JSObject*, getter);
        TraceRoot(trc, &tmp, "Descriptor::get");
        getter = JS_DATA_TO_FUNC_PTR(JSGetterOp, tmp);
    }
    if ((attrs & JSPROP_SETTER) && setter) {
        JSObject* tmp = JS_FUNC_TO_DATA_PTR(JSObject*, setter);
        TraceRoot(trc, &tmp, "Descriptor::set");
        setter = JS_DATA_TO_FUNC_PTR(JSSetterOp, tmp);
    }
}

namespace js {
namespace gc {

template<typename T>
struct PersistentRootedMarker
{
    typedef PersistentRooted<T> Element;
    typedef mozilla::LinkedList<Element> List;
    typedef void (*MarkFunc)(JSTracer* trc, T* ref, const char* name);

    static void
    markChainIfNotNull(JSTracer* trc, List& list, const char* name)
    {
        for (Element* r = list.getFirst(); r; r = r->getNext()) {
            if (r->get())
                TraceRoot(trc, r->address(), name);
        }
    }

    static void
    markChain(JSTracer* trc, List& list, const char* name)
    {
        for (Element* r = list.getFirst(); r; r = r->getNext())
            TraceRoot(trc, r->address(), name);
    }
};
}
}

void
js::gc::MarkPersistentRootedChains(JSTracer* trc)
{
    JSRuntime* rt = trc->runtime();

    // Mark the PersistentRooted chains of types that may be null.
    PersistentRootedMarker<JSFunction*>::markChainIfNotNull(trc, rt->functionPersistentRooteds,
                                                            "PersistentRooted<JSFunction*>");
    PersistentRootedMarker<JSObject*>::markChainIfNotNull(trc, rt->objectPersistentRooteds,
                                                          "PersistentRooted<JSObject*>");
    PersistentRootedMarker<JSScript*>::markChainIfNotNull(trc, rt->scriptPersistentRooteds,
                                                          "PersistentRooted<JSScript*>");
    PersistentRootedMarker<JSString*>::markChainIfNotNull(trc, rt->stringPersistentRooteds,
                                                          "PersistentRooted<JSString*>");

    // Mark the PersistentRooted chains of types that are never null.
    PersistentRootedMarker<jsid>::markChain(trc, rt->idPersistentRooteds,
                                            "PersistentRooted<jsid>");
    PersistentRootedMarker<Value>::markChain(trc, rt->valuePersistentRooteds,
                                             "PersistentRooted<Value>");
}

void
js::gc::GCRuntime::markRuntime(JSTracer* trc,
                               TraceOrMarkRuntime traceOrMark,
                               TraceRootsOrUsedSaved rootsSource)
{
    gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_ROOTS);

    MOZ_ASSERT(traceOrMark == TraceRuntime || traceOrMark == MarkRuntime);
    MOZ_ASSERT(rootsSource == TraceRoots || rootsSource == UseSavedRoots);

    MOZ_ASSERT(!rt->mainThread.suppressGC);

    if (traceOrMark == MarkRuntime) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_CCWS);

        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
            if (!c->zone()->isCollecting())
                c->markCrossCompartmentWrappers(trc);
        }
        Debugger::markIncomingCrossCompartmentEdges(trc);
    }

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_ROOTERS);

        AutoGCRooter::traceAll(trc);

        if (!rt->isBeingDestroyed()) {
            MarkExactStackRoots(rt, trc);
            rt->markSelfHostingGlobal(trc);
        }

        for (RootRange r = rootsHash.all(); !r.empty(); r.popFront()) {
            const RootEntry& entry = r.front();
            TraceRoot(trc, entry.key(), entry.value());
        }

        MarkPersistentRootedChains(trc);
    }

    if (rt->asyncStackForNewActivations)
        TraceRoot(trc, &rt->asyncStackForNewActivations, "asyncStackForNewActivations");

    if (rt->asyncCauseForNewActivations)
        TraceRoot(trc, &rt->asyncCauseForNewActivations, "asyncCauseForNewActivations");

    if (rt->scriptAndCountsVector) {
        ScriptAndCountsVector& vec = *rt->scriptAndCountsVector;
        for (size_t i = 0; i < vec.length(); i++)
            TraceRoot(trc, &vec[i].script, "scriptAndCountsVector");
    }

    if (!rt->isBeingDestroyed() && !rt->isHeapMinorCollecting()) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_RUNTIME_DATA);

        if (traceOrMark == TraceRuntime || rt->atomsCompartment()->zone()->isCollecting()) {
            MarkPermanentAtoms(trc);
            MarkAtoms(trc);
            MarkWellKnownSymbols(trc);
            jit::JitRuntime::Mark(trc);
        }
    }

    for (ContextIter acx(rt); !acx.done(); acx.next())
        acx->mark(trc);

    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        if (traceOrMark == MarkRuntime && !zone->isCollecting())
            continue;

        /* Do not discard scripts with counts while profiling. */
        if (rt->profilingScripts && !rt->isHeapMinorCollecting()) {
            for (ZoneCellIterUnderGC i(zone, AllocKind::SCRIPT); !i.done(); i.next()) {
                JSScript* script = i.get<JSScript>();
                if (script->hasScriptCounts()) {
                    TraceRoot(trc, &script, "profilingScripts");
                    MOZ_ASSERT(script == i.get<JSScript>());
                }
            }
        }
    }

    /* We can't use GCCompartmentsIter if we're called from TraceRuntime. */
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        if (rt->isHeapMinorCollecting())
            c->globalWriteBarriered = false;

        if (traceOrMark == MarkRuntime && !c->zone()->isCollecting())
            continue;

        /* During a GC, these are treated as weak pointers. */
        if (traceOrMark == TraceRuntime) {
            if (c->watchpointMap)
                c->watchpointMap->markAll(trc);
        }

        /* Mark debug scopes, if present */
        if (c->debugScopes)
            c->debugScopes->mark(trc);

        if (c->lazyArrayBuffers)
            c->lazyArrayBuffers->trace(trc);

        if (c->objectMetadataTable)
            c->objectMetadataTable->trace(trc);
    }

    MarkInterpreterActivations(rt, trc);

    jit::MarkJitActivations(rt, trc);

    if (!rt->isHeapMinorCollecting()) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_EMBEDDING);

        /*
         * All JSCompartment::markRoots() does is mark the globals for
         * compartments which have been entered. Globals aren't nursery
         * allocated so there's no need to do this for minor GCs.
         */
        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
            c->markRoots(trc);

        /*
         * The embedding can register additional roots here.
         *
         * We don't need to trace these in a minor GC because all pointers into
         * the nursery should be in the store buffer, and we want to avoid the
         * time taken to trace all these roots.
         */
        for (size_t i = 0; i < blackRootTracers.length(); i++) {
            const Callback<JSTraceDataOp>& e = blackRootTracers[i];
            (*e.op)(trc, e.data);
        }

        /* During GC, we don't mark gray roots at this stage. */
        if (JSTraceDataOp op = grayRootTracer.op) {
            if (traceOrMark == TraceRuntime)
                (*op)(trc, grayRootTracer.data);
        }
    }
}

// Append traced things to a buffer on the zone for use later in the GC.
// See the comment in GCRuntime.h above grayBufferState for details.
class BufferGrayRootsTracer : public JS::CallbackTracer
{
    // Set to false if we OOM while buffering gray roots.
    bool bufferingGrayRootsFailed;

    void onChild(const JS::GCCellPtr& thing) override;

  public:
    explicit BufferGrayRootsTracer(JSRuntime* rt)
      : JS::CallbackTracer(rt), bufferingGrayRootsFailed(false)
    {}

    bool failed() const { return bufferingGrayRootsFailed; }

#ifdef DEBUG
    TracerKind getTracerKind() const override { return TracerKind::GrayBuffering; }
#endif
};

#ifdef DEBUG
// Return true if this trace is happening on behalf of gray buffering during
// the marking phase of incremental GC.
bool
js::IsBufferGrayRootsTracer(JSTracer* trc)
{
    return trc->isCallbackTracer() &&
           trc->asCallbackTracer()->getTracerKind() == JS::CallbackTracer::TracerKind::GrayBuffering;
}
#endif

void
js::gc::GCRuntime::bufferGrayRoots()
{
    // Precondition: the state has been reset to "unused" after the last GC
    //               and the zone's buffers have been cleared.
    MOZ_ASSERT(grayBufferState == GrayBufferState::Unused);
    for (GCZonesIter zone(rt); !zone.done(); zone.next())
        MOZ_ASSERT(zone->gcGrayRoots.empty());


    BufferGrayRootsTracer grayBufferer(rt);
    if (JSTraceDataOp op = grayRootTracer.op)
        (*op)(&grayBufferer, grayRootTracer.data);

    // Propagate the failure flag from the marker to the runtime.
    if (grayBufferer.failed()) {
      grayBufferState = GrayBufferState::Failed;
      resetBufferedGrayRoots();
    } else {
      grayBufferState = GrayBufferState::Okay;
    }
}

struct SetMaybeAliveFunctor {
    template <typename T> void operator()(T* t) { SetMaybeAliveFlag(t); }
};

void
BufferGrayRootsTracer::onChild(const JS::GCCellPtr& thing)
{
    MOZ_ASSERT(runtime()->isHeapBusy());

    if (bufferingGrayRootsFailed)
        return;

    gc::TenuredCell* tenured = gc::TenuredCell::fromPointer(thing.asCell());

    Zone* zone = tenured->zone();
    if (zone->isCollecting()) {
        // See the comment on SetMaybeAliveFlag to see why we only do this for
        // objects and scripts. We rely on gray root buffering for this to work,
        // but we only need to worry about uncollected dead compartments during
        // incremental GCs (when we do gray root buffering).
        CallTyped(SetMaybeAliveFunctor(), tenured, thing.kind());

        if (!zone->gcGrayRoots.append(tenured))
            bufferingGrayRootsFailed = true;
    }
}

void
GCRuntime::markBufferedGrayRoots(JS::Zone* zone)
{
    MOZ_ASSERT(grayBufferState == GrayBufferState::Okay);
    MOZ_ASSERT(zone->isGCMarkingGray() || zone->isGCCompacting());

    for (auto cell : zone->gcGrayRoots)
        TraceManuallyBarrieredGenericPointerEdge(&marker, &cell, "buffered gray root");
}

void
GCRuntime::resetBufferedGrayRoots() const
{
    MOZ_ASSERT(grayBufferState != GrayBufferState::Okay,
               "Do not clear the gray buffers unless we are Failed or becoming Unused");
    for (GCZonesIter zone(rt); !zone.done(); zone.next())
        zone->gcGrayRoots.clearAndFree();
}

