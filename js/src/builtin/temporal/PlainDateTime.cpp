/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainDateTime.h"

#include "mozilla/Assertions.h"

#include <algorithm>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Duration.h"
#include "builtin/temporal/PlainDate.h"
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
#include "builtin/temporal/ZonedDateTime.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/BytecodeUtil.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsPlainDateTime(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainDateTimeObject>();
}

#ifdef DEBUG
/**
 * IsValidISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond )
 */
bool js::temporal::IsValidISODateTime(const PlainDateTime& dateTime) {
  return IsValidISODate(dateTime.date) && IsValidTime(dateTime.time);
}
#endif

/**
 * IsValidISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond )
 */
static bool ThrowIfInvalidISODateTime(JSContext* cx,
                                      const PlainDateTime& dateTime) {
  return ThrowIfInvalidISODate(cx, dateTime.date) &&
         ThrowIfInvalidTime(cx, dateTime.time);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ISODateTimeWithinLimits(T year, T month, T day, T hour, T minute,
                                    T second, T millisecond, T microsecond,
                                    T nanosecond) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  MOZ_ASSERT(IsValidISODate(year, month, day));
  MOZ_ASSERT(
      IsValidTime(hour, minute, second, millisecond, microsecond, nanosecond));

  // js> new Date(-8_64000_00000_00000).toISOString()
  // "-271821-04-20T00:00:00.000Z"
  //
  // js> new Date(+8_64000_00000_00000).toISOString()
  // "+275760-09-13T00:00:00.000Z"

  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // Definitely in range.
  if (minYear < year && year < maxYear) {
    return true;
  }

  // -271821 April, 20
  if (year < 0) {
    if (year != minYear) {
      return false;
    }
    if (month != 4) {
      return month > 4;
    }
    if (day != (20 - 1)) {
      return day > (20 - 1);
    }
    // Needs to be past midnight on April, 19.
    return !(hour == 0 && minute == 0 && second == 0 && millisecond == 0 &&
             microsecond == 0 && nanosecond == 0);
  }

  // 275760 September, 13
  if (year != maxYear) {
    return false;
  }
  if (month != 9) {
    return month < 9;
  }
  if (day > 13) {
    return false;
  }
  return true;
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ISODateTimeWithinLimits(T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  MOZ_ASSERT(IsValidISODate(year, month, day));

  // js> new Date(-8_64000_00000_00000).toISOString()
  // "-271821-04-20T00:00:00.000Z"
  //
  // js> new Date(+8_64000_00000_00000).toISOString()
  // "+275760-09-13T00:00:00.000Z"

  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // ISODateTimeWithinLimits is called with hour=12 and the remaining time
  // components set to zero. That means the maximum value is exclusive, whereas
  // the minimum value is inclusive.

  // Definitely in range.
  if (minYear < year && year < maxYear) {
    return true;
  }

  // -271821 April, 20
  if (year < 0) {
    if (year != minYear) {
      return false;
    }
    if (month != 4) {
      return month > 4;
    }
    if (day < (20 - 1)) {
      return false;
    }
    return true;
  }

  // 275760 September, 13
  if (year != maxYear) {
    return false;
  }
  if (month != 9) {
    return month < 9;
  }
  if (day > 13) {
    return false;
  }
  return true;
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(double year, double month,
                                           double day) {
  return ::ISODateTimeWithinLimits(year, month, day);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(const PlainDateTime& dateTime) {
  const auto& [date, time] = dateTime;
  return ::ISODateTimeWithinLimits(date.year, date.month, date.day, time.hour,
                                   time.minute, time.second, time.millisecond,
                                   time.microsecond, time.nanosecond);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(const PlainDate& date) {
  return ::ISODateTimeWithinLimits(date.year, date.month, date.day);
}

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
static PlainDateTimeObject* CreateTemporalDateTime(
    JSContext* cx, const CallArgs& args, double isoYear, double isoMonth,
    double isoDay, double hour, double minute, double second,
    double millisecond, double microsecond, double nanosecond,
    Handle<CalendarValue> calendar) {
  MOZ_ASSERT(IsInteger(isoYear));
  MOZ_ASSERT(IsInteger(isoMonth));
  MOZ_ASSERT(IsInteger(isoDay));
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, isoYear, isoMonth, isoDay)) {
    return nullptr;
  }

  // Step 2.
  if (!ThrowIfInvalidTime(cx, hour, minute, second, millisecond, microsecond,
                          nanosecond)) {
    return nullptr;
  }

  // Step 3.
  if (!ISODateTimeWithinLimits(isoYear, isoMonth, isoDay, hour, minute, second,
                               millisecond, microsecond, nanosecond)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_TIME_INVALID);
    return nullptr;
  }

  // Steps 4-5.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainDateTime,
                                          &proto)) {
    return nullptr;
  }

  auto* dateTime = NewObjectWithClassProto<PlainDateTimeObject>(cx, proto);
  if (!dateTime) {
    return nullptr;
  }

  // Step 6.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_YEAR_SLOT,
                         Int32Value(int32_t(isoYear)));

  // Step 7.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MONTH_SLOT,
                         Int32Value(int32_t(isoMonth)));

  // Step 8.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_DAY_SLOT,
                         Int32Value(int32_t(isoDay)));

  // Step 9.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_HOUR_SLOT,
                         Int32Value(int32_t(hour)));

  // Step 10.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MINUTE_SLOT,
                         Int32Value(int32_t(minute)));

  // Step 11.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_SECOND_SLOT,
                         Int32Value(int32_t(second)));

  // Step 12.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MILLISECOND_SLOT,
                         Int32Value(int32_t(millisecond)));

  // Step 13.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MICROSECOND_SLOT,
                         Int32Value(int32_t(microsecond)));

  // Step 14.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_NANOSECOND_SLOT,
                         Int32Value(int32_t(nanosecond)));

  // Step 15.
  dateTime->setFixedSlot(PlainDateTimeObject::CALENDAR_SLOT,
                         calendar.toSlotValue());

  // Step 16.
  return dateTime;
}

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
PlainDateTimeObject* js::temporal::CreateTemporalDateTime(
    JSContext* cx, const PlainDateTime& dateTime,
    Handle<CalendarValue> calendar) {
  const auto& [date, time] = dateTime;
  const auto& [isoYear, isoMonth, isoDay] = date;
  const auto& [hour, minute, second, millisecond, microsecond, nanosecond] =
      time;

  // Steps 1-2.
  if (!ThrowIfInvalidISODateTime(cx, dateTime)) {
    return nullptr;
  }

  // Step 3.
  if (!ISODateTimeWithinLimits(dateTime)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_TIME_INVALID);
    return nullptr;
  }

  // Steps 4-5.
  auto* object = NewBuiltinClassInstance<PlainDateTimeObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 6.
  object->setFixedSlot(PlainDateTimeObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 7.
  object->setFixedSlot(PlainDateTimeObject::ISO_MONTH_SLOT,
                       Int32Value(isoMonth));

  // Step 8.
  object->setFixedSlot(PlainDateTimeObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 9.
  object->setFixedSlot(PlainDateTimeObject::ISO_HOUR_SLOT, Int32Value(hour));

  // Step 10.
  object->setFixedSlot(PlainDateTimeObject::ISO_MINUTE_SLOT,
                       Int32Value(minute));

  // Step 11.
  object->setFixedSlot(PlainDateTimeObject::ISO_SECOND_SLOT,
                       Int32Value(second));

  // Step 12.
  object->setFixedSlot(PlainDateTimeObject::ISO_MILLISECOND_SLOT,
                       Int32Value(millisecond));

  // Step 13.
  object->setFixedSlot(PlainDateTimeObject::ISO_MICROSECOND_SLOT,
                       Int32Value(microsecond));

  // Step 14.
  object->setFixedSlot(PlainDateTimeObject::ISO_NANOSECOND_SLOT,
                       Int32Value(nanosecond));

  // Step 15.
  object->setFixedSlot(PlainDateTimeObject::CALENDAR_SLOT,
                       calendar.toSlotValue());

  // Step 16.
  return object;
}

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
static PlainDateTimeObject* CreateTemporalDateTime(
    JSContext* cx, Handle<PlainDateTimeWithCalendar> dateTime) {
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));
  return CreateTemporalDateTime(cx, dateTime, dateTime.calendar());
}

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
static bool CreateTemporalDateTime(
    JSContext* cx, const PlainDateTime& dateTime,
    Handle<CalendarValue> calendar,
    MutableHandle<PlainDateTimeWithCalendar> result) {
  // Steps 1-2.
  if (!ThrowIfInvalidISODateTime(cx, dateTime)) {
    return false;
  }

  // Step 3.
  if (!ISODateTimeWithinLimits(dateTime)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_TIME_INVALID);
    return false;
  }

  result.set(PlainDateTimeWithCalendar{dateTime, calendar});
  return true;
}

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
bool js::temporal::CreateTemporalDateTime(JSContext* cx, const PlainDate& date,
                                          const PlainTime& time,
                                          PlainDateTime* result) {
  auto dateTime = PlainDateTime{date, time};

  // Steps 1-2.
  if (!ThrowIfInvalidISODateTime(cx, dateTime)) {
    return false;
  }

  // Step 3.
  if (!ISODateTimeWithinLimits(dateTime)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_TIME_INVALID);
    return false;
  }

  *result = dateTime;
  return true;
}

