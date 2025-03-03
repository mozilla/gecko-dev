/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPSharedMemManager.h"
#include "mozilla/ipc/SharedMemory.h"

namespace mozilla::gmp {

GMPSharedMemManager::~GMPSharedMemManager() {
#ifdef DEBUG
  for (const auto& pool : mPool) {
    MOZ_ASSERT(pool.IsEmpty());
  }
#endif
}

void GMPSharedMemManager::PurgeSmallerShmem(nsTArray<ipc::Shmem>& aPool,
                                            size_t aSize) {
  aPool.RemoveElementsBy([&](ipc::Shmem& shmem) {
    if (!shmem.IsWritable()) {
      MOZ_ASSERT_UNREACHABLE("Shmem must be writable!");
      return true;
    }
    if (shmem.Size<uint8_t>() >= aSize) {
      return false;
    }
    MgrDeallocShmem(shmem);
    return true;
  });
}

bool GMPSharedMemManager::MgrTakeShmem(GMPSharedMemClass aClass,
                                       ipc::Shmem* aMem) {
  MOZ_ASSERT(MgrIsOnOwningThread());

  auto& pool = mPool[size_t(aClass)];
  if (pool.IsEmpty()) {
    return false;
  }

  *aMem = pool.PopLastElement();
  return true;
}

bool GMPSharedMemManager::MgrTakeShmem(GMPSharedMemClass aClass, size_t aSize,
                                       ipc::Shmem* aMem) {
  MOZ_ASSERT(MgrIsOnOwningThread());

  auto& pool = mPool[size_t(aClass)];
  size_t alignedSize = ipc::SharedMemory::PageAlignedSize(aSize);
  PurgeSmallerShmem(pool, alignedSize);
  if (pool.IsEmpty()) {
    return MgrAllocShmem(alignedSize, aMem);
  }

  *aMem = pool.PopLastElement();
  return true;
}

void GMPSharedMemManager::MgrGiveShmem(GMPSharedMemClass aClass,
                                       ipc::Shmem&& aMem) {
  MOZ_ASSERT(MgrIsOnOwningThread());

  if (!aMem.IsWritable()) {
    return;
  }

  auto& pool = mPool[size_t(aClass)];
  PurgeSmallerShmem(pool, aMem.Size<uint8_t>());

  if (pool.Length() >= kMaxPoolLength) {
    MgrDeallocShmem(aMem);
    return;
  }

  pool.AppendElement(std::move(aMem));
}

void GMPSharedMemManager::MgrPurgeShmems() {
  MOZ_ASSERT(MgrIsOnOwningThread());

  for (auto& pool : mPool) {
    for (auto& shmem : pool) {
      if (shmem.IsWritable()) {
        MgrDeallocShmem(shmem);
      } else {
        MOZ_ASSERT_UNREACHABLE("Shmem must be writable!");
      }
    }
    pool.Clear();
  }
}

}  // namespace mozilla::gmp
