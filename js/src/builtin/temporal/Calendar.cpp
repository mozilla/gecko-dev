/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Calendar.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/intl/ICU4XGeckoDataProvider.h"
#include "mozilla/intl/Locale.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"
#include "mozilla/UniquePtr.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "diplomat_runtime.h"
#include "ICU4XAnyCalendarKind.h"
#include "ICU4XCalendar.h"
#include "ICU4XDate.h"
#include "ICU4XIsoDate.h"
#include "ICU4XIsoWeekday.h"
#include "ICU4XWeekCalculator.h"
#include "ICU4XWeekOf.h"
#include "ICU4XWeekRelativeUnit.h"

#include "jsfriendapi.h"
#include "jsnum.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/Array.h"
#include "builtin/temporal/CalendarFields.h"
#include "builtin/temporal/Crash.h"
#include "builtin/temporal/Duration.h"
#include "builtin/temporal/Era.h"
#include "builtin/temporal/MonthCode.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "gc/GCEnum.h"
#include "gc/Tracer.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/ForOfIterator.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCAPI.h"
#include "js/GCHashTable.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/Value.h"
#include "util/Text.h"
#include "vm/ArrayObject.h"
#include "vm/BytecodeUtil.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PropertyInfo.h"
#include "vm/PropertyKey.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/Stack.h"
#include "vm/StringType.h"

#include "vm/Compartment-inl.h"
#include "vm/JSAtomUtils-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

void js::temporal::CalendarValue::trace(JSTracer* trc) {
  TraceRoot(trc, &value_, "CalendarValue::value");
}

bool js::temporal::WrapCalendarValue(JSContext* cx,
                                     MutableHandle<JS::Value> calendar) {
  MOZ_ASSERT(calendar.isInt32());
  return cx->compartment()->wrap(cx, calendar);
}

/**
 * IsISOLeapYear ( year )
 */
