/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_OrderedHashTableObject_h
#define builtin_OrderedHashTableObject_h

/*
 * This file defines js::OrderedHashMapObject (a base class of js::MapObject)
 * and js::OrderedHashSetObject (a base class of js::SetObject).
 *
 * It also defines two templates, js::OrderedHashMapImpl and
 * js::OrderedHashSetImpl, that operate on these objects and implement the
 * ordered hash table algorithm. These templates are defined separately from
 * the JS object types because it lets us switch between different template
 * instantiations to enable or disable GC barriers.
 *
 * The implemented hash table algorithm is also different from HashMap and
 * HashSet:
 *
 *   - Iterating over an Ordered hash table visits the entries in the order in
 *     which they were inserted. This means that unlike a HashMap, the behavior
 *     of an OrderedHashMapImpl is deterministic (as long as the HashPolicy
 *     methods are effect-free and consistent); the hashing is a pure
 *     performance optimization.
 *
 *   - Range objects over Ordered tables remain valid even when entries are
 *     added or removed or the table is resized. (However in the case of
 *     removing entries, note the warning on class Range below.)
 *
 *   - The API is a little different, so it's not a drop-in replacement.
 *     In particular, the hash policy is a little different.
 *     Also, the Ordered templates lack the Ptr and AddPtr types.
 *
 * Hash policies
 *
 * See the comment about "Hash policy" in HashTable.h for general features that
 * hash policy classes must provide. Hash policies for OrderedHashMapImpl and
 * Sets differ in that the hash() method takes an extra argument:
 *
 *     static js::HashNumber hash(Lookup, const HashCodeScrambler&);
 *
 * They must additionally provide a distinguished "empty" key value and the
 * following static member functions:
 *
 *     bool isEmpty(const Key&);
 *     void makeEmpty(Key*);
 */

#include "mozilla/CheckedInt.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TemplateLib.h"

#include <memory>
#include <tuple>
#include <utility>

#include "gc/Barrier.h"
#include "gc/Zone.h"
#include "js/GCPolicyAPI.h"
#include "js/HashTable.h"
#include "vm/Runtime.h"

class JSTracer;

namespace js {

namespace detail {

template <class T, class Ops>
class OrderedHashTableImpl;

// Base class for OrderedHashMapObject and OrderedHashSetObject.
class OrderedHashTableObject : public NativeObject {
  // Use a friend class to avoid exposing these slot definitions directly to
  // MapObject and SetObject.
  template <class T, class Ops>
  friend class OrderedHashTableImpl;

  enum Slots {
    HashTableSlot,
    DataSlot,
    DataLengthSlot,
    DataCapacitySlot,
    LiveCountSlot,
    HashShiftSlot,
    RangesSlot,
    NurseryRangesSlot,
    HashCodeScramblerSlot,
    SlotCount
  };

 public:
  static constexpr size_t offsetOfDataLength() {
    return getFixedSlotOffset(DataLengthSlot);
  }
  static constexpr size_t offsetOfData() {
    return getFixedSlotOffset(DataSlot);
  }
  static constexpr size_t offsetOfHashTable() {
    return getFixedSlotOffset(HashTableSlot);
  }
  static constexpr size_t offsetOfHashShift() {
    return getFixedSlotOffset(HashShiftSlot);
  }
  static constexpr size_t offsetOfLiveCount() {
    return getFixedSlotOffset(LiveCountSlot);
  }
  static constexpr size_t offsetOfHashCodeScrambler() {
    return getFixedSlotOffset(HashCodeScramblerSlot);
  }
};

/*
 * detail::OrderedHashTableImpl is the underlying code used to implement both
 * OrderedHashMapImpl and OrderedHashSetImpl. Programs should use one of those
 * two templates rather than OrderedHashTableImpl.
 */
template <class T, class Ops>
class MOZ_STACK_CLASS OrderedHashTableImpl {
 public:
  using Key = typename Ops::KeyType;
  using Lookup = typename Ops::Lookup;
  using HashCodeScrambler = mozilla::HashCodeScrambler;
  static constexpr size_t SlotCount = OrderedHashTableObject::SlotCount;

  struct Data {
    T element;
    Data* chain;

    Data(const T& e, Data* c) : element(e), chain(c) {}
    Data(T&& e, Data* c) : element(std::move(e)), chain(c) {}
  };

  class Range;
  friend class Range;

 private:
  using Slots = OrderedHashTableObject::Slots;
  OrderedHashTableObject* const obj;

  // Hash table. Has hashBuckets() elements.
  // Note: a single malloc buffer is used for the data and hashTable arrays and
  // the HashCodeScrambler. The pointer in DataSlot points to the start of this
  // buffer.
  Data** getHashTable() const {
    Value v = obj->getReservedSlot(Slots::HashTableSlot);
    return static_cast<Data**>(v.toPrivate());
  }
  void setHashTable(Data** table) {
    obj->setReservedSlot(Slots::HashTableSlot, PrivateValue(table));
  }

  // Array of Data objects. Elements data[0:dataLength] are constructed and the
  // total capacity is dataCapacity.
  Data* getData() const {
    Value v = obj->getReservedSlot(Slots::DataSlot);
    return static_cast<Data*>(v.toPrivate());
  }
  void setData(Data* data) {
    obj->setReservedSlot(Slots::DataSlot, PrivateValue(data));
  }

