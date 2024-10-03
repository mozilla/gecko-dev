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
  TraceNullableRoot(trc, &monthCode, "TemporalFields::monthCode");
  TraceNullableRoot(trc, &offset, "TemporalFields::offset");
  TraceNullableRoot(trc, &era, "TemporalFields::era");
  TraceRoot(trc, &timeZone, "TemporalFields::timeZone");
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

    void findPrevious() {
      while (index_ > 0 && !fields_.contains(sorted[index_])) {
        index_--;
      }
    }

   public:
    // Iterator traits.
    using difference_type = ptrdiff_t;
    using value_type = TemporalField;
    using pointer = TemporalField*;
    using reference = TemporalField&;
    using iterator_category = std::bidirectional_iterator_tag;

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

    auto& operator--() {
      MOZ_ASSERT(index_ > 0);
      index_--;
      findPrevious();
      return *this;
    }

    auto operator--(int) {
      auto result = *this;
      --(*this);
      return result;
    }
  };

  Iterator begin() const { return Iterator{fields_, 0}; };

  Iterator end() const { return Iterator{fields_, sorted.size()}; }
};

PropertyName* js::temporal::ToPropertyName(JSContext* cx, TemporalField field) {
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

mozilla::Maybe<TemporalField> js::temporal::ToTemporalField(
    JSContext* cx, PropertyKey property) {
  static constexpr TemporalField fieldNames[] = {
      TemporalField::Year,        TemporalField::Month,
      TemporalField::MonthCode,   TemporalField::Day,
      TemporalField::Hour,        TemporalField::Minute,
      TemporalField::Second,      TemporalField::Millisecond,
      TemporalField::Microsecond, TemporalField::Nanosecond,
      TemporalField::Offset,      TemporalField::Era,
      TemporalField::EraYear,     TemporalField::TimeZone,
  };

  for (const auto& fieldName : fieldNames) {
    auto* name = ToPropertyName(cx, fieldName);
    if (property.isAtom(name)) {
      return mozilla::Some(fieldName);
    }
  }
  return mozilla::Nothing();
}

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

static Value TemporalFieldDefaultValue(TemporalField field) {
  switch (field) {
    case TemporalField::Year:
    case TemporalField::Month:
    case TemporalField::MonthCode:
    case TemporalField::Day:
    case TemporalField::Offset:
    case TemporalField::Era:
    case TemporalField::EraYear:
    case TemporalField::TimeZone:
      return UndefinedValue();
    case TemporalField::Hour:
    case TemporalField::Minute:
    case TemporalField::Second:
    case TemporalField::Millisecond:
    case TemporalField::Microsecond:
    case TemporalField::Nanosecond:
      return Int32Value(0);
  }
  MOZ_CRASH("invalid temporal field name");
}

static bool TemporalFieldConvertValue(JSContext* cx, TemporalField field,
                                      MutableHandle<Value> value) {
  const auto* name = ToCString(field);
  switch (field) {
    case TemporalField::Year:
    case TemporalField::Hour:
    case TemporalField::Minute:
    case TemporalField::Second:
    case TemporalField::Millisecond:
    case TemporalField::Microsecond:
    case TemporalField::Nanosecond: {
      double num;
      if (!ToIntegerWithTruncation(cx, value, name, &num)) {
        return false;
      }
      value.setNumber(num);
      return true;
    }

    case TemporalField::EraYear: {
      // All supported calendar systems with eras require positive era years, so
      // we require era year to be greater than zero. If ICU4X' Ethiopian
      // implementation get changed to allow negative era years, we need to
      // update this code.
      //
      // Also see <https://unicode-org.atlassian.net/browse/ICU-21985>.
      [[fallthrough]];
    }

    case TemporalField::Month:
    case TemporalField::Day: {
      double num;
      if (!ToPositiveIntegerWithTruncation(cx, value, name, &num)) {
        return false;
      }
      value.setNumber(num);
      return true;
    }

    case TemporalField::MonthCode:
    case TemporalField::Offset:
    case TemporalField::Era: {
      JSString* str = ToPrimitiveAndRequireString(cx, value);
      if (!str) {
        return false;
      }
      value.setString(str);
      return true;
    }

    case TemporalField::TimeZone:
      // FIXME: spec issue - add conversion via ToTemporalTimeZoneSlotValue?

      // NB: timeZone has no conversion function.
      return true;
  }
  MOZ_CRASH("invalid temporal field name");
}

static void AssignFromFallback(TemporalField fieldName,
                               MutableHandle<TemporalFields> result) {
  // `const` can be changed to `constexpr` when we switch to C++20.
  //
  // Hazard analysis complains when |FallbackValues| is directly contained in
  // loop body of |PrepareTemporalFields|. As a workaround the code was moved
  // into the separate |AssignFromFallback| function.
  const TemporalFields FallbackValues{};

  switch (fieldName) {
    case TemporalField::Year:
      result.year() = FallbackValues.year;
      break;
    case TemporalField::Month:
      result.month() = FallbackValues.month;
      break;
    case TemporalField::MonthCode:
      result.monthCode().set(FallbackValues.monthCode);
      break;
    case TemporalField::Day:
      result.day() = FallbackValues.day;
      break;
    case TemporalField::Hour:
      result.hour() = FallbackValues.hour;
      break;
    case TemporalField::Minute:
      result.minute() = FallbackValues.minute;
      break;
    case TemporalField::Second:
      result.second() = FallbackValues.second;
      break;
    case TemporalField::Millisecond:
      result.millisecond() = FallbackValues.millisecond;
      break;
    case TemporalField::Microsecond:
      result.microsecond() = FallbackValues.microsecond;
      break;
    case TemporalField::Nanosecond:
      result.nanosecond() = FallbackValues.nanosecond;
      break;
    case TemporalField::Offset:
      result.offset().set(FallbackValues.offset);
      break;
    case TemporalField::Era:
      result.era().set(FallbackValues.era);
      break;
    case TemporalField::EraYear:
      result.eraYear() = FallbackValues.eraYear;
      break;
    case TemporalField::TimeZone:
      result.timeZone().set(FallbackValues.timeZone);
      break;
  }
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

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
bool js::temporal::PrepareTemporalFields(
    JSContext* cx, Handle<JSObject*> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    mozilla::EnumSet<TemporalField> requiredFields,
    MutableHandle<TemporalFields> result) {
  // Steps 1-6. (Not applicable in our implementation.)

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
      // Step 6.b.ii.1. (Not applicable in our implementation.)

      // Steps 7.b.ii.2-3.
      switch (fieldName) {
        case TemporalField::Year:
          if (!ToIntegerWithTruncation(cx, value, cstr, &result.year())) {
            return false;
          }
          break;
        case TemporalField::Month:
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr,
                                               &result.month())) {
            return false;
          }
          break;
        case TemporalField::MonthCode: {
          JSString* str = ToPrimitiveAndRequireString(cx, value);
          if (!str) {
            return false;
          }
          result.monthCode().set(str);
          break;
        }
        case TemporalField::Day:
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr,
                                               &result.day())) {
            return false;
          }
          break;
        case TemporalField::Hour:
          if (!ToIntegerWithTruncation(cx, value, cstr, &result.hour())) {
            return false;
          }
          break;
        case TemporalField::Minute:
          if (!ToIntegerWithTruncation(cx, value, cstr, &result.minute())) {
            return false;
          }
          break;
        case TemporalField::Second:
          if (!ToIntegerWithTruncation(cx, value, cstr, &result.second())) {
            return false;
          }
          break;
        case TemporalField::Millisecond:
          if (!ToIntegerWithTruncation(cx, value, cstr,
                                       &result.millisecond())) {
            return false;
          }
          break;
        case TemporalField::Microsecond:
          if (!ToIntegerWithTruncation(cx, value, cstr,
                                       &result.microsecond())) {
            return false;
          }
          break;
        case TemporalField::Nanosecond:
          if (!ToIntegerWithTruncation(cx, value, cstr, &result.nanosecond())) {
            return false;
          }
          break;
        case TemporalField::Offset: {
          JSString* str = ToPrimitiveAndRequireString(cx, value);
          if (!str) {
            return false;
          }
          result.offset().set(str);
          break;
        }
        case TemporalField::Era: {
          JSString* str = ToPrimitiveAndRequireString(cx, value);
          if (!str) {
            return false;
          }
          result.era().set(str);
          break;
        }
        case TemporalField::EraYear:
          // See TemporalFieldConvertValue why positive era years are required.
          if (!ToPositiveIntegerWithTruncation(cx, value, cstr,
                                               &result.eraYear())) {
            return false;
          }
          break;
        case TemporalField::TimeZone:
          // NB: TemporalField::TimeZone has no conversion function.
          result.timeZone().set(value);
          break;
      }
    } else {
      // Step 7.b.iii.1.
      if (requiredFields.contains(fieldName)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_TEMPORAL_MISSING_PROPERTY, cstr);
        return false;
      }

      // Steps 7.b.iii.2-3.
      AssignFromFallback(fieldName, result);
    }

    // Steps 7.c-d. (Not applicable in our implementation.)
  }

  // Step 8. (Not applicable in our implementation.)

  // Step 9.
  return true;
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
PlainObject* js::temporal::PrepareTemporalFields(
    JSContext* cx, Handle<JSObject*> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    mozilla::EnumSet<TemporalField> requiredFields) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Rooted<PlainObject*> result(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!result) {
    return nullptr;
  }

  // Step 3. (Not applicable in our implementation.)

  // Steps 4-6. (Not applicable in our implementation.)

  // Step 7.
  Rooted<Value> value(cx);
  Rooted<PropertyKey> property(cx);
  for (auto fieldName : SortedTemporalFields{fieldNames}) {
    property = NameToId(ToPropertyName(cx, fieldName));

    // Step 7.a.
    // FIXME: spec issue - this check is no longer needed

    // Step 7.b.i.
    if (!GetProperty(cx, fields, fields, property, &value)) {
      return nullptr;
    }

    // FIXME: spec issue - all field names should be valid now

    if (!value.isUndefined()) {
      // Step 7.b.ii.1. (Not applicable in our implementation.)

      // Step 7.b.ii.2.
      if (!TemporalFieldConvertValue(cx, fieldName, &value)) {
        return nullptr;
      }
    } else {
      // Step 7.b.iii.1.
      if (requiredFields.contains(fieldName)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_TEMPORAL_MISSING_PROPERTY,
                                  ToCString(fieldName));
        return nullptr;
      }

      // Step 7.b.iii.2.
      value = TemporalFieldDefaultValue(fieldName);
    }

    // Steps 7.b.ii.3 and 7.b.iii.3.
    if (!DefineDataProperty(cx, result, property, value)) {
      return nullptr;
    }

    // Steps 7.c-d. (Not applicable in our implementation.)
  }

  // Step 8. (Not applicable in our implementation.)

  // Step 9.
  return result;
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
PlainObject* js::temporal::PreparePartialTemporalFields(
    JSContext* cx, Handle<JSObject*> fields,
    mozilla::EnumSet<TemporalField> fieldNames) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Rooted<PlainObject*> result(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!result) {
    return nullptr;
  }

  // Step 3.
  bool any = false;

  // Steps 4-6. (Not applicable in our implementation.)

  // Step 7.
  Rooted<Value> value(cx);
  Rooted<PropertyKey> property(cx);
  for (auto fieldName : SortedTemporalFields{fieldNames}) {
    property = NameToId(ToPropertyName(cx, fieldName));

    // Step 7.a.
    // FIXME: spec issue - this check is no longer needed

    // Step 7.b.i.
    if (!GetProperty(cx, fields, fields, property, &value)) {
      return nullptr;
    }

    // Steps 7.b.ii-iii.
    if (!value.isUndefined()) {
      // Step 7.b.ii.1.
      any = true;

      if (!TemporalFieldConvertValue(cx, fieldName, &value)) {
        return nullptr;
      }

      // Steps 7.b.ii.3.
      if (!DefineDataProperty(cx, result, property, value)) {
        return nullptr;
      }
    } else {
      // Step 7.b.iii. (Not applicable in our implementation.)
    }

    // Steps 7.c-d. (Not applicable in our implementation.)
  }

  // Step 8.
  if (!any) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_MISSING_TEMPORAL_FIELDS);
    return nullptr;
  }

  // Step 9.
  return result;
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
    MutableHandle<PlainObject*> resultFields,
    mozilla::EnumSet<TemporalField>* resultFieldNames) {
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

  // Step 6.
  auto* flds =
      PrepareTemporalFields(cx, fields, fieldNames, requiredFieldNames);
  if (!flds) {
    return false;
  }

  // Step 7.
  resultFields.set(flds);
  *resultFieldNames = fieldNames;
  return true;
}

/**
 * PrepareCalendarFieldsAndFieldNames ( calendar, fields, calendarFieldNames [ ,
 * nonCalendarFieldNames [ , requiredFieldNames ] ] )
 */
bool js::temporal::PrepareCalendarFieldsAndFieldNames(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    MutableHandle<PlainObject*> resultFields,
    mozilla::EnumSet<TemporalField>* resultFieldNames) {
  return ::PrepareCalendarFieldsAndFieldNames(cx, calendar, fields,
                                              calendarFieldNames, {}, {},
                                              resultFields, resultFieldNames);
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
PlainObject* js::temporal::PrepareCalendarFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    mozilla::EnumSet<TemporalField> nonCalendarFieldNames,
    mozilla::EnumSet<TemporalField> requiredFieldNames) {
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
  Rooted<PlainObject*> resultFields(cx);
  mozilla::EnumSet<TemporalField> resultFieldNames{};
  if (!::PrepareCalendarFieldsAndFieldNames(
          cx, calendar, fields, calendarFieldNames, nonCalendarFieldNames,
          requiredFieldNames, &resultFields, &resultFieldNames)) {
    return nullptr;
  }
  return resultFields;
}