static constexpr bool IsISOLeapYear(int32_t year) {
  // Steps 1-5.
  return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

/**
 * IsISOLeapYear ( year )
 */
static bool IsISOLeapYear(double year) {
  // Step 1.
  MOZ_ASSERT(IsInteger(year));

  // Steps 2-5.
  return std::fmod(year, 4) == 0 &&
         (std::fmod(year, 100) != 0 || std::fmod(year, 400) == 0);
}

/**
 * ISODaysInYear ( year )
 */
int32_t js::temporal::ISODaysInYear(int32_t year) {
  // Steps 1-3.
  return IsISOLeapYear(year) ? 366 : 365;
}

/**
 * ISODaysInMonth ( year, month )
 */
static constexpr int32_t ISODaysInMonth(int32_t year, int32_t month) {
  MOZ_ASSERT(1 <= month && month <= 12);

  constexpr uint8_t daysInMonth[2][13] = {
      {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
      {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

  // Steps 1-4.
  return daysInMonth[IsISOLeapYear(year)][month];
}

/**
 * ISODaysInMonth ( year, month )
 */
int32_t js::temporal::ISODaysInMonth(int32_t year, int32_t month) {
  return ::ISODaysInMonth(year, month);
}

/**
 * ISODaysInMonth ( year, month )
 */
int32_t js::temporal::ISODaysInMonth(double year, int32_t month) {
  MOZ_ASSERT(1 <= month && month <= 12);

  static constexpr uint8_t daysInMonth[2][13] = {
      {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
      {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

  // Steps 1-4.
  return daysInMonth[IsISOLeapYear(year)][month];
}

/**
 * 21.4.1.6 Week Day
 *
 * Compute the week day from |day| without first expanding |day| into a full
 * date through |MakeDate(day, 0)|:
 *
 *   WeekDay(MakeDate(day, 0))
 * = WeekDay(day × msPerDay + 0)
 * = WeekDay(day × msPerDay)
 * = 𝔽(ℝ(Day(day × msPerDay) + 4𝔽) modulo 7)
 * = 𝔽(ℝ(𝔽(floor(ℝ((day × msPerDay) / msPerDay))) + 4𝔽) modulo 7)
 * = 𝔽(ℝ(𝔽(floor(ℝ(day))) + 4𝔽) modulo 7)
 * = 𝔽(ℝ(𝔽(day) + 4𝔽) modulo 7)
 */
static int32_t WeekDay(int32_t day) {
  int32_t result = (day + 4) % 7;
  if (result < 0) {
    result += 7;
  }
  return result;
}

/**
 * ToISODayOfWeek ( year, month, day )
 */
static int32_t ToISODayOfWeek(const PlainDate& date) {
  MOZ_ASSERT(ISODateWithinLimits(date));

  // Steps 1-3. (Not applicable in our implementation.)

  // TODO: Check if ES MakeDate + WeekDay is efficient enough.
  //
  // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Methods_in_computer_code

  // Step 4.
  int32_t day = MakeDay(date);

  // Step 5.
  int32_t weekday = WeekDay(day);
  return weekday != 0 ? weekday : 7;
}

static constexpr auto FirstDayOfMonth(int32_t year) {
  // The following array contains the day of year for the first day of each
  // month, where index 0 is January, and day 0 is January 1.
  std::array<int32_t, 13> days = {};
  for (int32_t month = 1; month <= 12; ++month) {
    days[month] = days[month - 1] + ::ISODaysInMonth(year, month);
  }
  return days;
}

/**
 * ToISODayOfYear ( year, month, day )
 */
static int32_t ToISODayOfYear(int32_t year, int32_t month, int32_t day) {
  MOZ_ASSERT(1 <= month && month <= 12);

  // First day of month arrays for non-leap and leap years.
  constexpr decltype(FirstDayOfMonth(0)) firstDayOfMonth[2] = {
      FirstDayOfMonth(1), FirstDayOfMonth(0)};

  // Steps 1-3. (Not applicable in our implementation.)

  // Steps 4-5.
  //
  // Instead of first computing the date and then using DayWithinYear to map the
  // date to the day within the year, directly lookup the first day of the month
  // and then add the additional days.
  return firstDayOfMonth[IsISOLeapYear(year)][month - 1] + day;
}

/**
 * ToISODayOfYear ( year, month, day )
 */
int32_t js::temporal::ToISODayOfYear(const PlainDate& date) {
  MOZ_ASSERT(ISODateWithinLimits(date));

  // Steps 1-5.
  const auto& [year, month, day] = date;
  return ::ToISODayOfYear(year, month, day);
}

static int32_t FloorDiv(int32_t dividend, int32_t divisor) {
  MOZ_ASSERT(divisor > 0);

  int32_t quotient = dividend / divisor;
  int32_t remainder = dividend % divisor;
  if (remainder < 0) {
    quotient -= 1;
  }
  return quotient;
}

/**
 * 21.4.1.3 Year Number, DayFromYear
 */
static int32_t DayFromYear(int32_t year) {
  return 365 * (year - 1970) + FloorDiv(year - 1969, 4) -
         FloorDiv(year - 1901, 100) + FloorDiv(year - 1601, 400);
}

/**
 * 21.4.1.11 MakeTime ( hour, min, sec, ms )
 */
static int64_t MakeTime(const PlainTime& time) {
  MOZ_ASSERT(IsValidTime(time));

  // Step 1 (Not applicable).

  // Step 2.
  int64_t h = time.hour;

  // Step 3.
  int64_t m = time.minute;

  // Step 4.
  int64_t s = time.second;

  // Step 5.
  int64_t milli = time.millisecond;

  // Steps 6-7.
  return h * ToMilliseconds(TemporalUnit::Hour) +
         m * ToMilliseconds(TemporalUnit::Minute) +
         s * ToMilliseconds(TemporalUnit::Second) + milli;
}

/**
 * 21.4.1.12 MakeDay ( year, month, date )
 */
int32_t js::temporal::MakeDay(const PlainDate& date) {
  MOZ_ASSERT(ISODateWithinLimits(date));

  return DayFromYear(date.year) + ToISODayOfYear(date) - 1;
}

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t js::temporal::MakeDate(const PlainDateTime& dateTime) {
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));

  // Step 1 (Not applicable).

  // Steps 2-3.
  int64_t tv = MakeDay(dateTime.date) * ToMilliseconds(TemporalUnit::Day) +
               MakeTime(dateTime.time);

  // Step 4.
  return tv;
}

/**
 * 21.4.1.12 MakeDay ( year, month, date )
 */
static int32_t MakeDay(int32_t year, int32_t month, int32_t day) {
  MOZ_ASSERT(1 <= month && month <= 12);

  // FIXME: spec issue - what should happen for invalid years/days?
  return DayFromYear(year) + ::ToISODayOfYear(year, month, day) - 1;
}

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t js::temporal::MakeDate(int32_t year, int32_t month, int32_t day) {
  // NOTE: This version accepts values outside the valid date-time limits.
  MOZ_ASSERT(1 <= month && month <= 12);

  // Step 1 (Not applicable).

  // Steps 2-3.
  int64_t tv = ::MakeDay(year, month, day) * ToMilliseconds(TemporalUnit::Day);

  // Step 4.
  return tv;
}

struct YearWeek final {
  int32_t year = 0;
  int32_t week = 0;
};

/**
 * ToISOWeekOfYear ( year, month, day )
 */
static YearWeek ToISOWeekOfYear(const PlainDate& date) {
  MOZ_ASSERT(ISODateWithinLimits(date));

  const auto& [year, month, day] = date;

  // TODO: https://en.wikipedia.org/wiki/Week#The_ISO_week_date_system
  // TODO: https://en.wikipedia.org/wiki/ISO_week_date#Algorithms

  // Steps 1-3. (Not applicable in our implementation.)

  // Steps 4-5.
  int32_t doy = ToISODayOfYear(date);
  int32_t dow = ToISODayOfWeek(date);

  int32_t woy = (10 + doy - dow) / 7;
  MOZ_ASSERT(0 <= woy && woy <= 53);

  // An ISO year has 53 weeks if the year starts on a Thursday or if it's a
  // leap year which starts on a Wednesday.
  auto isLongYear = [](int32_t year) {
    int32_t startOfYear = ToISODayOfWeek({year, 1, 1});
    return startOfYear == 4 || (startOfYear == 3 && IsISOLeapYear(year));
  };

  // Part of last year's last week, which is either week 52 or week 53.
  if (woy == 0) {
    return {year - 1, 52 + int32_t(isLongYear(year - 1))};
  }

  // Part of next year's first week if the current year isn't a long year.
  if (woy == 53 && !isLongYear(year)) {
    return {year + 1, 1};
  }

  return {year, woy};
}

/**
 * Return the BCP-47 string for the given calendar id.
 */
static std::string_view CalendarIdToBcp47(CalendarId id) {
  switch (id) {
    case CalendarId::ISO8601:
      return "iso8601";
    case CalendarId::Buddhist:
      return "buddhist";
    case CalendarId::Chinese:
      return "chinese";
    case CalendarId::Coptic:
      return "coptic";
    case CalendarId::Dangi:
      return "dangi";
    case CalendarId::Ethiopian:
      return "ethiopic";
    case CalendarId::EthiopianAmeteAlem:
      return "ethioaa";
    case CalendarId::Gregorian:
      return "gregory";
    case CalendarId::Hebrew:
      return "hebrew";
    case CalendarId::Indian:
      return "indian";
    case CalendarId::Islamic:
      return "islamic";
    case CalendarId::IslamicCivil:
      return "islamic-civil";
    case CalendarId::IslamicRGSA:
      return "islamic-rgsa";
    case CalendarId::IslamicTabular:
      return "islamic-tbla";
    case CalendarId::IslamicUmmAlQura:
      return "islamic-umalqura";
    case CalendarId::Japanese:
      return "japanese";
    case CalendarId::Persian:
      return "persian";
    case CalendarId::ROC:
      return "roc";
  }
  MOZ_CRASH("invalid calendar id");
}

class MOZ_STACK_CLASS AsciiLowerCaseChars final {
  static constexpr size_t InlineCapacity = 24;

  Vector<char, InlineCapacity> chars_;

 public:
  explicit AsciiLowerCaseChars(JSContext* cx) : chars_(cx) {}

  operator mozilla::Span<const char>() const {
    return mozilla::Span<const char>{chars_};
  }

  [[nodiscard]] bool init(JSLinearString* str) {
    MOZ_ASSERT(StringIsAscii(str));

    if (!chars_.resize(str->length())) {
      return false;
    }

    CopyChars(reinterpret_cast<JS::Latin1Char*>(chars_.begin()), *str);

    mozilla::intl::AsciiToLowerCase(chars_.begin(), chars_.length(),
                                    chars_.begin());

    return true;
  }
};

/**
 * CanonicalizeCalendar ( id )
 */
bool js::temporal::CanonicalizeCalendar(JSContext* cx, Handle<JSString*> id,
                                        MutableHandle<CalendarValue> result) {
  Rooted<JSLinearString*> linear(cx, id->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  // Steps 1-3.
  do {
    if (!StringIsAscii(linear) || linear->empty()) {
      break;
    }

    AsciiLowerCaseChars lowerCaseChars(cx);
    if (!lowerCaseChars.init(linear)) {
      return false;
    }
    mozilla::Span<const char> id = lowerCaseChars;

    // Reject invalid types before trying to resolve aliases.
    if (mozilla::intl::LocaleParser::CanParseUnicodeExtensionType(id).isErr()) {
      break;
    }

    // Resolve calendar aliases.
    static constexpr auto key = mozilla::MakeStringSpan("ca");
    if (const char* replacement =
            mozilla::intl::Locale::ReplaceUnicodeExtensionType(key, id)) {
      id = mozilla::MakeStringSpan(replacement);
    }

    // Step 1.
    static constexpr auto& calendars = AvailableCalendars();

    // Steps 2-3.
    for (auto identifier : calendars) {
      if (id == mozilla::Span{CalendarIdToBcp47(identifier)}) {
        result.set(CalendarValue(identifier));
        return true;
      }
    }
  } while (false);

  if (auto chars = QuoteString(cx, linear)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_TEMPORAL_CALENDAR_INVALID_ID, chars.get());
  }
  return false;
}

template <typename T, typename... Ts>
static bool ToTemporalCalendar(JSContext* cx, Handle<JSObject*> object,
                               MutableHandle<CalendarValue> result) {
  if (auto* unwrapped = object->maybeUnwrapIf<T>()) {
    result.set(unwrapped->calendar());
    return result.wrap(cx);
  }

  if constexpr (sizeof...(Ts) > 0) {
    return ToTemporalCalendar<Ts...>(cx, object, result);
  }

  result.set(CalendarValue());
  return true;
}

/**
 * ToTemporalCalendarSlotValue ( temporalCalendarLike )
 */
bool js::temporal::ToTemporalCalendar(JSContext* cx,
                                      Handle<Value> temporalCalendarLike,
                                      MutableHandle<CalendarValue> result) {
  // Step 1.
  if (temporalCalendarLike.isObject()) {
    Rooted<JSObject*> obj(cx, &temporalCalendarLike.toObject());

    // Step 1.a.
    Rooted<CalendarValue> calendar(cx);
    if (!::ToTemporalCalendar<PlainDateObject, PlainDateTimeObject,
                              PlainMonthDayObject, PlainYearMonthObject,
                              ZonedDateTimeObject>(cx, obj, &calendar)) {
      return false;
    }
    if (calendar) {
      result.set(calendar);
      return true;
    }
  }

  // Step 2.
  if (!temporalCalendarLike.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK,
                     temporalCalendarLike, nullptr, "not a string");
    return false;
  }
  Rooted<JSString*> str(cx, temporalCalendarLike.toString());

  // Step 3.
  Rooted<JSLinearString*> id(cx, ParseTemporalCalendarString(cx, str));
  if (!id) {
    return false;
  }

  // Step 4.
  return CanonicalizeCalendar(cx, id, result);
}

/**
 * GetTemporalCalendarSlotValueWithISODefault ( item )
 */
bool js::temporal::GetTemporalCalendarWithISODefault(
    JSContext* cx, Handle<JSObject*> item,
    MutableHandle<CalendarValue> result) {
  // Step 1.
  Rooted<CalendarValue> calendar(cx);
  if (!::ToTemporalCalendar<PlainDateObject, PlainDateTimeObject,
                            PlainMonthDayObject, PlainYearMonthObject,
                            ZonedDateTimeObject>(cx, item, &calendar)) {
    return false;
  }
  if (calendar) {
    result.set(calendar);
    return true;
  }

  // Step 2.
  Rooted<Value> calendarValue(cx);
  if (!GetProperty(cx, item, item, cx->names().calendar, &calendarValue)) {
    return false;
  }

  // Step 3.
  if (calendarValue.isUndefined()) {
    result.set(CalendarValue(CalendarId::ISO8601));
    return true;
  }

  // Step 4.
  return ToTemporalCalendar(cx, calendarValue, result);
}

/**
 * ToTemporalCalendarIdentifier ( calendarSlotValue )
 */
std::string_view js::temporal::ToTemporalCalendarIdentifier(
    const CalendarValue& calendar) {
  return CalendarIdToBcp47(calendar.identifier());
}

/**
 * ToTemporalCalendarIdentifier ( calendarSlotValue )
 */
JSLinearString* js::temporal::ToTemporalCalendarIdentifier(
    JSContext* cx, Handle<CalendarValue> calendar) {
  return NewStringCopy<CanGC>(cx, ToTemporalCalendarIdentifier(calendar));
}

static auto ToAnyCalendarKind(CalendarId id) {
  switch (id) {
    case CalendarId::ISO8601:
      return capi::ICU4XAnyCalendarKind_Iso;
    case CalendarId::Buddhist:
      return capi::ICU4XAnyCalendarKind_Buddhist;
    case CalendarId::Chinese:
      return capi::ICU4XAnyCalendarKind_Chinese;
    case CalendarId::Coptic:
      return capi::ICU4XAnyCalendarKind_Coptic;
    case CalendarId::Dangi:
      return capi::ICU4XAnyCalendarKind_Dangi;
    case CalendarId::Ethiopian:
      return capi::ICU4XAnyCalendarKind_Ethiopian;
    case CalendarId::EthiopianAmeteAlem:
      return capi::ICU4XAnyCalendarKind_EthiopianAmeteAlem;
    case CalendarId::Gregorian:
      return capi::ICU4XAnyCalendarKind_Gregorian;
    case CalendarId::Hebrew:
      return capi::ICU4XAnyCalendarKind_Hebrew;
    case CalendarId::Indian:
      return capi::ICU4XAnyCalendarKind_Indian;
    case CalendarId::IslamicCivil:
      return capi::ICU4XAnyCalendarKind_IslamicCivil;
    case CalendarId::Islamic:
      return capi::ICU4XAnyCalendarKind_IslamicObservational;
    case CalendarId::IslamicRGSA:
      // ICU4X doesn't support a separate islamic-rgsa calendar, so we use the
      // observational calendar instead. This also matches ICU4C.
      return capi::ICU4XAnyCalendarKind_IslamicObservational;
    case CalendarId::IslamicTabular:
      return capi::ICU4XAnyCalendarKind_IslamicTabular;
    case CalendarId::IslamicUmmAlQura:
      return capi::ICU4XAnyCalendarKind_IslamicUmmAlQura;
    case CalendarId::Japanese:
      return capi::ICU4XAnyCalendarKind_Japanese;
    case CalendarId::Persian:
      return capi::ICU4XAnyCalendarKind_Persian;
    case CalendarId::ROC:
      return capi::ICU4XAnyCalendarKind_Roc;
  }
  MOZ_CRASH("invalid calendar id");
}

class ICU4XCalendarDeleter {
 public:
  void operator()(capi::ICU4XCalendar* ptr) {
    capi::ICU4XCalendar_destroy(ptr);
  }
};

using UniqueICU4XCalendar =
    mozilla::UniquePtr<capi::ICU4XCalendar, ICU4XCalendarDeleter>;

static UniqueICU4XCalendar CreateICU4XCalendar(JSContext* cx, CalendarId id) {
  auto result = capi::ICU4XCalendar_create_for_kind(
      mozilla::intl::GetDataProvider(), ToAnyCalendarKind(id));
  if (!result.is_ok) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
    return nullptr;
  }
  return UniqueICU4XCalendar{result.ok};
}

class ICU4XDateDeleter {
 public:
  void operator()(capi::ICU4XDate* ptr) { capi::ICU4XDate_destroy(ptr); }
};

using UniqueICU4XDate = mozilla::UniquePtr<capi::ICU4XDate, ICU4XDateDeleter>;

static UniqueICU4XDate CreateICU4XDate(JSContext* cx, const PlainDate& date,
                                       const capi::ICU4XCalendar* calendar) {
  auto result = capi::ICU4XDate_create_from_iso_in_calendar(
      date.year, date.month, date.day, calendar);
  if (!result.is_ok) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
    return nullptr;
  }
  return UniqueICU4XDate{result.ok};
}

class ICU4XIsoDateDeleter {
 public:
  void operator()(capi::ICU4XIsoDate* ptr) { capi::ICU4XIsoDate_destroy(ptr); }
};

using UniqueICU4XIsoDate =
    mozilla::UniquePtr<capi::ICU4XIsoDate, ICU4XIsoDateDeleter>;

class ICU4XWeekCalculatorDeleter {
 public:
  void operator()(capi::ICU4XWeekCalculator* ptr) {
    capi::ICU4XWeekCalculator_destroy(ptr);
  }
};

using UniqueICU4XWeekCalculator =
    mozilla::UniquePtr<capi::ICU4XWeekCalculator, ICU4XWeekCalculatorDeleter>;

static UniqueICU4XWeekCalculator CreateICU4WeekCalculator(JSContext* cx,
                                                          CalendarId calendar) {
  MOZ_ASSERT(calendar == CalendarId::Gregorian);

  auto firstWeekday = capi::ICU4XIsoWeekday_Monday;
  uint8_t minWeekDays = 1;

  auto* result =
      capi::ICU4XWeekCalculator_create_from_first_day_of_week_and_min_week_days(
          firstWeekday, minWeekDays);
  return UniqueICU4XWeekCalculator{result};
}

static constexpr size_t EraNameMaxLength() {
  size_t length = 0;
  for (auto calendar : AvailableCalendars()) {
    for (auto era : CalendarEras(calendar)) {
      for (auto name : CalendarEraNames(calendar, era)) {
        length = std::max(length, name.length());
      }
    }
  }
  return length;
}

static mozilla::Maybe<EraCode> EraForString(CalendarId calendar,
                                            JSLinearString* string) {
  MOZ_ASSERT(CalendarEraRelevant(calendar));

  // Note: Assigning MaxLength to EraNameMaxLength() breaks the CDT indexer.
  constexpr size_t MaxLength = 24;
  static_assert(MaxLength >= EraNameMaxLength(),
                "Storage size is at least as large as the largest known era");

  if (string->length() > MaxLength || !StringIsAscii(string)) {
    return mozilla::Nothing();
  }

  char chars[MaxLength] = {};
  CopyChars(reinterpret_cast<JS::Latin1Char*>(chars), *string);

  auto stringView = std::string_view{chars, string->length()};

  for (auto era : CalendarEras(calendar)) {
    for (auto name : CalendarEraNames(calendar, era)) {
      if (name == stringView) {
        return mozilla::Some(era);
      }
    }
  }
  return mozilla::Nothing();
}

static constexpr std::string_view IcuEraName(CalendarId calendar, EraCode era) {
  switch (calendar) {
    // https://docs.rs/icu/latest/icu/calendar/iso/struct.Iso.html#era-codes
    case CalendarId::ISO8601: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "default";
    }

    // https://docs.rs/icu/latest/icu/calendar/buddhist/struct.Buddhist.html#era-codes
    case CalendarId::Buddhist: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "be";
    }

    // https://docs.rs/icu/latest/icu/calendar/chinese/struct.Chinese.html#year-and-era-codes
    case CalendarId::Chinese: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "chinese";
    }

    // https://docs.rs/icu/latest/icu/calendar/coptic/struct.Coptic.html#era-codes
    case CalendarId::Coptic: {
      MOZ_ASSERT(era == EraCode::Standard || era == EraCode::Inverse);
      return era == EraCode::Standard ? "ad" : "bd";
    }

    // https://docs.rs/icu/latest/icu/calendar/dangi/struct.Dangi.html#era-codes
    case CalendarId::Dangi: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "dangi";
    }

    // https://docs.rs/icu/latest/icu/calendar/ethiopian/struct.Ethiopian.html#era-codes
    case CalendarId::Ethiopian: {
      MOZ_ASSERT(era == EraCode::Standard || era == EraCode::Inverse);
      return era == EraCode::Standard ? "incar" : "pre-incar";
    }

    // https://docs.rs/icu/latest/icu/calendar/ethiopian/struct.Ethiopian.html#era-codes
    case CalendarId::EthiopianAmeteAlem: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "mundi";
    }

    // https://docs.rs/icu/latest/icu/calendar/gregorian/struct.Gregorian.html#era-codes
    case CalendarId::Gregorian: {
      MOZ_ASSERT(era == EraCode::Standard || era == EraCode::Inverse);
      return era == EraCode::Standard ? "ce" : "bce";
    }

    // https://docs.rs/icu/latest/icu/calendar/hebrew/struct.Hebrew.html
    case CalendarId::Hebrew: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "am";
    }

    // https://docs.rs/icu/latest/icu/calendar/indian/struct.Indian.html#era-codes
    case CalendarId::Indian: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "saka";
    }

    // https://docs.rs/icu/latest/icu/calendar/islamic/struct.IslamicCivil.html#era-codes
    // https://docs.rs/icu/latest/icu/calendar/islamic/struct.IslamicObservational.html#era-codes
    // https://docs.rs/icu/latest/icu/calendar/islamic/struct.IslamicTabular.html#era-codes
    // https://docs.rs/icu/latest/icu/calendar/islamic/struct.IslamicUmmAlQura.html#era-codes
    // https://docs.rs/icu/latest/icu/calendar/persian/struct.Persian.html#era-codes
    case CalendarId::Islamic:
    case CalendarId::IslamicCivil:
    case CalendarId::IslamicRGSA:
    case CalendarId::IslamicTabular:
    case CalendarId::IslamicUmmAlQura:
    case CalendarId::Persian: {
      MOZ_ASSERT(era == EraCode::Standard);
      return "ah";
    }

    // https://docs.rs/icu/latest/icu/calendar/japanese/struct.Japanese.html#era-codes
    case CalendarId::Japanese: {
      switch (era) {
        case EraCode::Standard:
          return "ce";
        case EraCode::Inverse:
          return "bce";
        case EraCode::Meiji:
          return "meiji";
        case EraCode::Taisho:
          return "taisho";
        case EraCode::Showa:
          return "showa";
        case EraCode::Heisei:
          return "heisei";
        case EraCode::Reiwa:
          return "reiwa";
      }
      break;
    }

    // https://docs.rs/icu/latest/icu/calendar/roc/struct.Roc.html#era-codes
    case CalendarId::ROC: {
      MOZ_ASSERT(era == EraCode::Standard || era == EraCode::Inverse);
      return era == EraCode::Standard ? "roc" : "roc-inverse";
    }
  }
  JS_CONSTEXPR_CRASH("invalid era");
}

