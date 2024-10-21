/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source code was derived from Chromium code, and as such is also subject
 * to the [Chromium license](ipc/chromium/src/LICENSE). */

#include "mozilla/ipc/SharedMemory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mozilla/Ashmem.h"

#ifdef MOZ_VALGRIND
#  include <valgrind/valgrind.h>
#endif

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfilerThreadSleep.h"
#include "mozilla/UniquePtrExtensions.h"
#include "prenv.h"

namespace mozilla::ipc {

void SharedMemory::ResetImpl() {};

SharedMemory::Handle SharedMemory::CloneHandle(const Handle& aHandle) {
  const int new_fd = dup(aHandle.get());
  if (new_fd < 0) {
    CHROMIUM_LOG(WARNING) << "failed to duplicate file descriptor: "
                          << strerror(errno);
    return nullptr;
  }
  return mozilla::UniqueFileHandle(new_fd);
}

void* SharedMemory::FindFreeAddressSpace(size_t size) {
  void* memory = mmap(nullptr, size, PROT_NONE,
                      MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
  if (memory == MAP_FAILED) {
    return nullptr;
  }
  munmap(memory, size);
  return memory;
}

Maybe<void*> SharedMemory::MapImpl(size_t nBytes, void* fixedAddress) {
  // Don't use MAP_FIXED when a fixed_address was specified, since that can
  // replace pages that are alread mapped at that address.
  void* mem =
      mmap(fixedAddress, nBytes, PROT_READ | (mReadOnly ? 0 : PROT_WRITE),
           MAP_SHARED, mHandle.get(), 0);

  if (mem == MAP_FAILED) {
    CHROMIUM_LOG(WARNING) << "Call to mmap failed: " << strerror(errno);
    return Nothing();
  }

  if (fixedAddress && mem != fixedAddress) {
    bool munmap_succeeded = munmap(mem, nBytes) == 0;
    DCHECK(munmap_succeeded) << "Call to munmap failed, errno=" << errno;
    return Nothing();
  }

  return Some(mem);
}

void SharedMemory::UnmapImpl(size_t nBytes, void* address) {
  munmap(address, nBytes);
}

// Android has its own shared memory API, ashmem.  It doesn't support POSIX
// shm_open, and the memfd support (see posix impl) also doesn't work because
// its SELinux policy prevents the procfs operations we'd use (see bug 1670277
// for more details).

bool SharedMemory::AppendPosixShmPrefix(std::string* str, pid_t pid) {
  return false;
}

bool SharedMemory::UsingPosixShm() { return false; }

bool SharedMemory::CreateImpl(size_t size, bool freezable) {
  DCHECK(size > 0);
  DCHECK(!mHandle);

  int fd = mozilla::android::ashmem_create(nullptr, size);
  if (fd < 0) {
    CHROMIUM_LOG(WARNING) << "failed to open shm: " << strerror(errno);
    return false;
  }

  mHandle.reset(fd);
  return true;
}

Maybe<SharedMemory::Handle> SharedMemory::ReadOnlyCopyImpl() {
  if (mozilla::android::ashmem_setProt(mHandle.get(), PROT_READ) != 0) {
    CHROMIUM_LOG(WARNING) << "failed to set ashmem read-only: "
                          << strerror(errno);
    return Nothing();
  }

  mozilla::UniqueFileHandle ro_file = std::move(mHandle);

  return Some(std::move(ro_file));
}

void SharedMemory::SystemProtect(char* aAddr, size_t aSize, int aRights) {
  if (!SystemProtectFallible(aAddr, aSize, aRights)) {
    MOZ_CRASH("can't mprotect()");
  }
}

bool SharedMemory::SystemProtectFallible(char* aAddr, size_t aSize,
                                         int aRights) {
  int flags = 0;
  if (aRights & RightsRead) flags |= PROT_READ;
  if (aRights & RightsWrite) flags |= PROT_WRITE;
  if (RightsNone == aRights) flags = PROT_NONE;

  return 0 == mprotect(aAddr, aSize, flags);
}

size_t SharedMemory::SystemPageSize() { return sysconf(_SC_PAGESIZE); }

}  // namespace mozilla::ipc
