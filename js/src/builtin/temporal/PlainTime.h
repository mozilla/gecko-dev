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

  static constexpr uint32_t PACKED_TIME_SLOT = 0;
  static constexpr uint32_t SLOT_COUNT = 1;

  /**
   * Extract the time fields from this PlainTime object.
   */
  PlainTime time() const {
    auto packed = PackedTime{mozilla::BitwiseCast<uint64_t>(
        getFixedSlot(PACKED_TIME_SLOT).toDouble())};
    return PackedTime::unpack(packed);
  }

 private:
  static const ClassSpec classSpec_;
};

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
 * CreateTemporalTime ( time [ , newTarget ] )
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