enum class CalendarError {
  // Catch-all kind for all other error types.
  Generic,

  // https://docs.rs/icu/latest/icu/calendar/enum.Error.html#variant.Overflow
  Overflow,

  // https://docs.rs/icu/latest/icu/calendar/enum.Error.html#variant.Underflow
  Underflow,

  // https://docs.rs/icu/latest/icu/calendar/enum.Error.html#variant.OutOfRange
  OutOfRange,

  // https://docs.rs/icu/latest/icu/calendar/enum.Error.html#variant.UnknownEra
  UnknownEra,

  // https://docs.rs/icu/latest/icu/calendar/enum.Error.html#variant.UnknownMonthCode
  UnknownMonthCode,
};

static mozilla::Result<UniqueICU4XDate, CalendarError> CreateDateFromCodes(
    CalendarId calendarId, const capi::ICU4XCalendar* calendar, EraYear eraYear,
    MonthCode monthCode, int32_t day) {
  MOZ_ASSERT(calendarId != CalendarId::ISO8601);
  MOZ_ASSERT(capi::ICU4XCalendar_kind(calendar) ==
             ToAnyCalendarKind(calendarId));
  MOZ_ASSERT(mozilla::EnumSet<EraCode>(CalendarEras(calendarId))
                 .contains(eraYear.era));
  MOZ_ASSERT_IF(CalendarEraRelevant(calendarId), eraYear.year > 0);
  MOZ_ASSERT(CalendarMonthCodes(calendarId).contains(monthCode));
  MOZ_ASSERT(day > 0);
  MOZ_ASSERT(day <= CalendarDaysInMonth(calendarId).second);

  auto era = IcuEraName(calendarId, eraYear.era);
  auto monthCodeView = std::string_view{monthCode};
  auto date = capi::ICU4XDate_create_from_codes_in_calendar(
      era.data(), era.length(), eraYear.year, monthCodeView.data(),
      monthCodeView.length(), day, calendar);
  if (date.is_ok) {
    return UniqueICU4XDate{date.ok};
  }

  // Map possible calendar errors.
  //
  // Calendar error codes which can't happen for `create_from_codes_in_calendar`
  // are mapped to `CalendarError::Generic`.
  switch (date.err) {
    case capi::ICU4XError_CalendarOverflowError:
      return mozilla::Err(CalendarError::Overflow);
    case capi::ICU4XError_CalendarUnderflowError:
      return mozilla::Err(CalendarError::Underflow);
    case capi::ICU4XError_CalendarOutOfRangeError:
      return mozilla::Err(CalendarError::OutOfRange);
    case capi::ICU4XError_CalendarUnknownEraError:
      return mozilla::Err(CalendarError::UnknownEra);
    case capi::ICU4XError_CalendarUnknownMonthCodeError:
      return mozilla::Err(CalendarError::UnknownMonthCode);
    default:
      return mozilla::Err(CalendarError::Generic);
  }
}

/**
 * The date `eraYear-monthCode-day` doesn't exist in `era`. Map it to the
 * closest valid date in `era`.
 *
 * For example:
 *
 * Reiwa 1, April 30 doesn't exist, because the Reiwa era started on May 1 2019,
 * the input is constrained to the first valid date in the Reiwa era, i.e.
 * Reiwa 1, May 1.
 *
 * Similarly, Heisei 31, May 1 doesn't exist, because on May 1 2019 the Reiwa
 * era started. The input is therefore constrained to Heisei 31, April 30.
 */
static mozilla::Result<UniqueICU4XDate, CalendarError>
CreateDateFromCodesConstrainToJapaneseEra(JSContext* cx, CalendarId calendarId,
                                          const capi::ICU4XCalendar* calendar,
                                          EraYear eraYear, MonthCode monthCode,
                                          int32_t day) {
  MOZ_ASSERT(calendarId == CalendarId::Japanese);
  MOZ_ASSERT(capi::ICU4XCalendar_kind(calendar) ==
             ToAnyCalendarKind(calendarId));
  MOZ_ASSERT(!CalendarEraStartsAtYearBoundary(calendarId, eraYear.era));
  MOZ_ASSERT(!monthCode.isLeapMonth());
  MOZ_ASSERT(1 <= monthCode.ordinal() && monthCode.ordinal() <= 12);
  MOZ_ASSERT(1 <= day && day <= 31);

  const auto& [era, year] = eraYear;

  int32_t month = monthCode.ordinal();
  const int32_t startMonth = month;

  // Case 1: The requested date is before the start of the era.
  if (year == 1) {
    // The first year of modern eras is guaranteed to end on December 31, so
    // we don't have to worry about the first era ending mid-year. If we ever
    // add support for JapaneseExtended, we have to update this code to handle
    // that case.
    MOZ_ASSERT(capi::ICU4XCalendar_kind(calendar) !=
               capi::ICU4XAnyCalendarKind_JapaneseExtended);

    auto firstEraYear = EraYear{era, 1};

    // Find the first month which is completely within the era.
    for (; month <= 12; month++) {
      auto firstDayOfMonth = CreateDateFromCodes(
          calendarId, calendar, firstEraYear, MonthCode{month}, 1);
      if (firstDayOfMonth.isOk()) {
        // If the month matches the start month, we only need to constrain day.
        if (month == startMonth) {
          int32_t lastDayOfMonth =
              capi::ICU4XDate_days_in_month(firstDayOfMonth.inspect().get());
          return CreateDateFromCodes(calendarId, calendar, firstEraYear,
                                     MonthCode{month},
                                     std::min(day, lastDayOfMonth));
        }
        break;
      }

      // Out-of-range error indicates the requested date isn't within the era,
      // so we have to keep looking. Any other error is reported back to the
      // caller.
      if (firstDayOfMonth.inspectErr() != CalendarError::OutOfRange) {
        return firstDayOfMonth.propagateErr();
      }
    }
    MOZ_ASSERT(startMonth < month);

    // When we've reached this point, we know that the era either starts in
    // |month - 1| or at the first day of |month|.
    auto monthCode = MonthCode{month - 1};

    // The requested month is before the era's first month. Return the start of
    // the era.
    if (startMonth < month - 1) {
      // The first day of |month| is within the era, but the first day of
      // |month - 1| isn't within the era. Maybe there's a day after the first
      // day of |month - 1| which is part of the era.
      for (int32_t firstDayOfEra = 2; firstDayOfEra <= 31; firstDayOfEra++) {
        auto date = CreateDateFromCodes(calendarId, calendar, firstEraYear,
                                        monthCode, firstDayOfEra);
        if (date.isOk()) {
          return date.unwrap();
        }

        // Out-of-range error indicates the requested date isn't within the era,
        // so we have to keep looking.
        if (date.inspectErr() == CalendarError::OutOfRange) {
          continue;
        }

        // Overflow error is reported when the date is past the last day of the
        // month.
        if (date.inspectErr() == CalendarError::Overflow) {
          break;
        }

        // Any other error is reported back to the caller.
        return date.propagateErr();
      }

      // No valid day was found in the last month, so the start of the era must
      // be the first day of |month|.
      return CreateDateFromCodes(calendarId, calendar, firstEraYear,
                                 MonthCode{month}, 1);
    }

    // We're done if |date| is now valid.
    auto date =
        CreateDateFromCodes(calendarId, calendar, firstEraYear, monthCode, day);
    if (date.isOk()) {
      return date.unwrap();
    }

    // Otherwise check in which direction we need to adjust |day|.
    auto errorCode = date.inspectErr();
    int32_t direction;
    if (errorCode == CalendarError::Overflow) {
      direction = -1;
    } else if (errorCode == CalendarError::OutOfRange) {
      direction = 1;
    } else {
      return date.propagateErr();
    }

    // Every Gregorian month has at least 28 days and no more than 31 days, so
    // we can stop when day is less-or-equal 28 resp. greater-or-equal to 31.
    while ((direction < 0 && day > 28) || (direction > 0 && day < 31)) {
      day += direction;

      auto date = CreateDateFromCodes(calendarId, calendar, firstEraYear,
                                      monthCode, day);
      if (date.isOk()) {
        return date.unwrap();
      }
      if (date.inspectErr() == errorCode) {
        continue;
      }
      return date.propagateErr();
    }

    // If we didn't find a valid date in the last month, the start of the era
    // must be the first day of |month|.
    return CreateDateFromCodes(calendarId, calendar, firstEraYear,
                               MonthCode{month}, 1);
  }

  // Case 2: The requested date is after the end of the era.

  // Check if the first day of the year is within the era.
  auto firstDayOfYear = CreateDateFromCodes(
      calendarId, calendar, EraYear{era, year}, MonthCode{1}, 1);

  int32_t lastYearInEra;
  if (firstDayOfYear.isOk()) {
    // Case 2.a: The era ends in the requested year.
    lastYearInEra = year;
  } else if (firstDayOfYear.inspectErr() == CalendarError::OutOfRange) {
    // Case 2.b: The era ends in a previous year.

    // Start with constraining the era year (using binary search).
    int32_t minYear = 1;
    int32_t maxYear = year;
    while (minYear != maxYear) {
      int32_t candidateYear = minYear + (maxYear - minYear) / 2;

      auto firstDayOfYear = CreateDateFromCodes(
          calendarId, calendar, EraYear{era, candidateYear}, MonthCode{1}, 1);
      if (firstDayOfYear.isOk()) {
        // The year is still too large, increase the lower bound.
        minYear = candidateYear + 1;
      } else if (firstDayOfYear.inspectErr() == CalendarError::OutOfRange) {
        // The year is still too large, reduce the upper bound.
        maxYear = candidateYear;
      } else {
        return firstDayOfYear.propagateErr();
      }
    }

    // Post-condition: |minYear| is the first invalid year.
    MOZ_ASSERT(1 < minYear && minYear <= year);

    // Start looking for the last valid date in the era iterating backwards from
    // December 31.
    lastYearInEra = minYear - 1;
    month = 12;
    day = 31;
  } else {
    return firstDayOfYear.propagateErr();
  }

  auto lastEraYear = EraYear{era, lastYearInEra};
  for (; month > 0; month--) {
    // Find the last month which is still within the era.
    auto monthCode = MonthCode{month};
    auto firstDayOfMonth =
        CreateDateFromCodes(calendarId, calendar, lastEraYear, monthCode, 1);
    if (firstDayOfMonth.isErr()) {
      // Out-of-range indicates we're still past the end of the era.
      if (firstDayOfMonth.inspectErr() == CalendarError::OutOfRange) {
        continue;
      }

      // Propagate any other error to the caller.
      return firstDayOfMonth.propagateErr();
    }
    auto intermediateDate = firstDayOfMonth.unwrap();

    int32_t lastDayOfMonth =
        capi::ICU4XDate_days_in_month(intermediateDate.get());

    if (lastYearInEra == year && month == startMonth) {
      // Constrain |day| to the maximum day of month.
      day = std::min(day, lastDayOfMonth);
    } else {
      MOZ_ASSERT_IF(lastYearInEra == year, month < startMonth);
      day = lastDayOfMonth;
    }

    // Iterate forward until we find the first invalid date.
    for (int32_t nextDay = 2; nextDay <= day; nextDay++) {
      auto nextDayOfMonth = CreateDateFromCodes(
          calendarId, calendar, lastEraYear, monthCode, nextDay);
      if (nextDayOfMonth.isErr()) {
        if (nextDayOfMonth.inspectErr() == CalendarError::OutOfRange) {
          break;
        }
        return nextDayOfMonth.propagateErr();
      }
      intermediateDate = nextDayOfMonth.unwrap();
    }
    return intermediateDate;
  }

  MOZ_CRASH("error constraining to end of era");
}

