/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainMonthDay.h"

#include "mozilla/Assertions.h"

#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalFields.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/ToString.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/BytecodeUtil.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsPlainMonthDay(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainMonthDayObject>();
}

/**
 * CreateTemporalMonthDay ( isoMonth, isoDay, calendar, referenceISOYear [ ,
 * newTarget ] )
 */
static PlainMonthDayObject* CreateTemporalMonthDay(
    JSContext* cx, const CallArgs& args, double isoYear, double isoMonth,
    double isoDay, Handle<CalendarValue> calendar) {
  MOZ_ASSERT(IsInteger(isoYear));
  MOZ_ASSERT(IsInteger(isoMonth));
  MOZ_ASSERT(IsInteger(isoDay));

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, isoYear, isoMonth, isoDay)) {
    return nullptr;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(isoYear, isoMonth, isoDay)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_MONTH_DAY_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainMonthDay,
                                          &proto)) {
    return nullptr;
  }

  auto* obj = NewObjectWithClassProto<PlainMonthDayObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // Step 5.
  obj->setFixedSlot(PlainMonthDayObject::ISO_MONTH_SLOT,
                    Int32Value(int32_t(isoMonth)));

  // Step 6.
  obj->setFixedSlot(PlainMonthDayObject::ISO_DAY_SLOT,
                    Int32Value(int32_t(isoDay)));

  // Step 7.
  obj->setFixedSlot(PlainMonthDayObject::CALENDAR_SLOT, calendar.toSlotValue());

  // Step 8.
  obj->setFixedSlot(PlainMonthDayObject::ISO_YEAR_SLOT,
                    Int32Value(int32_t(isoYear)));

  // Step 9.
  return obj;
}

/**
 * CreateTemporalMonthDay ( isoMonth, isoDay, calendar, referenceISOYear [ ,
 * newTarget ] )
 */
static PlainMonthDayObject* CreateTemporalMonthDay(
    JSContext* cx, const PlainDate& date, Handle<CalendarValue> calendar) {
  const auto& [isoYear, isoMonth, isoDay] = date;

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, date)) {
    return nullptr;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(date)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_MONTH_DAY_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  auto* obj = NewBuiltinClassInstance<PlainMonthDayObject>(cx);
  if (!obj) {
    return nullptr;
  }

  // Step 5.
  obj->setFixedSlot(PlainMonthDayObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 6.
  obj->setFixedSlot(PlainMonthDayObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 7.
  obj->setFixedSlot(PlainMonthDayObject::CALENDAR_SLOT, calendar.toSlotValue());

  // Step 8.
  obj->setFixedSlot(PlainMonthDayObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 9.
  return obj;
}

/**
 * CreateTemporalMonthDay ( isoMonth, isoDay, calendar, referenceISOYear [ ,
 * newTarget ] )
 */
PlainMonthDayObject* js::temporal::CreateTemporalMonthDay(
    JSContext* cx, Handle<PlainMonthDayWithCalendar> monthDay) {
  MOZ_ASSERT(ISODateTimeWithinLimits(monthDay));
  return CreateTemporalMonthDay(cx, monthDay, monthDay.calendar());
}

/**
 * CreateTemporalMonthDay ( isoMonth, isoDay, calendar, referenceISOYear [ ,
 * newTarget ] )
 */
bool js::temporal::CreateTemporalMonthDay(
    JSContext* cx, const PlainDate& date, Handle<CalendarValue> calendar,
    MutableHandle<PlainMonthDayWithCalendar> result) {
  // Step 1.
  if (!ThrowIfInvalidISODate(cx, date)) {
    return false;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(date)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_MONTH_DAY_INVALID);
    return false;
  }

  // Steps 3-9.
  result.set(PlainMonthDayWithCalendar{date, calendar});
  return true;
}

/**
 * ToTemporalMonthDay ( item [ , overflow ] )
 */
static bool ToTemporalMonthDay(
    JSContext* cx, Handle<JSObject*> item, TemporalOverflow overflow,
    MutableHandle<PlainMonthDayWithCalendar> result) {
  // Step 2.a.
  if (auto* plainMonthDay = item->maybeUnwrapIf<PlainMonthDayObject>()) {
    auto date = ToPlainDate(plainMonthDay);
    Rooted<CalendarValue> calendar(cx, plainMonthDay->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Step 2.a.i.
    result.set(PlainMonthDayWithCalendar{date, calendar});
    return true;
  }

  // FIXME: spec issue - call GetTemporalCalendarSlotValueWithISODefault here
  //
  // https://github.com/tc39/proposal-temporal/pull/2913

  // Steps 2.b-c.
  Rooted<CalendarValue> calendar(cx);
  if (!GetTemporalCalendarWithISODefault(cx, item, &calendar)) {
    return false;
  }

  // Step 2.d.
  Rooted<TemporalFields> fields(cx);
  if (!PrepareCalendarFields(cx, calendar, item,
                             {
                                 CalendarField::Day,
                                 CalendarField::Month,
                                 CalendarField::MonthCode,
                                 CalendarField::Year,
                             },
                             &fields)) {
    return false;
  }

  // Step 2.e.
  return CalendarMonthDayFromFields(cx, calendar, fields, overflow, result);
}

/**
 * ToTemporalMonthDay ( item [ , overflow ] )
 */
static bool ToTemporalMonthDay(
    JSContext* cx, Handle<Value> item, TemporalOverflow overflow,
    MutableHandle<PlainMonthDayWithCalendar> result) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  if (item.isObject()) {
    Rooted<JSObject*> itemObj(cx, &item.toObject());
    return ToTemporalMonthDay(cx, itemObj, overflow, result);
  }

  // Step 3.
  if (!item.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, item,
                     nullptr, "not a string");
    return false;
  }
  Rooted<JSString*> string(cx, item.toString());

  // Step 4.
  PlainDate date;
  bool hasYear;
  Rooted<JSString*> calendarString(cx);
  if (!ParseTemporalMonthDayString(cx, string, &date, &hasYear,
                                   &calendarString)) {
    return false;
  }

  // Steps 5-8.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  if (calendarString) {
    if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
      return false;
    }
  }

  // Step 9.
  if (!hasYear) {
    // Step 9.a.
    MOZ_ASSERT(calendar.identifier() == CalendarId::ISO8601);

    // Step 9.b.
    constexpr int32_t referenceISOYear = 1972;

    // Step 9.c.
    return CreateTemporalMonthDay(cx, {referenceISOYear, date.month, date.day},
                                  calendar, result);
  }

  // Step 10.
  Rooted<PlainMonthDayObject*> obj(cx,
                                   CreateTemporalMonthDay(cx, date, calendar));
  if (!obj) {
    return false;
  }

  // FIXME: spec issue - |obj| should be unobservable.

  // Steps 11-12.
  return CalendarMonthDayFromFields(cx, calendar, obj,
                                    TemporalOverflow::Constrain, result);
}

