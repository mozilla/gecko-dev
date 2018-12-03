/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_VALGRIND
#include <valgrind/memcheck.h>
#endif

#include "jstypes.h"

#include "builtin/MapObject.h"
#include "frontend/BytecodeCompiler.h"
#include "gc/GCInternals.h"
#include "gc/Marking.h"
#include "jit/MacroAssembler.h"
#include "js/HashTable.h"
#include "vm/Debugger.h"
#include "vm/JSContext.h"
#include "vm/JSONParser.h"

#include "gc/Nursery-inl.h"
#include "gc/PrivateIterators-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::gc;

using JS::AutoGCRooter;

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

template <typename T>
using TraceFunction = void (*)(JSTracer* trc, T* ref, const char* name);

// For more detail see JS::Rooted::ptr and js::DispatchWrapper.
//
// The JS::RootKind::Traceable list contains a bunch of totally disparate
// types, but the instantiations of DispatchWrapper below need /something/ in
// the type field. We use the following type as a compatible stand-in. No
// actual methods from ConcreteTraceable type are actually used at runtime --
// the real trace function has been stored inline in the DispatchWrapper.
struct ConcreteTraceable {
  ConcreteTraceable() { MOZ_CRASH("instantiation of ConcreteTraceable"); }
  void trace(JSTracer*) {}
};

template <typename T>
static inline void TraceStackOrPersistentRoot(JSTracer* trc, T* thingp,
                                              const char* name) {
  TraceNullableRoot(trc, thingp, name);
}

template <>
inline void TraceStackOrPersistentRoot(JSTracer* trc, ConcreteTraceable* thingp,
                                       const char* name) {
  js::DispatchWrapper<ConcreteTraceable>::TraceWrapped(trc, thingp, name);
}

template <typename T>
static inline void TraceExactStackRootList(JSTracer* trc,
                                           JS::Rooted<void*>* rooter,
                                           const char* name) {
  while (rooter) {
    T* addr = reinterpret_cast<JS::Rooted<T>*>(rooter)->address();
    TraceStackOrPersistentRoot(trc, addr, name);
    rooter = rooter->previous();
  }
}