  // Number of constructed elements in the data array.
  uint32_t getDataLength() const {
    return obj->getReservedSlot(Slots::DataLengthSlot).toPrivateUint32();
  }
  void setDataLength(uint32_t length) {
    obj->setReservedSlot(Slots::DataLengthSlot, PrivateUint32Value(length));
  }

  // Size of data array, in elements.
  uint32_t getDataCapacity() const {
    return obj->getReservedSlot(Slots::DataCapacitySlot).toPrivateUint32();
  }
  void setDataCapacity(uint32_t capacity) {
    obj->setReservedSlot(Slots::DataCapacitySlot, PrivateUint32Value(capacity));
  }

  // The number of elements in this table. This is different from dataLength
  // because the data array can contain empty/removed elements.
  uint32_t getLiveCount() const {
    return obj->getReservedSlot(Slots::LiveCountSlot).toPrivateUint32();
  }
  void setLiveCount(uint32_t liveCount) {
    obj->setReservedSlot(Slots::LiveCountSlot, PrivateUint32Value(liveCount));
  }

  // Multiplicative hash shift.
  uint32_t getHashShift() const {
    return obj->getReservedSlot(Slots::HashShiftSlot).toPrivateUint32();
  }
  void setHashShift(uint32_t hashShift) {
    obj->setReservedSlot(Slots::HashShiftSlot, PrivateUint32Value(hashShift));
  }

  // List of all live Ranges on this table in malloc memory. Populated when
  // ranges are created.
  Range* getRanges() const {
    Value v = obj->getReservedSlot(Slots::RangesSlot);
    return static_cast<Range*>(v.toPrivate());
  }
  void setRanges(Range* range) {
    obj->setReservedSlot(Slots::RangesSlot, PrivateValue(range));
  }
  Range** getRangesPtr() const {
    uintptr_t addr =
        uintptr_t(obj) + NativeObject::getFixedSlotOffset(Slots::RangesSlot);
    return reinterpret_cast<Range**>(addr);
  }

  // List of all live Ranges on this table in the GC nursery. Populated when
  // ranges are created. This is cleared at the start of minor GC and rebuilt
  // when ranges are moved.
  Range* getNurseryRanges() const {
    Value v = obj->getReservedSlot(Slots::NurseryRangesSlot);
    return static_cast<Range*>(v.toPrivate());
  }
  void setNurseryRanges(Range* range) {
    obj->setReservedSlot(Slots::NurseryRangesSlot, PrivateValue(range));
  }
  Range** getNurseryRangesPtr() const {
    uintptr_t addr = uintptr_t(obj) +
                     NativeObject::getFixedSlotOffset(Slots::NurseryRangesSlot);
    return reinterpret_cast<Range**>(addr);
  }

  // Scrambler to not reveal pointer hash codes.
  const HashCodeScrambler* getHashCodeScrambler() const {
    Value v = obj->getReservedSlot(Slots::HashCodeScramblerSlot);
    return static_cast<const HashCodeScrambler*>(v.toPrivate());
  }
  void setHashCodeScrambler(HashCodeScrambler* hcs) {
    obj->setReservedSlot(Slots::HashCodeScramblerSlot, PrivateValue(hcs));
  }

  // Logarithm base 2 of the number of buckets in the hash table initially.
  static constexpr uint32_t InitialBucketsLog2 = 1;
  static constexpr uint32_t InitialBuckets = 1 << InitialBucketsLog2;
  static constexpr uint32_t InitialHashShift =
      js::kHashNumberBits - InitialBucketsLog2;

  // The maximum load factor (mean number of entries per bucket).
  // It is an invariant that
  //     dataCapacity == floor(hashBuckets * FillFactor).
  //
  // The fill factor should be between 2 and 4, and it should be chosen so that
  // the fill factor times sizeof(Data) is close to but <= a power of 2.
  // This fixed fill factor was chosen to make the size of the data
  // array, in bytes, close to a power of two when sizeof(T) is 16.
  static constexpr double FillFactor = 8.0 / 3.0;

  // The minimum permitted value of (liveCount / dataLength).
  // If that ratio drops below this value, we shrink the table.
  static constexpr double MinDataFill = 0.25;

  template <typename F>
  void forEachRange(F&& f) {
    Range* next;
    for (Range* r = getRanges(); r; r = next) {
      next = r->next;
      f(r);
    }
    for (Range* r = getNurseryRanges(); r; r = next) {
      next = r->next;
      f(r);
    }
  }

  bool isInitialized() const {
    return !obj->getReservedSlot(Slots::DataSlot).isUndefined();
  }

  static MOZ_ALWAYS_INLINE bool calcAllocSize(uint32_t dataCapacity,
                                              uint32_t buckets,
                                              size_t* numBytes) {
    using CheckedSize = mozilla::CheckedInt<size_t>;
    auto res = CheckedSize(dataCapacity) * sizeof(Data) +
               CheckedSize(sizeof(HashCodeScrambler)) +
               CheckedSize(buckets) * sizeof(Data*);
    if (MOZ_UNLIKELY(!res.isValid())) {
      return false;
    }
    *numBytes = res.value();
    return true;
  }

