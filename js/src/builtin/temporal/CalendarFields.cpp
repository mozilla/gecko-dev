/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/CalendarFields.h"

#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/RangedPtr.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <stdint.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Crash.h"
#include "builtin/temporal/Era.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalParser.h"
#include "gc/Barrier.h"
#include "gc/Tracer.h"
#include "js/AllocPolicy.h"
#include "js/ComparisonOperators.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "util/Text.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"
#include "vm/SymbolType.h"

#include "vm/JSAtomUtils-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

void CalendarFields::trace(JSTracer* trc) {
  TraceNullableRoot(trc, &era_, "CalendarFields::era");
  timeZone_.trace(trc);
}

void CalendarFields::setFrom(CalendarField field,
                             const CalendarFields& source) {
  MOZ_ASSERT(source.has(field));

  switch (field) {
    case CalendarField::Era:
      setEra(source.era());
      return;
    case CalendarField::EraYear:
      setEraYear(source.eraYear());
      return;
    case CalendarField::Year:
      setYear(source.year());
      return;
    case CalendarField::Month:
      setMonth(source.month());
      return;
    case CalendarField::MonthCode:
      setMonthCode(source.monthCode());
      return;
    case CalendarField::Day:
      setDay(source.day());
      return;
    case CalendarField::Hour:
      setHour(source.hour());
      return;
    case CalendarField::Minute:
      setMinute(source.minute());
      return;
    case CalendarField::Second:
      setSecond(source.second());
      return;
    case CalendarField::Millisecond:
      setMillisecond(source.millisecond());
      return;
    case CalendarField::Microsecond:
      setMicrosecond(source.microsecond());
      return;
    case CalendarField::Nanosecond:
      setNanosecond(source.nanosecond());
      return;
    case CalendarField::Offset:
      setOffset(source.offset());
      return;
    case CalendarField::TimeZone:
      setTimeZone(source.timeZone());
      return;
  }
  MOZ_CRASH("invalid temporal field");
}

template <typename T, const auto& sorted>
class SortedEnumSet {
  mozilla::EnumSet<T> fields_;

 public:
  explicit SortedEnumSet(mozilla::EnumSet<T> fields) : fields_(fields) {}

  class Iterator {
    mozilla::EnumSet<T> fields_;
    size_t index_;

    void findNext() {
      while (index_ < sorted.size() && !fields_.contains(sorted[index_])) {
        index_++;
      }
    }

   public:
    // Iterator traits.
    using difference_type = ptrdiff_t;
    using value_type = CalendarField;
    using pointer = CalendarField*;
    using reference = CalendarField&;
    using iterator_category = std::forward_iterator_tag;

    Iterator(mozilla::EnumSet<T> fields, size_t index)
        : fields_(fields), index_(index) {
      findNext();
    }

    bool operator==(const Iterator& other) const {
      MOZ_ASSERT(fields_ == other.fields_);
      return index_ == other.index_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    auto operator*() const {
      MOZ_ASSERT(index_ < sorted.size());
      MOZ_ASSERT(fields_.contains(sorted[index_]));
      return sorted[index_];
    }

    auto& operator++() {
      MOZ_ASSERT(index_ < sorted.size());
      index_++;
      findNext();
      return *this;
    }

    auto operator++(int) {
      auto result = *this;
      ++(*this);
      return result;
    }
  };

  Iterator begin() const { return Iterator{fields_, 0}; };

