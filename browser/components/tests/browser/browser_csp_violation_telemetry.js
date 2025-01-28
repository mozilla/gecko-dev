/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_internal_page_telemetry() {
  Services.fog.testResetFOG();

  is(
    Glean.security.cspViolationInternalPage.testGetValue(),
    null,
    `No telemetry should have been recorded yet for cspViolationInternalPage`
  );

  // This page's CSP should disallow inline event handlers.
  const ROBOTS_URL = "chrome://browser/content/aboutRobots.xhtml";
  await BrowserTestUtils.withNewTab(ROBOTS_URL, async browser => {
    browser.contentDocument.documentElement.setAttribute("onclick", "foobar()");
    await BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "securitypolicyviolation"
    );
  });

  let testValue = Glean.security.cspViolationInternalPage.testGetValue();
  is(testValue.length, 1, "Should have telemetry for one violation");
  let extra = testValue[0].extra;
  is(extra.directive, "script-src-attr", "violation's `directive` is correct");
  is(extra.selftype, "chromeuri", "violation's `selftype` is correct");
  is(extra.selfdetails, ROBOTS_URL, "violation's `selfdetails` is correct");
  is(extra.sourcetype, "chromeuri", "violation's `sourcetype` is correct");
  ok(
    extra.sourcedetails.endsWith("/browser_csp_violation_telemetry.js"),
    "violation's `sourcedetails` is correct"
  );
  is(extra.blockeduritype, "inline", "violation's `blockeduritype` is correct");
  is(
    extra.blockeduridetails,
    undefined,
    "violation's `blockeduridetails` is correct"
  );
  is(extra.linenumber, "18", "violation's `linenumber` is correct");
  is(extra.columnnumber, "45", "violation's `columnnumber` is correct");
  is(extra.sample, "foobar()", "violation's sample is correct");
});
