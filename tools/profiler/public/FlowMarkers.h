/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FlowMarkers_h
#define FlowMarkers_h

#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsString.h"
#include "ETWTools.h"

namespace mozilla {

// These are convenience marker types for ad-hoc instrumentation.
// It's better to not use them and use a meaningful name for the flow.
class FlowMarker : public BaseMarkerType<FlowMarker> {
 public:
  static constexpr const char* Name = "FlowMarker";
  static constexpr const char* Description = "";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"flow", MS::InputType::Uint64, "Flow", MS::Format::Flow,
       MS::PayloadFlags::Searchable}};

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* AllLabels =
      "{marker.name} (flow={marker.data.flow})";

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Generic;

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter, Flow aFlow) {
    aWriter.FlowProperty("flow", aFlow);
  }
};

class FlowStackMarker : public BaseMarkerType<FlowStackMarker> {
 public:
  static constexpr const char* Name = "FlowStackMarker";
  static constexpr const char* Description = "";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"flow", MS::InputType::Uint64, "Flow", MS::Format::Flow,
       MS::PayloadFlags::Searchable}};

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* AllLabels =
      "{marker.name} (flow={marker.data.flow})";

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Generic;

  static constexpr bool IsStackBased = true;

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter, Flow aFlow) {
    aWriter.FlowProperty("flow", aFlow);
  }
};

class TerminatingFlowStackMarker
    : public BaseMarkerType<TerminatingFlowStackMarker> {
 public:
  static constexpr const char* Name = "TerminatingFlowStackMarker";
  static constexpr const char* Description = "";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"flow", MS::InputType::Uint64, "Flow", MS::Format::TerminatingFlow,
       MS::PayloadFlags::Searchable}};

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* AllLabels =
      "{marker.name} (flow={marker.data.flow})";

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Generic;

  static constexpr bool IsStackBased = true;

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter, Flow aFlow) {
    aWriter.FlowProperty("flow", aFlow);
  }
};

class FlowTextMarker : public BaseMarkerType<FlowTextMarker> {
 public:
  static constexpr const char* Name = "FlowTextMarker";
  static constexpr const char* Description = "";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"name", MS::InputType::CString, "Details", MS::Format::String,
       MS::PayloadFlags::Searchable},
      {"flow", MS::InputType::Uint64, "Flow", MS::Format::Flow,
       MS::PayloadFlags::Searchable}};

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* TableLabel =
      "{marker.name} - {marker.data.name}(flow={marker.data.flow})";
  static constexpr const char* ChartLabel = "{marker.name}";

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Generic;

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      const ProfilerString8View& aText, Flow aFlow) {
    aWriter.StringProperty("name", aText);
    aWriter.FlowProperty("flow", aFlow);
  }
};

class TerminatingFlowMarker : public BaseMarkerType<TerminatingFlowMarker> {
 public:
  static constexpr const char* Name = "TerminatingFlowMarker";
  static constexpr const char* Description = "";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"terminatingFlow", MS::InputType::Uint64, "Terminating Flow",
       MS::Format::TerminatingFlow, MS::PayloadFlags::Searchable}};

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* AllLabels =
      "{marker.name} (terminatingFlow={marker.data.terminatingFlow})";

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Generic;

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter, Flow aFlow) {
    aWriter.FlowProperty("terminatingFlow", aFlow);
  }
};

class TerminatingFlowTextMarker
    : public BaseMarkerType<TerminatingFlowTextMarker> {
 public:
  static constexpr const char* Name = "TerminatingFlowTextMarker";
  static constexpr const char* Description =
      "Generic text marker with terminating flow";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"name", MS::InputType::CString, "Details", MS::Format::String,
       MS::PayloadFlags::Searchable},
      {"terminatingFlow", MS::InputType::Uint64, "Terminating Flow",
       MS::Format::TerminatingFlow, MS::PayloadFlags::Searchable}};

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* TableLabel =
      "{marker.name} - "
      "{marker.data.name}(terminatingFlow={marker.data.terminatingFlow})";
  static constexpr const char* ChartLabel = "{marker.name}";

  static constexpr MS::ETWMarkerGroup Group = MS::ETWMarkerGroup::Generic;

  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      const ProfilerString8View& aText, Flow aFlow) {
    aWriter.StringProperty("name", aText);
    aWriter.FlowProperty("terminatingFlow", aFlow);
  }
};

class MOZ_RAII AutoProfilerFlowMarker {
 public:
  AutoProfilerFlowMarker(const char* aMarkerName,
                         const mozilla::MarkerCategory& aCategory, Flow aFlow)
      : mMarkerName(aMarkerName), mCategory(aCategory), mFlow(aFlow) {
    MOZ_ASSERT(mOptions.Timing().EndTime().IsNull(),
               "AutoProfilerTextMarker options shouldn't have an end time");
    if (profiler_is_active_and_unpaused() &&
        mOptions.Timing().StartTime().IsNull()) {
      mOptions.Set(mozilla::MarkerTiming::InstantNow());
    }
  }

  ~AutoProfilerFlowMarker() {
    if (profiler_is_active_and_unpaused()) {
      mOptions.TimingRef().SetIntervalEnd();
      profiler_add_marker(
          mozilla::ProfilerString8View::WrapNullTerminatedString(mMarkerName),
          mCategory, std::move(mOptions), FlowStackMarker{}, mFlow);
    }
  }

 public:
  const char* mMarkerName;
  mozilla::MarkerCategory mCategory;
  mozilla::MarkerOptions mOptions;
  Flow mFlow;
};

class MOZ_RAII AutoProfilerTerminatingFlowMarker {
 public:
  AutoProfilerTerminatingFlowMarker(const char* aMarkerName,
                                    const mozilla::MarkerCategory& aCategory,
                                    Flow aFlow)
      : mMarkerName(aMarkerName), mCategory(aCategory), mFlow(aFlow) {
    MOZ_ASSERT(mOptions.Timing().EndTime().IsNull(),
               "AutoProfilerTextMarker options shouldn't have an end time");
    if (profiler_is_active_and_unpaused() &&
        mOptions.Timing().StartTime().IsNull()) {
      mOptions.Set(mozilla::MarkerTiming::InstantNow());
    }
  }

  ~AutoProfilerTerminatingFlowMarker() {
    if (profiler_is_active_and_unpaused()) {
      mOptions.TimingRef().SetIntervalEnd();
      profiler_add_marker(
          mozilla::ProfilerString8View::WrapNullTerminatedString(mMarkerName),
          mCategory, std::move(mOptions), TerminatingFlowStackMarker{}, mFlow);
    }
  }

 public:
  const char* mMarkerName;
  mozilla::MarkerCategory mCategory;
  mozilla::MarkerOptions mOptions;
  Flow mFlow;
};

#define AUTO_PROFILER_FLOW_MARKER(markerName, categoryName, flow) \
  AutoProfilerFlowMarker PROFILER_RAII(                           \
      markerName, ::mozilla::baseprofiler::category::categoryName, flow)

#define AUTO_PROFILER_TERMINATING_FLOW_MARKER(markerName, categoryName, flow) \
  AutoProfilerTerminatingFlowMarker PROFILER_RAII(                            \
      markerName, ::mozilla::baseprofiler::category::categoryName, flow)

}  // namespace mozilla
#endif  // FlowMarkers_h
