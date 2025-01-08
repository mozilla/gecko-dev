/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/BufferAllocator-inl.h"

#include "mozilla/ScopeExit.h"

#ifdef XP_DARWIN
#  include <mach/mach_init.h>
#  include <mach/vm_map.h>
#endif

#include "gc/GCInternals.h"
#include "gc/GCLock.h"
#include "gc/PublicIterators.h"
#include "gc/Zone.h"
#include "js/HeapAPI.h"
#include "util/Poison.h"

#include "gc/Heap-inl.h"

using namespace js;
using namespace js::gc;

static constexpr size_t MinAllocSize = MinCellSize;  // 16 bytes

static constexpr size_t MinMediumAllocSize =
    1 << BufferAllocator::MinMediumAllocShift;
static constexpr size_t MaxMediumAllocSize =
    1 << BufferAllocator::MaxMediumAllocShift;
static constexpr size_t MinLargeAllocSize = MaxMediumAllocSize + PageSize;

#ifdef DEBUG
// Magic check values used debug builds.
static constexpr uint16_t MediumBufferCheckValue = 0xBFA1;
static constexpr uint32_t LargeBufferCheckValue = 0xBFA110C2;
static constexpr uint32_t FreeRegionCheckValue = 0xBFA110C3;
#endif

namespace js::gc {

bool SmallBuffer::isNurseryOwned() const {
  return header_.get() & NURSERY_OWNED_BIT;
}

void SmallBuffer::setNurseryOwned(bool value) {
  header_.set(value ? NURSERY_OWNED_BIT : 0);
}

struct alignas(CellAlignBytes) MediumBuffer {
  uint8_t sizeClass;
  bool isNurseryOwned = false;

#ifdef DEBUG
  uint16_t checkValue = MediumBufferCheckValue;
#endif

  MediumBuffer(uint8_t sizeClass, bool nurseryOwned)
      : sizeClass(sizeClass), isNurseryOwned(nurseryOwned) {}

  static MediumBuffer* from(BufferChunk* chunk, uintptr_t offset) {
    MOZ_ASSERT(offset < ChunkSize);
    MOZ_ASSERT((offset % MinMediumAllocSize) == 0);
    auto* buffer = reinterpret_cast<MediumBuffer*>(uintptr_t(chunk) + offset);
    buffer->check();
    return buffer;
  }

  void check() const { MOZ_ASSERT(checkValue == MediumBufferCheckValue); }

  size_t bytesIncludingHeader() const {
    return BufferAllocator::SizeClassBytes(sizeClass);
  }

  void* data() { return this + 1; }
};

class alignas(CellAlignBytes) LargeBuffer
    : protected ChunkBase,
      public SlimLinkedListElement<LargeBuffer> {
  // This is atomic for the mark flag only.
  mozilla::Atomic<uintptr_t, mozilla::Relaxed> zoneAndFlags;
  uint32_t sizeInPages;  // 16TB should be enough for anyone.

#ifdef DEBUG
  uint32_t checkValue = LargeBufferCheckValue;
#endif

  enum Flags : uint8_t {
    MarkedFlag = 0,  // Cleared off-thread, hence requires atomic storage.
    NurseryOwnedFlag,
    AllocatedDuringCollectionFlag,
    FlagCount
  };

  static constexpr uintptr_t MarkedMask = Bit(MarkedFlag);
  static constexpr uintptr_t NurseryOwnedMask = Bit(NurseryOwnedFlag);
  static constexpr uintptr_t AllocatedDuringCollectionMask =
      Bit(AllocatedDuringCollectionFlag);
  static constexpr uintptr_t FlagsMask = BitMask(FlagCount);

 public:
  LargeBuffer(Zone* zone, size_t bytes)
      : ChunkBase(zone->runtimeFromMainThread()),
        zoneAndFlags(uintptr_t(zone)),
        sizeInPages(bytes / PageSize) {
    kind = ChunkKind::LargeBuffer;
    MOZ_ASSERT((zoneAndFlags & FlagsMask) == 0);
    MOZ_ASSERT((bytes % PageSize) == 0);
  }

  void check() const { MOZ_ASSERT(checkValue == LargeBufferCheckValue); }

  bool isMarked() const { return zoneAndFlags & MarkedMask; };
  void clearMarked() { zoneAndFlags &= ~MarkedMask; }
  bool markAtomic() {
    uintptr_t oldValue;
    uintptr_t newValue;
    do {
      oldValue = zoneAndFlags;
      if (oldValue & MarkedMask) {
        return false;
      }
      newValue = oldValue | MarkedMask;
    } while (!zoneAndFlags.compareExchange(oldValue, newValue));
    return true;
  }

  bool isNurseryOwned() const { return zoneAndFlags & NurseryOwnedMask; }
  void setNurseryOwned(bool value) {
    if (value) {
      zoneAndFlags |= NurseryOwnedMask;
    } else {
      zoneAndFlags &= ~NurseryOwnedMask;
    }
  }

  bool wasAllocatedDuringCollection() const {
    bool result = zoneAndFlags & AllocatedDuringCollectionMask;
    MOZ_ASSERT_IF(isNurseryOwned(), !result);
    return result;
  }
  void setAllocatedDuringCollection(bool value) {
    MOZ_ASSERT(!isNurseryOwned());
    if (value) {
      zoneAndFlags |= AllocatedDuringCollectionMask;
    } else {
      zoneAndFlags &= ~AllocatedDuringCollectionMask;
    }
  }

  Zone* zone() const {
    return reinterpret_cast<Zone*>(zoneAndFlags & ~FlagsMask);
  }

  void setSizeInPages(size_t pages) { sizeInPages = pages; }
  size_t bytesIncludingHeader() const { return sizeInPages * PageSize; }

  void* data() { return this + 1; }

  bool isPointerWithinAllocation(void* ptr) const;
};

// An RAII guard to lock and unlock BufferAllocator::lock.
class BufferAllocator::AutoLockAllocator : public LockGuard<Mutex> {
 public:
  explicit AutoLockAllocator(BufferAllocator* allocator)
      : LockGuard(allocator->lock) {}
};

// Describes a free region in a buffer chunk. This structure is stored at the
// end of the region.
//
// Medium allocations are made in FreeRegions in increasing address order. The
// final allocation will contain the now empty and unused FreeRegion structure.
// FreeRegions are stored in buckets based on their size in FreeLists. Each
// bucket is a linked list of FreeRegions.
struct BufferAllocator::FreeRegion
    : public SlimLinkedListElement<BufferAllocator::FreeRegion> {
  uintptr_t startAddr;
  bool hasDecommittedPages;

#ifdef DEBUG
  uint32_t checkValue = FreeRegionCheckValue;
#endif

  explicit FreeRegion(uintptr_t startAddr, bool decommitted = false)
      : startAddr(startAddr), hasDecommittedPages(decommitted) {}

  static FreeRegion* fromEndOffset(BufferChunk* chunk, uintptr_t endOffset) {
    MOZ_ASSERT(endOffset <= ChunkSize);
    return fromEndAddr(uintptr_t(chunk) + endOffset);
  }
  static FreeRegion* fromEndAddr(uintptr_t endAddr) {
    MOZ_ASSERT((endAddr % MinMediumAllocSize) == 0);
    auto* region = reinterpret_cast<FreeRegion*>(endAddr - sizeof(FreeRegion));
    region->check();
    return region;
  }

  void check() const { MOZ_ASSERT(checkValue == FreeRegionCheckValue); }

  uintptr_t getEnd() const { return uintptr_t(this + 1); }
  size_t size() const { return getEnd() - startAddr; }
};

using BufferChunkAllocBitmap = mozilla::BitSet<ChunkSize / MinMediumAllocSize>;

using BufferChunkPageBitmap = mozilla::BitSet<ChunkSize / PageSize, uint32_t>;

// A chunk containing buffer allocations for a single zone. Unlike ArenaChunk,
// allocations from different zones do not share chunks.
struct BufferChunk : public ChunkBase,
                     public SlimLinkedListElement<BufferChunk> {
  // One bit minimum per allocation, no gray bits.
  static constexpr size_t BytesPerMarkBit = MinMediumAllocSize;
  using BufferMarkBitmap = MarkBitmap<BytesPerMarkBit, 0>;
  MainThreadOrGCTaskData<BufferMarkBitmap> markBits;

  MainThreadOrGCTaskData<BufferChunkPageBitmap> decommittedPages;

  MainThreadOrGCTaskData<BufferChunkAllocBitmap> allocBitmap;
  MainThreadData<Zone*> zone;  // Only used by GetAllocZone.

  MainThreadOrGCTaskData<bool> allocatedDuringCollection;
  MainThreadData<bool> hasNurseryOwnedAllocs;
  MainThreadOrGCTaskData<bool> hasNurseryOwnedAllocsAfterSweep;

  static BufferChunk* from(void* alloc) {
    ChunkBase* chunk = js::gc::detail::GetGCAddressChunkBase(alloc);
    MOZ_ASSERT(chunk->kind == ChunkKind::MediumBuffers);
    return static_cast<BufferChunk*>(chunk);
  }

  explicit BufferChunk(Zone* zone)
      : ChunkBase(zone->runtimeFromMainThread(), ChunkKind::MediumBuffers),
        zone(zone) {}

  ~BufferChunk() { MOZ_ASSERT(allocBitmap.ref().IsEmpty()); }

  void setAllocated(void* alloc, bool allocated);
  bool isAllocated(void* alloc) const;
  bool isAllocated(uintptr_t offset) const;

  // Find next/previous allocations from |offset|. Return ChunkSize on failure.
  size_t findNextAllocated(uintptr_t offset) const;
  size_t findPrevAllocated(uintptr_t offset) const;

  bool isPointerWithinAllocation(void* ptr) const;

 private:
  uintptr_t ptrToOffset(void* alloc) const;
};

constexpr size_t FirstMediumAllocOffset =
    RoundUp(sizeof(BufferChunk), MinMediumAllocSize);

// Iterate allocations in a BufferChunk.
class BufferChunkIter {
  BufferChunk* chunk;
  size_t offset = FirstMediumAllocOffset;
  size_t size = 0;

 public:
  explicit BufferChunkIter(BufferChunk* chunk) : chunk(chunk) { settle(); }
  bool done() const { return offset == ChunkSize; }
  void next() {
    MOZ_ASSERT(!done());
    offset += size;
    MOZ_ASSERT(offset <= ChunkSize);
    if (!done()) {
      settle();
    }
  }
  size_t getOffset() const {
    MOZ_ASSERT(!done());
    return offset;
  }
  MediumBuffer* get() const {
    MOZ_ASSERT(!done());
    return MediumBuffer::from(chunk, offset);
  }
  operator MediumBuffer*() { return get(); }
  MediumBuffer* operator->() { return get(); }
  MediumBuffer& operator*() { return *get(); }

