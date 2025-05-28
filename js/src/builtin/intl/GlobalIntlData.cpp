/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/intl/GlobalIntlData.h"

#include "mozilla/Assertions.h"
#include "mozilla/Span.h"

#include "builtin/intl/Collator.h"
#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/DateTimeFormat.h"
#include "builtin/intl/FormatBuffer.h"
#include "builtin/intl/NumberFormat.h"
#include "gc/Tracer.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/Value.h"
#include "vm/DateTime.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"

#include "vm/JSObject-inl.h"

using namespace js;

void js::intl::GlobalIntlData::resetCollator() {
  collatorLocale_ = nullptr;
  collator_ = nullptr;
}

void js::intl::GlobalIntlData::resetNumberFormat() {
  numberFormatLocale_ = nullptr;
  numberFormat_ = nullptr;
}

void js::intl::GlobalIntlData::resetDateTimeFormat() {
  dateTimeFormatLocale_ = nullptr;
  dateTimeFormatToLocaleAll_ = nullptr;
  dateTimeFormatToLocaleDate_ = nullptr;
  dateTimeFormatToLocaleTime_ = nullptr;
}

bool js::intl::GlobalIntlData::ensureRuntimeDefaultLocale(JSContext* cx) {
  const char* locale = cx->realm()->getLocale();
  if (!locale) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (!runtimeDefaultLocale_ ||
      !StringEqualsAscii(runtimeDefaultLocale_, locale)) {
    runtimeDefaultLocale_ = NewStringCopyZ<CanGC>(cx, locale);
    if (!runtimeDefaultLocale_) {
      return false;
    }

    // Clear all cached instances when the runtime default locale has changed.
    resetCollator();
    resetNumberFormat();
    resetDateTimeFormat();
  }

  return true;
}

static bool StringEqualsTwoByte(const JSLinearString* str,
                                mozilla::Span<const char16_t> chars) {
  if (str->length() != chars.size()) {
    return false;
  }

  JS::AutoCheckCannotGC nogc;
  return str->hasLatin1Chars()
             ? EqualChars(str->latin1Chars(nogc), chars.data(), chars.size())
             : EqualChars(str->twoByteChars(nogc), chars.data(), chars.size());
}

bool js::intl::GlobalIntlData::ensureRuntimeDefaultTimeZone(JSContext* cx) {
  FormatBuffer<char16_t, INITIAL_CHAR_BUFFER_SIZE> timeZone(cx);
  auto result =
      DateTimeInfo::timeZoneId(DateTimeInfo::forceUTC(cx->realm()), timeZone);
  if (result.isErr()) {
    ReportInternalError(cx, result.unwrapErr());
    return false;
  }

  if (!runtimeDefaultTimeZone_ ||
      !StringEqualsTwoByte(runtimeDefaultTimeZone_, timeZone)) {
    JSLinearString* str = timeZone.toString(cx);
    if (!str) {
      return false;
    }

    runtimeDefaultTimeZone_ = str;

    // Clear all cached DateTimeFormat instances when the time zone has changed.
    resetDateTimeFormat();
  }

  return true;
}

static inline bool EqualLocale(const JSLinearString* str1,
                               const JSLinearString* str2) {
  if (str1 && str2) {
    return EqualStrings(str1, str2);
  }
  return !str1 && !str2;
}

static inline Value LocaleOrDefault(JSLinearString* locale) {
  if (locale) {
    return StringValue(locale);
  }
  return UndefinedValue();
}

CollatorObject* js::intl::GlobalIntlData::getOrCreateCollator(
    JSContext* cx, Handle<JSLinearString*> locale) {
  // Ensure the runtime default locale didn't change.
  if (!ensureRuntimeDefaultLocale(cx)) {
    return nullptr;
  }

  // Ensure the cached locale matches the requested locale.
  if (!EqualLocale(collatorLocale_, locale)) {
    resetCollator();
    collatorLocale_ = locale;
  }

  if (!collator_) {
    Rooted<Value> locales(cx, LocaleOrDefault(locale));
    auto* collator = CreateCollator(cx, locales, UndefinedHandleValue);
    if (!collator) {
      return nullptr;
    }
    collator_ = collator;
  }

  return &collator_->as<CollatorObject>();
}

