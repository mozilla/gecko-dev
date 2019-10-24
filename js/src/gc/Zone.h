/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Zone_h
#define gc_Zone_h

#include "mozilla/Atomics.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/SegmentedVector.h"

#include "gc/FindSCCs.h"
#include "gc/NurseryAwareHashMap.h"
#include "gc/ZoneAllocator.h"
#include "js/GCHashTable.h"
#include "vm/MallocProvider.h"
#include "vm/Runtime.h"
#include "vm/TypeInference.h"

namespace js {

class Debugger;
class RegExpZone;

namespace jit {
class JitZone;
}  // namespace jit

namespace gc {

using ZoneComponentFinder = ComponentFinder<JS::Zone>;

struct UniqueIdGCPolicy {
  static bool needsSweep(Cell** cell, uint64_t* value);
};

// Maps a Cell* to a unique, 64bit id.
using UniqueIdMap = GCHashMap<Cell*, uint64_t, PointerHasher<Cell*>,
                              SystemAllocPolicy, UniqueIdGCPolicy>;

extern uint64_t NextCellUniqueId(JSRuntime* rt);

template <typename T>
class ZoneAllCellIter;

template <typename T>
class ZoneCellIter;

}  // namespace gc

using StringWrapperMap =
    NurseryAwareHashMap<JSString*, JSString*, DefaultHasher<JSString*>,
                        ZoneAllocPolicy>;

class MOZ_NON_TEMPORARY_CLASS ExternalStringCache {
  static const size_t NumEntries = 4;
  mozilla::Array<JSString*, NumEntries> entries_;

  ExternalStringCache(const ExternalStringCache&) = delete;
  void operator=(const ExternalStringCache&) = delete;

 public:
  ExternalStringCache() { purge(); }
  void purge() { mozilla::PodArrayZero(entries_); }

  MOZ_ALWAYS_INLINE JSString* lookup(const char16_t* chars, size_t len) const;
  MOZ_ALWAYS_INLINE void put(JSString* s);
};

class MOZ_NON_TEMPORARY_CLASS FunctionToStringCache {
  struct Entry {
    JSScript* script;
    JSString* string;

    void set(JSScript* scriptArg, JSString* stringArg) {
      script = scriptArg;
      string = stringArg;
    }
  };
  static const size_t NumEntries = 2;
  mozilla::Array<Entry, NumEntries> entries_;

  FunctionToStringCache(const FunctionToStringCache&) = delete;
  void operator=(const FunctionToStringCache&) = delete;

 public:
  FunctionToStringCache() { purge(); }
  void purge() { mozilla::PodArrayZero(entries_); }

  MOZ_ALWAYS_INLINE JSString* lookup(JSScript* script) const;
  MOZ_ALWAYS_INLINE void put(JSScript* script, JSString* string);
};

}  // namespace js

namespace JS {

// [SMDOC] GC Zones
//
// A zone is a collection of compartments. Every compartment belongs to exactly
// one zone. In Firefox, there is roughly one zone per tab along with a system
// zone for everything else. Zones mainly serve as boundaries for garbage
// collection. Unlike compartments, they have no special security properties.
//
// Every GC thing belongs to exactly one zone. GC things from the same zone but
// different compartments can share an arena (4k page). GC things from different
// zones cannot be stored in the same arena. The garbage collector is capable of
// collecting one zone at a time; it cannot collect at the granularity of
// compartments.
//
// GC things are tied to zones and compartments as follows:
//
// - JSObjects belong to a compartment and cannot be shared between
//   compartments. If an object needs to point to a JSObject in a different
//   compartment, regardless of zone, it must go through a cross-compartment
//   wrapper. Each compartment keeps track of its outgoing wrappers in a table.
//   JSObjects find their compartment via their ObjectGroup.
//
// - JSStrings do not belong to any particular compartment, but they do belong
//   to a zone. Thus, two different compartments in the same zone can point to a
//   JSString. When a string needs to be wrapped, we copy it if it's in a
//   different zone and do nothing if it's in the same zone. Thus, transferring
//   strings within a zone is very efficient.
//
// - Shapes and base shapes belong to a zone and are shared between compartments
//   in that zone where possible. Accessor shapes store getter and setter
//   JSObjects which belong to a single compartment, so these shapes and all
//   their descendants can't be shared with other compartments.
//
// - Scripts are also compartment-local and cannot be shared. A script points to
//   its compartment.
//
// - ObjectGroup and JitCode objects belong to a compartment and cannot be
//   shared. There is no mechanism to obtain the compartment from a JitCode
//   object.
//
// A zone remains alive as long as any GC things in the zone are alive. A
// compartment remains alive as long as any JSObjects, scripts, shapes, or base
// shapes within it are alive.
//
// We always guarantee that a zone has at least one live compartment by refusing
// to delete the last compartment in a live zone.
class Zone : public js::ZoneAllocator, public js::gc::GraphNodeBase<JS::Zone> {
 public:
  explicit Zone(JSRuntime* rt);
  ~Zone();
  MOZ_MUST_USE bool init(bool isSystem);
  void destroy(JSFreeOp* fop);

