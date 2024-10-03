/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_Calendar_h
#define builtin_temporal_Calendar_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"

#include <initializer_list>
#include <stdint.h>

#include "builtin/temporal/Wrapped.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"
#include "vm/StringType.h"

class JS_PUBLIC_API JSTracer;

namespace js {
struct ClassSpec;
class PlainObject;
}  // namespace js

namespace js::temporal {

enum class CalendarId : int32_t {
  ISO8601,

  // Thai Buddhist solar calendar.
  Buddhist,

  // Chinese lunisolar calendar.
  Chinese,

  // Coptic calendar.
  Coptic,

  // Korean lunisolar calendar.
  Dangi,

  // Ethiopian Amete Mihret calendar.
  Ethiopian,

  // Ethiopian Amete Alem calendar.
  EthiopianAmeteAlem,

  // Gregorian calendar.
  Gregorian,

  // Hebrew lunisolar calendar.
  Hebrew,

  // Indian national calendar.
  Indian,

  // Islamic lunar calendars.
  Islamic,
  IslamicCivil,
  IslamicRGSA,
  IslamicTabular,
  IslamicUmmAlQura,

  // Japanese calendar.
  Japanese,

  // Persian solar Hijri calendar.
  Persian,

  // Republic of China (ROC) calendar.
  ROC,
};

inline constexpr auto availableCalendars = {
    CalendarId::ISO8601,
    CalendarId::Buddhist,
    CalendarId::Chinese,
    CalendarId::Coptic,
    CalendarId::Dangi,
    CalendarId::Ethiopian,
    CalendarId::EthiopianAmeteAlem,
    CalendarId::Gregorian,
    CalendarId::Hebrew,
    CalendarId::Indian,
    CalendarId::Islamic,
    CalendarId::IslamicCivil,
    CalendarId::IslamicRGSA,
    CalendarId::IslamicTabular,
    CalendarId::IslamicUmmAlQura,
    CalendarId::Japanese,
    CalendarId::Persian,
    CalendarId::ROC,
};

/**
 * AvailableCalendars ( )
 */
constexpr auto& AvailableCalendars() { return availableCalendars; }

class CalendarObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t IDENTIFIER_SLOT = 0;
  static constexpr uint32_t SLOT_COUNT = 1;

  CalendarId identifier() const {
    return static_cast<CalendarId>(getFixedSlot(IDENTIFIER_SLOT).toInt32());
  }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Calendar value, which is a string containing a canonical calendar identifier.
 */
class MOZ_STACK_CLASS CalendarValue final {
  JS::Value value_{};

 public:
  /**
   * Default initialize this CalendarValue.
   */
  CalendarValue() = default;

  /**
   * Default initialize this CalendarValue.
   */
  explicit CalendarValue(const JS::Value& value) : value_(value) {
    MOZ_ASSERT(value.isInt32());
  }

  /**
   * Initialize this CalendarValue with a canonical calendar identifier.
   */
  explicit CalendarValue(CalendarId calendarId)
      : value_(JS::Int32Value(static_cast<int32_t>(calendarId))) {}

  /**
   * Return true iff this CalendarValue is initialized with either a canonical
   * calendar identifier or a calendar object.
   */
  explicit operator bool() const { return !value_.isUndefined(); }

  /**
   * Return the slot Value representation of this CalendarValue.
   */
  JS::Value toSlotValue() const { return value_; }

  /**
   * Return the calendar identifier.
   */
  CalendarId identifier() const {
    return static_cast<CalendarId>(value_.toInt32());
  }

  void trace(JSTracer* trc);

  JS::Value* valueDoNotUse() { return &value_; }
  JS::Value const* valueDoNotUse() const { return &value_; }
};

class MOZ_STACK_CLASS CalendarRecord final {
  CalendarValue receiver_;

 public:
  /**
   * Default initialize this CalendarRecord.
   */
  CalendarRecord() = default;

