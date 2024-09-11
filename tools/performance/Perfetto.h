/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Perfetto_h
#define mozilla_Perfetto_h

#ifdef MOZ_PERFETTO
#  include "mozilla/BaseProfilerMarkers.h"
#  include "mozilla/Span.h"
#  include "mozilla/TimeStamp.h"
#  include "nsString.h"
#  include "nsPrintfCString.h"
#  include "perfetto/perfetto.h"

// Initialization is called when a content process is created.
// This can be called multiple times.
extern void InitPerfetto();

/* Perfetto Tracing:
 *
 * This file provides an interface to the perfetto tracing API.  The API from
 * the perfetto sdk can be used directly, but an additional set of macros
 * prefixed with PERFETTO_* have been defined to help minimize the use of
 * ifdef's.
 *
 * The common perfetto macros require a category and name at the very least.
 * These must be static strings, or wrapped with perfetto::DynamicString if
 * dynamic. If the string is static, but provided through a runtime pointer,
 * then it must be wrapped with perfetto::StaticString.
 *
 * You can also provide additional parameters such as a timestamp,
 * or a lambda to add additional information to the trace marker.
 * For more info, see https://perfetto.dev/docs/instrumentation/tracing-sdk
 *
 * Examples:
 *
 *  // Add a trace event to measure work inside a block,
 *  // using static strings only.
 *
 *  {
 *    PERFETTO_TRACE_EVENT("js", "JS::RunScript");
 *    run_script();
 *  }
 *
 *  // Add a trace event to measure work inside a block,
 *  // using a dynamic string.
 *
 *  void runScript(nsCString& scriptName)
 *  {
 *    PERFETTO_TRACE_EVENT("js", perfetto::DynamicString{scriptName.get()});
 *    run_script();
 *  }
 *
 *  // Add a trace event using a dynamic category and name.
 *
 *  void runScript(nsCString& categoryName, nsCString& scriptName)
 *  {
 *    perfetto::DynamicCategory category{category.get()};
 *    PERFETTO_TRACE_EVENT(category, perfetto::DynamicString{scriptName.get()});
 *    run_script();
 *  }
 *
 *  // Add a trace event to measure two arbitrary points of code.
 *  // Events in the same category must always be nested.
 *
 *  void startWork() {
 *    PERFETTO_TRACE_EVENT_BEGIN("js", "StartWork");
 *    ...
 *    PERFETTO_TRACE_EVENT_END("js");
 *  }
 *
 *  // Create a trace marker for an event that has already occurred
 *  // using previously saved timestamps.
 *
 *  void record_event(TimeStamp startTimeStamp, TimeStamp endTimeStamp)
 *  {
 *    PERFETTO_TRACE_EVENT_BEGIN("js", "Some Event", startTimeStamp);
 *    PERFETTO_TRACE_EVENT_END("js", endTimeStamp);
 *  }
 */

// Wrap the common trace event macros from perfetto so
// they can be called without #ifdef's.
#  define PERFETTO_TRACE_EVENT(...) TRACE_EVENT(__VA_ARGS__)
#  define PERFETTO_TRACE_EVENT_BEGIN(...) TRACE_EVENT_BEGIN(__VA_ARGS__)
#  define PERFETTO_TRACE_EVENT_END(...) TRACE_EVENT_END(__VA_ARGS__)
#  define PERFETTO_TRACE_EVENT_INSTANT(...) TRACE_EVENT_INSTANT(__VA_ARGS__)

namespace perfetto {
// Specialize custom timestamps for mozilla::TimeStamp.
template <>
struct TraceTimestampTraits<mozilla::TimeStamp> {
  static inline TraceTimestamp ConvertTimestampToTraceTimeNs(
      const mozilla::TimeStamp& timestamp) {
    return {protos::gen::BuiltinClock::BUILTIN_CLOCK_MONOTONIC,
            timestamp.RawClockMonotonicNanosecondsSinceBoot()};
  }
};
}  // namespace perfetto

// Categories can be added dynamically, but to minimize overhead
// all categories should be pre-defined here whenever possible.
PERFETTO_DEFINE_CATEGORIES(perfetto::Category("task"),
                           perfetto::Category("usertiming"));

template <typename T, typename = void>
struct MarkerHasPayloadFields : std::false_type {};
template <typename T>
struct MarkerHasPayloadFields<T, std::void_t<decltype(T::PayloadFields)>>
    : std::true_type {};

using MS = mozilla::MarkerSchema;

