/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/ZonedDateTime.h"

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"

#include <cstdlib>
#include <limits>
#include <utility>

#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Duration.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/Int96.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalFields.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalRoundingMode.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/TimeZone.h"
#include "builtin/temporal/ToString.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/ComparisonOperators.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/Value.h"
#include "vm/BigIntType.h"
#include "vm/BytecodeUtil.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsZonedDateTime(Handle<Value> v) {
  return v.isObject() && v.toObject().is<ZonedDateTimeObject>();
}

// Returns |RoundNumberToIncrement(offsetNanoseconds, 60 × 10^9, "halfExpand")|.
static int64_t RoundNanosecondsToMinutesIncrement(int64_t offsetNanoseconds) {
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  constexpr int64_t increment = ToNanoseconds(TemporalUnit::Minute);

  int64_t quotient = offsetNanoseconds / increment;
  int64_t remainder = offsetNanoseconds % increment;
  if (std::abs(remainder * 2) >= increment) {
    quotient += (offsetNanoseconds > 0 ? 1 : -1);
  }
  return quotient * increment;
}

/**
 * InterpretISODateTimeOffset ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond, offsetBehaviour, offsetNanoseconds,
 * timeZoneRec, disambiguation, offsetOption, matchBehaviour )
 */
bool js::temporal::InterpretISODateTimeOffset(
    JSContext* cx, const PlainDateTime& dateTime,
    OffsetBehaviour offsetBehaviour, int64_t offsetNanoseconds,
    Handle<TimeZoneValue> timeZone, TemporalDisambiguation disambiguation,
    TemporalOffset offsetOption, MatchBehaviour matchBehaviour,
    Instant* result) {
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  // Step 1.
  MOZ_ASSERT(IsValidISODateTime(dateTime));

  // Step 2.
  if (!ISODateTimeWithinLimits(dateTime)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_TIME_INVALID);
    return false;
  }

  // Step 3.
  if (offsetBehaviour == OffsetBehaviour::Wall ||
      (offsetBehaviour == OffsetBehaviour::Option &&
       offsetOption == TemporalOffset::Ignore)) {
    // Steps 3.a-b.
    return GetInstantFor(cx, timeZone, dateTime, disambiguation, result);
  }

  // Step 4.
  if (offsetBehaviour == OffsetBehaviour::Exact ||
      (offsetBehaviour == OffsetBehaviour::Option &&
       offsetOption == TemporalOffset::Use)) {
    // Step 4.a.
    auto epochNanoseconds = GetUTCEpochNanoseconds(
        dateTime, InstantSpan::fromNanoseconds(offsetNanoseconds));

    // Step 4.b.
    if (!IsValidEpochInstant(epochNanoseconds)) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INSTANT_INVALID);
      return false;
    }

    // Step 4.c.
    *result = epochNanoseconds;
    return true;
  }

  // Step 5.
  MOZ_ASSERT(offsetBehaviour == OffsetBehaviour::Option);

  // Step 6.
  MOZ_ASSERT(offsetOption == TemporalOffset::Prefer ||
             offsetOption == TemporalOffset::Reject);

  // Step 7.
  PossibleInstants possibleInstants;
  if (!GetPossibleInstantsFor(cx, timeZone, dateTime, &possibleInstants)) {
    return false;
  }

  // FIXME: spec issue - extra test for empty `possibleInstants` not needed

  // Step 8.a.
  for (const auto& candidate : possibleInstants) {
    // Step 8.a.i.
    int64_t candidateNanoseconds;
    if (!GetOffsetNanosecondsFor(cx, timeZone, candidate,
                                 &candidateNanoseconds)) {
      return false;
    }
    MOZ_ASSERT(std::abs(candidateNanoseconds) <
               ToNanoseconds(TemporalUnit::Day));

    // Step 8.a.ii.
    if (candidateNanoseconds == offsetNanoseconds) {
      *result = candidate;
      return true;
    }

    // Step 8.a.iii.
    if (matchBehaviour == MatchBehaviour::MatchMinutes) {
      // Step 8.a.iii.1.
      int64_t roundedCandidateNanoseconds =
          RoundNanosecondsToMinutesIncrement(candidateNanoseconds);

      // Step 8.a.iii.2.
      if (roundedCandidateNanoseconds == offsetNanoseconds) {
        // Step 8.a.iii.2.a.
        *result = candidate;
        return true;
      }
    }
  }

  // Step 9.
  if (offsetOption == TemporalOffset::Reject) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_ZONED_DATE_TIME_NO_TIME_FOUND);
    return false;
  }

  // Step 10.
  Instant instant;
  if (!DisambiguatePossibleInstants(cx, possibleInstants, timeZone, dateTime,
                                    disambiguation, &instant)) {
    return false;
  }

  // Step 11.
  *result = instant;
  return true;
}

/**
 * ToTemporalZonedDateTime ( item [ , disambiguation [ , offsetOption [ ,
 * overflow ] ] ] )
 */
static bool ToTemporalZonedDateTime(JSContext* cx, Handle<Value> item,
                                    TemporalDisambiguation disambiguation,
                                    TemporalOffset offsetOption,
                                    TemporalOverflow overflow,
                                    MutableHandle<ZonedDateTime> result) {
  // FIXME: spec issue - defaults for optional arguments are missing.

  // Step 1.
  auto offsetBehaviour = OffsetBehaviour::Option;

  // Step 2.
  auto matchBehaviour = MatchBehaviour::MatchExactly;

  // Step 5. (Reordered)
  int64_t offsetNanoseconds = 0;

  // Step 5.
  Rooted<CalendarValue> calendar(cx);
  Rooted<TimeZoneValue> timeZone(cx);
  PlainDateTime dateTime;
  if (item.isObject()) {
    Rooted<JSObject*> itemObj(cx, &item.toObject());

    // Step 3.a.
    if (auto* zonedDateTime = itemObj->maybeUnwrapIf<ZonedDateTimeObject>()) {
      auto instant = ToInstant(zonedDateTime);
      Rooted<TimeZoneValue> timeZone(cx, zonedDateTime->timeZone());
      Rooted<CalendarValue> calendar(cx, zonedDateTime->calendar());

      if (!timeZone.wrap(cx)) {
        return false;
      }
      if (!calendar.wrap(cx)) {
        return false;
      }

      result.set(ZonedDateTime{instant, timeZone, calendar});
      return true;
    }

    // Step 3.b.
    if (!GetTemporalCalendarWithISODefault(cx, itemObj, &calendar)) {
      return false;
    }

    // Step 3.c.
    Rooted<TemporalFields> fields(cx);
    if (!PrepareCalendarFields(cx, calendar, itemObj,
                               {
                                   CalendarField::Day,
                                   CalendarField::Month,
                                   CalendarField::MonthCode,
                                   CalendarField::Year,
                               },
                               {
                                   TemporalField::Hour,
                                   TemporalField::Microsecond,
                                   TemporalField::Millisecond,
                                   TemporalField::Minute,
                                   TemporalField::Nanosecond,
                                   TemporalField::Offset,
                                   TemporalField::Second,
                                   TemporalField::TimeZone,
                               },
                               {TemporalField::TimeZone}, &fields)) {
      return false;
    }

    // Step 3.d.
    Handle<Value> timeZoneValue = fields.timeZone();

    // Step 3.e.
    if (!ToTemporalTimeZone(cx, timeZoneValue, &timeZone)) {
      return false;
    }

    // Step 3.f.
    Handle<JSString*> offset = fields.offset();

    // Step 3.g. (Not applicable in our implementation.)

    // Step 3.h.
    if (!offset) {
      offsetBehaviour = OffsetBehaviour::Wall;
    }

    // Step 3.i.
    if (!InterpretTemporalDateTimeFields(cx, calendar, fields, overflow,
                                         &dateTime)) {
      return false;
    }

    // Step 6.
    if (offsetBehaviour == OffsetBehaviour::Option) {
      if (!ParseDateTimeUTCOffset(cx, offset, &offsetNanoseconds)) {
        return false;
      }
    }
  } else {
    // Step 4.a.
    if (!item.isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, item,
                       nullptr, "not a string");
      return false;
    }
    Rooted<JSString*> string(cx, item.toString());

    // Case 1: 19700101Z[+02:00]
    // { [[Z]]: true, [[OffsetString]]: undefined, [[Name]]: "+02:00" }
    //
    // Case 2: 19700101+00:00[+02:00]
    // { [[Z]]: false, [[OffsetString]]: "+00:00", [[Name]]: "+02:00" }
    //
    // Case 3: 19700101[+02:00]
    // { [[Z]]: false, [[OffsetString]]: undefined, [[Name]]: "+02:00" }
    //
    // Case 4: 19700101Z[Europe/Berlin]
    // { [[Z]]: true, [[OffsetString]]: undefined, [[Name]]: "Europe/Berlin" }
    //
    // Case 5: 19700101+00:00[Europe/Berlin]
    // { [[Z]]: false, [[OffsetString]]: "+00:00", [[Name]]: "Europe/Berlin" }
    //
    // Case 6: 19700101[Europe/Berlin]
    // { [[Z]]: false, [[OffsetString]]: undefined, [[Name]]: "Europe/Berlin" }

    // Steps 4.b-c.
    bool isUTC;
    bool hasOffset;
    int64_t timeZoneOffset;
    Rooted<ParsedTimeZone> timeZoneAnnotation(cx);
    Rooted<JSString*> calendarString(cx);
    if (!ParseTemporalZonedDateTimeString(
            cx, string, &dateTime, &isUTC, &hasOffset, &timeZoneOffset,
            &timeZoneAnnotation, &calendarString)) {
      return false;
    }

    // Step 4.d.
    MOZ_ASSERT(timeZoneAnnotation);

    // Step 4.e.
    if (!ToTemporalTimeZone(cx, timeZoneAnnotation, &timeZone)) {
      return false;
    }

    // Step 4.f. (Not applicable in our implementation.)

    // Step 4.g.
    if (isUTC) {
      offsetBehaviour = OffsetBehaviour::Exact;
    }

    // Step 4.h.
    else if (!hasOffset) {
      offsetBehaviour = OffsetBehaviour::Wall;
    }

    // Steps 4.i-l.
    if (calendarString) {
      if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
        return false;
      }
    } else {
      calendar.set(CalendarValue(CalendarId::ISO8601));
    }

    // Step 4.m.
    matchBehaviour = MatchBehaviour::MatchMinutes;

    // Step 6.
    if (offsetBehaviour == OffsetBehaviour::Option) {
      MOZ_ASSERT(hasOffset);
      offsetNanoseconds = timeZoneOffset;
    }
  }

  // Step 7.
  Instant epochNanoseconds;
  if (!InterpretISODateTimeOffset(
          cx, dateTime, offsetBehaviour, offsetNanoseconds, timeZone,
          disambiguation, offsetOption, matchBehaviour, &epochNanoseconds)) {
    return false;
  }

  // Step 8.
  result.set(ZonedDateTime{epochNanoseconds, timeZone, calendar});
  return true;
}

