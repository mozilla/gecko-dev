/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainDate.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <stdint.h>
#include <type_traits>
#include <utility>

#include "jsdate.h"
#include "jsnum.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/CalendarFields.h"
#include "builtin/temporal/Duration.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
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
#include "js/Date.h"
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
#include "vm/PlainObject.h"
#include "vm/PropertyInfo.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsPlainDate(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainDateObject>();
}

#ifdef DEBUG
/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::IsValidISODate(const PlainDate& date) {
  const auto& [year, month, day] = date;

  // Step 1.
  if (month < 1 || month > 12) {
    return false;
  }

  // Step 2.
  int32_t daysInMonth = js::temporal::ISODaysInMonth(year, month);

  // Steps 3-4.
  return 1 <= day && day <= daysInMonth;
}
#endif

/**
 * ISODateWithinLimits ( isoDate )
 */
bool js::temporal::ISODateWithinLimits(const PlainDate& isoDate) {
  MOZ_ASSERT(IsValidISODate(isoDate));

  const auto& [year, month, day] = isoDate;

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

static void ReportInvalidDateValue(JSContext* cx, const char* name, int32_t min,
                                   int32_t max, double num) {
  Int32ToCStringBuf minCbuf;
  const char* minStr = Int32ToCString(&minCbuf, min);

  Int32ToCStringBuf maxCbuf;
  const char* maxStr = Int32ToCString(&maxCbuf, max);

  ToCStringBuf numCbuf;
  const char* numStr = NumberToCString(&numCbuf, num);

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_PLAIN_DATE_INVALID_VALUE, name,
                            minStr, maxStr, numStr);
}

template <typename T>
static inline bool ThrowIfInvalidDateValue(JSContext* cx, const char* name,
                                           int32_t min, int32_t max, T num) {
  if (min <= num && num <= max) {
    return true;
  }
  ReportInvalidDateValue(cx, name, min, max, num);
  return false;
}

/**
 * IsValidISODate ( year, month, day )
 */
template <typename T>
static bool ThrowIfInvalidISODate(JSContext* cx, T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));

  if constexpr (std::is_same_v<T, double>) {
    if (!ThrowIfInvalidDateValue(cx, "year", INT32_MIN, INT32_MAX, year)) {
      return false;
    }
  }

  // Step 2.
  if (!ThrowIfInvalidDateValue(cx, "month", 1, 12, month)) {
    return false;
  }

  // Step 3.
  int32_t daysInMonth =
      js::temporal::ISODaysInMonth(int32_t(year), int32_t(month));

  // Steps 4-5.
  return ThrowIfInvalidDateValue(cx, "day", 1, daysInMonth, day);
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::ThrowIfInvalidISODate(JSContext* cx, const PlainDate& date) {
  const auto& [year, month, day] = date;
  return ::ThrowIfInvalidISODate(cx, year, month, day);
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::ThrowIfInvalidISODate(JSContext* cx, double year,
                                         double month, double day) {
  return ::ThrowIfInvalidISODate(cx, year, month, day);
}

/**
 * RegulateISODate ( year, month, day, overflow )
 *
 * With |overflow = "constrain"|.
 */
static PlainDate ConstrainISODate(const PlainDate& date) {
  const auto& [year, month, day] = date;

  // Step 1.a.
  int32_t m = std::clamp(month, 1, 12);

  // Step 1.b.
  int32_t daysInMonth = temporal::ISODaysInMonth(year, m);

  // Step 1.c.
  int32_t d = std::clamp(day, 1, daysInMonth);

  // Step 1.d.
  return {year, m, d};
}

/**
 * RegulateISODate ( year, month, day, overflow )
 */
static bool RegulateISODate(JSContext* cx, const PlainDate& date,
                            TemporalOverflow overflow, PlainDate* result) {
  // Step 1.
  if (overflow == TemporalOverflow::Constrain) {
    *result = ::ConstrainISODate(date);
    return true;
  }

  // Step 2.a.
  MOZ_ASSERT(overflow == TemporalOverflow::Reject);

  // Step 2.b.
  if (!ThrowIfInvalidISODate(cx, date)) {
    return false;
  }

  // Step 2.b. (Inlined call to CreateISODateRecord.)
  *result = date;
  return true;
}

/**
 * RegulateISODate ( year, month, day, overflow )
 */
bool js::temporal::RegulateISODate(JSContext* cx, int32_t year, double month,
                                   double day, TemporalOverflow overflow,
                                   PlainDate* result) {
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));

  // Step 1.
  if (overflow == TemporalOverflow::Constrain) {
    // Step 1.a.
    int32_t m = int32_t(std::clamp(month, 1.0, 12.0));

    // Step 1.b.
    double daysInMonth = double(ISODaysInMonth(year, m));

    // Step 1.c.
    int32_t d = int32_t(std::clamp(day, 1.0, daysInMonth));

    // Step 1.d.
    *result = {year, m, d};
    return true;
  }

  // Step 2.a.
  MOZ_ASSERT(overflow == TemporalOverflow::Reject);

  // Step 2.b.
  if (!ThrowIfInvalidISODate(cx, year, month, day)) {
    return false;
  }

  // Step 2.b. (Inlined call to CreateISODateRecord.)
  *result = {year, int32_t(month), int32_t(day)};
  return true;
}

/**
 * CreateTemporalDate ( isoDate, calendar [ , newTarget ] )
 */
static PlainDateObject* CreateTemporalDate(JSContext* cx, const CallArgs& args,
                                           const PlainDate& isoDate,
                                           Handle<CalendarValue> calendar) {
  MOZ_ASSERT(IsValidISODate(isoDate));

  // Step 1.
  if (!ISODateWithinLimits(isoDate)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return nullptr;
  }

  // Steps 2-3.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainDate,
                                          &proto)) {
    return nullptr;
  }

  auto* object = NewObjectWithClassProto<PlainDateObject>(cx, proto);
  if (!object) {
    return nullptr;
  }

  // Step 4.
  auto packedDate = PackedDate::pack(isoDate);
  object->setFixedSlot(PlainDateObject::PACKED_DATE_SLOT,
                       PrivateUint32Value(packedDate.value));

  // Step 5.
  object->setFixedSlot(PlainDateObject::CALENDAR_SLOT, calendar.toSlotValue());

  // Step 6.
  return object;
}

/**
 * CreateTemporalDate ( isoDate, calendar [ , newTarget ] )
 */