  static JS::Zone* from(ZoneAllocator* zoneAlloc) {
    return static_cast<Zone*>(zoneAlloc);
  }

 private:
  enum class HelperThreadUse : uint32_t { None, Pending, Active };
  mozilla::Atomic<HelperThreadUse, mozilla::SequentiallyConsistent,
                  mozilla::recordreplay::Behavior::DontPreserve>
      helperThreadUse_;

  // The helper thread context with exclusive access to this zone, if
  // usedByHelperThread(), or nullptr when on the main thread.
  js::UnprotectedData<JSContext*> helperThreadOwnerContext_;

 public:
  bool ownedByCurrentHelperThread();
  void setHelperThreadOwnerContext(JSContext* cx);

  // Whether this zone was created for use by a helper thread.
  bool createdForHelperThread() const {
    return helperThreadUse_ != HelperThreadUse::None;
  }
  // Whether this zone is currently in use by a helper thread.
  bool usedByHelperThread() {
    MOZ_ASSERT_IF(isAtomsZone(), helperThreadUse_ == HelperThreadUse::None);
    return helperThreadUse_ == HelperThreadUse::Active;
  }
  void setCreatedForHelperThread() {
    MOZ_ASSERT(helperThreadUse_ == HelperThreadUse::None);
    helperThreadUse_ = HelperThreadUse::Pending;
  }
  void setUsedByHelperThread() {
    MOZ_ASSERT(helperThreadUse_ == HelperThreadUse::Pending);
    helperThreadUse_ = HelperThreadUse::Active;
  }
  void clearUsedByHelperThread() {
    MOZ_ASSERT(helperThreadUse_ != HelperThreadUse::None);
    helperThreadUse_ = HelperThreadUse::None;
  }

  MOZ_MUST_USE bool findSweepGroupEdges(Zone* atomsZone);

  enum ShouldDiscardBaselineCode : bool {
    KeepBaselineCode = false,
    DiscardBaselineCode
  };

  enum ShouldDiscardJitScripts : bool {
    KeepJitScripts = false,
    DiscardJitScripts
  };

  void discardJitCode(
      JSFreeOp* fop,
      ShouldDiscardBaselineCode discardBaselineCode = DiscardBaselineCode,
      ShouldDiscardJitScripts discardJitScripts = KeepJitScripts);

  void addSizeOfIncludingThis(
      mozilla::MallocSizeOf mallocSizeOf, size_t* typePool, size_t* regexpZone,
      size_t* jitZone, size_t* baselineStubsOptimized, size_t* cachedCFG,
      size_t* uniqueIdMap, size_t* shapeCaches, size_t* atomsMarkBitmaps,
      size_t* compartmentObjects, size_t* crossCompartmentWrappersTables,
      size_t* compartmentsPrivateData, size_t* scriptCountsMapArg);

  // Iterate over all cells in the zone. See the definition of ZoneCellIter
  // in gc/GC-inl.h for the possible arguments and documentation.
  template <typename T, typename... Args>
  js::gc::ZoneCellIter<T> cellIter(Args&&... args) {
    return js::gc::ZoneCellIter<T>(const_cast<Zone*>(this),
                                   std::forward<Args>(args)...);
  }

  // As above, but can return about-to-be-finalised things.
  template <typename T, typename... Args>
  js::gc::ZoneAllCellIter<T> cellIterUnsafe(Args&&... args) {
    return js::gc::ZoneAllCellIter<T>(const_cast<Zone*>(this),
                                      std::forward<Args>(args)...);
  }