  explicit CalendarRecord(const CalendarValue& receiver)
      : receiver_(receiver) {}

  const auto& receiver() const { return receiver_; }

  // Helper methods for (Mutable)WrappedPtrOperations.
  auto* receiverDoNotUse() const { return &receiver_; }

  // Trace implementation.
  void trace(JSTracer* trc);
};

struct DateDuration;
struct Duration;
struct PlainDate;
struct PlainDateTime;
class DurationObject;
class PlainDateObject;
class PlainDateWithCalendar;
class PlainMonthDayWithCalendar;
class PlainYearMonthWithCalendar;
enum class TemporalOverflow;
enum class TemporalUnit;

/**
 * ISODaysInYear ( year )
 */
int32_t ISODaysInYear(int32_t year);

/**
 * ISODaysInMonth ( year, month )
 */
int32_t ISODaysInMonth(int32_t year, int32_t month);

/**
 * ISODaysInMonth ( year, month )
 */
int32_t ISODaysInMonth(double year, int32_t month);

/**
 * ToISODayOfYear ( year, month, day )
 */
int32_t ToISODayOfYear(const PlainDate& date);

/**
 * 21.4.1.12 MakeDay ( year, month, date )
 */
int32_t MakeDay(const PlainDate& date);

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t MakeDate(const PlainDateTime& dateTime);

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t MakeDate(int32_t year, int32_t month, int32_t day);

/**
 * Return the case-normalized calendar identifier if |id| is a built-in calendar
 * identifier. Otherwise throws a RangeError.
 */
bool ToBuiltinCalendar(JSContext* cx, JS::Handle<JSString*> id,
                       JS::MutableHandle<CalendarValue> result);

/**
 * ToTemporalCalendarSlotValue ( temporalCalendarLike [ , default ] )
 */
bool ToTemporalCalendar(JSContext* cx,
                        JS::Handle<JS::Value> temporalCalendarLike,
                        JS::MutableHandle<CalendarValue> result);

/**
 * ToTemporalCalendarSlotValue ( temporalCalendarLike [ , default ] )
 */
bool ToTemporalCalendarWithISODefault(
    JSContext* cx, JS::Handle<JS::Value> temporalCalendarLike,
    JS::MutableHandle<CalendarValue> result);

/**
 * GetTemporalCalendarWithISODefault ( item )
 */
bool GetTemporalCalendarWithISODefault(JSContext* cx,
                                       JS::Handle<JSObject*> item,
                                       JS::MutableHandle<CalendarValue> result);

/**
 * ToTemporalCalendarIdentifier ( calendarSlotValue )
 */
std::string_view ToTemporalCalendarIdentifier(const CalendarValue& calendar);

/**
 * ToTemporalCalendarIdentifier ( calendarSlotValue )
 */
JSLinearString* ToTemporalCalendarIdentifier(
    JSContext* cx, JS::Handle<CalendarValue> calendar);

enum class CalendarField {
  Year,
  Month,
  MonthCode,
  Day,
};

using CalendarFieldNames = JS::StackGCVector<JS::PropertyKey>;

/**
 * CalendarFields ( calendarRec, fieldNames )
 */
bool CalendarFields(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                    mozilla::EnumSet<CalendarField> fieldNames,
                    JS::MutableHandle<CalendarFieldNames> result);

/**
 * CalendarMergeFields ( calendar, fields, additionalFields )
 */
PlainObject* CalendarMergeFields(JSContext* cx,
                                 JS::Handle<CalendarValue> calendar,
                                 JS::Handle<PlainObject*> fields,
                                 JS::Handle<PlainObject*> additionalFields);

/**
 * CalendarDateAdd ( calendarRec, date, duration [ , options ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarRecord> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date, const DateDuration& duration);

/**
 * CalendarDateAdd ( calendarRec, date, duration [ , options ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarRecord> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    JS::Handle<JSObject*> options);

/**
 * CalendarDateAdd ( calendarRec, date, duration [ , options ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarRecord> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date,
    JS::Handle<Wrapped<DurationObject*>> duration);

/**
 * CalendarDateAdd ( calendarRec, date, duration [ , options ] )
 */
Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, JS::Handle<CalendarRecord> calendar,
    JS::Handle<Wrapped<PlainDateObject*>> date,
    JS::Handle<Wrapped<DurationObject*>> duration,
    JS::Handle<JSObject*> options);