 private:
  void settle() {
    offset = chunk->findNextAllocated(offset);
    if (!done()) {
      size = get()->bytesIncludingHeader();
    }
  }
};

static void CheckHighBitsOfPointer(void* ptr) {
#ifdef JS_64BIT
  // We require bit 48 and higher be clear.
  MOZ_DIAGNOSTIC_ASSERT((uintptr_t(ptr) >> 47) == 0);
#endif
}

BufferAllocator::FreeLists::FreeLists(FreeLists&& other) {
  MOZ_ASSERT(this != &other);
  assertEmpty();
  std::swap(lists, other.lists);
  std::swap(available, other.available);
  other.assertEmpty();
}

BufferAllocator::FreeLists& BufferAllocator::FreeLists::operator=(
    FreeLists&& other) {
  MOZ_ASSERT(this != &other);
  assertEmpty();
  std::swap(lists, other.lists);
  std::swap(available, other.available);
  other.assertEmpty();
  return *this;
}

size_t BufferAllocator::FreeLists::getFirstAvailableSizeClass(
    size_t minSizeClass) const {
  size_t result = available.FindNext(minSizeClass);
  MOZ_ASSERT(result >= minSizeClass);
  MOZ_ASSERT_IF(result != SIZE_MAX, !lists[result].isEmpty());
  return result;
}

BufferAllocator::FreeRegion* BufferAllocator::FreeLists::getFirstRegion(
    size_t sizeClass) {
  MOZ_ASSERT(!lists[sizeClass].isEmpty());
  return lists[sizeClass].getFirst();
}

void BufferAllocator::FreeLists::pushFront(size_t sizeClass,
                                           FreeRegion* region) {
  MOZ_ASSERT(sizeClass < MediumAllocClasses);
  lists[sizeClass].pushFront(region);
  available[sizeClass] = true;
}

void BufferAllocator::FreeLists::pushBack(size_t sizeClass,
                                          FreeRegion* region) {
  MOZ_ASSERT(sizeClass < MediumAllocClasses);
  lists[sizeClass].pushBack(region);
  available[sizeClass] = true;
}

void BufferAllocator::FreeLists::append(FreeLists&& other) {
  for (size_t i = 0; i < MediumAllocClasses; i++) {
    if (!other.lists[i].isEmpty()) {
      lists[i].append(std::move(other.lists[i]));
      available[i] = true;
    }
  }
  other.available.ResetAll();
  other.assertEmpty();
}

void BufferAllocator::FreeLists::prepend(FreeLists&& other) {
  for (size_t i = 0; i < MediumAllocClasses; i++) {
    if (!other.lists[i].isEmpty()) {
      lists[i].prepend(std::move(other.lists[i]));
      available[i] = true;
    }
  }
  other.available.ResetAll();
  other.assertEmpty();
}

void BufferAllocator::FreeLists::remove(size_t sizeClass, FreeRegion* region) {
  MOZ_ASSERT(sizeClass < MediumAllocClasses);
  lists[sizeClass].remove(region);
  available[sizeClass] = !lists[sizeClass].isEmpty();
}

void BufferAllocator::FreeLists::clear() {
  for (auto& freeList : lists) {
    new (&freeList) FreeList;  // clear() is less efficient.
  }
  available.ResetAll();
}

template <typename Pred>
void BufferAllocator::FreeLists::eraseIf(Pred&& pred) {
  for (size_t i = 0; i < MediumAllocClasses; i++) {
    FreeList& freeList = lists[i];
    FreeRegion* region = freeList.getFirst();
    while (region) {
      FreeRegion* next = region->getNext();
      if (pred(region)) {
        freeList.remove(region);
      }
      region = next;
    }
    available[i] = !freeList.isEmpty();
  }
}

inline void BufferAllocator::FreeLists::assertEmpty() const {
#ifdef DEBUG
  for (size_t i = 0; i < MediumAllocClasses; i++) {
    MOZ_ASSERT(lists[i].isEmpty());
  }
  MOZ_ASSERT(available.IsEmpty());
#endif
}

inline void BufferAllocator::FreeLists::assertContains(
    size_t sizeClass, FreeRegion* region) const {
#ifdef DEBUG
  MOZ_ASSERT(available[sizeClass]);
  MOZ_ASSERT(lists[sizeClass].contains(region));
#endif
}

inline void BufferAllocator::FreeLists::checkAvailable() const {
#ifdef DEBUG
  for (size_t i = 0; i < MediumAllocClasses; i++) {
    MOZ_ASSERT(available[i] == !lists[i].isEmpty());
  }
#endif
}

}  // namespace js::gc

MOZ_ALWAYS_INLINE void PoisonAlloc(void* ptr, uint8_t value, size_t bytes,
                                   MemCheckKind kind) {
#ifndef EARLY_BETA_OR_EARLIER
  // Limit poisoning in release builds.
  bytes = std::min(bytes, size_t(256));
#endif
  AlwaysPoison(ptr, value, bytes, kind);
}

uintptr_t BufferChunk::ptrToOffset(void* alloc) const {
  MOZ_ASSERT((uintptr_t(alloc) & ~ChunkMask) == uintptr_t(this));

  uintptr_t offset = uintptr_t(alloc) & ChunkMask;
  MOZ_ASSERT(offset >= FirstMediumAllocOffset);

  return offset;
}

void BufferChunk::setAllocated(void* alloc, bool allocated) {
  uintptr_t offset = ptrToOffset(alloc);
  size_t bit = offset / MinMediumAllocSize;
  allocBitmap.ref()[bit] = allocated;
}

bool BufferChunk::isAllocated(void* alloc) const {
  return isAllocated(ptrToOffset(alloc));
}

bool BufferChunk::isAllocated(uintptr_t offset) const {
  MOZ_ASSERT(offset >= FirstMediumAllocOffset);
  MOZ_ASSERT(offset < ChunkSize);

  size_t bit = offset / MinMediumAllocSize;
  return allocBitmap.ref()[bit];
}

size_t BufferChunk::findNextAllocated(uintptr_t offset) const {
  MOZ_ASSERT(offset >= FirstMediumAllocOffset);
  MOZ_ASSERT(offset < ChunkSize);

  size_t bit = offset / MinMediumAllocSize;
  size_t next = allocBitmap.ref().FindNext(bit);
  if (next == SIZE_MAX) {
    return ChunkSize;
  }

  return next * MinMediumAllocSize;
}

size_t BufferChunk::findPrevAllocated(uintptr_t offset) const {
  MOZ_ASSERT(offset >= FirstMediumAllocOffset);
  MOZ_ASSERT(offset < ChunkSize);

  size_t bit = offset / MinMediumAllocSize;
  size_t prev = allocBitmap.ref().FindPrev(bit);
  if (prev == SIZE_MAX) {
    return ChunkSize;
  }

  return prev * MinMediumAllocSize;
}

BufferAllocator::BufferAllocator(Zone* zone)
    : zone(zone),
      lock(mutexid::BufferAllocator),
      sweptMediumMixedChunks(lock),
      sweptMediumTenuredChunks(lock),
      sweptMediumNurseryFreeLists(lock),
      sweptMediumTenuredFreeLists(lock),
      sweptLargeTenuredAllocs(lock),
      minorState(State::NotCollecting),
      majorState(State::NotCollecting),
      minorSweepingFinished(lock) {}

BufferAllocator::~BufferAllocator() {
#ifdef DEBUG
  checkGCStateNotInUse();
  MOZ_ASSERT(mediumMixedChunks.ref().isEmpty());
  MOZ_ASSERT(mediumTenuredChunks.ref().isEmpty());
  mediumFreeLists.ref().assertEmpty();
  MOZ_ASSERT(largeNurseryAllocs.ref().isEmpty());
  MOZ_ASSERT(largeTenuredAllocs.ref().isEmpty());
#endif
}

bool BufferAllocator::isEmpty() const {
  MOZ_ASSERT(!zone->wasGCStarted() || zone->isGCFinished());
  MOZ_ASSERT(minorState == State::NotCollecting);
  MOZ_ASSERT(majorState == State::NotCollecting);
  return mediumMixedChunks.ref().isEmpty() &&
         mediumTenuredChunks.ref().isEmpty() &&
         largeNurseryAllocs.ref().isEmpty() &&
         largeTenuredAllocs.ref().isEmpty();
}

/* static */
bool BufferAllocator::IsSmallAllocSize(size_t bytes) {
  return mozilla::RoundUpPow2(bytes + sizeof(SmallBuffer)) < MinMediumAllocSize;
}

/* static */
bool BufferAllocator::IsLargeAllocSize(size_t bytes) {
  return RoundUp(bytes + sizeof(MediumBuffer), PageSize) >= MinLargeAllocSize;
}

/* static */
size_t BufferAllocator::GetGoodAllocSize(size_t requiredBytes) {
  requiredBytes = std::max(requiredBytes, MinAllocSize);

  if (IsLargeAllocSize(requiredBytes)) {
    size_t headerSize = sizeof(LargeBuffer);
    return RoundUp(requiredBytes + headerSize, PageSize) - headerSize;
  }

  // Small and medium headers have the same size.
  size_t headerSize = sizeof(SmallBuffer);
  static_assert(sizeof(SmallBuffer) == sizeof(MediumBuffer));

  // TODO: Support more sizes than powers of 2
  return mozilla::RoundUpPow2(requiredBytes + headerSize) - headerSize;
}

/* static */
size_t BufferAllocator::GetGoodPower2AllocSize(size_t requiredBytes) {
  requiredBytes = std::max(requiredBytes, MinAllocSize);

  size_t headerSize;
  if (IsLargeAllocSize(requiredBytes)) {
    headerSize = sizeof(LargeBuffer);
  } else {
    // Small and medium headers have the same size.
    headerSize = sizeof(SmallBuffer);
    static_assert(sizeof(SmallBuffer) == sizeof(MediumBuffer));
  }

  return mozilla::RoundUpPow2(requiredBytes + headerSize) - headerSize;
}

/* static */
size_t BufferAllocator::GetGoodElementCount(size_t requiredElements,
                                            size_t elementSize) {
  size_t requiredBytes = requiredElements * elementSize;
  size_t goodSize = GetGoodAllocSize(requiredBytes);
  return goodSize / elementSize;
}

/* static */
size_t BufferAllocator::GetGoodPower2ElementCount(size_t requiredElements,
                                                  size_t elementSize) {
  size_t requiredBytes = requiredElements * elementSize;
  size_t goodSize = GetGoodPower2AllocSize(requiredBytes);
  return goodSize / elementSize;
}

void* BufferAllocator::alloc(size_t bytes, bool nurseryOwned) {
  MOZ_ASSERT_IF(zone->isGCMarkingOrSweeping(), majorState == State::Marking);

  if (IsSmallAllocSize(bytes)) {
    return allocSmall(bytes, nurseryOwned);
  }

  if (!IsLargeAllocSize(bytes)) {
    return allocMedium(bytes, nurseryOwned, false);
  }

  return allocLarge(bytes, nurseryOwned, false);
}

void* BufferAllocator::allocInGC(size_t bytes, bool nurseryOwned) {
  // Currently this is used during tenuring only.
  MOZ_ASSERT(minorState == State::Marking);

  MOZ_ASSERT_IF(zone->isGCMarkingOrSweeping(), majorState == State::Marking);

  void* result;
  if (IsSmallAllocSize(bytes)) {
    result = allocSmallInGC(bytes, nurseryOwned);
  } else if (IsLargeAllocSize(bytes)) {
    result = allocLarge(bytes, nurseryOwned, true);
  } else {
    result = allocMedium(bytes, nurseryOwned, true);
  }

  if (!result) {
    return nullptr;
  }

  // Barrier to mark nursery-owned allocations that happen during collection. We
  // don't need to do this for tenured-owned allocations because we don't sweep
  // tenured-owned allocations that happened after the start of a major
  // collection.
  if (nurseryOwned) {
    markNurseryOwnedAlloc(result, false);
  }

  return result;
}

#ifdef XP_DARWIN
static inline void VirtualCopyPages(void* dst, const void* src, size_t bytes) {
  MOZ_ASSERT((uintptr_t(dst) & PageMask) == 0);
  MOZ_ASSERT((uintptr_t(src) & PageMask) == 0);
  MOZ_ASSERT(bytes >= ChunkSize);

  kern_return_t r = vm_copy(mach_task_self(), vm_address_t(src),
                            vm_size_t(bytes), vm_address_t(dst));
  if (r != KERN_SUCCESS) {
    MOZ_CRASH("vm_copy() failed");
  }
}
#endif

