/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef RADIX_TREE_H
#define RADIX_TREE_H

#include "mozilla/ThreadSafety.h"

#include "Constants.h"
#include "Utils.h"
#include "BaseAlloc.h"
#include "Mutex.h"

// ***************************************************************************
// Radix tree data structures.
//
// The number of bits passed to the template is the number of significant bits
// in an address to do a radix lookup with.
//
// An address is looked up by splitting it in kBitsPerLevel bit chunks, except
// the most significant bits, where the bit chunk is kBitsAtLevel1 which can be
// different if Bits is not a multiple of kBitsPerLevel.
//
// With e.g. sizeof(void*)=4, Bits=16 and kBitsPerLevel=8, an address is split
// like the following:
// 0x12345678 -> mRoot[0x12][0x34]
template <size_t Bits>
class AddressRadixTree {
// Size of each radix tree node (as a power of 2).
// This impacts tree depth.
#ifdef HAVE_64BIT_BUILD
  static const size_t kNodeSize = kCacheLineSize;
#else
  static const size_t kNodeSize = 16_KiB;
#endif
  static const size_t kBitsPerLevel = LOG2(kNodeSize) - LOG2(sizeof(void*));
  static const size_t kBitsAtLevel1 =
      (Bits % kBitsPerLevel) ? Bits % kBitsPerLevel : kBitsPerLevel;
  static const size_t kHeight = (Bits + kBitsPerLevel - 1) / kBitsPerLevel;
  static_assert(kBitsAtLevel1 + (kHeight - 1) * kBitsPerLevel == Bits,
                "AddressRadixTree parameters don't work out");

  Mutex mLock MOZ_UNANNOTATED;
  // We guard only the single slot creations and assume read-only is safe
  // at any time.
  void** mRoot;

 public:
  bool Init() MOZ_REQUIRES(gInitLock) MOZ_EXCLUDES(mLock);

  inline void* Get(void* aAddr) MOZ_EXCLUDES(mLock);

  // Returns whether the value was properly set.
  inline bool Set(void* aAddr, void* aValue) MOZ_EXCLUDES(mLock);

  inline bool Unset(void* aAddr) MOZ_EXCLUDES(mLock) {
    return Set(aAddr, nullptr);
  }

 private:
  // GetSlotInternal is agnostic wrt mLock and used directly only in DEBUG
  // code.
  inline void** GetSlotInternal(void* aAddr, bool aCreate);

  inline void** GetSlotIfExists(void* aAddr) MOZ_EXCLUDES(mLock) {
    return GetSlotInternal(aAddr, false);
  }
  inline void** GetOrCreateSlot(void* aAddr) MOZ_REQUIRES(mLock) {
    return GetSlotInternal(aAddr, true);
  }
};

template <size_t Bits>
bool AddressRadixTree<Bits>::Init() {
  mLock.Init();
  mRoot = (void**)base_calloc(1 << kBitsAtLevel1, sizeof(void*));
  return mRoot;
}

template <size_t Bits>
void** AddressRadixTree<Bits>::GetSlotInternal(void* aAddr, bool aCreate) {
  uintptr_t key = reinterpret_cast<uintptr_t>(aAddr);
  uintptr_t subkey;
  unsigned i, lshift, height, bits;
  void** node;
  void** child;

  for (i = lshift = 0, height = kHeight, node = mRoot; i < height - 1;
       i++, lshift += bits, node = child) {
    bits = i ? kBitsPerLevel : kBitsAtLevel1;
    subkey = (key << lshift) >> ((sizeof(void*) << 3) - bits);
    child = (void**)node[subkey];
    if (!child && aCreate) {
      child = (void**)base_calloc(1 << kBitsPerLevel, sizeof(void*));
      if (child) {
        node[subkey] = child;
      }
    }
    if (!child) {
      return nullptr;
    }
  }

  // node is a leaf, so it contains values rather than node
  // pointers.
  bits = i ? kBitsPerLevel : kBitsAtLevel1;
  subkey = (key << lshift) >> ((sizeof(void*) << 3) - bits);
  return &node[subkey];
}

template <size_t Bits>
void* AddressRadixTree<Bits>::Get(void* aAddr) {
  void* ret = nullptr;

  void** slot = GetSlotIfExists(aAddr);

  if (slot) {
    ret = *slot;
  }
#ifdef MOZ_DEBUG
  MutexAutoLock lock(mLock);

  // Suppose that it were possible for a jemalloc-allocated chunk to be
  // munmap()ped, followed by a different allocator in another thread re-using
  // overlapping virtual memory, all without invalidating the cached rtree
  // value.  The result would be a false positive (the rtree would claim that
  // jemalloc owns memory that it had actually discarded).  I don't think this
  // scenario is possible, but the following assertion is a prudent sanity
  // check.
  if (!slot) {
    // In case a slot has been created in the meantime.
    slot = GetSlotInternal(aAddr, false);
  }
  if (slot) {
    // The MutexAutoLock above should act as a memory barrier, forcing
    // the compiler to emit a new read instruction for *slot.
    MOZ_ASSERT(ret == *slot);
  } else {
    MOZ_ASSERT(ret == nullptr);
  }
#endif
  return ret;
}

template <size_t Bits>
bool AddressRadixTree<Bits>::Set(void* aAddr, void* aValue) {
  MutexAutoLock lock(mLock);
  void** slot = GetOrCreateSlot(aAddr);
  if (slot) {
    *slot = aValue;
  }
  return slot;
}

#endif /* ! RADIX_TREE_H */
