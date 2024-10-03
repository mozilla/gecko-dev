/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TimeZone_h
#define builtin_temporal_TimeZone_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"

#include <array>
#include <stddef.h>
#include <stdint.h>

#include "builtin/temporal/TemporalTypes.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/JSObject.h"
#include "vm/NativeObject.h"
#include "vm/StringType.h"

class JSLinearString;
class JS_PUBLIC_API JSTracer;
struct JSClassOps;

namespace js {
struct ClassSpec;
}

namespace mozilla::intl {
class TimeZone;
}

namespace js::temporal {

class BuiltinTimeZoneObject : public NativeObject {
 public:
  static const JSClass class_;

  static constexpr uint32_t IDENTIFIER_SLOT = 0;
  static constexpr uint32_t OFFSET_MINUTES_SLOT = 1;
  static constexpr uint32_t INTL_TIMEZONE_SLOT = 2;
  static constexpr uint32_t SLOT_COUNT = 3;

  // Estimated memory use for intl::TimeZone (see IcuMemoryUsage).
  static constexpr size_t EstimatedMemoryUse = 6840;

  JSLinearString* identifier() const {
    return &getFixedSlot(IDENTIFIER_SLOT).toString()->asLinear();
  }

  const auto& offsetMinutes() const {
    return getFixedSlot(OFFSET_MINUTES_SLOT);
  }

  mozilla::intl::TimeZone* getTimeZone() const {
    const auto& slot = getFixedSlot(INTL_TIMEZONE_SLOT);
    if (slot.isUndefined()) {
      return nullptr;
    }
    return static_cast<mozilla::intl::TimeZone*>(slot.toPrivate());
  }

  void setTimeZone(mozilla::intl::TimeZone* timeZone) {
    setFixedSlot(INTL_TIMEZONE_SLOT, JS::PrivateValue(timeZone));
  }

 private:
  static const JSClassOps classOps_;

  static void finalize(JS::GCContext* gcx, JSObject* obj);
};

} /* namespace js::temporal */

namespace js::temporal {

/**
 * Temporal time zones can be either canonical time zone identifiers or time
 * zone offset strings.
 *
 * Examples of valid Temporal time zones:
 * - "UTC"
 * - "America/New_York"
 * - "+00:00"
 *
 * Examples of invalid Temporal time zones:
 * - "utc" (wrong case)
 * - "Etc/UTC" (canonical name is "UTC")
 * - "+00" (missing minutes part)
 * - "+00:00:00" (sub-minute precision)
 * - "+00:00:01" (sub-minute precision)
 * - "-00:00" (wrong sign for zero offset)
 *
 * The following two implementation approaches are possible:
 *
 * 1. Represent time zones as JSStrings. Additionally keep a mapping from
 *    JSString to `mozilla::intl::TimeZone` to avoid repeatedly creating new
 *    `mozilla::intl::TimeZone` for time zone operations. Offset string time
 *    zones have to be special cased, because they don't use
 *    `mozilla::intl::TimeZone`. Either detect offset strings by checking the
 *    time zone identifier or store offset strings as the offset in minutes
 *    value to avoid reparsing the offset string again and again.
 * 2. Represent time zones as objects which hold `mozilla::intl::TimeZone` in
 *    an internal slot.
 *
 * Option 2 is a bit easier to implement, so we use this approach for now.
 */
class MOZ_STACK_CLASS TimeZoneValue final {
  BuiltinTimeZoneObject* object_ = nullptr;

 public:
  /**
   * Default initialize this TimeZoneValue.
   */
  TimeZoneValue() = default;

  /**
   * Initialize this TimeZoneValue with a built-in time zone object.
   */
  explicit TimeZoneValue(BuiltinTimeZoneObject* timeZone) : object_(timeZone) {
    MOZ_ASSERT(object_);
  }

  /**
   * Initialize this TimeZoneValue from a slot Value.
   */
  explicit TimeZoneValue(const JS::Value& value)
      : object_(&value.toObject().as<BuiltinTimeZoneObject>()) {}