  // Allocate a single buffer that stores the data array followed by the hash
  // code scrambler and the hash table entries.
  using AllocationResult =
      std::tuple<Data*, Data**, HashCodeScrambler*, size_t>;
  AllocationResult allocateBuffer(uint32_t dataCapacity, uint32_t buckets) {
    size_t numBytes = 0;
    if (MOZ_UNLIKELY(!calcAllocSize(dataCapacity, buckets, &numBytes))) {
      js::ReportAllocationOverflow(static_cast<JSContext*>(nullptr));
      return {};
    }

    void* buf = obj->zone()->pod_malloc<uint8_t>(numBytes);
    if (!buf) {
      return {};
    }

    static_assert(alignof(Data) % alignof(HashCodeScrambler) == 0,
                  "Hash code scrambler must be aligned properly");
    static_assert(alignof(HashCodeScrambler) % alignof(Data*) == 0,
                  "Hash table entries must be aligned properly");

    auto* data = static_cast<Data*>(buf);
    auto* hcs = reinterpret_cast<HashCodeScrambler*>(data + dataCapacity);
    auto** table = reinterpret_cast<Data**>(hcs + 1);

    MOZ_ASSERT(uintptr_t(table + buckets) == uintptr_t(buf) + numBytes);

    return {data, table, hcs, numBytes};
  }

  void updateHashTableForRekey(Data* entry, HashNumber oldHash,
                               HashNumber newHash) {
    uint32_t hashShift = getHashShift();
    oldHash >>= hashShift;
    newHash >>= hashShift;

    if (oldHash == newHash) {
      return;
    }

    // Remove this entry from its old hash chain. (If this crashes reading
    // nullptr, it would mean we did not find this entry on the hash chain where
    // we expected it. That probably means the key's hash code changed since it
    // was inserted, breaking the hash code invariant.)
    Data** hashTable = getHashTable();
    Data** ep = &hashTable[oldHash];
    while (*ep != entry) {
      ep = &(*ep)->chain;
    }
    *ep = entry->chain;

    // Add it to the new hash chain. We could just insert it at the beginning of
    // the chain. Instead, we do a bit of work to preserve the invariant that
    // hash chains always go in reverse insertion order (descending memory
    // order). No code currently depends on this invariant, so it's fine to kill
    // it if needed.
    ep = &hashTable[newHash];
    while (*ep && *ep > entry) {
      ep = &(*ep)->chain;
    }
    entry->chain = *ep;
    *ep = entry;
  }

 public:
  explicit OrderedHashTableImpl(OrderedHashTableObject* obj) : obj(obj) {}

  [[nodiscard]] bool init(const HashCodeScrambler& hcs) {
    MOZ_ASSERT(!isInitialized(), "init must be called at most once");

    constexpr uint32_t buckets = InitialBuckets;
    constexpr uint32_t capacity = uint32_t(buckets * FillFactor);

    auto [dataAlloc, tableAlloc, hcsAlloc, numBytes] =
        allocateBuffer(capacity, buckets);
    if (!dataAlloc) {
      return false;
    }

    AddCellMemory(obj, numBytes, MemoryUse::MapObjectTable);

    *hcsAlloc = hcs;

    std::uninitialized_fill_n(tableAlloc, buckets, nullptr);

    obj->initReservedSlot(Slots::HashTableSlot, PrivateValue(tableAlloc));
    obj->initReservedSlot(Slots::DataSlot, PrivateValue(dataAlloc));
    obj->initReservedSlot(Slots::DataLengthSlot, PrivateUint32Value(0));
    obj->initReservedSlot(Slots::DataCapacitySlot,
                          PrivateUint32Value(capacity));
    obj->initReservedSlot(Slots::LiveCountSlot, PrivateUint32Value(0));
    obj->initReservedSlot(Slots::HashShiftSlot,
                          PrivateUint32Value(InitialHashShift));
    obj->initReservedSlot(Slots::RangesSlot, PrivateValue(nullptr));
    obj->initReservedSlot(Slots::NurseryRangesSlot, PrivateValue(nullptr));
    obj->initReservedSlot(Slots::HashCodeScramblerSlot, PrivateValue(hcsAlloc));
    MOZ_ASSERT(hashBuckets() == buckets);
    return true;
  }

  void destroy(JS::GCContext* gcx) {
    if (isInitialized()) {
      forEachRange([](Range* range) { range->onTableDestroyed(); });
      Data* data = getData();
      MOZ_ASSERT(data);
      freeData(gcx, data, getDataLength(), getDataCapacity(), hashBuckets());
      setData(nullptr);
    }
  }

  void trackMallocBufferOnPromotion() {
    MOZ_ASSERT(obj->isTenured());
    size_t numBytes = 0;
    MOZ_ALWAYS_TRUE(calcAllocSize(getDataCapacity(), hashBuckets(), &numBytes));
    AddCellMemory(obj, numBytes, MemoryUse::MapObjectTable);
  }

  size_t sizeOfExcludingObject(mozilla::MallocSizeOf mallocSizeOf) const {
    size_t size = 0;
    if (isInitialized()) {
      // Note: this also includes the HashCodeScrambler and the hashTable array.
      size += mallocSizeOf(getData());
    }
    return size;
  }

  /* Return the number of elements in the table. */
  uint32_t count() const { return getLiveCount(); }

  /* True if any element matches l. */
  bool has(const Lookup& l) const { return lookup(l) != nullptr; }

  /* Return a pointer to the element, if any, that matches l, or nullptr. */
  T* get(const Lookup& l) {
    Data* e = lookup(l, prepareHash(l));
    return e ? &e->element : nullptr;
  }

