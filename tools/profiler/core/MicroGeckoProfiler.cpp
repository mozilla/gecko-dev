/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"
#include "ProfilerStackWalk.h"

#include "mozilla/Maybe.h"
#include "nsPrintfCString.h"
#include "public/GeckoTraceEvent.h"
#include "mozilla/ProfilerState.h"

using namespace mozilla;
using webrtc::trace_event_internal::TraceValueUnion;

void uprofiler_register_thread(const char* name, void* stacktop) {
#ifdef MOZ_GECKO_PROFILER
  profiler_register_thread(name, stacktop);
#endif  // MOZ_GECKO_PROFILER
}

void uprofiler_unregister_thread() {
#ifdef MOZ_GECKO_PROFILER
  profiler_unregister_thread();
#endif  // MOZ_GECKO_PROFILER
}

#ifdef MOZ_GECKO_PROFILER
namespace {
Maybe<MarkerTiming> ToTiming(char phase) {
  switch (phase) {
    case 'B':
      return Some(MarkerTiming::IntervalStart());
    case 'E':
      return Some(MarkerTiming::IntervalEnd());
    case 'I':
      return Some(MarkerTiming::InstantNow());
    default:
      return Nothing();
  }
}

MarkerCategory ToCategory(const char category) {
  switch (category) {
    case 'S':
      return geckoprofiler::category::SANDBOX;
    case 'M':
      return geckoprofiler::category::MEDIA_RT;
    default:
      return geckoprofiler::category::OTHER;
  }
}

struct TraceOption {
  bool mPassed = false;
  ProfilerString8View mName;
  Variant<int64_t, bool, double, ProfilerString8View> mValue = AsVariant(false);
};

struct TraceMarker {
  static constexpr int MAX_NUM_ARGS = 6;
  using OptionsType = std::tuple<TraceOption, TraceOption, TraceOption,
                                 TraceOption, TraceOption, TraceOption>;
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return MakeStringSpan("TraceEvent");
  }
  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      const OptionsType& aArgs) {
    auto writeValue = [&](const auto& aName, const auto& aVariant) {
      aVariant.match(
          [&](const int64_t& aValue) { aWriter.IntProperty(aName, aValue); },
          [&](const bool& aValue) { aWriter.BoolProperty(aName, aValue); },
          [&](const double& aValue) { aWriter.DoubleProperty(aName, aValue); },
          [&](const ProfilerString8View& aValue) {
            aWriter.StringProperty(aName, aValue);
          });
    };
    if (const auto& arg = std::get<0>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name1", arg.mName);
      writeValue("val1", arg.mValue);
    }
    if (const auto& arg = std::get<1>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name2", arg.mName);
      writeValue("val2", arg.mValue);
    }
    if (const auto& arg = std::get<2>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name3", arg.mName);
      writeValue("val3", arg.mValue);
    }
    if (const auto& arg = std::get<3>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name4", arg.mName);
      writeValue("val4", arg.mValue);
    }
    if (const auto& arg = std::get<4>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name5", arg.mName);
      writeValue("val5", arg.mValue);
    }
    if (const auto& arg = std::get<5>(aArgs); arg.mPassed) {
      aWriter.StringProperty("name6", arg.mName);
      writeValue("val6", arg.mValue);
    }
  }
  static mozilla::MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.SetChartLabel("{marker.name}");
    schema.SetTableLabel(
        "{marker.name}  {marker.data.name1} {marker.data.val1}  "
        "{marker.data.name2} {marker.data.val2}"
        "{marker.data.name3} {marker.data.val3}"
        "{marker.data.name4} {marker.data.val4}"
        "{marker.data.name5} {marker.data.val5}"
        "{marker.data.name6} {marker.data.val6}");
    schema.AddKeyLabelFormatSearchable("name1", "Key 1", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val1", "Value 1", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("name2", "Key 2", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val2", "Value 2", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("name3", "Key 3", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val3", "Value 3", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("name4", "Key 4", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val4", "Value 4", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("name5", "Key 5", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val5", "Value 5", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("name6", "Key 6", MS::Format::String,
                                       MS::Searchable::Searchable);
    schema.AddKeyLabelFormatSearchable("val6", "Value 6", MS::Format::String,
                                       MS::Searchable::Searchable);
    return schema;
  }
};
}  // namespace

namespace mozilla {
template <>
struct ProfileBufferEntryWriter::Serializer<TraceOption> {
  static Length Bytes(const TraceOption& aOption) {
    // 1 byte to store passed flag, then object size if passed.
    return aOption.mPassed ? (1 + SumBytes(aOption.mName, aOption.mValue)) : 1;
  }

  static void Write(ProfileBufferEntryWriter& aEW, const TraceOption& aOption) {
    // 'T'/'t' is just an arbitrary 1-byte value to distinguish states.
    if (aOption.mPassed) {
      aEW.WriteObject<char>('T');
      // Use the Serializer for the name/value pair.
      aEW.WriteObject(aOption.mName);
      aEW.WriteObject(aOption.mValue);
    } else {
      aEW.WriteObject<char>('t');
    }
  }
};

template <>
struct ProfileBufferEntryReader::Deserializer<TraceOption> {
  static void ReadInto(ProfileBufferEntryReader& aER, TraceOption& aOption) {
    char c = aER.ReadObject<char>();
    if ((aOption.mPassed = (c == 'T'))) {
      aER.ReadIntoObject(aOption.mName);
      aER.ReadIntoObject(aOption.mValue);
    } else {
      MOZ_ASSERT(c == 't');
    }
  }

