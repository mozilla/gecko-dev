/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TemporalFields.h"

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
#include "builtin/temporal/Temporal.h"
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

void TemporalFields::trace(JSTracer* trc) {
  TraceNullableRoot(trc, &monthCode_, "TemporalFields::monthCode");
  TraceNullableRoot(trc, &offset_, "TemporalFields::offset");
  TraceNullableRoot(trc, &era_, "TemporalFields::era");
  TraceRoot(trc, &timeZone_, "TemporalFields::timeZone");
}

bool TemporalFields::isUndefined(TemporalField field) const {
  MOZ_ASSERT(has(field));
  switch (field) {
    case TemporalField::Year:
      return std::isnan(year_);
    case TemporalField::Month:
      return std::isnan(month_);
    case TemporalField::MonthCode:
      return !monthCode_;
    case TemporalField::Day:
      return std::isnan(day_);
    case TemporalField::Hour:
      MOZ_ASSERT(!std::isnan(hour_));
      return false;
    case TemporalField::Minute:
      MOZ_ASSERT(!std::isnan(minute_));
      return false;
    case TemporalField::Second:
      MOZ_ASSERT(!std::isnan(second_));
      return false;
    case TemporalField::Millisecond:
      MOZ_ASSERT(!std::isnan(millisecond_));
      return false;
    case TemporalField::Microsecond:
      MOZ_ASSERT(!std::isnan(microsecond_));
      return false;
    case TemporalField::Nanosecond:
      MOZ_ASSERT(!std::isnan(nanosecond_));
      return false;
    case TemporalField::Offset:
      return !offset_;
    case TemporalField::Era:
      return !era_;
    case TemporalField::EraYear:
      return std::isnan(eraYear_);
    case TemporalField::TimeZone:
      return timeZone_.isUndefined();
  }
  MOZ_CRASH("invalid temporal field");
}

