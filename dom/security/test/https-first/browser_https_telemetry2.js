/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We explicitly need HTTP URLs in this test
/* eslint-disable @microsoft/sdl/no-insecure-url */

requestLongerTimeout(2);

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

async function setPrefsAndResetFog(
  aHTTPSOnlyPref,
  aHTTPSFirstPref,
  aSchemeLessPref
) {
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", aHTTPSOnlyPref],
      ["dom.security.https_first", aHTTPSFirstPref],
      ["dom.security.https_first_schemeless", aSchemeLessPref],
    ],
  });
}

function verifyGleanValues(aDescription, aExpected) {
  info(aDescription);

  let notInitialized = aExpected.notInitialized || null;
  let noUpgrade = aExpected.noUpgrade || null;
  let alreadyHTTPS = aExpected.alreadyHTTPS || null;
  let hsts = aExpected.hsts || null;
  let httpsOnlyUpgrade = aExpected.httpsOnlyUpgrade || null;
  let httpsOnlyUpgradeDowngrade = aExpected.httpsOnlyUpgradeDowngrade || null;
  let httpsFirstUpgrade = aExpected.httpsFirstUpgrade || null;
  let httpsFirstUpgradeDowngrade = aExpected.httpsFirstUpgradeDowngrade || null;
  let httpsFirstSchemelessUpgrade =
    aExpected.httpsFirstSchemelessUpgrade || null;
  let httpsFirstSchemelessUpgradeDowngrade =
    aExpected.httpsFirstSchemelessUpgradeDowngrade || null;
  let cspUpgradeInsecureRequests = aExpected.cspUpgradeInsecureRequests || null;
  let httpsRR = aExpected.httpsRR || null;
  let webExtensionUpgrade = aExpected.webExtensionUpgrade || null;
  let upgradeException = aExpected.upgradeException || null;

  let glean = Glean.networking.httpToHttpsUpgradeReason;
  is(
    glean.not_initialized.testGetValue(),
    notInitialized,
    "verify not_initialized"
  );
  is(glean.no_upgrade.testGetValue(), noUpgrade, "verify no_upgrade");
  is(glean.already_https.testGetValue(), alreadyHTTPS, "verify already_https");
  is(glean.hsts.testGetValue(), hsts, "verify hsts");
  is(
    glean.https_only_upgrade.testGetValue(),
    httpsOnlyUpgrade,
    "verify https_only_upgrade"
  );
  is(
    glean.https_only_upgrade_downgrade.testGetValue(),
    httpsOnlyUpgradeDowngrade,
    "verify https_only_upgrade_downgrade"
  );
  is(
    glean.https_first_upgrade.testGetValue(),
    httpsFirstUpgrade,
    "verify https_first_upgrade"
  );
  is(
    glean.https_first_upgrade_downgrade.testGetValue(),
    httpsFirstUpgradeDowngrade,
    "verify https_first_upgrade_downgrade"
  );
  is(
    glean.https_first_schemeless_upgrade.testGetValue(),
    httpsFirstSchemelessUpgrade,
    "verify https_first_schemeless_upgrade"
  );
  is(
    glean.https_first_schemeless_upgrade_downgrade.testGetValue(),
    httpsFirstSchemelessUpgradeDowngrade,
    "verify https_first_schemeless_upgrade_downgrade"
  );
  is(
    glean.csp_uir.testGetValue(),
    cspUpgradeInsecureRequests,
    "verify csp_uir"
  );
  is(glean.https_rr.testGetValue(), httpsRR, "verify https_rr");
  is(
    glean.web_extension_upgrade.testGetValue(),
    webExtensionUpgrade,
    "verify web_extension_upgrade"
  );
  is(
    glean.upgrade_exception.testGetValue(),
    upgradeException,
    "verify upgrade_exception"
  );
}

async function runUpgradeTest(aURI, aDesc, aAssertURLStartsWith) {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, aURI);
    await loaded;

    await SpecialPowers.spawn(
      browser,
      [aDesc, aAssertURLStartsWith],
      async function (aDesc, aAssertURLStartsWith) {
        ok(
          content.document.location.href.startsWith(aAssertURLStartsWith),
          aDesc
        );
      }
    );
    await SpecialPowers.removePermission("https-only-load-insecure", aURI);
  });
}

add_task(async function () {
  info("(1) verify view-source");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  let random = Math.random();

  await runUpgradeTest(
    "view-source:https://example.com?" + random,
    "(1) verify view-source",
    "view-source"
  );
  verifyGleanValues("(1) verify view-source", { alreadyHTTPS: 1 });
});

add_task(async function () {
  info("(2) verify about: pages");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  // about:credits resolves to https://www.mozilla.org/credits/
  await runUpgradeTest("about:credits", "(2) verify about: pages", "https:");
  verifyGleanValues("(2) verify about: pages", { alreadyHTTPS: 1 });
});

add_task(async function () {
  info("(3) verify top-level csp upgrade-insecure-requests");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  let random = Math.random();

  await BrowserTestUtils.withNewTab(
    TEST_PATH_HTTP + "file_https_telemetry_csp_uir.html?" + random,
    async function (browser) {
      let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
      BrowserTestUtils.synthesizeMouse(
        "#mylink",
        2,
        2,
        { accelKey: true },
        browser
      );
      let tab = await newTabPromise;
      is(
        tab.linkedBrowser.currentURI.scheme,
        "https",
        "Should have opened https page."
      );
      BrowserTestUtils.removeTab(tab);
    }
  );

  // we record 2 loads:
  // * the load for TEST_PATH_HTTP which results in "no_upgrade"
  // * the link click where CSP UIR upgrades the load to https
  verifyGleanValues("(3) verify top-level csp upgrade-insecure-requests", {
    noUpgrade: 1,
    cspUpgradeInsecureRequests: 1,
  });
});