/**
 * ToTemporalMonthDay ( item [ , overflow ] )
 */
static bool ToTemporalMonthDay(
    JSContext* cx, Handle<Value> item,
    MutableHandle<PlainMonthDayWithCalendar> result) {
  return ToTemporalMonthDay(cx, item, TemporalOverflow::Constrain, result);
}

/**
 * Temporal.PlainMonthDay ( isoMonth, isoDay [ , calendarLike [ ,
 * referenceISOYear ] ] )
 */
static bool PlainMonthDayConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainMonthDay")) {
    return false;
  }

  // Step 3.
  double isoMonth;
  if (!ToIntegerWithTruncation(cx, args.get(0), "month", &isoMonth)) {
    return false;
  }

  // Step 4.
  double isoDay;
  if (!ToIntegerWithTruncation(cx, args.get(1), "day", &isoDay)) {
    return false;
  }

  // Steps 5-8.
  Rooted<CalendarValue> calendar(cx, CalendarValue(CalendarId::ISO8601));
  if (args.hasDefined(2)) {
    // Step 6.
    if (!args[2].isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, args[2],
                       nullptr, "not a string");
      return false;
    }

    // Steps 7-8.
    Rooted<JSString*> calendarString(cx, args[2].toString());
    if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
      return false;
    }
  }

  // Steps 2 and 9.
  double isoYear = 1972;
  if (args.hasDefined(3)) {
    if (!ToIntegerWithTruncation(cx, args[3], "year", &isoYear)) {
      return false;
    }
  }

  // Step 10.
  auto* monthDay =
      CreateTemporalMonthDay(cx, args, isoYear, isoMonth, isoDay, calendar);
  if (!monthDay) {
    return false;
  }

  args.rval().setObject(*monthDay);
  return true;
}

/**
 * Temporal.PlainMonthDay.from ( item [ , options ] )
 */
