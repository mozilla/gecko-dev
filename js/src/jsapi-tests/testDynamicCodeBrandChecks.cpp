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

// This snippet defines a TrustedType that wraps some trustedCode string and
// stringifies to that string, as well as a helper to create a fake instance
// that can stringify to a different string.
const char* customTypesSnippet =
    "function TrustedType(aTrustedCode) { this.trustedCode = aTrustedCode; };"
    "TrustedType.prototype.toString = function() { return this.trustedCode; };"
    "function CreateFakeTrustedType(aTrustedCode, aString) {"
    "  let fake = new TrustedType(aTrustedCode);"
    "  fake.toString = () => { return aString; };"
    "  return fake;"
    "};";

BEGIN_TEST(testDynamicCodeBrandChecks_CustomHostEnsureCanCompileStrings) {
  JSSecurityCallbacks securityCallbacksWithCustomHostEnsureCanCompileStrings = {
      StringifiedObjectsMatchTrustedCodeProperties,  // contentSecurityPolicyAllows
      ExtractTrustedCodeStringProperty,              // codeForEvalGets
      nullptr                                        // subsumes
  };
  JS_SetSecurityCallbacks(
      cx, &securityCallbacksWithCustomHostEnsureCanCompileStrings);
  JS::RootedValue v(cx);

  EXEC(customTypesSnippet);

  // String arguments are evaluated.
  EVAL("eval('5*8');", &v);
  CHECK(v.isNumber() && v.toNumber() == 40);
  EVAL("(new Function('a', 'b', 'return a * b'))(6, 7);", &v);
  CHECK(v.isNumber() && v.toNumber() == 42);

  // The same works with TrustedType wrappers.
  EVAL("eval(new TrustedType('5*8'));", &v);
  CHECK(v.isNumber() && v.toNumber() == 40);
  EVAL(
      "(new Function(new TrustedType('a'), new TrustedType('b'), new "
      "TrustedType('return a * b')))(6, 7);",
      &v);
  CHECK(v.isNumber() && v.toNumber() == 42);

  // new Function fails if one of the stringified argument does not match the
  // trustedCode property.
  CHECK(!execDontReport(
      "new Function(CreateFakeTrustedType('a', 'c'), 'b', 'return b');",
      __FILE__, __LINE__));
  CHECK(!execDontReport(
      "new Function('a', CreateFakeTrustedType('b', 'c'), 'return a');",
      __FILE__, __LINE__));
  CHECK(
      !execDontReport("new Function('a', 'b', CreateFakeTrustedType('return a "
                      "* b', 'return a + b'));",
                      __FILE__, __LINE__));

  // new Function also fails if StringifiedObjectsMatchTrustedCodeProperties
  // returns false.
  CHECK(!execDontReport("new Function('a', 'b', new TrustedType(undefined));",
                        __FILE__, __LINE__));

  // PerformEval relies on ExtractTrustedCodeProperty rather than toString() to
  // obtain the code to execute, so StringifiedObjectsMatchTrustedCodeProperties
  // will always allow the code execution for the specified security callbacks.
  EVAL("eval(CreateFakeTrustedType('5*8', '6*7'));", &v);
  CHECK(v.isNumber() && v.toNumber() == 40);
  EVAL("eval(new TrustedType(undefined));", &v);
  CHECK(v.isObject());
  JS::RootedObject obj(cx, &v.toObject());
  JS::RootedValue trustedCode(cx);
  CHECK(JS_GetProperty(cx, obj, "trustedCode", &trustedCode));
  CHECK(trustedCode.isUndefined());

  return true;
}