  /**
   * Return true if this TimeZoneValue is not null.
   */
  explicit operator bool() const { return !!object_; }

  /**
   * Return true if this TimeZoneValue is an offset time zone.
   */
  bool isOffset() const {
    MOZ_ASSERT(object_);
    return object_->offsetMinutes().isInt32();
  }

  /**
   * Return the offset of an offset time zone.
   */
  auto offsetMinutes() const {
    MOZ_ASSERT(isOffset());
    return object_->offsetMinutes().toInt32();
  }

  /**
   * Return the time zone identifier.
   */
  auto* identifier() const {
    MOZ_ASSERT(object_);
    return object_->identifier();
  }

  /**
   * Return the time zone implementation.
   */
  auto* getTimeZone() const {
    MOZ_ASSERT(object_);
    return object_->getTimeZone();
  }

  /**
   * Return the underlying BuiltinTimeZoneObject.
   */
  auto* toBuiltinTimeZoneObject() const {
    MOZ_ASSERT(object_);
    return object_;
  }

  /**
   * Return the slot Value representation of this TimeZoneValue.
   */
  JS::Value toSlotValue() const {
    MOZ_ASSERT(object_);
    return JS::ObjectValue(*object_);
  }

  // Helper methods for (Mutable)WrappedPtrOperations.
  auto address() { return &object_; }
  auto address() const { return &object_; }

  // Trace implementation.
  void trace(JSTracer* trc);
};

class PossibleInstants final {
  // GetPossibleInstantsFor can return up-to two elements.
  static constexpr size_t MaxLength = 2;

  std::array<Instant, MaxLength> array_ = {};
  size_t length_ = 0;

  void append(const Instant& instant) { array_[length_++] = instant; }

 public:
  PossibleInstants() = default;

  explicit PossibleInstants(const Instant& instant) { append(instant); }

  explicit PossibleInstants(const Instant& earlier, const Instant& later) {
    MOZ_ASSERT(earlier <= later);
    append(earlier);
    append(later);
  }

  size_t length() const { return length_; }
  bool empty() const { return length_ == 0; }

  const auto& operator[](size_t i) const { return array_[i]; }

  auto begin() const { return array_.begin(); }
  auto end() const { return array_.begin() + length_; }

  const auto& front() const {
    MOZ_ASSERT(length_ > 0);
    return array_[0];
  }
  const auto& back() const {
    MOZ_ASSERT(length_ > 0);
    return array_[length_ - 1];
  }
};

struct ParsedTimeZone;
struct PlainDateTime;
enum class TemporalDisambiguation;

/**
 * IsValidTimeZoneName ( timeZone )
 * IsAvailableTimeZoneName ( timeZone )
 */
bool IsValidTimeZoneName(JSContext* cx, JS::Handle<JSLinearString*> timeZone,
                         JS::MutableHandle<JSAtom*> validatedTimeZone);

/**
 * CanonicalizeTimeZoneName ( timeZone )
 */
JSLinearString* CanonicalizeTimeZoneName(JSContext* cx,
                                         JS::Handle<JSLinearString*> timeZone);

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
BuiltinTimeZoneObject* CreateTemporalTimeZone(
    JSContext* cx, JS::Handle<JSLinearString*> identifier);

/**
 * ToTemporalTimeZoneSlotValue ( temporalTimeZoneLike )
 */
bool ToTemporalTimeZone(JSContext* cx,
                        JS::Handle<JS::Value> temporalTimeZoneLike,
                        JS::MutableHandle<TimeZoneValue> result);

/**
 * ToTemporalTimeZoneSlotValue ( temporalTimeZoneLike )
 */
bool ToTemporalTimeZone(JSContext* cx, JS::Handle<ParsedTimeZone> string,
                        JS::MutableHandle<TimeZoneValue> result);

/**
 * TimeZoneEquals ( one, two )
 */
bool TimeZoneEquals(const TimeZoneValue& one, const TimeZoneValue& two);

/**
 * GetPlainDateTimeFor ( timeZoneRec, instant, calendar [ ,
 * precalculatedOffsetNanoseconds ] )
 */
PlainDateTime GetPlainDateTimeFor(const Instant& instant,
                                  int64_t offsetNanoseconds);

/**
 * GetPlainDateTimeFor ( timeZoneRec, instant, calendar [ ,
 * precalculatedOffsetNanoseconds ] )
 */
bool GetPlainDateTimeFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                         const Instant& instant, PlainDateTime* result);