PlainDateObject* js::temporal::CreateTemporalDate(
    JSContext* cx, const PlainDate& isoDate, Handle<CalendarValue> calendar) {
  MOZ_ASSERT(IsValidISODate(isoDate));

  // Step 1.
  if (!ISODateWithinLimits(isoDate)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return nullptr;
  }

  // Steps 2-3.
  auto* object = NewBuiltinClassInstance<PlainDateObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 4.
  auto packedDate = PackedDate::pack(isoDate);
  object->setFixedSlot(PlainDateObject::PACKED_DATE_SLOT,
                       PrivateUint32Value(packedDate.value));

  // Step 5.
  object->setFixedSlot(PlainDateObject::CALENDAR_SLOT, calendar.toSlotValue());

  // Step 6.
  return object;
}

/**
 * CreateTemporalDate ( isoDate, calendar [ , newTarget ] )
 */
PlainDateObject* js::temporal::CreateTemporalDate(
    JSContext* cx, Handle<PlainDateWithCalendar> date) {
  MOZ_ASSERT(ISODateWithinLimits(date));
  return CreateTemporalDate(cx, date, date.calendar());
}

/**
 * CreateTemporalDate ( isoDate, calendar [ , newTarget ] )
 */
bool js::temporal::CreateTemporalDate(
    JSContext* cx, const PlainDate& isoDate, Handle<CalendarValue> calendar,
    MutableHandle<PlainDateWithCalendar> result) {
  MOZ_ASSERT(IsValidISODate(isoDate));

  // Step 1.
  if (!ISODateWithinLimits(isoDate)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return false;
  }

  // Steps 2-6.
  result.set(PlainDateWithCalendar{isoDate, calendar});
  return true;
}

struct DateOptions {
  TemporalOverflow overflow = TemporalOverflow::Constrain;
};

/**
 * ToTemporalDate ( item [ , options ] )
 */
static bool ToTemporalDateOptions(JSContext* cx, Handle<Value> options,
                                  DateOptions* result) {
  if (options.isUndefined()) {
    *result = {};
    return true;
  }

  // NOTE: |options| are only passed from `Temporal.PlainDate.from`.

  Rooted<JSObject*> resolvedOptions(
      cx, RequireObjectArg(cx, "options", "from", options));
  if (!resolvedOptions) {
    return false;
  }

  auto overflow = TemporalOverflow::Constrain;
  if (!GetTemporalOverflowOption(cx, resolvedOptions, &overflow)) {
    return false;
  }

  *result = {overflow};
  return true;
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
static bool ToTemporalDate(JSContext* cx, Handle<JSObject*> item,
                           Handle<Value> options,
                           MutableHandle<PlainDateWithCalendar> result) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.a.
  if (auto* plainDate = item->maybeUnwrapIf<PlainDateObject>()) {
    auto date = plainDate->date();
    Rooted<CalendarValue> calendar(cx, plainDate->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Steps 2.a.i-ii.
    DateOptions ignoredOptions;
    if (!ToTemporalDateOptions(cx, options, &ignoredOptions)) {
      return false;
    }

    // Step 2.a.iii.
    result.set(PlainDateWithCalendar{date, calendar});
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

    // Steps 2.b.ii.
    PlainDateTime dateTime;
    if (!GetISODateTimeFor(cx, timeZone, epochInstant, &dateTime)) {
      return false;
    }

    // Steps 2.b.ii-iii.
    DateOptions ignoredOptions;
    if (!ToTemporalDateOptions(cx, options, &ignoredOptions)) {
      return false;
    }

    // Step 2.b.iv.
    result.set(PlainDateWithCalendar{dateTime.date, calendar});
    return true;
  }

  // Step 2.c.
  if (auto* dateTime = item->maybeUnwrapIf<PlainDateTimeObject>()) {
    auto date = dateTime->date();
    Rooted<CalendarValue> calendar(cx, dateTime->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Steps 2.c.i-ii.
    DateOptions ignoredOptions;
    if (!ToTemporalDateOptions(cx, options, &ignoredOptions)) {
      return false;
    }

    // Step 2.c.iii.
    result.set(PlainDateWithCalendar{date, calendar});
    return true;
  }

  // Step 2.d.
  Rooted<CalendarValue> calendar(cx);
  if (!GetTemporalCalendarWithISODefault(cx, item, &calendar)) {
    return false;
  }

  // Step 2.e.
  Rooted<CalendarFields> fields(cx);
  if (!PrepareCalendarFields(cx, calendar, item,
                             {
                                 CalendarField::Year,
                                 CalendarField::Month,
                                 CalendarField::MonthCode,
                                 CalendarField::Day,
                             },
                             &fields)) {
    return false;
  }

  // Steps 2.f-g.
  DateOptions resolvedOptions;
  if (!ToTemporalDateOptions(cx, options, &resolvedOptions)) {
    return false;
  }
  auto [overflow] = resolvedOptions;

  // Step 2.h.
  return CalendarDateFromFields(cx, calendar, fields, overflow, result);
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
static bool ToTemporalDate(JSContext* cx, Handle<Value> item,
                           Handle<Value> options,
                           MutableHandle<PlainDateWithCalendar> result) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  if (item.isObject()) {
    Rooted<JSObject*> itemObj(cx, &item.toObject());
    return ToTemporalDate(cx, itemObj, options, result);
  }

  // Step 3.
  if (!item.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, item,
                     nullptr, "not a string");
    return false;
  }
  Rooted<JSString*> string(cx, item.toString());

  // Step 4.
  PlainDateTime dateTime;
  Rooted<JSString*> calendarString(cx);
  if (!ParseTemporalDateTimeString(cx, string, &dateTime, &calendarString)) {
    return false;
  }
  MOZ_ASSERT(IsValidISODate(dateTime.date));

  // Steps 5-7.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  if (calendarString) {
    if (!CanonicalizeCalendar(cx, calendarString, &calendar)) {
      return false;
    }
  }

  // Steps 8-9.
  DateOptions ignoredOptions;
  if (!ToTemporalDateOptions(cx, options, &ignoredOptions)) {
    return false;
  }

  // Step 10.
  return CreateTemporalDate(cx, dateTime.date, calendar, result);
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
static bool ToTemporalDate(JSContext* cx, Handle<Value> item,
                           MutableHandle<PlainDateWithCalendar> result) {
  return ToTemporalDate(cx, item, UndefinedHandleValue, result);
}

/**
 * Mathematical Operations, "modulo" notation.
 */
static int32_t NonNegativeModulo(int64_t x, int32_t y) {
  MOZ_ASSERT(y > 0);

  int32_t result = mozilla::AssertedCast<int32_t>(x % y);
  return (result < 0) ? (result + y) : result;
}

struct BalancedYearMonth final {
  int64_t year = 0;
  int32_t month = 0;
};

/**
 * BalanceISOYearMonth ( year, month )
 */
static BalancedYearMonth BalanceISOYearMonth(int64_t year, int64_t month) {
  MOZ_ASSERT(std::abs(year) < (int64_t(1) << 33),
             "year is the addition of plain-date year with duration years");
  MOZ_ASSERT(std::abs(month) < (int64_t(1) << 33),
             "month is the addition of plain-date month with duration months");

  // Step 1. (Not applicable in our implementation.)

  // Note: If either abs(year) or abs(month) is greater than 2^53 (the double
  // integral precision limit), the additions resp. subtractions below are
  // imprecise. This doesn't matter for us, because the single caller to this
  // function (AddISODate) will throw an error for large values anyway.

  // Step 2.
  int64_t balancedYear = year + temporal::FloorDiv(month - 1, 12);

  // Step 3.
  int32_t balancedMonth = NonNegativeModulo(month - 1, 12) + 1;
  MOZ_ASSERT(1 <= balancedMonth && balancedMonth <= 12);

  // Step 4.
  return {balancedYear, balancedMonth};
}

static bool IsValidPlainDateEpochMilliseconds(int64_t epochMilliseconds) {
  // Epoch nanoseconds limits, adjusted to the range supported by PlainDate.
  constexpr auto oneDay =
      InstantSpan::fromSeconds(ToSeconds(TemporalUnit::Day));
  constexpr auto min = Instant::min() - oneDay;
  constexpr auto max = Instant::max() + oneDay;

  // NB: Minimum limit is inclusive, whereas maximim limit is exclusive.
  auto instant = Instant::fromMilliseconds(epochMilliseconds);
  return min <= instant && instant < max;
}

/**
 * BalanceISODate ( year, month, day )
 */
bool js::temporal::BalanceISODate(JSContext* cx, const PlainDate& date,
                                  int64_t days, PlainDate* result) {
  MOZ_ASSERT(IsValidISODate(date));
  MOZ_ASSERT(ISODateWithinLimits(date));

  // Step 1.
  auto epochDays = MakeDay(date) + mozilla::CheckedInt64{days};

  // Step 2.
  auto epochMilliseconds = epochDays * ToMilliseconds(TemporalUnit::Day);
  if (!epochMilliseconds.isValid() ||
      !IsValidPlainDateEpochMilliseconds(epochMilliseconds.value())) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return false;
  }

  // Step 3.
  auto [year, month, day] = ToYearMonthDay(epochMilliseconds.value());

  *result = PlainDate{year, month + 1, day};
  MOZ_ASSERT(IsValidISODate(*result));
  MOZ_ASSERT(ISODateWithinLimits(*result));

  return true;
}