#ifdef DEBUG
static bool IsPositiveInteger(double value) {
  return IsInteger(value) && value >= 0;
}
#endif

/**
 * ToTemporalTimeRecord ( temporalTimeLike [ , completeness ] )
 */
static auto ToTemporalTimeRecord(const TemporalFields& temporalTimeLike) {
  MOZ_ASSERT(IsPositiveInteger(temporalTimeLike.hour()));
  MOZ_ASSERT(IsPositiveInteger(temporalTimeLike.minute()));
  MOZ_ASSERT(IsPositiveInteger(temporalTimeLike.second()));
  MOZ_ASSERT(IsPositiveInteger(temporalTimeLike.millisecond()));
  MOZ_ASSERT(IsPositiveInteger(temporalTimeLike.microsecond()));
  MOZ_ASSERT(IsPositiveInteger(temporalTimeLike.nanosecond()));

  return TemporalTimeLike{
      temporalTimeLike.hour(),        temporalTimeLike.minute(),
      temporalTimeLike.second(),      temporalTimeLike.millisecond(),
      temporalTimeLike.microsecond(), temporalTimeLike.nanosecond(),
  };
}

/**
 * InterpretTemporalDateTimeFields ( calendar, fields, overflow )
 */
bool js::temporal::InterpretTemporalDateTimeFields(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<TemporalFields> fields, TemporalOverflow overflow,
    PlainDateTime* result) {
  // FIXME: spec issue - ToTemporalTimeRecord is infallible

  // Step 1.
  auto timeResult = ::ToTemporalTimeRecord(fields);

  // Step 2.
  Rooted<PlainDateWithCalendar> temporalDate(cx);
  if (!CalendarDateFromFields(cx, calendar, fields, overflow, &temporalDate)) {
    return false;
  }

  // Step 3.
  PlainTime time;
  if (!RegulateTime(cx, timeResult, overflow, &time)) {
    return false;
  }

  // Step 4.
  *result = {temporalDate.date(), time};
  return true;
}

/**
 * ToTemporalDateTime ( item [ , overflow ] )
 */
static bool ToTemporalDateTime(
    JSContext* cx, Handle<JSObject*> item, TemporalOverflow overflow,
    MutableHandle<PlainDateTimeWithCalendar> result) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.a.
  if (auto* plainDateTime = item->maybeUnwrapIf<PlainDateTimeObject>()) {
    auto dateTime = ToPlainDateTime(plainDateTime);
    Rooted<CalendarValue> calendar(cx, plainDateTime->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Step 2.a.i.
    result.set(PlainDateTimeWithCalendar{dateTime, calendar});
    return true;
  }

  // Step 2.b.
  if (auto* zonedDateTime = item->maybeUnwrapIf<ZonedDateTimeObject>()) {
    auto epochInstant = ToInstant(zonedDateTime);
    Rooted<TimeZoneValue> timeZone(cx, zonedDateTime->timeZone());
    Rooted<CalendarValue> calendar(cx, zonedDateTime->calendar());

    if (!timeZone.wrap(cx)) {
      return false;
    }
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Steps 2.b.i-ii.
    PlainDateTime dateTime;
    if (!GetPlainDateTimeFor(cx, timeZone, epochInstant, &dateTime)) {
      return false;
    }

    result.set(PlainDateTimeWithCalendar{dateTime, calendar});
    return true;
  }

  // Step 2.c.
  if (auto* plainDate = item->maybeUnwrapIf<PlainDateObject>()) {
    auto date = ToPlainDate(plainDate);
    Rooted<CalendarValue> calendar(cx, plainDate->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Step 2.c.i.
    return CreateTemporalDateTime(cx, PlainDateTime{date}, calendar, result);
  }

  // Step 2.d.
  Rooted<CalendarValue> calendar(cx);
  if (!GetTemporalCalendarWithISODefault(cx, item, &calendar)) {
    return false;
  }

  // Step 2.e.
  Rooted<TemporalFields> fields(cx);
  if (!PrepareCalendarFields(cx, calendar, item,
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
                                 TemporalField::Second,
                             },
                             &fields)) {
    return false;
  }

  // Step 2.f.
  PlainDateTime dateTime;
  if (!InterpretTemporalDateTimeFields(cx, calendar, fields, overflow,
                                       &dateTime)) {
    return false;
  }

  // Step 4.
  return CreateTemporalDateTime(cx, dateTime, calendar, result);
}

