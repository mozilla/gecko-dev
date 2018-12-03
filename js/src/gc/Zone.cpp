/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone-inl.h"

#include "gc/FreeOp.h"
#include "gc/Policy.h"
#include "gc/PublicIterators.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/JitRealm.h"
#include "vm/Debugger.h"
#include "vm/Runtime.h"
#include "wasm/WasmInstance.h"

#include "gc/GC-inl.h"
#include "gc/Marking-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Realm-inl.h"

using namespace js;
using namespace js::gc;

Zone* const Zone::NotOnList = reinterpret_cast<Zone*>(1);

JS::Zone::Zone(JSRuntime* rt)
    : JS::shadow::Zone(rt, &rt->gc.marker),
      // Note: don't use |this| before initializing helperThreadUse_!
      // ProtectedData checks in CheckZone::check may read this field.
      helperThreadUse_(HelperThreadUse::None),
      helperThreadOwnerContext_(nullptr),
      debuggers(this, nullptr),
      uniqueIds_(this),
      suppressAllocationMetadataBuilder(this, false),
      arenas(this),
      tenuredAllocsSinceMinorGC_(0),
      types(this),
      gcWeakMapList_(this),
      compartments_(),
      gcGrayRoots_(this),
      gcWeakRefs_(this),
      weakCaches_(this),
      gcWeakKeys_(this, SystemAllocPolicy(), rt->randomHashCodeScrambler()),
      typeDescrObjects_(this, this),
      markedAtoms_(this),
      atomCache_(this),
      externalStringCache_(this),
      functionToStringCache_(this),
      keepAtomsCount(this, 0),
      purgeAtomsDeferred(this, 0),
      usage(&rt->gc.usage),
      threshold(),
      gcDelayBytes(0),
      tenuredStrings(this, 0),
      allocNurseryStrings(this, true),
      propertyTree_(this, this),
      baseShapes_(this, this),
      initialShapes_(this, this),
      nurseryShapes_(this),
      data(this, nullptr),
      isSystem(this, false),
#ifdef DEBUG
      gcLastSweepGroupIndex(0),
#endif
      jitZone_(this, nullptr),
      gcScheduled_(false),
      gcScheduledSaved_(false),
      gcPreserveCode_(false),
      keepShapeTables_(this, false),
      listNext_(NotOnList) {
  /* Ensure that there are no vtables to mess us up here. */
  MOZ_ASSERT(reinterpret_cast<JS::shadow::Zone*>(this) ==
             static_cast<JS::shadow::Zone*>(this));

  AutoLockGC lock(rt);
  threshold.updateAfterGC(8192, GC_NORMAL, rt->gc.tunables,
                          rt->gc.schedulingState, lock);
  setGCMaxMallocBytes(rt->gc.tunables.maxMallocBytes(), lock);
  jitCodeCounter.setMax(jit::MaxCodeBytesPerProcess * 0.8, lock);
}

Zone::~Zone() {
  MOZ_ASSERT(helperThreadUse_ == HelperThreadUse::None);

  JSRuntime* rt = runtimeFromAnyThread();
  if (this == rt->gc.systemZone) {
    rt->gc.systemZone = nullptr;
  }

  js_delete(debuggers.ref());
  js_delete(jitZone_.ref());

#ifdef DEBUG
  // Avoid assertions failures warning that not everything has been destroyed
  // if the embedding leaked GC things.
  if (!rt->gc.shutdownCollectedEverything()) {
    gcWeakMapList().clear();
    regExps().clear();
  }
#endif
}

bool Zone::init(bool isSystemArg) {
  isSystem = isSystemArg;
  regExps_.ref() = make_unique<RegExpZone>(this);
  return regExps_.ref() && gcWeakKeys().init();
}

void Zone::setNeedsIncrementalBarrier(bool needs) {
  MOZ_ASSERT_IF(needs, canCollect());
  needsIncrementalBarrier_ = needs;
}

void Zone::beginSweepTypes() { types.beginSweep(); }

Zone::DebuggerVector* Zone::getOrCreateDebuggers(JSContext* cx) {
  if (debuggers) {
    return debuggers;
  }

  debuggers = js_new<DebuggerVector>();
  if (!debuggers) {
    ReportOutOfMemory(cx);
  }
  return debuggers;
}