/**
 * ToTemporalZonedDateTime ( item [ , disambiguation [ , offsetOption [ ,
 * overflow ] ] ] )
 */
static bool ToTemporalZonedDateTime(JSContext* cx, Handle<Value> item,
                                    MutableHandle<ZonedDateTime> result) {
  return ToTemporalZonedDateTime(cx, item, TemporalDisambiguation::Compatible,
                                 TemporalOffset::Reject,
                                 TemporalOverflow::Constrain, result);
}

/**
 * CreateTemporalZonedDateTime ( epochNanoseconds, timeZone, calendar [ ,
 * newTarget ] )
 */
static ZonedDateTimeObject* CreateTemporalZonedDateTime(
    JSContext* cx, const CallArgs& args, Handle<BigInt*> epochNanoseconds,
    Handle<TimeZoneValue> timeZone, Handle<CalendarValue> calendar) {
  // Step 1.
  MOZ_ASSERT(IsValidEpochNanoseconds(epochNanoseconds));

  // Steps 3-4.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_ZonedDateTime,
                                          &proto)) {
    return nullptr;
  }

  auto* obj = NewObjectWithClassProto<ZonedDateTimeObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // Step 4.
  auto instant = ToInstant(epochNanoseconds);
  obj->setFixedSlot(ZonedDateTimeObject::SECONDS_SLOT,
                    NumberValue(instant.seconds));
  obj->setFixedSlot(ZonedDateTimeObject::NANOSECONDS_SLOT,
                    Int32Value(instant.nanoseconds));

  // Step 5.
  obj->setFixedSlot(ZonedDateTimeObject::TIMEZONE_SLOT, timeZone.toSlotValue());

  // Step 6.
  obj->setFixedSlot(ZonedDateTimeObject::CALENDAR_SLOT, calendar.toSlotValue());

  // Step 7.
  return obj;
}

/**
 * CreateTemporalZonedDateTime ( epochNanoseconds, timeZone, calendar [ ,
 * newTarget ] )
 */
ZonedDateTimeObject* js::temporal::CreateTemporalZonedDateTime(
    JSContext* cx, const Instant& instant, Handle<TimeZoneValue> timeZone,
    Handle<CalendarValue> calendar) {
  // Step 1.
  MOZ_ASSERT(IsValidEpochInstant(instant));

  // Steps 2-3.
  auto* obj = NewBuiltinClassInstance<ZonedDateTimeObject>(cx);
  if (!obj) {
    return nullptr;
  }

  // Step 4.
  obj->setFixedSlot(ZonedDateTimeObject::SECONDS_SLOT,
                    NumberValue(instant.seconds));
  obj->setFixedSlot(ZonedDateTimeObject::NANOSECONDS_SLOT,
                    Int32Value(instant.nanoseconds));

  // Step 5.
  obj->setFixedSlot(ZonedDateTimeObject::TIMEZONE_SLOT, timeZone.toSlotValue());

  // Step 6.
  obj->setFixedSlot(ZonedDateTimeObject::CALENDAR_SLOT, calendar.toSlotValue());

  // Step 7.
  return obj;
}

/**
 * CreateTemporalZonedDateTime ( epochNanoseconds, timeZone, calendar [ ,
 * newTarget ] )
 */
static auto* CreateTemporalZonedDateTime(JSContext* cx,
                                         Handle<ZonedDateTime> zonedDateTime) {
  return CreateTemporalZonedDateTime(cx, zonedDateTime.instant(),
                                     zonedDateTime.timeZone(),
                                     zonedDateTime.calendar());
}

/**
 * AddDaysToZonedDateTime ( instant, dateTime, timeZone, calendar, days [ ,
 * overflow ] )
 */
static bool AddDaysToZonedDateTime(JSContext* cx, const Instant& instant,
                                   const PlainDateTime& dateTime,
                                   Handle<TimeZoneValue> timeZone,
                                   Handle<CalendarValue> calendar, int64_t days,
                                   TemporalOverflow overflow, Instant* result) {
  // FIXME: spec issue - always called with `overflow`, so no need for optional
  // FIXME: spec issue - return type can be simplified, `dateTime` not used
  // FIXME: spec issue - inline into AddZonedDateTime to share more steps

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  if (days == 0) {
    *result = instant;
    return true;
  }

  // Step 3.
  PlainDate addedDate;
  if (!AddISODate(cx, dateTime.date, {0, 0, 0, days}, overflow, &addedDate)) {
    return false;
  }

  // Step 4.
  PlainDateTime dateTimeResult;
  if (!CreateTemporalDateTime(cx, addedDate, dateTime.time, &dateTimeResult)) {
    return false;
  }

  // Steps 5-6.
  return GetInstantFor(cx, timeZone, dateTimeResult,
                       TemporalDisambiguation::Compatible, result);
}

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, norm [ , precalculatedPlainDateTime [ , overflow ] ] )
 */
static bool AddZonedDateTime(JSContext* cx, const Instant& epochNanoseconds,
                             Handle<TimeZoneValue> timeZone,
                             Handle<CalendarValue> calendar,
                             const NormalizedDuration& duration,
                             mozilla::Maybe<const PlainDateTime&> dateTime,
                             TemporalOverflow overflow, Instant* result) {
  MOZ_ASSERT(IsValidEpochInstant(epochNanoseconds));
  MOZ_ASSERT(IsValidDuration(duration));

  // Steps 1-2. (Not applicable in our implementation)

  // Step 3.
  if (duration.date == DateDuration{}) {
    // Step 3.a.
    return AddInstant(cx, epochNanoseconds, duration.time, result);
  }

  // Step 4. (Not applicable in our implementation)

  // Steps 5-6.
  PlainDateTime temporalDateTime;
  if (dateTime) {
    // Step 5.a.
    temporalDateTime = *dateTime;
  } else {
    // Step 6.a.
    if (!GetPlainDateTimeFor(cx, timeZone, epochNanoseconds,
                             &temporalDateTime)) {
      return false;
    }
  }
  const auto& [date, time] = temporalDateTime;

  // Step 7.
  if (duration.date.years == 0 && duration.date.months == 0 &&
      duration.date.weeks == 0) {
    // Step 7.b.
    Instant intermediate;
    if (!AddDaysToZonedDateTime(cx, epochNanoseconds, temporalDateTime,
                                timeZone, calendar, duration.date.days,
                                overflow, &intermediate)) {
      return false;
    }

    // Step 7.c.
    return AddInstant(cx, intermediate, duration.time, result);
  }

  // Step 8.
  const auto& datePart = date;

  // Step 9.
  const auto& dateDuration = duration.date;

  // Step 10.
  PlainDate addedDate;
  if (!CalendarDateAdd(cx, calendar, datePart, dateDuration, overflow,
                       &addedDate)) {
    return false;
  }

  // Step 11.
  PlainDateTime intermediateDateTime;
  if (!CreateTemporalDateTime(cx, addedDate, time, &intermediateDateTime)) {
    return false;
  }

  // Step 12.
  Instant intermediateInstant;
  if (!GetInstantFor(cx, timeZone, intermediateDateTime,
                     TemporalDisambiguation::Compatible,
                     &intermediateInstant)) {
    return false;
  }

  // Step 13.
  return AddInstant(cx, intermediateInstant, duration.time, result);
}

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, norm [ , precalculatedPlainDateTime [ , overflow ] ] )
 */
static bool AddZonedDateTime(JSContext* cx, const Instant& epochNanoseconds,
                             Handle<TimeZoneValue> timeZone,
                             Handle<CalendarValue> calendar,
                             const NormalizedDuration& duration,
                             TemporalOverflow overflow, Instant* result) {
  return ::AddZonedDateTime(cx, epochNanoseconds, timeZone, calendar, duration,
                            mozilla::Nothing(), overflow, result);
}

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, norm [ , precalculatedPlainDateTime [ , overflow ] ] )
 */