  void beginSweepTypes();

  bool hasMarkedRealms();

  void scheduleGC() {
    MOZ_ASSERT(!RuntimeHeapIsBusy());
    gcScheduled_ = true;
  }
  void unscheduleGC() { gcScheduled_ = false; }
  bool isGCScheduled() { return gcScheduled_; }

  void setPreservingCode(bool preserving) { gcPreserveCode_ = preserving; }
  bool isPreservingCode() const { return gcPreserveCode_; }

  // Whether this zone can currently be collected. This doesn't take account
  // of AutoKeepAtoms for the atoms zone.
  bool canCollect();

  void changeGCState(GCState prev, GCState next) {
    MOZ_ASSERT(RuntimeHeapIsBusy());
    MOZ_ASSERT(gcState() == prev);
    MOZ_ASSERT_IF(next != NoGC, canCollect());
    gcState_ = next;
  }

  bool isCollecting() const {
    MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(runtimeFromMainThread()));
    return isCollectingFromAnyThread();
  }

  bool isCollectingFromAnyThread() const {
    if (RuntimeHeapIsCollecting()) {
      return gcState_ != NoGC;
    } else {
      return needsIncrementalBarrier();
    }
  }

  bool shouldMarkInZone() const {
    return needsIncrementalBarrier() || isGCMarking();
  }

  // Was this zone collected in the last GC.
  bool wasCollected() const { return wasCollected_; }
  void setWasCollected(bool v) { wasCollected_ = v; }

  // Get a number that is incremented whenever this zone is collected, and
  // possibly at other times too.
  uint64_t gcNumber();

  void setNeedsIncrementalBarrier(bool needs);
  const uint32_t* addressOfNeedsIncrementalBarrier() const {
    return &needsIncrementalBarrier_;
  }

  static constexpr size_t offsetOfNeedsIncrementalBarrier() {
    return offsetof(Zone, needsIncrementalBarrier_);
  }

  js::jit::JitZone* getJitZone(JSContext* cx) {
    return jitZone_ ? jitZone_ : createJitZone(cx);
  }
  js::jit::JitZone* jitZone() { return jitZone_; }

  bool isAtomsZone() const { return runtimeFromAnyThread()->isAtomsZone(this); }
  bool isSelfHostingZone() const {
    return runtimeFromAnyThread()->isSelfHostingZone(this);
  }

  void prepareForCompacting();

#ifdef DEBUG
  // If this returns true, all object tracing must be done with a GC marking
  // tracer.
  bool requireGCTracer() const;

  // For testing purposes, return the index of the sweep group which this zone
  // was swept in in the last GC.
  unsigned lastSweepGroupIndex() { return gcSweepGroupIndex; }
#endif

  void sweepAfterMinorGC(JSTracer* trc);
  void sweepUniqueIds();
  void sweepWeakMaps();
  void sweepCompartments(JSFreeOp* fop, bool keepAtleastOne, bool lastGC);

 private:
  js::jit::JitZone* createJitZone(JSContext* cx);

  bool isQueuedForBackgroundSweep() { return isOnList(); }

  // Side map for storing a unique ids for cells, independent of address.
  js::ZoneOrGCTaskData<js::gc::UniqueIdMap> uniqueIds_;

  js::gc::UniqueIdMap& uniqueIds() { return uniqueIds_.ref(); }

 public:
  void notifyObservingDebuggers();

  void clearTables();

  /*
   * When true, skip calling the metadata callback. We use this:
   * - to avoid invoking the callback recursively;
   * - to avoid observing lazy prototype setup (which confuses callbacks that
   *   want to use the types being set up!);
   * - to avoid attaching allocation stacks to allocation stack nodes, which
   *   is silly
   * And so on.
   */
  js::ZoneData<bool> suppressAllocationMetadataBuilder;

  js::gc::ArenaLists arenas;

 private:
  // Number of allocations since the most recent minor GC for this thread.
  mozilla::Atomic<uint32_t, mozilla::Relaxed,
                  mozilla::recordreplay::Behavior::DontPreserve>
      tenuredAllocsSinceMinorGC_;