void* BufferAllocator::realloc(void* ptr, size_t bytes, bool nurseryOwned) {
  // Reallocate a buffer. This has the same semantics as standard libarary
  // realloc: if |ptr| is null it creates a new allocation, and if it fails it
  // returns |nullptr| and the original |ptr| is still valid.

  if (!ptr) {
    return alloc(bytes, nurseryOwned);
  }

  MOZ_ASSERT(GetAllocZone(ptr) == zone);
  MOZ_ASSERT(IsNurseryOwned(ptr) == nurseryOwned);
  MOZ_ASSERT_IF(zone->isGCMarkingOrSweeping(), majorState == State::Marking);

  size_t currentBytes = GetAllocSize(ptr);
  bytes = GetGoodAllocSize(bytes);
  if (bytes == currentBytes) {
    return ptr;
  }

  if (bytes > currentBytes) {
    // Can only grow medium allocations.
    if (IsMediumAlloc(ptr) && !IsLargeAllocSize(bytes)) {
      if (growMedium(ptr, bytes)) {
        return ptr;
      }
    }
  } else {
    // Can shrink medium or large allocations.
    if (IsMediumAlloc(ptr) && !IsSmallAllocSize(bytes)) {
      if (shrinkMedium(ptr, bytes)) {
        return ptr;
      }
    }
    if (IsLargeAlloc(ptr) && IsLargeAllocSize(bytes)) {
      if (shrinkLarge(ptr, bytes)) {
        return ptr;
      }
    }
  }

  void* newPtr = alloc(bytes, nurseryOwned);
  if (!newPtr) {
    return nullptr;
  }

  auto freeGuard = mozilla::MakeScopeExit([&]() { free(ptr); });

  size_t bytesToCopy = std::min(bytes, currentBytes);

#ifdef XP_DARWIN
  if (bytesToCopy >= ChunkSize) {
    MOZ_ASSERT((uintptr_t(ptr) & PageMask) == (uintptr_t(newPtr) & PageMask));
    size_t alignBytes = PageSize - (uintptr_t(ptr) & PageMask);
    memcpy(newPtr, ptr, alignBytes);
    void* dst = reinterpret_cast<void*>(uintptr_t(newPtr) + alignBytes);
    void* src = reinterpret_cast<void*>(uintptr_t(ptr) + alignBytes);
    bytesToCopy -= alignBytes;
    VirtualCopyPages(dst, src, bytesToCopy);
    return newPtr;
  }
#endif

  memcpy(newPtr, ptr, bytesToCopy);
  return newPtr;
}

template <typename HeaderT>
static HeaderT* GetHeaderFromAlloc(void* alloc) {
  auto* header = reinterpret_cast<HeaderT*>(uintptr_t(alloc) - sizeof(HeaderT));
  header->check();
  return header;
}

void BufferAllocator::free(void* ptr) {
  MOZ_ASSERT(ptr);
  MOZ_ASSERT(GetAllocZone(ptr) == zone);

  DebugOnlyPoison(ptr, JS_FREED_BUFFER_PATTERN, GetAllocSize(ptr),
                  MemCheckKind::MakeUndefined);

  if (IsLargeAlloc(ptr)) {
    freeLarge(ptr);
    return;
  }

  if (IsMediumAlloc(ptr)) {
    freeMedium(ptr);
    return;
  }

  // Can't free small allocations.
}

/* static */
bool BufferAllocator::IsBufferAlloc(void* alloc) {
  // Precondition: |alloc| is a pointer to a buffer allocation, a GC thing or a
  // direct nursery allocation returned by Nursery::allocateBuffer.

  ChunkKind chunkKind = detail::GetGCAddressChunkBase(alloc)->getKind();
  if (chunkKind == ChunkKind::MediumBuffers ||
      chunkKind == ChunkKind::LargeBuffer) {
    return true;
  }

  if (chunkKind == ChunkKind::TenuredArenas) {
    auto* arena = reinterpret_cast<Arena*>(uintptr_t(alloc) & ~ArenaMask);
    return IsBufferAllocKind(arena->getAllocKind());
  }

  return false;
}

/* static */
size_t BufferAllocator::GetAllocSize(void* alloc) {
  if (IsLargeAlloc(alloc)) {
    auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
    return header->bytesIncludingHeader() - sizeof(LargeBuffer);
  }

  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->arena()->getThingSize() - sizeof(SmallBuffer);
  }

  MOZ_ASSERT(IsMediumAlloc(alloc));
  auto* header = GetHeaderFromAlloc<MediumBuffer>(alloc);
  return SizeClassBytes(header->sizeClass) - sizeof(MediumBuffer);
}

/* static */
JS::Zone* BufferAllocator::GetAllocZone(void* alloc) {
  if (IsLargeAlloc(alloc)) {
    auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
    return header->zone();
  }

  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->zone();
  }

  return BufferChunk::from(alloc)->zone;
}

/* static */
bool BufferAllocator::IsNurseryOwned(void* alloc) {
  if (IsLargeAlloc(alloc)) {
    auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
    return header->isNurseryOwned();
  }

  if (IsSmallAlloc(alloc)) {
    // This is always false because we currently make such allocations directly
    // in the nursery.
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->isNurseryOwned();
  }

  return GetHeaderFromAlloc<MediumBuffer>(alloc)->isNurseryOwned;
}

void BufferAllocator::markNurseryOwnedAlloc(void* alloc, bool ownerWasTenured) {
  MOZ_ASSERT(alloc);
  MOZ_ASSERT(IsNurseryOwned(alloc));
  MOZ_ASSERT(GetAllocZone(alloc) == zone);
  MOZ_ASSERT(minorState == State::Marking);

  if (IsSmallAlloc(alloc)) {
    // This path is currently unused outside test code because we allocate
    // nursery buffers directly in the nursery rather than using this allocator.
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    if (ownerWasTenured) {
      cell->setNurseryOwned(false);
    }
    // Heap size tracked as part of GC heap for small allocations.
    return;
  }

  if (IsLargeAlloc(alloc)) {
    auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
    largeNurseryAllocs.ref().remove(header);
    if (ownerWasTenured) {
      header->setNurseryOwned(false);
      header->setAllocatedDuringCollection(majorState != State::NotCollecting);
      largeTenuredAllocs.ref().pushBack(header);
      size_t usableSize = header->bytesIncludingHeader() - sizeof(LargeBuffer);
      updateHeapSize(usableSize, false, false);
    } else {
      sweptLargeNurseryAllocs.ref().pushBack(header);
    }
    return;
  }

  MOZ_ASSERT(IsMediumAlloc(alloc));
  auto* header = GetHeaderFromAlloc<MediumBuffer>(alloc);
  MOZ_ASSERT(BufferChunk::from(alloc)->hasNurseryOwnedAllocs);
  if (ownerWasTenured) {
    header->isNurseryOwned = false;
    size_t usableSize =
        SizeClassBytes(header->sizeClass) - sizeof(MediumBuffer);
    updateHeapSize(usableSize, false, false);
  } else {
    BufferChunk* chunk = BufferChunk::from(alloc);
    MOZ_ASSERT(chunk->isAllocated(alloc));
    chunk->markBits.ref().markIfUnmarked(alloc, MarkColor::Black);
  }
}

/* static */
bool BufferAllocator::IsMarkedBlack(void* alloc) {
  if (IsLargeAlloc(alloc)) {
    return IsLargeAllocMarked(alloc);
  }

  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    MOZ_ASSERT(!cell->isMarkedGray());
    return cell->isMarkedBlack();
  }

  MOZ_ASSERT(IsMediumAlloc(alloc));
  BufferChunk* chunk = BufferChunk::from(alloc);
  MOZ_ASSERT(chunk->isAllocated(alloc));
  return chunk->markBits.ref().isMarkedBlack(alloc);
}

/* static */
void BufferAllocator::TraceEdge(JSTracer* trc, Cell* owner, void* buffer,
                                const char* name) {
  // Buffers are conceptually part of the owning cell and are not reported to
  // the tracer.

  // TODO: This should be unified with the rest of the tracing system.

  MOZ_ASSERT(owner);
  MOZ_ASSERT(buffer);

  if (js::gc::detail::GetGCAddressChunkBase(buffer)->isNurseryChunk()) {
    // JSObject slots and elements can be allocated in the nursery and this is
    // handled separately.
    return;
  }

  MOZ_ASSERT(IsBufferAlloc(buffer));

  if (IsSmallAlloc(buffer)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(buffer);
    TraceManuallyBarrieredEdge(trc, &cell, name);
    MOZ_ASSERT(cell->data() == buffer);  // TODO: Compact small buffers.
    return;
  }

  if (trc->isTenuringTracer()) {
    if (IsNurseryOwned(buffer)) {
      Zone* zone = owner->zone();
      zone->bufferAllocator.markNurseryOwnedAlloc(buffer, owner->isTenured());
    }
    return;
  }

  if (trc->isMarkingTracer()) {
    MOZ_ASSERT(!ChunkPtrIsInsideNursery(buffer));
    MarkTenuredAlloc(buffer);
    return;
  }
}

/* static */
bool BufferAllocator::MarkTenuredAlloc(void* alloc) {
  MOZ_ASSERT(alloc);
  MOZ_ASSERT(!IsNurseryOwned(alloc));

  if (IsLargeAlloc(alloc)) {
    return MarkLargeAlloc(alloc);
  }

  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->markIfUnmarkedAtomic(MarkColor::Black);
  }

  BufferChunk* chunk = BufferChunk::from(alloc);
  MOZ_ASSERT(chunk->isAllocated(alloc));
  if (chunk->allocatedDuringCollection) {
    // Will not be swept, already counted as marked.
    return false;
  }

  return chunk->markBits.ref().markIfUnmarkedAtomic(alloc, MarkColor::Black);
}

#ifdef DEBUG
template <typename T>
/* static */
bool BufferAllocator::listIsEmpty(const MutexData<SlimLinkedList<T>>& list) {
  AutoLockAllocator lock(this);
  return list.ref().isEmpty();
}
#endif

void BufferAllocator::startMinorCollection() {
  maybeMergeSweptData();

#ifdef DEBUG
  MOZ_ASSERT(minorState == State::NotCollecting);
  if (majorState == State::NotCollecting) {
    checkGCStateNotInUse();
  } else {
    // Large allocations that are marked when tracing the nursery will be moved
    // to this list.
    MOZ_ASSERT(sweptLargeNurseryAllocs.ref().isEmpty());
  }
#endif

  minorState = State::Marking;
}

bool BufferAllocator::startMinorSweeping(LargeAllocList& largeAllocsToFree) {
  // Called during minor GC. Operates on the active allocs/chunks lists. The 'to
  // sweep' lists do not contain nursery owned allocations.

#ifdef DEBUG
  MOZ_ASSERT(minorState == State::Marking);
  {
    AutoLockAllocator lock(this);
    MOZ_ASSERT(!minorSweepingFinished);
    MOZ_ASSERT(sweptMediumMixedChunks.ref().isEmpty());
  }
  for (LargeBuffer* header : largeNurseryAllocs.ref()) {
    MOZ_ASSERT(header->isNurseryOwned());
    MOZ_ASSERT(!header->isMarked());
  }
  for (LargeBuffer* header : sweptLargeNurseryAllocs.ref()) {
    MOZ_ASSERT(header->isNurseryOwned());
    MOZ_ASSERT(!header->isMarked());
  }
#endif

  // Large nursery allocations are moved out of |largeNurseryAllocs| when they
  // are marked, so any remaining are ready to be freed. Move them to the output
  // list.
  largeAllocsToFree.append(std::move(largeNurseryAllocs.ref()));
  MOZ_ASSERT(largeNurseryAllocs.ref().isEmpty());
  largeNurseryAllocs.ref() = std::move(sweptLargeNurseryAllocs.ref());

  // Check whether there are any medium chunks containing nursery owned
  // allocations that need to be swept.
  if (mediumMixedChunks.ref().isEmpty()) {
    // Nothing more to do. Don't transition to sweeping state.
    minorState = State::NotCollecting;
    return false;
  }

  // TODO: There are more efficient ways to remove the free regions in nursery
  // chunks from the free lists, but all require some more bookkeeping. I don't
  // know how much difference such a change would make.
  //
  // Some possibilities are:
  //  - maintain a separate list of free regions in each chunk and use that to
  //    remove those regions in nursery chunks
  //  - have separate free lists for nursery/tenured chunks
  //  - keep free regions at different ends of the free list depending on chunk
  //    kind
  mediumFreeLists.ref().eraseIf([](FreeRegion* region) {
    return BufferChunk::from(region)->hasNurseryOwnedAllocs;
  });

  mediumMixedChunksToSweep.ref() = std::move(mediumMixedChunks.ref());

  minorState = State::Sweeping;

  return true;
}