  Iterator end() const { return Iterator{fields_, sorted.size()}; }
};

static PropertyName* ToPropertyName(JSContext* cx, CalendarField field) {
  switch (field) {
    case CalendarField::Era:
      return cx->names().era;
    case CalendarField::EraYear:
      return cx->names().eraYear;
    case CalendarField::Year:
      return cx->names().year;
    case CalendarField::Month:
      return cx->names().month;
    case CalendarField::MonthCode:
      return cx->names().monthCode;
    case CalendarField::Day:
      return cx->names().day;
    case CalendarField::Hour:
      return cx->names().hour;
    case CalendarField::Minute:
      return cx->names().minute;
    case CalendarField::Second:
      return cx->names().second;
    case CalendarField::Millisecond:
      return cx->names().millisecond;
    case CalendarField::Microsecond:
      return cx->names().microsecond;
    case CalendarField::Nanosecond:
      return cx->names().nanosecond;
    case CalendarField::Offset:
      return cx->names().offset;
    case CalendarField::TimeZone:
      return cx->names().timeZone;
  }
  MOZ_CRASH("invalid temporal field name");
}

static constexpr const char* ToCString(CalendarField field) {
  switch (field) {
    case CalendarField::Era:
      return "era";
    case CalendarField::EraYear:
      return "eraYear";
    case CalendarField::Year:
      return "year";
    case CalendarField::Month:
      return "month";
    case CalendarField::MonthCode:
      return "monthCode";
    case CalendarField::Day:
      return "day";
    case CalendarField::Hour:
      return "hour";
    case CalendarField::Minute:
      return "minute";
    case CalendarField::Second:
      return "second";
    case CalendarField::Millisecond:
      return "millisecond";
    case CalendarField::Microsecond:
      return "microsecond";
    case CalendarField::Nanosecond:
      return "nanosecond";
    case CalendarField::Offset:
      return "offset";
    case CalendarField::TimeZone:
      return "timeZone";
  }
  JS_CONSTEXPR_CRASH("invalid temporal field name");
}

template <typename T, size_t N>
static constexpr bool IsSorted(const std::array<T, N>& arr) {
  for (size_t i = 1; i < arr.size(); i++) {
    auto a = std::string_view{ToCString(arr[i - 1])};
    auto b = std::string_view{ToCString(arr[i])};
    if (a.compare(b) >= 0) {
      return false;
    }
  }
  return true;
}

static constexpr auto sortedTemporalFields = std::array{
    CalendarField::Day,         CalendarField::Era,
    CalendarField::EraYear,     CalendarField::Hour,
    CalendarField::Microsecond, CalendarField::Millisecond,
    CalendarField::Minute,      CalendarField::Month,
    CalendarField::MonthCode,   CalendarField::Nanosecond,
    CalendarField::Offset,      CalendarField::Second,
    CalendarField::TimeZone,    CalendarField::Year,
};

static_assert(IsSorted(sortedTemporalFields));

// TODO: Consider reordering TemporalField so we don't need this. Probably best
// to decide after <https://github.com/tc39/proposal-temporal/issues/2826> has
// landed.
using SortedTemporalFields = SortedEnumSet<CalendarField, sortedTemporalFields>;

/**
 * CalendarExtraFields ( calendar, type )
 */
static mozilla::EnumSet<CalendarField> CalendarExtraFields(
    CalendarId calendar, mozilla::EnumSet<CalendarField> type) {
  MOZ_ASSERT(calendar != CalendarId::ISO8601);

  // FIXME: spec bug - |type| is always a List.

  // "era" and "eraYear" are relevant for calendars with multiple eras when
  // "year" is present.
  if (type.contains(CalendarField::Year) && CalendarEraRelevant(calendar)) {
    return {CalendarField::Era, CalendarField::EraYear};
  }
  return {};
}

/**
 * ToMonthCode ( argument )
 */
template <typename CharT>
static mozilla::Maybe<MonthCodeField> ToMonthCode(
    mozilla::Range<const CharT> chars) {
  // Steps 1-2. (Not applicable)

  // Step 3.
  //
  // Caller is responsible to ensure the string has the correct length.
  MOZ_ASSERT(chars.length() >= 3 && chars.length() <= 4);

  // Steps 4 and 7.
  //
  // Starts with capital letter 'M'. Leap months end with capital letter 'L'.
  bool isLeapMonth = chars.length() == 4;
  if (chars[0] != 'M' || (isLeapMonth && chars[3] != 'L')) {
    return mozilla::Nothing();
  }

  // Steps 5-6.
  //
  // Month numbers are ASCII digits.
  if (!mozilla::IsAsciiDigit(chars[1]) || !mozilla::IsAsciiDigit(chars[2])) {
    return mozilla::Nothing();
  }

  // Steps 8-9.
  int32_t ordinal =
      AsciiDigitToNumber(chars[1]) * 10 + AsciiDigitToNumber(chars[2]);

  // Step 10.
  if (ordinal == 0 && !isLeapMonth) {
    return mozilla::Nothing();
  }

  // Step 11.
  return mozilla::Some(MonthCodeField{ordinal, isLeapMonth});
}

/**
 * ToMonthCode ( argument )
 */
static auto ToMonthCode(const JSLinearString* linear) {
  JS::AutoCheckCannotGC nogc;

  if (linear->hasLatin1Chars()) {
    return ToMonthCode(linear->latin1Range(nogc));
  }
  return ToMonthCode(linear->twoByteRange(nogc));
}

/**
 * ToMonthCode ( argument )
 */
static bool ToMonthCode(JSContext* cx, Handle<Value> value,
                        MonthCodeField* result) {
  auto reportInvalidMonthCode = [&](JSLinearString* monthCode) {
    if (auto code = QuoteString(cx, monthCode)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                               code.get());
    }
    return false;
  };