static inline void TraceStackRoots(JSTracer* trc,
                                   JS::RootedListHeads& stackRoots) {
#define TRACE_ROOTS(name, type, _)                                    \
  TraceExactStackRootList<type*>(trc, stackRoots[JS::RootKind::name], \
                                 "exact-" #name);
  JS_FOR_EACH_TRACEKIND(TRACE_ROOTS)
#undef TRACE_ROOTS
  TraceExactStackRootList<jsid>(trc, stackRoots[JS::RootKind::Id], "exact-id");
  TraceExactStackRootList<Value>(trc, stackRoots[JS::RootKind::Value],
                                 "exact-value");

  // ConcreteTraceable calls through a function pointer.
  JS::AutoSuppressGCAnalysis nogc;

  TraceExactStackRootList<ConcreteTraceable>(
      trc, stackRoots[JS::RootKind::Traceable], "Traceable");
}

void JS::RootingContext::traceStackRoots(JSTracer* trc) {
  TraceStackRoots(trc, stackRoots_);
}

static void TraceExactStackRoots(JSContext* cx, JSTracer* trc) {
  cx->traceStackRoots(trc);
}

template <typename T>
static inline void TracePersistentRootedList(
    JSTracer* trc, mozilla::LinkedList<PersistentRooted<void*>>& list,
    const char* name) {
  for (PersistentRooted<void*>* r : list) {
    TraceStackOrPersistentRoot(
        trc, reinterpret_cast<PersistentRooted<T>*>(r)->address(), name);
  }
}

void JSRuntime::tracePersistentRoots(JSTracer* trc) {
#define TRACE_ROOTS(name, type, _)                                           \
  TracePersistentRootedList<type*>(trc, heapRoots.ref()[JS::RootKind::name], \
                                   "persistent-" #name);
  JS_FOR_EACH_TRACEKIND(TRACE_ROOTS)
#undef TRACE_ROOTS
  TracePersistentRootedList<jsid>(trc, heapRoots.ref()[JS::RootKind::Id],
                                  "persistent-id");
  TracePersistentRootedList<Value>(trc, heapRoots.ref()[JS::RootKind::Value],
                                   "persistent-value");

  // ConcreteTraceable calls through a function pointer.
  JS::AutoSuppressGCAnalysis nogc;

  TracePersistentRootedList<ConcreteTraceable>(
      trc, heapRoots.ref()[JS::RootKind::Traceable], "persistent-traceable");
}

static void TracePersistentRooted(JSRuntime* rt, JSTracer* trc) {
  rt->tracePersistentRoots(trc);
}

template <typename T>
static void FinishPersistentRootedChain(
    mozilla::LinkedList<PersistentRooted<void*>>& listArg) {
  auto& list =
      reinterpret_cast<mozilla::LinkedList<PersistentRooted<T>>&>(listArg);
  while (!list.isEmpty()) {
    list.getFirst()->reset();
  }
}

void JSRuntime::finishPersistentRoots() {
#define FINISH_ROOT_LIST(name, type, _) \
  FinishPersistentRootedChain<type*>(heapRoots.ref()[JS::RootKind::name]);
  JS_FOR_EACH_TRACEKIND(FINISH_ROOT_LIST)
#undef FINISH_ROOT_LIST
  FinishPersistentRootedChain<jsid>(heapRoots.ref()[JS::RootKind::Id]);
  FinishPersistentRootedChain<Value>(heapRoots.ref()[JS::RootKind::Value]);

  // Note that we do not finalize the Traceable list as we do not know how to
  // safely clear members. We instead assert that none escape the RootLists.
  // See the comment on RootLists::~RootLists for details.
}

inline void AutoGCRooter::trace(JSTracer* trc) {
  switch (tag_) {
    case Tag::Parser:
      frontend::TraceParser(trc, this);
      return;

#if defined(JS_BUILD_BINAST)
    case Tag::BinParser:
      frontend::TraceBinParser(trc, this);
      return;
#endif  // defined(JS_BUILD_BINAST)

    case Tag::ValueArray: {
      /*
       * We don't know the template size parameter, but we can safely treat it
       * as an AutoValueArray<1> because the length is stored separately.
       */
      AutoValueArray<1>* array = static_cast<AutoValueArray<1>*>(this);
      TraceRootRange(trc, array->length(), array->begin(),
                     "js::AutoValueArray");
      return;
    }

    case Tag::Wrapper: {
      /*
       * We need to use TraceManuallyBarrieredEdge here because we trace
       * wrapper roots in every slice. This is because of some rule-breaking
       * in RemapAllWrappersForObject; see comment there.
       */
      TraceManuallyBarrieredEdge(
          trc, &static_cast<AutoWrapperRooter*>(this)->value.get(),
          "js::AutoWrapperRooter.value");
      return;
    }

    case Tag::WrapperVector: {
      auto vector = static_cast<AutoWrapperVector*>(this);
      /*
       * We need to use TraceManuallyBarrieredEdge here because we trace
       * wrapper roots in every slice. This is because of some rule-breaking
       * in RemapAllWrappersForObject; see comment there.
       */
      for (WrapperValue* p = vector->begin(); p < vector->end(); p++) {
        TraceManuallyBarrieredEdge(trc, &p->get(),
                                   "js::AutoWrapperVector.vector");
      }
      return;
    }

    case Tag::Custom:
      static_cast<JS::CustomAutoRooter*>(this)->trace(trc);
      return;

    case Tag::Array: {
      auto array = static_cast<AutoArrayRooter*>(this);
      if (Value* vp = array->begin()) {
        TraceRootRange(trc, array->length(), vp, "js::AutoArrayRooter");
      }
      return;
    }
  }

  MOZ_CRASH("Bad AutoGCRooter::Tag");
}

/* static */ void AutoGCRooter::traceAll(JSContext* cx, JSTracer* trc) {
  for (AutoGCRooter* gcr = cx->autoGCRooters_; gcr; gcr = gcr->down) {
    gcr->trace(trc);
  }
}

/* static */ void AutoGCRooter::traceAllWrappers(JSContext* cx, JSTracer* trc) {
  for (AutoGCRooter* gcr = cx->autoGCRooters_; gcr; gcr = gcr->down) {
    if (gcr->tag_ == Tag::WrapperVector || gcr->tag_ == Tag::Wrapper) {
      gcr->trace(trc);
    }
  }
}

void StackShape::trace(JSTracer* trc) {
  if (base) {
    TraceRoot(trc, &base, "StackShape base");
  }

  TraceRoot(trc, (jsid*)&propid, "StackShape id");

  if ((attrs & JSPROP_GETTER) && rawGetter) {
    TraceRoot(trc, (JSObject**)&rawGetter, "StackShape getter");
  }

  if ((attrs & JSPROP_SETTER) && rawSetter) {
    TraceRoot(trc, (JSObject**)&rawSetter, "StackShape setter");
  }
}

void PropertyDescriptor::trace(JSTracer* trc) {
  if (obj) {
    TraceRoot(trc, &obj, "Descriptor::obj");
  }
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

void js::gc::GCRuntime::traceRuntimeForMajorGC(JSTracer* trc,
                                               AutoGCSession& session) {
  MOZ_ASSERT(!TlsContext.get()->suppressGC);

  // FinishRoots will have asserted that every root that we do not expect
  // is gone, so we can simply skip traceRuntime here.
  if (rt->isBeingDestroyed()) {
    return;
  }

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_ROOTS);
  if (atomsZone->isCollecting()) {
    traceRuntimeAtoms(trc, session.checkAtomsAccess());
  }
  traceKeptAtoms(trc);
  Compartment::traceIncomingCrossCompartmentEdgesForZoneGC(trc);
  traceRuntimeCommon(trc, MarkRuntime);
}

void js::gc::GCRuntime::traceRuntimeForMinorGC(JSTracer* trc,
                                               AutoGCSession& session) {
  MOZ_ASSERT(!TlsContext.get()->suppressGC);

  // Note that we *must* trace the runtime during the SHUTDOWN_GC's minor GC
  // despite having called FinishRoots already. This is because FinishRoots
  // does not clear the crossCompartmentWrapper map. It cannot do this
  // because Proxy's trace for CrossCompartmentWrappers asserts presence in
  // the map. And we can reach its trace function despite having finished the
  // roots via the edges stored by the pre-barrier verifier when we finish
  // the verifier for the last time.
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_ROOTS);

  jit::JitRuntime::TraceJitcodeGlobalTableForMinorGC(trc);

  traceRuntimeCommon(trc, TraceRuntime);
}

void js::TraceRuntime(JSTracer* trc) {
  MOZ_ASSERT(!trc->isMarkingTracer());

  JSRuntime* rt = trc->runtime();
  rt->gc.evictNursery();
  AutoPrepareForTracing prep(rt->mainContextFromOwnThread());
  gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::TRACE_HEAP);
  rt->gc.traceRuntime(trc, prep);
}