void Zone::sweepBreakpoints(FreeOp* fop) {
  if (fop->runtime()->debuggerList().isEmpty()) {
    return;
  }

  /*
   * Sweep all compartments in a zone at the same time, since there is no way
   * to iterate over the scripts belonging to a single compartment in a zone.
   */

  MOZ_ASSERT(isGCSweepingOrCompacting());
  for (auto iter = cellIter<JSScript>(); !iter.done(); iter.next()) {
    JSScript* script = iter;
    if (!script->hasAnyBreakpointsOrStepMode()) {
      continue;
    }

    bool scriptGone = IsAboutToBeFinalizedUnbarriered(&script);
    MOZ_ASSERT(script == iter);
    for (unsigned i = 0; i < script->length(); i++) {
      BreakpointSite* site = script->getBreakpointSite(script->offsetToPC(i));
      if (!site) {
        continue;
      }

      Breakpoint* nextbp;
      for (Breakpoint* bp = site->firstBreakpoint(); bp; bp = nextbp) {
        nextbp = bp->nextInSite();
        GCPtrNativeObject& dbgobj = bp->debugger->toJSObjectRef();

        // If we are sweeping, then we expect the script and the
        // debugger object to be swept in the same sweep group, except
        // if the breakpoint was added after we computed the sweep
        // groups. In this case both script and debugger object must be
        // live.
        MOZ_ASSERT_IF(isGCSweeping() && dbgobj->zone()->isCollecting(),
                      dbgobj->zone()->isGCSweeping() ||
                          (!scriptGone && dbgobj->asTenured().isMarkedAny()));

        bool dying = scriptGone || IsAboutToBeFinalized(&dbgobj);
        MOZ_ASSERT_IF(!dying, !IsAboutToBeFinalized(&bp->getHandlerRef()));
        if (dying) {
          bp->destroy(fop);
        }
      }
    }
  }

  for (RealmsInZoneIter realms(this); !realms.done(); realms.next()) {
    for (wasm::Instance* instance : realms->wasm.instances()) {
      if (!instance->debugEnabled()) {
        continue;
      }
      if (!IsAboutToBeFinalized(&instance->object_)) {
        continue;
      }
      instance->debug().clearAllBreakpoints(fop, instance->objectUnbarriered());
    }
  }
}

void Zone::sweepWeakMaps() {
  /* Finalize unreachable (key,value) pairs in all weak maps. */
  WeakMapBase::sweepZone(this);
}

void Zone::discardJitCode(FreeOp* fop, bool discardBaselineCode,
                          bool releaseTypes) {
  if (!jitZone()) {
    return;
  }

  if (isPreservingCode()) {
    return;
  }

  if (discardBaselineCode) {
#ifdef DEBUG
    /* Assert no baseline scripts are marked as active. */
    for (auto script = cellIter<JSScript>(); !script.done(); script.next()) {
      MOZ_ASSERT_IF(script->hasBaselineScript(),
                    !script->baselineScript()->active());
    }
#endif

    /* Mark baseline scripts on the stack as active. */
    jit::MarkActiveBaselineScripts(this);
  }

  /* Only mark OSI points if code is being discarded. */
  jit::InvalidateAll(fop, this);

  for (auto script = cellIter<JSScript>(); !script.done(); script.next()) {
    jit::FinishInvalidation(fop, script);

    /*
     * Discard baseline script if it's not marked as active. Note that
     * this also resets the active flag.
     */
    if (discardBaselineCode) {
      jit::FinishDiscardBaselineScript(fop, script);
    }

    /*
     * Warm-up counter for scripts are reset on GC. After discarding code we
     * need to let it warm back up to get information such as which
     * opcodes are setting array holes or accessing getter properties.
     */
    script->resetWarmUpCounter();

    /*
     * Make it impossible to use the control flow graphs cached on the
     * BaselineScript. They get deleted.
     */
    if (script->hasBaselineScript()) {
      script->baselineScript()->setControlFlowGraph(nullptr);
    }

    // Try to release the script's TypeScript. This should happen after
    // releasing JIT code because we can't do this when the script still has
    // JIT code.
    if (releaseTypes) {
      script->maybeReleaseTypes();
    }

    // The optimizedStubSpace will be purged below so make sure ICScript
    // doesn't point into it. We do this after (potentially) releasing types
    // because TypeScript contains the ICScript* and there's no need to
    // purge stubs if we just destroyed the Typescript.
    if (discardBaselineCode && script->hasICScript()) {
      script->icScript()->purgeOptimizedStubs(script);
    }
  }

  /*
   * When scripts contains pointers to nursery things, the store buffer
   * can contain entries that point into the optimized stub space. Since
   * this method can be called outside the context of a GC, this situation
   * could result in us trying to mark invalid store buffer entries.
   *
   * Defer freeing any allocated blocks until after the next minor GC.
   */
  if (discardBaselineCode) {
    jitZone()->optimizedStubSpace()->freeAllAfterMinorGC(this);
    jitZone()->purgeIonCacheIRStubInfo();
  }

  /*
   * Free all control flow graphs that are cached on BaselineScripts.
   * Assuming this happens on the main thread and all control flow
   * graph reads happen on the main thread, this is safe.
   */
  jitZone()->cfgSpace()->lifoAlloc().freeAll();
}

