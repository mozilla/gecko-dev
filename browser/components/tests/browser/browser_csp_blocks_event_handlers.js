/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_blocks_event_handlers() {
  Services.fog.testResetFOG();

  let main = document.documentElement;

  registerCleanupFunction(() => {
    delete window.dont_run_me;
    main.removeAttribute("onclick");
  });

  is(
    Glean.security.cspViolationBrowser.testGetValue(),
    null,
    `No telemetry should have been recorded yet for cspViolationBrowser`
  );

  window.dont_run_me = function () {
    ok(false, "Should not run!");
  };

  let violationPromise = BrowserTestUtils.waitForEvent(
    document,
    "securitypolicyviolation"
  );

  main.setAttribute("onclick", "dont_run_me()");

  // The document is not meant to be clicked.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  main.click();
  AccessibilityUtils.resetEnv();

  let violation = await violationPromise;
  is(
    violation.effectiveDirective,
    "script-src-attr",
    "effectiveDirective matches"
  );
  ok(
    violation.sourceFile.endsWith("browser_csp_blocks_event_handlers.js"),
    "sourceFile matches"
  );

  let testValue = Glean.security.cspViolationBrowser.testGetValue();
  is(testValue.length, 1, "Should have telemetry for one violation");
  let extra = testValue[0].extra;
  is(extra.directive, "script-src-attr", "violation's `directive` is correct");
  is(extra.sourcetype, "chromeuri", "violation's `sourcetype` is correct");
  ok(
    extra.sourcedetails.endsWith("browser_csp_blocks_event_handlers.js"),
    "violation's `sourcedetails` is correct"
  );
  is(extra.blockeduritype, "inline", "violation's `blockeduritype` is correct");
  is(extra.blockeduridetails, "", "violation's `blockeduridetails` is correct");
  is(extra.linenumber, "31", "violation's `linenumber` is correct");
  is(extra.columnnumber, "8", "violation's `columnnumber` is correct");
  is(extra.sample, "dont_run_me()", "violation's sample is correct");
});

add_task(async function test_pref_disable() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.browser_xhtml_csp.enabled", false]],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow();

  let ran = false;
  win.run_me = function () {
    ran = true;
  };

  let main = win.document.documentElement;
  main.setAttribute("onclick", "run_me()");
  main.click();

  ok(ran, "Event listener in new window should run");

  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});