void js::gc::GCRuntime::traceRuntime(JSTracer* trc, AutoTraceSession& session) {
  MOZ_ASSERT(!rt->isBeingDestroyed());

  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_ROOTS);

  traceRuntimeAtoms(trc, session);
  traceRuntimeCommon(trc, TraceRuntime);
}

void js::gc::GCRuntime::traceRuntimeAtoms(JSTracer* trc,
                                          const AutoAccessAtomsZone& access) {
  gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_RUNTIME_DATA);
  rt->tracePermanentAtoms(trc);
  TraceAtoms(trc, access);
  TraceWellKnownSymbols(trc);
  jit::JitRuntime::Trace(trc, access);
}

void js::gc::GCRuntime::traceKeptAtoms(JSTracer* trc) {
  // We don't have exact rooting information for atoms while parsing. When
  // this is happeninng we set a flag on the zone and trace all atoms in the
  // zone's cache.
  for (GCZonesIter zone(trc->runtime()); !zone.done(); zone.next()) {
    if (zone->hasKeptAtoms()) {
      zone->traceAtomCache(trc);
    }
  }
}

void js::gc::GCRuntime::traceRuntimeCommon(JSTracer* trc,
                                           TraceOrMarkRuntime traceOrMark) {
  {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_STACK);

    JSContext* cx = rt->mainContextFromOwnThread();

    // Trace active interpreter and JIT stack roots.
    TraceInterpreterActivations(cx, trc);
    jit::TraceJitActivations(cx, trc);

    // Trace legacy C stack roots.
    AutoGCRooter::traceAll(cx, trc);

    // Trace C stack roots.
    TraceExactStackRoots(cx, trc);

    for (RootRange r = rootsHash.ref().all(); !r.empty(); r.popFront()) {
      const RootEntry& entry = r.front();
      TraceRoot(trc, entry.key(), entry.value());
    }
  }

  // Trace runtime global roots.
  TracePersistentRooted(rt, trc);

  // Trace the self-hosting global compartment.
  rt->traceSelfHostingGlobal(trc);

  // Trace the shared Intl data.
  rt->traceSharedIntlData(trc);

  // Trace the JSContext.
  rt->mainContextFromOwnThread()->trace(trc);

  // Trace all realm roots, but not the realm itself; it is traced via the
  // parent pointer if traceRoots actually traces anything.
  for (RealmsIter r(rt); !r.done(); r.next()) {
    r->traceRoots(trc, traceOrMark);
  }

  // Trace helper thread roots.
  HelperThreadState().trace(trc);

  // Trace the embedding's black and gray roots.
  if (!JS::RuntimeHeapIsMinorCollecting()) {
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_EMBEDDING);

    // The analysis doesn't like the function pointers below.
    JS::AutoSuppressGCAnalysis nogc;

    /*
     * The embedding can register additional roots here.
     *
     * We don't need to trace these in a minor GC because all pointers into
     * the nursery should be in the store buffer, and we want to avoid the
     * time taken to trace all these roots.
     */
    for (size_t i = 0; i < blackRootTracers.ref().length(); i++) {
      const Callback<JSTraceDataOp>& e = blackRootTracers.ref()[i];
      (*e.op)(trc, e.data);
    }

    /* During GC, we don't trace gray roots at this stage. */
    if (JSTraceDataOp op = grayRootTracer.op) {
      if (traceOrMark == TraceRuntime) {
        (*op)(trc, grayRootTracer.data);
      }
    }
  }
}

