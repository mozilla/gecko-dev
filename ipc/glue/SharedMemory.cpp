/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source code was derived from Chromium code, and as such is also subject
 * to the [Chromium license](ipc/chromium/src/LICENSE). */

#include "mozilla/ipc/SharedMemory.h"

#include <utility>

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/Atomics.h"
#include "nsIMemoryReporter.h"

#ifdef FUZZING
#  include "mozilla/ipc/SharedMemoryFuzzer.h"
#endif

namespace mozilla::ipc {

static Atomic<size_t> gShmemAllocated;
static Atomic<size_t> gShmemMapped;

class ShmemReporter final : public nsIMemoryReporter {
  ~ShmemReporter() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override {
    MOZ_COLLECT_REPORT(
        "shmem-allocated", KIND_OTHER, UNITS_BYTES, gShmemAllocated,
        "Memory shared with other processes that is accessible (but not "
        "necessarily mapped).");

    MOZ_COLLECT_REPORT(
        "shmem-mapped", KIND_OTHER, UNITS_BYTES, gShmemMapped,
        "Memory shared with other processes that is mapped into the address "
        "space.");

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(ShmemReporter, nsIMemoryReporter)

SharedMemory::SharedMemory() : mAllocSize(0), mMappedSize(0) {
  static Atomic<bool> registered;
  if (registered.compareExchange(false, true)) {
    RegisterStrongMemoryReporter(new ShmemReporter());
  }
}

SharedMemory::~SharedMemory() {
  Unmap();
  CloseHandle();
  ResetImpl();

  MOZ_ASSERT(gShmemAllocated >= mAllocSize,
             "Can't destroy more than allocated");
  gShmemAllocated -= mAllocSize;
  mAllocSize = 0;
}

bool SharedMemory::Create(size_t aNBytes, bool freezable) {
  MOZ_ASSERT(!IsValid(), "already initialized");
  bool ok = CreateImpl(aNBytes, freezable);
  if (ok) {
    mAllocSize = aNBytes;
    mFreezable = freezable;
    mReadOnly = false;
    mExternalHandle = false;
    gShmemAllocated += mAllocSize;
  }
  return ok;
}

bool SharedMemory::Map(size_t aNBytes, void* fixedAddress) {
  if (!mHandle) {
    return false;
  }
  MOZ_ASSERT(!mMemory, "can't map memory when a mapping already exists");
  auto address = MapImpl(aNBytes, fixedAddress);
  if (address) {
    mMappedSize = aNBytes;
    mMemory = UniqueMapping(*address, MappingDeleter(mMappedSize));
    gShmemMapped += mMappedSize;
    return true;
  }
  return false;
}

void SharedMemory::Unmap() {
  if (!mMemory) {
    return;
  }
  MOZ_ASSERT(gShmemMapped >= mMappedSize, "Can't unmap more than mapped");
  mMemory = nullptr;
  gShmemMapped -= std::exchange(mMappedSize, 0);
}

void* SharedMemory::Memory() const {
#ifdef FUZZING
  return SharedMemoryFuzzer::MutateSharedMemory(mMemory.get(), mAllocSize);
#else
  return mMemory.get();
#endif
}

Span<uint8_t> SharedMemory::TakeMapping() {
  // NOTE: this doesn't reduce gShmemMapped since it _is_ still mapped memory
  // (and will be until the process terminates).
  return {static_cast<uint8_t*>(mMemory.release()),
          std::exchange(mMappedSize, 0)};
}

SharedMemory::Handle SharedMemory::TakeHandle() {
  gShmemAllocated -= mAllocSize;
  mAllocSize = 0;
  return std::move(mHandle);
}

bool SharedMemory::SetHandle(Handle aHandle, OpenRights aRights) {
  MOZ_ASSERT(!IsValid(),
             "SetHandle cannot be called when a valid handle is already held");
  ResetImpl();
  mHandle = std::move(aHandle);
  mAllocSize = 0;
  mMappedSize = 0;
  mFreezable = false;
  mReadOnly = aRights == OpenRights::RightsReadOnly;
  mExternalHandle = true;
  return true;
}

bool SharedMemory::ReadOnlyCopy(SharedMemory* ro_out) {
  MOZ_ASSERT(mHandle);
  MOZ_ASSERT(!mReadOnly);
  MOZ_ASSERT(mFreezable);
  if (ro_out == this) {
    MOZ_ASSERT(
        !mMemory,
        "Memory cannot be mapped when creating a read-only copy of this.");
  }
  auto handle = ReadOnlyCopyImpl();
  auto allocSize = mAllocSize;
  TakeHandle();
  if (!handle) {
    return false;
  }
  mFreezable = false;

  // Reset ro_out (unmapping, etc).
  ro_out->~SharedMemory();

  ro_out->mHandle = std::move(*handle);
  ro_out->mAllocSize = allocSize;
  gShmemAllocated += ro_out->mAllocSize;
  ro_out->mReadOnly = true;
  ro_out->mFreezable = false;
  ro_out->mExternalHandle = mExternalHandle;
  return true;
}

bool SharedMemory::WriteHandle(IPC::MessageWriter* aWriter) {
  Handle handle = CloneHandle(mHandle);
  if (!handle) {
    return false;
  }
  IPC::WriteParam(aWriter, std::move(handle));
  return true;
}

bool SharedMemory::ReadHandle(IPC::MessageReader* aReader) {
  Handle handle;
  return IPC::ReadParam(aReader, &handle) && IsHandleValid(handle) &&
         SetHandle(std::move(handle), RightsReadWrite);
}

void SharedMemory::Protect(char* aAddr, size_t aSize, int aRights) {
  // Don't allow altering of rights on freezable shared memory handles.
  MOZ_ASSERT(!mFreezable);

  char* memStart = reinterpret_cast<char*>(Memory());
  if (!memStart) MOZ_CRASH("SharedMemory region points at NULL!");
  char* memEnd = memStart + Size();

  char* protStart = aAddr;
  if (!protStart) MOZ_CRASH("trying to Protect() a NULL region!");
  char* protEnd = protStart + aSize;

  if (!(memStart <= protStart && protEnd <= memEnd)) {
    MOZ_CRASH("attempt to Protect() a region outside this SharedMemory");
  }

  // checks alignment etc.
  SystemProtect(aAddr, aSize, aRights);
}

size_t SharedMemory::PageAlignedSize(size_t aSize) {
  size_t pageSize = SystemPageSize();
  size_t nPagesNeeded = size_t(ceil(double(aSize) / double(pageSize)));
  return pageSize * nPagesNeeded;
}

}  // namespace mozilla::ipc
