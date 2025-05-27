/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "memory_markers.h"

#include "mozmemory.h"
#include "mozjemalloc_profiling.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ProfilerMarkers.h"

namespace geckoprofiler::markers {
struct PurgeArenaMarker : mozilla::BaseMarkerType<PurgeArenaMarker> {
  static constexpr const char* Name = "PurgeArena";

  static constexpr const char* Description =
      "Purge dirtied pages from the resident memory set";

  using MS = mozilla::MarkerSchema;
  using String8View = mozilla::ProfilerString8View;

  static constexpr MS::PayloadField PayloadFields[] = {
      {"id", MS::InputType::Uint32, "Arena Id", MS::Format::Integer},
      {"label", MS::InputType::CString, "Arena", MS::Format::String},
      {"caller", MS::InputType::CString, "Caller", MS::Format::String},
      {"pages", MS::InputType::Uint32, "Number of pages", MS::Format::Integer},
      {"syscalls", MS::InputType::Uint32, "Number of system calls",
       MS::Format::Integer}};

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter, uint32_t aId,
      const String8View& aLabel, const String8View& aCaller, uint32_t aPages,
      uint32_t aSyscalls) {
    aWriter.IntProperty("id", aId);
    aWriter.StringProperty("label", aLabel);
    aWriter.StringProperty("caller", aCaller);
    aWriter.IntProperty("pages", aPages);
    aWriter.IntProperty("syscalls", aSyscalls);
  }

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
};
}  // namespace geckoprofiler::markers

namespace mozilla {
namespace profiler {

class GeckoProfilerMallocCallbacks : public MallocProfilerCallbacks {
 public:
  virtual void OnPurge(TimeStamp aStart, TimeStamp aEnd,
                       const PurgeStats& aStats) override {
    PROFILER_MARKER(
        "PurgeArena", GCCC, MarkerTiming::Interval(aStart, aEnd),
        PurgeArenaMarker, aStats.arena_id,
        ProfilerString8View::WrapNullTerminatedString(aStats.arena_label),
        ProfilerString8View::WrapNullTerminatedString(aStats.caller),
        aStats.pages, aStats.system_calls);
  }
};
}  // namespace profiler

void register_profiler_memory_callbacks() {
  auto val = MakeRefPtr<profiler::GeckoProfilerMallocCallbacks>();
  jemalloc_set_profiler_callbacks(val);
}

void unregister_profiler_memory_callbacks() {
  jemalloc_set_profiler_callbacks(nullptr);
}

}  // namespace mozilla
