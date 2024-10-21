/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SharedMemoryImpl_mach.h"

#include <map>

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
#include <unistd.h>

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

SharedMemoryImpl::SharedMemoryImpl()
    : mPort(MACH_PORT_NULL), mMemory(nullptr), mOpenRights(RightsReadWrite) {}

bool SharedMemoryImpl::SetHandle(Handle aHandle, OpenRights aRights) {
  MOZ_ASSERT(mPort == MACH_PORT_NULL, "already initialized");

  mPort = std::move(aHandle);
  mOpenRights = aRights;
  return true;
}

static inline void* toPointer(mach_vm_address_t address) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

static inline mach_vm_address_t toVMAddress(void* pointer) {
  return static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(pointer));
}

bool SharedMemoryImpl::CreateImpl(size_t size) {
  MOZ_ASSERT(mPort == MACH_PORT_NULL, "already initialized");

  memory_object_size_t memoryObjectSize = round_page(size);

  kern_return_t kr =
      mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, 0,
                                MAP_MEM_NAMED_CREATE | VM_PROT_DEFAULT,
                                getter_Transfers(mPort), MACH_PORT_NULL);
  if (kr != KERN_SUCCESS || memoryObjectSize < round_page(size)) {
    LOG_ERROR("Failed to make memory entry (%zu bytes). %s (%x)\n", size,
              mach_error_string(kr), kr);
    TakeHandle();
    return false;
  }
  return true;
}

bool SharedMemoryImpl::MapImpl(size_t size, void* fixedAddress) {
  MOZ_ASSERT(mMemory == nullptr);

  if (MACH_PORT_NULL == mPort) {
    return false;
  }

  kern_return_t kr;
  mach_vm_address_t address = toVMAddress(fixedAddress);

  vm_prot_t vmProtection = VM_PROT_READ;
  if (mOpenRights == RightsReadWrite) {
    vmProtection |= VM_PROT_WRITE;
  }

  kr = mach_vm_map(mach_task_self(), &address, round_page(size), 0,
                   fixedAddress ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE,
                   mPort.get(), 0, false, vmProtection, vmProtection,
                   VM_INHERIT_NONE);
  if (kr != KERN_SUCCESS) {
    if (!fixedAddress) {
      LOG_ERROR(
          "Failed to map shared memory (%zu bytes) into %x, port %x. %s (%x)\n",
          size, mach_task_self(), mach_port_t(mPort.get()),
          mach_error_string(kr), kr);
    }
    return false;
  }

  if (fixedAddress && fixedAddress != toPointer(address)) {
    kr = vm_deallocate(mach_task_self(), address, size);
    if (kr != KERN_SUCCESS) {
      LOG_ERROR(
          "Failed to unmap shared memory at unsuitable address "
          "(%zu bytes) from %x, port %x. %s (%x)\n",
          size, mach_task_self(), mach_port_t(mPort.get()),
          mach_error_string(kr), kr);
    }
    return false;
  }

  mMemory = toPointer(address);
  return true;
}

void* SharedMemoryImpl::FindFreeAddressSpace(size_t size) {
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

auto SharedMemoryImpl::CloneHandle() -> Handle {
  return mozilla::RetainMachSendRight(mPort.get());
}

auto SharedMemoryImpl::TakeHandle() -> Handle {
  mOpenRights = RightsReadWrite;
  return std::move(mPort);
}

void SharedMemoryImpl::UnmapImpl(size_t mappedSize) {
  if (!mMemory) {
    return;
  }
  vm_address_t address = toVMAddress(mMemory);
  kern_return_t kr =
      vm_deallocate(mach_task_self(), address, round_page(mappedSize));
  if (kr != KERN_SUCCESS) {
    LOG_ERROR("Failed to deallocate shared memory. %s (%x)\n",
              mach_error_string(kr), kr);
    return;
  }
  mMemory = nullptr;
}

void* SharedMemoryImpl::MemoryImpl() const { return mMemory; }

bool SharedMemoryImpl::IsHandleValid(const Handle& aHandle) const {
  return aHandle != nullptr;
}

auto SharedMemoryImpl::NULLHandle() -> Handle { return Handle(); }

}  // namespace mozilla::ipc
