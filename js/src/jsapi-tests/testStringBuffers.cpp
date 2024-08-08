/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StringBuffer.h"

#include "jsapi.h"

#include "gc/Zone.h"
#include "js/String.h"
#include "jsapi-tests/tests.h"
#include "util/Text.h"

BEGIN_TEST(testStringBuffersLatin1) {
  static const JS::Latin1Char chars[] = "This is just some random string";
  static const size_t len = js_strlen(chars);

  RefPtr<mozilla::StringBuffer> buffer =
      mozilla::StringBuffer::Create(chars, len);
  CHECK(buffer);

  auto* bufferChars = static_cast<const JS::Latin1Char*>(buffer->Data());

  // Don't purge the ExternalStringCache.
  js::gc::AutoSuppressGC suppress(cx);

  JS::Rooted<JSString*> str1(cx,
                             JS::NewStringFromLatin1Buffer(cx, buffer, len));
  CHECK(str1);
  CHECK_EQUAL(JS_GetStringLength(str1), len);
  {
    JS::AutoCheckCannotGC nogc;
    size_t strLen;
    const JS::Latin1Char* strChars =
        JS_GetLatin1StringCharsAndLength(cx, nogc, str1, &strLen);
    CHECK_EQUAL(strLen, len);
    CHECK_EQUAL(strChars, bufferChars);
  }

  JS::Rooted<JSString*> str2(cx,
                             JS::NewStringFromLatin1Buffer(cx, buffer, len));
  CHECK(str2);

  cx->zone()->externalStringCache().purge();

  JS::Rooted<JSString*> str3(
      cx, JS::NewStringFromKnownLiveLatin1Buffer(cx, buffer, len));
  CHECK(str3);

  // Check the ExternalStringCache works.
  CHECK_EQUAL(str1, str2);

#ifdef DEBUG
  // Three references: buffer, str1/str2, str3.
  CHECK_EQUAL(buffer->RefCount(), 3u);
#endif

  mozilla::StringBuffer* buf;
  CHECK(!JS::IsTwoByteStringWithStringBuffer(str2, &buf));
  CHECK(JS::IsLatin1StringWithStringBuffer(str2, &buf));
  CHECK_EQUAL(buf, buffer);

  return true;
}
END_TEST(testStringBuffersLatin1)

BEGIN_TEST(testStringBuffersTwoByte) {
  static const char16_t chars[] = u"This is just some random string";
  static const size_t len = js_strlen(chars);

  RefPtr<mozilla::StringBuffer> buffer =
      mozilla::StringBuffer::Create(chars, len);
  CHECK(buffer);

  auto* bufferChars = static_cast<const char16_t*>(buffer->Data());

  // Don't purge the ExternalStringCache.
  js::gc::AutoSuppressGC suppress(cx);

  JS::Rooted<JSString*> str1(cx,
                             JS::NewStringFromTwoByteBuffer(cx, buffer, len));
  CHECK(str1);
  CHECK_EQUAL(JS_GetStringLength(str1), len);
  {
    JS::AutoCheckCannotGC nogc;
    size_t strLen;
    const char16_t* strChars =
        JS_GetTwoByteStringCharsAndLength(cx, nogc, str1, &strLen);
    CHECK_EQUAL(strLen, len);
    CHECK_EQUAL(strChars, bufferChars);
  }

  JS::Rooted<JSString*> str2(cx,
                             JS::NewStringFromTwoByteBuffer(cx, buffer, len));
  CHECK(str2);

  cx->zone()->externalStringCache().purge();

  JS::Rooted<JSString*> str3(
      cx, JS::NewStringFromKnownLiveTwoByteBuffer(cx, buffer, len));
  CHECK(str3);

  // Check the ExternalStringCache works.
  CHECK_EQUAL(str1, str2);

#ifdef DEBUG
  // Three references: buffer, str1/str2, str3.
  CHECK_EQUAL(buffer->RefCount(), 3u);
#endif

  mozilla::StringBuffer* buf;
  CHECK(!JS::IsLatin1StringWithStringBuffer(str2, &buf));
  CHECK(JS::IsTwoByteStringWithStringBuffer(str2, &buf));
  CHECK_EQUAL(buf, buffer);

  return true;
}
END_TEST(testStringBuffersTwoByte)

BEGIN_TEST(testStringBuffersUTF8) {
  // UTF8 ASCII string buffer.
  {
    static const char chars[] = "This is a UTF-8 string but also ASCII";
    static const size_t len = js_strlen(chars);

    RefPtr<mozilla::StringBuffer> buffer =
        mozilla::StringBuffer::Create(chars, len);
    CHECK(buffer);

    // Don't purge the ExternalStringCache.
    js::gc::AutoSuppressGC suppress(cx);

    JS::Rooted<JSString*> str1(cx,
                               JS::NewStringFromUTF8Buffer(cx, buffer, len));
    CHECK(str1);
    CHECK_EQUAL(JS_GetStringLength(str1), len);

    mozilla::StringBuffer* buf;
    CHECK(!JS::IsTwoByteStringWithStringBuffer(str1, &buf));
    CHECK(JS::IsLatin1StringWithStringBuffer(str1, &buf));
    CHECK_EQUAL(buf, buffer);

    JS::Rooted<JSString*> str2(
        cx, JS::NewStringFromKnownLiveUTF8Buffer(cx, buf, len));
    CHECK(str2);

    // Check the ExternalStringCache works.
    CHECK_EQUAL(str1, str2);

#ifdef DEBUG
    CHECK_EQUAL(buffer->RefCount(), 2u);  // |buffer| and str1/str2.
#endif
  }

  // UTF8 non-ASCII string buffer. The passed in buffer isn't used.
  {
    static const char chars[] =
        "This is a UTF-\xEF\xBC\x98 string but not ASCII";
    static const size_t len = js_strlen(chars);

    RefPtr<mozilla::StringBuffer> buffer =
        mozilla::StringBuffer::Create(chars, len);
    CHECK(buffer);

    JS::Rooted<JSString*> str1(cx,
                               JS::NewStringFromUTF8Buffer(cx, buffer, len));
    CHECK(str1);
    CHECK_EQUAL(JS_GetStringLength(str1), 36u);

    mozilla::StringBuffer* buf;
    CHECK(!JS::IsLatin1StringWithStringBuffer(str1, &buf));
    CHECK(!JS::IsTwoByteStringWithStringBuffer(str1, &buf));

    JS::Rooted<JSString*> str2(
        cx, JS::NewStringFromKnownLiveUTF8Buffer(cx, buffer, len));
    CHECK(str2);

#ifdef DEBUG
    CHECK_EQUAL(buffer->RefCount(), 1u);  // Just |buffer|.
#endif
  }

  return true;
}
END_TEST(testStringBuffersUTF8)