// Primary template.  Assert if a payload type has not been specialized so we
// don't miss payload information.
template <typename T, typename Enable = void>
struct AddDebugAnnotationImpl {
  static void call(perfetto::EventContext& ctx, const char* const aKey,
                   const T& aValue) {
    static_assert(false,
                  "Unsupported payload type for perfetto debug annotations.");
  }
};

// Do nothing for these types.
template <>
struct AddDebugAnnotationImpl<mozilla::Nothing> {
  static void call(perfetto::EventContext& ctx, const char* const aKey,
                   const mozilla::Nothing& aValue) {}
};

template <>
struct AddDebugAnnotationImpl<std::nullptr_t> {
  static void call(perfetto::EventContext& ctx, const char* const aKey,
                   const std::nullptr_t& aValue) {}
};

// Specialize mozilla::Maybe<>
template <typename T>
struct AddDebugAnnotationImpl<mozilla::Maybe<T>> {
  static void call(perfetto::EventContext& ctx, const char* const aKey,
                   const mozilla::Maybe<T>& aValue) {
    if (aValue.isNothing()) {
      return;
    }
    AddDebugAnnotationImpl<T>::call(ctx, aKey, *aValue);
  }
};

// Specialize integral types.
template <typename T>
struct AddDebugAnnotationImpl<T, std::enable_if_t<std::is_integral_v<T>>> {
  static void call(perfetto::EventContext& ctx, const char* const aKey,
                   const T& aValue) {
    auto* arg = ctx.event()->add_debug_annotations();
    arg->set_name(aKey);

    if constexpr (std::is_same_v<T, bool>) {
      arg->set_bool_value(static_cast<uint64_t>(aValue));
    } else if constexpr (std::is_signed_v<T>) {
      arg->set_int_value(static_cast<uint64_t>(aValue));
    } else {
      static_assert(std::is_unsigned_v<T>);
      arg->set_uint_value(static_cast<uint64_t>(aValue));
    }
  }
};

// Specialize time durations.
template <>
struct AddDebugAnnotationImpl<
    mozilla::BaseTimeDuration<mozilla::TimeDurationValueCalculator>> {
  static void call(
      perfetto::EventContext& ctx, const char* const aKey,
      const mozilla::BaseTimeDuration<mozilla::TimeDurationValueCalculator>&
          aValue) {
    auto* arg = ctx.event()->add_debug_annotations();
    arg->set_name(aKey);
    arg->set_uint_value(static_cast<uint64_t>(aValue.ToMilliseconds()));
  }
};

// Specialize the various string representations.
#  define ADD_DEBUG_STRING_ANNOTATION_IMPL(templatetype, stringtype,        \
                                           paramtype, getter)               \
    template <templatetype>                                                 \
    struct AddDebugAnnotationImpl<stringtype> {                             \
      static void call(perfetto::EventContext& ctx, const char* const aKey, \
                       const paramtype aValue) {                            \
        auto* arg = ctx.event()->add_debug_annotations();                   \
        arg->set_name(aKey);                                                \
        arg->set_string_value(getter);                                      \
      }                                                                     \
    };

#  define ADD_DEBUG_STRING_ANNOTATION(type, getter) \
    ADD_DEBUG_STRING_ANNOTATION_IMPL(, type, type&, getter)

ADD_DEBUG_STRING_ANNOTATION(mozilla::ProfilerString8View,
                            aValue.StringView().data())
ADD_DEBUG_STRING_ANNOTATION_IMPL(size_t N, nsAutoCStringN<N>,
                                 nsAutoCStringN<N>&, aValue.get())
ADD_DEBUG_STRING_ANNOTATION(nsCString, aValue.get())
ADD_DEBUG_STRING_ANNOTATION(nsAutoCString, aValue.get())
ADD_DEBUG_STRING_ANNOTATION(nsTLiteralString<char>, aValue.get())
ADD_DEBUG_STRING_ANNOTATION(nsPrintfCString, aValue.get())
ADD_DEBUG_STRING_ANNOTATION(NS_ConvertUTF16toUTF8, aValue.get())
ADD_DEBUG_STRING_ANNOTATION(nsTDependentString<char>, aValue.get())

ADD_DEBUG_STRING_ANNOTATION(nsACString, nsAutoCString(aValue).get())
ADD_DEBUG_STRING_ANNOTATION(std::string, aValue)
ADD_DEBUG_STRING_ANNOTATION_IMPL(size_t N, char[N], char*, aValue)
ADD_DEBUG_STRING_ANNOTATION(mozilla::ProfilerString16View,
                            NS_ConvertUTF16toUTF8(aValue).get())