/**
 * ToTemporalDateTime ( item [ , overflow ] )
 */
static bool ToTemporalDateTime(
    JSContext* cx, Handle<Value> item, TemporalOverflow overflow,
    MutableHandle<PlainDateTimeWithCalendar> result) {
  // Step 1. (Not applicable)

  // Step 2.
  if (item.isObject()) {
    Rooted<JSObject*> itemObj(cx, &item.toObject());
    return ToTemporalDateTime(cx, itemObj, overflow, result);
  }

  // Step 3.a.
  if (!item.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, item,
                     nullptr, "not a string");
    return false;
  }
  Rooted<JSString*> string(cx, item.toString());

  // Step 3.b.
  PlainDateTime dateTime;
  Rooted<JSString*> calendarString(cx);
  if (!ParseTemporalDateTimeString(cx, string, &dateTime, &calendarString)) {
    return false;
  }

  // Step 3.c.
  MOZ_ASSERT(IsValidISODate(dateTime.date));

  // Step 3.d.
  MOZ_ASSERT(IsValidTime(dateTime.time));

  // Steps 3.e-h.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  if (calendarString) {
    if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
      return false;
    }
  }

  // Step 4.
  return CreateTemporalDateTime(cx, dateTime, calendar, result);
}

/**
 * ToTemporalDateTime ( item [ , overflow ] )
 */
static bool ToTemporalDateTime(
    JSContext* cx, Handle<Value> item,
    MutableHandle<PlainDateTimeWithCalendar> result) {
  return ToTemporalDateTime(cx, item, TemporalOverflow::Constrain, result);
}

/**
 * CompareISODateTime ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2,
 * d2, h2, min2, s2, ms2, mus2, ns2 )
 */
static int32_t CompareISODateTime(const PlainDateTime& one,
                                  const PlainDateTime& two) {
  // Step 1. (Not applicable in our implementation.)

  // Steps 2-3.
  if (int32_t dateResult = CompareISODate(one.date, two.date)) {
    return dateResult;
  }

  // Steps 4.
  return CompareTemporalTime(one.time, two.time);
}

/**
 * AddDateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond, calendar, years, months, weeks, days, norm, overflow
 * )
 */
static bool AddDateTime(JSContext* cx, const PlainDateTime& dateTime,
                        Handle<CalendarValue> calendar,
                        const NormalizedDuration& duration,
                        TemporalOverflow overflow, PlainDateTime* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  // Step 1.
  MOZ_ASSERT(IsValidISODateTime(dateTime));

  // Step 2.
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));

  // Step 3.
  auto timeResult = AddTime(dateTime.time, duration.time);

  // Step 4.
  const auto& datePart = dateTime.date;

  // Step 5.
  auto dateDuration = DateDuration{
      duration.date.years,
      duration.date.months,
      duration.date.weeks,
      duration.date.days + timeResult.days,
  };
  if (!ThrowIfInvalidDuration(cx, dateDuration)) {
    return false;
  }

  // Step 6.
  PlainDate addedDate;
  if (!AddDate(cx, calendar, datePart, dateDuration, overflow, &addedDate)) {
    return false;
  }

  // Step 7.
  *result = {addedDate, timeResult.time};
  return true;
}

/**
 * DifferenceISODateTime ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2,
 * d2, h2, min2, s2, ms2, mus2, ns2, calendar, largestUnit )
 */
static bool DifferenceISODateTime(JSContext* cx, const PlainDateTime& one,
                                  const PlainDateTime& two,
                                  Handle<CalendarValue> calendar,
                                  TemporalUnit largestUnit,
                                  NormalizedDuration* result) {
  // Steps 1-2.
  MOZ_ASSERT(IsValidISODateTime(one));
  MOZ_ASSERT(IsValidISODateTime(two));
  MOZ_ASSERT(ISODateTimeWithinLimits(one));
  MOZ_ASSERT(ISODateTimeWithinLimits(two));

  // Step 3.
  auto timeDuration = DifferenceTime(one.time, two.time);

  // Step 4.
  int32_t timeSign = NormalizedTimeDurationSign(timeDuration);

  // Step 5.
  int32_t dateSign = CompareISODate(two.date, one.date);

  // Step 6.
  auto adjustedDate = two.date;

  // Step 7.
  if (timeSign == -dateSign) {
    // Step 7.a.
    adjustedDate = BalanceISODate(adjustedDate.year, adjustedDate.month,
                                  adjustedDate.day + timeSign);

    // Step 7.b.
    if (!Add24HourDaysToNormalizedTimeDuration(cx, timeDuration, -timeSign,
                                               &timeDuration)) {
      return false;
    }
  }

  MOZ_ASSERT(IsValidISODate(adjustedDate));
  MOZ_ASSERT(ISODateTimeWithinLimits(adjustedDate));

  // Step 8.
  const auto& date1 = one.date;

  // Step 9.
  const auto& date2 = adjustedDate;

  // Step 10.
  auto dateLargestUnit = std::min(TemporalUnit::Day, largestUnit);

  // Step 11.
  DateDuration dateDifference;
  if (!CalendarDateUntil(cx, calendar, date1, date2, dateLargestUnit,
                         &dateDifference)) {
    return false;
  }

  // Step 12. (Not applicable in our implementation.)

  // Steps 13.
  if (largestUnit != dateLargestUnit) {
    // Step 13.a.
    if (!Add24HourDaysToNormalizedTimeDuration(
            cx, timeDuration, dateDifference.days, &timeDuration)) {
      return false;
    }

    // Step 13.b.
    dateDifference.days = 0;
  }

  // Step 14.
  return CreateNormalizedDurationRecord(cx, dateDifference, timeDuration,
                                        result);
}