  /*
   * If the table already contains an entry that matches |element|,
   * replace that entry with |element|. Otherwise add a new entry.
   *
   * On success, return true, whether there was already a matching element or
   * not. On allocation failure, return false. If this returns false, it
   * means the element was not added to the table.
   */
  template <typename ElementInput>
  [[nodiscard]] bool put(ElementInput&& element) {
    HashNumber h = prepareHash(Ops::getKey(element));
    if (Data* e = lookup(Ops::getKey(element), h)) {
      e->element = std::forward<ElementInput>(element);
      return true;
    }

    if (getDataLength() == getDataCapacity() && !rehashOnFull()) {
      return false;
    }

    auto [entry, chain] = addEntry(h);
    new (entry) Data(std::forward<ElementInput>(element), chain);
    return true;
  }

  /*
   * If the table contains an element matching l, remove it and return true.
   * Otherwise return false.
   */
  bool remove(const Lookup& l) {
    // Note: This could be optimized so that removing the last entry,
    // data[dataLength - 1], decrements dataLength. LIFO use cases would
    // benefit.

    // If a matching entry exists, empty it.
    Data* e = lookup(l, prepareHash(l));
    if (e == nullptr) {
      return false;
    }

    MOZ_ASSERT(uint32_t(e - getData()) < getDataCapacity());

    uint32_t liveCount = getLiveCount();
    liveCount--;
    setLiveCount(liveCount);
    Ops::makeEmpty(&e->element);

    // Update active Ranges.
    uint32_t pos = e - getData();
    forEachRange([this, pos](Range* range) { range->onRemove(obj, pos); });

    // If many entries have been removed, try to shrink the table. Ignore OOM
    // because shrinking the table is an optimization and it's okay for it to
    // fail.
    if (hashBuckets() > InitialBuckets &&
        liveCount < getDataLength() * MinDataFill) {
      (void)rehash(getHashShift() + 1);
    }

    return true;
  }

  /*
   * Remove all entries.
   *
   * The effect on live Ranges is the same as removing all entries; in
   * particular, those Ranges are still live and will see any entries added
   * after a clear().
   */
  void clear() {
    if (getDataLength() != 0) {
      destroyData(getData(), getDataLength());
      setDataLength(0);
      setLiveCount(0);

      size_t buckets = hashBuckets();
      std::fill_n(getHashTable(), buckets, nullptr);

      forEachRange([](Range* range) { range->onClear(); });

      // Try to shrink the table. Ignore OOM because shrinking the table is an
      // optimization and it's okay for it to fail.
      if (buckets > InitialBuckets) {
        (void)rehash(InitialHashShift);
      }
    }

    MOZ_ASSERT(getHashTable());
    MOZ_ASSERT(getData());
    MOZ_ASSERT(getDataLength() == 0);
    MOZ_ASSERT(getLiveCount() == 0);
  }

  /*
   * Ranges are used to iterate over OrderedHashTableObjects.
   *
   * Suppose 'Map' is an OrderedHashMapImpl, and 'obj' is a MapObject.
   * Then you can walk all the key-value pairs like this:
   *
   *     for (Map::Range r = Map(obj).all(); !r.empty(obj); r.popFront(obj)) {
   *         Map::Entry& pair = r.front(obj);
   *         ... do something with pair ...
   *     }
   *
   * Ranges remain valid for the lifetime of the OrderedHashTableObject, even if
   * entries are added or removed or the table is resized. Don't do anything
   * to a Range, except destroy it, after the OrderedHashTableObject has been
   * destroyed. (We support destroying the two objects in either order to
   * humor the GC, bless its nondeterministic heart.)
   *
   * Warning: The behavior when the current front() entry is removed from the
   * table is subtly different from js::HashTable<>::Enum::removeFront()!
   * HashTable::Enum doesn't skip any entries when you removeFront() and then
   * popFront(). OrderedHashTableObject::Range does! (This is useful for using a
   * Range to implement JS Map.prototype.iterator.)
   *
   * The workaround is to call popFront() as soon as possible,
   * before there's any possibility of modifying the table:
   *
   *     for (Map::Range r = Map(obj).all(); !r.empty(obj); ) {
   *         Key key = r.front(obj).key;         // this won't modify the map
   *         Value val = r.front(obj).value;     // this won't modify the map
   *         r.popFront(obj);
   *         // ...do things that might modify the map...
   *     }
   */
  class Range {
    friend class OrderedHashTableImpl;

    /* The index of front() within the data array. */
    uint32_t i = 0;

    /*
     * The number of nonempty entries in the data array to the left of front().
     * This is used when the table is resized or compacted.
     */
    uint32_t count = 0;

    /*
     * Links in the doubly-linked list of active Ranges on the Map/Set object.
     *
     * prevp points to the previous Range's .next field;
     *   or to the table's RangesSlot or NurseryRangesSlot if this is the first
     *   Range in the list.
     * next points to the next Range;
     *   or nullptr if this is the last Range in the list.
     *
     * Invariant: *prevp == this.
     */
    Range** prevp;
    Range* next;

    /*
     * Create a Range over all the entries in |obj|.
     * (This is private on purpose. End users must use ::all().)
     */
    Range(OrderedHashTableObject* obj, Range** listp)
        : prevp(listp), next(*listp) {
      *prevp = this;
      if (next) {
        next->prevp = &next;
      }
      seek(obj);
      MOZ_ASSERT(valid());
    }