/**
 * CalendarDateAdd ( calendarRec, date, duration [ , options ] )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                     const PlainDate& date, const DateDuration& duration,
                     PlainDate* result);

/**
 * CalendarDateAdd ( calendarRec, date, duration [ , options ] )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                     const PlainDate& date, const DateDuration& duration,
                     JS::Handle<JSObject*> options, PlainDate* result);

/**
 * CalendarDateAdd ( calendarRec, date, duration [ , options ] )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                     JS::Handle<Wrapped<PlainDateObject*>> date,
                     const DateDuration& duration, PlainDate* result);

/**
 * CalendarDateAdd ( date, duration, overflow )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarValue> calendar,
                     const PlainDate& date, const Duration& duration,
                     TemporalOverflow overflow, PlainDate* result);

/**
 * CalendarDateAdd ( date, duration, overflow )
 */
bool CalendarDateAdd(JSContext* cx, JS::Handle<CalendarValue> calendar,
                     const PlainDate& date, const DateDuration& duration,
                     TemporalOverflow overflow, PlainDate* result);

/**
 * CalendarDateUntil ( one, two, largestUnit )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       const PlainDate& one, const PlainDate& two,
                       TemporalUnit largestUnit, DateDuration* result);

/**
 * CalendarDateUntil ( calendarRec, one, two, options )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                       const PlainDate& one, const PlainDate& two,
                       TemporalUnit largestUnit, DateDuration* result);

/**
 * CalendarDateUntil ( calendarRec, one, two, options )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                       const PlainDate& one, const PlainDate& two,
                       TemporalUnit largestUnit,
                       JS::Handle<PlainObject*> options, DateDuration* result);

/**
 * CalendarDateUntil ( calendarRec, one, two, options )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                       JS::Handle<Wrapped<PlainDateObject*>> one,
                       JS::Handle<Wrapped<PlainDateObject*>> two,
                       TemporalUnit largestUnit, DateDuration* result);

/**
 * CalendarDateUntil ( calendarRec, one, two, options )
 */
bool CalendarDateUntil(JSContext* cx, JS::Handle<CalendarRecord> calendar,
                       JS::Handle<Wrapped<PlainDateObject*>> one,
                       JS::Handle<Wrapped<PlainDateObject*>> two,
                       TemporalUnit largestUnit,
                       JS::Handle<PlainObject*> options, DateDuration* result);

/**
 * CalendarEra ( dateLike )
 */
bool CalendarEra(JSContext* cx, JS::Handle<CalendarValue> calendar,
                 const PlainDate& date, JS::MutableHandle<JS::Value> result);

/**
 * CalendarEraYear ( dateLike )
 */
bool CalendarEraYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                     const PlainDate& date,
                     JS::MutableHandle<JS::Value> result);
/**
 * CalendarYear ( dateLike )
 */
bool CalendarYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                  const PlainDate& date, JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonth ( dateLike )
 */
bool CalendarMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                   const PlainDate& date, JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthCode ( dateLike )
 */