/**
 * BalanceISODate ( year, month, day )
 */
PlainDate js::temporal::BalanceISODate(const PlainDate& date, int32_t days) {
  MOZ_ASSERT(IsValidISODate(date));
  MOZ_ASSERT(ISODateWithinLimits(date));
  MOZ_ASSERT(std::abs(days) <= 400'000'000, "days limit for ToYearMonthDay");

  // Step 1.
  int32_t epochDays = MakeDay(date) + days;

  // Step 2.
  int64_t epochMilliseconds = epochDays * ToMilliseconds(TemporalUnit::Day);

  // Step 3.
  auto [year, month, day] = ToYearMonthDay(epochMilliseconds);

  // NB: The returned date is possibly outside the valid limits!
  auto result = PlainDate{year, month + 1, day};
  MOZ_ASSERT(IsValidISODate(result));

  return result;
}

static bool CanBalanceISOYear(int64_t year) {
  // TODO: Export these values somewhere.
  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // If the year is below resp. above the min-/max-year, no value of |day| will
  // make the resulting date valid.
  return minYear <= year && year <= maxYear;
}

/**
 * AddISODate ( year, month, day, years, months, weeks, days, overflow )
 */
bool js::temporal::AddISODate(JSContext* cx, const PlainDate& date,
                              const DateDuration& duration,
                              TemporalOverflow overflow, PlainDate* result) {
  MOZ_ASSERT(IsValidISODate(date));
  MOZ_ASSERT(ISODateWithinLimits(date));
  MOZ_ASSERT(IsValidDuration(duration));

  // Steps 1-2. (Not applicable in our implementation.)

  // Step 3.
  auto yearMonth = BalanceISOYearMonth(date.year + duration.years,
                                       date.month + duration.months);
  MOZ_ASSERT(1 <= yearMonth.month && yearMonth.month <= 12);

  // FIXME: spec issue?
  // new Temporal.PlainDate(2021, 5, 31).subtract({months:1, days:1}).toString()
  // returns "2021-04-29", but "2021-04-30" seems more likely expected.
  // Note: "2021-04-29" agrees with java.time, though.
  //
  // Example where this creates inconsistent results:
  //
  // clang-format off
  //
  // js> Temporal.PlainDate.from("2021-05-31").since("2021-04-30", {largestUnit:"months"}).toString()
  // "P1M1D"
  // js> Temporal.PlainDate.from("2021-05-31").subtract("P1M1D").toString()
  // "2021-04-29"
  //
  // clang-format on
  //
  // Later: This now returns "P1M" instead "P1M1D", so the results are at least
  // consistent. Let's add a test case for this behaviour.
  //
  // Revisit when <https://github.com/tc39/proposal-temporal/issues/2535> has
  // been addressed.

  // |yearMonth.year| can only exceed the valid years range when called from
  // `Temporal.Calendar.prototype.dateAdd`. And because `dateAdd` uses the
  // result of AddISODate to create a new Temporal.PlainDate, we can directly
  // throw an error if the result isn't within the valid date-time limits. This
  // in turn allows to work on integer values and we don't have to worry about
  // imprecise double value computations.
  if (!CanBalanceISOYear(yearMonth.year)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return false;
  }

  // Step 4.
  PlainDate regulated;
  if (!RegulateISODate(cx, {int32_t(yearMonth.year), yearMonth.month, date.day},
                       overflow, &regulated)) {
    return false;
  }
  if (!ISODateWithinLimits(regulated)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return false;
  }

  // Steps 5-6.
  int64_t days = duration.days + duration.weeks * 7;

  // Step 7.
  PlainDate balanced;
  if (!BalanceISODate(cx, regulated, days, &balanced)) {
    return false;
  }
  MOZ_ASSERT(IsValidISODate(balanced));

  *result = balanced;
  return true;
}

