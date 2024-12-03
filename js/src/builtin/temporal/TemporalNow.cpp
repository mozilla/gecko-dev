/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TemporalNow.h"

#include "mozilla/Assertions.h"
#include "mozilla/Result.h"

#include <cstdlib>
#include <stdint.h>
#include <string_view>
#include <utility>

#include "jsdate.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TimeZone.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "gc/Barrier.h"
#include "gc/GCEnum.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/Date.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "vm/DateTime.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::temporal;

/**
 * SystemUTCEpochNanoseconds ( )
 */
static bool SystemUTCEpochNanoseconds(JSContext* cx, EpochNanoseconds* result) {
  // Steps 1-2.
  JS::ClippedTime nowMillis = DateNow(cx);
  MOZ_ASSERT(nowMillis.isValid());
  MOZ_ASSERT(nowMillis.toDouble() >= js::StartOfTime);
  MOZ_ASSERT(nowMillis.toDouble() <= js::EndOfTime);

  // Step 3.
  *result = EpochNanoseconds::fromMilliseconds(int64_t(nowMillis.toDouble()));
  return true;
}

/**
 * SystemDateTime ( temporalTimeZoneLike )
 */
static bool SystemDateTime(JSContext* cx, Handle<Value> temporalTimeZoneLike,
                           ISODateTime* dateTime) {
  // Steps 1-2.
  Rooted<TimeZoneValue> timeZone(cx);
  if (temporalTimeZoneLike.isUndefined()) {
    if (!SystemTimeZone(cx, &timeZone)) {
      return false;
    }
  } else {
    if (!ToTemporalTimeZone(cx, temporalTimeZoneLike, &timeZone)) {
      return false;
    }
  }

  // Step 3.
  EpochNanoseconds epochNs;
  if (!SystemUTCEpochNanoseconds(cx, &epochNs)) {
    return false;
  }

  // Step 4.
  return GetISODateTimeFor(cx, timeZone, epochNs, dateTime);
}

/**
 * Temporal.Now.timeZoneId ( )
 */
static bool Temporal_Now_timeZoneId(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  auto* result = SystemTimeZoneIdentifier(cx);
  if (!result) {
    return false;
  }

  args.rval().setString(result);
  return true;
}

/**
 * Temporal.Now.instant ( )
 */
static bool Temporal_Now_instant(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  EpochNanoseconds epochNs;
  if (!SystemUTCEpochNanoseconds(cx, &epochNs)) {
    return false;
  }

  // Step 2.
  auto* result = CreateTemporalInstant(cx, epochNs);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainDateTimeISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainDateTimeISO(JSContext* cx, unsigned argc,
                                          Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  ISODateTime dateTime;
  if (!SystemDateTime(cx, args.get(0), &dateTime)) {
    return false;
  }

  // Step 2.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  auto* result = CreateTemporalDateTime(cx, dateTime, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.zonedDateTimeISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_zonedDateTimeISO(JSContext* cx, unsigned argc,
                                          Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Steps 1-2.
  Rooted<TimeZoneValue> timeZone(cx);
  if (!args.hasDefined(0)) {
    if (!SystemTimeZone(cx, &timeZone)) {
      return false;
    }
  } else {
    if (!ToTemporalTimeZone(cx, args[0], &timeZone)) {
      return false;
    }
  }

  // Step 3.
  EpochNanoseconds epochNs;
  if (!SystemUTCEpochNanoseconds(cx, &epochNs)) {
    return false;
  }

  // Step 4.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  auto* result = CreateTemporalZonedDateTime(cx, epochNs, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainDateISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainDateISO(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  ISODateTime dateTime;
  if (!SystemDateTime(cx, args.get(0), &dateTime)) {
    return false;
  }

  // Step 2.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  auto* result = CreateTemporalDate(cx, dateTime.date, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainTimeISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainTimeISO(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  ISODateTime dateTime;
  if (!SystemDateTime(cx, args.get(0), &dateTime)) {
    return false;
  }

  // Step 2.
  auto* result = CreateTemporalTime(cx, dateTime.time);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

const JSClass TemporalNowObject::class_ = {
    "Temporal.Now",
    JSCLASS_HAS_CACHED_PROTO(JSProto_TemporalNow),
    JS_NULL_CLASS_OPS,
    &TemporalNowObject::classSpec_,
};

static const JSFunctionSpec TemporalNow_methods[] = {
    JS_FN("timeZoneId", Temporal_Now_timeZoneId, 0, 0),
    JS_FN("instant", Temporal_Now_instant, 0, 0),
    JS_FN("plainDateTimeISO", Temporal_Now_plainDateTimeISO, 0, 0),
    JS_FN("zonedDateTimeISO", Temporal_Now_zonedDateTimeISO, 0, 0),
    JS_FN("plainDateISO", Temporal_Now_plainDateISO, 0, 0),
    JS_FN("plainTimeISO", Temporal_Now_plainTimeISO, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec TemporalNow_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Temporal.Now", JSPROP_READONLY),
    JS_PS_END,
};

static JSObject* CreateTemporalNowObject(JSContext* cx, JSProtoKey key) {
  Rooted<JSObject*> proto(cx, &cx->global()->getObjectPrototype());
  return NewTenuredObjectWithGivenProto(cx, &TemporalNowObject::class_, proto);
}

const ClassSpec TemporalNowObject::classSpec_ = {
    CreateTemporalNowObject,
    nullptr,
    TemporalNow_methods,
    TemporalNow_properties,
    nullptr,
    nullptr,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
