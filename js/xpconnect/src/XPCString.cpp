/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Infrastructure for sharing DOMString data with JSStrings.
 *
 * Importing an nsAString into JS:
 * If possible (GetSharedBufferHandle works) use the external string support in
 * JS to create a JSString that points to the readable's buffer.  We keep a
 * reference to the buffer handle until the JSString is finalized.
 *
 * Exporting a JSString as an nsAReadable:
 * Wrap the JSString with a root-holding XPCJSReadableStringWrapper, which roots
 * the string and exposes its buffer via the nsAString interface, as
 * well as providing refcounting support.
 */

#include "nscore.h"
#include "nsString.h"
#include "mozilla/StringBuffer.h"
#include "jsapi.h"
#include "xpcpublic.h"

using namespace JS;
using mozilla::StringBuffer;

const XPCStringConvert::LiteralExternalString
    XPCStringConvert::sLiteralExternalString;

void XPCStringConvert::LiteralExternalString::finalize(
    JS::Latin1Char* aChars) const {
  // Nothing to do.
}

void XPCStringConvert::LiteralExternalString::finalize(char16_t* aChars) const {
  // Nothing to do.
}

size_t XPCStringConvert::LiteralExternalString::sizeOfBuffer(
    const JS::Latin1Char* aChars, mozilla::MallocSizeOf aMallocSizeOf) const {
  // This string's buffer is not heap-allocated, so its malloc size is 0.
  return 0;
}

size_t XPCStringConvert::LiteralExternalString::sizeOfBuffer(
    const char16_t* aChars, mozilla::MallocSizeOf aMallocSizeOf) const {
  // This string's buffer is not heap-allocated, so its malloc size is 0.
  return 0;
}

// convert a string to a JSString, either copying or sharing string data
// static
bool xpc::NonVoidStringToJsval(JSContext* cx, const nsAString& readable,
                               MutableHandleValue vp) {
  uint32_t length = readable.Length();

  if (readable.IsLiteral()) {
    return XPCStringConvert::StringLiteralToJSVal(cx, readable.BeginReading(),
                                                  length, vp);
  }

  if (StringBuffer* buf = readable.GetStringBuffer()) {
    return XPCStringConvert::UCStringBufferToJSVal(cx, buf, length, vp);
  }

  // blech, have to copy.
  JSString* str = JS_NewUCStringCopyN(cx, readable.BeginReading(), length);
  if (!str) {
    return false;
  }
  vp.setString(str);
  return true;
}

bool xpc::NonVoidLatin1StringToJsval(JSContext* cx, const nsACString& latin1,
                                     MutableHandleValue vp) {
  uint32_t length = latin1.Length();

  if (latin1.IsLiteral()) {
    return XPCStringConvert::StringLiteralToJSVal(
        cx, reinterpret_cast<const JS::Latin1Char*>(latin1.BeginReading()),
        length, vp);
  }

  if (StringBuffer* buf = latin1.GetStringBuffer()) {
    return XPCStringConvert::Latin1StringBufferToJSVal(cx, buf, length, vp);
  }

  JSString* str = JS_NewStringCopyN(cx, latin1.BeginReading(), length);
  if (!str) {
    return false;
  }
  vp.setString(str);
  return true;
}

bool xpc::NonVoidUTF8StringToJsval(JSContext* cx, const nsACString& utf8,
                                   MutableHandleValue vp) {
  uint32_t length = utf8.Length();

  if (utf8.IsLiteral()) {
    return XPCStringConvert::UTF8StringLiteralToJSVal(
        cx, JS::UTF8Chars(utf8.BeginReading(), length), vp);
  }

  if (StringBuffer* buf = utf8.GetStringBuffer()) {
    return XPCStringConvert::UTF8StringBufferToJSVal(cx, buf, length, vp);
  }

  JSString* str =
      JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(utf8.BeginReading(), length));
  if (!str) {
    return false;
  }
  vp.setString(str);
  return true;
}