struct YearMonthDuration {
  int32_t years = 0;
  int32_t months = 0;
};

/**
 * AddISODate ( year, month, day, years, months, weeks, days, overflow )
 *
 * With |overflow = "constrain"|.
 */
static PlainDate AddISODate(const PlainDate& date,
                            const YearMonthDuration& duration) {
  MOZ_ASSERT(IsValidISODate(date));
  MOZ_ASSERT(ISODateWithinLimits(date));

  MOZ_ASSERT_IF(duration.years < 0, duration.months <= 0);
  MOZ_ASSERT_IF(duration.years > 0, duration.months >= 0);

  // TODO: Export these values somewhere.
  [[maybe_unused]] constexpr int32_t minYear = -271821;
  [[maybe_unused]] constexpr int32_t maxYear = 275760;

  MOZ_ASSERT(std::abs(duration.years) <= (maxYear - minYear),
             "years doesn't exceed the maximum duration between valid years");
  MOZ_ASSERT(std::abs(duration.months) <= 12,
             "months duration is at most one year");

  // Steps 1-2. (Not applicable)

  // Step 3. (Inlined BalanceISOYearMonth)
  int32_t year = date.year + duration.years;
  int32_t month = date.month + duration.months;
  MOZ_ASSERT(-11 <= month && month <= 24);

  if (month > 12) {
    month -= 12;
    year += 1;
  } else if (month <= 0) {
    month += 12;
    year -= 1;
  }

  MOZ_ASSERT(1 <= month && month <= 12);
  MOZ_ASSERT(CanBalanceISOYear(year));

  // Steps 4-7.
  return ::ConstrainISODate({year, month, date.day});
}

/**
 * CompareISODate ( y1, m1, d1, y2, m2, d2 )
 */
int32_t js::temporal::CompareISODate(const PlainDate& one,
                                     const PlainDate& two) {
  // Steps 1-2.
  if (one.year != two.year) {
    return one.year < two.year ? -1 : 1;
  }

  // Steps 3-4.
  if (one.month != two.month) {
    return one.month < two.month ? -1 : 1;
  }

  // Steps 5-6.
  if (one.day != two.day) {
    return one.day < two.day ? -1 : 1;
  }

  // Step 7.
  return 0;
}

/**
 * CreateDateDurationRecord ( years, months, weeks, days )
 */
static DateDuration CreateDateDurationRecord(int32_t years, int32_t months,
                                             int32_t weeks, int32_t days) {
  MOZ_ASSERT(IsValidDuration(
      Duration{double(years), double(months), double(weeks), double(days)}));
  return {years, months, weeks, days};
}

/**
 * DifferenceISODate ( y1, m1, d1, y2, m2, d2, largestUnit )
 */
