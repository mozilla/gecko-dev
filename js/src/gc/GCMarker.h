/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCMarker_h
#define gc_GCMarker_h

#include "mozilla/Maybe.h"

#include "ds/OrderedHashTable.h"
#include "js/SliceBudget.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"

namespace js {

class AutoAccessAtomsZone;
class WeakMapBase;

static const size_t NON_INCREMENTAL_MARK_STACK_BASE_CAPACITY = 4096;
static const size_t INCREMENTAL_MARK_STACK_BASE_CAPACITY = 32768;

namespace gc {

struct Cell;

struct WeakKeyTableHashPolicy {
  using Lookup = Cell*;
  static HashNumber hash(const Lookup& v,
                         const mozilla::HashCodeScrambler& hcs) {
    return hcs.scramble(mozilla::HashGeneric(v));
  }
  static bool match(Cell* const& k, const Lookup& l) { return k == l; }
  static bool isEmpty(Cell* const& v) { return !v; }
  static void makeEmpty(Cell** vp) { *vp = nullptr; }
};

struct WeakMarkable {
  WeakMapBase* weakmap;
  Cell* key;

  WeakMarkable(WeakMapBase* weakmapArg, Cell* keyArg)
      : weakmap(weakmapArg), key(keyArg) {}
};

using WeakEntryVector = Vector<WeakMarkable, 2, js::SystemAllocPolicy>;

using WeakKeyTable =
    OrderedHashMap<Cell*, WeakEntryVector, WeakKeyTableHashPolicy,
                   js::SystemAllocPolicy>;

/*
 * When the mark stack is full, the GC does not call js::TraceChildren to mark
 * the reachable "children" of the thing. Rather the thing is put aside and
 * js::TraceChildren is called later when the mark stack is empty.
 *
 * To implement such delayed marking of the children with minimal overhead for
 * the normal case of sufficient stack, we link arenas into a list using
 * Arena::setNextDelayedMarkingArena(). The head of the list is stored in
 * GCMarker::delayedMarkingList. GCMarker::delayMarkingChildren() adds arenas
 * to the list as necessary while markAllDelayedChildren() pops the arenas from
 * the stack until it is empty.
 */
class MarkStack {
 public:
  /*
   * We use a common mark stack to mark GC things of different types and use
   * the explicit tags to distinguish them when it cannot be deduced from
   * the context of push or pop operation.
   */
  enum Tag {
    ValueArrayTag,
    ObjectTag,
    GroupTag,
    SavedValueArrayTag,
    JitCodeTag,
    ScriptTag,
    TempRopeTag,

    LastTag = TempRopeTag
  };

  static const uintptr_t TagMask = 7;
  static_assert(TagMask >= uintptr_t(LastTag),
                "The tag mask must subsume the tags.");
  static_assert(TagMask <= gc::CellAlignMask,
                "The tag mask must be embeddable in a Cell*.");

  class TaggedPtr {
    uintptr_t bits;

    Cell* ptr() const;

   public:
    TaggedPtr() {}
    TaggedPtr(Tag tag, Cell* ptr);
    Tag tag() const;
    template <typename T>
    T* as() const;

    JSObject* asValueArrayObject() const;
    JSObject* asSavedValueArrayObject() const;
    JSRope* asTempRope() const;

    void assertValid() const;
  };

  struct ValueArray {
    ValueArray(JSObject* obj, HeapSlot* start, HeapSlot* end);
    void assertValid() const;

    HeapSlot* end;
    HeapSlot* start;
    TaggedPtr ptr;
  };

  struct SavedValueArray {
    SavedValueArray(JSObject* obj, size_t index, HeapSlot::Kind kind);
    void assertValid() const;

    uintptr_t kind;
    uintptr_t index;
    TaggedPtr ptr;
  };

  explicit MarkStack(size_t maxCapacity = DefaultCapacity);
  ~MarkStack();

  static const size_t DefaultCapacity = SIZE_MAX;

  // The unit for MarkStack::capacity() is mark stack entries.
  size_t capacity() { return stack().length(); }

  size_t position() const { return topIndex_; }