  static TraceOption Read(ProfileBufferEntryReader& aER) {
    TraceOption option;
    ReadInto(aER, option);
    return option;
  }
};
}  // namespace mozilla
#endif  // MOZ_GECKO_PROFILER

void uprofiler_simple_event_marker_internal(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values, bool capture_stack = false,
    void* provided_stack = nullptr) {
#ifdef MOZ_GECKO_PROFILER
  if (!profiler_thread_is_being_profiled_for_markers()) {
    return;
  }
  Maybe<MarkerTiming> timing = ToTiming(phase);
  if (!timing) {
    if (getenv("MOZ_LOG_UNKNOWN_TRACE_EVENT_PHASES")) {
      fprintf(stderr, "XXX UProfiler: phase not handled: '%c'\n", phase);
    }
    return;
  }
  MOZ_ASSERT(num_args <= TraceMarker::MAX_NUM_ARGS);
  TraceMarker::OptionsType tuple;
  TraceOption* args[6] = {&std::get<0>(tuple), &std::get<1>(tuple),
                          &std::get<2>(tuple), &std::get<3>(tuple),
                          &std::get<4>(tuple), &std::get<5>(tuple)};
  for (int i = 0; i < std::min(num_args, TraceMarker::MAX_NUM_ARGS); ++i) {
    auto& arg = *args[i];
    arg.mPassed = true;
    arg.mName = ProfilerString8View::WrapNullTerminatedString(arg_names[i]);
    switch (arg_types[i]) {
      case TRACE_VALUE_TYPE_UINT:
        MOZ_ASSERT(arg_values[i] <= std::numeric_limits<int64_t>::max());
        arg.mValue = AsVariant(static_cast<int64_t>(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])->as_uint));
        break;
      case TRACE_VALUE_TYPE_INT:
        arg.mValue = AsVariant(static_cast<int64_t>(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])->as_int));
        break;
      case TRACE_VALUE_TYPE_BOOL:
        arg.mValue = AsVariant(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])->as_bool);
        break;
      case TRACE_VALUE_TYPE_DOUBLE:
        arg.mValue =
            AsVariant(reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                          ->as_double);
        break;
      case TRACE_VALUE_TYPE_POINTER:
        arg.mValue = AsVariant(ProfilerString8View(nsPrintfCString(
            "%p", reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                      ->as_pointer)));
        break;
      case TRACE_VALUE_TYPE_STRING:
        arg.mValue = AsVariant(ProfilerString8View::WrapNullTerminatedString(
            reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                ->as_string));
        break;
      case TRACE_VALUE_TYPE_COPY_STRING:
        arg.mValue = AsVariant(ProfilerString8View(
            nsCString(reinterpret_cast<const TraceValueUnion*>(&arg_values[i])
                          ->as_string)));
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected trace value type");
        arg.mValue = AsVariant(ProfilerString8View(
            nsPrintfCString("Unexpected type: %u", arg_types[i])));
        break;
    }
  }

  profiler_add_marker(
      ProfilerString8View::WrapNullTerminatedString(name), ToCategory(category),
      {timing.extract(),
       capture_stack
           ? MarkerStack::Capture(StackCaptureOptions::Full)
           : (provided_stack
                  ? MarkerStack::UseBacktrace(
                        *(static_cast<mozilla::ProfileChunkedBuffer*>(
                            provided_stack)))
                  : MarkerStack::Capture(StackCaptureOptions::NoStack))},
      TraceMarker{}, tuple);
#endif  // MOZ_GECKO_PROFILER
}

void uprofiler_simple_event_marker_capture_stack(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values) {
  uprofiler_simple_event_marker_internal(
      name, category, phase, num_args, arg_names, arg_types, arg_values, true);
}

void uprofiler_simple_event_marker_with_stack(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values, void* provided_stack) {
  MOZ_ASSERT(provided_stack != nullptr);
  uprofiler_simple_event_marker_internal(name, category, phase, num_args,
                                         arg_names, arg_types, arg_values,
                                         false, provided_stack);
}

void uprofiler_simple_event_marker(const char* name, const char category,
                                   char phase, int num_args,
                                   const char** arg_names,
                                   const unsigned char* arg_types,
                                   const unsigned long long* arg_values) {
  uprofiler_simple_event_marker_internal(name, category, phase, num_args,
                                         arg_names, arg_types, arg_values);
}

bool uprofiler_backtrace_into_buffer(NativeStack* aNativeStack, void* aBuffer) {
  return profiler_backtrace_into_buffer(
      *(static_cast<mozilla::ProfileChunkedBuffer*>(aBuffer)), *aNativeStack);
}

void uprofiler_native_backtrace(const void* top, NativeStack* nativeStack) {
  DoNativeBacktraceDirect(top, *nativeStack, nullptr);
}

bool uprofiler_is_active() { return profiler_is_active(); }

bool uprofiler_get(struct UprofilerFuncPtrs* aFuncPtrs) {
  if (!aFuncPtrs) {
    return false;
  }

  aFuncPtrs->register_thread = uprofiler_register_thread;
  aFuncPtrs->unregister_thread = uprofiler_unregister_thread;
  aFuncPtrs->simple_event_marker = uprofiler_simple_event_marker;
  aFuncPtrs->simple_event_marker_capture_stack =
      uprofiler_simple_event_marker_capture_stack;
  aFuncPtrs->simple_event_marker_with_stack =
      uprofiler_simple_event_marker_with_stack;
  aFuncPtrs->backtrace_into_buffer = uprofiler_backtrace_into_buffer;
  aFuncPtrs->native_backtrace = uprofiler_native_backtrace;
  aFuncPtrs->is_active = uprofiler_is_active;

  return true;
}