DateDuration js::temporal::DifferenceISODate(const PlainDate& start,
                                             const PlainDate& end,
                                             TemporalUnit largestUnit) {
  // Steps 1-2.
  MOZ_ASSERT(IsValidISODate(start));
  MOZ_ASSERT(IsValidISODate(end));

  // Both inputs are also within the date limits.
  MOZ_ASSERT(ISODateWithinLimits(start));
  MOZ_ASSERT(ISODateWithinLimits(end));

  // Because both inputs are valid dates, we don't need to worry about integer
  // overflow in any of the computations below.

  MOZ_ASSERT(TemporalUnit::Year <= largestUnit &&
             largestUnit <= TemporalUnit::Day);

  // Step 3.
  if (largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month) {
    // Step 3.a.
    int32_t sign = -CompareISODate(start, end);

    // Step 3.b.
    if (sign == 0) {
      return CreateDateDurationRecord(0, 0, 0, 0);
    }

    // FIXME: spec issue - results can be ambiguous, is this intentional?
    // https://github.com/tc39/proposal-temporal/issues/2535
    //
    // clang-format off
    // js> var end = new Temporal.PlainDate(1970, 2, 28)
    // js> var start = new Temporal.PlainDate(1970, 1, 28)
    // js> start.calendar.dateUntil(start, end, {largestUnit:"months"}).toString()
    // "P1M"
    // js> var start = new Temporal.PlainDate(1970, 1, 29)
    // js> start.calendar.dateUntil(start, end, {largestUnit:"months"}).toString()
    // "P1M"
    // js> var start = new Temporal.PlainDate(1970, 1, 30)
    // js> start.calendar.dateUntil(start, end, {largestUnit:"months"}).toString()
    // "P1M"
    // js> var start = new Temporal.PlainDate(1970, 1, 31)
    // js> start.calendar.dateUntil(start, end, {largestUnit:"months"}).toString()
    // "P1M"
    //
    // Compare to java.time.temporal
    //
    // jshell> import java.time.LocalDate
    // jshell> var end = LocalDate.of(1970, 2, 28)
    // end ==> 1970-02-28
    // jshell> var start = LocalDate.of(1970, 1, 28)
    // start ==> 1970-01-28
    // jshell> start.until(end)
    // $27 ==> P1M
    // jshell> var start = LocalDate.of(1970, 1, 29)
    // start ==> 1970-01-29
    // jshell> start.until(end)
    // $29 ==> P30D
    // jshell> var start = LocalDate.of(1970, 1, 30)
    // start ==> 1970-01-30
    // jshell> start.until(end)
    // $31 ==> P29D
    // jshell> var start = LocalDate.of(1970, 1, 31)
    // start ==> 1970-01-31
    // jshell> start.until(end)
    // $33 ==> P28D
    //
    // Also compare to:
    //
    // js> var end = new Temporal.PlainDate(1970, 2, 27)
    // js> var start = new Temporal.PlainDate(1970, 1, 27)
    // js> start.calendar.dateUntil(start, end, {largestUnit:"months"}).toString()
    // "P1M"
    // js> var start = new Temporal.PlainDate(1970, 1, 28)
    // js> start.calendar.dateUntil(start, end, {largestUnit:"months"}).toString()
    // "P30D"
    // js> var start = new Temporal.PlainDate(1970, 1, 29)
    // js> start.calendar.dateUntil(start, end, {largestUnit:"months"}).toString()
    // "P29D"
    //
    // clang-format on

    // Steps 3.c-d. (Not applicable in our implementation.)

    // FIXME: spec issue - consistently use either |end.[[Year]]| or |y2|.

    // Step 3.e.
    int32_t years = end.year - start.year;

    // TODO: We could inline this, because the AddISODate call is just a more
    // complicated way to perform:
    // mid = ConstrainISODate(end.year, start.month, start.day)
    //
    // The remaining computations can probably simplified similarily.

    // Step 3.f.
    auto mid = ::AddISODate(start, {years, 0});

    // Step 3.g.
    int32_t midSign = -CompareISODate(mid, end);

    // Step 3.h.
    if (midSign == 0) {
      // Step 3.h.i.
      if (largestUnit == TemporalUnit::Year) {
        return CreateDateDurationRecord(years, 0, 0, 0);
      }

      // Step 3.h.ii.
      return CreateDateDurationRecord(0, years * 12, 0, 0);
    }

    // Step 3.i.
    int32_t months = end.month - start.month;

    // Step 3.j.
    if (midSign != sign) {
      // Step 3.j.i.
      years -= sign;

      // Step 3.j.ii.
      months += sign * 12;
    }

    // Step 3.k.
    mid = ::AddISODate(start, {years, months});

    // Step 3.l.
    midSign = -CompareISODate(mid, end);

    // Step 3.m.
    if (midSign == 0) {
      // Step 3.m.i.
      if (largestUnit == TemporalUnit::Year) {
        return CreateDateDurationRecord(years, months, 0, 0);
      }

      // Step 3.m.ii.
      return CreateDateDurationRecord(0, months + years * 12, 0, 0);
    }

    // Step 3.n.
    if (midSign != sign) {
      // Step 3.n.i.
      months -= sign;

      // Step 3.n.ii.
      mid = ::AddISODate(start, {years, months});
    }

    // Steps 3.o-q.
    int32_t days;
    if (mid.month == end.month) {
      MOZ_ASSERT(mid.year == end.year);

      days = end.day - mid.day;
    } else if (sign < 0) {
      days = -mid.day - (ISODaysInMonth(end.year, end.month) - end.day);
    } else {
      days = end.day + (ISODaysInMonth(mid.year, mid.month) - mid.day);
    }

    // Step 3.r.
    if (largestUnit == TemporalUnit::Month) {
      // Step 3.r.i.
      months += years * 12;

      // Step 3.r.ii.
      years = 0;
    }

    // Step 3.s.
    return CreateDateDurationRecord(years, months, 0, days);
  }

  // Step 4.a.
  MOZ_ASSERT(largestUnit == TemporalUnit::Week ||
             largestUnit == TemporalUnit::Day);

  // Step 4.b.
  int32_t epochDaysStart = MakeDay(start);

  // Step 4.c.
  int32_t epochDaysEnd = MakeDay(end);

  // Step 4.d.
  int32_t days = epochDaysEnd - epochDaysStart;

  // Step 4.e.
  int32_t weeks = 0;

  // Step 4.f.
  if (largestUnit == TemporalUnit::Week) {
    // Step 4.f.i
    weeks = days / 7;

    // Step 4.f.ii.
    days = days % 7;
  }

  // Step 4.g.
  return CreateDateDurationRecord(0, 0, weeks, days);
}

/**
 * DifferenceTemporalPlainDate ( operation, temporalDate, other, options )
 */
static bool DifferenceTemporalPlainDate(JSContext* cx,
                                        TemporalDifference operation,
                                        const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 1. (Not applicable in our implementation)

  // Step 2.
  Rooted<PlainDateWithCalendar> other(cx);
  if (!ToTemporalDate(cx, args.get(0), &other)) {
    return false;
  }

  // Step 3.
  if (!CalendarEquals(calendar, other.calendar())) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE,
        ToTemporalCalendarIdentifier(calendar).data(),
        ToTemporalCalendarIdentifier(other.calendar()).data());
    return false;
  }

  // Steps 4-5.
  DifferenceSettings settings;
  if (args.hasDefined(1)) {
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", ToName(operation), args[1]));
    if (!options) {
      return false;
    }

    // Step 5.
    if (!GetDifferenceSettings(cx, operation, options, TemporalUnitGroup::Date,
                               TemporalUnit::Day, TemporalUnit::Day,
                               &settings)) {
      return false;
    }
  } else {
    // Steps 4-5.
    settings = {
        TemporalUnit::Day,
        TemporalUnit::Day,
        TemporalRoundingMode::Trunc,
        Increment{1},
    };
  }

  // Step 6.
  if (temporalDate->date() == other.date()) {
    auto* obj = CreateTemporalDuration(cx, {});
    if (!obj) {
      return false;
    }

    args.rval().setObject(*obj);
    return true;
  }

  // Step 7.
  DateDuration difference;
  if (!CalendarDateUntil(cx, calendar, temporalDate->date(), other.date(),
                         settings.largestUnit, &difference)) {
    return false;
  }

  // Step 10. (Moved below)

  // Step 11.
  bool roundingGranularityIsNoop = settings.smallestUnit == TemporalUnit::Day &&
                                   settings.roundingIncrement == Increment{1};

  // Step 12.
  if (!roundingGranularityIsNoop) {
    // Step 10. (Reordered)
    auto duration = NormalizedDuration{difference, {}};

    // Step 12.a.
    auto otherDateTime = PlainDateTime{other.date(), {}};
    auto destEpochNs = GetUTCEpochNanoseconds(otherDateTime);

    // Step 12.b.
    auto dateTime = PlainDateTime{temporalDate->date(), {}};

    // Step 12.c.
    Rooted<TimeZoneValue> timeZone(cx, TimeZoneValue{});
    RoundedRelativeDuration relative;
    if (!RoundRelativeDuration(
            cx, duration, destEpochNs, dateTime, timeZone, calendar,
            settings.largestUnit, settings.roundingIncrement,
            settings.smallestUnit, settings.roundingMode, &relative)) {
      return false;
    }
    MOZ_ASSERT(IsValidDuration(relative.duration));

    difference = relative.duration.toDateDuration();
  }

  // Step 12.
  auto duration = difference.toDuration();
  if (operation == TemporalDifference::Since) {
    duration = duration.negate();
  }
  MOZ_ASSERT(IsValidDuration(duration));

  auto* obj = CreateTemporalDuration(cx, duration);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * AddDurationToDate ( operation, temporalDate, temporalDurationLike, options )
 */
