/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BaseProfilerMarkersPrerequisites.h"
#include "mozilla/ProfilerMarkers.h"

namespace mozilla {

// Following markers are used for HTMLMediaElement

struct TimeUpdateMarker : public BaseMarkerType<TimeUpdateMarker> {
  static constexpr const char* Name = "HTMLMediaElement:Timeupdate";
  static constexpr const char* Description =
      "A marker shows the current playback position";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"currentTimeMs", MS::InputType::Uint64, "Current Time (Ms)",
       MS::Format::Milliseconds},
      {"mediaDurationMs", MS::InputType::Uint64, "Media Duration (Ms)",
       MS::Format::Milliseconds},
      {"paintedFrames", MS::InputType::Uint32, "Painted Frames",
       MS::Format::Integer},  // optional, zero for audio
  };
  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   uint64_t aCurrentTime, uint64_t aDuration,
                                   uint32_t aPaintedFrames) {
    aWriter.IntProperty("currentTimeMs", aCurrentTime);
    aWriter.IntProperty("mediaDurationMs", aDuration);
    if (aPaintedFrames != 0) {
      aWriter.IntProperty("paintedFrames", aPaintedFrames);
    }
  }
};

struct BufferedUpdateMarker : public BaseMarkerType<BufferedUpdateMarker> {
  static constexpr const char* Name = "HTMLMediaElement:BufferedUpdate";
  static constexpr const char* Description =
      "A marker shows the current buffered ranges";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"bufferStartMs", MS::InputType::Uint64, "Buffer Start (Ms)",
       MS::Format::Milliseconds},
      {"bufferEndMs", MS::InputType::Uint64, "Buffer End (Ms)",
       MS::Format::Milliseconds},
      {"mediaDurationMs", MS::InputType::Uint64, "Media Duration (Ms)",
       MS::Format::Milliseconds},
  };

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   uint64_t aBufferStart, uint64_t aBufferEnd,
                                   uint64_t aDuration) {
    aWriter.IntProperty("bufferStartMs", aBufferStart);
    aWriter.IntProperty("bufferEndMs", aBufferEnd);
    aWriter.IntProperty("mediaDurationMs", aDuration);
  }
};

struct VideoResizeMarker : public BaseMarkerType<VideoResizeMarker> {
  static constexpr const char* Name = "HTMLMediaElement:VideoResize";
  static constexpr const char* Description =
      "A marker shows the current displayed size of the video element";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"width", MS::InputType::Uint64, "Width", MS::Format::Integer},
      {"height", MS::InputType::Uint64, "Height", MS::Format::Integer},
  };

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   uint64_t aWidth, uint64_t aHeight) {
    aWriter.IntProperty("width", aWidth);
    aWriter.IntProperty("height", aHeight);
  }
};

struct MetadataMarker : public BaseMarkerType<MetadataMarker> {
  static constexpr const char* Name = "HTMLMediaElement:MetadataLoaded";
  static constexpr const char* Description =
      "A marker shows the current metadata of the video element";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"src", MS::InputType::String, "Source URL", MS::Format::String},
      {"audioMimeType", MS::InputType::CString, "Audio Mimetype",
       MS::Format::String},
      {"videoMimeType", MS::InputType::CString, "Video Mimetype",
       MS::Format::String},
  };

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString16View& aSrc,
                                   const ProfilerString8View& aAudioMimeType,
                                   const ProfilerString8View& aVideoMimeType) {
    StreamJSONMarkerDataImpl(aWriter, aSrc, aAudioMimeType, aVideoMimeType);
  }
};

struct CDMResolvedMarker : public BaseMarkerType<CDMResolvedMarker> {
  static constexpr const char* Name = "HTMLMediaElement:CDMResolved";
  static constexpr const char* Description =
      "A marker shows the supported config for a resolved CDM";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"keySystem", MS::InputType::String, "Key System", MS::Format::String},
      {"configuration", MS::InputType::CString, "Configuration",
       MS::Format::String},
  };

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString16View& aKeySystem,
                                   const ProfilerString8View& aConfiguration) {
    StreamJSONMarkerDataImpl(aWriter, aKeySystem, aConfiguration);
  }
};

struct LoadErrorMarker : public BaseMarkerType<LoadErrorMarker> {
  static constexpr const char* Name = "HTMLMediaElement:Error";
  static constexpr const char* Description =
      "A marker shows the detail of the load error";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"errorMessage", MS::InputType::CString, "Error Message",
       MS::Format::String},
  };
  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aErrorMsg) {
    StreamJSONMarkerDataImpl(aWriter, aErrorMsg);
  }
};

struct ErrorMarker : public BaseMarkerType<ErrorMarker> {
  static constexpr const char* Name = "HTMLMediaElement:Error";
  static constexpr const char* Description =
      "A marker shows the detail of the error";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"errorMessage", MS::InputType::String, "Error Message",
       MS::Format::String},
  };
  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString16View& aErrorMsg) {
    StreamJSONMarkerDataImpl(aWriter, aErrorMsg);
  }
};

struct LoadSourceMarker : public BaseMarkerType<LoadSourceMarker> {
  static constexpr const char* Name = "HTMLMediaElement:LoadSource";
  static constexpr const char* Description =
      "A marker shows the detail of the source a media element trying to load";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"src", MS::InputType::String, "Source URL", MS::Format::String},
      // Below attributes would only be set if the source if a source element
      {"contentType", MS::InputType::String, "Content Type",
       MS::Format::String},
      {"media", MS::InputType::String, "Media", MS::Format::String},
  };

  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString16View& aSrc,
                                   const ProfilerString16View& aType,
                                   const ProfilerString16View& aMedia) {
    StreamJSONMarkerDataImpl(aWriter, aSrc, aType, aMedia);
  }
};

// This marker is for HTMLVideoElement
struct RenderVideoMarker : public BaseMarkerType<RenderVideoMarker> {
  static constexpr const char* Name = "HTMLMediaElement:RenderVideo";
  static constexpr const char* Description =
      "A marker shows how many video frames has been painted";

  using MS = MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"paintedFrames", MS::InputType::Uint64, "Painted Frames",
       MS::Format::Integer},
  };
  static constexpr MS::Location Locations[] = {MS::Location::MarkerChart,
                                               MS::Location::MarkerTable};
  static constexpr const char* ChartLabel = "{marker.data.name}";
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   uint64_t aPaintedFrames) {
    aWriter.IntProperty("paintedFrames", aPaintedFrames);
  }
};

}  // namespace mozilla
