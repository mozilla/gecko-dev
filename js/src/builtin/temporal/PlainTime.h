/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_PlainTime_h
#define builtin_temporal_PlainTime_h

#include <stdint.h>

#include "builtin/temporal/TemporalTypes.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class PlainTimeObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  // TODO: Consider compacting fields to reduce object size.
  //
  // ceil(log2(24)) + 2 * ceil(log2(60)) + 3 * ceil(log2(1000)) = 47 bits are
  // needed to store a time value in a single int64. 47 bits can be stored as
  // raw bits in a JS::Value.

  static constexpr uint32_t HOUR_SLOT = 0;
  static constexpr uint32_t MINUTE_SLOT = 1;
  static constexpr uint32_t SECOND_SLOT = 2;
  static constexpr uint32_t MILLISECOND_SLOT = 3;
  static constexpr uint32_t MICROSECOND_SLOT = 4;
  static constexpr uint32_t NANOSECOND_SLOT = 5;
  static constexpr uint32_t SLOT_COUNT = 6;

  int32_t hour() const { return getFixedSlot(HOUR_SLOT).toInt32(); }

  int32_t minute() const { return getFixedSlot(MINUTE_SLOT).toInt32(); }

  int32_t second() const { return getFixedSlot(SECOND_SLOT).toInt32(); }

  int32_t millisecond() const {
    return getFixedSlot(MILLISECOND_SLOT).toInt32();
  }

  int32_t microsecond() const {
    return getFixedSlot(MICROSECOND_SLOT).toInt32();
  }

  int32_t nanosecond() const { return getFixedSlot(NANOSECOND_SLOT).toInt32(); }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Extract the time fields from the PlainTime object.
 */
inline PlainTime ToPlainTime(const PlainTimeObject* time) {
  return {time->hour(),        time->minute(),      time->second(),
          time->millisecond(), time->microsecond(), time->nanosecond()};
}

class Increment;
enum class TemporalOverflow;
enum class TemporalRoundingMode;
enum class TemporalUnit;

#ifdef DEBUG
/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool IsValidTime(const PlainTime& time);

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool IsValidTime(double hour, double minute, double second, double millisecond,
                 double microsecond, double nanosecond);
#endif

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool ThrowIfInvalidTime(JSContext* cx, const PlainTime& time);

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool ThrowIfInvalidTime(JSContext* cx, double hour, double minute,
                        double second, double millisecond, double microsecond,
                        double nanosecond);

/**
 * CreateTemporalTime ( hour, minute, second, millisecond, microsecond,
 * nanosecond [ , newTarget ] )
 */
PlainTimeObject* CreateTemporalTime(JSContext* cx, const PlainTime& time);

/**
 * ToTemporalTime ( item [ , overflow ] )
 */
bool ToTemporalTime(JSContext* cx, JS::Handle<JS::Value> item,
                    PlainTime* result);

struct TimeRecord final {
  int64_t days = 0;
  PlainTime time;
};

/**
 * AddTime ( time, timeDuration )
 */
TimeRecord AddTime(const PlainTime& time,
                   const NormalizedTimeDuration& duration);

/**
 * DifferenceTime ( time1, time2 )
 */
NormalizedTimeDuration DifferenceTime(const PlainTime& time1,
                                      const PlainTime& time2);

struct TemporalTimeLike final {
  double hour = 0;
  double minute = 0;
  double second = 0;
  double millisecond = 0;
  double microsecond = 0;
  double nanosecond = 0;
};

/**
 * RegulateTime ( hour, minute, second, millisecond, microsecond, nanosecond,
 * overflow )
 */
bool RegulateTime(JSContext* cx, const TemporalTimeLike& time,
                  TemporalOverflow overflow, PlainTime* result);

/**
 * CompareTimeRecord ( time1, time2 )
 */
int32_t CompareTimeRecord(const PlainTime& one, const PlainTime& two);

/**
 * BalanceTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
TimeRecord BalanceTime(const PlainTime& time, int64_t nanoseconds);

/**
 * RoundTime ( time, increment, unit, roundingMode )
 */
TimeRecord RoundTime(const PlainTime& time, Increment increment,
                     TemporalUnit unit, TemporalRoundingMode roundingMode);

} /* namespace js::temporal */

#endif /* builtin_temporal_PlainTime_h */
