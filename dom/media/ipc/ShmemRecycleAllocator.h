/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_ShmemRecycleAllocator_h
#define include_dom_media_ipc_ShmemRecycleAllocator_h

#include "mozilla/ShmemPool.h"

namespace mozilla {

// When used with ShmemRecycleAllocator instances, it allows for preventing a
// subset of shmems from being recycled until a transaction is complete. This is
// useful in cases such as encoding, where an arbitrary number of frames may be
// sent for encoding before any others complete. This instead allows us to
// recycle shmems as individual transactions complete without waiting for all
// outstanding to be complete first.
class ShmemRecycleTicket {
 public:
  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(ShmemRecycleTicket)

  ShmemRecycleTicket() = default;

 private:
  template <class T>
  friend class ShmemRecycleAllocator;

  ~ShmemRecycleTicket() { MOZ_DIAGNOSTIC_ASSERT(mUsedShmems.IsEmpty()); }

  AutoTArray<ShmemBuffer, 4> mUsedShmems;
};

template <class T>
class ShmemRecycleAllocator {
 public:
  explicit ShmemRecycleAllocator(T* aActor)
      : mActor(aActor), mPool(1, ShmemPool::PoolType::DynamicPool) {}
  ShmemBuffer AllocateBuffer(size_t aSize,
                             ShmemRecycleTicket* aTicket = nullptr,
                             ShmemPool::AllocationPolicy aPolicy =
                                 ShmemPool::AllocationPolicy::Unsafe) {
    ShmemBuffer buffer = mPool.Get(mActor, aSize, aPolicy);
    if (!buffer.Valid()) {
      return buffer;
    }
    MOZ_DIAGNOSTIC_ASSERT(aSize <= buffer.Get().Size<uint8_t>());
    if (aTicket) {
      aTicket->mUsedShmems.AppendElement(buffer.Get());
    } else {
      mUsedShmems.AppendElement(buffer.Get());
    }
    mNeedCleanup = true;
    return buffer;
  }

  void ReleaseBuffer(ShmemBuffer&& aBuffer) { mPool.Put(std::move(aBuffer)); }

  void ReleaseAllBuffers() {
    for (auto&& mem : mUsedShmems) {
      ReleaseBuffer(ShmemBuffer(mem.Get()));
    }
    mUsedShmems.Clear();
  }

  void ReleaseTicket(ShmemRecycleTicket* aTicket) {
    for (auto&& mem : aTicket->mUsedShmems) {
      ReleaseBuffer(ShmemBuffer(mem.Get()));
    }
    aTicket->mUsedShmems.Clear();
  }

  void CleanupShmemRecycleAllocator() {
    ReleaseAllBuffers();
    mPool.Cleanup(mActor);
    mNeedCleanup = false;
  }

  ~ShmemRecycleAllocator() {
    MOZ_DIAGNOSTIC_ASSERT(mUsedShmems.IsEmpty() && !mNeedCleanup,
                          "Shmems not all deallocated");
  }

 private:
  T* const mActor;
  ShmemPool mPool;
  AutoTArray<ShmemBuffer, 4> mUsedShmems;
  bool mNeedCleanup = false;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_ShmemRecycleAllocator_h