static void ReportCalendarFieldOverflow(JSContext* cx, const char* name,
                                        double num) {
  ToCStringBuf numCbuf;
  const char* numStr = NumberToCString(&numCbuf, num);

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_CALENDAR_OVERFLOW_FIELD, name,
                            numStr);
}

static UniqueICU4XDate CreateDateFromCodes(JSContext* cx, CalendarId calendarId,
                                           const capi::ICU4XCalendar* calendar,
                                           EraYear eraYear, MonthCode monthCode,
                                           int32_t day,
                                           TemporalOverflow overflow) {
  MOZ_ASSERT(CalendarMonthCodes(calendarId).contains(monthCode));
  MOZ_ASSERT(day > 0);
  MOZ_ASSERT(day <= CalendarDaysInMonth(calendarId).second);

  // Constrain day to the maximum possible day for the input month.
  //
  // Special cases like February 29 in leap years of the Gregorian calendar are
  // handled below.
  int32_t daysInMonth = CalendarDaysInMonth(calendarId, monthCode).second;
  if (overflow == TemporalOverflow::Constrain) {
    day = std::min(day, daysInMonth);
  } else {
    MOZ_ASSERT(overflow == TemporalOverflow::Reject);

    if (day > daysInMonth) {
      ReportCalendarFieldOverflow(cx, "day", day);
      return nullptr;
    }
  }

  auto result =
      CreateDateFromCodes(calendarId, calendar, eraYear, monthCode, day);
  if (result.isOk()) {
    return result.unwrap();
  }

  switch (result.inspectErr()) {
    case CalendarError::UnknownMonthCode: {
      // We've asserted above that |monthCode| is valid for this calendar, so
      // any unknown month code must be for a leap month which doesn't happen in
      // the current year.
      MOZ_ASSERT(CalendarHasLeapMonths(calendarId));
      MOZ_ASSERT(monthCode.isLeapMonth());

      if (overflow == TemporalOverflow::Reject) {
        // Ensure the month code is null-terminated.
        char code[5] = {};
        auto monthCodeView = std::string_view{monthCode};
        monthCodeView.copy(code, monthCodeView.length());

        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                                 code);
        return nullptr;
      }

      // Retry as non-leap month when we're allowed to constrain.
      //
      // CalendarDateToISO ( calendar, fields, overflow )
      //
      // If the month is a leap month that doesn't exist in the year, pick
      // another date according to the cultural conventions of that calendar's
      // users. Usually this will result in the same day in the month before or
      // after where that month would normally fall in a leap year.
      //
      // Hebrew calendar:
      // Replace Adar I (M05L) with Adar (M06).
      //
      // Chinese/Dangi calendar:
      // Pick the next month, for example M03L -> M04, except for M12L, because
      // we don't to switch over to the next year.

      int32_t nonLeapMonth = std::min(monthCode.ordinal() + 1, 12);
      auto nonLeapMonthCode = MonthCode{nonLeapMonth};
      return CreateDateFromCodes(cx, calendarId, calendar, eraYear,
                                 nonLeapMonthCode, day, overflow);
    }

    case CalendarError::Overflow: {
      // ICU4X throws an overflow error when:
      // 1. month > monthsInYear(year), or
      // 2. days > daysInMonthOf(year, month).
      //
      // Case 1 can't happen for month-codes, so it doesn't apply here.
      // Case 2 can only happen when |day| is larger than the minimum number
      // of days in the month.
      MOZ_ASSERT(day > CalendarDaysInMonth(calendarId, monthCode).first);

      if (overflow == TemporalOverflow::Reject) {
        ReportCalendarFieldOverflow(cx, "day", day);
        return nullptr;
      }

      auto firstDayOfMonth = CreateDateFromCodes(
          cx, calendarId, calendar, eraYear, monthCode, 1, overflow);
      if (!firstDayOfMonth) {
        return nullptr;
      }

      int32_t daysInMonth =
          capi::ICU4XDate_days_in_month(firstDayOfMonth.get());
      MOZ_ASSERT(day > daysInMonth);
      return CreateDateFromCodes(cx, calendarId, calendar, eraYear, monthCode,
                                 daysInMonth, overflow);
    }

    case CalendarError::OutOfRange: {
      // ICU4X throws an out-of-range error if:
      // 1. Non-positive era years are given.
      // 2. Dates are before/after the requested named Japanese era.
      //
      // Case 1 doesn't happen for us, because we always pass strictly positive
      // era years, so this error must be for case 2.
      MOZ_ASSERT(calendarId == CalendarId::Japanese);
      MOZ_ASSERT(!CalendarEraStartsAtYearBoundary(calendarId, eraYear.era));

      if (overflow == TemporalOverflow::Reject) {
        ReportCalendarFieldOverflow(cx, "eraYear", eraYear.year);
        return nullptr;
      }

      auto result = CreateDateFromCodesConstrainToJapaneseEra(
          cx, calendarId, calendar, eraYear, monthCode, day);
      if (result.isOk()) {
        return result.unwrap();
      }
      break;
    }

    case CalendarError::Underflow:
    case CalendarError::UnknownEra:
      MOZ_ASSERT(false, "unexpected calendar error");
      break;

    case CalendarError::Generic:
      break;
  }

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
  return nullptr;
}

static UniqueICU4XDate CreateDateFrom(JSContext* cx, CalendarId calendarId,
                                      const capi::ICU4XCalendar* calendar,
                                      EraYear eraYear, int32_t month,
                                      int32_t day, TemporalOverflow overflow) {
  MOZ_ASSERT(calendarId != CalendarId::ISO8601);
  MOZ_ASSERT(month > 0);
  MOZ_ASSERT(day > 0);
  MOZ_ASSERT(month <= CalendarMonthsPerYear(calendarId));
  MOZ_ASSERT(day <= CalendarDaysInMonth(calendarId).second);

  switch (calendarId) {
    case CalendarId::ISO8601:
    case CalendarId::Buddhist:
    case CalendarId::Coptic:
    case CalendarId::Ethiopian:
    case CalendarId::EthiopianAmeteAlem:
    case CalendarId::Gregorian:
    case CalendarId::Indian:
    case CalendarId::Islamic:
    case CalendarId::IslamicCivil:
    case CalendarId::IslamicRGSA:
    case CalendarId::IslamicTabular:
    case CalendarId::IslamicUmmAlQura:
    case CalendarId::Japanese:
    case CalendarId::Persian:
    case CalendarId::ROC: {
      MOZ_ASSERT(!CalendarHasLeapMonths(calendarId));

      // Use the month-code corresponding to the ordinal month number for
      // calendar systems without leap months.
      auto date = CreateDateFromCodes(cx, calendarId, calendar, eraYear,
                                      MonthCode{month}, day, overflow);
      if (!date) {
        return nullptr;
      }
      MOZ_ASSERT_IF(
          CalendarEraStartsAtYearBoundary(calendarId),
          capi::ICU4XDate_ordinal_month(date.get()) == uint32_t(month));
      return date;
    }

    case CalendarId::Dangi:
    case CalendarId::Chinese: {
      static_assert(CalendarHasLeapMonths(CalendarId::Chinese));
      static_assert(CalendarMonthsPerYear(CalendarId::Chinese) == 13);
      static_assert(CalendarHasLeapMonths(CalendarId::Dangi));
      static_assert(CalendarMonthsPerYear(CalendarId::Dangi) == 13);

      MOZ_ASSERT(1 <= month && month <= 13);

      // Create date with month number replaced by month-code.
      auto monthCode = MonthCode{std::min(month, 12)};
      auto date = CreateDateFromCodes(cx, calendarId, calendar, eraYear,
                                      monthCode, day, overflow);
      if (!date) {
        return nullptr;
      }

      // If the ordinal month of |date| matches the input month, no additional
      // changes are necessary and we can directly return |date|.
      int32_t ordinal = capi::ICU4XDate_ordinal_month(date.get());
      if (ordinal == month) {
        return date;
      }

      // Otherwise we need to handle three cases:
      // 1. The input year contains a leap month and we need to adjust the
      //    month-code.
      // 2. The thirteenth month of a year without leap months was requested.
      // 3. The thirteenth month of a year with leap months was requested.
      if (ordinal > month) {
        MOZ_ASSERT(1 < month && month <= 12);

        // This case can only happen in leap years.
        MOZ_ASSERT(capi::ICU4XDate_months_in_year(date.get()) == 13);

        // Leap months can occur after any month in the Chinese calendar.
        //
        // Example when the fourth month is a leap month between M03 and M04.
        //
        // Month code:     M01  M02  M03  M03L  M04  M05  M06 ...
        // Ordinal month:  1    2    3    4     5    6    7

        // The month can be off by exactly one.
        MOZ_ASSERT((ordinal - month) == 1);

        // First try the case when the previous month isn't a leap month. This
        // case can only occur when |month > 2|, because otherwise we know that
        // "M01L" is the correct answer.
        if (month > 2) {
          auto previousMonthCode = MonthCode{month - 1};
          date = CreateDateFromCodes(cx, calendarId, calendar, eraYear,
                                     previousMonthCode, day, overflow);
          if (!date) {
            return nullptr;
          }

          int32_t ordinal = capi::ICU4XDate_ordinal_month(date.get());
          if (ordinal == month) {
            return date;
          }
        }

        // Fall-through when the previous month is a leap month.
      } else {
        MOZ_ASSERT(month == 13);
        MOZ_ASSERT(ordinal == 12);

        // Years with leap months contain thirteen months.
        if (capi::ICU4XDate_months_in_year(date.get()) != 13) {
          if (overflow == TemporalOverflow::Reject) {
            ReportCalendarFieldOverflow(cx, "month", month);
            return nullptr;
          }
          return date;
        }

        // Fall-through to return leap month "M12L" at the end of the year.
      }

      // Finally handle the case when the previous month is a leap month.
      auto leapMonthCode = MonthCode{month - 1, /* isLeapMonth= */ true};
      date = CreateDateFromCodes(cx, calendarId, calendar, eraYear,
                                 leapMonthCode, day, overflow);
      if (!date) {
        return nullptr;
      }
      MOZ_ASSERT(capi::ICU4XDate_ordinal_month(date.get()) == uint32_t(month),
                 "unexpected ordinal month");
      return date;
    }

    case CalendarId::Hebrew: {
      static_assert(CalendarHasLeapMonths(CalendarId::Hebrew));
      static_assert(CalendarMonthsPerYear(CalendarId::Hebrew) == 13);

      MOZ_ASSERT(1 <= month && month <= 13);

      // Create date with month number replaced by month-code.
      auto monthCode = MonthCode{std::min(month, 12)};
      auto date = CreateDateFromCodes(cx, calendarId, calendar, eraYear,
                                      monthCode, day, overflow);
      if (!date) {
        return nullptr;
      }

      // If the ordinal month of |date| matches the input month, no additional
      // changes are necessary and we can directly return |date|.
      int32_t ordinal = capi::ICU4XDate_ordinal_month(date.get());
      if (ordinal == month) {
        return date;
      }

      // Otherwise we need to handle two cases:
      // 1. The input year contains a leap month and we need to adjust the
      //    month-code.
      // 2. The thirteenth month of a year without leap months was requested.
      if (ordinal > month) {
        MOZ_ASSERT(1 < month && month <= 12);

        // This case can only happen in leap years.
        MOZ_ASSERT(capi::ICU4XDate_months_in_year(date.get()) == 13);

        // Leap months can occur between M05 and M06 in the Hebrew calendar.
        //
        // Month code:     M01  M02  M03  M04  M05  M05L  M06 ...
        // Ordinal month:  1    2    3    4    5    6     7

        // The month can be off by exactly one.
        MOZ_ASSERT((ordinal - month) == 1);
      } else {
        MOZ_ASSERT(month == 13);
        MOZ_ASSERT(ordinal == 12);

        if (overflow == TemporalOverflow::Reject) {
          ReportCalendarFieldOverflow(cx, "month", month);
          return nullptr;
        }
        return date;
      }

      // The previous month is the leap month Adar I iff |month| is six.
      bool isLeapMonth = month == 6;
      auto previousMonthCode = MonthCode{month - 1, isLeapMonth};
      date = CreateDateFromCodes(cx, calendarId, calendar, eraYear,
                                 previousMonthCode, day, overflow);
      if (!date) {
        return nullptr;
      }
      MOZ_ASSERT(capi::ICU4XDate_ordinal_month(date.get()) == uint32_t(month),
                 "unexpected ordinal month");
      return date;
    }
  }
  MOZ_CRASH("invalid calendar id");
}

