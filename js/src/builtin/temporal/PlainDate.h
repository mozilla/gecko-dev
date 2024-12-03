/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_PlainDate_h
#define builtin_temporal_PlainDate_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <initializer_list>
#include <stdint.h>

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/TemporalTypes.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

class JS_PUBLIC_API JSTracer;

namespace js {
struct ClassSpec;
}  // namespace js

namespace js::temporal {

class PlainDateObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t PACKED_DATE_SLOT = 0;
  static constexpr uint32_t CALENDAR_SLOT = 1;
  static constexpr uint32_t SLOT_COUNT = 2;

  /**
   * Extract the date fields from this PlainDate object.
   */
  PlainDate date() const {
    auto packed = PackedDate{getFixedSlot(PACKED_DATE_SLOT).toPrivateUint32()};
    return PackedDate::unpack(packed);
  }

  CalendarValue calendar() const {
    return CalendarValue(getFixedSlot(CALENDAR_SLOT));
  }

 private:
  static const ClassSpec classSpec_;
};

enum class TemporalOverflow;
enum class TemporalUnit;

#ifdef DEBUG
/**
 * IsValidISODate ( year, month, day )
 */
bool IsValidISODate(const PlainDate& date);
#endif

/**
 * IsValidISODate ( year, month, day )
 */
bool ThrowIfInvalidISODate(JSContext* cx, const PlainDate& date);

/**
 * IsValidISODate ( year, month, day )
 */
bool ThrowIfInvalidISODate(JSContext* cx, double year, double month,
                           double day);

/**
 * ISODateWithinLimits ( isoDate )
 */
bool ISODateWithinLimits(const PlainDate& isoDate);

class MOZ_STACK_CLASS PlainDateWithCalendar final {
  PlainDate date_;
  CalendarValue calendar_;

 public:
  PlainDateWithCalendar() = default;

  PlainDateWithCalendar(const PlainDate& date, const CalendarValue& calendar)
      : date_(date), calendar_(calendar) {
    MOZ_ASSERT(ISODateWithinLimits(date));
  }

  explicit PlainDateWithCalendar(const PlainDateObject* date)
      : PlainDateWithCalendar(date->date(), date->calendar()) {}

  const auto& date() const { return date_; }
  const auto& calendar() const { return calendar_; }

  // Allow implicit conversion to a calendar-less PlainDate.
  operator const PlainDate&() const { return date(); }

  explicit operator bool() const { return !!calendar_; }

  void trace(JSTracer* trc) { calendar_.trace(trc); }

  const auto* calendarDoNotUse() const { return &calendar_; }
};

/**
 * CreateTemporalDate ( isoDate, calendar [ , newTarget ] )
 */
PlainDateObject* CreateTemporalDate(JSContext* cx, const PlainDate& isoDate,
                                    JS::Handle<CalendarValue> calendar);

/**
 * CreateTemporalDate ( isoDate, calendar [ , newTarget ] )
 */
PlainDateObject* CreateTemporalDate(JSContext* cx,
                                    JS::Handle<PlainDateWithCalendar> date);

/**
 * CreateTemporalDate ( isoDate, calendar [ , newTarget ] )
 */
bool CreateTemporalDate(JSContext* cx, const PlainDate& isoDate,
                        JS::Handle<CalendarValue> calendar,
                        JS::MutableHandle<PlainDateWithCalendar> result);

/**
 * RegulateISODate ( year, month, day, overflow )
 */
bool RegulateISODate(JSContext* cx, int32_t year, double month, double day,
                     TemporalOverflow overflow, PlainDate* result);

/**
 * AddISODate ( year, month, day, years, months, weeks, days, overflow )
 */
bool AddISODate(JSContext* cx, const PlainDate& date,
                const DateDuration& duration, TemporalOverflow overflow,
                PlainDate* result);

/**
 * DifferenceISODate ( y1, m1, d1, y2, m2, d2, largestUnit )
 */
DateDuration DifferenceISODate(const PlainDate& start, const PlainDate& end,
                               TemporalUnit largestUnit);

/**
 * CompareISODate ( y1, m1, d1, y2, m2, d2 )
 */
int32_t CompareISODate(const PlainDate& one, const PlainDate& two);

/**
 * BalanceISODate ( year, month, day )
 */
bool BalanceISODate(JSContext* cx, const PlainDate& date, int64_t days,
                    PlainDate* result);

/**
 * BalanceISODate ( year, month, day )
 */
PlainDate BalanceISODate(const PlainDate& date, int32_t days);

} /* namespace js::temporal */

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::PlainDateWithCalendar, Wrapper> {
  const auto& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  explicit operator bool() const { return bool(container()); }

  const auto& date() const { return container().date(); }

  JS::Handle<temporal::CalendarValue> calendar() const {
    return JS::Handle<temporal::CalendarValue>::fromMarkedLocation(
        container().calendarDoNotUse());
  }

  // Allow implicit conversion to a calendar-less PlainDate.
  operator const temporal::PlainDate&() const { return date(); }
};

}  // namespace js

#endif /* builtin_temporal_PlainDate_h */