#ifdef JSGC_HASH_TABLE_CHECKS
void JS::Zone::checkUniqueIdTableAfterMovingGC() {
  for (auto r = uniqueIds().all(); !r.empty(); r.popFront()) {
    js::gc::CheckGCThingAfterMovingGC(r.front().key());
  }
}
#endif

uint64_t Zone::gcNumber() {
  // Zones in use by exclusive threads are not collected, and threads using
  // them cannot access the main runtime's gcNumber without racing.
  return usedByHelperThread() ? 0 : runtimeFromMainThread()->gc.gcNumber();
}

js::jit::JitZone* Zone::createJitZone(JSContext* cx) {
  MOZ_ASSERT(!jitZone_);

  if (!cx->runtime()->getJitRuntime(cx)) {
    return nullptr;
  }

  UniquePtr<jit::JitZone> jitZone(cx->new_<js::jit::JitZone>());
  if (!jitZone) {
    return nullptr;
  }

  jitZone_ = jitZone.release();
  return jitZone_;
}

bool Zone::hasMarkedRealms() {
  for (RealmsInZoneIter realm(this); !realm.done(); realm.next()) {
    if (realm->marked()) {
      return true;
    }
  }
  return false;
}

bool Zone::canCollect() {
  // The atoms zone cannot be collected while off-thread parsing is taking
  // place.
  if (isAtomsZone()) {
    return !runtimeFromAnyThread()->hasHelperThreadZones();
  }

  // Zones that will be or are currently used by other threads cannot be
  // collected.
  return !createdForHelperThread();
}

void Zone::notifyObservingDebuggers() {
  JSRuntime* rt = runtimeFromMainThread();
  JSContext* cx = rt->mainContextFromOwnThread();

  for (RealmsInZoneIter realms(this); !realms.done(); realms.next()) {
    RootedGlobalObject global(cx, realms->unsafeUnbarrieredMaybeGlobal());
    if (!global) {
      continue;
    }

    GlobalObject::DebuggerVector* dbgs = global->getDebuggers();
    if (!dbgs) {
      continue;
    }

    for (GlobalObject::DebuggerVector::Range r = dbgs->all(); !r.empty();
         r.popFront()) {
      if (!r.front()->debuggeeIsBeingCollected(rt->gc.majorGCCount())) {
#ifdef DEBUG
        fprintf(stderr,
                "OOM while notifying observing Debuggers of a GC: The "
                "onGarbageCollection\n"
                "hook will not be fired for this GC for some Debuggers!\n");
#endif
        return;
      }
    }
  }
}

bool Zone::isOnList() const { return listNext_ != NotOnList; }

Zone* Zone::nextZone() const {
  MOZ_ASSERT(isOnList());
  return listNext_;
}

void Zone::clearTables() {
  MOZ_ASSERT(regExps().empty());

  baseShapes().clear();
  initialShapes().clear();
}

void Zone::fixupAfterMovingGC() { fixupInitialShapeTable(); }