/**
 * RoundISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond, increment, unit, roundingMode )
 */
PlainDateTime js::temporal::RoundISODateTime(
    const PlainDateTime& dateTime, Increment increment, TemporalUnit unit,
    TemporalRoundingMode roundingMode) {
  const auto& [date, time] = dateTime;

  // Step 1.
  MOZ_ASSERT(IsValidISODateTime(dateTime));

  // Step 2.
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));

  // Step 3.
  auto roundedTime = RoundTime(time, increment, unit, roundingMode);
  MOZ_ASSERT(0 <= roundedTime.days && roundedTime.days <= 1);

  // Step 4.
  auto balanceResult = BalanceISODate(date.year, date.month,
                                      date.day + int32_t(roundedTime.days));

  // Step 5.
  return {balanceResult, roundedTime.time};
}

/**
 * DifferencePlainDateTimeWithRounding ( y1, mon1, d1, h1, min1, s1, ms1, mus1,
 * ns1, y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2, calendar, largestUnit,
 * roundingIncrement, smallestUnit, roundingMode )
 */
bool js::temporal::DifferencePlainDateTimeWithRounding(
    JSContext* cx, const PlainDateTime& one, const PlainDateTime& two,
    Handle<CalendarValue> calendar, const DifferenceSettings& settings,
    Duration* result) {
  // Steps 1-2.
  MOZ_ASSERT(ISODateTimeWithinLimits(one));
  MOZ_ASSERT(ISODateTimeWithinLimits(two));

  // Step 3.
  if (one == two) {
    // Steps 3.a-b.
    *result = {};
    return true;
  }

  // Step 4.
  NormalizedDuration diff;
  if (!::DifferenceISODateTime(cx, one, two, calendar, settings.largestUnit,
                               &diff)) {
    return false;
  }

  // Step 5.
  if (settings.smallestUnit == TemporalUnit::Nanosecond &&
      settings.roundingIncrement == Increment{1}) {
    // Step 5.a.
    NormalizedTimeDuration withDays;
    if (!Add24HourDaysToNormalizedTimeDuration(cx, diff.time, diff.date.days,
                                               &withDays)) {
      return false;
    }

    // Step 5.b.
    TimeDuration timeResult;
    if (!BalanceTimeDuration(cx, withDays, settings.largestUnit, &timeResult)) {
      return false;
    }

    // Step 5.c. (Not application in our implementation.)

    // Steps 5.d-e.
    *result = {
        double(diff.date.years),    double(diff.date.months),
        double(diff.date.weeks),    double(timeResult.days),
        double(timeResult.hours),   double(timeResult.minutes),
        double(timeResult.seconds), double(timeResult.milliseconds),
        timeResult.microseconds,    timeResult.nanoseconds,
    };
    MOZ_ASSERT(IsValidDuration(*result));
    return true;
  }

  // Step 6.
  const auto& dateTime = one;

  // Step 7.
  auto destEpochNs = GetUTCEpochNanoseconds(two);

  // Step 8.
  Rooted<TimeZoneValue> timeZone(cx, TimeZoneValue{});
  RoundedRelativeDuration relative;
  if (!RoundRelativeDuration(cx, diff, destEpochNs, dateTime, calendar,
                             timeZone, settings.largestUnit,
                             settings.roundingIncrement, settings.smallestUnit,
                             settings.roundingMode, &relative)) {
    return false;
  }
  MOZ_ASSERT(IsValidDuration(relative.duration));

  *result = relative.duration;
  return true;
}

/**
 * DifferencePlainDateTimeWithRounding ( y1, mon1, d1, h1, min1, s1, ms1, mus1,
 * ns1, y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2, calendar, largestUnit,
 * roundingIncrement, smallestUnit, roundingMode )
 */
bool js::temporal::DifferencePlainDateTimeWithRounding(
    JSContext* cx, const PlainDateTime& one, const PlainDateTime& two,
    Handle<CalendarValue> calendar, TemporalUnit unit, double* result) {
  // Steps 1-2.
  MOZ_ASSERT(ISODateTimeWithinLimits(one));
  MOZ_ASSERT(ISODateTimeWithinLimits(two));

  // Step 3.
  if (one == two) {
    // Steps 3.a-b.
    *result = 0;
    return true;
  }

  // Step 4.
  NormalizedDuration diff;
  if (!DifferenceISODateTime(cx, one, two, calendar, unit, &diff)) {
    return false;
  }

  // Step 5.
  if (unit == TemporalUnit::Nanosecond) {
    // Step 5.a.
    NormalizedTimeDuration withDays;
    if (!Add24HourDaysToNormalizedTimeDuration(cx, diff.time, diff.date.days,
                                               &withDays)) {
      return false;
    }

    // Step 5.b. (Not application in our implementation.)

    // Steps 5.c-d.
    *result = double(withDays.toNanoseconds());
    return true;
  }

  // Step 6.
  const auto& dateTime = one;

  // Step 7.
  auto destEpochNs = GetUTCEpochNanoseconds(two);

  // Step 8.
  Rooted<TimeZoneValue> timeZone(cx, TimeZoneValue{});
  RoundedRelativeDuration relative;
  if (!RoundRelativeDuration(cx, diff, destEpochNs, dateTime, calendar,
                             timeZone, unit, Increment{1}, unit,
                             TemporalRoundingMode::Trunc, &relative)) {
    return false;
  }
  MOZ_ASSERT(!std::isnan(relative.total));

  *result = relative.total;
  return true;
}

/**
 * DifferenceTemporalPlainDateTime ( operation, dateTime, other, options )
 */