static constexpr size_t ICUEraNameMaxLength() {
  size_t length = 0;
  for (auto calendar : AvailableCalendars()) {
    for (auto era : CalendarEras(calendar)) {
      auto name = IcuEraName(calendar, era);
      length = std::max(length, name.length());
    }
  }
  return length;
}

/**
 * Retrieve the era code from |date| and then map the returned ICU4X era code to
 * the corresponding |EraCode| member.
 */
static bool CalendarDateEra(JSContext* cx, CalendarId calendar,
                            const capi::ICU4XDate* date, EraCode* result) {
  MOZ_ASSERT(calendar != CalendarId::ISO8601);

  // Note: Assigning MaxLength to ICUEraNameMaxLength() breaks the CDT indexer.
  constexpr size_t MaxLength = 15;
  static_assert(MaxLength >= ICUEraNameMaxLength(),
                "Storage size is at least as large as the largest known era");

  // Storage for the largest known era string and the terminating NUL-character.
  char buf[MaxLength + 1] = {};
  auto writable = capi::diplomat_simple_writeable(buf, std::size(buf));

  if (!capi::ICU4XDate_era(date, &writable).is_ok) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
    return false;
  }
  MOZ_ASSERT(writable.buf == buf, "unexpected buffer relocation");

  auto dateEra = std::string_view{writable.buf, writable.len};

  // Map to era name to era code.
  for (auto era : CalendarEras(calendar)) {
    if (IcuEraName(calendar, era) == dateEra) {
      *result = era;
      return true;
    }
  }

  // Invalid/Unknown era name.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
  return false;
}

/**
 * Return the extended (non-era) year from |date|.
 */
static bool CalendarDateYear(JSContext* cx, CalendarId calendar,
                             const capi::ICU4XDate* date, int32_t* result) {
  MOZ_ASSERT(calendar != CalendarId::ISO8601);

  // FIXME: ICU4X doesn't yet support CalendarDateYear, so we need to manually
  // adjust the era year to determine the non-era year.
  //
  // https://github.com/unicode-org/icu4x/issues/3962

  if (!CalendarEraRelevant(calendar)) {
    int32_t year = capi::ICU4XDate_year_in_era(date);
    *result = year;
    return true;
  }

  if (calendar != CalendarId::Japanese) {
    MOZ_ASSERT(CalendarEras(calendar).size() == 2);

    int32_t year = capi::ICU4XDate_year_in_era(date);
    MOZ_ASSERT(year > 0, "era years are strictly positive in ICU4X");

    EraCode era;
    if (!CalendarDateEra(cx, calendar, date, &era)) {
      return false;
    }

    // Map from era year to extended year.
    //
    // For example in the Gregorian calendar:
    //
    // ----------------------------
    // | Era Year | Extended Year |
    // | 2 CE     |  2            |
    // | 1 CE     |  1            |
    // | 1 BCE    |  0            |
    // | 2 BCE    | -1            |
    // ----------------------------
    if (era == EraCode::Inverse) {
      year = -(year - 1);
    } else {
      MOZ_ASSERT(era == EraCode::Standard);
    }

    *result = year;
    return true;
  }

  // Japanese uses a proleptic Gregorian calendar, so we can use the ISO year.
  UniqueICU4XIsoDate isoDate{capi::ICU4XDate_to_iso(date)};
  int32_t isoYear = capi::ICU4XIsoDate_year(isoDate.get());

  *result = isoYear;
  return true;
}

/**
 * Retrieve the month code from |date| and then map the returned ICU4X month
 * code to the corresponding |MonthCode| member.
 */
static bool CalendarDateMonthCode(JSContext* cx, CalendarId calendar,
                                  const capi::ICU4XDate* date,
                                  MonthCode* result) {
  MOZ_ASSERT(calendar != CalendarId::ISO8601);

  // Valid month codes are "M01".."M13" and "M01L".."M12L".
  constexpr size_t MaxLength =
      std::string_view{MonthCode::maxLeapMonth()}.length();
  static_assert(
      MaxLength > std::string_view{MonthCode::maxNonLeapMonth()}.length(),
      "string representation of max-leap month is larger");

  // Storage for the largest valid month code and the terminating NUL-character.
  char buf[MaxLength + 1] = {};
  auto writable = capi::diplomat_simple_writeable(buf, std::size(buf));

  if (!capi::ICU4XDate_month_code(date, &writable).is_ok) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
    return false;
  }
  MOZ_ASSERT(writable.buf == buf, "unexpected buffer relocation");

  auto view = std::string_view{writable.buf, writable.len};

  MOZ_ASSERT(view.length() >= 3);
  MOZ_ASSERT(view[0] == 'M');
  MOZ_ASSERT(mozilla::IsAsciiDigit(view[1]));
  MOZ_ASSERT(mozilla::IsAsciiDigit(view[2]));
  MOZ_ASSERT_IF(view.length() > 3, view[3] == 'L');

  int32_t ordinal =
      AsciiDigitToNumber(view[1]) * 10 + AsciiDigitToNumber(view[2]);
  bool isLeapMonth = view.length() > 3;
  auto monthCode = MonthCode{ordinal, isLeapMonth};

  static constexpr auto IrregularAdarII =
      MonthCode{6, /* isLeapMonth = */ true};
  static constexpr auto RegularAdarII = MonthCode{6};

  // Handle the irregular month code "M06L" for Adar II in leap years.
  //
  // https://docs.rs/icu/latest/icu/calendar/hebrew/struct.Hebrew.html#month-codes
  if (calendar == CalendarId::Hebrew && monthCode == IrregularAdarII) {
    monthCode = RegularAdarII;
  }

  // The month code must be valid for this calendar.
  MOZ_ASSERT(CalendarMonthCodes(calendar).contains(monthCode));

  *result = monthCode;
  return true;
}

class MonthCodeString {
  // Zero-terminated month code string.
  char str_[4 + 1];

 public:
  explicit MonthCodeString(MonthCodeField field) {
    str_[0] = 'M';
    str_[1] = char('0' + (field.ordinal() / 10));
    str_[2] = char('0' + (field.ordinal() % 10));
    str_[3] = field.isLeapMonth() ? 'L' : '\0';
    str_[4] = '\0';
  }

  const char* toCString() const { return str_; }
};

/**
 * CalendarResolveFields ( calendar, fields, type )
 */
static bool ISOCalendarResolveMonth(JSContext* cx,
                                    Handle<CalendarFields> fields,
                                    double* result) {
  double month = fields.month();
  MOZ_ASSERT_IF(fields.has(CalendarField::Month),
                IsInteger(month) && month > 0);

  // CalendarResolveFields, steps 1.e.
  if (!fields.has(CalendarField::MonthCode)) {
    MOZ_ASSERT(fields.has(CalendarField::Month));

    *result = month;
    return true;
  }

  auto monthCode = fields.monthCode();

  // CalendarResolveFields, steps 1.f-k.
  int32_t ordinal = monthCode.ordinal();
  if (ordinal < 1 || ordinal > 12 || monthCode.isLeapMonth()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                             MonthCodeString{monthCode}.toCString());
    return false;
  }

  // CalendarResolveFields, steps 1.l-m.
  if (fields.has(CalendarField::Month) && month != ordinal) {
    ToCStringBuf cbuf;
    const char* monthStr = NumberToCString(&cbuf, month);

    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE_MONTHCODE,
                             MonthCodeString{monthCode}.toCString(), monthStr);
    return false;
  }

  // CalendarResolveFields, steps 1.n.
  *result = ordinal;
  return true;
}

struct EraYears {
  // Year starting from the calendar epoch.
  mozilla::Maybe<EraYear> fromEpoch;

  // Year starting from a specific calendar era.
  mozilla::Maybe<EraYear> fromEra;
};

/**
 * CalendarResolveFields ( calendar, fields, type )
 * CalendarDateToISO ( calendar, fields, overflow )
 * CalendarMonthDayToISOReferenceDate ( calendar, fields, overflow )
 *
 * Extract `year` and `eraYear` from |fields| and perform some initial
 * validation to ensure the values are valid for the requested calendar.
 */
static bool CalendarFieldYear(JSContext* cx, CalendarId calendar,
                              Handle<CalendarFields> fields, EraYears* result) {
  MOZ_ASSERT(fields.has(CalendarField::Year) ||
             fields.has(CalendarField::EraYear));

  // |eraYear| is to be ignored when not relevant for |calendar| per
  // CalendarResolveFields.
  bool hasRelevantEra =
      fields.has(CalendarField::Era) && CalendarEraRelevant(calendar);
  MOZ_ASSERT_IF(fields.has(CalendarField::Era), CalendarEraRelevant(calendar));

  // Case 1: |year| field is present.
  mozilla::Maybe<EraYear> fromEpoch;
  if (fields.has(CalendarField::Year)) {
    double year = fields.year();
    MOZ_ASSERT(IsInteger(year));

    int32_t intYear;
    if (!mozilla::NumberEqualsInt32(year, &intYear)) {
      ReportCalendarFieldOverflow(cx, "year", year);
      return false;
    }

    fromEpoch = mozilla::Some(CalendarEraYear(calendar, intYear));
  } else {
    MOZ_ASSERT(hasRelevantEra);
  }

  // Case 2: |era| and |eraYear| fields are present and relevant for |calendar|.
  mozilla::Maybe<EraYear> fromEra;
  if (hasRelevantEra) {
    MOZ_ASSERT(fields.has(CalendarField::Era));
    MOZ_ASSERT(fields.has(CalendarField::EraYear));

    auto era = fields.era();
    MOZ_ASSERT(era);

    double eraYear = fields.eraYear();
    MOZ_ASSERT(IsInteger(eraYear));

    auto* linearEra = era->ensureLinear(cx);
    if (!linearEra) {
      return false;
    }

    // Ensure the requested era is valid for |calendar|.
    auto eraCode = EraForString(calendar, linearEra);
    if (!eraCode) {
      if (auto code = QuoteString(cx, era)) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_TEMPORAL_CALENDAR_INVALID_ERA,
                                 code.get());
      }
      return false;
    }

    int32_t intEraYear;
    if (!mozilla::NumberEqualsInt32(eraYear, &intEraYear)) {
      ReportCalendarFieldOverflow(cx, "eraYear", eraYear);
      return false;
    }

    fromEra = mozilla::Some(EraYear{*eraCode, intEraYear});
  }

  *result = {fromEpoch, fromEra};
  return true;
}