  // Step 1.
  Rooted<Value> monthCode(cx, value);
  if (!ToPrimitive(cx, JSTYPE_STRING, &monthCode)) {
    return false;
  }

  // Step 2.
  if (!monthCode.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, monthCode,
                     nullptr, "not a string");
    return false;
  }

  JSLinearString* monthCodeStr = monthCode.toString()->ensureLinear(cx);
  if (!monthCodeStr) {
    return false;
  }

  // Step 3.
  if (monthCodeStr->length() < 3 || monthCodeStr->length() > 4) {
    return reportInvalidMonthCode(monthCodeStr);
  }

  // Steps 4-11.
  auto parsed = ToMonthCode(monthCodeStr);
  if (!parsed) {
    return reportInvalidMonthCode(monthCodeStr);
  }

  *result = *parsed;
  return true;
}

/**
 * ToOffsetString ( argument )
 */
static bool ToOffsetString(JSContext* cx, Handle<Value> value,
                           int64_t* result) {
  // Step 1.
  Rooted<Value> offset(cx, value);
  if (!ToPrimitive(cx, JSTYPE_STRING, &offset)) {
    return false;
  }

  // Step 2.
  if (!offset.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, offset,
                     nullptr, "not a string");
    return false;
  }
  Rooted<JSString*> offsetStr(cx, offset.toString());

  // Steps 3-4.
  return ParseDateTimeUTCOffset(cx, offsetStr, result);
}

enum class Partial : bool { No, Yes };

/**
 * PrepareCalendarFields ( calendar, fields, calendarFieldNames,
 * nonCalendarFieldNames, requiredFieldNames )
 */