bool Zone::addTypeDescrObject(JSContext* cx, HandleObject obj) {
  // Type descriptor objects are always tenured so we don't need post barriers
  // on the set.
  MOZ_ASSERT(!IsInsideNursery(obj));

  if (!typeDescrObjects().put(obj)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

void Zone::deleteEmptyCompartment(JS::Compartment* comp) {
  MOZ_ASSERT(comp->zone() == this);
  MOZ_ASSERT(arenas.checkEmptyArenaLists());

  MOZ_ASSERT(compartments().length() == 1);
  MOZ_ASSERT(compartments()[0] == comp);
  MOZ_ASSERT(comp->realms().length() == 1);

  Realm* realm = comp->realms()[0];
  FreeOp* fop = runtimeFromMainThread()->defaultFreeOp();
  realm->destroy(fop);
  comp->destroy(fop);

  compartments().clear();
}

void Zone::setHelperThreadOwnerContext(JSContext* cx) {
  MOZ_ASSERT_IF(cx, TlsContext.get() == cx);
  helperThreadOwnerContext_ = cx;
}

bool Zone::ownedByCurrentHelperThread() {
  MOZ_ASSERT(usedByHelperThread());
  MOZ_ASSERT(TlsContext.get());
  return helperThreadOwnerContext_ == TlsContext.get();
}

void Zone::releaseAtoms() {
  MOZ_ASSERT(hasKeptAtoms());

  keepAtomsCount--;

  if (!hasKeptAtoms() && purgeAtomsDeferred) {
    purgeAtomsDeferred = false;
    purgeAtomCache();
  }
}

void Zone::purgeAtomCacheOrDefer() {
  if (hasKeptAtoms()) {
    purgeAtomsDeferred = true;
    return;
  }

  purgeAtomCache();
}

void Zone::purgeAtomCache() {
  MOZ_ASSERT(!hasKeptAtoms());
  MOZ_ASSERT(!purgeAtomsDeferred);

  atomCache().clearAndCompact();

  // Also purge the dtoa caches so that subsequent lookups populate atom
  // cache too.
  for (RealmsInZoneIter r(this); !r.done(); r.next()) {
    r->dtoaCache.purge();
  }
}

void Zone::traceAtomCache(JSTracer* trc) {
  MOZ_ASSERT(hasKeptAtoms());
  for (auto r = atomCache().all(); !r.empty(); r.popFront()) {
    JSAtom* atom = r.front().asPtrUnbarriered();
    TraceRoot(trc, &atom, "kept atom");
    MOZ_ASSERT(r.front().asPtrUnbarriered() == atom);
  }
}

void* Zone::onOutOfMemory(js::AllocFunction allocFunc, size_t nbytes,
                          void* reallocPtr) {
  if (!js::CurrentThreadCanAccessRuntime(runtime_)) {
    return nullptr;
  }
  return runtimeFromMainThread()->onOutOfMemory(allocFunc, nbytes, reallocPtr);
}

void Zone::reportAllocationOverflow() { js::ReportAllocationOverflow(nullptr); }

void JS::Zone::maybeTriggerGCForTooMuchMalloc(js::gc::MemoryCounter& counter,
                                              TriggerKind trigger) {
  JSRuntime* rt = runtimeFromAnyThread();

  if (!js::CurrentThreadCanAccessRuntime(rt)) {
    return;
  }

  bool wouldInterruptGC = rt->gc.isIncrementalGCInProgress() && !isCollecting();
  if (wouldInterruptGC && !counter.shouldResetIncrementalGC(rt->gc.tunables)) {
    return;
  }

  if (!rt->gc.triggerZoneGC(this, JS::gcreason::TOO_MUCH_MALLOC,
                            counter.bytes(), counter.maxBytes())) {
    return;
  }

  counter.recordTrigger(trigger);
}

ZoneList::ZoneList() : head(nullptr), tail(nullptr) {}

ZoneList::ZoneList(Zone* zone) : head(zone), tail(zone) {
  MOZ_RELEASE_ASSERT(!zone->isOnList());
  zone->listNext_ = nullptr;
}

ZoneList::~ZoneList() { MOZ_ASSERT(isEmpty()); }

void ZoneList::check() const {
#ifdef DEBUG
  MOZ_ASSERT((head == nullptr) == (tail == nullptr));
  if (!head) {
    return;
  }

  Zone* zone = head;
  for (;;) {
    MOZ_ASSERT(zone && zone->isOnList());
    if (zone == tail) break;
    zone = zone->listNext_;
  }
  MOZ_ASSERT(!zone->listNext_);
#endif
}

bool ZoneList::isEmpty() const { return head == nullptr; }

Zone* ZoneList::front() const {
  MOZ_ASSERT(!isEmpty());
  MOZ_ASSERT(head->isOnList());
  return head;
}

void ZoneList::append(Zone* zone) {
  ZoneList singleZone(zone);
  transferFrom(singleZone);
}

void ZoneList::transferFrom(ZoneList& other) {
  check();
  other.check();
  MOZ_ASSERT(tail != other.tail);

  if (tail) {
    tail->listNext_ = other.head;
  } else {
    head = other.head;
  }
  tail = other.tail;

  other.head = nullptr;
  other.tail = nullptr;
}

Zone* ZoneList::removeFront() {
  MOZ_ASSERT(!isEmpty());
  check();

  Zone* front = head;
  head = head->listNext_;
  if (!head) {
    tail = nullptr;
  }

  front->listNext_ = Zone::NotOnList;

  return front;
}

void ZoneList::clear() {
  while (!isEmpty()) {
    removeFront();
  }
}

JS_PUBLIC_API void JS::shadow::RegisterWeakCache(
    JS::Zone* zone, detail::WeakCacheBase* cachep) {
  zone->registerWeakCache(cachep);
}