/**
 * GetInstantFor ( timeZoneRec, dateTime, disambiguation )
 */
bool GetInstantFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                   const PlainDateTime& dateTime,
                   TemporalDisambiguation disambiguation, Instant* result);

/**
 * FormatUTCOffsetNanoseconds ( offsetNanoseconds )
 */
JSString* FormatUTCOffsetNanoseconds(JSContext* cx, int64_t offsetNanoseconds);

/**
 * GetOffsetStringFor ( timeZoneRec, instant )
 */
JSString* GetOffsetStringFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                             const Instant& instant);

/**
 * GetOffsetNanosecondsFor ( timeZoneRec, instant )
 */
bool GetOffsetNanosecondsFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                             const Instant& instant,
                             int64_t* offsetNanoseconds);

/**
 * GetPossibleInstantsFor ( timeZoneRec, dateTime )
 */
bool GetPossibleInstantsFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                            const PlainDateTime& dateTime,
                            PossibleInstants* result);

/**
 * DisambiguatePossibleInstants ( possibleInstants, timeZoneRec, dateTime,
 * disambiguation )
 */
bool DisambiguatePossibleInstants(JSContext* cx,
                                  const PossibleInstants& possibleInstants,
                                  JS::Handle<TimeZoneValue> timeZone,
                                  const PlainDateTime& dateTime,
                                  TemporalDisambiguation disambiguation,
                                  Instant* result);

/**
 * GetNamedTimeZoneNextTransition ( timeZoneIdentifier, epochNanoseconds )
 */
bool GetNamedTimeZoneNextTransition(JSContext* cx,
                                    JS::Handle<TimeZoneValue> timeZone,
                                    const Instant& epochInstant,
                                    mozilla::Maybe<Instant>* result);

/**
 * GetNamedTimeZonePreviousTransition ( timeZoneIdentifier, epochNanoseconds )
 */
bool GetNamedTimeZonePreviousTransition(JSContext* cx,
                                        JS::Handle<TimeZoneValue> timeZone,
                                        const Instant& epochInstant,
                                        mozilla::Maybe<Instant>* result);

// Helper for MutableWrappedPtrOperations.
bool WrapTimeZoneValueObject(
    JSContext* cx, JS::MutableHandle<BuiltinTimeZoneObject*> timeZone);

} /* namespace js::temporal */

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::TimeZoneValue, Wrapper> {
  const auto& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  explicit operator bool() const { return !!container(); }

  bool isOffset() const { return container().isOffset(); }

  auto offsetMinutes() const { return container().offsetMinutes(); }

  auto* identifier() const { return container().identifier(); }

  auto* getTimeZone() const { return container().getTimeZone(); }

  JS::Value toSlotValue() const { return container().toSlotValue(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<temporal::TimeZoneValue, Wrapper>
    : public WrappedPtrOperations<temporal::TimeZoneValue, Wrapper> {
  auto& container() { return static_cast<Wrapper*>(this)->get(); }

 public:
  /**
   * Wrap the time zone value into the current compartment.
   */
  bool wrap(JSContext* cx) {
    MOZ_ASSERT(container());
    auto mh =
        JS::MutableHandle<temporal::BuiltinTimeZoneObject*>::fromMarkedLocation(
            container().address());
    return temporal::WrapTimeZoneValueObject(cx, mh);
  }
};

} /* namespace js */

#endif /* builtin_temporal_TimeZone_h */