void BufferAllocator::sweepForMinorCollection() {
  // Called on a background thread.

  MOZ_ASSERT(minorState.refNoCheck() == State::Sweeping);

  MOZ_ASSERT(listIsEmpty(sweptMediumMixedChunks));
  while (!mediumMixedChunksToSweep.ref().isEmpty()) {
    BufferChunk* chunk = mediumMixedChunksToSweep.ref().popFirst();
    FreeLists sweptFreeLists;
    if (sweepChunk(chunk, OwnerKind::Nursery, false, sweptFreeLists)) {
      {
        AutoLockAllocator lock(this);
        sweptMediumMixedChunks.ref().pushBack(chunk);
        if (chunk->hasNurseryOwnedAllocsAfterSweep) {
          sweptMediumNurseryFreeLists.ref().append(std::move(sweptFreeLists));
        } else {
          sweptMediumTenuredFreeLists.ref().append(std::move(sweptFreeLists));
        }
      }

      // Signal to the main thread that swept data is available by setting this
      // relaxed atomic flag.
      sweptChunksAvailable = true;
    }
  }

  // Signal to main thread to update minorState.
  AutoLockAllocator lock(this);
  MOZ_ASSERT(!minorSweepingFinished);
  minorSweepingFinished = true;
}

/* static */
void BufferAllocator::FreeLargeAllocs(LargeAllocList& largeAllocsToFree) {
  while (!largeAllocsToFree.isEmpty()) {
    LargeBuffer* header = largeAllocsToFree.popFirst();
    header->zone()->bufferAllocator.unmapLarge(header, true);
  }
}

void BufferAllocator::startMajorCollection() {
  maybeMergeSweptData();

#ifdef DEBUG
  MOZ_ASSERT(majorState == State::NotCollecting);
  checkGCStateNotInUse();

  // Everything is tenured since we just evicted the nursery, or will be by the
  // time minor sweeping finishes.
  MOZ_ASSERT(mediumMixedChunks.ref().isEmpty());
  MOZ_ASSERT(largeNurseryAllocs.ref().isEmpty());
#endif

  mediumTenuredChunksToSweep.ref() = std::move(mediumTenuredChunks.ref());
  largeTenuredAllocsToSweep.ref() = std::move(largeTenuredAllocs.ref());

  // Clear the active free lists to prevent further allocation in chunks that
  // will be swept.
  mediumFreeLists.ref().clear();

  if (minorState == State::Sweeping) {
    // Ensure swept nursery chunks are moved to the mediumTenuredChunks lists in
    // mergeSweptData.
    majorStartedWhileMinorSweeping = true;
  }

#ifdef DEBUG
  MOZ_ASSERT(mediumTenuredChunks.ref().isEmpty());
  mediumFreeLists.ref().assertEmpty();
  MOZ_ASSERT(largeTenuredAllocs.ref().isEmpty());
#endif

  majorState = State::Marking;
}

void BufferAllocator::startMajorSweeping() {
  // Called when a zone transitions from marking to sweeping.

#ifdef DEBUG
  MOZ_ASSERT(majorState == State::Marking);
  MOZ_ASSERT(zone->isGCFinished());
#endif

  maybeMergeSweptData();
  MOZ_ASSERT(!majorStartedWhileMinorSweeping);

  majorState = State::Sweeping;
}

void BufferAllocator::sweepForMajorCollection(bool shouldDecommit) {
  // Called on a background thread.

  MOZ_ASSERT(majorState.refNoCheck() == State::Sweeping);

  while (!mediumTenuredChunksToSweep.ref().isEmpty()) {
    BufferChunk* chunk = mediumTenuredChunksToSweep.ref().popFirst();
    FreeLists sweptFreeLists;
    if (sweepChunk(chunk, OwnerKind::Tenured, shouldDecommit, sweptFreeLists)) {
      {
        AutoLockAllocator lock(this);
        sweptMediumTenuredChunks.ref().pushBack(chunk);
        sweptMediumTenuredFreeLists.ref().append(std::move(sweptFreeLists));
      }

      // Signal to the main thread that swept data is available by setting this
      // relaxed atomic flag.
      sweptChunksAvailable = true;
    }
  }

  // It's tempting to try and optimize this by moving the allocations between
  // lists when they are marked, in the same way as for nursery sweeping. This
  // would require synchronizing the list modification when marking in parallel,
  // so is probably not worth it.
  LargeAllocList sweptList;
  while (!largeTenuredAllocsToSweep.ref().isEmpty()) {
    LargeBuffer* header = largeTenuredAllocsToSweep.ref().popFirst();
    if (sweepLargeTenured(header)) {
      sweptList.pushBack(header);
    }
  }

  // It would be possible to add these to the output list as we sweep but
  // there's currently no advantage to that.
  AutoLockAllocator lock(this);
  sweptLargeTenuredAllocs.ref() = std::move(sweptList);
}

static void ClearAllocatedDuringCollection(SlimLinkedList<BufferChunk>& list) {
  for (auto* buffer : list) {
    buffer->allocatedDuringCollection = false;
  }
}
static void ClearAllocatedDuringCollection(SlimLinkedList<LargeBuffer>& list) {
  for (auto* element : list) {
    element->setAllocatedDuringCollection(false);
  }
}

void BufferAllocator::finishMajorCollection() {
  maybeMergeSweptData();

#ifdef DEBUG
  // This can be called without startMajorSweeping if collection is aborted.
  MOZ_ASSERT(majorState == State::Marking || majorState == State::Sweeping);
  MOZ_ASSERT_IF(majorState == State::Marking,
                listIsEmpty(sweptMediumTenuredChunks));
  MOZ_ASSERT_IF(majorState == State::Sweeping,
                mediumTenuredChunksToSweep.ref().isEmpty());
#endif

  // TODO: It may be more efficient if we can clear this flag before merging
  // swept data above.
  ClearAllocatedDuringCollection(mediumMixedChunks.ref());
  ClearAllocatedDuringCollection(mediumTenuredChunks.ref());
  // This flag is not set for large nursery-owned allocations.
  ClearAllocatedDuringCollection(largeTenuredAllocs.ref());
  if (minorState == State::Sweeping) {
    // Ensure this flag is cleared when chunks are merged in mergeSweptData.
    majorFinishedWhileMinorSweeping = true;
  }

  if (majorState == State::Marking) {
    // We have aborted collection without sweeping this zone. Restore or rebuild
    // the original state.

    for (BufferChunk* chunk : mediumTenuredChunksToSweep.ref()) {
      chunk->markBits.ref().clear();
    }
    for (LargeBuffer* alloc : largeTenuredAllocsToSweep.ref()) {
      alloc->clearMarked();
    }

    // Rebuild free lists for chunks we didn't end up sweeping.
    for (BufferChunk* chunk : mediumTenuredChunksToSweep.ref()) {
      MOZ_ALWAYS_TRUE(
          sweepChunk(chunk, OwnerKind::None, false, mediumFreeLists.ref()));
    }

    mediumTenuredChunks.ref().prepend(
        std::move(mediumTenuredChunksToSweep.ref()));
    largeTenuredAllocs.ref().prepend(
        std::move(largeTenuredAllocsToSweep.ref()));
  }

  majorState = State::NotCollecting;

#ifdef DEBUG
  checkGCStateNotInUse();
#endif
}

void BufferAllocator::maybeMergeSweptData() {
  if (minorState == State::Sweeping || majorState == State::Sweeping) {
    mergeSweptData();
  }
}

void BufferAllocator::mergeSweptData() {
  MOZ_ASSERT(minorState == State::Sweeping || majorState == State::Sweeping);

  AutoLockAllocator lock(this);

  // Merge swept chunks that previously contained nursery owned allocations. If
  // semispace nursery collection is in use then these chunks may contain both
  // nursery and tenured-owned allocations, otherwise all allocations will be
  // tenured-owned.
  while (!sweptMediumMixedChunks.ref().isEmpty()) {
    BufferChunk* chunk = sweptMediumMixedChunks.ref().popLast();
    MOZ_ASSERT(chunk->hasNurseryOwnedAllocs);
    chunk->hasNurseryOwnedAllocs = chunk->hasNurseryOwnedAllocsAfterSweep;

    MOZ_ASSERT_IF(
        majorState == State::NotCollecting && !majorFinishedWhileMinorSweeping,
        !chunk->allocatedDuringCollection);
    if (majorFinishedWhileMinorSweeping) {
      chunk->allocatedDuringCollection = false;
    }

    if (chunk->hasNurseryOwnedAllocs) {
      mediumMixedChunks.ref().pushFront(chunk);
    } else if (majorStartedWhileMinorSweeping) {
      mediumTenuredChunksToSweep.ref().pushFront(chunk);
    } else {
      mediumTenuredChunks.ref().pushFront(chunk);
    }
  }

  // Merge swept chunks that did not contain nursery owned allocations.
#ifdef DEBUG
  for (BufferChunk* chunk : sweptMediumTenuredChunks.ref()) {
    MOZ_ASSERT(!chunk->hasNurseryOwnedAllocs);
    MOZ_ASSERT(!chunk->hasNurseryOwnedAllocsAfterSweep);
  }
#endif
  mediumTenuredChunks.ref().prepend(std::move(sweptMediumTenuredChunks.ref()));

  mediumFreeLists.ref().prepend(std::move(sweptMediumNurseryFreeLists.ref()));
  if (!majorStartedWhileMinorSweeping) {
    mediumFreeLists.ref().prepend(std::move(sweptMediumTenuredFreeLists.ref()));
  } else {
    sweptMediumTenuredFreeLists.ref().clear();
  }

  largeTenuredAllocs.ref().prepend(std::move(sweptLargeTenuredAllocs.ref()));

  sweptChunksAvailable = false;

  if (minorSweepingFinished) {
    MOZ_ASSERT(minorState == State::Sweeping);
    minorState = State::NotCollecting;
    minorSweepingFinished = false;
    majorStartedWhileMinorSweeping = false;
    majorFinishedWhileMinorSweeping = false;

#ifdef DEBUG
    if (majorState == State::NotCollecting) {
      checkGCStateNotInUse(lock);
    } else {
      for (BufferChunk* chunk : mediumMixedChunks.ref()) {
        verifyChunk(chunk, true);
      }
      for (BufferChunk* chunk : mediumTenuredChunks.ref()) {
        verifyChunk(chunk, false);
      }
    }
#endif
  }
}

bool BufferAllocator::isPointerWithinMediumOrLargeBuffer(void* ptr) {
  maybeMergeSweptData();

  for (const auto* chunks :
       {&mediumMixedChunks.ref(), &mediumTenuredChunks.ref()}) {
    for (auto* chunk : *chunks) {
      if (chunk->isPointerWithinAllocation(ptr)) {
        return true;
      }
    }
  }

  if (majorState == State::Marking) {
    for (auto* chunk : mediumTenuredChunksToSweep.ref()) {
      if (chunk->isPointerWithinAllocation(ptr)) {
        return true;
      }
    }
  }

  // Note we cannot safely access data that is being swept on another thread.

  for (const auto* allocs :
       {&largeNurseryAllocs.ref(), &largeTenuredAllocs.ref()}) {
    for (auto* alloc : *allocs) {
      if (alloc->isPointerWithinAllocation(ptr)) {
        return true;
      }
    }
  }

  return false;
}

bool BufferChunk::isPointerWithinAllocation(void* ptr) const {
  uintptr_t offset = uintptr_t(ptr) - uintptr_t(this);
  if (offset >= ChunkSize || offset < FirstMediumAllocOffset) {
    return false;
  }

  uintptr_t allocOffset = findPrevAllocated(offset);
  if (allocOffset == ChunkSize) {
    return false;
  }

  auto* header =
      MediumBuffer::from(const_cast<BufferChunk*>(this), allocOffset);

  return offset < allocOffset + header->bytesIncludingHeader();
}

bool LargeBuffer::isPointerWithinAllocation(void* ptr) const {
  return uintptr_t(ptr) - uintptr_t(this) < bytesIncludingHeader();
}

#ifdef DEBUG

void BufferAllocator::checkGCStateNotInUse() {
  AutoLockAllocator lock(this);  // Some fields are protected by this lock.
  checkGCStateNotInUse(lock);
}