void TemporalFields::setFrom(TemporalField field,
                             const TemporalFields& source) {
  MOZ_ASSERT(source.has(field));
  MOZ_ASSERT(!source.isUndefined(field));

  switch (field) {
    case TemporalField::Year:
      setYear(source.year());
      return;
    case TemporalField::Month:
      setMonth(source.month());
      return;
    case TemporalField::MonthCode:
      setMonthCode(source.monthCode());
      return;
    case TemporalField::Day:
      setDay(source.day());
      return;
    case TemporalField::Hour:
      setHour(source.hour());
      return;
    case TemporalField::Minute:
      setMinute(source.minute());
      return;
    case TemporalField::Second:
      setSecond(source.second());
      return;
    case TemporalField::Millisecond:
      setMillisecond(source.millisecond());
      return;
    case TemporalField::Microsecond:
      setMicrosecond(source.microsecond());
      return;
    case TemporalField::Nanosecond:
      setNanosecond(source.nanosecond());
      return;
    case TemporalField::Offset:
      setOffset(source.offset());
      return;
    case TemporalField::Era:
      setEra(source.era());
      return;
    case TemporalField::EraYear:
      setEraYear(source.eraYear());
      return;
    case TemporalField::TimeZone:
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
    using value_type = TemporalField;
    using pointer = TemporalField*;
    using reference = TemporalField&;
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

static PropertyName* ToPropertyName(JSContext* cx, TemporalField field) {
  switch (field) {
    case TemporalField::Year:
      return cx->names().year;
    case TemporalField::Month:
      return cx->names().month;
    case TemporalField::MonthCode:
      return cx->names().monthCode;
    case TemporalField::Day:
      return cx->names().day;
    case TemporalField::Hour:
      return cx->names().hour;
    case TemporalField::Minute:
      return cx->names().minute;
    case TemporalField::Second:
      return cx->names().second;
    case TemporalField::Millisecond:
      return cx->names().millisecond;
    case TemporalField::Microsecond:
      return cx->names().microsecond;
    case TemporalField::Nanosecond:
      return cx->names().nanosecond;
    case TemporalField::Offset:
      return cx->names().offset;
    case TemporalField::Era:
      return cx->names().era;
    case TemporalField::EraYear:
      return cx->names().eraYear;
    case TemporalField::TimeZone:
      return cx->names().timeZone;
  }
  MOZ_CRASH("invalid temporal field name");
}

static constexpr const char* ToCString(TemporalField field) {
  switch (field) {
    case TemporalField::Year:
      return "year";
    case TemporalField::Month:
      return "month";
    case TemporalField::MonthCode:
      return "monthCode";
    case TemporalField::Day:
      return "day";
    case TemporalField::Hour:
      return "hour";
    case TemporalField::Minute:
      return "minute";
    case TemporalField::Second:
      return "second";
    case TemporalField::Millisecond:
      return "millisecond";
    case TemporalField::Microsecond:
      return "microsecond";
    case TemporalField::Nanosecond:
      return "nanosecond";
    case TemporalField::Offset:
      return "offset";
    case TemporalField::Era:
      return "era";
    case TemporalField::EraYear:
      return "eraYear";
    case TemporalField::TimeZone:
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
    TemporalField::Day,         TemporalField::Era,
    TemporalField::EraYear,     TemporalField::Hour,
    TemporalField::Microsecond, TemporalField::Millisecond,
    TemporalField::Minute,      TemporalField::Month,
    TemporalField::MonthCode,   TemporalField::Nanosecond,
    TemporalField::Offset,      TemporalField::Second,
    TemporalField::TimeZone,    TemporalField::Year,
};

static_assert(IsSorted(sortedTemporalFields));

// TODO: Consider reordering TemporalField so we don't need this. Probably best
// to decide after <https://github.com/tc39/proposal-temporal/issues/2826> has
// landed.
using SortedTemporalFields = SortedEnumSet<TemporalField, sortedTemporalFields>;

static JSString* ToPrimitiveAndRequireString(JSContext* cx,
                                             Handle<Value> value) {
  Rooted<Value> primitive(cx, value);
  if (!ToPrimitive(cx, JSTYPE_STRING, &primitive)) {
    return nullptr;
  }
  if (!primitive.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, primitive,
                     nullptr, "not a string");
    return nullptr;
  }
  return primitive.toString();
}

// clang-format off
//
// TODO: |fields| is often a built-in Temporal type, so we likely want to
// optimise for this case.
//
// Consider the case when PlainDate.prototype.toPlainMonthDay is called. The
// following steps are applied:
//
// 1. CalendarFields(calendar, «"day", "monthCode"») is called to retrieve the
//    relevant calendar fields. For (most?) built-in calendars this will just
//    return the input list «"day", "monthCode"».
// 2. PrepareTemporalFields(plainDate, «"day", "monthCode"») is called. This
//    will access the properties `plainDate.day` and `plainDate.monthCode`.
//   a. `plainDate.day` will call CalendarDay(calendar, plainDate).
//   b. For built-in calendars, this will simply access `plainDate.[[IsoDay]]`.
//   c. `plainDate.monthCode` will call CalendarMonthCode(calendar, plainDate).
//   d. For built-in calendars, ISOMonthCode(plainDate.[[IsoMonth]]) is called.
// 3. CalendarMonthDayFromFields(calendar, {day, monthCode}) is called.
// 4. For built-in calendars, this calls PrepareTemporalFields({day, monthCode},
//    «"day", "month", "monthCode", "year"», «"day"»).
// 5. The previous PrepareTemporalFields call is a no-op and returns {day, monthCode}.
// 6. Then ISOMonthDayFromFields({day, monthCode}, "constrain") gets called.
// 7. ResolveISOMonth(monthCode) is called to parse the just created `monthCode`.
// 8. RegulateISODate(referenceISOYear, month, day, "constrain") is called.
// 9. Finally CreateTemporalMonthDay is called to create the PlainMonthDay instance.
//
// All these steps could be simplified to just:
// 1. CreateTemporalMonthDay(referenceISOYear, plainDate.[[IsoMonth]], plainDate.[[IsoDay]]).
//
// When the following conditions are true:
// 1. The `plainDate` is a Temporal.PlainDate instance and has no overridden methods.
// 2. Temporal.PlainDate.prototype is in its initial state.
//
// PlainDate_toPlainMonthDay has an example implementation for this optimisation.
//
// clang-format on

enum class Partial : bool { No, Yes };

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
bool js::temporal::PrepareTemporalFields(
    JSContext* cx, Handle<TemporalFields> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    mozilla::EnumSet<TemporalField> requiredFields,
    MutableHandle<TemporalFields> result) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  //
  // Default initialize the result.
  auto resultFields = TemporalFields{};

  // Steps 3-6. (Not applicable in our implementation.)

  // Step 7.
  for (auto fieldName : fieldNames) {
    // Step 7.a. (Not applicable in our implementation.)

    // Step 7.b.i-iii.
    if (fields.has(fieldName) && !fields.isUndefined(fieldName)) {
      resultFields.setFrom(fieldName, fields);
    } else {
      // Step 7.b.iii.1.
      if (requiredFields.contains(fieldName)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_TEMPORAL_MISSING_PROPERTY,
                                  ToCString(fieldName));
        return false;
      }

      // Steps 7.b.iii.2-3. (Not applicable in our implementation.)
      resultFields.setDefault(fieldName);
    }

    // Steps 7.c-d. (Not applicable in our implementation.)
  }

  result.set(resultFields);
  return true;
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
static bool PrepareTemporalFields(
    JSContext* cx, Handle<JSObject*> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    mozilla::EnumSet<TemporalField> requiredFields, Partial partial,
    MutableHandle<TemporalFields> result) {
  MOZ_ASSERT_IF(partial == Partial::Yes, requiredFields.isEmpty());

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  //
  // Default initialize the result.
  result.set(TemporalFields{});

  // Steps 3-6. (Not applicable in our implementation.)

  // Step 7.
  Rooted<Value> value(cx);
  for (auto fieldName : SortedTemporalFields{fieldNames}) {
    auto* property = ToPropertyName(cx, fieldName);
    const auto* cstr = ToCString(fieldName);

    // Step 7.a. (Not applicable in our implementation.)

    // Step 7.b.i.
    if (!GetProperty(cx, fields, fields, property, &value)) {
      return false;
    }

    // Steps 7.b.ii-iii.
    if (!value.isUndefined()) {
      // Step 7.b.ii.1. (Not applicable in our implementation.)

      // Steps 7.b.ii.2-3.
      switch (fieldName) {
        case TemporalField::Year: {
          double year;
          if (!ToIntegerWithTruncation(cx, value, cstr, &year)) {
            return false;
          }
          result.setYear(year);
          break;
        }
        case TemporalField::Month: {
          double month;
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr, &month)) {
            return false;
          }
          result.setMonth(month);
          break;
        }
        case TemporalField::MonthCode: {
          JSString* monthCode = ToPrimitiveAndRequireString(cx, value);
          if (!monthCode) {
            return false;
          }
          result.setMonthCode(monthCode);
          break;
        }
        case TemporalField::Day: {
          double day;
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr, &day)) {
            return false;
          }
          result.setDay(day);
          break;
        }
        case TemporalField::Hour: {
          double hour;
          if (!ToIntegerWithTruncation(cx, value, cstr, &hour)) {
            return false;
          }
          result.setHour(hour);
          break;
        }
        case TemporalField::Minute: {
          double minute;
          if (!ToIntegerWithTruncation(cx, value, cstr, &minute)) {
            return false;
          }
          result.setMinute(minute);
          break;
        }
        case TemporalField::Second: {
          double second;
          if (!ToIntegerWithTruncation(cx, value, cstr, &second)) {
            return false;
          }
          result.setSecond(second);
          break;
        }
        case TemporalField::Millisecond: {
          double millisecond;
          if (!ToIntegerWithTruncation(cx, value, cstr, &millisecond)) {
            return false;
          }
          result.setMillisecond(millisecond);
          break;
        }
        case TemporalField::Microsecond: {
          double microsecond;
          if (!ToIntegerWithTruncation(cx, value, cstr, &microsecond)) {
            return false;
          }
          result.setMicrosecond(microsecond);
          break;
        }
        case TemporalField::Nanosecond: {
          double nanosecond;
          if (!ToIntegerWithTruncation(cx, value, cstr, &nanosecond)) {
            return false;
          }
          result.setNanosecond(nanosecond);
          break;
        }
        case TemporalField::Offset: {
          JSString* offset = ToPrimitiveAndRequireString(cx, value);
          if (!offset) {
            return false;
          }
          result.setOffset(offset);
          break;
        }
        case TemporalField::Era: {
          JSString* era = ToPrimitiveAndRequireString(cx, value);
          if (!era) {
            return false;
          }
          result.setEra(era);
          break;
        }
        case TemporalField::EraYear: {
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
        case TemporalField::TimeZone:
          // FIXME: spec issue - add conversion via ToTemporalTimeZoneSlotValue?

          // NB: TemporalField::TimeZone has no conversion function.
          result.setTimeZone(value);
          break;
      }
    } else if (partial == Partial::No) {
      // Step 7.b.iii.1.
      if (requiredFields.contains(fieldName)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_TEMPORAL_MISSING_PROPERTY, cstr);
        return false;
      }

      // Steps 7.b.iii.2-3. (Not applicable in our implementation.)
      result.setDefault(fieldName);
    }

    // Steps 7.c-d. (Not applicable in our implementation.)
  }

  // Step 8.
  if (partial == Partial::Yes && result.keys().isEmpty()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_MISSING_TEMPORAL_FIELDS);
    return false;
  }

  // Step 9.
  return true;
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
bool js::temporal::PrepareTemporalFields(
    JSContext* cx, Handle<JSObject*> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    mozilla::EnumSet<TemporalField> requiredFields,
    MutableHandle<TemporalFields> result) {
  return PrepareTemporalFields(cx, fields, fieldNames, requiredFields,
                               Partial::No, result);
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
bool js::temporal::PreparePartialTemporalFields(
    JSContext* cx, Handle<JSObject*> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    JS::MutableHandle<TemporalFields> result) {
  return PrepareTemporalFields(cx, fields, fieldNames, {}, Partial::Yes,
                               result);
}

static auto AsTemporalFieldSet(mozilla::EnumSet<CalendarField> values) {
  using T = std::underlying_type_t<TemporalField>;
  static_assert(std::is_same_v<T, std::underlying_type_t<CalendarField>>);

  static_assert(static_cast<T>(TemporalField::Year) ==
                static_cast<T>(CalendarField::Year));
  static_assert(static_cast<T>(TemporalField::Month) ==
                static_cast<T>(CalendarField::Month));
  static_assert(static_cast<T>(TemporalField::MonthCode) ==
                static_cast<T>(CalendarField::MonthCode));
  static_assert(static_cast<T>(TemporalField::Day) ==
                static_cast<T>(CalendarField::Day));

  auto result = mozilla::EnumSet<TemporalField>{};
  result.deserialize(values.serialize());
  return result;
}

/**
 * PrepareCalendarFieldsAndFieldNames ( calendar, fields, calendarFieldNames [ ,
 * nonCalendarFieldNames [ , requiredFieldNames ] ] )
 */
static bool PrepareCalendarFieldsAndFieldNames(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    mozilla::EnumSet<TemporalField> nonCalendarFieldNames,
    mozilla::EnumSet<TemporalField> requiredFieldNames,
    MutableHandle<TemporalFields> result) {
  auto calendarId = calendar.identifier();

  // Steps 1-2. (Not applicable in our implementation.)

  // Step 3.
  auto fieldNames = AsTemporalFieldSet(calendarFieldNames);

  // Steps 4.
  if (calendarId != CalendarId::ISO8601) {
    fieldNames += CalendarFieldDescriptors(calendar, calendarFieldNames);
  }

  // Step 5.
  fieldNames += nonCalendarFieldNames;

  // FIXME: spec issue - `fieldNames` doesn't need to be returned, because it
  // can be retrieved through the keys of `resultFields`.

  // FIXME: spec issue - `fields` parameter shadowed.

  // Steps 6-7
  return PrepareTemporalFields(cx, fields, fieldNames, requiredFieldNames,
                               result);
}

/**
 * PrepareCalendarFieldsAndFieldNames ( calendar, fields, calendarFieldNames [ ,
 * nonCalendarFieldNames [ , requiredFieldNames ] ] )
 */
bool js::temporal::PrepareCalendarFieldsAndFieldNames(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    MutableHandle<TemporalFields> result) {
  return ::PrepareCalendarFieldsAndFieldNames(
      cx, calendar, fields, calendarFieldNames, {}, {}, result);
}

#ifdef DEBUG
static constexpr auto NonCalendarFieldNames = mozilla::EnumSet<TemporalField>{
    TemporalField::Hour,        TemporalField::Minute,
    TemporalField::Second,      TemporalField::Millisecond,
    TemporalField::Microsecond, TemporalField::Nanosecond,
    TemporalField::Offset,      TemporalField::TimeZone,
};
#endif

/**
 * PrepareCalendarFields ( calendar, fields, calendarFieldNames,
 * nonCalendarFieldNames, requiredFieldNames )
 */
bool js::temporal::PrepareCalendarFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    mozilla::EnumSet<TemporalField> nonCalendarFieldNames,
    mozilla::EnumSet<TemporalField> requiredFieldNames,
    MutableHandle<TemporalFields> result) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  //
  // Ensure `nonCalendarFieldNames ⊆ NonCalendarFieldNames`.
  MOZ_ASSERT(NonCalendarFieldNames.contains(nonCalendarFieldNames));

  // Step 3.
  //
  // Ensure `requiredFieldNames ⊆ (calendarFieldNames ∪ nonCalendarFieldNames)`.
  MOZ_ASSERT((AsTemporalFieldSet(calendarFieldNames) + nonCalendarFieldNames)
                 .contains(requiredFieldNames));

  // Steps 4-5.
  return ::PrepareCalendarFieldsAndFieldNames(
      cx, calendar, fields, calendarFieldNames, nonCalendarFieldNames,
      requiredFieldNames, result);
}