#ifdef DEBUG
class AssertNoRootsTracer : public JS::CallbackTracer {
  void onChild(const JS::GCCellPtr& thing) override {
    MOZ_CRASH("There should not be any roots after finishRoots");
  }

 public:
  AssertNoRootsTracer(JSRuntime* rt, WeakMapTraceKind weakTraceKind)
      : JS::CallbackTracer(rt, weakTraceKind) {}
};
#endif  // DEBUG

void js::gc::GCRuntime::finishRoots() {
  AutoNoteSingleThreadedRegion anstr;

  rt->finishAtoms();

  rootsHash.ref().clear();

  rt->finishPersistentRoots();

  rt->finishSelfHosting();

  for (RealmsIter r(rt); !r.done(); r.next()) {
    r->finishRoots();
  }

#ifdef DEBUG
  // The nsWrapperCache may not be empty before our shutdown GC, so we have
  // to skip that table when verifying that we are fully unrooted.
  auto prior = grayRootTracer;
  grayRootTracer = Callback<JSTraceDataOp>(nullptr, nullptr);

  AssertNoRootsTracer trc(rt, TraceWeakMapKeysValues);
  AutoTraceSession session(rt);
  gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::TRACE_HEAP);
  traceRuntime(&trc, session);

  // Restore the wrapper tracing so that we leak instead of leaving dangling
  // pointers.
  grayRootTracer = prior;
#endif  // DEBUG
}

// Append traced things to a buffer on the zone for use later in the GC.
// See the comment in GCRuntime.h above grayBufferState for details.
class BufferGrayRootsTracer final : public JS::CallbackTracer {
  // Set to false if we OOM while buffering gray roots.
  bool bufferingGrayRootsFailed;

  void onObjectEdge(JSObject** objp) override { bufferRoot(*objp); }
  void onStringEdge(JSString** stringp) override { bufferRoot(*stringp); }
  void onScriptEdge(JSScript** scriptp) override { bufferRoot(*scriptp); }
  void onSymbolEdge(JS::Symbol** symbolp) override { bufferRoot(*symbolp); }
#ifdef ENABLE_BIGINT
  void onBigIntEdge(JS::BigInt** bip) override { bufferRoot(*bip); }
#endif

  void onChild(const JS::GCCellPtr& thing) override {
    MOZ_CRASH("Unexpected gray root kind");
  }

  template <typename T>
  inline void bufferRoot(T* thing);

 public:
  explicit BufferGrayRootsTracer(JSRuntime* rt)
      : JS::CallbackTracer(rt), bufferingGrayRootsFailed(false) {}