void BufferAllocator::checkGCStateNotInUse(const AutoLockAllocator& lock) {
  MOZ_ASSERT(majorState == State::NotCollecting);
  bool isNurserySweeping = minorState == State::Sweeping;

  checkChunkListGCStateNotInUse(mediumMixedChunks.ref(), true, false);
  checkChunkListGCStateNotInUse(mediumTenuredChunks.ref(), false, false);

  if (isNurserySweeping) {
    checkChunkListGCStateNotInUse(sweptMediumMixedChunks.ref(), true,
                                  majorFinishedWhileMinorSweeping);
    checkChunkListGCStateNotInUse(sweptMediumTenuredChunks.ref(), false, false);
  } else {
    MOZ_ASSERT(mediumMixedChunksToSweep.ref().isEmpty());

    MOZ_ASSERT(sweptMediumMixedChunks.ref().isEmpty());
    MOZ_ASSERT(sweptMediumTenuredChunks.ref().isEmpty());
    sweptMediumNurseryFreeLists.ref().assertEmpty();
    sweptMediumTenuredFreeLists.ref().assertEmpty();

    MOZ_ASSERT(!majorStartedWhileMinorSweeping);
    MOZ_ASSERT(!majorFinishedWhileMinorSweeping);
    MOZ_ASSERT(!sweptChunksAvailable);
    MOZ_ASSERT(!minorSweepingFinished);
  }

  MOZ_ASSERT(mediumTenuredChunksToSweep.ref().isEmpty());

  checkAllocListGCStateNotInUse(largeNurseryAllocs.ref(), true);
  checkAllocListGCStateNotInUse(largeTenuredAllocs.ref(), false);

  MOZ_ASSERT(largeTenuredAllocsToSweep.ref().isEmpty());

  MOZ_ASSERT(sweptLargeNurseryAllocs.ref().isEmpty());
  MOZ_ASSERT(sweptLargeTenuredAllocs.ref().isEmpty());
}

void BufferAllocator::checkChunkListGCStateNotInUse(
    BufferChunkList& chunks, bool hasNurseryOwnedAllocs,
    bool allowAllocatedDuringCollection) {
  for (BufferChunk* chunk : chunks) {
    checkChunkGCStateNotInUse(chunk, allowAllocatedDuringCollection);
    verifyChunk(chunk, hasNurseryOwnedAllocs);
  }
}

void BufferAllocator::checkChunkGCStateNotInUse(
    BufferChunk* chunk, bool allowAllocatedDuringCollection) {
  MOZ_ASSERT_IF(!allowAllocatedDuringCollection,
                !chunk->allocatedDuringCollection);

  static constexpr size_t StepBytes = MinMediumAllocSize;

  // Check nothing's marked.
  uintptr_t chunkAddr = uintptr_t(chunk);
  for (size_t offset = 0; offset < ChunkSize; offset += StepBytes) {
    void* alloc = reinterpret_cast<void*>(chunkAddr + offset);
    MOZ_ASSERT(!chunk->markBits.ref().isMarkedBlack(alloc));
  }
}

void BufferAllocator::verifyChunk(BufferChunk* chunk,
                                  bool hasNurseryOwnedAllocs) {
  MOZ_ASSERT(chunk->hasNurseryOwnedAllocs == hasNurseryOwnedAllocs);

  static constexpr size_t StepBytes = MinMediumAllocSize;

  size_t freeOffset = FirstMediumAllocOffset;

  for (BufferChunkIter alloc(chunk); !alloc.done(); alloc.next()) {
    // Check any free region preceding this allocation.
    size_t offset = alloc.getOffset();
    MOZ_ASSERT(offset >= FirstMediumAllocOffset);
    if (offset > freeOffset) {
      verifyFreeRegion(chunk, offset, offset - freeOffset);
    }

    // Check this allocation.
    MediumBuffer* header = alloc.get();
    MOZ_ASSERT_IF(header->isNurseryOwned, hasNurseryOwnedAllocs);
    size_t bytes = SizeClassBytes(header->sizeClass);
    uintptr_t endOffset = offset + bytes;
    MOZ_ASSERT(endOffset <= ChunkSize);
    for (size_t i = offset + StepBytes; i < endOffset; i += StepBytes) {
      MOZ_ASSERT(!chunk->isAllocated(i));
    }

    freeOffset = endOffset;
  }

  // Check any free region following the last allocation.
  if (freeOffset < ChunkSize) {
    verifyFreeRegion(chunk, ChunkSize, ChunkSize - freeOffset);
  }
}

void BufferAllocator::verifyFreeRegion(BufferChunk* chunk, uintptr_t endOffset,
                                       size_t expectedSize) {
  auto* freeRegion = FreeRegion::fromEndOffset(chunk, endOffset);
  MOZ_ASSERT(freeRegion->isInList());
  MOZ_ASSERT(freeRegion->size() == expectedSize);
}

void BufferAllocator::checkAllocListGCStateNotInUse(LargeAllocList& list,
                                                    bool isNurseryOwned) {
  for (LargeBuffer* header : list) {
    MOZ_ASSERT(header->isNurseryOwned() == isNurseryOwned);
    MOZ_ASSERT(!header->isMarked());
    MOZ_ASSERT_IF(!isNurseryOwned, !header->wasAllocatedDuringCollection());
  }
}

#endif

void* BufferAllocator::allocSmall(size_t bytes, bool nurseryOwned) {
  AllocKind kind = AllocKindForSmallAlloc(bytes);

  void* ptr = CellAllocator::AllocTenuredCellUnchecked<NoGC>(zone, kind);
  if (!ptr) {
    return nullptr;
  }

  auto* cell = new (ptr) SmallBuffer();
  cell->setNurseryOwned(nurseryOwned);
  MOZ_ASSERT(cell->isNurseryOwned() == nurseryOwned);
  void* alloc = cell->data();

  MOZ_ASSERT(IsSmallAlloc(alloc));
  MOZ_ASSERT(GetAllocSize(alloc) >= bytes);
  MOZ_ASSERT(GetAllocSize(alloc) < 2 * (bytes + sizeof(SmallBuffer)));

  return alloc;
}

/* static */
void* BufferAllocator::allocSmallInGC(size_t bytes, bool nurseryOwned) {
  AllocKind kind = AllocKindForSmallAlloc(bytes);

  void* ptr = AllocateTenuredCellInGC(zone, kind);
  if (!ptr) {
    return nullptr;
  }

  auto* cell = new (ptr) SmallBuffer();
  cell->setNurseryOwned(nurseryOwned);
  void* alloc = cell->data();

  MOZ_ASSERT(GetAllocSize(alloc) >= bytes);
  MOZ_ASSERT(GetAllocSize(alloc) < 2 * (bytes + sizeof(SmallBuffer)));

  return alloc;
}

/* static */
AllocKind BufferAllocator::AllocKindForSmallAlloc(size_t bytes) {
  bytes = std::max(bytes, MinAllocSize);

  size_t totalBytes = bytes + sizeof(SmallBuffer);
  MOZ_ASSERT(totalBytes < MinMediumAllocSize);

  size_t logBytes = mozilla::CeilingLog2(totalBytes);
  MOZ_ASSERT(totalBytes <= (size_t(1) << logBytes));

  MOZ_ASSERT(logBytes >= mozilla::CeilingLog2(MinAllocSize));
  size_t kindIndex = logBytes - mozilla::CeilingLog2(MinAllocSize);

  AllocKind kind = AllocKind(size_t(AllocKind::BUFFER_FIRST) + kindIndex);
  MOZ_ASSERT(IsValidAllocKind(kind));
  MOZ_ASSERT(kind <= AllocKind::BUFFER_LAST);

  return kind;
}

/* static */
bool BufferAllocator::IsSmallAlloc(void* alloc) {
  MOZ_ASSERT(IsBufferAlloc(alloc));

  ChunkBase* chunk = detail::GetGCAddressChunkBase(alloc);
  return chunk->getKind() == ChunkKind::TenuredArenas;
}

void* BufferAllocator::allocMedium(size_t bytes, bool nurseryOwned, bool inGC) {
  // Get size class from |bytes|.
  size_t totalBytes = mozilla::RoundUpPow2(bytes + sizeof(MediumBuffer));
  size_t sizeClass = SizeClassForAlloc(totalBytes);
  MOZ_ASSERT(SizeClassBytes(sizeClass) ==
             GetGoodAllocSize(bytes) + sizeof(MediumBuffer));

  void* ptr = bumpAllocOrRetry(sizeClass, inGC);
  if (!ptr) {
    return nullptr;
  }

  auto* header = new (ptr) MediumBuffer(sizeClass, nurseryOwned);
  void* alloc = header->data();

  BufferChunk* chunk = BufferChunk::from(ptr);
  chunk->setAllocated(alloc, true);

  if (nurseryOwned && !chunk->hasNurseryOwnedAllocs) {
    mediumTenuredChunks.ref().remove(chunk);
    chunk->hasNurseryOwnedAllocs = true;
    mediumMixedChunks.ref().pushBack(chunk);
  }

  MOZ_ASSERT(!chunk->markBits.ref().isMarkedBlack(alloc));

  if (!nurseryOwned) {
    size_t usableBytes = totalBytes - sizeof(MediumBuffer);
    bool checkThresholds = !inGC;
    updateHeapSize(usableBytes, checkThresholds, false);
  }

  return alloc;
}

void* BufferAllocator::bumpAllocOrRetry(size_t sizeClass, bool inGC) {
  void* ptr = bumpAlloc(sizeClass);
  if (ptr) {
    return ptr;
  }

  if (sweptChunksAvailable) {
    // Avoid taking the lock unless we know there is data to merge. This reduces
    // context switches.
    mergeSweptData();
    ptr = bumpAlloc(sizeClass);
    if (ptr) {
      return ptr;
    }
  }

  if (!allocNewChunk(inGC)) {
    return nullptr;
  }

  ptr = bumpAlloc(sizeClass);
  MOZ_ASSERT(ptr);
  return ptr;
}

void* BufferAllocator::bumpAlloc(size_t sizeClass) {
  size_t requestedBytes = SizeClassBytes(sizeClass);

  mediumFreeLists.ref().checkAvailable();

  // Find smallest suitable size class that has free regions.
  sizeClass = mediumFreeLists.ref().getFirstAvailableSizeClass(sizeClass);
  if (sizeClass == SIZE_MAX) {
    return nullptr;
  }

  FreeRegion* region = mediumFreeLists.ref().getFirstRegion(sizeClass);
  void* ptr = allocFromRegion(region, requestedBytes, sizeClass);
  updateFreeListsAfterAlloc(&mediumFreeLists.ref(), region, sizeClass);
  return ptr;
}

void* BufferAllocator::allocFromRegion(FreeRegion* region,
                                       size_t requestedBytes,
                                       size_t sizeClass) {
  uintptr_t start = region->startAddr;
  MOZ_ASSERT(region->getEnd() > start);
  MOZ_ASSERT(region->size() >= SizeClassBytes(sizeClass));
  MOZ_ASSERT((region->size() % MinMediumAllocSize) == 0);

  // Ensure whole region is commited.
  if (region->hasDecommittedPages) {
    recommitRegion(region);
  }

  // Allocate from start of region.
  void* ptr = reinterpret_cast<void*>(start);
  start += requestedBytes;
  MOZ_ASSERT(region->getEnd() >= start);

  // Update region start.
  region->startAddr = start;

  return ptr;
}

void BufferAllocator::updateFreeListsAfterAlloc(FreeLists* freeLists,
                                                FreeRegion* region,
                                                size_t sizeClass) {
  // Updates |freeLists| after an allocation of class |sizeClass| from |region|.

  freeLists->assertContains(sizeClass, region);

  // If the region is still valid for further allocations of this size class
  // then there's nothing to do.
  size_t classBytes = SizeClassBytes(sizeClass);
  size_t newSize = region->size();
  MOZ_ASSERT((newSize % MinMediumAllocSize) == 0);
  if (newSize >= classBytes) {
    return;
  }

  // Remove region from this free list.
  freeLists->remove(sizeClass, region);

  // If the region is now empty then we're done.
  if (newSize == 0) {
    return;
  }

  // Otherwise region is now too small. Move it to the appropriate bucket for
  // its reduced size.
  size_t newSizeClass = SizeClassForFreeRegion(newSize);
  MOZ_ASSERT(newSize >= SizeClassBytes(newSizeClass));
  MOZ_ASSERT(newSizeClass < sizeClass);
  freeLists->pushFront(newSizeClass, region);
}