static bool AddDurationToDate(JSContext* cx, TemporalAddDuration operation,
                              const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();

  // Step 1.
  auto date = temporalDate->date();

  // Step 2.
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  Duration duration;
  if (!ToTemporalDuration(cx, args.get(0), &duration)) {
    return false;
  }

  // Step 4.
  if (operation == TemporalAddDuration::Subtract) {
    duration = duration.negate();
  }

  // Step 5.
  auto dateDuration = NormalizeDurationWithoutTime(duration);

  // Steps 6-7.
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 6.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", ToName(operation), args[1]));
    if (!options) {
      return false;
    }

    // Step 7.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 8.
  PlainDate result;
  if (!CalendarDateAdd(cx, calendar, date, dateDuration, overflow, &result)) {
    return false;
  }

  // Step 9.
  auto* obj = CreateTemporalDate(cx, result, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate ( isoYear, isoMonth, isoDay [ , calendarLike ] )
 */
static bool PlainDateConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainDate")) {
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

  // Steps 5-7.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  if (args.hasDefined(3)) {
    // Step 6.
    if (!args[3].isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, args[3],
                       nullptr, "not a string");
      return false;
    }

    // Step 7.
    Rooted<JSString*> calendarString(cx, args[3].toString());
    if (!CanonicalizeCalendar(cx, calendarString, &calendar)) {
      return false;
    }
  }

  // Step 8.
  if (!ThrowIfInvalidISODate(cx, isoYear, isoMonth, isoDay)) {
    return false;
  }

  // Step 9.
  auto isoDate =
      PlainDate{int32_t(isoYear), int32_t(isoMonth), int32_t(isoDay)};

  // Step 10.
  auto* temporalDate = CreateTemporalDate(cx, args, isoDate, calendar);
  if (!temporalDate) {
    return false;
  }

  args.rval().setObject(*temporalDate);
  return true;
}

/**
 * Temporal.PlainDate.from ( item [ , options ] )
 */
static bool PlainDate_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<PlainDateWithCalendar> date(cx);
  if (!ToTemporalDate(cx, args.get(0), args.get(1), &date)) {
    return false;
  }

  auto* result = CreateTemporalDate(cx, date);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainDate.compare ( one, two )
 */
static bool PlainDate_compare(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<PlainDateWithCalendar> one(cx);
  if (!ToTemporalDate(cx, args.get(0), &one)) {
    return false;
  }

  // Step 2.
  Rooted<PlainDateWithCalendar> two(cx);
  if (!ToTemporalDate(cx, args.get(1), &two)) {
    return false;
  }

  // Step 3.
  args.rval().setInt32(CompareISODate(one, two));
  return true;
}

/**
 * get Temporal.PlainDate.prototype.calendarId
 */
static bool PlainDate_calendarId(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  auto* calendarId = ToTemporalCalendarIdentifier(cx, calendar);
  if (!calendarId) {
    return false;
  }

  args.rval().setString(calendarId);
  return true;
}

/**
 * get Temporal.PlainDate.prototype.calendarId
 */
