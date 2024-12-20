/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source code was derived from Chromium code, and as such is also subject
 * to the [Chromium license](ipc/chromium/src/LICENSE). */

#include "SharedMemoryPlatform.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mozilla/Ashmem.h"

#ifdef MOZ_VALGRIND
#  include <valgrind/valgrind.h>
#endif

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtrExtensions.h"
#include "prenv.h"

namespace mozilla::ipc::shared_memory {

// Right now we do nothing different for freezable shared memory on Android.
static Maybe<PlatformHandle> CreateImpl(size_t aSize, bool aFreezable) {
  MOZ_ASSERT(aSize > 0);

  int fd = mozilla::android::ashmem_create(nullptr, aSize);
  if (fd < 0) {
    MOZ_LOG_FMT(gSharedMemoryLog, LogLevel::Warning, "failed to open shm: {}",
                strerror(errno));
    return Nothing();
  }

  return Some(fd);
}

bool Platform::Create(Handle& aHandle, size_t aSize) {
  if (auto ph = CreateImpl(aSize, false)) {
    aHandle.mHandle = std::move(*ph);
    aHandle.mSize = aSize;
    return true;
  }
  return false;
}

bool Platform::CreateFreezable(FreezableHandle& aHandle, size_t aSize) {
  if (auto ph = CreateImpl(aSize, true)) {
    aHandle.mHandle = std::move(*ph);
    aHandle.mSize = aSize;
    return true;
  }
  return false;
}

PlatformHandle Platform::CloneHandle(const PlatformHandle& aHandle) {
  const int new_fd = dup(aHandle.get());
  if (new_fd < 0) {
    MOZ_LOG_FMT(gSharedMemoryLog, LogLevel::Warning,
                "failed to duplicate file descriptor: {}", strerror(errno));
    return nullptr;
  }
  return mozilla::UniqueFileHandle(new_fd);
}

bool Platform::Freeze(FreezableHandle& aHandle) {
  if (mozilla::android::ashmem_setProt(aHandle.mHandle.get(), PROT_READ) != 0) {
    MOZ_LOG_FMT(gSharedMemoryLog, LogLevel::Warning,
                "failed to set ashmem read-only: {}", strerror(errno));
    return false;
  }
  return true;
}

Maybe<void*> Platform::Map(const HandleBase& aHandle, void* aFixedAddress,
                           bool aReadOnly) {
  // Don't use MAP_FIXED when a fixed_address was specified, since that can
  // replace pages that are alread mapped at that address.
  void* mem = mmap(aFixedAddress, aHandle.Size(),
                   PROT_READ | (aReadOnly ? 0 : PROT_WRITE), MAP_SHARED,
                   aHandle.mHandle.get(), 0);

  if (mem == MAP_FAILED) {
    MOZ_LOG_FMT(gSharedMemoryLog, LogLevel::Warning, "call to mmap failed: {}",
                strerror(errno));
    return Nothing();
  }

  if (aFixedAddress && mem != aFixedAddress) {
    DebugOnly<bool> munmap_succeeded = munmap(mem, aHandle.Size()) == 0;
    MOZ_ASSERT(munmap_succeeded, "call to munmap failed");
    return Nothing();
  }

  return Some(mem);
}

void Platform::Unmap(void* aMemory, size_t aSize) { munmap(aMemory, aSize); }

bool Platform::Protect(char* aAddr, size_t aSize, Access aAccess) {
  int flags = PROT_NONE;
  if (aAccess & AccessRead) flags |= PROT_READ;
  if (aAccess & AccessWrite) flags |= PROT_WRITE;

  return 0 == mprotect(aAddr, aSize, flags);
}

void* Platform::FindFreeAddressSpace(size_t aSize) {
  void* memory = mmap(nullptr, aSize, PROT_NONE,
                      MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
  if (memory == MAP_FAILED) {
    return nullptr;
  }
  munmap(memory, aSize);
  return memory;
}

size_t Platform::PageSize() { return sysconf(_SC_PAGESIZE); }

bool Platform::IsSafeToMap(const PlatformHandle&) { return true; }

}  // namespace mozilla::ipc::shared_memory