 public:
  void addTenuredAllocsSinceMinorGC(uint32_t allocs) {
    tenuredAllocsSinceMinorGC_ += allocs;
  }

  uint32_t getAndResetTenuredAllocsSinceMinorGC() {
    return tenuredAllocsSinceMinorGC_.exchange(0);
  }

  js::TypeZone types;

 private:
  /* Live weakmaps in this zone. */
  js::ZoneOrGCTaskData<mozilla::LinkedList<js::WeakMapBase>> gcWeakMapList_;

 public:
  mozilla::LinkedList<js::WeakMapBase>& gcWeakMapList() {
    return gcWeakMapList_.ref();
  }

  typedef js::Vector<JS::Compartment*, 1, js::SystemAllocPolicy>
      CompartmentVector;

 private:
  // The set of compartments in this zone.
  js::MainThreadOrGCTaskData<CompartmentVector> compartments_;

  // All cross-zone string wrappers in the zone.
  js::MainThreadOrGCTaskData<js::StringWrapperMap> crossZoneStringWrappers_;

 public:
  CompartmentVector& compartments() { return compartments_.ref(); }

  js::StringWrapperMap& crossZoneStringWrappers() {
    return crossZoneStringWrappers_.ref();
  }
  const js::StringWrapperMap& crossZoneStringWrappers() const {
    return crossZoneStringWrappers_.ref();
  }

  void dropStringWrappersOnGC();

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkAllCrossCompartmentWrappersAfterMovingGC();
  void checkStringWrappersAfterMovingGC();
#endif

  void sweepAllCrossCompartmentWrappers();
  static void fixupAllCrossCompartmentWrappersAfterMovingGC(JSTracer* trc);

  // This zone's gray roots.
  using GrayRootVector =
      mozilla::SegmentedVector<js::gc::Cell*, 1024 * sizeof(js::gc::Cell*),
                               js::SystemAllocPolicy>;

 private:
  js::ZoneOrGCTaskData<GrayRootVector> gcGrayRoots_;

 public:
  GrayRootVector& gcGrayRoots() { return gcGrayRoots_.ref(); }

 private:
  // List of non-ephemeron weak containers to sweep during
  // beginSweepingSweepGroup.
  js::ZoneOrGCTaskData<mozilla::LinkedList<detail::WeakCacheBase>> weakCaches_;

 public:
  mozilla::LinkedList<detail::WeakCacheBase>& weakCaches() {
    return weakCaches_.ref();
  }
  void registerWeakCache(detail::WeakCacheBase* cachep) {
    weakCaches().insertBack(cachep);
  }

 private:
  /*
   * Mapping from not yet marked keys to a vector of all values that the key
   * maps to in any live weak map. Separate tables for nursery and tenured
   * keys.
   */
  js::ZoneOrGCTaskData<js::gc::WeakKeyTable> gcWeakKeys_;
  js::ZoneOrGCTaskData<js::gc::WeakKeyTable> gcNurseryWeakKeys_;

 public:
  js::gc::WeakKeyTable& gcWeakKeys() { return gcWeakKeys_.ref(); }
  js::gc::WeakKeyTable& gcNurseryWeakKeys() { return gcNurseryWeakKeys_.ref(); }

 private:
  void sweepWeakKeysAfterMinorGC();

 public:
  // A set of edges from this zone to other zones used during GC to calculate
  // sweep groups.
  NodeSet& gcSweepGroupEdges() {
    return gcGraphEdges;  // Defined in GraphNodeBase base class.
  }
  bool hasSweepGroupEdgeTo(Zone* otherZone) const {
    return gcGraphEdges.has(otherZone);
  }
  MOZ_MUST_USE bool addSweepGroupEdgeTo(Zone* otherZone) {
    MOZ_ASSERT(otherZone->isGCMarking());
    return gcSweepGroupEdges().put(otherZone);
  }
  void clearSweepGroupEdges() { gcSweepGroupEdges().clear(); }

  // Keep track of all TypeDescr and related objects in this compartment.
  // This is used by the GC to trace them all first when compacting, since the
  // TypedObject trace hook may access these objects.
  //
  // There are no barriers here - the set contains only tenured objects so no
  // post-barrier is required, and these are weak references so no pre-barrier
  // is required.
  using TypeDescrObjectSet =
      js::GCHashSet<JSObject*, js::MovableCellHasher<JSObject*>,
                    js::SystemAllocPolicy>;

