/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Cell_h
#define gc_Cell_h

#include "gc/GCEnum.h"
#include "gc/Heap.h"
#include "js/GCAnnotations.h"
#include "js/TraceKind.h"
#include "js/TypeDecls.h"

namespace JS {

namespace shadow {
struct Zone;
} /* namespace shadow */

enum class TraceKind;
} /* namespace JS */

namespace js {

class GenericPrinter;

extern bool RuntimeFromMainThreadIsHeapMajorCollecting(
    JS::shadow::Zone* shadowZone);

#ifdef DEBUG

// Barriers can't be triggered during backend Ion compilation, which may run on
// a helper thread.
extern bool CurrentThreadIsIonCompiling();
#endif

extern void TraceManuallyBarrieredGenericPointerEdge(JSTracer* trc,
                                                     gc::Cell** thingp,
                                                     const char* name);

namespace gc {

class Arena;
enum class AllocKind : uint8_t;
struct Chunk;
class StoreBuffer;
class TenuredCell;

// [SMDOC] GC Cell
//
// A GC cell is the base class for all GC things. All types allocated on the GC
// heap extend either gc::Cell or gc::TenuredCell. If a type is always tenured,
// prefer the TenuredCell class as base.
//
// The first word (a pointer or uintptr_t) of each Cell must reserve the low
// Cell::ReservedBits bits for GC purposes. The remaining bits are available to
// sub-classes and typically store a pointer to another gc::Cell.
//
// During moving GC operation a Cell may be marked as forwarded. This indicates
// that a gc::RelocationOverlay is currently stored in the Cell's memory and
// should be used to find the new location of the Cell.
struct alignas(gc::CellAlignBytes) Cell {
 public:
  // The low bits of the first word of each Cell are reserved for GC flags.
  static constexpr int ReservedBits = 2;
  static constexpr uintptr_t RESERVED_MASK = JS_BITMASK(ReservedBits);

  // Indicates if the cell is currently a RelocationOverlay
  static constexpr uintptr_t FORWARD_BIT = JS_BIT(0);

  // When a Cell is in the nursery, this will indicate if it is a JSString (1)
  // or JSObject (0). When not in nursery, this bit is still reserved for
  // JSString to use as JSString::NON_ATOM bit. This may be removed by Bug
  // 1376646.
  static constexpr uintptr_t JSSTRING_BIT = JS_BIT(1);

  MOZ_ALWAYS_INLINE bool isTenured() const { return !IsInsideNursery(this); }
  MOZ_ALWAYS_INLINE const TenuredCell& asTenured() const;
  MOZ_ALWAYS_INLINE TenuredCell& asTenured();

  MOZ_ALWAYS_INLINE bool isMarkedAny() const;
  MOZ_ALWAYS_INLINE bool isMarkedBlack() const;
  MOZ_ALWAYS_INLINE bool isMarkedGray() const;

  inline JSRuntime* runtimeFromMainThread() const;

  // Note: Unrestricted access to the runtime of a GC thing from an arbitrary
  // thread can easily lead to races. Use this method very carefully.
  inline JSRuntime* runtimeFromAnyThread() const;

  // May be overridden by GC thing kinds that have a compartment pointer.
  inline JS::Compartment* maybeCompartment() const { return nullptr; }

  // The StoreBuffer used to record incoming pointers from the tenured heap.
  // This will return nullptr for a tenured cell.
  inline StoreBuffer* storeBuffer() const;

  inline JS::TraceKind getTraceKind() const;

  static MOZ_ALWAYS_INLINE bool needWriteBarrierPre(JS::Zone* zone);

  inline bool isForwarded() const {
    uintptr_t firstWord = *reinterpret_cast<const uintptr_t*>(this);
    return firstWord & FORWARD_BIT;
  }

  inline bool nurseryCellIsString() const {
    MOZ_ASSERT(!isTenured());
    uintptr_t firstWord = *reinterpret_cast<const uintptr_t*>(this);
    return firstWord & JSSTRING_BIT;
  }