static bool DifferenceTemporalPlainDateTime(JSContext* cx,
                                            TemporalDifference operation,
                                            const CallArgs& args) {
  Rooted<PlainDateTimeWithCalendar> dateTime(
      cx, &args.thisv().toObject().as<PlainDateTimeObject>());

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Rooted<PlainDateTimeWithCalendar> other(cx);
  if (!ToTemporalDateTime(cx, args.get(0), &other)) {
    return false;
  }

  // Step 3.
  if (!CalendarEquals(dateTime.calendar(), other.calendar())) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE,
        ToTemporalCalendarIdentifier(dateTime.calendar()).data(),
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
            TemporalUnit::Nanosecond, TemporalUnit::Day, &settings)) {
      return false;
    }
  } else {
    // Steps 4-5.
    settings = {
        TemporalUnit::Nanosecond,
        TemporalUnit::Day,
        TemporalRoundingMode::Trunc,
        Increment{1},
    };
  }

  // Steps 6-8.
  if (ToPlainDateTime(dateTime) == ToPlainDateTime(other)) {
    auto* obj = CreateTemporalDuration(cx, {});
    if (!obj) {
      return false;
    }

    args.rval().setObject(*obj);
    return true;
  }

  // Steps 9-10.
  Duration duration;
  if (!DifferencePlainDateTimeWithRounding(
          cx, dateTime, other, dateTime.calendar(), settings, &duration)) {
    return false;
  }
  MOZ_ASSERT(IsValidDuration(duration));

  // Step 11.
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

enum class PlainDateTimeDuration { Add, Subtract };

/**
 * AddDurationToOrSubtractDurationFromPlainDateTime ( operation, dateTime,
 * temporalDurationLike, options )
 */
static bool AddDurationToOrSubtractDurationFromPlainDateTime(
    JSContext* cx, PlainDateTimeDuration operation, const CallArgs& args) {
  Rooted<PlainDateTimeWithCalendar> dateTime(
      cx, &args.thisv().toObject().as<PlainDateTimeObject>());

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
        operation == PlainDateTimeDuration::Add ? "add" : "subtract";

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
  if (operation == PlainDateTimeDuration::Subtract) {
    duration = duration.negate();
  }
  auto normalized = CreateNormalizedDurationRecord(duration);

  // Step 6
  PlainDateTime result;
  if (!AddDateTime(cx, dateTime, dateTime.calendar(), normalized, overflow,
                   &result)) {
    return false;
  }

  // Steps 7-8.
  MOZ_ASSERT(IsValidISODateTime(result));

  // Step 9.
  auto* obj = CreateTemporalDateTime(cx, result, dateTime.calendar());
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime ( isoYear, isoMonth, isoDay [ , hour [ , minute [ ,
 * second [ , millisecond [ , microsecond [ , nanosecond [ , calendarLike ] ] ]
 * ] ] ] ] )
 */
static bool PlainDateTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainDateTime")) {
    return false;
  }

  // Step 2.
  double isoYear;
  if (!ToIntegerWithTruncation(cx, args.get(0), "year", &isoYear)) {
    return false;
  }

  // Step 3.
  double isoMonth;
  if (!ToIntegerWithTruncation(cx, args.get(1), "month", &isoMonth)) {
    return false;
  }

  // Step 4.
  double isoDay;
  if (!ToIntegerWithTruncation(cx, args.get(2), "day", &isoDay)) {
    return false;
  }

  // Step 5.
  double hour = 0;
  if (args.hasDefined(3)) {
    if (!ToIntegerWithTruncation(cx, args[3], "hour", &hour)) {
      return false;
    }
  }

  // Step 6.
  double minute = 0;
  if (args.hasDefined(4)) {
    if (!ToIntegerWithTruncation(cx, args[4], "minute", &minute)) {
      return false;
    }
  }

  // Step 7.
  double second = 0;
  if (args.hasDefined(5)) {
    if (!ToIntegerWithTruncation(cx, args[5], "second", &second)) {
      return false;
    }
  }

  // Step 8.
  double millisecond = 0;
  if (args.hasDefined(6)) {
    if (!ToIntegerWithTruncation(cx, args[6], "millisecond", &millisecond)) {
      return false;
    }
  }

  // Step 9.
  double microsecond = 0;
  if (args.hasDefined(7)) {
    if (!ToIntegerWithTruncation(cx, args[7], "microsecond", &microsecond)) {
      return false;
    }
  }

  // Step 10.
  double nanosecond = 0;
  if (args.hasDefined(8)) {
    if (!ToIntegerWithTruncation(cx, args[8], "nanosecond", &nanosecond)) {
      return false;
    }
  }

  // Steps 11-14.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  if (args.hasDefined(9)) {
    // Step 12.
    if (!args[9].isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, args[9],
                       nullptr, "not a string");
      return false;
    }

    // Steps 13-14.
    Rooted<JSString*> calendarString(cx, args[9].toString());
    if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
      return false;
    }
  }

  // Step 15.
  auto* temporalDateTime = CreateTemporalDateTime(
      cx, args, isoYear, isoMonth, isoDay, hour, minute, second, millisecond,
      microsecond, nanosecond, calendar);
  if (!temporalDateTime) {
    return false;
  }

  args.rval().setObject(*temporalDateTime);
  return true;
}

/**
 * Temporal.PlainDateTime.from ( item [ , options ] )
 */
static bool PlainDateTime_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Steps 1-2.
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 1.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "from", args[1]));
    if (!options) {
      return false;
    }

    // Step 2.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Steps 3-4.
  Rooted<PlainDateTimeWithCalendar> dateTime(cx);
  if (!ToTemporalDateTime(cx, args.get(0), overflow, &dateTime)) {
    return false;
  }

  auto* result = CreateTemporalDateTime(cx, dateTime);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainDateTime.compare ( one, two )
 */
