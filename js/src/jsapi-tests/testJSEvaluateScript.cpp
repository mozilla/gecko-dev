/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testJSEvaluateScript)
{
    JS::RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), global));
    CHECK(obj);

    CHECK(JS::ContextOptionsRef(cx).varObjFix());

    static const char src[] = "var x = 5;";

    JS::RootedValue retval(cx);
    CHECK(JS_EvaluateScript(cx, obj, src, sizeof(src) - 1, __FILE__, __LINE__,
                            retval.address()));

    bool hasProp = true;
    CHECK(JS_AlreadyHasOwnProperty(cx, obj, "x", &hasProp));
    CHECK(!hasProp);

    hasProp = false;
    CHECK(JS_HasProperty(cx, global, "x", &hasProp));
    CHECK(hasProp);

    // Now do the same thing, but without JSOPTION_VAROBJFIX
    JS::ContextOptionsRef(cx).setVarObjFix(false);

    static const char src2[] = "var y = 5;";

    CHECK(JS_EvaluateScript(cx, obj, src2, sizeof(src2) - 1, __FILE__, __LINE__,
                            retval.address()));

    hasProp = false;
    CHECK(JS_AlreadyHasOwnProperty(cx, obj, "y", &hasProp));
    CHECK(hasProp);

    hasProp = true;
    CHECK(JS_AlreadyHasOwnProperty(cx, global, "y", &hasProp));
    CHECK(!hasProp);

    return true;
}
END_TEST(testJSEvaluateScript)