   public:
    Range(OrderedHashTableObject* obj, const Range& other, bool inNursery)
        : i(other.i),
          count(other.count),
          prevp(inNursery ? OrderedHashTableImpl(obj).getNurseryRangesPtr()
                          : OrderedHashTableImpl(obj).getRangesPtr()),
          next(*prevp) {
      *prevp = this;
      if (next) {
        next->prevp = &next;
      }
      MOZ_ASSERT(valid());
    }

    ~Range() {
      if (!prevp) {
        // Head of removed nursery ranges.
        return;
      }
      *prevp = next;
      if (next) {
        next->prevp = prevp;
      }
    }

   protected:
    // Prohibit copy assignment.
    Range& operator=(const Range& other) = delete;

    void seek(OrderedHashTableObject* obj) {
      Data* data = OrderedHashTableImpl(obj).getData();
      uint32_t dataLength = OrderedHashTableImpl(obj).getDataLength();
      while (i < dataLength && Ops::isEmpty(Ops::getKey(data[i].element))) {
        i++;
      }
    }

    /*
     * The hash table calls this when an entry is removed.
     * j is the index of the removed entry.
     */
    void onRemove(OrderedHashTableObject* obj, uint32_t j) {
      MOZ_ASSERT(valid());
      if (j < i) {
        count--;
      }
      if (j == i) {
        seek(obj);
      }
    }

    /*
     * The hash table calls this when the table is resized or compacted.
     * Since |count| is the number of nonempty entries to the left of
     * front(), discarding the empty entries will not affect count, and it
     * will make i and count equal.
     */
    void onCompact() {
      MOZ_ASSERT(valid());
      i = count;
    }

    /* The hash table calls this when cleared. */
    void onClear() {
      MOZ_ASSERT(valid());
      i = count = 0;
    }

#ifdef DEBUG
    bool valid() const { return /* *prevp == this && */ next != this; }
#endif

    void onTableDestroyed() {
      MOZ_ASSERT(valid());
      prevp = &next;
      next = this;
      MOZ_ASSERT(!valid());
    }

   public:
    bool empty(OrderedHashTableObject* obj) const {
      MOZ_ASSERT(valid());
      return i >= OrderedHashTableImpl(obj).getDataLength();
    }

    /*
     * Return the first element in the range. This must not be called if
     * this->empty().
     *
     * Warning: Removing an entry from the table also removes it from any
     * live Ranges, and a Range can become empty that way, rendering
     * front() invalid. If in doubt, check empty() before calling front().
     */
    const T& front(OrderedHashTableObject* obj) const {
      MOZ_ASSERT(valid());
      MOZ_ASSERT(!empty(obj));
      return OrderedHashTableImpl(obj).getData()[i].element;
    }

    /*
     * Remove the first element from this range.
     * This must not be called if this->empty().
     *
     * Warning: Removing an entry from the table also removes it from any
     * live Ranges, and a Range can become empty that way, rendering
     * popFront() invalid. If in doubt, check empty() before calling
     * popFront().
     */
    void popFront(OrderedHashTableObject* obj) {
      MOZ_ASSERT(valid());
      MOZ_ASSERT(!empty(obj));
      MOZ_ASSERT(!Ops::isEmpty(
          Ops::getKey(OrderedHashTableImpl(obj).getData()[i].element)));
      count++;
      i++;
      seek(obj);
    }

    static size_t offsetOfI() { return offsetof(Range, i); }
    static size_t offsetOfCount() { return offsetof(Range, count); }
    static size_t offsetOfPrevP() { return offsetof(Range, prevp); }
    static size_t offsetOfNext() { return offsetof(Range, next); }
  };

  Range all() const {
    // Range operates on a mutable table but its interface does not permit
    // modification of the contents of the table.
    return Range(obj, getRangesPtr());
  }

  void trace(JSTracer* trc) {
    Data* data = getData();
    uint32_t dataLength = getDataLength();
    for (uint32_t i = 0; i < dataLength; i++) {
      if (!Ops::isEmpty(Ops::getKey(data[i].element))) {
        Ops::trace(trc, this, i, data[i].element);
      }
    }
  }

  // For use by the implementation of Ops::trace.
  template <typename Key>
  void traceKey(JSTracer* trc, uint32_t index, Key& key) {
    MOZ_ASSERT(index < getDataLength());
    using MutableKey = std::remove_const_t<Key>;
    using UnbarrieredKey = typename RemoveBarrier<MutableKey>::Type;
    UnbarrieredKey newKey = key;
    JS::GCPolicy<UnbarrieredKey>::trace(trc, &newKey,
                                        "OrderedHashTableObject key");
    if (newKey != key) {
      rekey(&getData()[index], newKey);
    }
  }
  template <typename Value>
  void traceValue(JSTracer* trc, Value& value) {
    JS::GCPolicy<Value>::trace(trc, &value, "OrderedHashMapObject value");
  }

  /*
   * Allocate a new Range, possibly in nursery memory. The buffer must be
   * large enough to hold a Range object.
   */
  Range* createRange(void* buffer, bool inNursery) const {
    Range** listp = inNursery ? getNurseryRangesPtr() : getRangesPtr();
    new (buffer) Range(obj, listp);
    return static_cast<Range*>(buffer);
  }

  void destroyNurseryRanges() {
    if (Range* range = getNurseryRanges()) {
      range->prevp = nullptr;
    }
    setNurseryRanges(nullptr);
  }