  template <class T>
  inline bool is() const {
    return getTraceKind() == JS::MapTypeToTraceKind<T>::kind;
  }

  template <class T>
  inline T* as() {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<T*>(this);
  }

  template <class T>
  inline const T* as() const {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<const T*>(this);
  }

#ifdef DEBUG
  static inline bool thingIsNotGray(Cell* cell);
  inline bool isAligned() const;
  void dump(GenericPrinter& out) const;
  void dump() const;
#endif

 protected:
  uintptr_t address() const;
  inline Chunk* chunk() const;
} JS_HAZ_GC_THING;

// A GC TenuredCell gets behaviors that are valid for things in the Tenured
// heap, such as access to the arena and mark bits.
class TenuredCell : public Cell {
 public:
  // Construct a TenuredCell from a void*, making various sanity assertions.
  static MOZ_ALWAYS_INLINE TenuredCell* fromPointer(void* ptr);
  static MOZ_ALWAYS_INLINE const TenuredCell* fromPointer(const void* ptr);

  // Mark bit management.
  MOZ_ALWAYS_INLINE bool isMarkedAny() const;
  MOZ_ALWAYS_INLINE bool isMarkedBlack() const;
  MOZ_ALWAYS_INLINE bool isMarkedGray() const;

  // The return value indicates if the cell went from unmarked to marked.
  MOZ_ALWAYS_INLINE bool markIfUnmarked(
      MarkColor color = MarkColor::Black) const;
  MOZ_ALWAYS_INLINE void markBlack() const;
  MOZ_ALWAYS_INLINE void copyMarkBitsFrom(const TenuredCell* src);
  MOZ_ALWAYS_INLINE void unmark();

  // Access to the arena.
  inline Arena* arena() const;
  inline AllocKind getAllocKind() const;
  inline JS::TraceKind getTraceKind() const;
  inline JS::Zone* zone() const;
  inline JS::Zone* zoneFromAnyThread() const;
  inline bool isInsideZone(JS::Zone* zone) const;

  MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZone() const {
    return JS::shadow::Zone::asShadowZone(zone());
  }
  MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZoneFromAnyThread() const {
    return JS::shadow::Zone::asShadowZone(zoneFromAnyThread());
  }

  template <class T>
  inline bool is() const {
    return getTraceKind() == JS::MapTypeToTraceKind<T>::kind;
  }

  template <class T>
  inline T* as() {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<T*>(this);
  }

  template <class T>
  inline const T* as() const {
    // |this|-qualify the |is| call below to avoid compile errors with even
    // fairly recent versions of gcc, e.g. 7.1.1 according to bz.
    MOZ_ASSERT(this->is<T>());
    return static_cast<const T*>(this);
  }

  static MOZ_ALWAYS_INLINE void readBarrier(TenuredCell* thing);
  static MOZ_ALWAYS_INLINE void writeBarrierPre(TenuredCell* thing);

  static void MOZ_ALWAYS_INLINE writeBarrierPost(void* cellp,
                                                 TenuredCell* prior,
                                                 TenuredCell* next);