 private:
  js::ZoneData<JS::WeakCache<TypeDescrObjectSet>> typeDescrObjects_;

  js::MainThreadData<js::UniquePtr<js::RegExpZone>> regExps_;

 public:
  js::RegExpZone& regExps() { return *regExps_.ref(); }

  JS::WeakCache<TypeDescrObjectSet>& typeDescrObjects() {
    return typeDescrObjects_.ref();
  }

  bool addTypeDescrObject(JSContext* cx, HandleObject obj);

  void keepAtoms() { keepAtomsCount++; }
  void releaseAtoms();
  bool hasKeptAtoms() const { return keepAtomsCount; }

 private:
  // Bitmap of atoms marked by this zone.
  js::ZoneOrGCTaskData<js::SparseBitmap> markedAtoms_;

  // Set of atoms recently used by this Zone. Purged on GC unless
  // keepAtomsCount is non-zero.
  js::ZoneOrGCTaskData<js::AtomSet> atomCache_;

  // Cache storing allocated external strings. Purged on GC.
  js::ZoneOrGCTaskData<js::ExternalStringCache> externalStringCache_;

  // Cache for Function.prototype.toString. Purged on GC.
  js::ZoneOrGCTaskData<js::FunctionToStringCache> functionToStringCache_;

  // Count of AutoKeepAtoms instances for this zone. When any instances exist,
  // atoms in the runtime will be marked from this zone's atom mark bitmap,
  // rather than when traced in the normal way. Threads parsing off the main
  // thread do not increment this value, but the presence of any such threads
  // also inhibits collection of atoms. We don't scan the stacks of exclusive
  // threads, so we need to avoid collecting their objects in another way. The
  // only GC thing pointers they have are to their exclusive compartment
  // (which is not collected) or to the atoms compartment. Therefore, we avoid
  // collecting the atoms zone when exclusive threads are running.
  js::ZoneOrGCTaskData<unsigned> keepAtomsCount;

  // Whether purging atoms was deferred due to keepAtoms being set. If this
  // happen then the cache will be purged when keepAtoms drops to zero.
  js::ZoneOrGCTaskData<bool> purgeAtomsDeferred;

 public:
  js::SparseBitmap& markedAtoms() { return markedAtoms_.ref(); }

  js::AtomSet& atomCache() { return atomCache_.ref(); }

  void traceAtomCache(JSTracer* trc);
  void purgeAtomCacheOrDefer();
  void purgeAtomCache();

  js::ExternalStringCache& externalStringCache() {
    return externalStringCache_.ref();
  };

  js::FunctionToStringCache& functionToStringCache() {
    return functionToStringCache_.ref();
  }

  js::ZoneData<uint32_t> tenuredStrings;
  js::ZoneData<bool> allocNurseryStrings;

 private:
  // Shared Shape property tree.
  js::ZoneData<js::PropertyTree> propertyTree_;

 public:
  js::PropertyTree& propertyTree() { return propertyTree_.ref(); }

 private:
  // Set of all unowned base shapes in the Zone.
  js::ZoneData<js::BaseShapeSet> baseShapes_;

 public:
  js::BaseShapeSet& baseShapes() { return baseShapes_.ref(); }

 private:
  // Set of initial shapes in the Zone. For certain prototypes -- namely,
  // those of various builtin classes -- there are two entries: one for a
  // lookup via TaggedProto, and one for a lookup via JSProtoKey. See
  // InitialShapeProto.
  js::ZoneData<js::InitialShapeSet> initialShapes_;

 public:
  js::InitialShapeSet& initialShapes() { return initialShapes_.ref(); }

 private:
  // List of shapes that may contain nursery pointers.
  using NurseryShapeVector =
      js::Vector<js::AccessorShape*, 0, js::SystemAllocPolicy>;
  js::ZoneData<NurseryShapeVector> nurseryShapes_;

 public:
  NurseryShapeVector& nurseryShapes() { return nurseryShapes_.ref(); }

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkInitialShapesTableAfterMovingGC();
  void checkBaseShapeTableAfterMovingGC();
#endif
  void fixupInitialShapeTable();
  void fixupAfterMovingGC();
  void fixupScriptMapsAfterMovingGC(JSTracer* trc);

