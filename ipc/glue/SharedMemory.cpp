/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SharedMemory.h"

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

  MOZ_ASSERT(gShmemAllocated >= mAllocSize,
             "Can't destroy more than allocated");
  gShmemAllocated -= mAllocSize;
  mAllocSize = 0;
}

bool SharedMemory::WriteHandle(IPC::MessageWriter* aWriter) {
  Handle handle = CloneHandle();
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

bool SharedMemory::Create(size_t aNBytes) {
  bool ok = CreateImpl(aNBytes);
  if (ok) {
    mAllocSize = aNBytes;
    gShmemAllocated += mAllocSize;
  }
  return ok;
}

bool SharedMemory::Map(size_t aNBytes, void* fixedAddress) {
  bool ok = MapImpl(aNBytes, fixedAddress);
  if (ok) {
    mMappedSize = aNBytes;
    gShmemMapped += mMappedSize;
  }
  return ok;
}

void SharedMemory::Unmap() {
  MOZ_ASSERT(gShmemMapped >= mMappedSize, "Can't unmap more than mapped");
  UnmapImpl(mMappedSize);
  gShmemMapped -= mMappedSize;
  mMappedSize = 0;
}

void* SharedMemory::Memory() const {
#ifdef FUZZING
  return SharedMemoryFuzzer::MutateSharedMemory(MemoryImpl(), mAllocSize);
#else
  return MemoryImpl();
#endif
}

}  // namespace mozilla::ipc