NumberFormatObject* js::intl::GlobalIntlData::getOrCreateNumberFormat(
    JSContext* cx, Handle<JSLinearString*> locale) {
  // Ensure the runtime default locale didn't change.
  if (!ensureRuntimeDefaultLocale(cx)) {
    return nullptr;
  }

  // Ensure the cached locale matches the requested locale.
  if (!EqualLocale(numberFormatLocale_, locale)) {
    resetNumberFormat();
    numberFormatLocale_ = locale;
  }

  if (!numberFormat_) {
    Rooted<Value> locales(cx, LocaleOrDefault(locale));
    auto* numberFormat = CreateNumberFormat(cx, locales, UndefinedHandleValue);
    if (!numberFormat) {
      return nullptr;
    }
    numberFormat_ = numberFormat;
  }

  return &numberFormat_->as<NumberFormatObject>();
}

DateTimeFormatObject* js::intl::GlobalIntlData::getOrCreateDateTimeFormat(
    JSContext* cx, DateTimeFormatKind kind, Handle<JSLinearString*> locale) {
  // Ensure the runtime default locale didn't change.
  if (!ensureRuntimeDefaultLocale(cx)) {
    return nullptr;
  }

  // Ensure the runtime default time zone didn't change.
  if (!ensureRuntimeDefaultTimeZone(cx)) {
    return nullptr;
  }

  // Ensure the cached locale matches the requested locale.
  if (!EqualLocale(dateTimeFormatLocale_, locale)) {
    resetDateTimeFormat();
    dateTimeFormatLocale_ = locale;
  }

  JSObject* dtfObject = nullptr;
  switch (kind) {
    case DateTimeFormatKind::All:
      dtfObject = dateTimeFormatToLocaleAll_;
      break;
    case DateTimeFormatKind::Date:
      dtfObject = dateTimeFormatToLocaleDate_;
      break;
    case DateTimeFormatKind::Time:
      dtfObject = dateTimeFormatToLocaleTime_;
      break;
  }

  if (!dtfObject) {
    Rooted<Value> locales(cx, LocaleOrDefault(locale));
    auto* dateTimeFormat =
        CreateDateTimeFormat(cx, locales, UndefinedHandleValue, kind);
    if (!dateTimeFormat) {
      return nullptr;
    }

    switch (kind) {
      case DateTimeFormatKind::All:
        dateTimeFormatToLocaleAll_ = dateTimeFormat;
        break;
      case DateTimeFormatKind::Date:
        dateTimeFormatToLocaleDate_ = dateTimeFormat;
        break;
      case DateTimeFormatKind::Time:
        dateTimeFormatToLocaleTime_ = dateTimeFormat;
        break;
    }

    dtfObject = dateTimeFormat;
  }

  return &dtfObject->as<DateTimeFormatObject>();
}

void js::intl::GlobalIntlData::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &runtimeDefaultLocale_,
                    "GlobalIntlData::runtimeDefaultLocale_");
  TraceNullableEdge(trc, &runtimeDefaultTimeZone_,
                    "GlobalIntlData::runtimeDefaultTimeZone_");

  TraceNullableEdge(trc, &collatorLocale_, "GlobalIntlData::collatorLocale_");
  TraceNullableEdge(trc, &collator_, "GlobalIntlData::collator_");

  TraceNullableEdge(trc, &numberFormatLocale_,
                    "GlobalIntlData::numberFormatLocale_");
  TraceNullableEdge(trc, &numberFormat_, "GlobalIntlData::numberFormat_");

  TraceNullableEdge(trc, &dateTimeFormatLocale_,
                    "GlobalIntlData::dateTimeFormatLocale_");
  TraceNullableEdge(trc, &dateTimeFormatToLocaleAll_,
                    "GlobalIntlData::dateTimeFormatToLocaleAll_");
  TraceNullableEdge(trc, &dateTimeFormatToLocaleDate_,
                    "GlobalIntlData::dateTimeFormatToLocaleDate_");
  TraceNullableEdge(trc, &dateTimeFormatToLocaleTime_,
                    "GlobalIntlData::dateTimeFormatToLocaleTime_");
}
