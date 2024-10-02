/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalTypes.h"
#include "mozilla/BitSet.h"
#include "nsTArray.h"

#include <windows.h>

namespace mozilla::hal_impl {

static mozilla::Maybe<hal::HeterogeneousCpuInfo> CreateHeterogeneousCpuInfo() {
  ULONG returnedLength;
  GetSystemCpuSetInformation(NULL, 0, &returnedLength, NULL, 0);

  if (!returnedLength) {
    return Nothing();
  }

  AutoTArray<uint8_t, 1024> cpuSets;

  cpuSets.SetLength(returnedLength);

  if (!GetSystemCpuSetInformation(
          reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(cpuSets.Elements()),
          returnedLength, &returnedLength, NULL, 0)) {
    return Nothing();
  }

  hal::HeterogeneousCpuInfo info;
  info.mTotalNumCpus = 0;

  BYTE maxEfficiencyClass = 0;
  BYTE minEfficiencyClass = UINT8_MAX;

  size_t currentPosition = 0;

  while (currentPosition < returnedLength) {
    SYSTEM_CPU_SET_INFORMATION& set =
        *reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(cpuSets.Elements() +
                                                       currentPosition);
    currentPosition += set.Size;
    if (currentPosition > returnedLength) {
      MOZ_ASSERT(false);
      return Nothing();
    }

    if (set.Type != CpuSetInformation || !set.CpuSet.Id) {
      continue;
    }
    info.mTotalNumCpus += 1;
    maxEfficiencyClass =
        std::max(maxEfficiencyClass, set.CpuSet.EfficiencyClass);
    minEfficiencyClass =
        std::min(minEfficiencyClass, set.CpuSet.EfficiencyClass);
  }

  if (!info.mTotalNumCpus) {
    // This is weird.
    return Nothing();
  }

  currentPosition = 0;
  size_t currentCPU = 0;

  // The API has currently a limit how many cpu cores it can tell about.
  while (currentPosition < returnedLength) {
    if (currentCPU >= 32) {
      break;
    }

    SYSTEM_CPU_SET_INFORMATION& set =
        *reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(cpuSets.Elements() +
                                                       currentPosition);
    currentPosition += set.Size;

    // If this happens the code above should already have bailed.
    MOZ_ASSERT(currentPosition <= returnedLength);

    // EfficiencyClass doesn't obviously translate to our model, for now what
    // we are doing is counting everything of 'max' power use as a big core,
    // everything of 'min' power use as a little code, and everything else as
    // medium.
    if (set.CpuSet.EfficiencyClass == maxEfficiencyClass) {
      info.mBigCpus[currentCPU++] = true;
    } else if (set.CpuSet.EfficiencyClass == minEfficiencyClass) {
      info.mLittleCpus[currentCPU++] = true;
    } else {
      info.mMediumCpus[currentCPU++] = true;
    }
  }

  return Some(info);
}

const Maybe<hal::HeterogeneousCpuInfo>& GetHeterogeneousCpuInfo() {
  static const Maybe<hal::HeterogeneousCpuInfo> cpuInfo =
      CreateHeterogeneousCpuInfo();
  return cpuInfo;
}

}  // namespace mozilla::hal_impl
