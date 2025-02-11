/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "memory_counter.h"

#include "mozilla/UniquePtr.h"
#include "mozmemory.h"

#include "GeckoProfiler.h"

namespace mozilla::profiler {
class MemoryCounter : public BaseProfilerCount {
 public:
  MemoryCounter()
      : BaseProfilerCount("malloc", "Memory", "Amount of allocated memory") {};

  virtual ~MemoryCounter() {
    // The counter is removed by ActivePS::Destroy()
  }

  CountSample Sample() override {
    CountSample sample = {
        .count = 0,
        .number = 0,
        .isSampleNew = true,
    };

    jemalloc_stats_lite_t mallocStats;
    jemalloc_stats_lite(&mallocStats);
    sample.count += int64_t(mallocStats.allocated_bytes);
    sample.number += mallocStats.num_operations;

    js::gc::ProfilerMemoryCounts jsStats = js::gc::GetProfilerMemoryCounts();
    sample.count += int64_t(jsStats.bytes);
    sample.number += jsStats.operations;

    return sample;
  }
};

UniquePtr<BaseProfilerCount> create_memory_counter() {
  return MakeUnique<MemoryCounter>();
}

}  // namespace mozilla::profiler