  MOZ_MUST_USE bool init(JSGCMode gcMode);

  MOZ_MUST_USE bool setCapacityForMode(JSGCMode mode);

  size_t maxCapacity() const { return maxCapacity_; }
  void setMaxCapacity(size_t maxCapacity);

  template <typename T>
  MOZ_MUST_USE bool push(T* ptr);

  MOZ_MUST_USE bool push(JSObject* obj, HeapSlot* start, HeapSlot* end);
  MOZ_MUST_USE bool push(const ValueArray& array);
  MOZ_MUST_USE bool push(const SavedValueArray& array);

  // GCMarker::eagerlyMarkChildren uses unused marking stack as temporary
  // storage to hold rope pointers.
  MOZ_MUST_USE bool pushTempRope(JSRope* ptr);

  bool isEmpty() const { return topIndex_ == 0; }

  Tag peekTag() const;
  TaggedPtr popPtr();
  ValueArray popValueArray();
  SavedValueArray popSavedValueArray();

  void clear() { topIndex_ = 0; }

  void setGCMode(JSGCMode gcMode);

  void poisonUnused();

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  using StackVector = Vector<TaggedPtr, 0, SystemAllocPolicy>;
  const StackVector& stack() const { return stack_.ref(); }
  StackVector& stack() { return stack_.ref(); }

  MOZ_MUST_USE bool ensureSpace(size_t count);

  /* Grow the stack, ensuring there is space for at least count elements. */
  MOZ_MUST_USE bool enlarge(size_t count);

  MOZ_MUST_USE bool resize(size_t newCapacity);

  TaggedPtr* topPtr();

  const TaggedPtr& peekPtr() const;
  MOZ_MUST_USE bool pushTaggedPtr(Tag tag, Cell* ptr);

  // Index of the top of the stack.
  MainThreadOrGCTaskData<size_t> topIndex_;

  // The maximum stack capacity to grow to.
  MainThreadOrGCTaskData<size_t> maxCapacity_;

  // Vector containing allocated stack memory. Unused beyond topIndex_.
  MainThreadOrGCTaskData<StackVector> stack_;

#ifdef DEBUG
  mutable size_t iteratorCount_;
#endif

  friend class MarkStackIter;
};

class MarkStackIter {
  MarkStack& stack_;
  size_t pos_;

 public:
  explicit MarkStackIter(MarkStack& stack);
  ~MarkStackIter();

  bool done() const;
  MarkStack::Tag peekTag() const;
  MarkStack::TaggedPtr peekPtr() const;
  MarkStack::ValueArray peekValueArray() const;
  void next();
  void nextPtr();
  void nextArray();

  // Mutate the current ValueArray to a SavedValueArray.
  void saveValueArray(const MarkStack::SavedValueArray& savedArray);

 private:
  size_t position() const;
};

} /* namespace gc */

class GCMarker : public JSTracer {
 public:
  explicit GCMarker(JSRuntime* rt);
  MOZ_MUST_USE bool init(JSGCMode gcMode);

  void setMaxCapacity(size_t maxCap) { stack.setMaxCapacity(maxCap); }
  size_t maxCapacity() const { return stack.maxCapacity(); }

  void start();
  void stop();
  void reset();

  // Mark the given GC thing and traverse its children at some point.
  template <typename T>
  void traverse(T thing);

  // Calls traverse on target after making additional assertions.
  template <typename S, typename T>
  void traverseEdge(S source, T* target);
  template <typename S, typename T>
  void traverseEdge(S source, const T& target);

  // Helper methods that coerce their second argument to the base pointer
  // type.
  template <typename S>
  void traverseObjectEdge(S source, JSObject* target) {
    traverseEdge(source, target);
  }
  template <typename S>
  void traverseStringEdge(S source, JSString* target) {
    traverseEdge(source, target);
  }

  /*
   * Care must be taken changing the mark color from gray to black. The cycle
   * collector depends on the invariant that there are no black to gray edges
   * in the GC heap. This invariant lets the CC not trace through black
   * objects. If this invariant is violated, the cycle collector may free
   * objects that are still reachable.
   */
  void setMarkColorGray();
  void setMarkColorBlack();
  void setMarkColor(gc::MarkColor newColor);
  gc::MarkColor markColor() const { return color; }

