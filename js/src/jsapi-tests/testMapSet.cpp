/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsfriendapi.h"

#include "js/MapAndSet.h"
#include "js/RootingAPI.h"
#include "js/Wrapper.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testMap) {
  JS::Rooted<JSObject*> theMap(cx, JS::NewMapObject(cx));
  CHECK(theMap);

  auto runTests = [&](JS::Handle<JSObject*> map) {
    CHECK_EQUAL(JS::MapSize(cx, map), 0u);

    JS::Rooted<JS::Value> val1(cx, JS::ObjectValue(*JS_NewObject(cx, nullptr)));
    JS::Rooted<JS::Value> val2(cx, JS::ObjectValue(*JS_NewObject(cx, nullptr)));
    JS::Rooted<JS::Value> rval(cx);

    // Set and Size
    CHECK(JS::MapSet(cx, map, val1, val2));
    CHECK_EQUAL(JS::MapSize(cx, map), 1u);

    // Has
    bool b;
    CHECK(JS::MapHas(cx, map, val1, &b));
    CHECK_EQUAL(b, true);
    CHECK(JS::MapHas(cx, map, val2, &b));
    CHECK_EQUAL(b, false);

    // Get
    CHECK(JS::MapGet(cx, map, val1, &rval));
    CHECK(rval == val2);
    CHECK(JS::MapGet(cx, map, val2, &rval));
    CHECK(rval.isUndefined());

    // Delete
    CHECK(JS::MapDelete(cx, map, val2, &b));
    CHECK_EQUAL(b, false);
    CHECK(JS::MapDelete(cx, map, val1, &b));
    CHECK_EQUAL(b, true);
    CHECK_EQUAL(JS::MapSize(cx, map), 0u);

    // Set
    CHECK(JS::MapSet(cx, map, val1, val2));
    CHECK(JS::MapSet(cx, map, val2, val1));
    CHECK_EQUAL(JS::MapSize(cx, map), 2u);

    // Iterator
    CHECK(JS::MapKeys(cx, map, &rval));
    CHECK(rval.isObject());
    js::AssertSameCompartment(cx, rval);

    // Clear
    CHECK(JS::MapClear(cx, map));
    CHECK_EQUAL(JS::MapSize(cx, map), 0u);
    return true;
  };

  // Run tests with unwrapped MapObject.
  if (!runTests(theMap)) {
    return false;
  }

  // Run tests with wrapped MapObject.
  JS::RealmOptions globalOptions;
  JS::Rooted<JSObject*> newGlobal(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, globalOptions));
  CHECK(newGlobal);

  JSAutoRealm ar(cx, newGlobal);
  CHECK(JS_WrapObject(cx, &theMap));
  CHECK(js::IsCrossCompartmentWrapper(theMap));
  return runTests(theMap);
}
END_TEST(testMap)

BEGIN_TEST(testSet) {
  JS::Rooted<JSObject*> theSet(cx, JS::NewSetObject(cx));
  CHECK(theSet);

  auto runTests = [&](JS::Handle<JSObject*> set) {
    CHECK_EQUAL(JS::SetSize(cx, set), 0u);

    JS::Rooted<JS::Value> val1(cx, JS::ObjectValue(*JS_NewObject(cx, nullptr)));
    JS::Rooted<JS::Value> val2(cx, JS::ObjectValue(*JS_NewObject(cx, nullptr)));
    JS::Rooted<JS::Value> rval(cx);

    // Add and Size
    CHECK(JS::SetAdd(cx, set, val1));
    CHECK_EQUAL(JS::SetSize(cx, set), 1u);

    // Has
    bool b;
    CHECK(JS::SetHas(cx, set, val1, &b));
    CHECK_EQUAL(b, true);
    CHECK(JS::SetHas(cx, set, val2, &b));
    CHECK_EQUAL(b, false);

    // Delete
    CHECK(JS::SetDelete(cx, set, val2, &b));
    CHECK_EQUAL(b, false);
    CHECK(JS::SetDelete(cx, set, val1, &b));
    CHECK_EQUAL(b, true);
    CHECK_EQUAL(JS::SetSize(cx, set), 0u);

    // Add
    CHECK(JS::SetAdd(cx, set, val1));
    CHECK(JS::SetAdd(cx, set, val2));
    CHECK_EQUAL(JS::SetSize(cx, set), 2u);

    // Iterator
    CHECK(JS::SetKeys(cx, set, &rval));
    CHECK(rval.isObject());
    js::AssertSameCompartment(cx, rval);

    // Clear
    CHECK(JS::SetClear(cx, set));
    CHECK_EQUAL(JS::SetSize(cx, set), 0u);
    return true;
  };

  // Run tests with unwrapped SetObject.
  if (!runTests(theSet)) {
    return false;
  }

  // Run tests with wrapped SetObject.
  JS::RealmOptions globalOptions;
  JS::Rooted<JSObject*> newGlobal(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, globalOptions));
  CHECK(newGlobal);

  JSAutoRealm ar(cx, newGlobal);
  CHECK(JS_WrapObject(cx, &theSet));
  CHECK(js::IsCrossCompartmentWrapper(theSet));
  return runTests(theSet);
}
END_TEST(testSet)