struct Month {
  // Month code.
  MonthCode code;

  // Ordinal month number.
  int32_t ordinal = 0;
};

/**
 * CalendarResolveFields ( calendar, fields, type )
 * CalendarDateToISO ( calendar, fields, overflow )
 * CalendarMonthDayToISOReferenceDate ( calendar, fields, overflow )
 *
 * Extract `month` and `monthCode` from |fields| and perform some initial
 * validation to ensure the values are valid for the requested calendar.
 */
static bool CalendarFieldMonth(JSContext* cx, CalendarId calendar,
                               Handle<CalendarFields> fields,
                               TemporalOverflow overflow, Month* result) {
  MOZ_ASSERT(fields.has(CalendarField::Month) ||
             fields.has(CalendarField::MonthCode));

  // Case 1: |month| field is present.
  int32_t intMonth = 0;
  if (fields.has(CalendarField::Month)) {
    double month = fields.month();
    MOZ_ASSERT(IsInteger(month) && month > 0);

    if (!mozilla::NumberEqualsInt32(month, &intMonth)) {
      intMonth = 0;
    }

    const int32_t monthsPerYear = CalendarMonthsPerYear(calendar);
    if (intMonth < 1 || intMonth > monthsPerYear) {
      if (overflow == TemporalOverflow::Reject) {
        ReportCalendarFieldOverflow(cx, "month", month);
        return false;
      }
      MOZ_ASSERT(overflow == TemporalOverflow::Constrain);

      intMonth = monthsPerYear;
    }

    MOZ_ASSERT(intMonth > 0);
  }

  // Case 2: |monthCode| field is present.
  MonthCode fromMonthCode;
  if (fields.has(CalendarField::MonthCode)) {
    auto monthCode = fields.monthCode();
    int32_t ordinal = monthCode.ordinal();
    bool isLeapMonth = monthCode.isLeapMonth();

    constexpr int32_t minMonth = MonthCode{1}.ordinal();
    constexpr int32_t maxNonLeapMonth = MonthCode::maxNonLeapMonth().ordinal();
    constexpr int32_t maxLeapMonth = MonthCode::maxLeapMonth().ordinal();

    // Minimum month number is 1. Maximum month is 12 (or 13 when the calendar
    // uses epagomenal months).
    const int32_t maxMonth = isLeapMonth ? maxLeapMonth : maxNonLeapMonth;
    if (minMonth <= ordinal && ordinal <= maxMonth) {
      fromMonthCode = MonthCode{ordinal, isLeapMonth};
    }

    // Ensure the month code is valid for this calendar.
    const auto& monthCodes = CalendarMonthCodes(calendar);
    if (!monthCodes.contains(fromMonthCode)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                               MonthCodeString{monthCode}.toCString());
      return false;
    }
  }

  *result = {fromMonthCode, intMonth};
  return true;
}

/**
 * CalendarResolveFields ( calendar, fields, type )
 * CalendarDateToISO ( calendar, fields, overflow )
 * CalendarMonthDayToISOReferenceDate ( calendar, fields, overflow )
 *
 * Extract `day` from |fields| and perform some initial validation to ensure the
 * value is valid for the requested calendar.
 */
static bool CalendarFieldDay(JSContext* cx, CalendarId calendar,
                             Handle<CalendarFields> fields,
                             TemporalOverflow overflow, int32_t* result) {
  MOZ_ASSERT(fields.has(CalendarField::Day));

  double day = fields.day();
  MOZ_ASSERT(IsInteger(day) && day > 0);

  int32_t intDay;
  if (!mozilla::NumberEqualsInt32(day, &intDay)) {
    intDay = 0;
  }

  // Constrain to a valid day value in this calendar.
  int32_t daysPerMonth = CalendarDaysInMonth(calendar).second;
  if (intDay < 1 || intDay > daysPerMonth) {
    if (overflow == TemporalOverflow::Reject) {
      ReportCalendarFieldOverflow(cx, "day", day);
      return false;
    }
    MOZ_ASSERT(overflow == TemporalOverflow::Constrain);

    intDay = daysPerMonth;
  }

  *result = intDay;
  return true;
}

/**
 * CalendarResolveFields ( calendar, fields, type )
 *
 * > The operation throws a TypeError exception if the properties of fields are
 * > internally inconsistent within the calendar [...]. For example:
 * >
 * > [...] The values for "era" and "eraYear" do not together identify the same
 * > year as the value for "year".
 */
static bool CalendarFieldEraYearMatchesYear(JSContext* cx, CalendarId calendar,
                                            Handle<CalendarFields> fields,
                                            const capi::ICU4XDate* date) {
  MOZ_ASSERT(fields.has(CalendarField::EraYear));
  MOZ_ASSERT(fields.has(CalendarField::Year));

  double year = fields.year();
  MOZ_ASSERT(IsInteger(year));

  int32_t intYear;
  MOZ_ALWAYS_TRUE(mozilla::NumberEqualsInt32(year, &intYear));

  int32_t yearFromEraYear;
  if (!CalendarDateYear(cx, calendar, date, &yearFromEraYear)) {
    return false;
  }

  // The user requested year must match the actual (extended/epoch) year.
  if (intYear != yearFromEraYear) {
    ToCStringBuf yearCbuf;
    const char* yearStr = NumberToCString(&yearCbuf, intYear);

    ToCStringBuf fromEraCbuf;
    const char* fromEraStr = NumberToCString(&fromEraCbuf, yearFromEraYear);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE_YEAR,
                              yearStr, fromEraStr);
    return false;
  }
  return true;
}

/**
 * CalendarResolveFields ( calendar, fields, type )
 *
 * > The operation throws a TypeError exception if the properties of fields are
 * > internally inconsistent within the calendar [...]. For example:
 * >
 * > If "month" and "monthCode" in the calendar [...] do not identify the same
 * > month.
 */
static bool CalendarFieldMonthCodeMatchesMonth(JSContext* cx,
                                               Handle<CalendarFields> fields,
                                               const capi::ICU4XDate* date,
                                               int32_t month) {
  int32_t ordinal = capi::ICU4XDate_ordinal_month(date);

  // The user requested month must match the actual ordinal month.
  if (month != ordinal) {
    ToCStringBuf cbuf;
    const char* monthStr = NumberToCString(&cbuf, fields.month());

    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE_MONTHCODE,
                             MonthCodeString{fields.monthCode()}.toCString(),
                             monthStr);
    return false;
  }
  return true;
}

static PlainDate ToPlainDate(const capi::ICU4XDate* date) {
  UniqueICU4XIsoDate isoDate{capi::ICU4XDate_to_iso(date)};

  int32_t isoYear = capi::ICU4XIsoDate_year(isoDate.get());

  int32_t isoMonth = capi::ICU4XIsoDate_month(isoDate.get());
  MOZ_ASSERT(1 <= isoMonth && isoMonth <= 12);

  int32_t isoDay = capi::ICU4XIsoDate_day_of_month(isoDate.get());
  MOZ_ASSERT(1 <= isoDay && isoDay <= ::ISODaysInMonth(isoYear, isoMonth));

  return {isoYear, isoMonth, isoDay};
}

static UniqueICU4XDate CreateDateFrom(JSContext* cx, CalendarId calendar,
                                      const capi::ICU4XCalendar* cal,
                                      const EraYears& eraYears,
                                      const Month& month, int32_t day,
                                      Handle<CalendarFields> fields,
                                      TemporalOverflow overflow) {
  // Use |eraYear| if present, so we can more easily check for consistent
  // |year| and |eraYear| fields.
  auto eraYear = eraYears.fromEra ? *eraYears.fromEra : *eraYears.fromEpoch;

  UniqueICU4XDate date;
  if (month.code != MonthCode{}) {
    date = CreateDateFromCodes(cx, calendar, cal, eraYear, month.code, day,
                               overflow);
  } else {
    date = CreateDateFrom(cx, calendar, cal, eraYear, month.ordinal, day,
                          overflow);
  }
  if (!date) {
    return nullptr;
  }

  // |year| and |eraYear| must be consistent.
  if (eraYears.fromEpoch && eraYears.fromEra) {
    if (!CalendarFieldEraYearMatchesYear(cx, calendar, fields, date.get())) {
      return nullptr;
    }
  }

  // |month| and |monthCode| must be consistent.
  if (month.code != MonthCode{} && month.ordinal > 0) {
    if (!CalendarFieldMonthCodeMatchesMonth(cx, fields, date.get(),
                                            month.ordinal)) {
      return nullptr;
    }
  }

  return date;
}

/**
 * CalendarDateToISO ( calendar, fields, overflow )
 */
static bool CalendarDateToISO(JSContext* cx, CalendarId calendar,
                              Handle<CalendarFields> fields,
                              TemporalOverflow overflow, PlainDate* result) {
  // Step 1.
  if (calendar == CalendarId::ISO8601) {
    // Step 1.a.
    MOZ_ASSERT(fields.has(CalendarField::Year));
    MOZ_ASSERT(fields.has(CalendarField::Month) ||
               fields.has(CalendarField::MonthCode));
    MOZ_ASSERT(fields.has(CalendarField::Day));

    // Remaining steps from CalendarResolveFields to resolve the month.
    double month;
    if (!ISOCalendarResolveMonth(cx, fields, &month)) {
      return false;
    }

    // Step 1.b.
    RegulatedISODate regulated;
    if (!RegulateISODate(cx, fields.year(), month, fields.day(), overflow,
                         &regulated)) {
      return false;
    }

    int32_t intYear;
    if (!mozilla::NumberEqualsInt32(regulated.year, &intYear)) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
      return false;
    }

    *result = PlainDate{intYear, regulated.month, regulated.day};
    return true;
  }

  // Step 2.

  EraYears eraYears;
  if (!CalendarFieldYear(cx, calendar, fields, &eraYears)) {
    return false;
  }

  Month month;
  if (!CalendarFieldMonth(cx, calendar, fields, overflow, &month)) {
    return false;
  }

  int32_t day;
  if (!CalendarFieldDay(cx, calendar, fields, overflow, &day)) {
    return false;
  }

  auto cal = CreateICU4XCalendar(cx, calendar);
  if (!cal) {
    return false;
  }

  auto date = CreateDateFrom(cx, calendar, cal.get(), eraYears, month, day,
                             fields, overflow);
  if (!date) {
    return false;
  }

  *result = ToPlainDate(date.get());
  return true;
}

/**
 * CalendarMonthDayToISOReferenceDate ( calendar, fields, overflow )
 */
