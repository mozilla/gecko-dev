/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SharedMemory.h"

#include <utility>

#include <mach/vm_map.h>
#include <mach/mach_port.h>
#if defined(XP_IOS)
#  include <mach/vm_map.h>
#  define mach_vm_address_t vm_address_t
#  define mach_vm_map vm_map
#  define mach_vm_read vm_read
#  define mach_vm_region_recurse vm_region_recurse_64
#  define mach_vm_size_t vm_size_t
#else
#  include <mach/mach_vm.h>
#endif
#include <pthread.h>
#include <sys/mman.h>  // mprotect
#include <unistd.h>

#if defined(XP_MACOSX) && defined(__x86_64__)
#  include "prenv.h"
#endif

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Printf.h"
#include "mozilla/StaticMutex.h"

#ifdef DEBUG
#  define LOG_ERROR(str, args...)                                  \
    PR_BEGIN_MACRO                                                 \
    mozilla::SmprintfPointer msg = mozilla::Smprintf(str, ##args); \
    NS_WARNING(msg.get());                                         \
    PR_END_MACRO
#else
#  define LOG_ERROR(str, args...) \
    do { /* nothing */            \
    } while (0)
#endif

namespace mozilla::ipc {

static inline void* toPointer(mach_vm_address_t address) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

static inline mach_vm_address_t toVMAddress(void* pointer) {
  return static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(pointer));
}

void SharedMemory::ResetImpl() {};

bool SharedMemory::CreateImpl(size_t size, bool freezable) {
  memory_object_size_t memoryObjectSize = round_page(size);

  kern_return_t kr =
      mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, 0,
                                MAP_MEM_NAMED_CREATE | VM_PROT_DEFAULT,
                                getter_Transfers(mHandle), MACH_PORT_NULL);
  if (kr != KERN_SUCCESS || memoryObjectSize < round_page(size)) {
    LOG_ERROR("Failed to make memory entry (%zu bytes). %s (%x)\n", size,
              mach_error_string(kr), kr);
    TakeHandle();
    return false;
  }
  return true;
}

Maybe<void*> MapMemory(size_t size, void* fixedAddress,
                       const mozilla::UniqueMachSendRight& port,
                       bool readOnly) {
  kern_return_t kr;
  mach_vm_address_t address = toVMAddress(fixedAddress);

  vm_prot_t vmProtection = VM_PROT_READ;
  if (!readOnly) {
    vmProtection |= VM_PROT_WRITE;
  }

  kr =
      mach_vm_map(mach_task_self(), &address, round_page(size), 0,
                  fixedAddress ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE, port.get(),
                  0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
  if (kr != KERN_SUCCESS) {
    if (!fixedAddress) {
      LOG_ERROR(
          "Failed to map shared memory (%zu bytes) into %x, port %x. %s (%x)\n",
          size, mach_task_self(), mach_port_t(port.get()),
          mach_error_string(kr), kr);
    }
    return Nothing();
  }

  if (fixedAddress && fixedAddress != toPointer(address)) {
    kr = vm_deallocate(mach_task_self(), address, size);
    if (kr != KERN_SUCCESS) {
      LOG_ERROR(
          "Failed to unmap shared memory at unsuitable address "
          "(%zu bytes) from %x, port %x. %s (%x)\n",
          size, mach_task_self(), mach_port_t(port.get()),
          mach_error_string(kr), kr);
    }
    return Nothing();
  }

  return Some(toPointer(address));
}

Maybe<void*> SharedMemory::MapImpl(size_t size, void* fixedAddress) {
  return MapMemory(size, fixedAddress, mHandle, mReadOnly);
}

void* SharedMemory::FindFreeAddressSpace(size_t size) {
  mach_vm_address_t address = 0;
  size = round_page(size);
  if (mach_vm_map(mach_task_self(), &address, size, 0, VM_FLAGS_ANYWHERE,
                  MEMORY_OBJECT_NULL, 0, false, VM_PROT_NONE, VM_PROT_NONE,
                  VM_INHERIT_NONE) != KERN_SUCCESS ||
      vm_deallocate(mach_task_self(), address, size) != KERN_SUCCESS) {
    return nullptr;
  }
  return toPointer(address);
}

auto SharedMemory::CloneHandle(const Handle& aHandle) -> Handle {
  return mozilla::RetainMachSendRight(aHandle.get());
}

void SharedMemory::UnmapImpl(size_t nBytes, void* address) {
  vm_address_t vm_address = toVMAddress(address);
  kern_return_t kr =
      vm_deallocate(mach_task_self(), vm_address, round_page(nBytes));
  if (kr != KERN_SUCCESS) {
    LOG_ERROR("Failed to deallocate shared memory. %s (%x)\n",
              mach_error_string(kr), kr);
  }
}

Maybe<SharedMemory::Handle> SharedMemory::ReadOnlyCopyImpl() {
  memory_object_size_t memoryObjectSize = round_page(mAllocSize);

  mozilla::UniqueMachSendRight port;

  void* address = mMemory.get();
  bool unmap = false;

  if (!address) {
    // Temporarily map memory (as readonly) to get an address.
    if (auto memory = MapMemory(memoryObjectSize, nullptr, mHandle, true)) {
      address = *memory;
      unmap = true;
    } else {
      return Nothing();
    }
  }

  kern_return_t kr = mach_make_memory_entry_64(
      mach_task_self(), &memoryObjectSize,
      static_cast<memory_object_offset_t>(reinterpret_cast<uintptr_t>(address)),
      VM_PROT_READ, getter_Transfers(port), MACH_PORT_NULL);

  if (unmap) {
    kern_return_t kr =
        vm_deallocate(mach_task_self(), toVMAddress(address), memoryObjectSize);
    if (kr != KERN_SUCCESS) {
      LOG_ERROR("Failed to deallocate shared memory. %s (%x)\n",
                mach_error_string(kr), kr);
    }
  }

  if (kr != KERN_SUCCESS || memoryObjectSize < round_page(mAllocSize)) {
    LOG_ERROR("Failed to make memory entry (%zu bytes). %s (%x)\n", mAllocSize,
              mach_error_string(kr), kr);
    return Nothing();
  }

  return Some(std::move(port));
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

#if defined(XP_MACOSX) && defined(__x86_64__)
std::atomic<size_t> sPageSizeOverride = 0;
#endif

size_t SharedMemory::SystemPageSize() {
#if defined(XP_MACOSX) && defined(__x86_64__)
  if (sPageSizeOverride == 0) {
    if (PR_GetEnv("MOZ_SHMEM_PAGESIZE_16K")) {
      sPageSizeOverride = 16 * 1024;
    } else {
      sPageSizeOverride = sysconf(_SC_PAGESIZE);
    }
  }
  return sPageSizeOverride;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}

}  // namespace mozilla::ipc