  // Default implementation for kinds that don't require fixup.
  void fixupAfterMovingGC() {}

#ifdef DEBUG
  inline bool isAligned() const;
#endif
};

MOZ_ALWAYS_INLINE const TenuredCell& Cell::asTenured() const {
  MOZ_ASSERT(isTenured());
  return *static_cast<const TenuredCell*>(this);
}

MOZ_ALWAYS_INLINE TenuredCell& Cell::asTenured() {
  MOZ_ASSERT(isTenured());
  return *static_cast<TenuredCell*>(this);
}

MOZ_ALWAYS_INLINE bool Cell::isMarkedAny() const {
  return !isTenured() || asTenured().isMarkedAny();
}

MOZ_ALWAYS_INLINE bool Cell::isMarkedBlack() const {
  return !isTenured() || asTenured().isMarkedBlack();
}

MOZ_ALWAYS_INLINE bool Cell::isMarkedGray() const {
  return isTenured() && asTenured().isMarkedGray();
}

inline JSRuntime* Cell::runtimeFromMainThread() const {
  JSRuntime* rt = chunk()->trailer.runtime;
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
  return rt;
}

inline JSRuntime* Cell::runtimeFromAnyThread() const {
  return chunk()->trailer.runtime;
}

inline uintptr_t Cell::address() const {
  uintptr_t addr = uintptr_t(this);
  MOZ_ASSERT(addr % CellAlignBytes == 0);
  MOZ_ASSERT(Chunk::withinValidRange(addr));
  return addr;
}

Chunk* Cell::chunk() const {
  uintptr_t addr = uintptr_t(this);
  MOZ_ASSERT(addr % CellAlignBytes == 0);
  addr &= ~ChunkMask;
  return reinterpret_cast<Chunk*>(addr);
}

inline StoreBuffer* Cell::storeBuffer() const {
  return chunk()->trailer.storeBuffer;
}

inline JS::TraceKind Cell::getTraceKind() const {
  if (isTenured()) {
    return asTenured().getTraceKind();
  }
  if (nurseryCellIsString()) {
    return JS::TraceKind::String;
  }
  return JS::TraceKind::Object;
}

/* static */ MOZ_ALWAYS_INLINE bool Cell::needWriteBarrierPre(JS::Zone* zone) {
  return JS::shadow::Zone::asShadowZone(zone)->needsIncrementalBarrier();
}

/* static */ MOZ_ALWAYS_INLINE TenuredCell* TenuredCell::fromPointer(
    void* ptr) {
  MOZ_ASSERT(static_cast<TenuredCell*>(ptr)->isTenured());
  return static_cast<TenuredCell*>(ptr);
}

/* static */ MOZ_ALWAYS_INLINE const TenuredCell* TenuredCell::fromPointer(
    const void* ptr) {
  MOZ_ASSERT(static_cast<const TenuredCell*>(ptr)->isTenured());
  return static_cast<const TenuredCell*>(ptr);
}

bool TenuredCell::isMarkedAny() const {
  MOZ_ASSERT(arena()->allocated());
  return chunk()->bitmap.isMarkedAny(this);
}

bool TenuredCell::isMarkedBlack() const {
  MOZ_ASSERT(arena()->allocated());
  return chunk()->bitmap.isMarkedBlack(this);
}

bool TenuredCell::isMarkedGray() const {
  MOZ_ASSERT(arena()->allocated());
  return chunk()->bitmap.isMarkedGray(this);
}

bool TenuredCell::markIfUnmarked(MarkColor color /* = Black */) const {
  return chunk()->bitmap.markIfUnmarked(this, color);
}

void TenuredCell::markBlack() const { chunk()->bitmap.markBlack(this); }

void TenuredCell::copyMarkBitsFrom(const TenuredCell* src) {
  ChunkBitmap& bitmap = chunk()->bitmap;
  bitmap.copyMarkBit(this, src, ColorBit::BlackBit);
  bitmap.copyMarkBit(this, src, ColorBit::GrayOrBlackBit);
}

void TenuredCell::unmark() { chunk()->bitmap.unmark(this); }

inline Arena* TenuredCell::arena() const {
  MOZ_ASSERT(isTenured());
  uintptr_t addr = address();
  addr &= ~ArenaMask;
  return reinterpret_cast<Arena*>(addr);
}

AllocKind TenuredCell::getAllocKind() const { return arena()->getAllocKind(); }

JS::TraceKind TenuredCell::getTraceKind() const {
  return MapAllocToTraceKind(getAllocKind());
}

JS::Zone* TenuredCell::zone() const {
  JS::Zone* zone = arena()->zone;
  MOZ_ASSERT(CurrentThreadCanAccessZone(zone));
  return zone;
}

JS::Zone* TenuredCell::zoneFromAnyThread() const { return arena()->zone; }

bool TenuredCell::isInsideZone(JS::Zone* zone) const {
  return zone == arena()->zone;
}

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::readBarrier(
    TenuredCell* thing) {
  MOZ_ASSERT(!CurrentThreadIsIonCompiling());
  MOZ_ASSERT(thing);
  MOZ_ASSERT(CurrentThreadCanAccessZone(thing->zoneFromAnyThread()));

  // It would be good if barriers were never triggered during collection, but
  // at the moment this can happen e.g. when rekeying tables containing
  // read-barriered GC things after a moving GC.
  //
  // TODO: Fix this and assert we're not collecting if we're on the active
  // thread.

  JS::shadow::Zone* shadowZone = thing->shadowZoneFromAnyThread();
  if (shadowZone->needsIncrementalBarrier()) {
    // Barriers are only enabled on the main thread and are disabled while
    // collecting.
    MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(shadowZone));
    Cell* tmp = thing;
    TraceManuallyBarrieredGenericPointerEdge(shadowZone->barrierTracer(), &tmp,
                                             "read barrier");
    MOZ_ASSERT(tmp == thing);
  }