static bool PlainMonthDay_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Steps 1-2.
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 1.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "from", args[1]));
    if (!options) {
      return false;
    }

    // Step 2.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Steps 3-4.
  Rooted<PlainMonthDayWithCalendar> monthDay(cx);
  if (!ToTemporalMonthDay(cx, args.get(0), overflow, &monthDay)) {
    return false;
  }

  auto* result = CreateTemporalMonthDay(cx, monthDay);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * get Temporal.PlainMonthDay.prototype.calendarId
 */
static bool PlainMonthDay_calendarId(JSContext* cx, const CallArgs& args) {
  auto* monthDay = &args.thisv().toObject().as<PlainMonthDayObject>();

  // Step 3.
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());
  auto* calendarId = ToTemporalCalendarIdentifier(cx, calendar);
  if (!calendarId) {
    return false;
  }

  args.rval().setString(calendarId);
  return true;
}

/**
 * get Temporal.PlainMonthDay.prototype.calendarId
 */
static bool PlainMonthDay_calendarId(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_calendarId>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainMonthDay.prototype.monthCode
 */
static bool PlainMonthDay_monthCode(JSContext* cx, const CallArgs& args) {
  auto* monthDay = &args.thisv().toObject().as<PlainMonthDayObject>();
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 3.
  return CalendarMonthCode(cx, calendar, ToPlainDate(monthDay), args.rval());
}

/**
 * get Temporal.PlainMonthDay.prototype.monthCode
 */
static bool PlainMonthDay_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_monthCode>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainMonthDay.prototype.day
 */
static bool PlainMonthDay_day(JSContext* cx, const CallArgs& args) {
  auto* monthDay = &args.thisv().toObject().as<PlainMonthDayObject>();
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 3.
  return CalendarDay(cx, calendar, ToPlainDate(monthDay), args.rval());
}

/**
 * get Temporal.PlainMonthDay.prototype.day
 */
static bool PlainMonthDay_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_day>(cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.with ( temporalMonthDayLike [ , options ] )
 */
static bool PlainMonthDay_with(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  Rooted<JSObject*> temporalMonthDayLike(
      cx, RequireObjectArg(cx, "temporalMonthDayLike", "with", args.get(0)));
  if (!temporalMonthDayLike) {
    return false;
  }
  if (!ThrowIfTemporalLikeObject(cx, temporalMonthDayLike)) {
    return false;
  }

  // Steps 4-5.
  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 4.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "with", args[1]));
    if (!options) {
      return false;
    }

    // Step 5.
    if (!GetTemporalOverflowOption(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 6.
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 7.
  Rooted<TemporalFields> fields(cx);
  if (!PrepareCalendarFieldsAndFieldNames(cx, calendar, monthDay,
                                          {
                                              CalendarField::Day,
                                              CalendarField::Month,
                                              CalendarField::MonthCode,
                                              CalendarField::Year,
                                          },
                                          &fields)) {
    return false;
  }

  // Step 8.
  Rooted<TemporalFields> partialMonthDay(cx);
  if (!PreparePartialTemporalFields(cx, temporalMonthDayLike, fields.keys(),
                                    &partialMonthDay)) {
    return false;
  }
  MOZ_ASSERT(!partialMonthDay.keys().isEmpty());

  // Step 9.
  Rooted<TemporalFields> mergedFields(
      cx, CalendarMergeFields(calendar, fields, partialMonthDay));

  // Step 10.
  if (!PrepareTemporalFields(cx, mergedFields, fields.keys(), &fields)) {
    return false;
  }

  // Step 11.
  Rooted<PlainMonthDayWithCalendar> result(cx);
  if (!CalendarMonthDayFromFields(cx, calendar, fields, overflow, &result)) {
    return false;
  }

  auto* obj = CreateTemporalMonthDay(cx, result);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.with ( temporalMonthDayLike [ , options ] )
 */
static bool PlainMonthDay_with(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_with>(cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.equals ( other )
 */
static bool PlainMonthDay_equals(JSContext* cx, const CallArgs& args) {
  auto* monthDay = &args.thisv().toObject().as<PlainMonthDayObject>();
  auto date = ToPlainDate(monthDay);
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 3.
  Rooted<PlainMonthDayWithCalendar> other(cx);
  if (!ToTemporalMonthDay(cx, args.get(0), &other)) {
    return false;
  }

  // Steps 4-7.
  bool equals =
      date == other.date() && CalendarEquals(calendar, other.calendar());

  args.rval().setBoolean(equals);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.equals ( other )
 */
static bool PlainMonthDay_equals(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_equals>(cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.toString ( [ options ] )
 */
static bool PlainMonthDay_toString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  auto showCalendar = ShowCalendar::Auto;
  if (args.hasDefined(0)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "toString", args[0]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (!GetTemporalShowCalendarNameOption(cx, options, &showCalendar)) {
      return false;
    }
  }

  // Step 5.
  JSString* str = TemporalMonthDayToString(cx, monthDay, showCalendar);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toString ( [ options ] )
 */
static bool PlainMonthDay_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toString>(cx,
                                                                       args);
}

/**
 * Temporal.PlainMonthDay.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainMonthDay_toLocaleString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  JSString* str = TemporalMonthDayToString(cx, monthDay, ShowCalendar::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainMonthDay_toLocaleString(JSContext* cx, unsigned argc,
                                         Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toLocaleString>(
      cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.toJSON ( )
 */
static bool PlainMonthDay_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  JSString* str = TemporalMonthDayToString(cx, monthDay, ShowCalendar::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toJSON ( )
 */
static bool PlainMonthDay_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toJSON>(cx, args);
}

/**
 *  Temporal.PlainMonthDay.prototype.valueOf ( )
 */
static bool PlainMonthDay_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainMonthDay", "primitive type");
  return false;
}

/**
 * Temporal.PlainMonthDay.prototype.toPlainDate ( item )
 */
static bool PlainMonthDay_toPlainDate(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  Rooted<JSObject*> item(
      cx, RequireObjectArg(cx, "item", "toPlainDate", args.get(0)));
  if (!item) {
    return false;
  }

  // Step 4.
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 5.
  Rooted<TemporalFields> receiverFields(cx);
  if (!PrepareCalendarFieldsAndFieldNames(cx, calendar, monthDay,
                                          {
                                              CalendarField::Day,
                                              CalendarField::MonthCode,
                                          },
                                          &receiverFields)) {
    return false;
  }

  // Step 6.
  Rooted<TemporalFields> inputFields(cx);
  if (!PrepareCalendarFieldsAndFieldNames(cx, calendar, item,
                                          {
                                              CalendarField::Year,
                                          },
                                          &inputFields)) {
    return false;
  }

  // Step 7.
  Rooted<TemporalFields> mergedFields(
      cx, CalendarMergeFields(calendar, receiverFields, inputFields));

  // Step 8.
  auto concatenatedFieldNames = receiverFields.keys() + inputFields.keys();

  // Step 9.
  if (!PrepareTemporalFields(cx, mergedFields, concatenatedFieldNames,
                             &mergedFields)) {
    return false;
  }

  // Step 10.
  Rooted<PlainDateWithCalendar> result(cx);
  if (!CalendarDateFromFields(cx, calendar, mergedFields,
                              TemporalOverflow::Constrain, &result)) {
    return false;
  }

  auto* obj = CreateTemporalDate(cx, result);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toPlainDate ( item )
 */
static bool PlainMonthDay_toPlainDate(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toPlainDate>(cx,
                                                                          args);
}

const JSClass PlainMonthDayObject::class_ = {
    "Temporal.PlainMonthDay",
    JSCLASS_HAS_RESERVED_SLOTS(PlainMonthDayObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainMonthDay),
    JS_NULL_CLASS_OPS,
    &PlainMonthDayObject::classSpec_,
};

const JSClass& PlainMonthDayObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainMonthDay_methods[] = {
    JS_FN("from", PlainMonthDay_from, 1, 0),
    JS_FS_END,
};

static const JSFunctionSpec PlainMonthDay_prototype_methods[] = {
    JS_FN("with", PlainMonthDay_with, 1, 0),
    JS_FN("equals", PlainMonthDay_equals, 1, 0),
    JS_FN("toString", PlainMonthDay_toString, 0, 0),
    JS_FN("toLocaleString", PlainMonthDay_toLocaleString, 0, 0),
    JS_FN("toJSON", PlainMonthDay_toJSON, 0, 0),
    JS_FN("valueOf", PlainMonthDay_valueOf, 0, 0),
    JS_FN("toPlainDate", PlainMonthDay_toPlainDate, 1, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainMonthDay_prototype_properties[] = {
    JS_PSG("calendarId", PlainMonthDay_calendarId, 0),
    JS_PSG("monthCode", PlainMonthDay_monthCode, 0),
    JS_PSG("day", PlainMonthDay_day, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.PlainMonthDay", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec PlainMonthDayObject::classSpec_ = {
    GenericCreateConstructor<PlainMonthDayConstructor, 2,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainMonthDayObject>,
    PlainMonthDay_methods,
    nullptr,
    PlainMonthDay_prototype_methods,
    PlainMonthDay_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