bool CalendarMonthCode(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       const PlainDate& date,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDay ( dateLike )
 */
bool CalendarDay(JSContext* cx, JS::Handle<CalendarValue> calendar,
                 const PlainDate& date, JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfWeek ( dateLike )
 */
bool CalendarDayOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       const PlainDate& date,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarDayOfYear ( dateLike )
 */
bool CalendarDayOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                       const PlainDate& date,
                       JS::MutableHandle<JS::Value> result);

/**
 * CalendarWeekOfYear ( dateLike )
 */
bool CalendarWeekOfYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDate& date,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarYearOfWeek ( dateLike )
 */
bool CalendarYearOfWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDate& date,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInWeek ( dateLike )
 */
bool CalendarDaysInWeek(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDate& date,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInMonth ( dateLike )
 */
bool CalendarDaysInMonth(JSContext* cx, JS::Handle<CalendarValue> calendar,
                         const PlainDate& date,
                         JS::MutableHandle<JS::Value> result);

/**
 * CalendarDaysInYear ( dateLike )
 */
bool CalendarDaysInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDate& date,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarMonthsInYear ( dateLike )
 */
bool CalendarMonthsInYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                          const PlainDate& date,
                          JS::MutableHandle<JS::Value> result);

/**
 * CalendarInLeapYear ( dateLike )
 */
bool CalendarInLeapYear(JSContext* cx, JS::Handle<CalendarValue> calendar,
                        const PlainDate& date,
                        JS::MutableHandle<JS::Value> result);

/**
 * CalendarDateFromFields ( calendar, fields, overflow )
 */
bool CalendarDateFromFields(JSContext* cx, JS::Handle<CalendarValue> calendar,
                            JS::Handle<PlainObject*> fields,
                            TemporalOverflow overflow,
                            MutableHandle<PlainDateWithCalendar> result);

/**
 * CalendarYearMonthFromFields ( calendar, fields, overflow )
 */
bool CalendarYearMonthFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields, TemporalOverflow overflow,
    JS::MutableHandle<PlainYearMonthWithCalendar> result);

/**
 * CalendarMonthDayFromFields ( calendar, fields, overflow )
 */
bool CalendarMonthDayFromFields(
    JSContext* cx, JS::Handle<CalendarValue> calendar,
    JS::Handle<JSObject*> fields, TemporalOverflow overflow,
    JS::MutableHandle<PlainMonthDayWithCalendar> result);

/**
 * CalendarEquals ( one, two )
 */
inline bool CalendarEquals(const CalendarValue& one, const CalendarValue& two) {
  // Steps 1-2.
  return one.identifier() == two.identifier();
}

/**
 * CreateCalendarMethodsRecord ( calendar, methods )
 */
bool CreateCalendarMethodsRecord(JSContext* cx,
                                 JS::Handle<CalendarValue> calendar,
                                 JS::MutableHandle<CalendarRecord> result);

// Helper for MutableWrappedPtrOperations.
bool WrapCalendarValue(JSContext* cx, JS::MutableHandle<JS::Value> calendar);

} /* namespace js::temporal */

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::CalendarValue, Wrapper> {
  const auto& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  explicit operator bool() const { return bool(container()); }

  JS::Handle<JS::Value> toSlotValue() const {
    return JS::Handle<JS::Value>::fromMarkedLocation(
        container().valueDoNotUse());
  }

  temporal::CalendarId identifier() const { return container().identifier(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<temporal::CalendarValue, Wrapper>
    : public WrappedPtrOperations<temporal::CalendarValue, Wrapper> {
  auto& container() { return static_cast<Wrapper*>(this)->get(); }

  JS::MutableHandle<JS::Value> toMutableValue() {
    return JS::MutableHandle<JS::Value>::fromMarkedLocation(
        container().valueDoNotUse());
  }

 public:
  bool wrap(JSContext* cx) {
    return temporal::WrapCalendarValue(cx, toMutableValue());
  }
};

template <typename Wrapper>
class WrappedPtrOperations<temporal::CalendarRecord, Wrapper> {
  const auto& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  JS::Handle<temporal::CalendarValue> receiver() const {
    return JS::Handle<temporal::CalendarValue>::fromMarkedLocation(
        container().receiverDoNotUse());
  }
};

} /* namespace js */

#endif /* builtin_temporal_Calendar_h */