  if (thing->isMarkedGray()) {
    // There shouldn't be anything marked grey unless we're on the main thread.
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(thing->runtimeFromAnyThread()));
    if (!JS::RuntimeHeapIsCollecting()) {
      JS::UnmarkGrayGCThingRecursively(
          JS::GCCellPtr(thing, thing->getTraceKind()));
    }
  }
}

void AssertSafeToSkipBarrier(TenuredCell* thing);

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::writeBarrierPre(
    TenuredCell* thing) {
  MOZ_ASSERT(!CurrentThreadIsIonCompiling());
  if (!thing) {
    return;
  }

#ifdef JS_GC_ZEAL
  // When verifying pre barriers we need to switch on all barriers, even
  // those on the Atoms Zone. Normally, we never enter a parse task when
  // collecting in the atoms zone, so will filter out atoms below.
  // Unfortuantely, If we try that when verifying pre-barriers, we'd never be
  // able to handle off thread parse tasks at all as we switch on the verifier
  // any time we're not doing GC. This would cause us to deadlock, as off thread
  // parsing is meant to resume after GC work completes. Instead we filter out
  // any off thread barriers that reach us and assert that they would normally
  // not be possible.
  if (!CurrentThreadCanAccessRuntime(thing->runtimeFromAnyThread())) {
    AssertSafeToSkipBarrier(thing);
    return;
  }
#endif

  JS::shadow::Zone* shadowZone = thing->shadowZoneFromAnyThread();
  if (shadowZone->needsIncrementalBarrier()) {
    MOZ_ASSERT(!RuntimeFromMainThreadIsHeapMajorCollecting(shadowZone));
    Cell* tmp = thing;
    TraceManuallyBarrieredGenericPointerEdge(shadowZone->barrierTracer(), &tmp,
                                             "pre barrier");
    MOZ_ASSERT(tmp == thing);
  }
}

static MOZ_ALWAYS_INLINE void AssertValidToSkipBarrier(TenuredCell* thing) {
  MOZ_ASSERT(!IsInsideNursery(thing));
  MOZ_ASSERT_IF(
      thing,
      MapAllocToTraceKind(thing->getAllocKind()) != JS::TraceKind::Object &&
          MapAllocToTraceKind(thing->getAllocKind()) != JS::TraceKind::String);
}

/* static */ MOZ_ALWAYS_INLINE void TenuredCell::writeBarrierPost(
    void* cellp, TenuredCell* prior, TenuredCell* next) {
  AssertValidToSkipBarrier(next);
}

#ifdef DEBUG

/* static */ bool Cell::thingIsNotGray(Cell* cell) {
  return JS::CellIsNotGray(cell);
}

bool Cell::isAligned() const {
  if (!isTenured()) {
    return true;
  }
  return asTenured().isAligned();
}

bool TenuredCell::isAligned() const {
  return Arena::isAligned(address(), arena()->getThingSize());
}

#endif

} /* namespace gc */
} /* namespace js */

#endif /* gc_Cell_h */
