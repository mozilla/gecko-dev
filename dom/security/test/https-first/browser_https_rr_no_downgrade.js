/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const TIMEOUT_PAGE_URI_HTTP = TEST_PATH_HTTP + "file_https_rr_no_downgrade.sjs";

async function runPrefTest(aURI, aDesc, aSecure) {
  let assertURLStartsWith = aSecure ? "https://" : "http://";
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, aURI);
    await loaded;

    await ContentTask.spawn(
      browser,
      { aDesc, assertURLStartsWith },
      function ({ aDesc, assertURLStartsWith }) {
        dump(`The URL we ended up at: ${content.document.location.href}\n`);
        ok(
          content.document.location.href.startsWith(assertURLStartsWith),
          aDesc
        );
      }
    );

    await SpecialPowers.removePermission("https-only-load-insecure", aURI);
  });
}

add_task(async function () {
  requestLongerTimeout(2);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.dns.mock_HTTPS_RR_domain", "example.org"],
      ["network.dns.force_use_https_rr", true],
      ["dom.security.https_only_fire_http_request_background_timer_ms", 600],
    ],
  });

  Services.fog.testResetFOG();
  await runPrefTest(
    TIMEOUT_PAGE_URI_HTTP,
    "On a timeout we should downgrade.",
    false // secure?
  );

  let glean = Glean.networking.httpToHttpsUpgradeReason;
  is(glean.https_first_upgrade.testGetValue(), 1, "Should upgrade");
  is(glean.https_first_upgrade_downgrade.testGetValue(), 1, "Timerdowngrade.");

  Services.fog.testResetFOG();
  await runPrefTest(
    TIMEOUT_PAGE_URI_HTTP.replace("example.com", "example.org"),
    "For example.org we pretend to have an HTTPS RR and don't downgrade.",
    true // secure?
  );
  is(glean.https_first_upgrade.testGetValue(), 1, "Should upgrade");
  // The following doesn't work because we do not register the downgrade if
  // the follow up connection is upgraded by HTTPS RR. So this succeeds with
  // or without the fix for bug 1906590.
  is(glean.https_first_upgrade_downgrade.testGetValue(), null, "No downgrade");
  // The following doesn't work because our telemetry thinks that HTTPS RR
  // didn't cause the upgrade. Which is somewhat true. It just may have
  // prevented the downgrade, though. This also is the same with and without the
  // fix for bug 1906590.
  //is(glean.https_rr.testGetValue(), 1, "verify https_rr");
  is(glean.https_rr.testGetValue(), null, "verify https_rr");
  // If a downgrade happens a new connection is started which is exempt from
  // upgrades because of the downgrade!
  is(glean.upgrade_exception.testGetValue(), null, "verify upgrade_exception");
});