static bool PlainDate_calendarId(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_calendarId>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.era
 */
static bool PlainDate_era(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarEra(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.era
 */
static bool PlainDate_era(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_era>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.eraYear
 */
static bool PlainDate_eraYear(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Steps 3-5.
  return CalendarEraYear(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.eraYear
 */
static bool PlainDate_eraYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_eraYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.year
 */
static bool PlainDate_year(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarYear(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.year
 */
static bool PlainDate_year(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_year>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.month
 */
static bool PlainDate_month(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarMonth(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.month
 */
static bool PlainDate_month(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_month>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.monthCode
 */
static bool PlainDate_monthCode(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarMonthCode(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.monthCode
 */
static bool PlainDate_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_monthCode>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.day
 */
static bool PlainDate_day(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarDay(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.day
 */
static bool PlainDate_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_day>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.dayOfWeek
 */
static bool PlainDate_dayOfWeek(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarDayOfWeek(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.dayOfWeek
 */
static bool PlainDate_dayOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_dayOfWeek>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.dayOfYear
 */
static bool PlainDate_dayOfYear(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarDayOfYear(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.dayOfYear
 */
static bool PlainDate_dayOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_dayOfYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.weekOfYear
 */
static bool PlainDate_weekOfYear(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Steps 3-5.
  return CalendarWeekOfYear(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.weekOfYear
 */
static bool PlainDate_weekOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_weekOfYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.yearOfWeek
 */
static bool PlainDate_yearOfWeek(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Steps 3-5.
  return CalendarYearOfWeek(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.yearOfWeek
 */
static bool PlainDate_yearOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_yearOfWeek>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.daysInWeek
 */
static bool PlainDate_daysInWeek(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarDaysInWeek(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.daysInWeek
 */
static bool PlainDate_daysInWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_daysInWeek>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.daysInMonth
 */
static bool PlainDate_daysInMonth(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarDaysInMonth(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.daysInMonth
 */
static bool PlainDate_daysInMonth(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_daysInMonth>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.daysInYear
 */
static bool PlainDate_daysInYear(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarDaysInYear(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.daysInYear
 */
static bool PlainDate_daysInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_daysInYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.monthsInYear
 */
static bool PlainDate_monthsInYear(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarMonthsInYear(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.monthsInYear
 */
static bool PlainDate_monthsInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_monthsInYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.inLeapYear
 */
static bool PlainDate_inLeapYear(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  return CalendarInLeapYear(cx, calendar, temporalDate->date(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.inLeapYear
 */
static bool PlainDate_inLeapYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_inLeapYear>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toPlainYearMonth ( )
 */
static bool PlainDate_toPlainYearMonth(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateWithCalendar> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  auto calendar = temporalDate.calendar();

  // Step 4.
  Rooted<CalendarFields> fields(cx);
  if (!ISODateToFields(cx, temporalDate, &fields)) {
    return false;
  }

  // Step 5.
  Rooted<PlainYearMonthWithCalendar> result(cx);
  if (!CalendarYearMonthFromFields(cx, calendar, fields,
                                   TemporalOverflow::Constrain, &result)) {
    return false;
  }

  // Steps 6-7.
  auto* obj = CreateTemporalYearMonth(cx, result);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toPlainYearMonth ( )
 */
static bool PlainDate_toPlainYearMonth(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toPlainYearMonth>(cx,
                                                                       args);
}

/**
 * Temporal.PlainDate.prototype.toPlainMonthDay ( )
 */
static bool PlainDate_toPlainMonthDay(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateWithCalendar> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  auto calendar = temporalDate.calendar();

  // Step 4.
  Rooted<CalendarFields> fields(cx);
  if (!ISODateToFields(cx, temporalDate, &fields)) {
    return false;
  }

  // Step 5.
  Rooted<PlainMonthDayWithCalendar> result(cx);
  if (!CalendarMonthDayFromFields(cx, calendar, fields,
                                  TemporalOverflow::Constrain, &result)) {
    return false;
  }

  // Steps 6-7.
  auto* obj = CreateTemporalMonthDay(cx, result);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toPlainMonthDay ( )
 */
static bool PlainDate_toPlainMonthDay(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toPlainMonthDay>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toPlainDateTime ( [ temporalTime ] )
 */
static bool PlainDate_toPlainDateTime(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Default initialize the time component to all zero.
  PlainDateTime dateTime = {temporalDate->date(), {}};

  // Step 3. (Inlined ToTemporalTimeOrMidnight)
  if (args.hasDefined(0)) {
    if (!ToTemporalTime(cx, args[0], &dateTime.time)) {
      return false;
    }
  }

  // Step 4.
  auto* obj = CreateTemporalDateTime(cx, dateTime, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toPlainDateTime ( [ temporalTime ] )
 */
static bool PlainDate_toPlainDateTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toPlainDateTime>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.add ( temporalDurationLike [ , options ] )
 */
static bool PlainDate_add(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return AddDurationToDate(cx, TemporalAddDuration::Add, args);
}

/**
 * Temporal.PlainDate.prototype.add ( temporalDurationLike [ , options ] )
 */
static bool PlainDate_add(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_add>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.subtract ( temporalDurationLike [ , options ] )
 */
static bool PlainDate_subtract(JSContext* cx, const CallArgs& args) {
  // Step 4.
  return AddDurationToDate(cx, TemporalAddDuration::Subtract, args);
}

/**
 * Temporal.PlainDate.prototype.subtract ( temporalDurationLike [ , options ] )
 */
static bool PlainDate_subtract(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_subtract>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.with ( temporalDateLike [ , options ] )
 */
static bool PlainDate_with(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateWithCalendar> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  Rooted<JSObject*> temporalDateLike(
      cx, RequireObjectArg(cx, "temporalDateLike", "with", args.get(0)));
  if (!temporalDateLike) {
    return false;
  }
  if (!ThrowIfTemporalLikeObject(cx, temporalDateLike)) {
    return false;
  }

  // Step 4.
  auto calendar = temporalDate.calendar();

  // Step 5.
  Rooted<CalendarFields> fields(cx);
  if (!ISODateToFields(cx, temporalDate, &fields)) {
    return false;
  }

  // Step 6.
  Rooted<CalendarFields> partialDate(cx);
  if (!PreparePartialCalendarFields(cx, calendar, temporalDateLike,
                                    {
                                        CalendarField::Year,
                                        CalendarField::Month,
                                        CalendarField::MonthCode,
                                        CalendarField::Day,
                                    },
                                    &partialDate)) {
    return false;
  }
  MOZ_ASSERT(!partialDate.keys().isEmpty());

  // Step 7.
  fields = CalendarMergeFields(calendar, fields, partialDate);

  // Steps 8-9.
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 8.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "with", args[1]));
    if (!options) {
      return false;
    }

    // Step 9.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 10.
  Rooted<PlainDateWithCalendar> date(cx);
  if (!CalendarDateFromFields(cx, calendar, fields, overflow, &date)) {
    return false;
  }

  // Step 11.
  auto* result = CreateTemporalDate(cx, date);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainDate.prototype.with ( temporalDateLike [ , options ] )
 */
static bool PlainDate_with(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_with>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.withCalendar ( calendar )
 */
static bool PlainDate_withCalendar(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  auto date = temporalDate->date();

  // Step 3.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendar(cx, args.get(0), &calendar)) {
    return false;
  }

  // Step 4.
  auto* result = CreateTemporalDate(cx, date, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainDate.prototype.withCalendar ( calendar )
 */
static bool PlainDate_withCalendar(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_withCalendar>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.until ( other [ , options ] )
 */
static bool PlainDate_until(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return DifferenceTemporalPlainDate(cx, TemporalDifference::Until, args);
}

/**
 * Temporal.PlainDate.prototype.until ( other [ , options ] )
 */
static bool PlainDate_until(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_until>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.since ( other [ , options ] )
 */
static bool PlainDate_since(JSContext* cx, const CallArgs& args) {
  // Step 3.
  return DifferenceTemporalPlainDate(cx, TemporalDifference::Since, args);
}

/**
 * Temporal.PlainDate.prototype.since ( other [ , options ] )
 */
static bool PlainDate_since(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_since>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.equals ( other )
 */
static bool PlainDate_equals(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  auto date = temporalDate->date();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Step 3.
  Rooted<PlainDateWithCalendar> other(cx);
  if (!ToTemporalDate(cx, args.get(0), &other)) {
    return false;
  }

  // Steps 4-7.
  bool equals =
      date == other.date() && CalendarEquals(calendar, other.calendar());

  args.rval().setBoolean(equals);
  return true;
}

/**
 * Temporal.PlainDate.prototype.equals ( other )
 */
static bool PlainDate_equals(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_equals>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toZonedDateTime ( item )
 *
 * The |item| argument represents either a time zone or an options object. The
 * following cases are supported:
 * - |item| is a `Temporal.TimeZone` object.
 * - |item| is a user-defined time zone object.
 * - |item| is an options object with `timeZone` and `plainTime` properties.
 * - |item| is a time zone identifier string.
 *
 * User-defined time zone objects are distinguished from options objects by the
 * `timeZone` property, i.e. if a `timeZone` property is present, the object is
 * treated as an options object, otherwise an object is treated as a
 * user-defined time zone.
 */
static bool PlainDate_toZonedDateTime(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  auto date = temporalDate->date();
  Rooted<CalendarValue> calendar(cx, temporalDate->calendar());

  // Steps 3-4
  Rooted<TimeZoneValue> timeZone(cx);
  Rooted<Value> temporalTime(cx);
  if (args.get(0).isObject()) {
    Rooted<JSObject*> item(cx, &args[0].toObject());

    // Step 3.a.
    Rooted<Value> timeZoneLike(cx);
    if (!GetProperty(cx, item, item, cx->names().timeZone, &timeZoneLike)) {
      return false;
    }

    // Steps 3.b-c.
    if (timeZoneLike.isUndefined()) {
      // Step 3.b.i.
      if (!ToTemporalTimeZone(cx, args[0], &timeZone)) {
        return false;
      }

      // Step 3.b.ii.  (Not applicable in our implementation.)
    } else {
      // Step 3.c.i.
      if (!ToTemporalTimeZone(cx, timeZoneLike, &timeZone)) {
        return false;
      }

      // Step 3.c.ii.
      if (!GetProperty(cx, item, item, cx->names().plainTime, &temporalTime)) {
        return false;
      }
    }
  } else {
    // Step 4.a.
    if (!ToTemporalTimeZone(cx, args.get(0), &timeZone)) {
      return false;
    }

    // Step 4.b. (Not applicable in our implementation.)
  }

  // Steps 5-6.
  Instant instant;
  if (temporalTime.isUndefined()) {
    // Steps 5.a-b.
    if (!GetStartOfDay(cx, timeZone, date, &instant)) {
      return false;
    }
  } else {
    // Step 6.a.
    PlainTime time = {};
    if (!ToTemporalTime(cx, temporalTime, &time)) {
      return false;
    }

    // Steps 6.b-c.
    PlainDateTime temporalDateTime;
    if (!CreateTemporalDateTime(cx, date, time, &temporalDateTime)) {
      return false;
    }

    // Step 6.d.
    if (!GetInstantFor(cx, timeZone, temporalDateTime,
                       TemporalDisambiguation::Compatible, &instant)) {
      return false;
    }
  }

  // Step 7.
  auto* result = CreateTemporalZonedDateTime(cx, instant, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toZonedDateTime ( item )
 */
static bool PlainDate_toZonedDateTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toZonedDateTime>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toString ( [ options ] )
 */
static bool PlainDate_toString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  auto showCalendar = ShowCalendar::Auto;
  if (args.hasDefined(0)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "toString", args[0]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (!GetTemporalShowCalendarNameOption(cx, options, &showCalendar)) {
      return false;
    }
  }

  // Step 5.
  JSString* str = TemporalDateToString(cx, temporalDate, showCalendar);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toString ( [ options ] )
 */
static bool PlainDate_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toString>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainDate_toLocaleString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  JSString* str = TemporalDateToString(cx, temporalDate, ShowCalendar::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainDate_toLocaleString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toLocaleString>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toJSON ( )
 */
static bool PlainDate_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  JSString* str = TemporalDateToString(cx, temporalDate, ShowCalendar::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toJSON ( )
 */
static bool PlainDate_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toJSON>(cx, args);
}

/**
 *  Temporal.PlainDate.prototype.valueOf ( )
 */
static bool PlainDate_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainDate", "primitive type");
  return false;
}

const JSClass PlainDateObject::class_ = {
    "Temporal.PlainDate",
    JSCLASS_HAS_RESERVED_SLOTS(PlainDateObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainDate),
    JS_NULL_CLASS_OPS,
    &PlainDateObject::classSpec_,
};

const JSClass& PlainDateObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainDate_methods[] = {
    JS_FN("from", PlainDate_from, 1, 0),
    JS_FN("compare", PlainDate_compare, 2, 0),
    JS_FS_END,
};

static const JSFunctionSpec PlainDate_prototype_methods[] = {
    JS_FN("toPlainMonthDay", PlainDate_toPlainMonthDay, 0, 0),
    JS_FN("toPlainYearMonth", PlainDate_toPlainYearMonth, 0, 0),
    JS_FN("toPlainDateTime", PlainDate_toPlainDateTime, 0, 0),
    JS_FN("add", PlainDate_add, 1, 0),
    JS_FN("subtract", PlainDate_subtract, 1, 0),
    JS_FN("with", PlainDate_with, 1, 0),
    JS_FN("withCalendar", PlainDate_withCalendar, 1, 0),
    JS_FN("until", PlainDate_until, 1, 0),
    JS_FN("since", PlainDate_since, 1, 0),
    JS_FN("equals", PlainDate_equals, 1, 0),
    JS_FN("toZonedDateTime", PlainDate_toZonedDateTime, 1, 0),
    JS_FN("toString", PlainDate_toString, 0, 0),
    JS_FN("toLocaleString", PlainDate_toLocaleString, 0, 0),
    JS_FN("toJSON", PlainDate_toJSON, 0, 0),
    JS_FN("valueOf", PlainDate_valueOf, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainDate_prototype_properties[] = {
    JS_PSG("calendarId", PlainDate_calendarId, 0),
    JS_PSG("era", PlainDate_era, 0),
    JS_PSG("eraYear", PlainDate_eraYear, 0),
    JS_PSG("year", PlainDate_year, 0),
    JS_PSG("month", PlainDate_month, 0),
    JS_PSG("monthCode", PlainDate_monthCode, 0),
    JS_PSG("day", PlainDate_day, 0),
    JS_PSG("dayOfWeek", PlainDate_dayOfWeek, 0),
    JS_PSG("dayOfYear", PlainDate_dayOfYear, 0),
    JS_PSG("weekOfYear", PlainDate_weekOfYear, 0),
    JS_PSG("yearOfWeek", PlainDate_yearOfWeek, 0),
    JS_PSG("daysInWeek", PlainDate_daysInWeek, 0),
    JS_PSG("daysInMonth", PlainDate_daysInMonth, 0),
    JS_PSG("daysInYear", PlainDate_daysInYear, 0),
    JS_PSG("monthsInYear", PlainDate_monthsInYear, 0),
    JS_PSG("inLeapYear", PlainDate_inLeapYear, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.PlainDate", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec PlainDateObject::classSpec_ = {
    GenericCreateConstructor<PlainDateConstructor, 3, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainDateObject>,
    PlainDate_methods,
    nullptr,
    PlainDate_prototype_methods,
    PlainDate_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