static bool PrepareCalendarFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> fieldNames,
    mozilla::EnumSet<CalendarField> requiredFields, Partial partial,
    MutableHandle<CalendarFields> result) {
  MOZ_ASSERT_IF(partial == Partial::Yes, requiredFields.isEmpty());

  // FIXME: spec issue - still necessary to have separate |calendarFieldNames|
  // and |nonCalendarFieldNames| parameters?

  // FIXME: spec issue - callers don't have to sort input alphabetically, but
  // can instead use the logical order, i.e. year -> month -> monthCode -> day..

  // Steps 1-2. (Not applicable in our implementation.)

  // Step 3.
  auto calendarId = calendar.identifier();
  if (calendarId != CalendarId::ISO8601) {
    // Step 3.a.
    auto extraFieldNames = CalendarExtraFields(calendarId, fieldNames);

    // Step 3.b.
    fieldNames += extraFieldNames;
  }

  // Step 5.
  //
  // Default initialize the result.
  result.set(CalendarFields{});

  // Steps 6-7. (Not applicable in our implementation.)

  // Step 8.
  Rooted<Value> value(cx);
  for (auto fieldName : SortedTemporalFields{fieldNames}) {
    auto* propertyName = ToPropertyName(cx, fieldName);
    const auto* cstr = ToCString(fieldName);

    // Step 8.a. (Not applicable in our implementation.)

    // Step 8.b.
    if (!GetProperty(cx, fields, fields, propertyName, &value)) {
      return false;
    }

    // Steps 8.c-d.
    if (!value.isUndefined()) {
      // Steps 8.c.i-ii. (Not applicable in our implementation.)

      // Steps 8.c.iii-ix.
      switch (fieldName) {
        case CalendarField::Era: {
          JSString* era = ToString(cx, value);
          if (!era) {
            return false;
          }
          result.setEra(era);
          break;
        }
        case CalendarField::EraYear: {
          // All supported calendar systems with eras require positive era
          // years, so we require era year to be greater than zero. If ICU4X'
          // Ethiopian implementation get changed to allow negative era years,
          // we need to update this code.
          //
          // Also see <https://unicode-org.atlassian.net/browse/ICU-21985>.
          double eraYear;
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr, &eraYear)) {
            return false;
          }
          result.setEraYear(eraYear);
          break;
        }
        case CalendarField::Year: {
          double year;
          if (!ToIntegerWithTruncation(cx, value, cstr, &year)) {
            return false;
          }
          result.setYear(year);
          break;
        }
        case CalendarField::Month: {
          double month;
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr, &month)) {
            return false;
          }
          result.setMonth(month);
          break;
        }
        case CalendarField::MonthCode: {
          MonthCodeField monthCode;
          if (!ToMonthCode(cx, value, &monthCode)) {
            return false;
          }
          result.setMonthCode(monthCode);
          break;
        }
        case CalendarField::Day: {
          double day;
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr, &day)) {
            return false;
          }
          result.setDay(day);
          break;
        }
        case CalendarField::Hour: {
          double hour;
          if (!ToIntegerWithTruncation(cx, value, cstr, &hour)) {
            return false;
          }
          result.setHour(hour);
          break;
        }
        case CalendarField::Minute: {
          double minute;
          if (!ToIntegerWithTruncation(cx, value, cstr, &minute)) {
            return false;
          }
          result.setMinute(minute);
          break;
        }
        case CalendarField::Second: {
          double second;
          if (!ToIntegerWithTruncation(cx, value, cstr, &second)) {
            return false;
          }
          result.setSecond(second);
          break;
        }
        case CalendarField::Millisecond: {
          double millisecond;
          if (!ToIntegerWithTruncation(cx, value, cstr, &millisecond)) {
            return false;
          }
          result.setMillisecond(millisecond);
          break;
        }
        case CalendarField::Microsecond: {
          double microsecond;
          if (!ToIntegerWithTruncation(cx, value, cstr, &microsecond)) {
            return false;
          }
          result.setMicrosecond(microsecond);
          break;
        }
        case CalendarField::Nanosecond: {
          double nanosecond;
          if (!ToIntegerWithTruncation(cx, value, cstr, &nanosecond)) {
            return false;
          }
          result.setNanosecond(nanosecond);
          break;
        }
        case CalendarField::Offset: {
          int64_t offset;
          if (!ToOffsetString(cx, value, &offset)) {
            return false;
          }
          result.setOffset(OffsetField{offset});
          break;
        }
        case CalendarField::TimeZone:
          Rooted<TimeZoneValue> timeZone(cx);
          if (!ToTemporalTimeZone(cx, value, &timeZone)) {
            return false;
          }
          result.setTimeZone(timeZone);
          break;
      }
    } else if (partial == Partial::No) {
      // Step 8.d.i.
      if (requiredFields.contains(fieldName)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_TEMPORAL_MISSING_PROPERTY, cstr);
        return false;
      }

      // Step 8.d.ii.
      result.setDefault(fieldName);
    }
  }

  // Step 9.
  if (partial == Partial::Yes && result.keys().isEmpty()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_MISSING_TEMPORAL_FIELDS);
    return false;
  }

  // Step 10.
  return true;
}

/**
 * PrepareCalendarFields ( calendar, fields, calendarFieldNames,
 * nonCalendarFieldNames, requiredFieldNames )
 */