static bool CalendarMonthDayToISOReferenceDate(JSContext* cx,
                                               CalendarId calendar,
                                               Handle<CalendarFields> fields,
                                               TemporalOverflow overflow,
                                               PlainDate* result) {
  // Step 1.
  if (calendar == CalendarId::ISO8601) {
    // Step 1.a.
    MOZ_ASSERT(fields.has(CalendarField::Month) ||
               fields.has(CalendarField::MonthCode));
    MOZ_ASSERT(fields.has(CalendarField::Day));

    // Remaining steps from CalendarResolveFields to resolve the month.
    double month;
    if (!ISOCalendarResolveMonth(cx, fields, &month)) {
      return false;
    }

    // Step 1.b.
    int32_t referenceISOYear = 1972;

    // Step 1.c.
    double year =
        !fields.has(CalendarField::Year) ? referenceISOYear : fields.year();

    // Step 1.d.
    RegulatedISODate regulated;
    if (!RegulateISODate(cx, year, month, fields.day(), overflow, &regulated)) {
      return false;
    }

    // Step 1.e.
    *result = {referenceISOYear, regulated.month, regulated.day};
    return true;
  }

  // Step 2.

  EraYears eraYears;
  if (fields.has(CalendarField::Year) || fields.has(CalendarField::EraYear)) {
    if (!CalendarFieldYear(cx, calendar, fields, &eraYears)) {
      return false;
    }
  } else {
    MOZ_ASSERT(fields.has(CalendarField::MonthCode));
  }

  Month month;
  if (!CalendarFieldMonth(cx, calendar, fields, overflow, &month)) {
    return false;
  }

  int32_t day;
  if (!CalendarFieldDay(cx, calendar, fields, overflow, &day)) {
    return false;
  }

  auto cal = CreateICU4XCalendar(cx, calendar);
  if (!cal) {
    return false;
  }

  // We first have to compute the month-code if it wasn't provided to us.
  auto monthCode = month.code;
  if (fields.has(CalendarField::Year) || fields.has(CalendarField::EraYear)) {
    auto date = CreateDateFrom(cx, calendar, cal.get(), eraYears, month, day,
                               fields, overflow);
    if (!date) {
      return false;
    }

    if (!fields.has(CalendarField::MonthCode)) {
      if (!CalendarDateMonthCode(cx, calendar, date.get(), &monthCode)) {
        return false;
      }
    }
  }
  MOZ_ASSERT(monthCode != MonthCode{});

  // Try years starting from 31 December, 1972.
  constexpr auto isoReferenceDate = PlainDate{1972, 12, 31};

  auto fromIsoDate = CreateICU4XDate(cx, isoReferenceDate, cal.get());
  if (!fromIsoDate) {
    return false;
  }

  // Find the calendar year for the ISO reference date.
  int32_t calendarYear;
  if (!CalendarDateYear(cx, calendar, fromIsoDate.get(), &calendarYear)) {
    return false;
  }

  // Constrain day to maximum possible day for the input month.
  int32_t daysInMonth = CalendarDaysInMonth(calendar, monthCode).second;
  if (overflow == TemporalOverflow::Constrain) {
    day = std::min(day, daysInMonth);
  } else {
    MOZ_ASSERT(overflow == TemporalOverflow::Reject);

    if (day > daysInMonth) {
      ReportCalendarFieldOverflow(cx, "day", day);
      return false;
    }
  }

  // 10'000 is sufficient to find all possible month-days, even for rare cases
  // like `{calendar: "chinese", monthCode: "M09L", day: 30}`.
  constexpr size_t maxIterations = 10'000;

  UniqueICU4XDate date;
  for (size_t i = 0; i < maxIterations; i++) {
    // This loop can run for a long time.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    auto candidateYear = CalendarEraYear(calendar, calendarYear);

    auto result =
        CreateDateFromCodes(calendar, cal.get(), candidateYear, monthCode, day);
    if (result.isOk()) {
      // Make sure the resolved date is before December 31, 1972.
      auto plainDate = ToPlainDate(result.inspect().get());
      if (plainDate.year > isoReferenceDate.year) {
        calendarYear -= 1;
        continue;
      }

      date = result.unwrap();
      break;
    }

    switch (result.inspectErr()) {
      case CalendarError::UnknownMonthCode: {
        MOZ_ASSERT(CalendarHasLeapMonths(calendar));
        MOZ_ASSERT(monthCode.isLeapMonth());

        // Try the next candidate year if the requested leap month doesn't
        // occur in the current year.
        calendarYear -= 1;
        continue;
      }

      case CalendarError::Overflow: {
        // ICU4X throws an overflow error when:
        // 1. month > monthsInYear(year), or
        // 2. days > daysInMonthOf(year, month).
        //
        // Case 1 can't happen for month-codes, so it doesn't apply here.
        // Case 2 can only happen when |day| is larger than the minimum number
        // of days in the month.
        MOZ_ASSERT(day > CalendarDaysInMonth(calendar, monthCode).first);

        // Try next candidate year to find an earlier year which can fulfill
        // the input request.
        calendarYear -= 1;
        continue;
      }

      case CalendarError::OutOfRange:
      case CalendarError::Underflow:
      case CalendarError::UnknownEra:
        MOZ_ASSERT(false, "unexpected calendar error");
        break;

      case CalendarError::Generic:
        break;
    }

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
    return false;
  }

  // We shouldn't end up here with |maxIterations == 10'000|, but just in case
  // still handle this case and report an error.
  if (!date) {
    ReportCalendarFieldOverflow(cx, "day", day);
    return false;
  }

  // |month| and |monthCode| must be consistent.
  if (month.code != MonthCode{} && month.ordinal > 0) {
    if (!CalendarFieldMonthCodeMatchesMonth(cx, fields, date.get(),
                                            month.ordinal)) {
      return false;
    }
  }

  *result = ToPlainDate(date.get());
  return true;
}

enum class FieldType { Date, YearMonth, MonthDay };

/**
 * CalendarResolveFields ( calendar, fields, type )
 */
static bool CalendarResolveFields(JSContext* cx, CalendarId calendar,
                                  Handle<CalendarFields> fields,
                                  FieldType type) {
  // Step 1.
  if (calendar == CalendarId::ISO8601) {
    // Steps 1.a-e.
    const char* missingField = nullptr;
    if ((type == FieldType::Date || type == FieldType::YearMonth) &&
        !fields.has(CalendarField::Year)) {
      missingField = "year";
    } else if ((type == FieldType::Date || type == FieldType::MonthDay) &&
               !fields.has(CalendarField::Day)) {
      missingField = "day";
    } else if (!fields.has(CalendarField::MonthCode) &&
               !fields.has(CalendarField::Month)) {
      missingField = "month";
    }

    if (missingField) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_CALENDAR_MISSING_FIELD,
                                missingField);
      return false;
    }

    // Steps 1.f-n. (Handled in ISOCalendarResolveMonth.)

    return true;
  }

  // Step 2.

  // Date and Month-Day require |day| to be present.
  bool requireDay = type == FieldType::Date || type == FieldType::MonthDay;

  // Date and Year-Month require |year| (or |eraYear|) to be present.
  // Month-Day requires |year| (or |eraYear|) if |monthCode| is absent.
  bool requireYear = type == FieldType::Date || type == FieldType::YearMonth ||
                     !fields.has(CalendarField::MonthCode);

  // Determine if any calendar fields are missing.
  const char* missingField = nullptr;
  if (!fields.has(CalendarField::MonthCode) &&
      !fields.has(CalendarField::Month)) {
    // |monthCode| or |month| must be present.
    missingField = "monthCode";
  } else if (requireDay && !fields.has(CalendarField::Day)) {
    missingField = "day";
  } else if (!CalendarEraRelevant(calendar)) {
    if (requireYear && !fields.has(CalendarField::Year)) {
      missingField = "year";
    }
  } else {
    if (fields.has(CalendarField::Era) != fields.has(CalendarField::EraYear)) {
      // |era| and |eraYear| must either both be present or both absent.
      missingField = fields.has(CalendarField::Era) ? "eraYear" : "era";
    } else if (requireYear && !fields.has(CalendarField::EraYear) &&
               !fields.has(CalendarField::Year)) {
      missingField = "eraYear";
    }
  }

  if (missingField) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_MISSING_FIELD,
                              missingField);
    return false;
  }

  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[Era]] field.
 */
bool js::temporal::CalendarEra(JSContext* cx, Handle<CalendarValue> calendar,
                               const PlainDate& date,
                               MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setUndefined();
    return true;
  }

  // Step 2.
  if (!CalendarEraRelevant(calendarId)) {
    result.setUndefined();
    return true;
  }

  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  EraCode era;
  if (!CalendarDateEra(cx, calendarId, dt.get(), &era)) {
    return false;
  }

  auto* str = NewStringCopy<CanGC>(cx, CalendarEraName(calendarId, era));
  if (!str) {
    return false;
  }

  result.setString(str);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[EraYear]] field.
 */
bool js::temporal::CalendarEraYear(JSContext* cx,
                                   Handle<CalendarValue> calendar,
                                   const PlainDate& date,
                                   MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setUndefined();
    return true;
  }

  // Step 2.
  if (!CalendarEraRelevant(calendarId)) {
    result.setUndefined();
    return true;
  }

  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  int32_t year = capi::ICU4XDate_year_in_era(dt.get());
  result.setInt32(year);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[Year]] field.
 */
bool js::temporal::CalendarYear(JSContext* cx, Handle<CalendarValue> calendar,
                                const PlainDate& date,
                                MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(date.year);
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  int32_t year;
  if (!CalendarDateYear(cx, calendarId, dt.get(), &year)) {
    return false;
  }

  result.setInt32(year);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[Month]] field.
 */
bool js::temporal::CalendarMonth(JSContext* cx, Handle<CalendarValue> calendar,
                                 const PlainDate& date,
                                 MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(date.month);
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  int32_t month = capi::ICU4XDate_ordinal_month(dt.get());
  result.setInt32(month);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[MonthCode]] field.
 */
bool js::temporal::CalendarMonthCode(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     const PlainDate& date,
                                     MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    // Steps 1.a-b.
    auto monthCode = MonthCode{date.month};
    JSString* str = NewStringCopy<CanGC>(cx, std::string_view{monthCode});
    if (!str) {
      return false;
    }

    result.setString(str);
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  MonthCode monthCode;
  if (!CalendarDateMonthCode(cx, calendarId, dt.get(), &monthCode)) {
    return false;
  }

  auto* str = NewStringCopy<CanGC>(cx, std::string_view{monthCode});
  if (!str) {
    return false;
  }

  result.setString(str);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[Day]] field.
 */
bool js::temporal::CalendarDay(JSContext* cx, Handle<CalendarValue> calendar,
                               const PlainDate& date,
                               MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(date.day);
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  int32_t day = capi::ICU4XDate_day_of_month(dt.get());
  result.setInt32(day);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[DayOfWeek]] field.
 */
bool js::temporal::CalendarDayOfWeek(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     const PlainDate& date,
                                     MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(ToISODayOfWeek(date));
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  // Week day codes are correctly ordered.
  static_assert(capi::ICU4XIsoWeekday_Monday == 1);
  static_assert(capi::ICU4XIsoWeekday_Tuesday == 2);
  static_assert(capi::ICU4XIsoWeekday_Wednesday == 3);
  static_assert(capi::ICU4XIsoWeekday_Thursday == 4);
  static_assert(capi::ICU4XIsoWeekday_Friday == 5);
  static_assert(capi::ICU4XIsoWeekday_Saturday == 6);
  static_assert(capi::ICU4XIsoWeekday_Sunday == 7);

  capi::ICU4XIsoWeekday day = capi::ICU4XDate_day_of_week(dt.get());
  result.setInt32(static_cast<int32_t>(day));
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[DayOfYear]] field.
 */
bool js::temporal::CalendarDayOfYear(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     const PlainDate& date,
                                     MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(ToISODayOfYear(date));
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  // Workaround for https://github.com/unicode-org/icu4x/issues/5655
  if (calendarId == CalendarId::Japanese) {
    // Use the extended year instead of the era year to correctly handle the
    // case when the era changes in the current year. This can happen in the
    // Japanese calendar.
    int32_t year;
    if (!CalendarDateYear(cx, calendarId, dt.get(), &year)) {
      return false;
    }
    auto eraYear = CalendarEraYear(calendarId, year);

    int32_t dayOfYear = capi::ICU4XDate_day_of_month(dt.get());
    int32_t month = capi::ICU4XDate_ordinal_month(dt.get());

    // Add the number of days of all preceding months to compute the overall day
    // of the year.
    while (month > 1) {
      auto previousMonth = CreateDateFrom(cx, calendarId, cal.get(), eraYear,
                                          --month, 1, TemporalOverflow::Reject);
      if (!previousMonth) {
        return false;
      }

      dayOfYear += capi::ICU4XDate_days_in_month(previousMonth.get());
    }

    MOZ_ASSERT(dayOfYear <= capi::ICU4XDate_days_in_year(dt.get()));

    result.setInt32(dayOfYear);
    return true;
  }

  int32_t day = capi::ICU4XDate_day_of_year(dt.get());
  result.setInt32(day);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[WeekOfYear]].[[Week]] field.
 */
bool js::temporal::CalendarWeekOfYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDate& date,
                                      MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(ToISOWeekOfYear(date).week);
    return true;
  }

  // Step 2.

  // Non-Gregorian calendars don't get week-of-year support for now.
  //
  // https://github.com/tc39/proposal-intl-era-monthcode/issues/15
  if (calendarId != CalendarId::Gregorian) {
    result.setUndefined();
    return true;
  }

  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  auto weekCal = CreateICU4WeekCalculator(cx, calendarId);
  if (!weekCal) {
    return false;
  }

  auto week = capi::ICU4XDate_week_of_year(dt.get(), weekCal.get());
  if (!week.is_ok) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
    return false;
  }

  result.setInt32(week.ok.week);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[WeekOfYear]].[[Year]] field.
 */
