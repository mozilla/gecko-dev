/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_BufferAllocator_inl_h
#define gc_BufferAllocator_inl_h

#include "gc/BufferAllocator.h"

#include "gc/Allocator-inl.h"

inline size_t js::gc::GetGoodAllocSize(size_t requiredBytes) {
  return BufferAllocator::GetGoodAllocSize(requiredBytes);
}

inline size_t js::gc::GetGoodElementCount(size_t requiredCount,
                                          size_t elementSize) {
  return BufferAllocator::GetGoodElementCount(requiredCount, elementSize);
}

inline size_t js::gc::GetGoodPower2AllocSize(size_t requiredBytes) {
  return BufferAllocator::GetGoodPower2AllocSize(requiredBytes);
}

inline size_t js::gc::GetGoodPower2ElementCount(size_t requiredCount,
                                                size_t elementSize) {
  return BufferAllocator::GetGoodPower2ElementCount(requiredCount, elementSize);
}

inline void* js::gc::AllocBuffer(JS::Zone* zone, size_t bytes,
                                 bool nurseryOwned) {
  if (js::oom::ShouldFailWithOOM()) {
    return nullptr;
  }

  return zone->bufferAllocator.alloc(bytes, nurseryOwned);
}

inline void* js::gc::AllocBufferInGC(JS::Zone* zone, size_t bytes,
                                     bool nurseryOwned) {
  return zone->bufferAllocator.allocInGC(bytes, nurseryOwned);
}

inline void* js::gc::ReallocBuffer(JS::Zone* zone, void* alloc, size_t bytes,
                                   bool nurseryOwned) {
  if (js::oom::ShouldFailWithOOM()) {
    return nullptr;
  }

  return zone->bufferAllocator.realloc(alloc, bytes, nurseryOwned);
}

inline void js::gc::FreeBuffer(JS::Zone* zone, void* alloc) {
  return zone->bufferAllocator.free(alloc);
}

inline bool js::gc::IsBufferAlloc(void* alloc) {
  return BufferAllocator::IsBufferAlloc(alloc);
}

inline size_t js::gc::GetAllocSize(void* alloc) {
  return BufferAllocator::GetAllocSize(alloc);
}

inline JS::Zone* js::gc::GetAllocZone(void* alloc) {
  return BufferAllocator::GetAllocZone(alloc);
}

inline bool js::gc::IsNurseryOwned(void* alloc) {
  return BufferAllocator::IsNurseryOwned(alloc);
}

inline bool js::gc::IsBufferAllocMarkedBlack(void* alloc) {
  return BufferAllocator::IsMarkedBlack(alloc);
}

#endif  // gc_BufferAllocator_inl_h
