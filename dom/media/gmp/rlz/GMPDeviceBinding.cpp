/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDeviceBinding.h"
#include "mozilla/Attributes.h"
#include "prenv.h"

#include <string>

#ifdef XP_WIN
#include "windows.h"
#ifdef MOZ_SANDBOX
#include <intrin.h>
#include <assert.h>
#endif
#endif

#if defined(HASH_NODE_ID_WITH_DEVICE_ID)

// In order to provide EME plugins with a "device binding" capability,
// in the parent we generate and store some random bytes as salt for every
// (origin, urlBarOrigin) pair that uses EME. We store these bytes so
// that every time we revisit the same origin we get the same salt.
// We send this salt to the child on startup. The child collects some
// device specific data and munges that with the salt to create the
// "node id" that we expose to EME plugins. It then overwrites the device
// specific data, and activates the sandbox.

#include "rlz/lib/machine_id.h"
#include "rlz/lib/string_utils.h"
#include "sha256.h"

#ifdef XP_WIN
#include "windows.h"
#ifdef MOZ_SANDBOX
#include <intrin.h>
#include <assert.h>
#endif
#endif

#ifdef XP_MACOSX
#include <assert.h>
#ifdef HASH_NODE_ID_WITH_DEVICE_ID
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#endif

#endif // HASH_NODE_ID_WITH_DEVICE_ID

namespace mozilla {
namespace gmp {

#if defined(XP_WIN) && defined(HASH_NODE_ID_WITH_DEVICE_ID)
MOZ_NEVER_INLINE
static bool
GetStackAfterCurrentFrame(uint8_t** aOutTop, uint8_t** aOutBottom)
{
  // "Top" of the free space on the stack is directly after the memory
  // holding our return address.
  uint8_t* top = (uint8_t*)_AddressOfReturnAddress();

  // Look down the stack until we find the guard page...
  MEMORY_BASIC_INFORMATION memInfo = {0};
  uint8_t* bottom = top;
  while (1) {
    if (!VirtualQuery(bottom, &memInfo, sizeof(memInfo))) {
      return false;
    }
    if ((memInfo.Protect & PAGE_GUARD) == PAGE_GUARD) {
      bottom = (uint8_t*)memInfo.BaseAddress + memInfo.RegionSize;
#ifdef DEBUG
      if (!VirtualQuery(bottom, &memInfo, sizeof(memInfo))) {
        return false;
      }
      assert(!(memInfo.Protect & PAGE_GUARD)); // Should have found boundary.
#endif
      break;
    } else if (memInfo.State != MEM_COMMIT ||
               (memInfo.AllocationProtect & PAGE_READWRITE) != PAGE_READWRITE) {
      return false;
    }
    bottom = (uint8_t*)memInfo.BaseAddress - 1;
  }
  *aOutTop = top;
  *aOutBottom = bottom;
  return true;
}
#endif

#if defined(XP_MACOSX) && defined(HASH_NODE_ID_WITH_DEVICE_ID)
static mach_vm_address_t
RegionContainingAddress(mach_vm_address_t aAddress)
{
  mach_port_t task;
  kern_return_t kr = task_for_pid(mach_task_self(), getpid(), &task);
  if (kr != KERN_SUCCESS) {
    return 0;
  }

  mach_vm_address_t address = aAddress;
  mach_vm_size_t size;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name;
  kr = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO_64,
                      reinterpret_cast<vm_region_info_t>(&info), &count,
                      &object_name);
  if (kr != KERN_SUCCESS || size == 0
      || address > aAddress || address + size <= aAddress) {
    // mach_vm_region failed, or couldn't find region at given address.
    return 0;
  }

  return address;
}

MOZ_NEVER_INLINE
static bool
GetStackAfterCurrentFrame(uint8_t** aOutTop, uint8_t** aOutBottom)
{
  mach_vm_address_t stackFrame =
    reinterpret_cast<mach_vm_address_t>(__builtin_frame_address(0));
  *aOutTop = reinterpret_cast<uint8_t*>(stackFrame);
  // Kernel code shows that stack is always a single region.
  *aOutBottom = reinterpret_cast<uint8_t*>(RegionContainingAddress(stackFrame));
  return *aOutBottom && (*aOutBottom < *aOutTop);
}
#endif

#ifdef HASH_NODE_ID_WITH_DEVICE_ID
static void SecureMemset(void* start, uint8_t value, size_t size)
{
  // Inline instructions equivalent to RtlSecureZeroMemory().
  for (size_t i = 0; i < size; ++i) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(start) + i;
    *p = value;
  }
}
#endif

bool
CalculateGMPDeviceId(char* aOriginSalt,
                     uint32_t aOriginSaltLen,
                     std::string& aOutNodeId)
{
#ifdef HASH_NODE_ID_WITH_DEVICE_ID
  if (aOriginSaltLen > 0) {
    std::vector<uint8_t> deviceId;
    int volumeId;
    if (!rlz_lib::GetRawMachineId(&deviceId, &volumeId)) {
      return false;
    }

    SHA256Context ctx;
    SHA256_Begin(&ctx);
    SHA256_Update(&ctx, (const uint8_t*)aOriginSalt, aOriginSaltLen);
    SHA256_Update(&ctx, deviceId.data(), deviceId.size());
    SHA256_Update(&ctx, (const uint8_t*)&volumeId, sizeof(int));
    uint8_t digest[SHA256_LENGTH] = {0};
    unsigned int digestLen = 0;
    SHA256_End(&ctx, digest, &digestLen, SHA256_LENGTH);

    // Overwrite all data involved in calculation as it could potentially
    // identify the user, so there's no chance a GMP can read it and use
    // it for identity tracking.
    SecureMemset(&ctx, 0, sizeof(ctx));
    SecureMemset(aOriginSalt, 0, aOriginSaltLen);
    SecureMemset(&volumeId, 0, sizeof(volumeId));
    SecureMemset(deviceId.data(), '*', deviceId.size());
    deviceId.clear();

    if (!rlz_lib::BytesToString(digest, SHA256_LENGTH, &aOutNodeId)) {
      return false;
    }

    if (!PR_GetEnv("MOZ_GMP_DISABLE_NODE_ID_CLEANUP")) {
      // We've successfully bound the origin salt to node id.
      // rlz_lib::GetRawMachineId and/or the system functions it
      // called could have left user identifiable data on the stack,
      // so carefully zero the stack down to the guard page.
      uint8_t* top;
      uint8_t* bottom;
      if (!GetStackAfterCurrentFrame(&top, &bottom)) {
        return false;
      }
      assert(top >= bottom);
      // Inline instructions equivalent to RtlSecureZeroMemory().
      // We can't just use RtlSecureZeroMemory here directly, as in debug
      // builds, RtlSecureZeroMemory() can't be inlined, and the stack
      // memory it uses would get wiped by itself running, causing crashes.
      for (volatile uint8_t* p = (volatile uint8_t*)bottom; p < top; p++) {
        *p = 0;
      }
    }
  } else
#endif
  {
    aOutNodeId = std::string(aOriginSalt, aOriginSalt + aOriginSaltLen);
  }
  return true;
}

} // namespace gmp
} // namespace mozilla
