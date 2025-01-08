/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_blocks_event_handlers() {
  let main = document.documentElement;

  registerCleanupFunction(() => {
    delete window.dont_run_me;
    main.removeAttribute("onclick");
  });

  window.dont_run_me = function () {
    ok(false, "Should not run!");
  };

  let violationPromise = BrowserTestUtils.waitForEvent(
    document,
    "securitypolicyviolation"
  );

  main.setAttribute("onclick", "dont_run_me()");
  main.click();

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
