/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Tests the JSClass::getProperty hook
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static int called_test_fn;
static int called_test_prop_get;

static bool test_prop_get( JSContext* cx, JS::HandleObject obj, JS::HandleId id, JS::MutableHandleValue vp )
{
    called_test_prop_get++;
    return true;
}

static bool
PTest(JSContext* cx, unsigned argc, JS::Value* vp);

static const JSClassOps ptestClassOps = {
    nullptr, // addProperty
    nullptr, // delProperty
    test_prop_get,
    nullptr // setProperty
};

static const JSClass ptestClass = {
    "PTest",
    JSCLASS_HAS_PRIVATE,
    &ptestClassOps
};

static bool
PTest(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JSObject* obj = JS_NewObjectForConstructor(cx, &ptestClass, args);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}
static bool test_fn(JSContext* cx, unsigned argc, JS::Value* vp)
{
    called_test_fn++;
    return true;
}

static const JSFunctionSpec ptestFunctions[] = {
    JS_FS( "test_fn", test_fn, 0, 0 ),
    JS_FS_END
};

BEGIN_TEST(testClassGetter_isCalled)
{
    CHECK(JS_InitClass(cx, global, nullptr, &ptestClass, PTest, 0,
                       nullptr, ptestFunctions, nullptr, nullptr));

    EXEC("function check() { var o = new PTest(); o.test_fn(); o.test_value1; o.test_value2; o.test_value1; }");

    for (int i = 1; i < 9; i++) {
        JS::RootedValue rval(cx);
        CHECK(JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK(called_test_fn == i);
        CHECK(called_test_prop_get == 4 * i);
    }
    return true;
}
END_TEST(testClassGetter_isCalled)