  bool failed() const { return bufferingGrayRootsFailed; }
  void setFailed() { bufferingGrayRootsFailed = true; }

#ifdef DEBUG
  TracerKind getTracerKind() const override {
    return TracerKind::GrayBuffering;
  }
#endif
};

#ifdef DEBUG
// Return true if this trace is happening on behalf of gray buffering during
// the marking phase of incremental GC.
bool js::IsBufferGrayRootsTracer(JSTracer* trc) {
  return trc->isCallbackTracer() &&
         trc->asCallbackTracer()->getTracerKind() ==
             JS::CallbackTracer::TracerKind::GrayBuffering;
}
#endif

void js::gc::GCRuntime::bufferGrayRoots() {
  // Precondition: the state has been reset to "unused" after the last GC
  //               and the zone's buffers have been cleared.
  MOZ_ASSERT(grayBufferState == GrayBufferState::Unused);
  for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
    MOZ_ASSERT(zone->gcGrayRoots().empty());
  }

  BufferGrayRootsTracer grayBufferer(rt);
  if (JSTraceDataOp op = grayRootTracer.op) {
    (*op)(&grayBufferer, grayRootTracer.data);
  }

  // Propagate the failure flag from the marker to the runtime.
  if (grayBufferer.failed()) {
    grayBufferState = GrayBufferState::Failed;
    resetBufferedGrayRoots();
  } else {
    grayBufferState = GrayBufferState::Okay;
  }
}

template <typename T>
inline void BufferGrayRootsTracer::bufferRoot(T* thing) {
  MOZ_ASSERT(JS::RuntimeHeapIsBusy());
  MOZ_ASSERT(thing);
  // Check if |thing| is corrupt by calling a method that touches the heap.
  MOZ_ASSERT(thing->getTraceKind() != JS::TraceKind(0xff));

  TenuredCell* tenured = &thing->asTenured();

  // This is run from a helper thread while the mutator is paused so we have
  // to use *FromAnyThread methods here.
  Zone* zone = tenured->zoneFromAnyThread();
  if (zone->isCollectingFromAnyThread()) {
    // See the comment on SetMaybeAliveFlag to see why we only do this for
    // objects and scripts. We rely on gray root buffering for this to work,
    // but we only need to worry about uncollected dead compartments during
    // incremental GCs (when we do gray root buffering).
    SetMaybeAliveFlag(thing);

    if (!zone->gcGrayRoots().append(tenured)) {
      bufferingGrayRootsFailed = true;
    }
  }
}

void GCRuntime::markBufferedGrayRoots(JS::Zone* zone) {
  MOZ_ASSERT(grayBufferState == GrayBufferState::Okay);
  MOZ_ASSERT(zone->isGCMarkingGray() || zone->isGCCompacting());

  auto& roots = zone->gcGrayRoots();
  if (roots.empty()) {
    return;
  }

  for (size_t i = 0; i < roots.length(); i++) {
    Cell* cell = roots[i];

    // Bug 1203273: Check for bad pointers on OSX and output diagnostics.
#if defined(XP_DARWIN) && defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
    auto addr = uintptr_t(cell);
    if (addr < ChunkSize || addr % CellAlignBytes != 0) {
      MOZ_CRASH_UNSAFE_PRINTF(
          "Bad GC thing pointer in gray root buffer: %p at index %zu of %zu, "
          "address %p",
          cell, i, roots.length(), &roots[i]);
    }
#else
    MOZ_ASSERT(IsCellPointerValid(cell));
#endif

    TraceManuallyBarrieredGenericPointerEdge(&marker, &cell,
                                             "buffered gray root");
  }
}

void GCRuntime::resetBufferedGrayRoots() const {
  MOZ_ASSERT(
      grayBufferState != GrayBufferState::Okay,
      "Do not clear the gray buffers unless we are Failed or becoming Unused");
  for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
    zone->gcGrayRoots().clearAndFree();
  }
}

JS_PUBLIC_API void JS::AddPersistentRoot(JS::RootingContext* cx, RootKind kind,
                                         PersistentRooted<void*>* root) {
  static_cast<JSContext*>(cx)->runtime()->heapRoots.ref()[kind].insertBack(
      root);
}

JS_PUBLIC_API void JS::AddPersistentRoot(JSRuntime* rt, RootKind kind,
                                         PersistentRooted<void*>* root) {
  rt->heapRoots.ref()[kind].insertBack(root);
}