static bool PlainDateTime_compare(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<PlainDateTimeWithCalendar> one(cx);
  if (!ToTemporalDateTime(cx, args.get(0), &one)) {
    return false;
  }

  // Step 2.
  Rooted<PlainDateTimeWithCalendar> two(cx);
  if (!ToTemporalDateTime(cx, args.get(1), &two)) {
    return false;
  }

  // Step 3.
  args.rval().setInt32(CompareISODateTime(one, two));
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.calendarId
 */
static bool PlainDateTime_calendarId(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();

  // Step 3.
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());
  auto* calendarId = ToTemporalCalendarIdentifier(cx, calendar);
  if (!calendarId) {
    return false;
  }

  args.rval().setString(calendarId);
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.calendarId
 */
static bool PlainDateTime_calendarId(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_calendarId>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.era
 */
static bool PlainDateTime_era(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarEra(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.era
 */
static bool PlainDateTime_era(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_era>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.eraYear
 */
static bool PlainDateTime_eraYear(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Steps 3-5.
  return CalendarEraYear(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.eraYear
 */
static bool PlainDateTime_eraYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_eraYear>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.year
 */
static bool PlainDateTime_year(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarYear(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.year
 */
static bool PlainDateTime_year(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_year>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.month
 */
static bool PlainDateTime_month(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarMonth(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.month
 */
static bool PlainDateTime_month(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_month>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.monthCode
 */
static bool PlainDateTime_monthCode(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarMonthCode(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.monthCode
 */
static bool PlainDateTime_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_monthCode>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainDateTime.prototype.day
 */
static bool PlainDateTime_day(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarDay(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.day
 */
static bool PlainDateTime_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_day>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.hour
 */
static bool PlainDateTime_hour(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoHour());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.hour
 */
static bool PlainDateTime_hour(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_hour>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.minute
 */
static bool PlainDateTime_minute(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoMinute());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.minute
 */
static bool PlainDateTime_minute(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_minute>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.second
 */
static bool PlainDateTime_second(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoSecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.second
 */
static bool PlainDateTime_second(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_second>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.millisecond
 */
static bool PlainDateTime_millisecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoMillisecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.millisecond
 */
static bool PlainDateTime_millisecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_millisecond>(cx,
                                                                          args);
}

/**
 * get Temporal.PlainDateTime.prototype.microsecond
 */
static bool PlainDateTime_microsecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoMicrosecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.microsecond
 */
static bool PlainDateTime_microsecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_microsecond>(cx,
                                                                          args);
}

/**
 * get Temporal.PlainDateTime.prototype.nanosecond
 */
static bool PlainDateTime_nanosecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoNanosecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.nanosecond
 */
static bool PlainDateTime_nanosecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_nanosecond>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfWeek
 */
static bool PlainDateTime_dayOfWeek(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarDayOfWeek(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfWeek
 */
static bool PlainDateTime_dayOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_dayOfWeek>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfYear
 */
static bool PlainDateTime_dayOfYear(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarDayOfYear(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfYear
 */
static bool PlainDateTime_dayOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_dayOfYear>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainDateTime.prototype.weekOfYear
 */
static bool PlainDateTime_weekOfYear(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Steps 3-5.
  return CalendarWeekOfYear(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.weekOfYear
 */
static bool PlainDateTime_weekOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_weekOfYear>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.yearOfWeek
 */
static bool PlainDateTime_yearOfWeek(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Steps 3-5.
  return CalendarYearOfWeek(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.yearOfWeek
 */
static bool PlainDateTime_yearOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_yearOfWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.daysInWeek
 */
static bool PlainDateTime_daysInWeek(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarDaysInWeek(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.daysInWeek
 */
static bool PlainDateTime_daysInWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_daysInWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.daysInMonth
 */
static bool PlainDateTime_daysInMonth(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarDaysInMonth(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.daysInMonth
 */
static bool PlainDateTime_daysInMonth(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_daysInMonth>(cx,
                                                                          args);
}

/**
 * get Temporal.PlainDateTime.prototype.daysInYear
 */
static bool PlainDateTime_daysInYear(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarDaysInYear(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.daysInYear
 */
static bool PlainDateTime_daysInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_daysInYear>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.monthsInYear
 */
static bool PlainDateTime_monthsInYear(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarMonthsInYear(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.monthsInYear
 */
static bool PlainDateTime_monthsInYear(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_monthsInYear>(
      cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.inLeapYear
 */
static bool PlainDateTime_inLeapYear(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  return CalendarInLeapYear(cx, calendar, ToPlainDate(dateTime), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.inLeapYear
 */
static bool PlainDateTime_inLeapYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_inLeapYear>(cx,
                                                                         args);
}

/**
 * Temporal.PlainDateTime.prototype.with ( temporalDateTimeLike [ , options ] )
 */
static bool PlainDateTime_with(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateTimeObject*> dateTime(
      cx, &args.thisv().toObject().as<PlainDateTimeObject>());

  // Step 3.
  Rooted<JSObject*> temporalDateTimeLike(
      cx, RequireObjectArg(cx, "temporalDateTimeLike", "with", args.get(0)));
  if (!temporalDateTimeLike) {
    return false;
  }
  if (!ThrowIfTemporalLikeObject(cx, temporalDateTimeLike)) {
    return false;
  }

  // Steps 4-5.
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 4.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "with", args[1]));
    if (!options) {
      return false;
    }

    // Step 5.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 6.
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 7.
  Rooted<TemporalFields> fields(cx);
  if (!PrepareCalendarFieldsAndFieldNames(cx, calendar, dateTime,
                                          {
                                              CalendarField::Day,
                                              CalendarField::Month,
                                              CalendarField::MonthCode,
                                              CalendarField::Year,
                                          },
                                          &fields)) {
    return false;
  }

  // Steps 8-14.
  fields.setHour(dateTime->isoHour());
  fields.setMinute(dateTime->isoMinute());
  fields.setSecond(dateTime->isoSecond());
  fields.setMillisecond(dateTime->isoMillisecond());
  fields.setMicrosecond(dateTime->isoMicrosecond());
  fields.setNanosecond(dateTime->isoNanosecond());

  // Step 15.
  Rooted<TemporalFields> partialDateTime(cx);
  if (!PreparePartialTemporalFields(cx, temporalDateTimeLike, fields.keys(),
                                    &partialDateTime)) {
    return false;
  }
  MOZ_ASSERT(!partialDateTime.keys().isEmpty());

  // Step 16.
  Rooted<TemporalFields> mergedFields(
      cx, CalendarMergeFields(calendar, fields, partialDateTime));

  // Step 17.
  if (!PrepareTemporalFields(cx, mergedFields, fields.keys(), &fields)) {
    return false;
  }

  // Step 18.
  PlainDateTime result;
  if (!InterpretTemporalDateTimeFields(cx, calendar, fields, overflow,
                                       &result)) {
    return false;
  }

  // Steps 19-20.
  MOZ_ASSERT(IsValidISODateTime(result));

  // Step 21.
  auto* obj = CreateTemporalDateTime(cx, result, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.with ( temporalDateTimeLike [ , options ] )
 */
static bool PlainDateTime_with(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_with>(cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.withPlainTime ( [ plainTimeLike ] )
 */
static bool PlainDateTime_withPlainTime(JSContext* cx, const CallArgs& args) {
  auto* temporalDateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto date = ToPlainDate(temporalDateTime);
  Rooted<CalendarValue> calendar(cx, temporalDateTime->calendar());

  // Step 3. (Inlined ToTemporalTimeOrMidnight)
  PlainTime time = {};
  if (args.hasDefined(0)) {
    if (!ToTemporalTime(cx, args[0], &time)) {
      return false;
    }
  }

  // Step 4.
  auto* obj = CreateTemporalDateTime(cx, {date, time}, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.withPlainTime ( [ plainTimeLike ] )
 */
static bool PlainDateTime_withPlainTime(JSContext* cx, unsigned argc,
                                        Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_withPlainTime>(
      cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.withCalendar ( calendar )
 */
static bool PlainDateTime_withCalendar(JSContext* cx, const CallArgs& args) {
  auto* temporalDateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto dateTime = ToPlainDateTime(temporalDateTime);

  // Step 3.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendar(cx, args.get(0), &calendar)) {
    return false;
  }

  // Step 4.
  auto* result = CreateTemporalDateTime(cx, dateTime, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.withCalendar ( calendar )
 */
static bool PlainDateTime_withCalendar(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_withCalendar>(
      cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.add ( temporalDurationLike [ , options ] )
 */
static bool PlainDateTime_add(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return AddDurationToOrSubtractDurationFromPlainDateTime(
      cx, PlainDateTimeDuration::Add, args);
}

/**
 * Temporal.PlainDateTime.prototype.add ( temporalDurationLike [ , options ] )
 */
static bool PlainDateTime_add(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_add>(cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.subtract ( temporalDurationLike [ , options
 * ] )
 */
static bool PlainDateTime_subtract(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return AddDurationToOrSubtractDurationFromPlainDateTime(
      cx, PlainDateTimeDuration::Subtract, args);
}

/**
 * Temporal.PlainDateTime.prototype.subtract ( temporalDurationLike [ , options
 * ] )
 */
static bool PlainDateTime_subtract(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_subtract>(cx,
                                                                       args);
}

/**
 * Temporal.PlainDateTime.prototype.until ( other [ , options ] )
 */
static bool PlainDateTime_until(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return DifferenceTemporalPlainDateTime(cx, TemporalDifference::Until, args);
}

/**
 * Temporal.PlainDateTime.prototype.until ( other [ , options ] )
 */
static bool PlainDateTime_until(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_until>(cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.since ( other [ , options ] )
 */
static bool PlainDateTime_since(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return DifferenceTemporalPlainDateTime(cx, TemporalDifference::Since, args);
}

/**
 * Temporal.PlainDateTime.prototype.since ( other [ , options ] )
 */
static bool PlainDateTime_since(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_since>(cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.round ( roundTo )
 */
static bool PlainDateTime_round(JSContext* cx, const CallArgs& args) {
  auto* temporalDateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto dateTime = ToPlainDateTime(temporalDateTime);
  Rooted<CalendarValue> calendar(cx, temporalDateTime->calendar());

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

    MOZ_ASSERT(TemporalUnit::Day <= smallestUnit &&
               smallestUnit <= TemporalUnit::Nanosecond);

    // Steps 6-8 and 10-12. (Implicit)
  } else {
    // Steps 3 and 5.
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
    auto* obj = CreateTemporalDateTime(cx, dateTime, calendar);
    if (!obj) {
      return false;
    }

    args.rval().setObject(*obj);
    return true;
  }

  // Step 14.
  auto result =
      RoundISODateTime(dateTime, roundingIncrement, smallestUnit, roundingMode);

  // Step 15.
  auto* obj = CreateTemporalDateTime(cx, result, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.round ( roundTo )
 */
static bool PlainDateTime_round(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_round>(cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.equals ( other )
 */
static bool PlainDateTime_equals(JSContext* cx, const CallArgs& args) {
  auto* temporalDateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto dateTime = ToPlainDateTime(temporalDateTime);
  Rooted<CalendarValue> calendar(cx, temporalDateTime->calendar());

  // Step 3.
  Rooted<PlainDateTimeWithCalendar> other(cx);
  if (!ToTemporalDateTime(cx, args.get(0), &other)) {
    return false;
  }

  // Steps 4-6.
  bool equals = dateTime == other.dateTime() &&
                CalendarEquals(calendar, other.calendar());

  args.rval().setBoolean(equals);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.equals ( other )
 */
static bool PlainDateTime_equals(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_equals>(cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.toString ( [ options ] )
 */
static bool PlainDateTime_toString(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto dt = ToPlainDateTime(dateTime);
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  SecondsStringPrecision precision = {Precision::Auto(),
                                      TemporalUnit::Nanosecond, Increment{1}};
  auto roundingMode = TemporalRoundingMode::Trunc;
  auto showCalendar = ShowCalendar::Auto;
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
    if (!GetRoundingModeOption(cx, options, &roundingMode)) {
      return false;
    }

    // Step 8.
    auto smallestUnit = TemporalUnit::Auto;
    if (!GetTemporalUnitValuedOption(cx, options, TemporalUnitKey::SmallestUnit,
                                     TemporalUnitGroup::Time, &smallestUnit)) {
      return false;
    }

    // Step 9.
    if (smallestUnit == TemporalUnit::Hour) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INVALID_UNIT_OPTION, "hour",
                                "smallestUnit");
      return false;
    }

    // Step 10.
    precision = ToSecondsStringPrecision(smallestUnit, digits);
  }

  // Step 11.
  auto result =
      RoundISODateTime(dt, precision.increment, precision.unit, roundingMode);

  // Step 12.
  JSString* str = ::TemporalDateTimeToString(cx, result, calendar,
                                             precision.precision, showCalendar);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toString ( [ options ] )
 */
static bool PlainDateTime_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toString>(cx,
                                                                       args);
}

/**
 * Temporal.PlainDateTime.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainDateTime_toLocaleString(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto dt = ToPlainDateTime(dateTime);
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  JSString* str = ::TemporalDateTimeToString(
      cx, dt, calendar, Precision::Auto(), ShowCalendar::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainDateTime_toLocaleString(JSContext* cx, unsigned argc,
                                         Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toLocaleString>(
      cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.toJSON ( )
 */
static bool PlainDateTime_toJSON(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto dt = ToPlainDateTime(dateTime);
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  JSString* str = ::TemporalDateTimeToString(
      cx, dt, calendar, Precision::Auto(), ShowCalendar::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toJSON ( )
 */
static bool PlainDateTime_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toJSON>(cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.valueOf ( )
 */
static bool PlainDateTime_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainDateTime", "primitive type");
  return false;
}

/**
 * Temporal.PlainDateTime.prototype.toZonedDateTime ( temporalTimeZoneLike [ ,
 * options ] )
 */
static bool PlainDateTime_toZonedDateTime(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateTimeObject*> dateTime(
      cx, &args.thisv().toObject().as<PlainDateTimeObject>());
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  Rooted<TimeZoneValue> timeZone(cx);
  if (!ToTemporalTimeZone(cx, args.get(0), &timeZone)) {
    return false;
  }

  auto disambiguation = TemporalDisambiguation::Compatible;
  if (args.hasDefined(1)) {
    // Step 4.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "toZonedDateTime", args[1]));
    if (!options) {
      return false;
    }

    // Step 5.
    if (!GetTemporalDisambiguationOption(cx, options, &disambiguation)) {
      return false;
    }
  }

  // Steps 6-7.
  Instant instant;
  if (!GetInstantFor(cx, timeZone, ToPlainDateTime(dateTime), disambiguation,
                     &instant)) {
    return false;
  }

  // Step 8.
  auto* result = CreateTemporalZonedDateTime(cx, instant, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toZonedDateTime ( temporalTimeZoneLike [ ,
 * options ] )
 */
static bool PlainDateTime_toZonedDateTime(JSContext* cx, unsigned argc,
                                          Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toZonedDateTime>(
      cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.toPlainDate ( )
 */
static bool PlainDateTime_toPlainDate(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 3.
  auto* obj = CreateTemporalDate(cx, ToPlainDate(dateTime), calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toPlainDate ( )
 */
static bool PlainDateTime_toPlainDate(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toPlainDate>(cx,
                                                                          args);
}

/**
 * Temporal.PlainDateTime.prototype.toPlainTime ( )
 */
static bool PlainDateTime_toPlainTime(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();

  // Step 3.
  auto* obj = CreateTemporalTime(cx, ToPlainTime(dateTime));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toPlainTime ( )
 */
static bool PlainDateTime_toPlainTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toPlainTime>(cx,
                                                                          args);
}

const JSClass PlainDateTimeObject::class_ = {
    "Temporal.PlainDateTime",
    JSCLASS_HAS_RESERVED_SLOTS(PlainDateTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainDateTime),
    JS_NULL_CLASS_OPS,
    &PlainDateTimeObject::classSpec_,
};

const JSClass& PlainDateTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainDateTime_methods[] = {
    JS_FN("from", PlainDateTime_from, 1, 0),
    JS_FN("compare", PlainDateTime_compare, 2, 0),
    JS_FS_END,
};

static const JSFunctionSpec PlainDateTime_prototype_methods[] = {
    JS_FN("with", PlainDateTime_with, 1, 0),
    JS_FN("withPlainTime", PlainDateTime_withPlainTime, 0, 0),
    JS_FN("withCalendar", PlainDateTime_withCalendar, 1, 0),
    JS_FN("add", PlainDateTime_add, 1, 0),
    JS_FN("subtract", PlainDateTime_subtract, 1, 0),
    JS_FN("until", PlainDateTime_until, 1, 0),
    JS_FN("since", PlainDateTime_since, 1, 0),
    JS_FN("round", PlainDateTime_round, 1, 0),
    JS_FN("equals", PlainDateTime_equals, 1, 0),
    JS_FN("toString", PlainDateTime_toString, 0, 0),
    JS_FN("toLocaleString", PlainDateTime_toLocaleString, 0, 0),
    JS_FN("toJSON", PlainDateTime_toJSON, 0, 0),
    JS_FN("valueOf", PlainDateTime_valueOf, 0, 0),
    JS_FN("toZonedDateTime", PlainDateTime_toZonedDateTime, 1, 0),
    JS_FN("toPlainDate", PlainDateTime_toPlainDate, 0, 0),
    JS_FN("toPlainTime", PlainDateTime_toPlainTime, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainDateTime_prototype_properties[] = {
    JS_PSG("calendarId", PlainDateTime_calendarId, 0),
    JS_PSG("era", PlainDateTime_era, 0),
    JS_PSG("eraYear", PlainDateTime_eraYear, 0),
    JS_PSG("year", PlainDateTime_year, 0),
    JS_PSG("month", PlainDateTime_month, 0),
    JS_PSG("monthCode", PlainDateTime_monthCode, 0),
    JS_PSG("day", PlainDateTime_day, 0),
    JS_PSG("hour", PlainDateTime_hour, 0),
    JS_PSG("minute", PlainDateTime_minute, 0),
    JS_PSG("second", PlainDateTime_second, 0),
    JS_PSG("millisecond", PlainDateTime_millisecond, 0),
    JS_PSG("microsecond", PlainDateTime_microsecond, 0),
    JS_PSG("nanosecond", PlainDateTime_nanosecond, 0),
    JS_PSG("dayOfWeek", PlainDateTime_dayOfWeek, 0),
    JS_PSG("dayOfYear", PlainDateTime_dayOfYear, 0),
    JS_PSG("weekOfYear", PlainDateTime_weekOfYear, 0),
    JS_PSG("yearOfWeek", PlainDateTime_yearOfWeek, 0),
    JS_PSG("daysInWeek", PlainDateTime_daysInWeek, 0),
    JS_PSG("daysInMonth", PlainDateTime_daysInMonth, 0),
    JS_PSG("daysInYear", PlainDateTime_daysInYear, 0),
    JS_PSG("monthsInYear", PlainDateTime_monthsInYear, 0),
    JS_PSG("inLeapYear", PlainDateTime_inLeapYear, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.PlainDateTime", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec PlainDateTimeObject::classSpec_ = {
    GenericCreateConstructor<PlainDateTimeConstructor, 3,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainDateTimeObject>,
    PlainDateTime_methods,
    nullptr,
    PlainDateTime_prototype_methods,
    PlainDateTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
