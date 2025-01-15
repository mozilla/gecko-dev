/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ObjectWithStashedPointer.h"
#include "jsapi-tests/tests.h"

static void alter_value(int* valuePtr) { *valuePtr = 33; }

BEGIN_TEST(testObjectWithStashedPointer_basic) {
  int value = 55;

  JSObject* obj = JS::NewObjectWithStashedPointer(cx, &value, alter_value);
  CHECK(obj);

  {
    JS::RootedObject rooted{cx, obj};
    JS_GC(cx);
    CHECK_EQUAL(value, 55);
  }

  JS_GC(cx);
  CHECK_EQUAL(value, 33);
  return true;
}
END_TEST(testObjectWithStashedPointer_basic)

BEGIN_TEST(testObjectWithStashedPointer_noFreeFunc) {
  int value = 55;

  JSObject* obj = JS::NewObjectWithStashedPointer(cx, &value);
  CHECK(obj);

  CHECK_EQUAL(*JS::ObjectGetStashedPointer<int>(cx, obj), 55);
  return true;
}
END_TEST(testObjectWithStashedPointer_noFreeFunc)

BEGIN_TEST(testObjectWithStashedPointer_null) {
  JSObject* obj = JS::NewObjectWithStashedPointer<void>(cx, nullptr);
  CHECK(obj);

  CHECK_NULL(JS::ObjectGetStashedPointer<void>(cx, obj));
  return true;
}
END_TEST(testObjectWithStashedPointer_null)