void BufferAllocator::recommitRegion(FreeRegion* region) {
  MOZ_ASSERT(region->hasDecommittedPages);

  BufferChunk* chunk = BufferChunk::from(region);
  uintptr_t startAddr = RoundUp(region->startAddr, PageSize);
  uintptr_t endAddr = RoundDown(uintptr_t(region), PageSize);

  size_t startPage = (startAddr - uintptr_t(chunk)) / PageSize;
  size_t endPage = (endAddr - uintptr_t(chunk)) / PageSize;

  // If the start of the region does not lie on a page boundary the page it is
  // in should be committed as it must either contain the start of the chunk, a
  // FreeRegion or an allocation.
  MOZ_ASSERT_IF((region->startAddr % PageSize) != 0,
                !chunk->decommittedPages.ref()[startPage - 1]);

  // The end of the region should be committed as it holds FreeRegion |region|.
  MOZ_ASSERT(!chunk->decommittedPages.ref()[endPage]);

  MarkPagesInUseSoft(reinterpret_cast<void*>(startAddr), endAddr - startAddr);
  for (size_t i = startPage; i != endPage; i++) {
    chunk->decommittedPages.ref()[i] = false;
  }

  region->hasDecommittedPages = false;
}

static inline StallAndRetry ShouldStallAndRetry(bool inGC) {
  return inGC ? StallAndRetry::Yes : StallAndRetry::No;
}

bool BufferAllocator::allocNewChunk(bool inGC) {
  GCRuntime* gc = &zone->runtimeFromMainThread()->gc;
  AutoLockGCBgAlloc lock(gc);
  ArenaChunk* baseChunk = gc->takeOrAllocChunk(ShouldStallAndRetry(inGC), lock);
  if (!baseChunk) {
    return false;
  }

  CheckHighBitsOfPointer(baseChunk);

  // Ensure memory initially committed.
  if (!baseChunk->decommittedPages.IsEmpty()) {
    MarkPagesInUseSoft(baseChunk, ChunkSize);
  }

  // Unpoison past the ChunkBase header.
  void* ptr = reinterpret_cast<void*>(uintptr_t(baseChunk) + sizeof(ChunkBase));
  size_t size = ChunkSize - sizeof(ChunkBase);
  SetMemCheckKind(ptr, size, MemCheckKind::MakeUndefined);

  BufferChunk* chunk = new (baseChunk) BufferChunk(zone);
  chunk->allocatedDuringCollection = majorState != State::NotCollecting;

  mediumTenuredChunks.ref().pushBack(chunk);

  uintptr_t freeStart = uintptr_t(chunk) + FirstMediumAllocOffset;
  uintptr_t freeEnd = uintptr_t(chunk) + ChunkSize;

  size_t sizeClass = SizeClassForFreeRegion(freeEnd - freeStart);

  ptr = reinterpret_cast<void*>(freeEnd - sizeof(FreeRegion));
  FreeRegion* region = new (ptr) FreeRegion(freeStart);
  MOZ_ASSERT(region->getEnd() == freeEnd);
  mediumFreeLists.ref().pushFront(sizeClass, region);

  return true;
}

bool BufferAllocator::sweepChunk(BufferChunk* chunk, OwnerKind ownerKindToSweep,
                                 bool shouldDecommit, FreeLists& freeLists) {
  // Find all regions of free space in |chunk| and add them to the swept free
  // lists.

  // TODO: Ideally we'd arrange things so we allocate from most-full chunks
  // first. This could happen by sweeping all chunks and then sorting them by
  // how much free space they had and then adding their free regions to the free
  // lists in that order.

  GCRuntime* gc = &zone->runtimeFromAnyThread()->gc;

  bool hasNurseryOwnedAllocs = false;

  size_t freeStart = FirstMediumAllocOffset;
  bool sweptAny = false;
  size_t mallocHeapBytesFreed = 0;

  for (BufferChunkIter alloc(chunk); !alloc.done(); alloc.next()) {
    MediumBuffer* header = alloc.get();

    size_t bytes = header->bytesIncludingHeader();
    uintptr_t allocEnd = alloc.getOffset() + bytes;

    OwnerKind ownerKind = OwnerKind(uint8_t(header->isNurseryOwned));
    MOZ_ASSERT_IF(header->isNurseryOwned, ownerKind == OwnerKind::Nursery);
    MOZ_ASSERT_IF(!header->isNurseryOwned, ownerKind == OwnerKind::Tenured);
    bool canSweep = ownerKind == ownerKindToSweep;
    bool shouldSweep = canSweep && !chunk->markBits.ref().isMarkedBlack(header);

    if (shouldSweep) {
      // Dead. Update allocated bitmap and heap size accounting.
      chunk->setAllocated(header, false);
      if (!header->isNurseryOwned) {
        size_t usableBytes = bytes - sizeof(MediumBuffer);
        mallocHeapBytesFreed += usableBytes;
      }
      PoisonAlloc(header, JS_SWEPT_TENURED_PATTERN, bytes,
                  MemCheckKind::MakeUndefined);
      sweptAny = true;
    } else {
      // Alive. Add any free space before this allocation.
      uintptr_t allocStart = alloc.getOffset();
      if (freeStart != allocStart) {
        addSweptRegion(chunk, freeStart, allocStart, shouldDecommit, !sweptAny,
                       freeLists);
      }
      freeStart = allocEnd;
      if (canSweep) {
        chunk->markBits.ref().unmarkOneBit(header, ColorBit::BlackBit);
      }
      if (header->isNurseryOwned) {
        MOZ_ASSERT(ownerKindToSweep == OwnerKind::Nursery);
        hasNurseryOwnedAllocs = true;
      }
      sweptAny = false;
    }
  }

  if (mallocHeapBytesFreed) {
    zone->mallocHeapSize.removeBytes(mallocHeapBytesFreed, true);
  }

  if (freeStart == FirstMediumAllocOffset &&
      ownerKindToSweep != OwnerKind::None) {
    // Chunk is empty. Give it back to the system.
    bool allMemoryCommitted = chunk->decommittedPages.ref().IsEmpty();
    chunk->~BufferChunk();
    ArenaChunk* tenuredChunk =
        ArenaChunk::emplace(chunk, gc, allMemoryCommitted);
    AutoLockGC lock(gc);
    gc->recycleChunk(tenuredChunk, lock);
    return false;
  }

  // Add any free space from the last allocation to the end of the chunk.
  if (freeStart != ChunkSize) {
    addSweptRegion(chunk, freeStart, ChunkSize, shouldDecommit, !sweptAny,
                   freeLists);
  }

  chunk->hasNurseryOwnedAllocsAfterSweep = hasNurseryOwnedAllocs;

  return true;
}

void BufferAllocator::addSweptRegion(BufferChunk* chunk, uintptr_t freeStart,
                                     uintptr_t freeEnd, bool shouldDecommit,
                                     bool expectUnchanged,
                                     FreeLists& freeLists) {
  // Add the region from |freeStart| to |freeEnd| to the appropriate swept free
  // list based on its size.

  MOZ_ASSERT(freeStart >= FirstMediumAllocOffset);
  MOZ_ASSERT(freeStart < freeEnd);
  MOZ_ASSERT(freeEnd <= ChunkSize);
  MOZ_ASSERT((freeStart % MinMediumAllocSize) == 0);
  MOZ_ASSERT((freeEnd % MinMediumAllocSize) == 0);

  // Decommit pages if |shouldDecommit| was specified, but leave space for
  // the FreeRegion structure at the end.
  bool anyDecommitted = false;
  uintptr_t decommitStart = RoundUp(freeStart, PageSize);
  uintptr_t decommitEnd = RoundDown(freeEnd - sizeof(FreeRegion), PageSize);
  size_t endPage = decommitEnd / PageSize;
  if (shouldDecommit && decommitEnd > decommitStart) {
    void* ptr = reinterpret_cast<void*>(decommitStart + uintptr_t(chunk));
    MarkPagesUnusedSoft(ptr, decommitEnd - decommitStart);
    size_t startPage = decommitStart / PageSize;
    for (size_t i = startPage; i != endPage; i++) {
      chunk->decommittedPages.ref()[i] = true;
    }
    anyDecommitted = true;
  } else {
    // Check for any previously decommitted pages.
    uintptr_t startPage = RoundDown(freeStart, PageSize) / PageSize;
    for (size_t i = startPage; i != endPage; i++) {
      if (chunk->decommittedPages.ref()[i]) {
        anyDecommitted = true;
      }
    }
  }

  // Ensure last page is committed.
  if (chunk->decommittedPages.ref()[endPage]) {
    void* ptr = reinterpret_cast<void*>(decommitEnd + uintptr_t(chunk));
    MarkPagesInUseSoft(ptr, PageSize);
    chunk->decommittedPages.ref()[endPage] = false;
  }

  freeStart += uintptr_t(chunk);
  freeEnd += uintptr_t(chunk);

  size_t sizeClass = SizeClassForFreeRegion(freeEnd - freeStart);
  addFreeRegion(&freeLists, sizeClass, freeStart, freeEnd, anyDecommitted,
                ListPosition::Back, expectUnchanged);
}

void BufferAllocator::freeMedium(void* alloc) {
  // Free a medium sized allocation. This coalesces the free space with any
  // neighboring free regions. Coalescing is necessary for resize to work
  // properly.

  BufferChunk* chunk = BufferChunk::from(alloc);
  if (isSweepingChunk(chunk)) {
    return;  // We can't free if the chunk is currently being swept.
  }

  auto* header = GetHeaderFromAlloc<MediumBuffer>(alloc);

  // Set region as not allocated and then clear mark bit.
  chunk->setAllocated(alloc, false);

  // TODO: Since the mark bits are atomic, it's probably OK to unmark even if
  // the chunk is currently being swept. If we get lucky the memory will be
  // freed sooner.
  chunk->markBits.ref().unmarkOneBit(alloc, ColorBit::BlackBit);

  // Update heap size for tenured owned allocations.
  size_t bytes = SizeClassBytes(header->sizeClass);
  if (!header->isNurseryOwned) {
    bool updateRetained =
        majorState == State::Marking && !chunk->allocatedDuringCollection;
    size_t usableBytes = bytes - sizeof(MediumBuffer);
    zone->mallocHeapSize.removeBytes(usableBytes, updateRetained);
  }

  PoisonAlloc(header, JS_SWEPT_TENURED_PATTERN, bytes,
              MemCheckKind::MakeUndefined);

  FreeLists* freeLists = getChunkFreeLists(chunk);

  uintptr_t startAddr = uintptr_t(header);
  uintptr_t endAddr = startAddr + bytes;

  // First check whether there is a free region following the allocation.
  FreeRegion* region;
  uintptr_t endOffset = endAddr & ChunkMask;
  if (endOffset == 0 || chunk->isAllocated(endOffset)) {
    // The allocation abuts the end of the chunk or another allocation. Add the
    // allocation as a new free region.
    //
    // The new region is added to the front of relevant list so as to reuse
    // recently freed memory preferentially. This may reduce fragmentation. See
    // "The Memory Fragmentation Problem: Solved?"  by Johnstone et al.
    size_t sizeClass = SizeClassForFreeRegion(bytes);
    region = addFreeRegion(freeLists, sizeClass, startAddr, endAddr, false,
                           ListPosition::Front);
  } else {
    // There is a free region following this allocation. Expand the existing
    // region down to cover the newly freed space.
    region = findFollowingFreeRegion(endAddr);
    MOZ_ASSERT(region->startAddr == endAddr);
    updateFreeRegionStart(freeLists, region, startAddr);
  }

  // Next check for any preceding free region and coalesce.
  FreeRegion* precRegion = findPrecedingFreeRegion(startAddr);
  if (precRegion) {
    if (freeLists) {
      size_t sizeClass = SizeClassForFreeRegion(precRegion->size());
      freeLists->remove(sizeClass, precRegion);
    }

    updateFreeRegionStart(freeLists, region, precRegion->startAddr);
    if (precRegion->hasDecommittedPages) {
      region->hasDecommittedPages = true;
    }
  }
}

bool BufferAllocator::isSweepingChunk(BufferChunk* chunk) {
  if (minorState == State::Sweeping && chunk->hasNurseryOwnedAllocs) {
    // TODO: We could set a flag for nursery chunks allocated during minor
    // collection to allow operations on chunks that are not being swept here.

    if (!sweptChunksAvailable) {
      // We are currently sweeping nursery owned allocations.
      return true;
    }

    // Merge swept data, which may update hasNurseryOwnedAllocs.
    //
    // TODO: It would be good to know how often this helps and if it is
    // worthwhile.
    mergeSweptData();
    if (chunk->hasNurseryOwnedAllocs) {
      // We are currently sweeping nursery owned allocations.
      return true;
    }
  }

  if (majorState == State::Sweeping && !chunk->allocatedDuringCollection) {
    // We are currently sweeping tenured owned allocations.
    return true;
  }

  return false;
}

