/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MODULE_PATH = getRootDirectory(gTestPath) + "file_csp_module.sys.mjs";

async function test_in_module(fun, directive, blockedURI) {
  await SpecialPowers.pushPrefEnv({
    set: [["security.csp.testing.allow_internal_csp_violation", true]],
  });

  let violationPromise = BrowserTestUtils.waitForEvent(
    document,
    "securitypolicyviolation"
  );

  let module = ChromeUtils.importESModule(MODULE_PATH);
  module[fun](document);

  let violation = await violationPromise;
  is(violation.effectiveDirective, directive, "effectiveDirective matches");
  is(violation.blockedURI, blockedURI, "blockedURI matches");
  ok(
    violation.sourceFile.endsWith("file_csp_module.sys.mjs"),
    "sourceFile matches"
  );
}

add_task(async function test_event_handler() {
  await test_in_module("test_event_handler", "script-src-attr", "inline");
});

add_task(async function test_inline_script() {
  await test_in_module("test_inline_script", "script-src-elem", "inline");
});

add_task(async function test_data_url_script() {
  await test_in_module(
    "test_data_url_script",
    "script-src-elem",
    `data:text/javascript,throw new Error("unreachable code");`
  );
});

add_task(function test_eval_in_module() {
  // nsContentSecurityUtils::IsEvalAllowed uses MOZ_CRASH in debug and fuzzing builds.
  // Non-Nightly builds still allow eval due to nsContentSecurityUtils::DetectJsHacks.
  if (AppConstants.DEBUG || !AppConstants.NIGHTLY_BUILD) {
    ok(true, "Don't crash");
    return;
  }

  const { test_eval } = ChromeUtils.importESModule(MODULE_PATH);

  let error = null;
  try {
    test_eval();
  } catch (e) {
    error = e;
  }

  is(error.name, "EvalError", "eval threw EvalError");
});