bool js::temporal::AddZonedDateTime(JSContext* cx,
                                    const Instant& epochNanoseconds,
                                    Handle<TimeZoneValue> timeZone,
                                    Handle<CalendarValue> calendar,
                                    const NormalizedDuration& duration,
                                    Instant* result) {
  return ::AddZonedDateTime(cx, epochNanoseconds, timeZone, calendar, duration,
                            mozilla::Nothing(), TemporalOverflow::Constrain,
                            result);
}

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, norm [ , precalculatedPlainDateTime [ , overflow ] ] )
 */
bool js::temporal::AddZonedDateTime(JSContext* cx,
                                    const Instant& epochNanoseconds,
                                    Handle<TimeZoneValue> timeZone,
                                    Handle<CalendarValue> calendar,
                                    const NormalizedDuration& duration,
                                    const PlainDateTime& dateTime,
                                    Instant* result) {
  return ::AddZonedDateTime(cx, epochNanoseconds, timeZone, calendar, duration,
                            mozilla::SomeRef(dateTime),
                            TemporalOverflow::Constrain, result);
}

/**
 * DifferenceZonedDateTime ( ns1, ns2, timeZone, calendar, largestUnit,
 * startDateTime )
 */
static bool DifferenceZonedDateTime(JSContext* cx, const Instant& ns1,
                                    const Instant& ns2,
                                    Handle<TimeZoneValue> timeZone,
                                    Handle<CalendarValue> calendar,
                                    TemporalUnit largestUnit,
                                    const PlainDateTime& startDateTime,
                                    NormalizedDuration* result) {
  MOZ_ASSERT(IsValidEpochInstant(ns1));
  MOZ_ASSERT(IsValidEpochInstant(ns2));

  // Steps 1.
  if (ns1 == ns2) {
    *result = CreateNormalizedDurationRecord({}, {});
    return true;
  }

  // Steps 2-3.
  PlainDateTime endDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, ns2, &endDateTime)) {
    return false;
  }

  // Step 4.
  int32_t sign = (ns2 - ns1 < InstantSpan{}) ? -1 : 1;

  // Step 5.
  int32_t maxDayCorrection = 1 + (sign > 0);

  // Step 6.
  int32_t dayCorrection = 0;

  // Step 7.
  auto timeDuration = DifferenceTime(startDateTime.time, endDateTime.time);

  // Step 8.
  if (NormalizedTimeDurationSign(timeDuration) == -sign) {
    dayCorrection += 1;
  }

  // Steps 9-10.
  while (dayCorrection <= maxDayCorrection) {
    // Step 10.a.
    auto intermediateDate =
        BalanceISODate(endDateTime.date.year, endDateTime.date.month,
                       endDateTime.date.day - dayCorrection * sign);

    // FIXME: spec issue - CreateTemporalDateTime is fallible
    // https://github.com/tc39/proposal-temporal/issues/2824

    // Step 10.b.
    PlainDateTime intermediateDateTime;
    if (!CreateTemporalDateTime(cx, intermediateDate, startDateTime.time,
                                &intermediateDateTime)) {
      return false;
    }

    // Steps 10.c-d.
    Instant intermediateInstant;
    if (!GetInstantFor(cx, timeZone, intermediateDateTime,
                       TemporalDisambiguation::Compatible,
                       &intermediateInstant)) {
      return false;
    }

    // Step 10.e.
    auto norm = NormalizedTimeDurationFromEpochNanosecondsDifference(
        ns2, intermediateInstant);

    // Step 10.f.
    int32_t timeSign = NormalizedTimeDurationSign(norm);

    // Step 10.g.
    if (sign != -timeSign) {
      // Step 12.
      const auto& date1 = startDateTime.date;
      MOZ_ASSERT(ISODateTimeWithinLimits(date1));

      // Step 13.
      const auto& date2 = intermediateDate;
      MOZ_ASSERT(ISODateTimeWithinLimits(date2));

      // Step 14.
      auto dateLargestUnit = std::min(largestUnit, TemporalUnit::Day);

      // Step 15.
      DateDuration dateDifference;
      if (!CalendarDateUntil(cx, calendar, date1, date2, dateLargestUnit,
                             &dateDifference)) {
        return false;
      }

      // Step 16.
      return CreateNormalizedDurationRecord(cx, dateDifference, norm, result);
    }

    // Step 10.h.
    dayCorrection += 1;
  }

  // Step 11.
  JS_ReportErrorNumberASCII(
      cx, GetErrorMessage, nullptr,
      JSMSG_TEMPORAL_ZONED_DATE_TIME_INCONSISTENT_INSTANT);
  return false;
}

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
bool js::temporal::DifferenceZonedDateTimeWithRounding(
    JSContext* cx, const Instant& ns1, const Instant& ns2,
    Handle<TimeZoneValue> timeZone, Handle<CalendarValue> calendar,
    const PlainDateTime& precalculatedPlainDateTime,
    const DifferenceSettings& settings, Duration* result) {
  // FIXME: spec issue - calendar and timeZone arguments should be switched to
  // follow other zoned-datetime operations.

  // FIXME: spec issue - align testing for largest unit being larger than "day".
  // DifferenceTemporalZonedDateTime lists every unit, whereas this operation
  // uses IsCalendarUnit with special case for "day".

  // FIXME: spec issue - try to share duplicate steps for computing the diff
  // when unit larger than "day" with DifferenceTemporalZonedDateTime.

  // FIXME: spec issue - DifferenceZonedDateTimeWithRounding can be called
  // with precalculatedPlainDateTime = undefined from Duration.round.

  // Step 1.
  if (settings.largestUnit > TemporalUnit::Day) {
    return DifferenceZonedDateTimeWithRounding(cx, ns1, ns2, settings, result);
  }

  // Step 2.
  NormalizedDuration difference;
  if (!DifferenceZonedDateTime(cx, ns1, ns2, timeZone, calendar,
                               settings.largestUnit, precalculatedPlainDateTime,
                               &difference)) {
    return false;
  }

  // FIXME: spec issue - inline roundingGranularityIsNoop to match with similar
  // step in DifferencePlainDateTimeWithRounding

  // Step 3.
  bool roundingGranularityIsNoop =
      settings.smallestUnit == TemporalUnit::Nanosecond &&
      settings.roundingIncrement == Increment{1};

  // Step 4.
  if (roundingGranularityIsNoop) {
    // Step 4.a.
    auto timeDuration =
        BalanceTimeDuration(difference.time, TemporalUnit::Hour);

    // Step 4.b. (Not applicable in our implementation.)

    // Steps 4.c-d.
    *result = {
        double(difference.date.years), double(difference.date.months),
        double(difference.date.weeks), double(difference.date.days),
        double(timeDuration.hours),    double(timeDuration.minutes),
        double(timeDuration.seconds),  double(timeDuration.milliseconds),
        timeDuration.microseconds,     timeDuration.nanoseconds,
    };
    return true;
  }

  // Steps 5-6.
  RoundedRelativeDuration relative;
  if (!RoundRelativeDuration(cx, difference, ns2, precalculatedPlainDateTime,
                             calendar, timeZone, settings.largestUnit,
                             settings.roundingIncrement, settings.smallestUnit,
                             settings.roundingMode, &relative)) {
    return false;
  }
  MOZ_ASSERT(IsValidDuration(relative.duration));

  *result = relative.duration;
  return true;
}

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
bool js::temporal::DifferenceZonedDateTimeWithRounding(
    JSContext* cx, const Instant& ns1, const Instant& ns2,
    const DifferenceSettings& settings, Duration* result) {
  MOZ_ASSERT(settings.largestUnit > TemporalUnit::Day);
  MOZ_ASSERT(settings.smallestUnit >= settings.largestUnit);

  // Steps 1.a-b.
  auto difference =
      DifferenceInstant(ns1, ns2, settings.roundingIncrement,
                        settings.smallestUnit, settings.roundingMode);

  // Step 1.c.
  TimeDuration balancedTime;
  if (!BalanceTimeDuration(cx, difference, settings.largestUnit,
                           &balancedTime)) {
    return false;
  }
  MOZ_ASSERT(balancedTime.days == 0);

  // Steps 1.d-e.
  *result = balancedTime.toDuration();
  return true;
}

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
bool js::temporal::DifferenceZonedDateTimeWithRounding(
    JSContext* cx, const Instant& ns1, const Instant& ns2,
    Handle<TimeZoneValue> timeZone, Handle<CalendarValue> calendar,
    const PlainDateTime& precalculatedPlainDateTime, TemporalUnit unit,
    double* result) {
  // Step 1.
  if (unit > TemporalUnit::Day) {
    *result = DifferenceZonedDateTimeWithRounding(ns1, ns2, unit);
    return true;
  }

  // Step 2.
  NormalizedDuration difference;
  if (!DifferenceZonedDateTime(cx, ns1, ns2, timeZone, calendar, unit,
                               precalculatedPlainDateTime, &difference)) {
    return false;
  }

  // Steps 3-4. (Not applicable)

  // Steps 5-6.
  RoundedRelativeDuration rounded;
  if (!RoundRelativeDuration(cx, difference, ns2, precalculatedPlainDateTime,
                             calendar, timeZone, unit, Increment{1}, unit,
                             TemporalRoundingMode::Trunc, &rounded)) {
    return false;
  }
  MOZ_ASSERT(!std::isnan(rounded.total));

  *result = rounded.total;
  return true;
}

