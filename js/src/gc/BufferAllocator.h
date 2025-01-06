/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// GC-internal header file for the buffer allocator.

#ifndef gc_BufferAllocator_h
#define gc_BufferAllocator_h

#include <cstdint>
#include <stddef.h>
#include <utility>

#include "jstypes.h"  // JS_PUBLIC_API

#include "ds/SlimLinkedList.h"
#include "threading/ProtectedData.h"

class JS_PUBLIC_API JSTracer;

namespace JS {
class JS_PUBLIC_API Zone;
}  // namespace JS

namespace js {

class GCMarker;
class Nursery;

namespace gc {

enum class AllocKind : uint8_t;

struct Cell;

// BufferAllocator allocates dynamically sized blocks of memory which can be
// reclaimed by the garbage collector and are associated with GC things.
//
// Although these blocks can be reclaimed by GC, explicit free and resize is
// also supported. This is important for buffers that can grow or shrink.
//
// The allocator uses a different strategy depending on the size of the
// allocation requested. There are three size ranges, divided as follows:
//
//   Size:            Kind:   Allocator implementation:
//    16 B  - 128 B   Small   Uses the cell (arena) allocator
//   256 B  - 512 KB  Medium  Uses a free list allocator
//     1 MB -         Large   Uses the OS page allocator (e.g. mmap)
//
// The smallest supported allocation size is 16 bytes. This will be used for a
// dynamic slots allocation with zero slots to set a unique ID on a native
// object. This will be the size of two Values, i.e. 16 bytes.
//
// Supported operations
// --------------------
//
//  - Allocate a buffer. Buffers are always owned by a GC cell, and the
//    allocator tracks whether the owner is in the nursery or the tenured heap.
//
//  - Trace an edge to buffer. When the owning cell is traced it must trace the
//    edge to the buffer. This will mark the buffer in a GC and prevent it from
//    being swept.
//
// Small allocations
// -----------------
//
// These are implemented by the cell allocator and are allocated in arenas (slab
// allocation).
//
// Note: currently we do not allocate these in the nursery because nursery-owned
// buffers are allocated directly in the nursery without going through this
// allocation API.

class BufferAllocator : public SlimLinkedListElement<BufferAllocator> {
 public:
  static constexpr size_t MinMediumAllocShift = 8;   // 256 B
  static constexpr size_t MaxMediumAllocShift = 19;  // 512 KB

  static constexpr size_t MediumAllocClasses =
      MaxMediumAllocShift - MinMediumAllocShift + 1;

 private:
  // The zone this allocator is associated with.
  MainThreadOrGCTaskData<JS::Zone*> zone;

 public:
  explicit BufferAllocator(JS::Zone* zone);

  static size_t GetGoodAllocSize(size_t requiredBytes);
  static size_t GetGoodElementCount(size_t requiredElements,
                                    size_t elementSize);
  static size_t GetGoodPower2AllocSize(size_t requiredBytes);
  static size_t GetGoodPower2ElementCount(size_t requiredElements,
                                          size_t elementSize);
  static bool IsBufferAlloc(void* alloc);
  static size_t GetAllocSize(void* alloc);
  static JS::Zone* GetAllocZone(void* alloc);
  static bool IsNurseryOwned(void* alloc);
  static bool IsMarkedBlack(void* alloc);
  static void TraceEdge(JSTracer* trc, Cell* owner, void* buffer,
                        const char* name);

  void* alloc(size_t bytes, bool nurseryOwned);
  void* allocInGC(size_t bytes, bool nurseryOwned);
  void* realloc(void* ptr, size_t bytes, bool nurseryOwned);
  void free(void* ptr);

 private:
  // GC-internal APIs:
  static bool MarkTenuredAlloc(void* alloc);
  friend class js::GCMarker;

  void markNurseryOwned(void* alloc, bool ownerWasTenured);
  friend class js::Nursery;

  // Small allocation methods:

  static bool IsSmallAllocSize(size_t bytes);
  static bool IsSmallAlloc(void* alloc);

  void* allocSmall(size_t bytes, bool nurseryOwned);
  void* allocSmallInGC(size_t bytes, bool nurseryOwned);

  static AllocKind AllocKindForSmallAlloc(size_t bytes);
};

}  // namespace gc
}  // namespace js

#endif  // gc_BufferAllocator_h
