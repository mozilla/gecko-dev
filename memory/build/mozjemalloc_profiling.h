/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZJEMALLOC_PROFILING_H
#define _MOZJEMALLOC_PROFILING_H

#include "mozilla/Atomics.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozjemalloc_types.h"
#include "mozmemory_wrap.h"

namespace mozilla {

struct PurgeStats {
  arena_id_t arena_id;
  const char* arena_label;
  const char* caller;
  size_t pages = 0;
  size_t system_calls = 0;

  PurgeStats(arena_id_t aId, const char* aLabel, const char* aCaller)
      : arena_id(aId), arena_label(aLabel), caller(aCaller) {}
};

#ifdef MOZJEMALLOC_PROFILING_CALLBACKS
class MallocProfilerCallbacks
    : public external::AtomicRefCounted<MallocProfilerCallbacks> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(MallocProfilerCallbacks)

  virtual ~MallocProfilerCallbacks() {}

  using TS = mozilla::TimeStamp;

  virtual void OnPurge(TS aStart, TS aEnd, const PurgeStats& aStats) = 0;
};

MOZ_JEMALLOC_API void jemalloc_set_profiler_callbacks(
    RefPtr<MallocProfilerCallbacks>&& aCallbacks);
#endif

}  // namespace mozilla

#endif  // ! _MOZJEMALLOC_PROFILING_H
