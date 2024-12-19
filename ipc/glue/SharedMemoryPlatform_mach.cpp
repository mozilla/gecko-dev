/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedMemoryPlatform.h"

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

namespace mozilla::ipc::shared_memory {

static inline void* toPointer(mach_vm_address_t aAddress) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(aAddress));
}

static inline mach_vm_address_t toVMAddress(void* aPointer) {
  return static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(aPointer));
}

static Maybe<PlatformHandle> CreateImpl(size_t aSize, bool aFreezable) {
  memory_object_size_t memoryObjectSize = round_page(aSize);
  PlatformHandle handle;

  kern_return_t kr =
      mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, 0,
                                MAP_MEM_NAMED_CREATE | VM_PROT_DEFAULT,
                                getter_Transfers(handle), MACH_PORT_NULL);
  if (kr != KERN_SUCCESS || memoryObjectSize < round_page(aSize)) {
    LOG_ERROR("Failed to make memory entry (%zu bytes). %s (%x)\n", aSize,
              mach_error_string(kr), kr);
    return Nothing();
  }
  return Some(std::move(handle));
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
  return mozilla::RetainMachSendRight(aHandle.get());
}

static Maybe<void*> MapMemory(size_t aSize, void* aFixedAddress,
                              const mozilla::UniqueMachSendRight& aPort,
                              bool aReadOnly) {
  kern_return_t kr;
  mach_vm_address_t address = toVMAddress(aFixedAddress);

  vm_prot_t vmProtection = VM_PROT_READ;
  if (!aReadOnly) {
    vmProtection |= VM_PROT_WRITE;
  }

  kr = mach_vm_map(mach_task_self(), &address, round_page(aSize), 0,
                   aFixedAddress ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE,
                   aPort.get(), 0, false, vmProtection, vmProtection,
                   VM_INHERIT_NONE);
  if (kr != KERN_SUCCESS) {
    if (!aFixedAddress) {
      LOG_ERROR(
          "Failed to map shared memory (%zu bytes) into %x, port %x. %s (%x)\n",
          aSize, mach_task_self(), mach_port_t(aPort.get()),
          mach_error_string(kr), kr);
    }
    return Nothing();
  }

  if (aFixedAddress && aFixedAddress != toPointer(address)) {
    kr = vm_deallocate(mach_task_self(), address, aSize);
    if (kr != KERN_SUCCESS) {
      LOG_ERROR(
          "Failed to unmap shared memory at unsuitable address "
          "(%zu bytes) from %x, port %x. %s (%x)\n",
          aSize, mach_task_self(), mach_port_t(aPort.get()),
          mach_error_string(kr), kr);
    }
    return Nothing();
  }

  return Some(toPointer(address));
}

bool Platform::Freeze(FreezableHandle& aHandle) {
  memory_object_size_t memoryObjectSize = round_page(aHandle.Size());

  mozilla::UniqueMachSendRight port;

  // Temporarily map memory (as readonly) to get an address.
  auto memory = MapMemory(memoryObjectSize, nullptr, aHandle.mHandle, true);
  if (!memory) {
    return false;
  }

  kern_return_t kr = mach_make_memory_entry_64(
      mach_task_self(), &memoryObjectSize,
      static_cast<memory_object_offset_t>(reinterpret_cast<uintptr_t>(*memory)),
      VM_PROT_READ, getter_Transfers(port), MACH_PORT_NULL);

  // Immediately try to deallocate, regardless of success.
  {
    kern_return_t kr =
        vm_deallocate(mach_task_self(), toVMAddress(*memory), memoryObjectSize);
    if (kr != KERN_SUCCESS) {
      LOG_ERROR("Failed to deallocate shared memory. %s (%x)\n",
                mach_error_string(kr), kr);
    }
  }

  if (kr != KERN_SUCCESS || memoryObjectSize < round_page(aHandle.Size())) {
    LOG_ERROR("Failed to make memory entry (%zu bytes). %s (%x)\n",
              aHandle.Size(), mach_error_string(kr), kr);
    return false;
  }

  aHandle.mHandle = std::move(port);

  return true;
}

Maybe<void*> Platform::Map(const HandleBase& aHandle, void* aFixedAddress,
                           bool aReadOnly) {
  return MapMemory(aHandle.Size(), aFixedAddress, aHandle.mHandle, aReadOnly);
}

void Platform::Unmap(void* aMemory, size_t aSize) {
  vm_address_t vm_address = toVMAddress(aMemory);
  kern_return_t kr =
      vm_deallocate(mach_task_self(), vm_address, round_page(aSize));
  if (kr != KERN_SUCCESS) {
    LOG_ERROR("Failed to deallocate shared memory. %s (%x)\n",
              mach_error_string(kr), kr);
  }
}

bool Platform::Protect(char* aAddr, size_t aSize, Access aAccess) {
  int flags = PROT_NONE;
  if (aAccess & AccessRead) flags |= PROT_READ;
  if (aAccess & AccessWrite) flags |= PROT_WRITE;

  return 0 == mprotect(aAddr, aSize, flags);
}

void* Platform::FindFreeAddressSpace(size_t aSize) {
  mach_vm_address_t address = 0;
  aSize = round_page(aSize);
  if (mach_vm_map(mach_task_self(), &address, aSize, 0, VM_FLAGS_ANYWHERE,
                  MEMORY_OBJECT_NULL, 0, false, VM_PROT_NONE, VM_PROT_NONE,
                  VM_INHERIT_NONE) != KERN_SUCCESS ||
      vm_deallocate(mach_task_self(), address, aSize) != KERN_SUCCESS) {
    return nullptr;
  }
  return toPointer(address);
}

size_t Platform::PageSize() {
#if defined(XP_MACOSX) && defined(__x86_64__)
  static std::atomic<size_t> sPageSizeOverride = 0;

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

bool Platform::IsSafeToMap(const PlatformHandle&) { return true; }

}  // namespace mozilla::ipc::shared_memory