BufferAllocator::FreeRegion* BufferAllocator::addFreeRegion(
    FreeLists* freeLists, size_t sizeClass, uintptr_t start, uintptr_t end,
    bool anyDecommitted, ListPosition position,
    bool expectUnchanged /* = false */) {
#ifdef DEBUG
  MOZ_ASSERT(end - start >= SizeClassBytes(sizeClass));
  if (expectUnchanged) {
    // We didn't free any allocations so there should already be a FreeRegion
    // from |start| to |end|.
    auto* region = FreeRegion::fromEndAddr(end);
    region->check();
    MOZ_ASSERT(region->startAddr == start);
  }
#endif

  void* ptr = reinterpret_cast<void*>(end - sizeof(FreeRegion));
  FreeRegion* region = new (ptr) FreeRegion(start, anyDecommitted);
  MOZ_ASSERT(region->getEnd() == end);

  if (freeLists) {
    if (position == ListPosition::Front) {
      freeLists->pushFront(sizeClass, region);
    } else {
      freeLists->pushBack(sizeClass, region);
    }
  }

  return region;
}

void BufferAllocator::updateFreeRegionStart(FreeLists* freeLists,
                                            FreeRegion* region,
                                            uintptr_t newStart) {
  MOZ_ASSERT((newStart & ~ChunkMask) == (uintptr_t(region) & ~ChunkMask));
  MOZ_ASSERT(region->startAddr != newStart);

  size_t oldSize = region->size();
  region->startAddr = newStart;

  if (!freeLists) {
    return;
  }

  size_t currentSizeClass = SizeClassForFreeRegion(oldSize);
  size_t newSizeClass = SizeClassForFreeRegion(region->size());
  if (currentSizeClass != newSizeClass) {
    freeLists->remove(currentSizeClass, region);
    freeLists->pushFront(newSizeClass, region);
  }
}

bool BufferAllocator::growMedium(void* alloc, size_t newBytes) {
  BufferChunk* chunk = BufferChunk::from(alloc);
  if (isSweepingChunk(chunk)) {
    return false;  // We can't grow if the chunk is currently being swept.
  }

  auto* header = GetHeaderFromAlloc<MediumBuffer>(alloc);
  newBytes += sizeof(MediumBuffer);

  size_t currentBytes = SizeClassBytes(header->sizeClass);
  MOZ_ASSERT(newBytes > currentBytes);

  uintptr_t endOffset = (uintptr_t(header) & ChunkMask) + currentBytes;
  MOZ_ASSERT(endOffset <= ChunkSize);
  if (endOffset == ChunkSize) {
    return false;  // Can't extend because we're at the end of the chunk.
  }

  size_t endAddr = uintptr_t(chunk) + endOffset;
  if (chunk->isAllocated(endOffset)) {
    return false;  // Can't extend because we abut another allocation.
  }

  FreeRegion* region = findFollowingFreeRegion(endAddr);
  MOZ_ASSERT(region->startAddr == endAddr);

  size_t extraBytes = newBytes - currentBytes;
  if (region->size() < extraBytes) {
    return false;  // Can't extend because following free region is too small.
  }

  size_t sizeClass = SizeClassForFreeRegion(region->size());

  allocFromRegion(region, extraBytes, sizeClass);

  // If the allocation is in a chunk where we've cleared the free lists before
  // sweeping we don't need to update the free lists.
  if (FreeLists* freeLists = getChunkFreeLists(chunk)) {
    updateFreeListsAfterAlloc(freeLists, region, sizeClass);
  }

  header->sizeClass = SizeClassForAlloc(newBytes);
  if (!header->isNurseryOwned) {
    bool updateRetained =
        majorState == State::Marking && !chunk->allocatedDuringCollection;
    updateHeapSize(extraBytes, true, updateRetained);
  }

  return true;
}

bool BufferAllocator::shrinkMedium(void* alloc, size_t newBytes) {
  BufferChunk* chunk = BufferChunk::from(alloc);
  if (isSweepingChunk(chunk)) {
    return false;  // We can't shrink if the chunk is currently being swept.
  }

  auto* header = GetHeaderFromAlloc<MediumBuffer>(alloc);
  size_t currentBytes = SizeClassBytes(header->sizeClass);
  newBytes += sizeof(MediumBuffer);

  MOZ_ASSERT(newBytes < currentBytes);
  size_t sizeChange = currentBytes - newBytes;

  // Update allocation size.
  header->sizeClass = SizeClassForAlloc(newBytes);
  if (!header->isNurseryOwned) {
    bool updateRetained =
        majorState == State::Marking && !chunk->allocatedDuringCollection;
    zone->mallocHeapSize.removeBytes(sizeChange, updateRetained);
  }

  uintptr_t startOffset = uintptr_t(header) & ChunkMask;
  uintptr_t oldEndOffset = startOffset + currentBytes;
  uintptr_t newEndOffset = startOffset + newBytes;
  MOZ_ASSERT(oldEndOffset <= ChunkSize);

  // Poison freed memory.
  uintptr_t chunkAddr = uintptr_t(chunk);
  PoisonAlloc(reinterpret_cast<void*>(chunkAddr + newEndOffset),
              JS_SWEPT_TENURED_PATTERN, sizeChange,
              MemCheckKind::MakeUndefined);

  FreeLists* freeLists = getChunkFreeLists(chunk);

  // If we abut another allocation then add a new free region.
  if (oldEndOffset == ChunkSize || chunk->isAllocated(oldEndOffset)) {
    size_t sizeClass = SizeClassForFreeRegion(sizeChange);
    uintptr_t freeStart = chunkAddr + newEndOffset;
    uintptr_t freeEnd = chunkAddr + oldEndOffset;
    addFreeRegion(freeLists, sizeClass, freeStart, freeEnd, false,
                  ListPosition::Front);
    return true;
  }

  // Otherwise find the following free region and extend it down.
  FreeRegion* region = findFollowingFreeRegion(chunkAddr + oldEndOffset);
  MOZ_ASSERT(region->startAddr == chunkAddr + oldEndOffset);
  updateFreeRegionStart(freeLists, region, chunkAddr + newEndOffset);

  return true;
}

BufferAllocator::FreeLists* BufferAllocator::getChunkFreeLists(
    BufferChunk* chunk) {
  MOZ_ASSERT_IF(majorState == State::Sweeping,
                chunk->allocatedDuringCollection);

  if (majorState == State::Marking && !chunk->allocatedDuringCollection) {
    // The chunk has been queued for sweeping and the free lists cleared.
    return nullptr;
  }

  return &mediumFreeLists.ref();
}

BufferAllocator::FreeRegion* BufferAllocator::findFollowingFreeRegion(
    uintptr_t startAddr) {
  // Find the free region that starts at |startAddr|, which is not allocated and
  // not at the end of the chunk. Always returns a region.

  uintptr_t offset = uintptr_t(startAddr) & ChunkMask;
  MOZ_ASSERT(offset >= FirstMediumAllocOffset);
  MOZ_ASSERT(offset < ChunkSize);
  MOZ_ASSERT((offset % MinMediumAllocSize) == 0);

  BufferChunk* chunk = BufferChunk::from(reinterpret_cast<void*>(startAddr));
  MOZ_ASSERT(!chunk->isAllocated(offset));  // Already marked as not allocated.
  offset = chunk->findNextAllocated(offset);
  MOZ_ASSERT(offset <= ChunkSize);

  auto* region = FreeRegion::fromEndOffset(chunk, offset);
  MOZ_ASSERT(region->startAddr == startAddr);

  return region;
}

BufferAllocator::FreeRegion* BufferAllocator::findPrecedingFreeRegion(
    uintptr_t endAddr) {
  // Find the free region, if any, that ends at |endAddr|, which may be
  // allocated or at the start of the chunk.

  uintptr_t offset = uintptr_t(endAddr) & ChunkMask;
  MOZ_ASSERT(offset >= FirstMediumAllocOffset);
  MOZ_ASSERT(offset < ChunkSize);
  MOZ_ASSERT((offset % MinMediumAllocSize) == 0);

  if (offset == FirstMediumAllocOffset) {
    return nullptr;  // Already at start of chunk.
  }

  BufferChunk* chunk = BufferChunk::from(reinterpret_cast<void*>(endAddr));
  MOZ_ASSERT(!chunk->isAllocated(offset));
  offset = chunk->findPrevAllocated(offset);

  if (offset != ChunkSize) {
    // Found a preceding allocation.
    auto* header = MediumBuffer::from(chunk, offset);
    size_t bytes = SizeClassBytes(header->sizeClass);
    MOZ_ASSERT(uintptr_t(header) + bytes <= endAddr);
    if (uintptr_t(header) + bytes == endAddr) {
      // No free space between preceding allocation and |endAddr|.
      return nullptr;
    }
  }

  auto* region = FreeRegion::fromEndAddr(endAddr);
#ifdef DEBUG
  region->check();
  if (offset != ChunkSize) {
    auto* header = MediumBuffer::from(chunk, offset);
    size_t bytes = SizeClassBytes(header->sizeClass);
    MOZ_ASSERT(region->startAddr == uintptr_t(header) + bytes);
  } else {
    MOZ_ASSERT(region->startAddr == uintptr_t(chunk) + FirstMediumAllocOffset);
  }
#endif

  return region;
}

/* static */
size_t BufferAllocator::SizeClassForAlloc(size_t bytes) {
  MOZ_ASSERT(bytes >= MinMediumAllocSize);
  MOZ_ASSERT(bytes < MinLargeAllocSize);

  size_t log2Size = mozilla::CeilingLog2(bytes);
  MOZ_ASSERT((size_t(1) << log2Size) >= bytes);
  MOZ_ASSERT(MinMediumAllocShift == mozilla::CeilingLog2(MinMediumAllocSize));
  MOZ_ASSERT(log2Size >= MinMediumAllocShift);
  size_t sizeClass = log2Size - MinMediumAllocShift;
  MOZ_ASSERT(sizeClass < MediumAllocClasses);
  return sizeClass;
}

/* static */
size_t BufferAllocator::SizeClassForFreeRegion(size_t bytes) {
  MOZ_ASSERT(bytes >= MinMediumAllocSize);
  MOZ_ASSERT(bytes < ChunkSize);

  size_t log2Size = mozilla::FloorLog2(bytes);
  MOZ_ASSERT((size_t(1) << log2Size) <= bytes);
  MOZ_ASSERT(log2Size >= MinMediumAllocShift);
  size_t sizeClass =
      std::min(log2Size - MinMediumAllocShift, MediumAllocClasses - 1);
  MOZ_ASSERT(sizeClass < MediumAllocClasses);

  return sizeClass;
}

/* static */
inline size_t BufferAllocator::SizeClassBytes(size_t sizeClass) {
  MOZ_ASSERT(sizeClass < MediumAllocClasses);
  return 1 << (sizeClass + MinMediumAllocShift);
}

/* static */
bool BufferAllocator::IsMediumAlloc(void* alloc) {
  ChunkBase* chunk = js::gc::detail::GetGCAddressChunkBase(alloc);
  return chunk->getKind() == ChunkKind::MediumBuffers;
}

void* BufferAllocator::allocLarge(size_t bytes, bool nurseryOwned, bool inGC) {
  size_t totalBytes = RoundUp(bytes + sizeof(LargeBuffer), PageSize);
  MOZ_ASSERT(totalBytes >= MinLargeAllocSize);

  // Large allocations are aligned to the chunk size, even if they are smaller
  // than a chunk. This allows us to tell large from small allocations by
  // looking at the low bits of the pointer.
  void* ptr = MapAlignedPages(totalBytes, ChunkSize, ShouldStallAndRetry(inGC));
  if (!ptr) {
    return nullptr;
  }

  CheckHighBitsOfPointer(ptr);

  auto* header = new (ptr) LargeBuffer(zone, totalBytes);
  header->setNurseryOwned(nurseryOwned);

  if (nurseryOwned) {
    largeNurseryAllocs.ref().pushBack(header);
  } else {
    header->setAllocatedDuringCollection(majorState != State::NotCollecting);
    largeTenuredAllocs.ref().pushBack(header);
  }

  // Update memory accounting and trigger an incremental slice if needed.
  if (!nurseryOwned) {
    size_t usableBytes = totalBytes - sizeof(LargeBuffer);
    bool checkThresholds = !inGC;
    updateHeapSize(usableBytes, checkThresholds, false);
  }

  void* alloc = header->data();
  MOZ_ASSERT(IsLargeAlloc(alloc));

  return alloc;
}