bool js::temporal::PrepareCalendarFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> fieldNames,
    mozilla::EnumSet<CalendarField> requiredFields,
    MutableHandle<CalendarFields> result) {
  return PrepareCalendarFields(cx, calendar, fields, fieldNames, requiredFields,
                               Partial::No, result);
}

/**
 * PrepareCalendarFields ( calendar, fields, calendarFieldNames,
 * nonCalendarFieldNames, requiredFieldNames )
 */
bool js::temporal::PreparePartialCalendarFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> fieldNames,
    JS::MutableHandle<CalendarFields> result) {
  return PrepareCalendarFields(cx, calendar, fields, fieldNames, {},
                               Partial::Yes, result);
}

enum class DateFieldType { Date, YearMonth, MonthDay };

/**
 * ISODateToFields ( calendar, isoDate, type )
 */
static bool ISODateToFields(JSContext* cx, Handle<CalendarValue> calendar,
                            const PlainDate& date, DateFieldType type,
                            MutableHandle<CalendarFields> result) {
  // Step 1.
  result.set(CalendarFields{});

  // Step 2.
  Rooted<Value> value(cx);
  if (!CalendarMonthCode(cx, calendar, date, &value)) {
    return false;
  }
  MOZ_ASSERT(value.isString());

  MonthCodeField monthCode;
  if (!ToMonthCode(cx, value, &monthCode)) {
    return false;
  }
  result.setMonthCode(monthCode);

  // Step 3.
  if (type == DateFieldType::MonthDay || type == DateFieldType::Date) {
    if (!CalendarDay(cx, calendar, date, &value)) {
      return false;
    }
    MOZ_ASSERT(value.isInt32());

    result.setDay(value.toInt32());
  }

  // Step 4.
  if (type == DateFieldType::YearMonth || type == DateFieldType::Date) {
    if (!CalendarYear(cx, calendar, date, &value)) {
      return false;
    }
    MOZ_ASSERT(value.isInt32());

    result.setYear(value.toInt32());
  }

  // Step 5.
  return true;
}

/**
 * TemporalObjectToFields ( temporalObject )
 */
bool js::temporal::TemporalObjectToFields(
    JSContext* cx, Handle<PlainDateWithCalendar> temporalObject,
    MutableHandle<CalendarFields> result) {
  // Step 1.
  auto calendar = temporalObject.calendar();

  // Step 2.
  auto date = temporalObject.date();

  // Steps 3-5.
  auto type = DateFieldType::Date;

  // Step 6.
  return ISODateToFields(cx, calendar, date, type, result);
}

/**
 * TemporalObjectToFields ( temporalObject )
 */
bool js::temporal::TemporalObjectToFields(
    JSContext* cx, Handle<PlainDateTimeWithCalendar> temporalObject,
    MutableHandle<CalendarFields> result) {
  // Step 1.
  auto calendar = temporalObject.calendar();

  // Step 2.
  auto date = temporalObject.date();

  // Steps 3-5.
  auto type = DateFieldType::Date;

  // Step 6.
  return ISODateToFields(cx, calendar, date, type, result);
}

/**
 * TemporalObjectToFields ( temporalObject )
 */
bool js::temporal::TemporalObjectToFields(
    JSContext* cx, Handle<PlainMonthDayWithCalendar> temporalObject,
    MutableHandle<CalendarFields> result) {
  // Step 1.
  auto calendar = temporalObject.calendar();

  // Step 2.
  auto date = temporalObject.date();

  // Steps 3-5.
  auto type = DateFieldType::MonthDay;

  // Step 6.
  return ISODateToFields(cx, calendar, date, type, result);
}

/**
 * TemporalObjectToFields ( temporalObject )
 */
bool js::temporal::TemporalObjectToFields(
    JSContext* cx, Handle<PlainYearMonthWithCalendar> temporalObject,
    MutableHandle<CalendarFields> result) {
  // Step 1.
  auto calendar = temporalObject.calendar();

  // Step 2.
  auto date = temporalObject.date();

  // Steps 3-5.
  auto type = DateFieldType::YearMonth;

  // Step 6.
  return ISODateToFields(cx, calendar, date, type, result);
}
