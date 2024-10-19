/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MLUtils.h"

#include "prsystem.h"
#include "mozilla/Casting.h"
#include <sys/types.h>

#if defined(XP_WIN)
#  include <sysinfoapi.h>
#endif
#if defined(XP_MACOSX)
#  include <sys/sysctl.h>
#  include <mach/mach.h>
#endif
#if defined(XP_LINUX)
#  include <sys/sysinfo.h>
#endif

namespace mozilla::ml {

NS_IMPL_ISUPPORTS(MLUtils, nsIMLUtils)

/**
 * See nsIMLUtils for the method documentation.
 */
NS_IMETHODIMP MLUtils::HasEnoughMemoryToInfer(uint64_t aModelSizeInMemory,
                                              uint32_t aThresholdPercentage,
                                              uint64_t aMinMemoryRequirement,
                                              bool* _retval) {
  // Check for the physical memory. On devices with less than
  // `aMinMemoryRequirement`, we give up.
  uint64_t totalMemory = PR_GetPhysicalMemorySize();

  if (totalMemory <= aMinMemoryRequirement) {
    // We are not doing inference if the device has less than what is required.
    *_retval = false;
    return NS_OK;
  }

  // Ensure threshold is valid and within 0-100 range
  if (aThresholdPercentage <= 0 || aThresholdPercentage > 100) {
    *_retval = false;
    return NS_ERROR_FAILURE;  // throw an error
  }

  // Convert the threshold percentage to a usable value (e.g., 80% becomes 0.8)
  double threshold = static_cast<double>(aThresholdPercentage) / 100.0f;

  // Determin the available resident memory on different platforms.
  uint64_t availableResidentMemory = 0;
#if defined(XP_WIN)
  MEMORYSTATUSEX memStatus = {sizeof(memStatus)};

  if (GlobalMemoryStatusEx(&memStatus)) {
    availableResidentMemory = memStatus.ullAvailPhys;
  } else {
    return NS_ERROR_FAILURE;
  }
#endif

#if defined(XP_MACOSX)
  mach_port_t host_port = mach_host_self();
  vm_size_t page_size;
  vm_statistics64_data_t vm_stats;
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

  if (host_page_size(host_port, &page_size) != KERN_SUCCESS ||
      host_statistics64(host_port, HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&vm_stats),
                        &count) != KERN_SUCCESS) {
    *_retval = false;
    return NS_ERROR_FAILURE;
  }

  availableResidentMemory =
      static_cast<uint64_t>(vm_stats.free_count + vm_stats.inactive_count) *
      page_size;
#endif

#if defined(XP_LINUX)
  struct sysinfo memInfo {
    0
  };
  if (sysinfo(&memInfo) != 0) {
    *_retval = false;
    return NS_ERROR_FAILURE;
  }
  availableResidentMemory = memInfo.freeram * memInfo.mem_unit;
#endif

  // Check if the modelSize fits within memory using the threshold
  *_retval = AssertedCast<double>(aModelSizeInMemory) <=
             AssertedCast<double>(availableResidentMemory) * threshold;
  return NS_OK;
}

}  // namespace mozilla::ml