/**
 * DifferenceZonedDateTimeWithRounding ( ns1, ns2, calendar, timeZone,
 * precalculatedPlainDateTime, largestUnit, roundingIncrement, smallestUnit,
 * roundingMode )
 */
double js::temporal::DifferenceZonedDateTimeWithRounding(const Instant& ns1,
                                                         const Instant& ns2,
                                                         TemporalUnit unit) {
  MOZ_ASSERT(IsValidEpochInstant(ns1));
  MOZ_ASSERT(IsValidEpochInstant(ns2));
  MOZ_ASSERT(unit > TemporalUnit::Day);

  // Step 1.a. (Inlined DifferenceInstant)
  //
  // DifferenceInstant, step 1.
  auto diff = NormalizedTimeDurationFromEpochNanosecondsDifference(ns1, ns2);
  MOZ_ASSERT(IsValidInstantSpan(diff.to<InstantSpan>()));

  // DifferenceInstant, step 2. (Inlined RoundTimeDuration)
  //
  // RoundTimeDuration, step 3.c.
  return DivideNormalizedTimeDuration(diff, unit);
}

/**
 * DifferenceTemporalZonedDateTime ( operation, zonedDateTime, other, options )
 */
static bool DifferenceTemporalZonedDateTime(JSContext* cx,
                                            TemporalDifference operation,
                                            const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Rooted<ZonedDateTime> other(cx);
  if (!ToTemporalZonedDateTime(cx, args.get(0), &other)) {
    return false;
  }

  // Step 3.
  if (!CalendarEquals(zonedDateTime.calendar(), other.calendar())) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE,
        ToTemporalCalendarIdentifier(zonedDateTime.calendar()).data(),
        ToTemporalCalendarIdentifier(other.calendar()).data());
    return false;
  }

  // Steps 4-5.
  DifferenceSettings settings;
  if (args.hasDefined(1)) {
    // Step 4.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", ToName(operation), args[1]));
    if (!options) {
      return false;
    }

    // Step 5.
    if (!GetDifferenceSettings(
            cx, operation, options, TemporalUnitGroup::DateTime,
            TemporalUnit::Nanosecond, TemporalUnit::Hour, &settings)) {
      return false;
    }
  } else {
    // Steps 4-5.
    settings = {
        TemporalUnit::Nanosecond,
        TemporalUnit::Hour,
        TemporalRoundingMode::Trunc,
        Increment{1},
    };
  }

  // Step 6.
  if (settings.largestUnit > TemporalUnit::Day) {
    MOZ_ASSERT(settings.smallestUnit >= settings.largestUnit);

    // Steps 6.a-b.
    auto difference = DifferenceInstant(
        zonedDateTime.instant(), other.instant(), settings.roundingIncrement,
        settings.smallestUnit, settings.roundingMode);

    // Step 6.c.
    TimeDuration balancedTime;
    if (!BalanceTimeDuration(cx, difference, settings.largestUnit,
                             &balancedTime)) {
      return false;
    }

    // Step 6.d.
    auto duration = balancedTime.toDuration();
    if (operation == TemporalDifference::Since) {
      duration = duration.negate();
    }

    auto* result = CreateTemporalDuration(cx, duration);
    if (!result) {
      return false;
    }

    args.rval().setObject(*result);
    return true;
  }

  // Steps 7-8.
  if (!TimeZoneEquals(zonedDateTime.timeZone(), other.timeZone())) {
    if (auto one = QuoteString(cx, zonedDateTime.timeZone().identifier())) {
      if (auto two = QuoteString(cx, other.timeZone().identifier())) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_TEMPORAL_TIMEZONE_INCOMPATIBLE,
                                 one.get(), two.get());
      }
    }
    return false;
  }

  // Step 9.
  if (zonedDateTime.instant() == other.instant()) {
    auto* obj = CreateTemporalDuration(cx, {});
    if (!obj) {
      return false;
    }

    args.rval().setObject(*obj);
    return true;
  }

  // Step 10.
  auto timeZone = zonedDateTime.timeZone();

  // Step 11.
  auto calendar = zonedDateTime.calendar();

  // Steps 12-13.
  PlainDateTime precalculatedPlainDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, zonedDateTime.instant(),
                           &precalculatedPlainDateTime)) {
    return false;
  }

  // Steps 14-15.
  Duration duration;
  if (!DifferenceZonedDateTimeWithRounding(
          cx, zonedDateTime.instant(), other.instant(), timeZone, calendar,
          precalculatedPlainDateTime, settings, &duration)) {
    return false;
  }
  MOZ_ASSERT(IsValidDuration(duration));

  // Step 16.
  if (operation == TemporalDifference::Since) {
    duration = duration.negate();
  }

  auto* obj = CreateTemporalDuration(cx, duration);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

enum class ZonedDateTimeDuration { Add, Subtract };

/**
 * AddDurationToOrSubtractDurationFromZonedDateTime ( operation, zonedDateTime,
 * temporalDurationLike, options )
 */
static bool AddDurationToOrSubtractDurationFromZonedDateTime(
    JSContext* cx, ZonedDateTimeDuration operation, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, &args.thisv().toObject().as<ZonedDateTimeObject>());

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Duration duration;
  if (!ToTemporalDurationRecord(cx, args.get(0), &duration)) {
    return false;
  }

  // Steps 3-4.
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    const char* name =
        operation == ZonedDateTimeDuration::Add ? "add" : "subtract";

    // Step 3.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", name, args[1]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 5.
  auto calendar = zonedDateTime.calendar();

  // Step 6.
  auto timeZone = zonedDateTime.timeZone();

  // Step 7.
  if (operation == ZonedDateTimeDuration::Subtract) {
    duration = duration.negate();
  }
  auto normalized = CreateNormalizedDurationRecord(duration);

  // Step 8.
  Instant resultInstant;
  if (!::AddZonedDateTime(cx, zonedDateTime.instant(), timeZone, calendar,
                          normalized, overflow, &resultInstant)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(resultInstant));

  // Step 9.
  auto* result =
      CreateTemporalZonedDateTime(cx, resultInstant, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime ( epochNanoseconds, timeZoneLike [ , calendarLike ] )
 */
static bool ZonedDateTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.ZonedDateTime")) {
    return false;
  }

  // Step 2.
  Rooted<BigInt*> epochNanoseconds(cx, js::ToBigInt(cx, args.get(0)));
  if (!epochNanoseconds) {
    return false;
  }

  // Step 3.
  if (!IsValidEpochNanoseconds(epochNanoseconds)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INSTANT_INVALID);
    return false;
  }

  // Step 4.
  if (!args.get(1).isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, args.get(1),
                     nullptr, "not a string");
    return false;
  }

  // Step 5.
  Rooted<JSString*> timeZoneString(cx, args[1].toString());
  Rooted<ParsedTimeZone> timeZoneParse(cx);
  if (!ParseTimeZoneIdentifier(cx, timeZoneString, &timeZoneParse)) {
    return false;
  }

  // Steps 6-7.
  Rooted<TimeZoneValue> timeZone(cx);
  if (!ToTemporalTimeZone(cx, timeZoneParse, &timeZone)) {
    return false;
  }

  // Steps 8-11.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  if (args.hasDefined(2)) {
    // Step 9.
    if (!args[2].isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, args[2],
                       nullptr, "not a string");
      return false;
    }

    // Steps 10-11.
    Rooted<JSString*> calendarString(cx, args[2].toString());
    if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
      return false;
    }
  }

  // Step 6.
  auto* obj = CreateTemporalZonedDateTime(cx, args, epochNanoseconds, timeZone,
                                          calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.ZonedDateTime.from ( item [ , options ] )
 */
static bool ZonedDateTime_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Steps 1-4.
  auto disambiguation = TemporalDisambiguation::Compatible;
  auto offset = TemporalOffset::Reject;
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 1.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "from", args[1]));
    if (!options) {
      return false;
    }

    // Step 2.
    if (!GetTemporalDisambiguationOption(cx, options, &disambiguation)) {
      return false;
    }

    // Step 3.
    if (!GetTemporalOffsetOption(cx, options, &offset)) {
      return false;
    }

    // Step 4.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 3.
  Rooted<ZonedDateTime> zonedDateTime(cx);
  if (!ToTemporalZonedDateTime(cx, args.get(0), disambiguation, offset,
                               overflow, &zonedDateTime)) {
    return false;
  }

  auto* result = CreateTemporalZonedDateTime(cx, zonedDateTime);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.compare ( one, two )
 */