  // Return whether a cell is marked relative to the current marking color. If
  // the cell is black then this returns true, but if it's gray it will return
  // false if the mark color is black.
  template <typename T>
  bool isMarked(T* thingp) {
    return color == gc::MarkColor::Black ? gc::IsMarkedBlack(runtime(), thingp)
                                         : gc::IsMarked(runtime(), thingp);
  }
  template <typename T>
  bool isMarkedUnbarriered(T* thingp) {
    return color == gc::MarkColor::Black
               ? gc::IsMarkedBlackUnbarriered(runtime(), thingp)
               : gc::IsMarkedUnbarriered(runtime(), thingp);
  }

  void enterWeakMarkingMode();
  void leaveWeakMarkingMode();
  void abortLinearWeakMarking() {
    leaveWeakMarkingMode();
    linearWeakMarkingDisabled_ = true;
  }

  void delayMarkingChildren(gc::Cell* cell);

  bool isDrained() { return isMarkStackEmpty() && !delayedMarkingList; }

  // The mark queue is a testing-only feature for controlling mark ordering and
  // yield timing.
  enum MarkQueueProgress {
    QueueYielded,   // End this incremental GC slice, if possible
    QueueComplete,  // Done with the queue
    QueueSuspended  // Continue the GC without ending the slice
  };
  MarkQueueProgress processMarkQueue();

  MOZ_MUST_USE bool markUntilBudgetExhausted(SliceBudget& budget);

  void setGCMode(JSGCMode mode) { stack.setGCMode(mode); }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

#ifdef DEBUG
  bool shouldCheckCompartments() { return strictCompartmentChecking; }
#endif

  void markEphemeronValues(gc::Cell* markedCell, gc::WeakEntryVector& entry);

  size_t getMarkCount() const { return markCount; }
  void clearMarkCount() { markCount = 0; }

  static GCMarker* fromTracer(JSTracer* trc) {
    MOZ_ASSERT(trc->isMarkingTracer());
    return static_cast<GCMarker*>(trc);
  }

  template <typename T>
  void markImplicitEdges(T* oldThing);

 private:
#ifdef DEBUG
  void checkZone(void* p);
#else
  void checkZone(void* p) {}
#endif

  // Push an object onto the stack for later tracing and assert that it has
  // already been marked.
  inline void repush(JSObject* obj);

  template <typename T>
  void markAndTraceChildren(T* thing);
  template <typename T>
  void markAndPush(T* thing);
  template <typename T>
  void markAndScan(T* thing);
  template <typename T>
  void markImplicitEdgesHelper(T oldThing);
  void eagerlyMarkChildren(JSLinearString* str);
  void eagerlyMarkChildren(JSRope* rope);
  void eagerlyMarkChildren(JSString* str);
  void eagerlyMarkChildren(LazyScript* thing);
  void eagerlyMarkChildren(Shape* shape);
  void eagerlyMarkChildren(Scope* scope);
  void lazilyMarkChildren(ObjectGroup* group);

  // We may not have concrete types yet, so this has to be outside the header.
  template <typename T>
  void dispatchToTraceChildren(T* thing);

  // Mark the given GC thing, but do not trace its children. Return true
  // if the thing became marked.
  template <typename T>
  MOZ_MUST_USE bool mark(T* thing);

  template <typename T>
  inline void pushTaggedPtr(T* ptr);

  inline void pushValueArray(JSObject* obj, HeapSlot* start, HeapSlot* end);

  bool isMarkStackEmpty() { return stack.isEmpty(); }

  bool hasBlackEntries() const { return stack.position() > grayPosition; }

  bool hasGrayEntries() const { return grayPosition > 0 && !stack.isEmpty(); }

  MOZ_MUST_USE bool restoreValueArray(
      const gc::MarkStack::SavedValueArray& array, HeapSlot** vpp,
      HeapSlot** endp);
  gc::MarkStack::ValueArray restoreValueArray(
      const gc::MarkStack::SavedValueArray& savedArray);