// This is a HostEnsureCanCompileStrings() implementation similar to some checks
// described in the CSP spec: verify that aBodyString and aParameterStrings
// match the corresponding trustedCode property on aBodyArg and aParameterArgs
// objects. See https://w3c.github.io/webappsec-csp/#can-compile-strings
static bool StringifiedObjectsMatchTrustedCodeProperties(
    JSContext* aCx, JS::RuntimeCode aKind, JS::Handle<JSString*> aCodeString,
    JS::CompilationType aCompilationType,
    JS::Handle<JS::StackGCVector<JSString*>> aParameterStrings,
    JS::Handle<JSString*> aBodyString,
    JS::Handle<JS::StackGCVector<JS::Value>> aParameterArgs,
    JS::Handle<JS::Value> aBodyArg, bool* aOutCanCompileStrings) {
  bool isTrusted = true;
  auto comparePropertyAndString = [&aCx, &isTrusted](
                                      JS::Handle<JS::Value> aValue,
                                      JS::Handle<JSString*> aString) {
    if (!aValue.isObject()) {
      // Just trust non-Objects.
      return true;
    }
    JS::RootedObject obj(aCx, &aValue.toObject());

    JS::RootedString trustedCode(aCx);
    if (!ExtractTrustedCodeStringProperty(aCx, obj, &trustedCode)) {
      // Propagate the failure.
      return false;
    }
    if (!trustedCode) {
      // Emulate a failure if trustedCode is undefined.
      JS_ReportErrorASCII(aCx,
                          "test failed, trustedCode property is undefined");
      return false;
    }
    bool equals;
    if (!EqualStrings(aCx, trustedCode, aString, &equals)) {
      // Propagate the failure.
      return false;
    }
    if (!equals) {
      isTrusted = false;
    }
    return true;
  };
  if (!comparePropertyAndString(aBodyArg, aBodyString)) {
    // Propagate the failure.
    return false;
  }
  if (isTrusted) {
    MOZ_ASSERT(aParameterArgs.length() == aParameterStrings.length());
    for (size_t index = 0; index < aParameterArgs.length(); index++) {
      if (!comparePropertyAndString(aParameterArgs[index],
                                    aParameterStrings[index])) {
        // Propagate the failure.
        return false;
      }
      if (!isTrusted) {
        break;
      }
    }
  }
  // Allow compilation if arguments are trusted.
  *aOutCanCompileStrings = isTrusted;
  return true;
}

END_TEST(testDynamicCodeBrandChecks_CustomHostEnsureCanCompileStrings)

BEGIN_TEST(testDynamicCodeBrandChecks_RejectObjectForEval) {
  JSSecurityCallbacks securityCallbacksRejectObjectBody = {
      DisallowObjectsAndFailOtherwise,   // contentSecurityPolicyAllows
      ExtractTrustedCodeStringProperty,  // codeForEvalGets
      nullptr                            // subsumes
  };

  JS_SetSecurityCallbacks(cx, &securityCallbacksRejectObjectBody);
  JS::RootedValue v(cx);

  EXEC(customTypesSnippet);

  // With the specified security callbacks, eval() will always fail.
  CHECK(!execDontReport("eval('5*8))", __FILE__, __LINE__));
  CHECK(!execDontReport("eval(new TrustedType('5*8'))", __FILE__, __LINE__));

  return true;
}

static bool DisallowObjectsAndFailOtherwise(
    JSContext* aCx, JS::RuntimeCode aKind, JS::Handle<JSString*> aCodeString,
    JS::CompilationType aCompilationType,
    JS::Handle<JS::StackGCVector<JSString*>> aParameterStrings,
    JS::Handle<JSString*> aBodyString,
    JS::Handle<JS::StackGCVector<JS::Value>> aParameterArgs,
    JS::Handle<JS::Value> aBodyArg, bool* aOutCanCompileStrings) {
  if (aBodyArg.isObject()) {
    // Disallow compilation for objects.
    *aOutCanCompileStrings = false;
    return true;
  }
  // Otherwise, emulate a failure.
  JS_ReportErrorASCII(aCx, "aBodyArg is not an Object");
  return false;
}

END_TEST(testDynamicCodeBrandChecks_RejectObjectForEval)
