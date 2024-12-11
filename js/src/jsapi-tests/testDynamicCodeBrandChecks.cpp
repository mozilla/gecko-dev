/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Tests that the column number of error reports is properly copied over from
 * other reports when invoked from the C++ api.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testDynamicCodeBrandChecks_DefaultHostGetCodeForEval) {
  JS::RootedValue v(cx);

  // String arguments are evaluated.
  EVAL("eval('5*8');", &v);
  CHECK(v.isNumber() && v.toNumber() == 40);

  // Other arguments are returned as is by eval.
  EVAL("eval({myProp: 41});", &v);
  CHECK(v.isObject());
  JS::RootedObject obj(cx, &v.toObject());
  JS::RootedValue myProp(cx);
  CHECK(JS_GetProperty(cx, obj, "myProp", &myProp));
  CHECK(myProp.isNumber() && myProp.toNumber() == 41);

  EVAL("eval({trustedCode: '6*7'}).trustedCode;", &v);
  CHECK(v.isString());
  JSString* str = v.toString();
  CHECK(JS_LinearStringEqualsLiteral(JS_ASSERT_STRING_IS_LINEAR(str), "6*7"));

  EVAL("eval({trustedCode: 42}).trustedCode;", &v);
  CHECK(v.isNumber() && v.toNumber() == 42);

  return true;
}
END_TEST(testDynamicCodeBrandChecks_DefaultHostGetCodeForEval)

static bool ExtractTrustedCodeStringProperty(
    JSContext* aCx, JS::Handle<JSObject*> aCode,
    JS::MutableHandle<JSString*> outCode) {
  JS::RootedValue value(aCx);
  if (!JS_GetProperty(aCx, aCode, "trustedCode", &value)) {
    return false;
  }
  if (value.isUndefined()) {
    // If the property is undefined, return NO-CODE.
    outCode.set(nullptr);
    return true;
  }
  if (value.isString()) {
    // If the property is a string, return it.
    outCode.set(value.toString());
    return true;
  }
  // Otherwise, emulate a failure.
  JS_ReportErrorASCII(aCx, "Unsupported value for trustedCode property");
  return false;
}

BEGIN_TEST(testDynamicCodeBrandChecks_CustomHostGetCodeForEval) {
  JSSecurityCallbacks securityCallbacksWithEvalAcceptingObject = {
      nullptr,                           // contentSecurityPolicyAllows
      ExtractTrustedCodeStringProperty,  // codeForEvalGets
      nullptr                            // subsumes
  };
  JS_SetSecurityCallbacks(cx, &securityCallbacksWithEvalAcceptingObject);
  JS::RootedValue v(cx);

  // String arguments are evaluated.
  EVAL("eval('5*8');", &v);
  CHECK(v.isNumber() && v.toNumber() == 40);

  // Other arguments are returned as is by eval...
  EVAL("eval({myProp: 41});", &v);
  CHECK(v.isObject());
  JS::RootedObject obj(cx, &v.toObject());
  JS::RootedValue myProp(cx);
  CHECK(JS_GetProperty(cx, obj, "myProp", &myProp));
  CHECK(myProp.isNumber() && myProp.toNumber() == 41);

  // ... but Objects are first tentatively converted to String by the
  // codeForEvalGets callback.
  EVAL("eval({trustedCode: '6*7'});", &v);
  CHECK(v.isNumber() && v.toNumber() == 6 * 7);

  // And if that codeForEvalGets callback fails, then so does the eval call.
  CHECK(!execDontReport("eval({trustedCode: 6*7});", __FILE__, __LINE__));

  return true;
}
END_TEST(testDynamicCodeBrandChecks_CustomHostGetCodeForEval)