  void updateRangesAfterMove(OrderedHashTableObject* old) {
    if (Range* range = getRanges()) {
      MOZ_ASSERT(range->prevp == OrderedHashTableImpl(old).getRangesPtr());
      range->prevp = getRangesPtr();
    }
    if (Range* range = getNurseryRanges()) {
      MOZ_ASSERT(range->prevp ==
                 OrderedHashTableImpl(old).getNurseryRangesPtr());
      range->prevp = getNurseryRangesPtr();
    }
  }

#ifdef DEBUG
  bool hasNurseryRanges() const { return getNurseryRanges(); }
#endif

  /*
   * Change the value of the given key.
   *
   * This calls Ops::hash on both the current key and the new key.
   * Ops::hash on the current key must return the same hash code as
   * when the entry was added to the table.
   */
  void rekeyOneEntry(const Key& current, const Key& newKey, const T& element) {
    if (current == newKey) {
      return;
    }

    HashNumber currentHash = prepareHash(current);
    HashNumber newHash = prepareHash(newKey);

    Data* entry = lookup(current, currentHash);
    MOZ_ASSERT(entry);
    entry->element = element;

    updateHashTableForRekey(entry, currentHash, newHash);
  }

  static constexpr size_t offsetOfDataElement() {
    static_assert(offsetof(Data, element) == 0,
                  "RangeFront and RangePopFront depend on offsetof(Data, "
                  "element) being 0");
    return offsetof(Data, element);
  }
  static constexpr size_t offsetOfDataChain() { return offsetof(Data, chain); }
  static constexpr size_t sizeofData() { return sizeof(Data); }

  HashNumber prepareHash(const Lookup& l) const {
    const HashCodeScrambler& hcs = *getHashCodeScrambler();
    return mozilla::ScrambleHashCode(Ops::hash(l, hcs));
  }

 private:
  /* The size of the hash table, in elements. Always a power of two. */
  uint32_t hashBuckets() const {
    return 1 << (js::kHashNumberBits - getHashShift());
  }

  void destroyData(Data* data, uint32_t length) {
    Data* end = data + length;
    for (Data* p = data; p != end; p++) {
      p->~Data();
    }
  }

  void freeData(JS::GCContext* gcx, Data* data, uint32_t length,
                uint32_t capacity, uint32_t hashBuckets) {
    MOZ_ASSERT(data);
    MOZ_ASSERT(capacity > 0);

    destroyData(data, length);

    size_t numBytes;
    MOZ_ALWAYS_TRUE(calcAllocSize(capacity, hashBuckets, &numBytes));

    gcx->free_(obj, data, numBytes, MemoryUse::MapObjectTable);
  }

  Data* lookup(const Lookup& l, HashNumber h) const {
    Data** hashTable = getHashTable();
    uint32_t hashShift = getHashShift();
    for (Data* e = hashTable[h >> hashShift]; e; e = e->chain) {
      if (Ops::match(Ops::getKey(e->element), l)) {
        return e;
      }
    }
    return nullptr;
  }

  const Data* lookup(const Lookup& l) const {
    return lookup(l, prepareHash(l));
  }

  std::tuple<Data*, Data*> addEntry(HashNumber hash) {
    uint32_t dataLength = getDataLength();
    MOZ_ASSERT(dataLength < getDataCapacity());

    Data* entry = &getData()[dataLength];
    setDataLength(dataLength + 1);
    setLiveCount(getLiveCount() + 1);

    Data** hashTable = getHashTable();
    hash >>= getHashShift();
    Data* chain = hashTable[hash];
    hashTable[hash] = entry;

    return std::make_tuple(entry, chain);
  }

  /* This is called after rehashing the table. */
  void compacted() {
    // If we had any empty entries, compacting may have moved live entries
    // to the left within the data array. Notify all live Ranges of the change.
    forEachRange([](Range* range) { range->onCompact(); });
  }

  /* Compact the entries in the data array and rehash them. */
  void rehashInPlace() {
    Data** hashTable = getHashTable();
    std::fill_n(hashTable, hashBuckets(), nullptr);

    Data* const data = getData();
    uint32_t hashShift = getHashShift();
    Data* wp = data;
    Data* end = data + getDataLength();
    for (Data* rp = data; rp != end; rp++) {
      if (!Ops::isEmpty(Ops::getKey(rp->element))) {
        HashNumber h = prepareHash(Ops::getKey(rp->element)) >> hashShift;
        if (rp != wp) {
          wp->element = std::move(rp->element);
        }
        wp->chain = hashTable[h];
        hashTable[h] = wp;
        wp++;
      }
    }
    MOZ_ASSERT(wp == data + getLiveCount());

    while (wp != end) {
      wp->~Data();
      wp++;
    }
    setDataLength(getLiveCount());
    compacted();
  }

  [[nodiscard]] bool rehashOnFull() {
    MOZ_ASSERT(getDataLength() == getDataCapacity());

    // If the hashTable is more than 1/4 deleted data, simply rehash in
    // place to free up some space. Otherwise, grow the table.
    uint32_t newHashShift = getLiveCount() >= getDataCapacity() * 0.75
                                ? getHashShift() - 1
                                : getHashShift();
    return rehash(newHashShift);
  }