  // Per-zone data for use by an embedder.
  js::ZoneData<void*> data;

  js::ZoneData<bool> isSystem;

#ifdef DEBUG
  js::MainThreadData<unsigned> gcSweepGroupIndex;
#endif

  static js::HashNumber UniqueIdToHash(uint64_t uid);

  // Creates a HashNumber based on getUniqueId. Returns false on OOM.
  MOZ_MUST_USE bool getHashCode(js::gc::Cell* cell, js::HashNumber* hashp);

  // Gets an existing UID in |uidp| if one exists.
  MOZ_MUST_USE bool maybeGetUniqueId(js::gc::Cell* cell, uint64_t* uidp);

  // Puts an existing UID in |uidp|, or creates a new UID for this Cell and
  // puts that into |uidp|. Returns false on OOM.
  MOZ_MUST_USE bool getOrCreateUniqueId(js::gc::Cell* cell, uint64_t* uidp);

  js::HashNumber getHashCodeInfallible(js::gc::Cell* cell);
  uint64_t getUniqueIdInfallible(js::gc::Cell* cell);

  // Return true if this cell has a UID associated with it.
  MOZ_MUST_USE bool hasUniqueId(js::gc::Cell* cell);

  // Transfer an id from another cell. This must only be called on behalf of a
  // moving GC. This method is infallible.
  void transferUniqueId(js::gc::Cell* tgt, js::gc::Cell* src);

  // Remove any unique id associated with this Cell.
  void removeUniqueId(js::gc::Cell* cell);

  // When finished parsing off-thread, transfer any UIDs we created in the
  // off-thread zone into the target zone.
  void adoptUniqueIds(JS::Zone* source);

#ifdef JSGC_HASH_TABLE_CHECKS
  // Assert that the UniqueId table has been redirected successfully.
  void checkUniqueIdTableAfterMovingGC();
#endif

  bool keepShapeCaches() const { return keepShapeCaches_; }
  void setKeepShapeCaches(bool b) { keepShapeCaches_ = b; }

  // Delete an empty compartment after its contents have been merged.
  void deleteEmptyCompartment(JS::Compartment* comp);

  // Non-zero if the storage underlying any typed object in this zone might
  // be detached. This is stored in Zone because IC stubs bake in a pointer
  // to this field and Baseline IC code is shared across realms within a
  // Zone. Furthermore, it's not entirely clear if this flag is ever set to
  // a non-zero value since bug 1458011.
  uint32_t detachedTypedObjects = 0;

 private:
  js::ZoneOrGCTaskData<js::jit::JitZone*> jitZone_;

  js::MainThreadData<bool> gcScheduled_;
  js::MainThreadData<bool> gcScheduledSaved_;
  js::MainThreadData<bool> gcPreserveCode_;
  js::ZoneData<bool> keepShapeCaches_;
  js::MainThreadData<bool> wasCollected_;

  // Allow zones to be linked into a list
  friend class js::gc::ZoneList;
  static Zone* const NotOnList;
  js::MainThreadOrGCTaskData<Zone*> listNext_;
  bool isOnList() const;
  Zone* nextZone() const;

  friend bool js::CurrentThreadCanAccessZone(Zone* zone);
  friend class js::gc::GCRuntime;

 public:
  // Script side-tables. These used to be held by Realm, but are now placed
  // here in order to allow JSScript to access them during finalize (see bug
  // 1568245; this change in 1575350). The tables are initialized lazily by
  // JSScript.
  js::UniquePtr<js::ScriptCountsMap> scriptCountsMap;
  js::UniquePtr<js::ScriptLCovMap> scriptLCovMap;
  js::UniquePtr<js::DebugScriptMap> debugScriptMap;
#ifdef MOZ_VTUNE
  js::UniquePtr<js::ScriptVTuneIdMap> scriptVTuneIdMap;
#endif

  void traceScriptTableRoots(JSTracer* trc);

  void clearScriptCounts(Realm* realm);
  void clearScriptLCov(Realm* realm);

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkScriptMapsAfterMovingGC();
#endif
};

}  // namespace JS

namespace js {
namespace gc {
const char* StateName(JS::Zone::GCState state);
}  // namespace gc
}  // namespace js

#endif  // gc_Zone_h