void BufferAllocator::updateHeapSize(size_t bytes, bool checkThresholds,
                                     bool updateRetainedSize) {
  // Update memory accounting and trigger an incremental slice if needed.
  // TODO: This will eventually be attributed to gcHeapSize.
  zone->mallocHeapSize.addBytes(bytes, updateRetainedSize);
  if (checkThresholds) {
    GCRuntime* gc = &zone->runtimeFromAnyThread()->gc;
    gc->maybeTriggerGCAfterMalloc(zone);
  }
}

/* static */
bool BufferAllocator::IsLargeAlloc(void* alloc) {
  ChunkBase* chunk = js::gc::detail::GetGCAddressChunkBase(alloc);
  return chunk->kind == ChunkKind::LargeBuffer;
}

/* static */
bool BufferAllocator::IsLargeAllocMarked(void* alloc) {
  auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
  return header->isMarked();
}

/* static */
bool BufferAllocator::MarkLargeAlloc(void* alloc) {
  auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
  if (header->wasAllocatedDuringCollection()) {
    return false;
  }

  if (header->isNurseryOwned()) {
    // Nursery-owned allocations are always marked.
    return false;
  }

  return header->markAtomic();
}

bool BufferAllocator::sweepLargeTenured(LargeBuffer* header) {
  MOZ_ASSERT(!header->isNurseryOwned());
  MOZ_ASSERT(header->zone() == zone);
  MOZ_ASSERT(!header->isInList());

  if (!header->isMarked()) {
    unmapLarge(header, true);
    return false;
  }

  header->clearMarked();
  return true;
}

void BufferAllocator::freeLarge(void* alloc) {
  auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
  MOZ_ASSERT(header->isInList());
  MOZ_ASSERT(header->zone() == zone);

  if (!header->isNurseryOwned() && majorState == State::Sweeping &&
      !header->wasAllocatedDuringCollection()) {
    // TODO: Can we assert that this allocation is marked?
    return;  // Large allocations are currently being swept.
  }

  if (header->isNurseryOwned()) {
    largeNurseryAllocs.ref().remove(header);
  } else if (majorState == State::Marking &&
             !header->wasAllocatedDuringCollection()) {
    largeTenuredAllocsToSweep.ref().remove(header);
  } else {
    largeTenuredAllocs.ref().remove(header);
  }

  unmapLarge(header, false);
}

bool BufferAllocator::shrinkLarge(void* alloc, size_t newBytes) {
  MOZ_ASSERT(IsLargeAlloc(alloc));
  MOZ_ASSERT(IsLargeAllocSize(newBytes));

#ifdef XP_WIN
  // Can't unmap part of a region mapped with VirtualAlloc on Windows.
  //
  // It is possible to decommit the physical pages so we could do that and
  // track virtual size as well as committed size. This would also allow us to
  // grow the allocation again if necessary.
  return false;
#else
  auto* header = GetHeaderFromAlloc<LargeBuffer>(alloc);
  MOZ_ASSERT(header->isInList());
  MOZ_ASSERT(header->zone() == zone);

  if (!header->isNurseryOwned() && majorState == State::Sweeping &&
      !header->wasAllocatedDuringCollection()) {
    // TODO: Can we assert that this allocation is marked?
    return false;  // Large allocations are currently being swept.
  }

  newBytes = RoundUp(newBytes + sizeof(LargeBuffer), PageSize);
  size_t oldBytes = header->bytesIncludingHeader();
  MOZ_ASSERT(oldBytes > newBytes);
  size_t shrinkBytes = oldBytes - newBytes;

  if (!header->isNurseryOwned()) {
    zone->mallocHeapSize.removeBytes(shrinkBytes, false);
  }

  header->setSizeInPages(newBytes / PageSize);
  void* endPtr = reinterpret_cast<void*>(uintptr_t(header) + newBytes);
  UnmapPages(endPtr, shrinkBytes);

  return true;
#endif
}

void BufferAllocator::unmapLarge(LargeBuffer* header, bool isSweeping) {
  MOZ_ASSERT(header->zone() == zone);
  MOZ_ASSERT(!header->isInList());

  size_t bytes = header->bytesIncludingHeader();

  if (!header->isNurseryOwned()) {
    size_t usableBytes = bytes - sizeof(LargeBuffer);
    zone->mallocHeapSize.removeBytes(usableBytes, isSweeping);
  }

  UnmapPages(header, bytes);
}

#include "js/Printer.h"
#include "util/GetPidProvider.h"

static const char* const BufferAllocatorStatsPrefix = "BufAllc:";

#define FOR_EACH_BUFFER_STATS_FIELD(_)                \
  _("PID", 7, "%7zu", pid)                            \
  _("Runtime", 14, "0x%12p", runtime)                 \
  _("Timestamp", 10, "%10.6f", timestamp.ToSeconds()) \
  _("Reason", 20, "%-20.20s", reason)                 \
  _("", 2, "%2s", "")                                 \
  _("TotalKB", 8, "%8zu", totalBytes / 1024)          \
  _("UsedKB", 8, "%8zu", usedBytes / 1024)            \
  _("FreeKB", 8, "%8zu", freeBytes / 1024)            \
  _("Zs", 3, "%3zu", zoneCount)                       \
  _("", 7, "%7s", "")                                 \
  _("MNCs", 6, "%6zu", mediumMixedChunks)             \
  _("MTCs", 6, "%6zu", mediumTenuredChunks)           \
  _("FRs", 6, "%6zu", freeRegions)                    \
  _("LNAs", 6, "%6zu", largeNurseryAllocs)            \
  _("LTAs", 6, "%6zu", largeTenuredAllocs)

/* static */
void BufferAllocator::printStatsHeader(FILE* file) {
  Sprinter sprinter;
  if (!sprinter.init()) {
    return;
  }
  sprinter.put(BufferAllocatorStatsPrefix);

#define PRINT_METADATA_NAME(name, width, _1, _2) \
  sprinter.printf(" %-*s", width, name);

  FOR_EACH_BUFFER_STATS_FIELD(PRINT_METADATA_NAME)
#undef PRINT_METADATA_NAME

  sprinter.put("\n");

  JS::UniqueChars str = sprinter.release();
  if (!str) {
    return;
  }
  fputs(str.get(), file);
}

/* static */
void BufferAllocator::printStats(GCRuntime* gc, mozilla::TimeStamp creationTime,
                                 bool isMajorGC, FILE* file) {
  Sprinter sprinter;
  if (!sprinter.init()) {
    return;
  }
  sprinter.put(BufferAllocatorStatsPrefix);

  size_t pid = getpid();
  JSRuntime* runtime = gc->rt;
  mozilla::TimeDuration timestamp = mozilla::TimeStamp::Now() - creationTime;
  const char* reason = isMajorGC ? "post major slice" : "pre minor GC";

  size_t zoneCount = 0;
  size_t usedBytes = 0;
  size_t freeBytes = 0;
  size_t adminBytes = 0;
  size_t mediumMixedChunks = 0;
  size_t mediumTenuredChunks = 0;
  size_t freeRegions = 0;
  size_t largeNurseryAllocs = 0;
  size_t largeTenuredAllocs = 0;
  for (AllZonesIter zone(gc); !zone.done(); zone.next()) {
    zoneCount++;
    zone->bufferAllocator.getStats(usedBytes, freeBytes, adminBytes,
                                   mediumMixedChunks, mediumTenuredChunks,
                                   freeRegions, largeNurseryAllocs,
                                   largeTenuredAllocs);
  }

  size_t totalBytes = usedBytes + freeBytes + adminBytes;

#define PRINT_FIELD_VALUE(_1, _2, format, value) \
  sprinter.printf(" " format, value);

  FOR_EACH_BUFFER_STATS_FIELD(PRINT_FIELD_VALUE)
#undef PRINT_FIELD_VALUE

  sprinter.put("\n");

  JS::UniqueChars str = sprinter.release();
  if (!str) {
    return;
  }

  fputs(str.get(), file);
}

size_t BufferAllocator::getSizeOfNurseryBuffers() {
  maybeMergeSweptData();

  MOZ_ASSERT(minorState == State::NotCollecting);
  MOZ_ASSERT(majorState == State::NotCollecting);

  size_t bytes = 0;

  for (BufferChunk* chunk : mediumMixedChunks.ref()) {
    for (BufferChunkIter alloc(chunk); !alloc.done(); alloc.next()) {
      if (alloc->isNurseryOwned) {
        bytes += alloc->bytesIncludingHeader() - sizeof(MediumBuffer);
      }
    }
  }

  for (const LargeBuffer* buffer : largeNurseryAllocs.ref()) {
    bytes += buffer->bytesIncludingHeader() - sizeof(LargeBuffer);
  }

  return bytes;
}

void BufferAllocator::addSizeOfExcludingThis(size_t* usedBytesOut,
                                             size_t* freeBytesOut,
                                             size_t* adminBytesOut) {
  maybeMergeSweptData();

  MOZ_ASSERT(minorState == State::NotCollecting);
  MOZ_ASSERT(majorState == State::NotCollecting);

  size_t usedBytes = 0;
  size_t freeBytes = 0;
  size_t adminBytes = 0;
  size_t mediumMixedChunks = 0;
  size_t mediumTenuredChunks = 0;
  size_t freeRegions = 0;
  size_t largeNurseryAllocs = 0;
  size_t largeTenuredAllocs = 0;
  getStats(usedBytes, freeBytes, adminBytes, mediumMixedChunks,
           mediumTenuredChunks, freeRegions, largeNurseryAllocs,
           largeTenuredAllocs);

  *usedBytesOut += usedBytes;
  *freeBytesOut += freeBytes;
  *adminBytesOut += adminBytes;
}

void BufferAllocator::getStats(size_t& usedBytes, size_t& freeBytes,
                               size_t& adminBytes,
                               size_t& mediumNurseryChunkCount,
                               size_t& mediumTenuredChunkCount,
                               size_t& freeRegions,
                               size_t& largeNurseryAllocCount,
                               size_t& largeTenuredAllocCount) {
  maybeMergeSweptData();

  MOZ_ASSERT(minorState == State::NotCollecting);

  for (const BufferChunk* chunk : mediumMixedChunks.ref()) {
    (void)chunk;
    mediumNurseryChunkCount++;
    usedBytes += ChunkSize - FirstMediumAllocOffset;
    adminBytes += FirstMediumAllocOffset;
  }
  for (const BufferChunk* chunk : mediumTenuredChunks.ref()) {
    (void)chunk;
    mediumTenuredChunkCount++;
    usedBytes += ChunkSize - FirstMediumAllocOffset;
    adminBytes += FirstMediumAllocOffset;
  }
  for (const LargeBuffer* buffer : largeNurseryAllocs.ref()) {
    largeNurseryAllocCount++;
    usedBytes += buffer->bytesIncludingHeader() - sizeof(LargeBuffer);
    adminBytes += sizeof(LargeBuffer);
  }
  for (const LargeBuffer* buffer : largeTenuredAllocs.ref()) {
    largeTenuredAllocCount++;
    usedBytes += buffer->bytesIncludingHeader() - sizeof(LargeBuffer);
    adminBytes += sizeof(LargeBuffer);
  }
  for (const FreeList& freeList : mediumFreeLists.ref()) {
    for (const FreeRegion* region : freeList) {
      freeRegions++;
      size_t size = region->size();
      MOZ_ASSERT(usedBytes >= size);
      usedBytes -= size;
      freeBytes += size;
    }
  }
}

JS::ubi::Node::Size JS::ubi::Concrete<SmallBuffer>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return get().arena()->getThingSize();
}

const char16_t JS::ubi::Concrete<SmallBuffer>::concreteTypeName[] =
    u"SmallBuffer";