  /*
   * Grow, shrink, or compact both the hash table and data array.
   *
   * On success, this returns true, dataLength == liveCount, and there are no
   * empty elements in data[0:dataLength]. On allocation failure, this
   * leaves everything as it was and returns false.
   */
  [[nodiscard]] bool rehash(uint32_t newHashShift) {
    // If the size of the table is not changing, rehash in place to avoid
    // allocating memory.
    if (newHashShift == getHashShift()) {
      rehashInPlace();
      return true;
    }

    // Ensure the new capacity fits into INT32_MAX.
    constexpr size_t maxCapacityLog2 =
        mozilla::tl::FloorLog2<size_t(INT32_MAX / FillFactor)>::value;
    static_assert(maxCapacityLog2 < kHashNumberBits);

    // Fail if |(js::kHashNumberBits - newHashShift) > maxCapacityLog2|.
    //
    // Reorder |kHashNumberBits| so both constants are on the right-hand side.
    if (MOZ_UNLIKELY(newHashShift < (js::kHashNumberBits - maxCapacityLog2))) {
      ReportAllocationOverflow(static_cast<JSContext*>(nullptr));
      return false;
    }

    uint32_t newHashBuckets = uint32_t(1)
                              << (js::kHashNumberBits - newHashShift);
    uint32_t newCapacity = uint32_t(newHashBuckets * FillFactor);

    auto [newData, newHashTable, newHcs, numBytes] =
        allocateBuffer(newCapacity, newHashBuckets);
    if (!newData) {
      return false;
    }

    *newHcs = *getHashCodeScrambler();

    std::uninitialized_fill_n(newHashTable, newHashBuckets, nullptr);

    Data* const oldData = getData();
    const uint32_t oldDataLength = getDataLength();

    Data* wp = newData;
    Data* end = oldData + oldDataLength;
    for (Data* p = oldData; p != end; p++) {
      if (!Ops::isEmpty(Ops::getKey(p->element))) {
        HashNumber h = prepareHash(Ops::getKey(p->element)) >> newHashShift;
        new (wp) Data(std::move(p->element), newHashTable[h]);
        newHashTable[h] = wp;
        wp++;
      }
    }
    MOZ_ASSERT(wp == newData + getLiveCount());

    freeData(obj->runtimeFromMainThread()->gcContext(), oldData, oldDataLength,
             getDataCapacity(), hashBuckets());

    AddCellMemory(obj, numBytes, MemoryUse::MapObjectTable);

    setHashTable(newHashTable);
    setData(newData);
    setDataLength(getLiveCount());
    setDataCapacity(newCapacity);
    setHashShift(newHashShift);
    setHashCodeScrambler(newHcs);
    MOZ_ASSERT(hashBuckets() == newHashBuckets);

    compacted();
    return true;
  }

  // Change the key of the front entry.
  //
  // This calls Ops::hash on both the current key and the new key. Ops::hash on
  // the current key must return the same hash code as when the entry was added
  // to the table.
  void rekey(Data* entry, const Key& k) {
    HashNumber oldHash = prepareHash(Ops::getKey(entry->element));
    HashNumber newHash = prepareHash(k);
    Ops::setKey(entry->element, k);
    updateHashTableForRekey(entry, oldHash, newHash);
  }
};

}  // namespace detail

class OrderedHashMapObject : public detail::OrderedHashTableObject {};

template <class Key, class Value, class OrderedHashPolicy>
class MOZ_STACK_CLASS OrderedHashMapImpl {
 public:
  class Entry {
    template <class, class>
    friend class detail::OrderedHashTableImpl;
    void operator=(const Entry& rhs) {
      const_cast<Key&>(key) = rhs.key;
      value = rhs.value;
    }

    void operator=(Entry&& rhs) {
      MOZ_ASSERT(this != &rhs, "self-move assignment is prohibited");
      const_cast<Key&>(key) = std::move(rhs.key);
      value = std::move(rhs.value);
    }

   public:
    Entry() = default;
    explicit Entry(const Key& k) : key(k) {}
    template <typename V>
    Entry(const Key& k, V&& v) : key(k), value(std::forward<V>(v)) {}
    Entry(Entry&& rhs) : key(std::move(rhs.key)), value(std::move(rhs.value)) {}

    const Key key{};
    Value value{};

    static constexpr size_t offsetOfKey() { return offsetof(Entry, key); }
    static constexpr size_t offsetOfValue() { return offsetof(Entry, value); }
  };

 private:
  struct MapOps;
  using Impl = detail::OrderedHashTableImpl<Entry, MapOps>;

  struct MapOps : OrderedHashPolicy {
    using KeyType = Key;
    static void makeEmpty(Entry* e) {
      OrderedHashPolicy::makeEmpty(const_cast<Key*>(&e->key));

      // Clear the value. Destroying it is another possibility, but that
      // would complicate class Entry considerably.
      e->value = Value();
    }
    static const Key& getKey(const Entry& e) { return e.key; }
    static void setKey(Entry& e, const Key& k) { const_cast<Key&>(e.key) = k; }
    static void trace(JSTracer* trc, Impl* table, uint32_t index,
                      Entry& entry) {
      table->traceKey(trc, index, entry.key);
      table->traceValue(trc, entry.value);
    }
  };

  Impl impl;

 public:
  using Lookup = typename Impl::Lookup;
  using Range = typename Impl::Range;
  static constexpr size_t SlotCount = Impl::SlotCount;

  explicit OrderedHashMapImpl(OrderedHashMapObject* obj) : impl(obj) {}

  [[nodiscard]] bool init(const mozilla::HashCodeScrambler& hcs) {
    return impl.init(hcs);
  }
  uint32_t count() const { return impl.count(); }
  bool has(const Lookup& key) const { return impl.has(key); }
  Range all() const { return impl.all(); }
  Entry* get(const Lookup& key) { return impl.get(key); }
  bool remove(const Lookup& key) { return impl.remove(key); }
  void clear() { impl.clear(); }