ADD_DEBUG_STRING_ANNOTATION(nsAString, NS_ConvertUTF16toUTF8(aValue).get())
ADD_DEBUG_STRING_ANNOTATION_IMPL(, const nsAString&, nsAString&,
                                 NS_ConvertUTF16toUTF8(aValue).get())
ADD_DEBUG_STRING_ANNOTATION(nsString, NS_ConvertUTF16toUTF8(aValue).get())

// Main helper call that dispatches to proper specialization.
template <typename T>
void AddDebugAnnotation(perfetto::EventContext& ctx, const char* const aKey,
                        const T& aValue) {
  AddDebugAnnotationImpl<T>::call(ctx, aKey, aValue);
}

extern const char* ProfilerCategoryNames[];

// Main entry point from the gecko profiler for each marker.
template <typename MarkerType, typename... PayloadArguments>
void EmitPerfettoTrackEvent(const mozilla::ProfilerString8View& aName,
                            const mozilla::MarkerCategory& aCategory,
                            const mozilla::MarkerOptions& aOptions,
                            MarkerType aMarkerType,
                            const PayloadArguments&... aPayloadArguments) {
  mozilla::TimeStamp startTime, endTime;
  mozilla::MarkerTiming::Phase phase;

  if (aOptions.IsTimingUnspecified()) {
    startTime = mozilla::TimeStamp::Now();
    phase = mozilla::MarkerTiming::Phase::Instant;
  } else {
    startTime = aOptions.Timing().StartTime();
    endTime = aOptions.Timing().EndTime();
    phase = aOptions.Timing().MarkerPhase();
  }

  const char* nameStr = aName.StringView().data();
  if (!nameStr) {
    return;
  }

  // Create a dynamic category for the marker.
  const char* categoryName =
      ProfilerCategoryNames[static_cast<uint32_t>(aCategory.GetCategory())];
  perfetto::DynamicCategory category{categoryName};
  perfetto::DynamicString name{nameStr};

  // If the Marker has payload fields, we can annotate them in the perfetto
  // track event. Otherwise, we define an empty lambda which does nothing.
  std::function<void(perfetto::EventContext)> annotateTrackEvent =
      [&](perfetto::EventContext ctx) {};
  if constexpr (MarkerHasPayloadFields<MarkerType>::value) {
    annotateTrackEvent = [&](perfetto::EventContext ctx) {
      size_t i = 0;
      auto processArgument = [&](const auto& payloadArg) {
        AddDebugAnnotation(ctx, MarkerType::PayloadFields[i++].Key, payloadArg);
      };
      (processArgument(aPayloadArguments), ...);
    };
  }

  // Create a unique id for each marker so it has it's own track.
  mozilla::HashNumber hash =
      mozilla::HashStringKnownLength(nameStr, aName.StringView().length());

  switch (phase) {
    case mozilla::MarkerTiming::Phase::Interval: {
      hash = mozilla::AddToHash(
          hash, startTime.RawClockMonotonicNanosecondsSinceBoot());
      hash = mozilla::AddToHash(
          hash, endTime.RawClockMonotonicNanosecondsSinceBoot());
      perfetto::Track track(hash);

      PERFETTO_TRACE_EVENT_BEGIN(category, name, track, startTime);
      PERFETTO_TRACE_EVENT_END(category, track, endTime, annotateTrackEvent);
    } break;
    case mozilla::MarkerTiming::Phase::Instant: {
      PERFETTO_TRACE_EVENT_INSTANT(category, name, startTime);
    } break;
    case mozilla::MarkerTiming::Phase::IntervalStart: {
      PERFETTO_TRACE_EVENT_BEGIN(category, name, perfetto::Track(hash),
                                 startTime);
    } break;
    case mozilla::MarkerTiming::Phase::IntervalEnd: {
      PERFETTO_TRACE_EVENT_END(category, perfetto::Track(hash), endTime,
                               annotateTrackEvent);
    } break;
  }
}

#else  // !defined(MOZ_PERFETTO)
#  define PERFETTO_TRACE_EVENT(...) \
    do {                            \
    } while (0)
#  define PERFETTO_TRACE_EVENT_BEGIN(...) \
    do {                                  \
    } while (0)
#  define PERFETTO_TRACE_EVENT_END(...) \
    do {                                \
    } while (0)
#  define PERFETTO_TRACE_EVENT_INSTANT(...) \
    do {                                    \
    } while (0)
inline void InitPerfetto() {}
#endif  // MOZ_PERFETTO

#endif  // mozilla_Perfetto_h
