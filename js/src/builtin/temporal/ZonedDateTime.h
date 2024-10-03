/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_ZonedDateTime_h
#define builtin_temporal_ZonedDateTime_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <stdint.h>

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TimeZone.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

class JS_PUBLIC_API JSTracer;

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class ZonedDateTimeObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t SECONDS_SLOT = 0;
  static constexpr uint32_t NANOSECONDS_SLOT = 1;
  static constexpr uint32_t TIMEZONE_SLOT = 2;
  static constexpr uint32_t CALENDAR_SLOT = 3;
  static constexpr uint32_t SLOT_COUNT = 4;

  int64_t seconds() const {
    double seconds = getFixedSlot(SECONDS_SLOT).toNumber();
    MOZ_ASSERT(-8'640'000'000'000 <= seconds && seconds <= 8'640'000'000'000);
    return int64_t(seconds);
  }

  int32_t nanoseconds() const {
    int32_t nanoseconds = getFixedSlot(NANOSECONDS_SLOT).toInt32();
    MOZ_ASSERT(0 <= nanoseconds && nanoseconds <= 999'999'999);
    return nanoseconds;
  }

  TimeZoneValue timeZone() const {
    return TimeZoneValue(getFixedSlot(TIMEZONE_SLOT));
  }

  CalendarValue calendar() const {
    return CalendarValue(getFixedSlot(CALENDAR_SLOT));
  }

 private:
  static const ClassSpec classSpec_;
};
/**
 * Extract the instant fields from the ZonedDateTime object.
 */
inline Instant ToInstant(const ZonedDateTimeObject* zonedDateTime) {
  return {zonedDateTime->seconds(), zonedDateTime->nanoseconds()};
}

class MOZ_STACK_CLASS ZonedDateTime final {
  Instant instant_;
  TimeZoneValue timeZone_;
  CalendarValue calendar_;

 public:
  ZonedDateTime() = default;

  ZonedDateTime(const Instant& instant, const TimeZoneValue& timeZone,
                const CalendarValue& calendar)
      : instant_(instant), timeZone_(timeZone), calendar_(calendar) {
    MOZ_ASSERT(IsValidEpochInstant(instant));
    MOZ_ASSERT(timeZone);
    MOZ_ASSERT(calendar);
  }

  explicit ZonedDateTime(const ZonedDateTimeObject* obj)
      : ZonedDateTime(ToInstant(obj), obj->timeZone(), obj->calendar()) {}

  const auto& instant() const { return instant_; }

  const auto& timeZone() const { return timeZone_; }

  const auto& calendar() const { return calendar_; }

  explicit operator bool() const { return !!timeZone_ && !!calendar_; }

  void trace(JSTracer* trc) {
    timeZone_.trace(trc);
    calendar_.trace(trc);
  }

  const auto* timeZoneDoNotUse() const { return &timeZone_; }
  const auto* calendarDoNotUse() const { return &calendar_; }
};

struct DifferenceSettings;
enum class TemporalDisambiguation;
enum class TemporalOffset;
enum class TemporalOverflow;
enum class TemporalUnit;

/**
 * CreateTemporalZonedDateTime ( epochNanoseconds, timeZone, calendar [ ,
 * newTarget ] )
 */
ZonedDateTimeObject* CreateTemporalZonedDateTime(
    JSContext* cx, const Instant& instant, JS::Handle<TimeZoneValue> timeZone,
    JS::Handle<CalendarValue> calendar);

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, norm [ , precalculatedPlainDateTime [ , overflow ] ] )
 */
bool AddZonedDateTime(JSContext* cx, const Instant& epochNanoseconds,
                      JS::Handle<TimeZoneValue> timeZone,
                      JS::Handle<CalendarValue> calendar,
                      const NormalizedDuration& duration, Instant* result);

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, norm [ , precalculatedPlainDateTime [ , overflow ] ] )
 */
bool AddZonedDateTime(JSContext* cx, const Instant& epochNanoseconds,
                      JS::Handle<TimeZoneValue> timeZone,
                      JS::Handle<CalendarValue> calendar,
                      const NormalizedDuration& duration,
                      const PlainDateTime& dateTime, Instant* result);

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
bool DifferenceZonedDateTimeWithRounding(
    JSContext* cx, const Instant& ns1, const Instant& ns2,
    JS::Handle<TimeZoneValue> timeZone, JS::Handle<CalendarValue> calendar,
    const PlainDateTime& precalculatedPlainDateTime,
    const DifferenceSettings& settings, Duration* result);

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
bool DifferenceZonedDateTimeWithRounding(JSContext* cx, const Instant& ns1,
                                         const Instant& ns2,
                                         const DifferenceSettings& settings,
                                         Duration* result);

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
bool DifferenceZonedDateTimeWithRounding(
    JSContext* cx, const Instant& ns1, const Instant& ns2,
    JS::Handle<TimeZoneValue> timeZone, JS::Handle<CalendarValue> calendar,
    const PlainDateTime& precalculatedPlainDateTime, TemporalUnit unit,
    double* result);

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
double DifferenceZonedDateTimeWithRounding(const Instant& ns1,
                                           const Instant& ns2,
                                           TemporalUnit unit);

enum class OffsetBehaviour { Option, Exact, Wall };

enum class MatchBehaviour { MatchExactly, MatchMinutes };

/**
 * InterpretISODateTimeOffset ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond, offsetBehaviour, offsetNanoseconds,
 * timeZoneRec, disambiguation, offsetOption, matchBehaviour )
 */
bool InterpretISODateTimeOffset(JSContext* cx, const PlainDateTime& dateTime,
                                OffsetBehaviour offsetBehaviour,
                                int64_t offsetNanoseconds,
                                JS::Handle<TimeZoneValue> timeZone,
                                TemporalDisambiguation disambiguation,
                                TemporalOffset offsetOption,
                                MatchBehaviour matchBehaviour, Instant* result);

} /* namespace js::temporal */

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::ZonedDateTime, Wrapper> {
  const auto& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  explicit operator bool() const { return bool(container()); }

  const auto& instant() const { return container().instant(); }

  JS::Handle<temporal::TimeZoneValue> timeZone() const {
    return JS::Handle<temporal::TimeZoneValue>::fromMarkedLocation(
        container().timeZoneDoNotUse());
  }

  JS::Handle<temporal::CalendarValue> calendar() const {
    return JS::Handle<temporal::CalendarValue>::fromMarkedLocation(
        container().calendarDoNotUse());
  }
};

} /* namespace js */

#endif /* builtin_temporal_ZonedDateTime_h */
