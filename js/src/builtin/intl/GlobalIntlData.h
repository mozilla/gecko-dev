/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_intl_GlobalIntlData_h
#define builtin_intl_GlobalIntlData_h

#include "gc/Barrier.h"
#include "js/TypeDecls.h"
#include "vm/JSObject.h"
#include "vm/StringType.h"

class JS_PUBLIC_API JSTracer;

namespace js {
class CollatorObject;
class DateTimeFormatObject;
class NumberFormatObject;
}  // namespace js

namespace js::intl {

enum class DateTimeFormatKind;

/**
 * Cached per-global Intl data. In contrast to SharedIntlData, which is
 * a per-runtime shared Intl cache, this cache is per-global.
 */
class GlobalIntlData {
  /**
   * The locale information provided by the embedding, guiding SpiderMonkey's
   * selection of a default locale. See intl_RuntimeDefaultLocale(), whose value
   * controls the value returned by DefaultLocale() that's what's *actually*
   * used.
   */
  GCPtr<JSLinearString*> runtimeDefaultLocale_;

  /**
   * Time zone information provided by ICU. See intl_defaultTimeZone(), whose
   * value controls the value returned by DefaultTimeZone() that's what's
   * *actually* used.
   */
  GCPtr<JSLinearString*> runtimeDefaultTimeZone_;

  /**
   * Locale string passed to the last call to localeCompare String method. Not
   * necessarily the actual locale when the string can't be resolved to a
   * supported Collator locale.
   */
  GCPtr<JSLinearString*> collatorLocale_;

  /**
   * Cached Intl.Collator when String.prototype.localeCompare is called with
   * |locales| either a |undefined| or a string, and |options| having the value
   * |undefined|.
   */
  GCPtr<JSObject*> collator_;

  /**
   * Locale string passed to the last call to toLocaleString Number method. Not
   * necessarily the actual locale when the string can't be resolved to a
   * supported NumberFormat locale.
   */
  GCPtr<JSLinearString*> numberFormatLocale_;

  /**
   * Cached Intl.NumberFormat when Number.prototype.toLocaleString is called
   * with |locales| either a |undefined| or a string, and |options| having the
   * value |undefined|.
   */
  GCPtr<JSObject*> numberFormat_;

  /**
   * Locale string passed to the last call to toLocale*String Date method. Not
   * necessarily the actual locale when the string can't be resolved to a
   * supported DateTimeFormat locale.
   */
  GCPtr<JSLinearString*> dateTimeFormatLocale_;

  /**
   * Cached Intl.DateTimeFormat when Date.prototype.toLocaleString is called
   * with |locales| either a |undefined| or a string, and |options| having the
   * value |undefined|.
   */
  GCPtr<JSObject*> dateTimeFormatToLocaleAll_;

  /**
   * Cached Intl.DateTimeFormat when Date.prototype.toLocaleDateString is called
   * with |locales| either a |undefined| or a string, and |options| having the
   * value |undefined|.
   */
  GCPtr<JSObject*> dateTimeFormatToLocaleDate_;

  /**
   * Cached Intl.DateTimeFormat when Date.prototype.toLocaleTimeString is called
   * with |locales| either a |undefined| or a string, and |options| having the
   * value |undefined|.
   */
  GCPtr<JSObject*> dateTimeFormatToLocaleTime_;

 public:
  /**
   * Get or create the Intl.Collator instance for |locale|. The default locale
   * is used when |locale| is null.
   */
  CollatorObject* getOrCreateCollator(JSContext* cx,
                                      JS::Handle<JSLinearString*> locale);

  /**
   * Get or create the Intl.NumberFormat instance for |locale|. The default
   * locale is used when |locale| is null.
   */
  NumberFormatObject* getOrCreateNumberFormat(
      JSContext* cx, JS::Handle<JSLinearString*> locale);

  /**
   * Get or create the Intl.DateTimeFormat instance for |locale|. The default
   * locale is used when |locale| is null.
   */
  DateTimeFormatObject* getOrCreateDateTimeFormat(
      JSContext* cx, DateTimeFormatKind kind,
      JS::Handle<JSLinearString*> locale);

  void trace(JSTracer* trc);

 private:
  bool ensureRuntimeDefaultLocale(JSContext* cx);
  bool ensureRuntimeDefaultTimeZone(JSContext* cx);

  void resetCollator();
  void resetNumberFormat();
  void resetDateTimeFormat();
};

}  // namespace js::intl

#endif /* builtin_intl_GlobalIntlData_h */