  void destroy(JS::GCContext* gcx) { impl.destroy(gcx); }

  template <typename K, typename V>
  [[nodiscard]] bool put(K&& key, V&& value) {
    return impl.put(Entry(std::forward<K>(key), std::forward<V>(value)));
  }

  HashNumber hash(const Lookup& key) const { return impl.prepareHash(key); }

  template <typename GetNewKey>
  mozilla::Maybe<Key> rekeyOneEntry(Lookup& current, GetNewKey&& getNewKey) {
    // TODO: This is inefficient because we also look up the entry in
    // impl.rekeyOneEntry below.
    const Entry* e = get(current);
    if (!e) {
      return mozilla::Nothing();
    }

    Key newKey = getNewKey(current);
    impl.rekeyOneEntry(current, newKey, Entry(newKey, e->value));
    return mozilla::Some(newKey);
  }

  Range* createRange(void* buffer, bool inNursery) const {
    return impl.createRange(buffer, inNursery);
  }

  void destroyNurseryRanges() { impl.destroyNurseryRanges(); }
  void updateRangesAfterMove(OrderedHashMapObject* old) {
    impl.updateRangesAfterMove(old);
  }
#ifdef DEBUG
  bool hasNurseryRanges() const { return impl.hasNurseryRanges(); }
#endif

  void trackMallocBufferOnPromotion() {
    return impl.trackMallocBufferOnPromotion();
  }

  void trace(JSTracer* trc) { impl.trace(trc); }

  static constexpr size_t offsetOfEntryKey() { return Entry::offsetOfKey(); }
  static constexpr size_t offsetOfImplDataElement() {
    return Impl::offsetOfDataElement();
  }
  static constexpr size_t offsetOfImplDataChain() {
    return Impl::offsetOfDataChain();
  }
  static constexpr size_t sizeofImplData() { return Impl::sizeofData(); }

  size_t sizeOfExcludingObject(mozilla::MallocSizeOf mallocSizeOf) const {
    return impl.sizeOfExcludingObject(mallocSizeOf);
  }
};

class OrderedHashSetObject : public detail::OrderedHashTableObject {};

template <class T, class OrderedHashPolicy>
class MOZ_STACK_CLASS OrderedHashSetImpl {
 private:
  struct SetOps;
  using Impl = detail::OrderedHashTableImpl<T, SetOps>;

  struct SetOps : OrderedHashPolicy {
    using KeyType = const T;
    static const T& getKey(const T& v) { return v; }
    static void setKey(const T& e, const T& v) { const_cast<T&>(e) = v; }
    static void trace(JSTracer* trc, Impl* table, uint32_t index, T& entry) {
      table->traceKey(trc, index, entry);
    }
  };

  Impl impl;

 public:
  using Lookup = typename Impl::Lookup;
  using Range = typename Impl::Range;
  static constexpr size_t SlotCount = Impl::SlotCount;

  explicit OrderedHashSetImpl(OrderedHashSetObject* obj) : impl(obj) {}

  [[nodiscard]] bool init(const mozilla::HashCodeScrambler& hcs) {
    return impl.init(hcs);
  }
  uint32_t count() const { return impl.count(); }
  bool has(const Lookup& value) const { return impl.has(value); }
  Range all() const { return impl.all(); }
  template <typename Input>
  [[nodiscard]] bool put(Input&& value) {
    return impl.put(std::forward<Input>(value));
  }
  bool remove(const Lookup& value) { return impl.remove(value); }
  void clear() { impl.clear(); }

  void destroy(JS::GCContext* gcx) { impl.destroy(gcx); }

  HashNumber hash(const Lookup& value) const { return impl.prepareHash(value); }

  template <typename GetNewKey>
  mozilla::Maybe<T> rekeyOneEntry(Lookup& current, GetNewKey&& getNewKey) {
    // TODO: This is inefficient because we also look up the entry in
    // impl.rekeyOneEntry below.
    if (!has(current)) {
      return mozilla::Nothing();
    }

    T newKey = getNewKey(current);
    impl.rekeyOneEntry(current, newKey, newKey);
    return mozilla::Some(newKey);
  }

  Range* createRange(void* buffer, bool inNursery) const {
    return impl.createRange(buffer, inNursery);
  }

  void destroyNurseryRanges() { impl.destroyNurseryRanges(); }
  void updateRangesAfterMove(OrderedHashSetObject* old) {
    impl.updateRangesAfterMove(old);
  }
#ifdef DEBUG
  bool hasNurseryRanges() const { return impl.hasNurseryRanges(); }
#endif

  void trackMallocBufferOnPromotion() {
    return impl.trackMallocBufferOnPromotion();
  }

  void trace(JSTracer* trc) { impl.trace(trc); }

  static constexpr size_t offsetOfEntryKey() { return 0; }
  static constexpr size_t offsetOfImplDataElement() {
    return Impl::offsetOfDataElement();
  }
  static constexpr size_t offsetOfImplDataChain() {
    return Impl::offsetOfDataChain();
  }
  static constexpr size_t sizeofImplData() { return Impl::sizeofData(); }

  size_t sizeOfExcludingObject(mozilla::MallocSizeOf mallocSizeOf) const {
    return impl.sizeOfExcludingObject(mallocSizeOf);
  }
};

}  // namespace js

#endif /* builtin_OrderedHashTableObject_h */
