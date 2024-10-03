/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TemporalFields_h
#define builtin_temporal_TemporalFields_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"

#include "jstypes.h"

#include "builtin/temporal/Calendar.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

class JS_PUBLIC_API JSTracer;

namespace js::temporal {
enum class TemporalField {
  Year,
  Month,
  MonthCode,
  Day,
  Hour,
  Minute,
  Second,
  Millisecond,
  Microsecond,
  Nanosecond,
  Offset,
  Era,
  EraYear,
  TimeZone,
};

struct FieldDescriptors {
  mozilla::EnumSet<TemporalField> relevant;
  mozilla::EnumSet<TemporalField> required;

#ifdef DEBUG
  FieldDescriptors(mozilla::EnumSet<TemporalField> relevant,
                   mozilla::EnumSet<TemporalField> required)
      : relevant(relevant), required(required) {
    MOZ_ASSERT(relevant.contains(required),
               "required is a subset of the relevant fields");
  }
#endif
};

// Default values are specified in Table 15 [1]. `undefined` is replaced with
// an appropriate value based on the type, for example `double` fields use
// NaN whereas pointer fields use nullptr.
//
// [1] <https://tc39.es/proposal-temporal/#table-temporal-field-requirements>
class MOZ_STACK_CLASS TemporalFields final {
  mozilla::EnumSet<TemporalField> fields_ = {};

  double year_ = mozilla::UnspecifiedNaN<double>();
  double month_ = mozilla::UnspecifiedNaN<double>();
  JSString* monthCode_ = nullptr;
  double day_ = mozilla::UnspecifiedNaN<double>();
  double hour_ = 0;
  double minute_ = 0;
  double second_ = 0;
  double millisecond_ = 0;
  double microsecond_ = 0;
  double nanosecond_ = 0;
  JSString* offset_ = nullptr;
  JSString* era_ = nullptr;
  double eraYear_ = mozilla::UnspecifiedNaN<double>();
  JS::Value timeZone_ = JS::UndefinedValue();

  /**
   * Mark the field as assigned. Each field should be assigned exactly once.
   */
  void setAssigned(TemporalField field) {
    MOZ_ASSERT(!fields_.contains(field));
    fields_ += field;
  }

  void setOverride(TemporalField field) { fields_ += field; }

 public:
  TemporalFields() = default;
  TemporalFields(const TemporalFields&) = default;

  auto year() const { return year_; }
  auto month() const { return month_; }
  auto* monthCode() const { return monthCode_; }
  auto day() const { return day_; }
  auto hour() const { return hour_; }
  auto minute() const { return minute_; }
  auto second() const { return second_; }
  auto millisecond() const { return millisecond_; }
  auto microsecond() const { return microsecond_; }
  auto nanosecond() const { return nanosecond_; }
  auto* offset() const { return offset_; }
  auto* era() const { return era_; }
  auto eraYear() const { return eraYear_; }
  auto timeZone() const { return timeZone_; }

  void setYear(double year) {
    setAssigned(TemporalField::Year);
    year_ = year;
  }
  void setMonth(double month) {
    setAssigned(TemporalField::Month);
    month_ = month;
  }
  void setMonthCode(JSString* monthCode) {
    setAssigned(TemporalField::MonthCode);
    monthCode_ = monthCode;
  }
  void setDay(double day) {
    setAssigned(TemporalField::Day);
    day_ = day;
  }
  void setHour(double hour) {
    setAssigned(TemporalField::Hour);
    hour_ = hour;
  }
  void setMinute(double minute) {
    setAssigned(TemporalField::Minute);
    minute_ = minute;
  }
  void setSecond(double second) {
    setAssigned(TemporalField::Second);
    second_ = second;
  }
  void setMillisecond(double millisecond) {
    setAssigned(TemporalField::Millisecond);
    millisecond_ = millisecond;
  }
  void setMicrosecond(double microsecond) {
    setAssigned(TemporalField::Microsecond);
    microsecond_ = microsecond;
  }
  void setNanosecond(double nanosecond) {
    setAssigned(TemporalField::Nanosecond);
    nanosecond_ = nanosecond;
  }
  void setOffset(JSString* offset) {
    setAssigned(TemporalField::Offset);
    offset_ = offset;
  }
  void setEra(JSString* era) {
    setAssigned(TemporalField::Era);
    era_ = era;
  }
  void setEraYear(double eraYear) {
    setAssigned(TemporalField::EraYear);
    eraYear_ = eraYear;
  }
  void setTimeZone(const Value& timeZone) {
    setAssigned(TemporalField::TimeZone);
    timeZone_ = timeZone;
  }