bool js::temporal::CalendarYearOfWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDate& date,
                                      MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(ToISOWeekOfYear(date).year);
    return true;
  }

  // Step 2.

  // Non-Gregorian calendars don't get week-of-year support for now.
  //
  // https://github.com/tc39/proposal-intl-era-monthcode/issues/15
  if (calendarId != CalendarId::Gregorian) {
    result.setUndefined();
    return true;
  }

  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  auto weekCal = CreateICU4WeekCalculator(cx, calendarId);
  if (!weekCal) {
    return false;
  }

  auto week = capi::ICU4XDate_week_of_year(dt.get(), weekCal.get());
  if (!week.is_ok) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_CALENDAR_INTERNAL_ERROR);
    return false;
  }

  int32_t relative = 0;
  switch (week.ok.unit) {
    case capi::ICU4XWeekRelativeUnit_Previous:
      relative = -1;
      break;
    case capi::ICU4XWeekRelativeUnit_Current:
      relative = 0;
      break;
    case capi::ICU4XWeekRelativeUnit_Next:
      relative = 1;
      break;
  }

  int32_t calendarYear;
  if (!CalendarDateYear(cx, calendarId, dt.get(), &calendarYear)) {
    return false;
  }

  result.setInt32(calendarYear + relative);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[DaysInWeek]] field.
 */
bool js::temporal::CalendarDaysInWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDate& date,
                                      MutableHandle<Value> result) {
  // All supported ICU4X calendars use a 7-day week and so does the ISO 8601
  // calendar.
  //
  // This function isn't supported through the ICU4X FFI, so we have to
  // hardcode the result.

  // Step 1-2.
  result.setInt32(7);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[DaysInMonth]] field.
 */
bool js::temporal::CalendarDaysInMonth(JSContext* cx,
                                       Handle<CalendarValue> calendar,
                                       const PlainDate& date,
                                       MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(::ISODaysInMonth(date.year, date.month));
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  int32_t days = capi::ICU4XDate_days_in_month(dt.get());
  result.setInt32(days);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[DaysInYear]] field.
 */
bool js::temporal::CalendarDaysInYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDate& date,
                                      MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(ISODaysInYear(date.year));
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  int32_t days = capi::ICU4XDate_days_in_year(dt.get());
  result.setInt32(days);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[MonthsInYear]] field.
 */
bool js::temporal::CalendarMonthsInYear(JSContext* cx,
                                        Handle<CalendarValue> calendar,
                                        const PlainDate& date,
                                        MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setInt32(12);
    return true;
  }

  // Step 2
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  int32_t months = capi::ICU4XDate_months_in_year(dt.get());
  result.setInt32(months);
  return true;
}

/**
 * CalendarISOToDate ( calendar, isoDate )
 *
 * Return the Calendar Date Record's [[InLeapYear]] field.
 */
bool js::temporal::CalendarInLeapYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDate& date,
                                      MutableHandle<Value> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (calendarId == CalendarId::ISO8601) {
    result.setBoolean(IsISOLeapYear(date.year));
    return true;
  }

  // Step 2.

  // FIXME: Not supported in ICU4X.
  //
  // https://github.com/unicode-org/icu4x/issues/5654

  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  bool inLeapYear = false;
  switch (calendarId) {
    case CalendarId::ISO8601:
    case CalendarId::Buddhist:
    case CalendarId::Gregorian:
    case CalendarId::Japanese:
    case CalendarId::Coptic:
    case CalendarId::Ethiopian:
    case CalendarId::EthiopianAmeteAlem:
    case CalendarId::Indian:
    case CalendarId::Persian:
    case CalendarId::ROC: {
      MOZ_ASSERT(!CalendarHasLeapMonths(calendarId));

      // Solar calendars have either 365 or 366 days per year.
      int32_t days = capi::ICU4XDate_days_in_year(dt.get());
      MOZ_ASSERT(days == 365 || days == 366);

      // Leap years have 366 days.
      inLeapYear = days == 366;
      break;
    }

    case CalendarId::Islamic:
    case CalendarId::IslamicCivil:
    case CalendarId::IslamicRGSA:
    case CalendarId::IslamicTabular:
    case CalendarId::IslamicUmmAlQura: {
      MOZ_ASSERT(!CalendarHasLeapMonths(calendarId));

      // Lunar Islamic calendars have either 354 or 355 days per year.
      //
      // Allow 353 days to workaround
      // <https://github.com/unicode-org/icu4x/issues/4930>.
      int32_t days = capi::ICU4XDate_days_in_year(dt.get());
      MOZ_ASSERT(days == 353 || days == 354 || days == 355);

      // Leap years have 355 days.
      inLeapYear = days == 355;
      break;
    }

    case CalendarId::Chinese:
    case CalendarId::Dangi:
    case CalendarId::Hebrew: {
      MOZ_ASSERT(CalendarHasLeapMonths(calendarId));

      // Calendars with separate leap months have either 12 or 13 months per
      // year.
      int32_t months = capi::ICU4XDate_months_in_year(dt.get());
      MOZ_ASSERT(months == 12 || months == 13);

      // Leap years have 13 months.
      inLeapYear = months == 13;
      break;
    }
  }

  result.setBoolean(inLeapYear);
  return true;
}

enum class DateFieldType { Date, YearMonth, MonthDay };

/**
 * ISODateToFields ( calendar, isoDate, type )
 */
static bool ISODateToFields(JSContext* cx, Handle<CalendarValue> calendar,
                            const PlainDate& date, DateFieldType type,
                            MutableHandle<CalendarFields> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  result.set(CalendarFields{});

  // Steps 2-6. (Optimization for the ISO 8601 calendar.)
  if (calendarId == CalendarId::ISO8601) {
    // Step 2. (Not applicable in our implementation.)

    // Step 3.
    result.setMonthCode(MonthCode{date.month});

    // Step 4.
    if (type == DateFieldType::MonthDay || type == DateFieldType::Date) {
      result.setDay(date.day);
    }

    // Step 5.
    if (type == DateFieldType::YearMonth || type == DateFieldType::Date) {
      result.setYear(date.year);
    }

    // Step 6.
    return true;
  }

  // Step 2.
  auto cal = CreateICU4XCalendar(cx, calendarId);
  if (!cal) {
    return false;
  }

  auto dt = CreateICU4XDate(cx, date, cal.get());
  if (!dt) {
    return false;
  }

  // Step 3.
  MonthCode monthCode;
  if (!CalendarDateMonthCode(cx, calendarId, dt.get(), &monthCode)) {
    return false;
  }
  result.setMonthCode(monthCode);

  // Step 4.
  if (type == DateFieldType::MonthDay || type == DateFieldType::Date) {
    int32_t day = capi::ICU4XDate_day_of_month(dt.get());
    result.setDay(day);
  }

  // Step 5.
  if (type == DateFieldType::YearMonth || type == DateFieldType::Date) {
    int32_t year;
    if (!CalendarDateYear(cx, calendarId, dt.get(), &year)) {
      return false;
    }
    result.setYear(year);
  }

  // Step 6.
  return true;
}

/**
 * ISODateToFields ( calendar, isoDate, type )
 */
bool js::temporal::ISODateToFields(JSContext* cx,
                                   Handle<PlainDateWithCalendar> date,
                                   MutableHandle<CalendarFields> result) {
  return ISODateToFields(cx, date.calendar(), date, DateFieldType::Date,
                         result);
}

/**
 * ISODateToFields ( calendar, isoDate, type )
 */
bool js::temporal::ISODateToFields(JSContext* cx,
                                   Handle<PlainDateTimeWithCalendar> dateTime,
                                   MutableHandle<CalendarFields> result) {
  return ISODateToFields(cx, dateTime.calendar(), dateTime.date(),
                         DateFieldType::Date, result);
}

/**
 * ISODateToFields ( calendar, isoDate, type )
 */
bool js::temporal::ISODateToFields(JSContext* cx,
                                   Handle<PlainMonthDayWithCalendar> monthDay,
                                   MutableHandle<CalendarFields> result) {
  return ISODateToFields(cx, monthDay.calendar(), monthDay.date(),
                         DateFieldType::MonthDay, result);
}

/**
 * ISODateToFields ( calendar, isoDate, type )
 */
bool js::temporal::ISODateToFields(JSContext* cx,
                                   Handle<PlainYearMonthWithCalendar> yearMonth,
                                   MutableHandle<CalendarFields> result) {
  return ISODateToFields(cx, yearMonth.calendar(), yearMonth.date(),
                         DateFieldType::YearMonth, result);
}

/**
 * CalendarDateFromFields ( calendar, fields, overflow )
 */
bool js::temporal::CalendarDateFromFields(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<CalendarFields> fields, TemporalOverflow overflow,
    MutableHandle<PlainDateWithCalendar> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (!CalendarResolveFields(cx, calendarId, fields, FieldType::Date)) {
    return false;
  }

  // Step 2.
  PlainDate date;
  if (!CalendarDateToISO(cx, calendarId, fields, overflow, &date)) {
    return false;
  }

  // Steps 3-4.
  return CreateTemporalDate(cx, date, calendar, result);
}

/**
 * CalendarYearMonthFromFields ( calendar, fields, overflow )
 */
bool js::temporal::CalendarYearMonthFromFields(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<CalendarFields> fields, TemporalOverflow overflow,
    MutableHandle<PlainYearMonthWithCalendar> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (!CalendarResolveFields(cx, calendarId, fields, FieldType::YearMonth)) {
    return false;
  }

  // Step 2.
  int32_t firstDayIndex = 1;

  // Step 3.
  Rooted<CalendarFields> resolvedFields(cx, CalendarFields{fields});
  resolvedFields.setDay(firstDayIndex);

  // Step 4.
  PlainDate date;
  if (!CalendarDateToISO(cx, calendarId, resolvedFields, overflow, &date)) {
    return false;
  }

  // Steps 5-6.
  return CreateTemporalYearMonth(cx, date, calendar, result);
}

/**
 * CalendarMonthDayFromFields ( calendar, fields, overflow )
 */
bool js::temporal::CalendarMonthDayFromFields(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<CalendarFields> fields, TemporalOverflow overflow,
    MutableHandle<PlainMonthDayWithCalendar> result) {
  auto calendarId = calendar.identifier();

  // Step 1.
  if (!CalendarResolveFields(cx, calendarId, fields, FieldType::MonthDay)) {
    return false;
  }

  // Step 2.
  PlainDate date;
  if (!CalendarMonthDayToISOReferenceDate(cx, calendarId, fields, overflow,
                                          &date)) {
    return false;
  }

  // Step 3-4.
  return CreateTemporalMonthDay(cx, date, calendar, result);
}

/**
 * CalendarDateAdd ( date, duration, overflow )
 */
bool js::temporal::CalendarDateAdd(JSContext* cx,
                                   Handle<CalendarValue> calendar,
                                   const PlainDate& date,
                                   const DateDuration& duration,
                                   TemporalOverflow overflow,
                                   PlainDate* result) {
  MOZ_ASSERT(IsValidISODate(date));
  MOZ_ASSERT(IsValidDuration(duration));

  // FIXME: Not supported in ICU4X. Use the ISO8601 calendar code for now.
  //
  // https://github.com/unicode-org/icu4x/issues/3964

  // Steps 1-2.
  if (!AddISODate(cx, date, duration, overflow, result)) {
    return false;
  }

  // Step 3.
  if (!ISODateWithinLimits(*result)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return false;
  }

  // Step 4.
  return true;
}

/**
 * CalendarDateUntil ( one, two, largestUnit )
 */
bool js::temporal::CalendarDateUntil(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     const PlainDate& one, const PlainDate& two,
                                     TemporalUnit largestUnit,
                                     DateDuration* result) {
  MOZ_ASSERT(largestUnit <= TemporalUnit::Day);

  // FIXME: Not supported in ICU4X. Use the ISO8601 calendar code for now.
  //
  // https://github.com/unicode-org/icu4x/issues/3964

  // Steps 1-2.
  *result = DifferenceISODate(one, two, largestUnit);
  return true;
}