static bool ZonedDateTime_compare(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<ZonedDateTime> one(cx);
  if (!ToTemporalZonedDateTime(cx, args.get(0), &one)) {
    return false;
  }

  // Step 2.
  Rooted<ZonedDateTime> two(cx);
  if (!ToTemporalZonedDateTime(cx, args.get(1), &two)) {
    return false;
  }

  // Step 3.
  const auto& oneNs = one.instant();
  const auto& twoNs = two.instant();
  args.rval().setInt32(oneNs > twoNs ? 1 : oneNs < twoNs ? -1 : 0);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.calendarId
 */
static bool ZonedDateTime_calendarId(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  Rooted<CalendarValue> calendar(cx, zonedDateTime->calendar());
  auto* calendarId = ToTemporalCalendarIdentifier(cx, calendar);
  if (!calendarId) {
    return false;
  }

  args.rval().setString(calendarId);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.calendarId
 */
static bool ZonedDateTime_calendarId(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_calendarId>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.timeZoneId
 */
static bool ZonedDateTime_timeZoneId(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  args.rval().setString(zonedDateTime->timeZone().identifier());
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.timeZoneId
 */
static bool ZonedDateTime_timeZoneId(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_timeZoneId>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.era
 */
static bool ZonedDateTime_era(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarEra(cx, zonedDateTime.calendar(), dateTime.date, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.era
 */
static bool ZonedDateTime_era(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_era>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.eraYear
 */
static bool ZonedDateTime_eraYear(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Steps 6-8.
  return CalendarEraYear(cx, zonedDateTime.calendar(), dateTime.date,
                         args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.eraYear
 */
static bool ZonedDateTime_eraYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_eraYear>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.year
 */
static bool ZonedDateTime_year(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarYear(cx, zonedDateTime.calendar(), dateTime.date, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.year
 */
static bool ZonedDateTime_year(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_year>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.month
 */
static bool ZonedDateTime_month(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarMonth(cx, zonedDateTime.calendar(), dateTime.date,
                       args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.month
 */
static bool ZonedDateTime_month(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_month>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.monthCode
 */
static bool ZonedDateTime_monthCode(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarMonthCode(cx, zonedDateTime.calendar(), dateTime.date,
                           args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.monthCode
 */
static bool ZonedDateTime_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_monthCode>(cx,
                                                                        args);
}

/**
 * get Temporal.ZonedDateTime.prototype.day
 */
static bool ZonedDateTime_day(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarDay(cx, zonedDateTime.calendar(), dateTime.date, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.day
 */
static bool ZonedDateTime_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_day>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.hour
 */
static bool ZonedDateTime_hour(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  args.rval().setInt32(dateTime.time.hour);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.hour
 */
static bool ZonedDateTime_hour(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_hour>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.minute
 */
static bool ZonedDateTime_minute(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  args.rval().setInt32(dateTime.time.minute);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.minute
 */
static bool ZonedDateTime_minute(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_minute>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.second
 */
static bool ZonedDateTime_second(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  args.rval().setInt32(dateTime.time.second);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.second
 */
static bool ZonedDateTime_second(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_second>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.millisecond
 */
static bool ZonedDateTime_millisecond(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  args.rval().setInt32(dateTime.time.millisecond);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.millisecond
 */
static bool ZonedDateTime_millisecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_millisecond>(cx,
                                                                          args);
}

/**
 * get Temporal.ZonedDateTime.prototype.microsecond
 */
static bool ZonedDateTime_microsecond(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  args.rval().setInt32(dateTime.time.microsecond);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.microsecond
 */
static bool ZonedDateTime_microsecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_microsecond>(cx,
                                                                          args);
}

/**
 * get Temporal.ZonedDateTime.prototype.nanosecond
 */
static bool ZonedDateTime_nanosecond(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  args.rval().setInt32(dateTime.time.nanosecond);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.nanosecond
 */
static bool ZonedDateTime_nanosecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_nanosecond>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.epochMilliseconds
 */
static bool ZonedDateTime_epochMilliseconds(JSContext* cx,
                                            const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  auto instant = ToInstant(zonedDateTime);

  // Steps 4-5.
  args.rval().setNumber(instant.floorToMilliseconds());
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.epochMilliseconds
 */
static bool ZonedDateTime_epochMilliseconds(JSContext* cx, unsigned argc,
                                            Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_epochMilliseconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.epochNanoseconds
 */
static bool ZonedDateTime_epochNanoseconds(JSContext* cx,
                                           const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  auto* nanoseconds = ToEpochNanoseconds(cx, ToInstant(zonedDateTime));
  if (!nanoseconds) {
    return false;
  }

  args.rval().setBigInt(nanoseconds);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.epochNanoseconds
 */
static bool ZonedDateTime_epochNanoseconds(JSContext* cx, unsigned argc,
                                           Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_epochNanoseconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfWeek
 */
static bool ZonedDateTime_dayOfWeek(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarDayOfWeek(cx, zonedDateTime.calendar(), dateTime.date,
                           args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfWeek
 */
static bool ZonedDateTime_dayOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_dayOfWeek>(cx,
                                                                        args);
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfYear
 */
static bool ZonedDateTime_dayOfYear(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarDayOfYear(cx, zonedDateTime.calendar(), dateTime.date,
                           args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfYear
 */
static bool ZonedDateTime_dayOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_dayOfYear>(cx,
                                                                        args);
}

/**
 * get Temporal.ZonedDateTime.prototype.weekOfYear
 */
static bool ZonedDateTime_weekOfYear(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Steps 6-8.
  return CalendarWeekOfYear(cx, zonedDateTime.calendar(), dateTime.date,
                            args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.weekOfYear
 */
static bool ZonedDateTime_weekOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_weekOfYear>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.yearOfWeek
 */
static bool ZonedDateTime_yearOfWeek(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Steps 6-8.
  return CalendarYearOfWeek(cx, zonedDateTime.calendar(), dateTime.date,
                            args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.yearOfWeek
 */
static bool ZonedDateTime_yearOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_yearOfWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.hoursInDay
 */
static bool ZonedDateTime_hoursInDay(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  auto timeZone = zonedDateTime.timeZone();

  // Step 4.
  const auto& instant = zonedDateTime.instant();

  // Step 5.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &temporalDateTime)) {
    return false;
  }

  // Steps 6-8.
  const auto& date = temporalDateTime.date;

  // Step 9.
  PlainDateTime today;
  if (!CreateTemporalDateTime(cx, date, {}, &today)) {
    return false;
  }

  // Step 10.
  auto tomorrowFields = BalanceISODate(date.year, date.month, date.day + 1);

  // Step 11.
  PlainDateTime tomorrow;
  if (!CreateTemporalDateTime(cx, tomorrowFields, {}, &tomorrow)) {
    return false;
  }

  // Step 12.
  Instant todayInstant;
  if (!GetInstantFor(cx, timeZone, today, TemporalDisambiguation::Compatible,
                     &todayInstant)) {
    return false;
  }

  // Step 13.
  Instant tomorrowInstant;
  if (!GetInstantFor(cx, timeZone, tomorrow, TemporalDisambiguation::Compatible,
                     &tomorrowInstant)) {
    return false;
  }

  // Step 14.
  auto diff = tomorrowInstant - todayInstant;
  MOZ_ASSERT(IsValidInstantSpan(diff));

  // Step 15.
  constexpr auto nsPerHour = Int128{ToNanoseconds(TemporalUnit::Hour)};
  args.rval().setNumber(FractionToDouble(diff.toNanoseconds(), nsPerHour));
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.hoursInDay
 */
static bool ZonedDateTime_hoursInDay(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_hoursInDay>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInWeek
 */
static bool ZonedDateTime_daysInWeek(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarDaysInWeek(cx, zonedDateTime.calendar(), dateTime.date,
                            args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInWeek
 */
static bool ZonedDateTime_daysInWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_daysInWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInMonth
 */
static bool ZonedDateTime_daysInMonth(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarDaysInMonth(cx, zonedDateTime.calendar(), dateTime.date,
                             args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInMonth
 */
static bool ZonedDateTime_daysInMonth(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_daysInMonth>(cx,
                                                                          args);
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInYear
 */
static bool ZonedDateTime_daysInYear(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarDaysInYear(cx, zonedDateTime.calendar(), dateTime.date,
                            args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInYear
 */
static bool ZonedDateTime_daysInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_daysInYear>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.monthsInYear
 */
static bool ZonedDateTime_monthsInYear(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarMonthsInYear(cx, zonedDateTime.calendar(), dateTime.date,
                              args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.monthsInYear
 */
static bool ZonedDateTime_monthsInYear(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_monthsInYear>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.inLeapYear
 */
static bool ZonedDateTime_inLeapYear(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-5.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }

  // Step 6.
  return CalendarInLeapYear(cx, zonedDateTime.calendar(), dateTime.date,
                            args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.inLeapYear
 */
static bool ZonedDateTime_inLeapYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_inLeapYear>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.offsetNanoseconds
 */
static bool ZonedDateTime_offsetNanoseconds(JSContext* cx,
                                            const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  auto timeZone = zonedDateTime.timeZone();

  // Step 4.
  const auto& instant = zonedDateTime.instant();

  // Step 5.
  int64_t offsetNanoseconds;
  if (!GetOffsetNanosecondsFor(cx, timeZone, instant, &offsetNanoseconds)) {
    return false;
  }
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  args.rval().setNumber(offsetNanoseconds);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.offsetNanoseconds
 */
static bool ZonedDateTime_offsetNanoseconds(JSContext* cx, unsigned argc,
                                            Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_offsetNanoseconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.offset
 */
static bool ZonedDateTime_offset(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  auto timeZone = zonedDateTime.timeZone();

  // Step 4.
  const auto& instant = zonedDateTime.instant();

  // Step 5.
  JSString* str = GetOffsetStringFor(cx, timeZone, instant);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.offset
 */
static bool ZonedDateTime_offset(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_offset>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.with ( temporalZonedDateTimeLike [ , options
 * ] )
 */
static bool ZonedDateTime_with(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  Rooted<JSObject*> temporalZonedDateTimeLike(
      cx,
      RequireObjectArg(cx, "temporalZonedDateTimeLike", "with", args.get(0)));
  if (!temporalZonedDateTimeLike) {
    return false;
  }
  if (!ThrowIfTemporalLikeObject(cx, temporalZonedDateTimeLike)) {
    return false;
  }

  // FIXME: spec issue - read options earlier to match other operations?

  // Step 4.
  Rooted<JSObject*> options(cx);
  if (args.hasDefined(1)) {
    options = RequireObjectArg(cx, "options", "with", args[1]);
    if (!options) {
      return false;
    }
  }

  // Step 5.
  auto timeZone = zonedDateTime.timeZone();

  // Step 6.
  auto calendar = zonedDateTime.calendar();

  // Step 7.
  const auto& instant = zonedDateTime.instant();

  // Step 8.
  int64_t offsetNanoseconds;
  if (!GetOffsetNanosecondsFor(cx, timeZone, instant, &offsetNanoseconds)) {
    return false;
  }

  // FIXME: spec issue - make |dateTimeObj| unobservable.

  // Step 9.
  auto dateTime = GetPlainDateTimeFor(instant, offsetNanoseconds);
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));
  Rooted<PlainDateTimeObject*> dateTimeObj(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateTimeObj) {
    return false;
  }

  // Step 10.
  Rooted<TemporalFields> fields(cx);
  if (!PrepareCalendarFieldsAndFieldNames(cx, calendar, dateTimeObj,
                                          {
                                              CalendarField::Day,
                                              CalendarField::Month,
                                              CalendarField::MonthCode,
                                              CalendarField::Year,
                                          },
                                          &fields)) {
    return false;
  }

  // Steps 11-18.
  JSString* fieldsOffset = FormatUTCOffsetNanoseconds(cx, offsetNanoseconds);
  if (!fieldsOffset) {
    return false;
  }
  fields.setHour(dateTime.time.hour);
  fields.setMinute(dateTime.time.minute);
  fields.setSecond(dateTime.time.second);
  fields.setMillisecond(dateTime.time.millisecond);
  fields.setMicrosecond(dateTime.time.microsecond);
  fields.setNanosecond(dateTime.time.nanosecond);
  fields.setOffset(fieldsOffset);

  // Step 19.
  Rooted<TemporalFields> partialZonedDateTime(cx);
  if (!PreparePartialTemporalFields(cx, temporalZonedDateTimeLike,
                                    fields.keys(), &partialZonedDateTime)) {
    return false;
  }
  MOZ_ASSERT(!partialZonedDateTime.keys().isEmpty());

  // Step 20.
  Rooted<TemporalFields> mergedFields(
      cx, CalendarMergeFields(calendar, fields, partialZonedDateTime));

  // Step 21.
  if (!PrepareTemporalFields(cx, mergedFields, fields.keys(),
                             {TemporalField::Offset}, &fields)) {
    return false;
  }

  // Steps 22-24.
  auto disambiguation = TemporalDisambiguation::Compatible;
  auto offset = TemporalOffset::Prefer;
  auto overflow = TemporalOverflow::Constrain;
  if (options) {
    // Step 22.
    if (!GetTemporalDisambiguationOption(cx, options, &disambiguation)) {
      return false;
    }

    // Step 23.
    if (!GetTemporalOffsetOption(cx, options, &offset)) {
      return false;
    }

    // Step 24.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 25.
  PlainDateTime dateTimeResult;
  if (!InterpretTemporalDateTimeFields(cx, calendar, fields, overflow,
                                       &dateTimeResult)) {
    return false;
  }

  // Step 26.
  Handle<JSString*> offsetString = fields.offset();

  // Step 27.
  MOZ_ASSERT(offsetString);

  // Step 28.
  int64_t newOffsetNanoseconds;
  if (!ParseDateTimeUTCOffset(cx, offsetString, &newOffsetNanoseconds)) {
    return false;
  }

  // Step 29.
  Instant epochNanoseconds;
  if (!InterpretISODateTimeOffset(
          cx, dateTimeResult, OffsetBehaviour::Option, newOffsetNanoseconds,
          timeZone, disambiguation, offset, MatchBehaviour::MatchExactly,
          &epochNanoseconds)) {
    return false;
  }

  // Step 30.
  auto* result =
      CreateTemporalZonedDateTime(cx, epochNanoseconds, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.with ( temporalZonedDateTimeLike [ , options
 * ] )
 */
static bool ZonedDateTime_with(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_with>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.withPlainTime ( [ plainTimeLike ] )
 */
static bool ZonedDateTime_withPlainTime(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3. (Inlined ToTemporalTimeOrMidnight)
  PlainTime time = {};
  if (args.hasDefined(0)) {
    if (!ToTemporalTime(cx, args[0], &time)) {
      return false;
    }
  }

  // Step 4.
  auto timeZone = zonedDateTime.timeZone();

  // Steps 5 and 7.
  PlainDateTime plainDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, zonedDateTime.instant(),
                           &plainDateTime)) {
    return false;
  }

  // Step 6.
  auto calendar = zonedDateTime.calendar();

  // Step 8.
  PlainDateTime resultPlainDateTime;
  if (!CreateTemporalDateTime(cx, plainDateTime.date, time,
                              &resultPlainDateTime)) {
    return false;
  }

  // Step 9.
  Instant instant;
  if (!GetInstantFor(cx, timeZone, resultPlainDateTime,
                     TemporalDisambiguation::Compatible, &instant)) {
    return false;
  }

  // Step 10.
  auto* result = CreateTemporalZonedDateTime(cx, instant, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.withPlainTime ( [ plainTimeLike ] )
 */
static bool ZonedDateTime_withPlainTime(JSContext* cx, unsigned argc,
                                        Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_withPlainTime>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.withTimeZone ( timeZoneLike )
 */
static bool ZonedDateTime_withTimeZone(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  Rooted<TimeZoneValue> timeZone(cx);
  if (!ToTemporalTimeZone(cx, args.get(0), &timeZone)) {
    return false;
  }

  // Step 4.
  auto* result = CreateTemporalZonedDateTime(
      cx, zonedDateTime.instant(), timeZone, zonedDateTime.calendar());
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.withTimeZone ( timeZoneLike )
 */
static bool ZonedDateTime_withTimeZone(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_withTimeZone>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.withCalendar ( calendarLike )
 */
static bool ZonedDateTime_withCalendar(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendar(cx, args.get(0), &calendar)) {
    return false;
  }

  // Step 4.
  auto* result = CreateTemporalZonedDateTime(
      cx, zonedDateTime.instant(), zonedDateTime.timeZone(), calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.withCalendar ( calendarLike )
 */
static bool ZonedDateTime_withCalendar(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_withCalendar>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.add ( temporalDurationLike [ , options ] )
 */
static bool ZonedDateTime_add(JSContext* cx, const CallArgs& args) {
  return AddDurationToOrSubtractDurationFromZonedDateTime(
      cx, ZonedDateTimeDuration::Add, args);
}

/**
 * Temporal.ZonedDateTime.prototype.add ( temporalDurationLike [ , options ] )
 */
static bool ZonedDateTime_add(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_add>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.subtract ( temporalDurationLike [ , options
 * ] )
 */
static bool ZonedDateTime_subtract(JSContext* cx, const CallArgs& args) {
  return AddDurationToOrSubtractDurationFromZonedDateTime(
      cx, ZonedDateTimeDuration::Subtract, args);
}

/**
 * Temporal.ZonedDateTime.prototype.subtract ( temporalDurationLike [ , options
 * ] )
 */
static bool ZonedDateTime_subtract(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_subtract>(cx,
                                                                       args);
}

/**
 * Temporal.ZonedDateTime.prototype.until ( other [ , options ] )
 */
static bool ZonedDateTime_until(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return DifferenceTemporalZonedDateTime(cx, TemporalDifference::Until, args);
}

/**
 * Temporal.ZonedDateTime.prototype.until ( other [ , options ] )
 */
static bool ZonedDateTime_until(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_until>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.since ( other [ , options ] )
 */
static bool ZonedDateTime_since(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return DifferenceTemporalZonedDateTime(cx, TemporalDifference::Since, args);
}

/**
 * Temporal.ZonedDateTime.prototype.since ( other [ , options ] )
 */
static bool ZonedDateTime_since(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_since>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.round ( roundTo )
 */
static bool ZonedDateTime_round(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-12.
  auto smallestUnit = TemporalUnit::Auto;
  auto roundingMode = TemporalRoundingMode::HalfExpand;
  auto roundingIncrement = Increment{1};
  if (args.get(0).isString()) {
    // Step 4. (Not applicable in our implementation.)

    // Step 9.
    Rooted<JSString*> paramString(cx, args[0].toString());
    if (!GetTemporalUnitValuedOption(
            cx, paramString, TemporalUnitKey::SmallestUnit,
            TemporalUnitGroup::DayTime, &smallestUnit)) {
      return false;
    }

    // Steps 6-8 and 10-12. (Implicit)
  } else {
    // Steps 3 and 5.a
    Rooted<JSObject*> roundTo(
        cx, RequireObjectArg(cx, "roundTo", "round", args.get(0)));
    if (!roundTo) {
      return false;
    }

    // Steps 6-7.
    if (!GetRoundingIncrementOption(cx, roundTo, &roundingIncrement)) {
      return false;
    }

    // Step 8.
    if (!GetRoundingModeOption(cx, roundTo, &roundingMode)) {
      return false;
    }

    // Step 9.
    if (!GetTemporalUnitValuedOption(cx, roundTo, TemporalUnitKey::SmallestUnit,
                                     TemporalUnitGroup::DayTime,
                                     &smallestUnit)) {
      return false;
    }

    if (smallestUnit == TemporalUnit::Auto) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_MISSING_OPTION, "smallestUnit");
      return false;
    }

    MOZ_ASSERT(TemporalUnit::Day <= smallestUnit &&
               smallestUnit <= TemporalUnit::Nanosecond);

    // Steps 10-11.
    auto maximum = Increment{1};
    bool inclusive = true;
    if (smallestUnit > TemporalUnit::Day) {
      maximum = MaximumTemporalDurationRoundingIncrement(smallestUnit);
      inclusive = false;
    }

    // Step 12.
    if (!ValidateTemporalRoundingIncrement(cx, roundingIncrement, maximum,
                                           inclusive)) {
      return false;
    }
  }

  // Step 13.
  if (smallestUnit == TemporalUnit::Nanosecond &&
      roundingIncrement == Increment{1}) {
    // Step 13.a.
    auto* result = CreateTemporalZonedDateTime(cx, zonedDateTime.instant(),
                                               zonedDateTime.timeZone(),
                                               zonedDateTime.calendar());
    if (!result) {
      return false;
    }

    args.rval().setObject(*result);
    return true;
  }

  // Step 14.
  auto timeZone = zonedDateTime.timeZone();

  // Step 16. (Reordered)
  auto calendar = zonedDateTime.calendar();

  // Steps 15 and 17.
  int64_t offsetNanoseconds;
  if (!GetOffsetNanosecondsFor(cx, timeZone, zonedDateTime.instant(),
                               &offsetNanoseconds)) {
    return false;
  }
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  // Step 18.
  auto temporalDateTime =
      GetPlainDateTimeFor(zonedDateTime.instant(), offsetNanoseconds);

  // Step 19.
  Instant epochNanoseconds;
  if (smallestUnit == TemporalUnit::Day) {
    // Step 19.a.
    PlainDateTime dtStart;
    if (!CreateTemporalDateTime(cx, temporalDateTime.date, {}, &dtStart)) {
      return false;
    }

    // Step 19.b.
    auto dateEnd =
        BalanceISODate(temporalDateTime.date.year, temporalDateTime.date.month,
                       temporalDateTime.date.day + 1);

    // Step 19.c.
    PlainDateTime dtEnd;
    if (!CreateTemporalDateTime(cx, dateEnd, {}, &dtEnd)) {
      return false;
    }

    // Step 19.d.
    const auto& thisNs = zonedDateTime.instant();

    // Steps 19.e-f.
    Instant startNs;
    if (!GetInstantFor(cx, timeZone, dtStart,
                       TemporalDisambiguation::Compatible, &startNs)) {
      return false;
    }

    // Step 19.g.
    if (thisNs < startNs) {
      JS_ReportErrorNumberASCII(
          cx, GetErrorMessage, nullptr,
          JSMSG_TEMPORAL_ZONED_DATE_TIME_INCONSISTENT_INSTANT);
      return false;
    }

    // Steps 19.h-i.
    Instant endNs;
    if (!GetInstantFor(cx, timeZone, dtEnd, TemporalDisambiguation::Compatible,
                       &endNs)) {
      return false;
    }

    // Step 19.j.
    if (thisNs >= endNs) {
      JS_ReportErrorNumberASCII(
          cx, GetErrorMessage, nullptr,
          JSMSG_TEMPORAL_ZONED_DATE_TIME_INCONSISTENT_INSTANT);
      return false;
    }

    // Step 19.k.
    auto dayLengthNs = endNs - startNs;
    MOZ_ASSERT(IsValidInstantSpan(dayLengthNs));
    MOZ_ASSERT(dayLengthNs > InstantSpan{}, "dayLengthNs is positive");

    // Step 19.l. (Inlined NormalizedTimeDurationFromEpochNanosecondsDifference)
    auto dayProgressNs = thisNs - startNs;
    MOZ_ASSERT(IsValidInstantSpan(dayProgressNs));
    MOZ_ASSERT(dayProgressNs >= InstantSpan{}, "dayProgressNs is non-negative");

    MOZ_ASSERT(startNs <= thisNs && thisNs < endNs);
    MOZ_ASSERT(dayProgressNs < dayLengthNs);

    // Step 19.m. (Inlined RoundNormalizedTimeDurationToIncrement)
    auto rounded =
        RoundNumberToIncrement(dayProgressNs.toNanoseconds(),
                               dayLengthNs.toNanoseconds(), roundingMode);
    auto roundedDaysNs = InstantSpan::fromNanoseconds(rounded);
    MOZ_ASSERT(roundedDaysNs == InstantSpan{} || roundedDaysNs == dayLengthNs);
    MOZ_ASSERT(IsValidInstantSpan(roundedDaysNs));

    // Step 19.n.
    epochNanoseconds = startNs + roundedDaysNs;
    MOZ_ASSERT(epochNanoseconds == startNs || epochNanoseconds == endNs);
  } else {
    // Step 20.a.
    auto roundResult = RoundISODateTime(temporalDateTime, roundingIncrement,
                                        smallestUnit, roundingMode);

    // Step 20.b.
    if (!InterpretISODateTimeOffset(
            cx, roundResult, OffsetBehaviour::Option, offsetNanoseconds,
            timeZone, TemporalDisambiguation::Compatible,
            TemporalOffset::Prefer, MatchBehaviour::MatchExactly,
            &epochNanoseconds)) {
      return false;
    }
  }
  MOZ_ASSERT(IsValidEpochInstant(epochNanoseconds));

  // Step 22.
  auto* result =
      CreateTemporalZonedDateTime(cx, epochNanoseconds, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.round ( roundTo )
 */
static bool ZonedDateTime_round(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_round>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.equals ( other )
 */
static bool ZonedDateTime_equals(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  Rooted<ZonedDateTime> other(cx);
  if (!ToTemporalZonedDateTime(cx, args.get(0), &other)) {
    return false;
  }

  // Steps 4-6.
  bool equals = zonedDateTime.instant() == other.instant() &&
                TimeZoneEquals(zonedDateTime.timeZone(), other.timeZone()) &&
                CalendarEquals(zonedDateTime.calendar(), other.calendar());

  args.rval().setBoolean(equals);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.equals ( other )
 */
static bool ZonedDateTime_equals(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_equals>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.toString ( [ options ] )
 */
static bool ZonedDateTime_toString(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  SecondsStringPrecision precision = {Precision::Auto(),
                                      TemporalUnit::Nanosecond, Increment{1}};
  auto roundingMode = TemporalRoundingMode::Trunc;
  auto showCalendar = ShowCalendar::Auto;
  auto showTimeZone = ShowTimeZoneName::Auto;
  auto showOffset = ShowOffset::Auto;
  if (args.hasDefined(0)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "toString", args[0]));
    if (!options) {
      return false;
    }

    // Steps 4-5.
    if (!GetTemporalShowCalendarNameOption(cx, options, &showCalendar)) {
      return false;
    }

    // Step 6.
    auto digits = Precision::Auto();
    if (!GetTemporalFractionalSecondDigitsOption(cx, options, &digits)) {
      return false;
    }

    // Step 7.
    if (!GetTemporalShowOffsetOption(cx, options, &showOffset)) {
      return false;
    }

    // Step 8.
    if (!GetRoundingModeOption(cx, options, &roundingMode)) {
      return false;
    }

    // Step 9.
    auto smallestUnit = TemporalUnit::Auto;
    if (!GetTemporalUnitValuedOption(cx, options, TemporalUnitKey::SmallestUnit,
                                     TemporalUnitGroup::Time, &smallestUnit)) {
      return false;
    }

    // Step 10.
    if (smallestUnit == TemporalUnit::Hour) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INVALID_UNIT_OPTION, "hour",
                                "smallestUnit");
      return false;
    }

    // Step 11.
    if (!GetTemporalShowTimeZoneNameOption(cx, options, &showTimeZone)) {
      return false;
    }

    // Step 12.
    precision = ToSecondsStringPrecision(smallestUnit, digits);
  }

  // Step 13.
  JSString* str = TemporalZonedDateTimeToString(
      cx, zonedDateTime, precision.precision, showCalendar, showTimeZone,
      showOffset, precision.increment, precision.unit, roundingMode);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toString ( [ options ] )
 */
static bool ZonedDateTime_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toString>(cx,
                                                                       args);
}

/**
 * Temporal.ZonedDateTime.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool ZonedDateTime_toLocaleString(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  JSString* str = TemporalZonedDateTimeToString(
      cx, zonedDateTime, Precision::Auto(), ShowCalendar::Auto,
      ShowTimeZoneName::Auto, ShowOffset::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool ZonedDateTime_toLocaleString(JSContext* cx, unsigned argc,
                                         Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toLocaleString>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.toJSON ( )
 */
static bool ZonedDateTime_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  JSString* str = TemporalZonedDateTimeToString(
      cx, zonedDateTime, Precision::Auto(), ShowCalendar::Auto,
      ShowTimeZoneName::Auto, ShowOffset::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toJSON ( )
 */
static bool ZonedDateTime_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toJSON>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.valueOf ( )
 */
static bool ZonedDateTime_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "ZonedDateTime", "primitive type");
  return false;
}

/**
 * Temporal.ZonedDateTime.prototype.startOfDay ( )
 */
static bool ZonedDateTime_startOfDay(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  auto timeZone = zonedDateTime.timeZone();

  // Step 4.
  auto calendar = zonedDateTime.calendar();

  // Step 5.
  const auto& instant = zonedDateTime.instant();

  // Steps 5-6.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &temporalDateTime)) {
    return false;
  }

  // Step 7.
  PlainDateTime startDateTime;
  if (!CreateTemporalDateTime(cx, temporalDateTime.date, {}, &startDateTime)) {
    return false;
  }

  // Step 8.
  Instant startInstant;
  if (!GetInstantFor(cx, timeZone, startDateTime,
                     TemporalDisambiguation::Compatible, &startInstant)) {
    return false;
  }

  // Step 9.
  auto* result =
      CreateTemporalZonedDateTime(cx, startInstant, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.startOfDay ( )
 */
static bool ZonedDateTime_startOfDay(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_startOfDay>(cx,
                                                                         args);
}

/**
 * Temporal.ZonedDateTime.prototype.getTimeZoneTransition ( directionParam )
 */
static bool ZonedDateTime_getTimeZoneTransition(JSContext* cx,
                                                const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Step 3.
  auto timeZone = zonedDateTime.timeZone();

  // Steps 5-8.
  auto direction = Direction::Next;
  if (args.get(0).isString()) {
    // Step 5. (Not applicable in our implementation.)

    // Steps 6 and 8.
    Rooted<JSString*> directionString(cx, args[0].toString());
    if (!GetDirectionOption(cx, directionString, &direction)) {
      return false;
    }
  } else {
    // Steps 5 and 7.
    Rooted<JSObject*> options(cx, RequireObjectArg(cx, "getTimeZoneTransition",
                                                   "direction", args.get(0)));
    if (!options) {
      return false;
    }

    // Step 8.
    if (!GetDirectionOption(cx, options, &direction)) {
      return false;
    }
  }

  // Step 9.
  if (timeZone.isOffset()) {
    args.rval().setNull();
    return true;
  }

  // FIXME: spec issue - why is this special case needed?
  // https://github.com/tc39/proposal-temporal/issues/2951

  if (StringEqualsLiteral(timeZone.identifier(), "UTC")) {
    args.rval().setNull();
    return true;
  }

  // Steps 10-11.
  mozilla::Maybe<Instant> transition;
  if (direction == Direction::Next) {
    if (!GetNamedTimeZoneNextTransition(cx, timeZone, zonedDateTime.instant(),
                                        &transition)) {
      return false;
    }
  } else {
    if (!GetNamedTimeZonePreviousTransition(
            cx, timeZone, zonedDateTime.instant(), &transition)) {
      return false;
    }
  }

  // Step 12.
  if (!transition) {
    args.rval().setNull();
    return true;
  }

  // Step 13.
  auto* result = CreateTemporalZonedDateTime(cx, *transition, timeZone,
                                             zonedDateTime.calendar());
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.getTimeZoneTransition ( directionParam )
 */
static bool ZonedDateTime_getTimeZoneTransition(JSContext* cx, unsigned argc,
                                                Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime,
                              ZonedDateTime_getTimeZoneTransition>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.toInstant ( )
 */
static bool ZonedDateTime_toInstant(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);

  // Step 3.
  auto* result = CreateTemporalInstant(cx, instant);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toInstant ( )
 */
static bool ZonedDateTime_toInstant(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toInstant>(cx,
                                                                        args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDate ( )
 */
static bool ZonedDateTime_toPlainDate(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-6.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &temporalDateTime)) {
    return false;
  }

  // Step 7.
  auto* result =
      CreateTemporalDate(cx, temporalDateTime.date, zonedDateTime.calendar());
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDate ( )
 */
static bool ZonedDateTime_toPlainDate(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainDate>(cx,
                                                                          args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainTime ( )
 */
static bool ZonedDateTime_toPlainTime(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-6.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &temporalDateTime)) {
    return false;
  }

  // Step 7.
  auto* result = CreateTemporalTime(cx, temporalDateTime.time);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainTime ( )
 */
static bool ZonedDateTime_toPlainTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainTime>(cx,
                                                                          args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDateTime ( )
 */
static bool ZonedDateTime_toPlainDateTime(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTime> zonedDateTime(
      cx, ZonedDateTime{&args.thisv().toObject().as<ZonedDateTimeObject>()});

  // Steps 3-4.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, zonedDateTime.timeZone(),
                           zonedDateTime.instant(), &dateTime)) {
    return false;
  }
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));

  auto* result = CreateTemporalDateTime(cx, dateTime, zonedDateTime.calendar());
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDateTime ( )
 */
static bool ZonedDateTime_toPlainDateTime(JSContext* cx, unsigned argc,
                                          Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainDateTime>(
      cx, args);
}

const JSClass ZonedDateTimeObject::class_ = {
    "Temporal.ZonedDateTime",
    JSCLASS_HAS_RESERVED_SLOTS(ZonedDateTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_ZonedDateTime),
    JS_NULL_CLASS_OPS,
    &ZonedDateTimeObject::classSpec_,
};

const JSClass& ZonedDateTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec ZonedDateTime_methods[] = {
    JS_FN("from", ZonedDateTime_from, 1, 0),
    JS_FN("compare", ZonedDateTime_compare, 2, 0),
    JS_FS_END,
};

static const JSFunctionSpec ZonedDateTime_prototype_methods[] = {
    JS_FN("with", ZonedDateTime_with, 1, 0),
    JS_FN("withPlainTime", ZonedDateTime_withPlainTime, 0, 0),
    JS_FN("withTimeZone", ZonedDateTime_withTimeZone, 1, 0),
    JS_FN("withCalendar", ZonedDateTime_withCalendar, 1, 0),
    JS_FN("add", ZonedDateTime_add, 1, 0),
    JS_FN("subtract", ZonedDateTime_subtract, 1, 0),
    JS_FN("until", ZonedDateTime_until, 1, 0),
    JS_FN("since", ZonedDateTime_since, 1, 0),
    JS_FN("round", ZonedDateTime_round, 1, 0),
    JS_FN("equals", ZonedDateTime_equals, 1, 0),
    JS_FN("toString", ZonedDateTime_toString, 0, 0),
    JS_FN("toLocaleString", ZonedDateTime_toLocaleString, 0, 0),
    JS_FN("toJSON", ZonedDateTime_toJSON, 0, 0),
    JS_FN("valueOf", ZonedDateTime_valueOf, 0, 0),
    JS_FN("startOfDay", ZonedDateTime_startOfDay, 0, 0),
    JS_FN("getTimeZoneTransition", ZonedDateTime_getTimeZoneTransition, 1, 0),
    JS_FN("toInstant", ZonedDateTime_toInstant, 0, 0),
    JS_FN("toPlainDate", ZonedDateTime_toPlainDate, 0, 0),
    JS_FN("toPlainTime", ZonedDateTime_toPlainTime, 0, 0),
    JS_FN("toPlainDateTime", ZonedDateTime_toPlainDateTime, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec ZonedDateTime_prototype_properties[] = {
    JS_PSG("calendarId", ZonedDateTime_calendarId, 0),
    JS_PSG("timeZoneId", ZonedDateTime_timeZoneId, 0),
    JS_PSG("era", ZonedDateTime_era, 0),
    JS_PSG("eraYear", ZonedDateTime_eraYear, 0),
    JS_PSG("year", ZonedDateTime_year, 0),
    JS_PSG("month", ZonedDateTime_month, 0),
    JS_PSG("monthCode", ZonedDateTime_monthCode, 0),
    JS_PSG("day", ZonedDateTime_day, 0),
    JS_PSG("hour", ZonedDateTime_hour, 0),
    JS_PSG("minute", ZonedDateTime_minute, 0),
    JS_PSG("second", ZonedDateTime_second, 0),
    JS_PSG("millisecond", ZonedDateTime_millisecond, 0),
    JS_PSG("microsecond", ZonedDateTime_microsecond, 0),
    JS_PSG("nanosecond", ZonedDateTime_nanosecond, 0),
    JS_PSG("epochMilliseconds", ZonedDateTime_epochMilliseconds, 0),
    JS_PSG("epochNanoseconds", ZonedDateTime_epochNanoseconds, 0),
    JS_PSG("dayOfWeek", ZonedDateTime_dayOfWeek, 0),
    JS_PSG("dayOfYear", ZonedDateTime_dayOfYear, 0),
    JS_PSG("weekOfYear", ZonedDateTime_weekOfYear, 0),
    JS_PSG("yearOfWeek", ZonedDateTime_yearOfWeek, 0),
    JS_PSG("hoursInDay", ZonedDateTime_hoursInDay, 0),
    JS_PSG("daysInWeek", ZonedDateTime_daysInWeek, 0),
    JS_PSG("daysInMonth", ZonedDateTime_daysInMonth, 0),
    JS_PSG("daysInYear", ZonedDateTime_daysInYear, 0),
    JS_PSG("monthsInYear", ZonedDateTime_monthsInYear, 0),
    JS_PSG("inLeapYear", ZonedDateTime_inLeapYear, 0),
    JS_PSG("offsetNanoseconds", ZonedDateTime_offsetNanoseconds, 0),
    JS_PSG("offset", ZonedDateTime_offset, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.ZonedDateTime", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec ZonedDateTimeObject::classSpec_ = {
    GenericCreateConstructor<ZonedDateTimeConstructor, 2,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<ZonedDateTimeObject>,
    ZonedDateTime_methods,
    nullptr,
    ZonedDateTime_prototype_methods,
    ZonedDateTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