  void saveValueRanges();
  gc::MarkStack::SavedValueArray saveValueRange(
      const gc::MarkStack::ValueArray& array);

  inline void processMarkStackTop(SliceBudget& budget);

  void markDelayedChildren(gc::Arena* arena, gc::MarkColor color);
  MOZ_MUST_USE bool markAllDelayedChildren(SliceBudget& budget);
  bool processDelayedMarkingList(gc::MarkColor color, SliceBudget& budget);
  bool hasDelayedChildren() const { return !!delayedMarkingList; }
  void rebuildDelayedMarkingList();
  void appendToDelayedMarkingList(gc::Arena** listTail, gc::Arena* arena);

  template <typename F>
  void forEachDelayedMarkingArena(F&& f);

  /* The mark stack. Pointers in this stack are "gray" in the GC sense. */
  gc::MarkStack stack;

  /* Stack entries at positions below this are considered gray. */
  MainThreadOrGCTaskData<size_t> grayPosition;

  /* The color is only applied to objects and functions. */
  MainThreadOrGCTaskData<gc::MarkColor> color;

  /* Pointer to the top of the stack of arenas we are delaying marking on. */
  MainThreadOrGCTaskData<js::gc::Arena*> delayedMarkingList;

  /* Whether more work has been added to the delayed marking list. */
  MainThreadData<bool> delayedMarkingWorkAdded;

  /*
   * If the weakKeys table OOMs, disable the linear algorithm and fall back
   * to iterating until the next GC.
   */
  MainThreadData<bool> linearWeakMarkingDisabled_;

  /* The count of marked objects during GC. */
  size_t markCount;

#ifdef DEBUG
  /* Count of arenas that are currently in the stack. */
  MainThreadData<size_t> markLaterArenas;

  /* Assert that start and stop are called with correct ordering. */
  MainThreadOrGCTaskData<bool> started;

  /* The test marking queue might want to be marking a particular color. */
  mozilla::Maybe<js::gc::MarkColor> queueMarkColor;

  /*
   * If this is true, all marked objects must belong to a compartment being
   * GCed. This is used to look for compartment bugs.
   */
  MainThreadOrGCTaskData<bool> strictCompartmentChecking;

 public:
  /*
   * List of objects to mark at the beginning of a GC. May also contains string
   * directives to change mark color or wait until different phases of the GC.
   *
   * This is a WeakCache because not everything in this list is guaranteed to
   * end up marked (eg if you insert an object from an already-processed sweep
   * group in the middle of an incremental GC). Also, the mark queue is not
   * used during shutdown GCs. In either case, unmarked objects may need to be
   * discarded.
   */

  JS::WeakCache<GCVector<JS::Heap<JS::Value>, 0, SystemAllocPolicy>> markQueue;

  /* Position within the test mark queue. */
  size_t queuePos;
#endif  // DEBUG
};

namespace gc {

/*
 * Temporarily change the mark color while this class is on the stack.
 *
 * During incremental sweeping this also transitions zones in the
 * current sweep group into the Mark or MarkGray state as appropriate.
 */
class MOZ_RAII AutoSetMarkColor {
  GCMarker& marker_;
  MarkColor initialColor_;

 public:
  AutoSetMarkColor(GCMarker& marker, MarkColor newColor)
      : marker_(marker), initialColor_(marker.markColor()) {
    marker_.setMarkColor(newColor);
  }

  ~AutoSetMarkColor() { marker_.setMarkColor(initialColor_); }
};

} /* namespace gc */

} /* namespace js */

// Exported for Tracer.cpp
inline bool ThingIsPermanentAtomOrWellKnownSymbol(js::gc::Cell* thing) {
  return false;
}
bool ThingIsPermanentAtomOrWellKnownSymbol(JSString*);
bool ThingIsPermanentAtomOrWellKnownSymbol(JSLinearString*);
bool ThingIsPermanentAtomOrWellKnownSymbol(JSAtom*);
bool ThingIsPermanentAtomOrWellKnownSymbol(js::PropertyName*);
bool ThingIsPermanentAtomOrWellKnownSymbol(JS::Symbol*);

#endif /* gc_GCMarker_h */
