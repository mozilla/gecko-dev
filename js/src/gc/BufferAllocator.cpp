/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/BufferAllocator-inl.h"

#include "gc/GCInternals.h"
#include "gc/Zone.h"
#include "js/HeapAPI.h"
#include "util/Poison.h"

#include "gc/Heap-inl.h"

using namespace js;
using namespace js::gc;

static constexpr size_t MinAllocSize = MinCellSize;  // 16 bytes

static constexpr size_t MinMediumAllocSize =
    1 << BufferAllocator::MinMediumAllocShift;

namespace js::gc {

bool SmallBuffer::isNurseryOwned() const {
  return header_.get() & NURSERY_OWNED_BIT;
}

void SmallBuffer::setNurseryOwned(bool value) {
  header_.set(value ? NURSERY_OWNED_BIT : 0);
}

}  // namespace js::gc

BufferAllocator::BufferAllocator(Zone* zone) : zone(zone) {}

/* static */
bool BufferAllocator::IsSmallAllocSize(size_t bytes) {
  return mozilla::RoundUpPow2(bytes + sizeof(SmallBuffer)) < MinMediumAllocSize;
}

/* static */
size_t BufferAllocator::GetGoodAllocSize(size_t requiredBytes) {
  requiredBytes = std::max(requiredBytes, MinAllocSize);

  MOZ_ASSERT(requiredBytes < MinMediumAllocSize);

  size_t headerSize = sizeof(SmallBuffer);

  // TODO: Support more sizes than powers of 2
  return mozilla::RoundUpPow2(requiredBytes + headerSize) - headerSize;
}

/* static */
size_t BufferAllocator::GetGoodPower2AllocSize(size_t requiredBytes) {
  requiredBytes = std::max(requiredBytes, MinAllocSize);

  MOZ_ASSERT(requiredBytes < MinMediumAllocSize);

  size_t headerSize = sizeof(SmallBuffer);
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
  if (IsSmallAllocSize(bytes)) {
    return allocSmall(bytes, nurseryOwned);
  }

  MOZ_CRASH("Not yet implemented");
}

void* BufferAllocator::allocInGC(size_t bytes, bool nurseryOwned) {
  if (IsSmallAllocSize(bytes)) {
    return allocSmallInGC(bytes, nurseryOwned);
  }

  MOZ_CRASH("Not yet implemented");
}

void* BufferAllocator::realloc(void* ptr, size_t bytes, bool nurseryOwned) {
  if (!ptr) {
    return alloc(bytes, nurseryOwned);
  }

  MOZ_ASSERT(GetAllocZone(ptr) == zone);
  MOZ_ASSERT(IsNurseryOwned(ptr) == nurseryOwned);

  size_t currentBytes = GetAllocSize(ptr);
  bytes = GetGoodAllocSize(bytes);
  if (bytes == currentBytes) {
    return ptr;
  }

  void* newPtr = alloc(bytes, nurseryOwned);
  if (!newPtr) {
    return nullptr;
  }

  size_t bytesToCopy = std::min(bytes, currentBytes);
  memcpy(newPtr, ptr, bytesToCopy);
  free(ptr);
  return newPtr;
}

template <typename HeaderT>
static HeaderT* GetHeaderFromAlloc(void* alloc) {
  return reinterpret_cast<HeaderT*>(uintptr_t(alloc) - sizeof(HeaderT));
}

void BufferAllocator::free(void* ptr) {
  MOZ_ASSERT(ptr);
  MOZ_ASSERT(GetAllocZone(ptr) == zone);

  DebugOnlyPoison(ptr, JS_FREED_BUFFER_PATTERN, GetAllocSize(ptr),
                  MemCheckKind::MakeUndefined);

  // Can't free small allocations.
}

/* static */
bool BufferAllocator::IsBufferAlloc(void* alloc) {
  // |alloc| is a pointer to a buffer allocation or a GC thing.

  ChunkKind chunkKind = detail::GetGCAddressChunkBase(alloc)->getKind();
  if (chunkKind == ChunkKind::TenuredArenas) {
    auto* arena = reinterpret_cast<Arena*>(uintptr_t(alloc) & ~ArenaMask);
    return IsBufferAllocKind(arena->getAllocKind());
  }

  return false;
}

/* static */
size_t BufferAllocator::GetAllocSize(void* alloc) {
  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->arena()->getThingSize() - sizeof(SmallBuffer);
  }

  MOZ_CRASH("Not yet implemented");
}

/* static */
JS::Zone* BufferAllocator::GetAllocZone(void* alloc) {
  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->zone();
  }

  MOZ_CRASH("Not yet implemented");
}

/* static */
bool BufferAllocator::IsNurseryOwned(void* alloc) {
  if (IsSmallAlloc(alloc)) {
    // This is always false because we currently make such allocations directly
    // in the nursery.
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->isNurseryOwned();
  }

  MOZ_CRASH("Not yet implemented");
}

void BufferAllocator::markNurseryOwned(void* alloc, bool ownerWasTenured) {
  MOZ_ASSERT(alloc);
  MOZ_ASSERT(IsNurseryOwned(alloc));
  MOZ_ASSERT(GetAllocZone(alloc) == zone);

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

  MOZ_CRASH("Not yet implemented");
}

/* static */
bool BufferAllocator::IsMarkedBlack(void* alloc) {
  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    MOZ_ASSERT(!cell->isMarkedGray());
    return cell->isMarkedBlack();
  }

  MOZ_CRASH("Not yet implemented");
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

  MOZ_CRASH("Not yet implemented");
}

/* static */
bool BufferAllocator::MarkTenuredAlloc(void* alloc) {
  MOZ_ASSERT(alloc);
  MOZ_ASSERT(!IsNurseryOwned(alloc));

  if (IsSmallAlloc(alloc)) {
    auto* cell = GetHeaderFromAlloc<SmallBuffer>(alloc);
    return cell->markIfUnmarkedAtomic(MarkColor::Black);
  }

  MOZ_CRASH("Not yet implemented");
}

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

JS::ubi::Node::Size JS::ubi::Concrete<SmallBuffer>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return get().arena()->getThingSize();
}

const char16_t JS::ubi::Concrete<SmallBuffer>::concreteTypeName[] =
    u"SmallBuffer";
