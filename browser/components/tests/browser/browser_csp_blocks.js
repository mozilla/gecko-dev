/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_blocks_event_handlers() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.csp.testing.allow_internal_csp_violation", true]],
  });
  Services.fog.testResetFOG();

  let main = document.documentElement;

  registerCleanupFunction(() => {
    delete window.run_me;
    main.removeAttribute("onclick");
  });

  is(
    Glean.security.cspViolationInternalPage.testGetValue(),
    null,
    `No telemetry should have been recorded yet for cspViolationInternalPage`
  );

  let ran = false;
  window.run_me = function () {
    ran = true;
  };

  let violationPromise = BrowserTestUtils.waitForEvent(
    document,
    "securitypolicyviolation"
  );

  main.setAttribute("onclick", "run_me()");

  // The document is not meant to be clicked.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  main.click();
  AccessibilityUtils.resetEnv();

  is(ran, false, "Event handler should not run");

  let violation = await violationPromise;
  is(
    violation.effectiveDirective,
    "script-src-attr",
    "effectiveDirective matches"
  );
  ok(
    violation.sourceFile.endsWith("browser_csp_blocks.js"),
    "sourceFile matches"
  );
  is(violation.disposition, "enforce", "disposition matches");

  let testValue = Glean.security.cspViolationInternalPage.testGetValue();
  is(testValue.length, 1, "Should have telemetry for one violation");
  let extra = testValue[0].extra;
  is(extra.directive, "script-src-attr", "violation's `directive` is correct");
  is(extra.selftype, "chromeuri", "violation's `selftype` is correct");
  is(
    extra.selfdetails,
    "chrome://browser/content/browser.xhtml",
    "violation's `selfdetails` is correct"
  );
  is(extra.sourcetype, "chromeuri", "violation's `sourcetype` is correct");
  ok(
    extra.sourcedetails.endsWith("browser_csp_blocks.js"),
    "violation's `sourcedetails` is correct"
  );
  is(extra.blockeduritype, "inline", "violation's `blockeduritype` is correct");
  is(
    extra.blockeduridetails,
    undefined,
    "violation's `blockeduridetails` is correct"
  );
  is(extra.linenumber, "35", "violation's `linenumber` is correct");
  is(extra.columnnumber, "8", "violation's `columnnumber` is correct");
  is(extra.sample, "run_me()", "violation's sample is correct");

  await SpecialPowers.popPrefEnv();
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

add_task(async function test_blocks_script_src() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.csp.testing.allow_internal_csp_violation", true]],
  });

  let violationPromise = BrowserTestUtils.waitForEvent(
    document,
    "securitypolicyviolation"
  );

  let script = document.createElement("script");
  script.src = "file:///foo.js"; // Use a different URL?
  document.documentElement.append(script);
  script.remove();

  let violation = await violationPromise;
  is(
    violation.effectiveDirective,
    "script-src-elem",
    "effectiveDirective matches"
  );
  is(violation.blockedURI, "file:///foo.js", "blockedURI matches");
  ok(
    violation.sourceFile.endsWith("browser_csp_blocks.js"),
    "sourceFile matches"
  );
  is(violation.disposition, "enforce", "disposition matches");
});