  void setMonthOverride(double month) {
    setOverride(TemporalField::Month);
    month_ = month;
  }

  /**
   * Return `true` if the field is present.
   */
  bool has(TemporalField field) const { return fields_.contains(field); }

  /**
   * Return `true` if the field's value is `undefined`. The field must be
   * present.
   */
  bool isUndefined(TemporalField field) const;

  /**
   * Return the set of all present fields.
   */
  mozilla::EnumSet<TemporalField> keys() const { return fields_; }

  /**
   * Mark that `field` is present, but uses its default value. The field must
   * not already be present in `this`.
   */
  void setDefault(TemporalField field) { setAssigned(field); }

  /**
   * Set `field` from `source`. The field must be present and not undefined in
   * `source` and must not already be present in `this`.
   */
  void setFrom(TemporalField field, const TemporalFields& source);

  // Helper methods for WrappedPtrOperations.
  auto monthCodeDoNotUse() const { return &monthCode_; }
  auto offsetDoNotUse() const { return &offset_; }
  auto eraDoNotUse() const { return &era_; }
  auto timeZoneDoNotUse() const { return &timeZone_; }

  // Trace implementation.
  void trace(JSTracer* trc);
};
}  // namespace js::temporal

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::TemporalFields, Wrapper> {
  const temporal::TemporalFields& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  double year() const { return container().year(); }
  double month() const { return container().month(); }
  double day() const { return container().day(); }
  double hour() const { return container().hour(); }
  double minute() const { return container().minute(); }
  double second() const { return container().second(); }
  double millisecond() const { return container().millisecond(); }
  double microsecond() const { return container().microsecond(); }
  double nanosecond() const { return container().nanosecond(); }
  double eraYear() const { return container().eraYear(); }

  JS::Handle<JSString*> monthCode() const {
    return JS::Handle<JSString*>::fromMarkedLocation(
        container().monthCodeDoNotUse());
  }
  JS::Handle<JSString*> offset() const {
    return JS::Handle<JSString*>::fromMarkedLocation(
        container().offsetDoNotUse());
  }
  JS::Handle<JSString*> era() const {
    return JS::Handle<JSString*>::fromMarkedLocation(container().eraDoNotUse());
  }
  JS::Handle<JS::Value> timeZone() const {
    return JS::Handle<JS::Value>::fromMarkedLocation(
        container().timeZoneDoNotUse());
  }

  bool has(temporal::TemporalField field) const {
    return container().has(field);
  }
  bool isUndefined(temporal::TemporalField field) const {
    return container().isUndefined(field);
  }
  auto keys() const { return container().keys(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<temporal::TemporalFields, Wrapper>
    : public WrappedPtrOperations<temporal::TemporalFields, Wrapper> {
  temporal::TemporalFields& container() {
    return static_cast<Wrapper*>(this)->get();
  }

 public:
  void setYear(double year) { container().setYear(year); }
  void setMonth(double month) { container().setMonth(month); }
  void setMonthCode(JSString* monthCode) {
    container().setMonthCode(monthCode);
  }
  void setDay(double day) { container().setDay(day); }
  void setHour(double hour) { container().setHour(hour); }
  void setMinute(double minute) { container().setMinute(minute); }
  void setSecond(double second) { container().setSecond(second); }
  void setMillisecond(double millisecond) {
    container().setMillisecond(millisecond);
  }
  void setMicrosecond(double microsecond) {
    container().setMicrosecond(microsecond);
  }
  void setNanosecond(double nanosecond) {
    container().setNanosecond(nanosecond);
  }
  void setOffset(JSString* offset) { container().setOffset(offset); }
  void setEra(JSString* era) { container().setEra(era); }
  void setEraYear(double eraYear) { container().setEraYear(eraYear); }
  void setTimeZone(const Value& timeZone) { container().setTimeZone(timeZone); }

  void setMonthOverride(double month) { container().setMonthOverride(month); }

  void setDefault(temporal::TemporalField field) {
    container().setDefault(field);
  }
};

}  // namespace js

namespace js::temporal {

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
bool PrepareTemporalFields(JSContext* cx, JS::Handle<TemporalFields> fields,
                           mozilla::EnumSet<TemporalField> fieldNames,
                           mozilla::EnumSet<TemporalField> requiredFields,
                           JS::MutableHandle<TemporalFields> result);

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
inline bool PrepareTemporalFields(JSContext* cx,
                                  JS::Handle<TemporalFields> fields,
                                  mozilla::EnumSet<TemporalField> fieldNames,
                                  JS::MutableHandle<TemporalFields> result) {
  return PrepareTemporalFields(cx, fields, fieldNames, {}, result);
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
inline bool PrepareTemporalFields(
    JSContext* cx, JS::Handle<TemporalFields> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    mozilla::EnumSet<TemporalField> requiredFields,
    const FieldDescriptors& extraFieldDescriptors,
    JS::MutableHandle<TemporalFields> result) {
  return PrepareTemporalFields(
      cx, fields, fieldNames + extraFieldDescriptors.relevant,
      requiredFields + extraFieldDescriptors.required, result);
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
bool PrepareTemporalFields(JSContext* cx, JS::Handle<JSObject*> fields,
                           mozilla::EnumSet<TemporalField> fieldNames,
                           mozilla::EnumSet<TemporalField> requiredFields,
                           JS::MutableHandle<TemporalFields> result);

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
inline bool PrepareTemporalFields(JSContext* cx, JS::Handle<JSObject*> fields,
                                  mozilla::EnumSet<TemporalField> fieldNames,
                                  JS::MutableHandle<TemporalFields> result) {
  return PrepareTemporalFields(cx, fields, fieldNames, {}, result);
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
inline bool PrepareTemporalFields(
    JSContext* cx, JS::Handle<JSObject*> fields,
    mozilla::EnumSet<TemporalField> fieldNames,
    mozilla::EnumSet<TemporalField> requiredFields,
    const FieldDescriptors& extraFieldDescriptors,
    JS::MutableHandle<TemporalFields> result) {
  return PrepareTemporalFields(
      cx, fields, fieldNames + extraFieldDescriptors.relevant,
      requiredFields + extraFieldDescriptors.required, result);
}

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields [ ,
 * extraFieldDescriptors [ , duplicateBehaviour ] ] )
 */
bool PreparePartialTemporalFields(JSContext* cx, JS::Handle<JSObject*> fields,
                                  mozilla::EnumSet<TemporalField> fieldNames,
                                  JS::MutableHandle<TemporalFields> result);

/**
 * PrepareCalendarFieldsAndFieldNames ( calendar, fields, calendarFieldNames [ ,
 * nonCalendarFieldNames [ , requiredFieldNames ] ] )
 */
bool PrepareCalendarFieldsAndFieldNames(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    JS::MutableHandle<TemporalFields> result);

/**
 * PrepareCalendarFields ( calendar, fields, calendarFieldNames,
 * nonCalendarFieldNames, requiredFieldNames )
 */
bool PrepareCalendarFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    mozilla::EnumSet<TemporalField> nonCalendarFieldNames,
    mozilla::EnumSet<TemporalField> requiredFieldNames,
    JS::MutableHandle<TemporalFields> result);

/**
 * PrepareCalendarFields ( calendar, fields, calendarFieldNames,
 * nonCalendarFieldNames, requiredFieldNames )
 */
inline bool PrepareCalendarFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    mozilla::EnumSet<TemporalField> nonCalendarFieldNames,
    JS::MutableHandle<TemporalFields> result) {
  return PrepareCalendarFields(cx, calendar, fields, calendarFieldNames,
                               nonCalendarFieldNames, {}, result);
}

/**
 * PrepareCalendarFields ( calendar, fields, calendarFieldNames,
 * nonCalendarFieldNames, requiredFieldNames )
 */
inline bool PrepareCalendarFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields,
    mozilla::EnumSet<CalendarField> calendarFieldNames,
    JS::MutableHandle<TemporalFields> result) {
  return PrepareCalendarFields(cx, calendar, fields, calendarFieldNames, {}, {},
                               result);
}

} /* namespace js::temporal */

#endif /* builtin_temporal_TemporalFields_h */
