/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We explicitly need HTTP URLs in this test
/* eslint-disable @microsoft/sdl/no-insecure-url */

requestLongerTimeout(3);

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_PATH_HTTPS = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_PATH_SCHEMELESS = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "example.com"
);

const HSTS_SITE = TEST_PATH_HTTPS + "file_https_telemetry_hsts.sjs";

const NO_HTTPS_SUPPORT_SITE = TEST_PATH_HTTP + "file_no_https_support.sjs";
const NO_HTTPS_SUPPORT_SITE_SCHEMELESS =
  TEST_PATH_SCHEMELESS + "file_no_https_support.sjs";

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

async function runSchemelessTest(aURI, aDesc, aAssertURLStartsWith) {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: aURI,
    });
    EventUtils.synthesizeKey("KEY_Enter", { ctrlKey: true });
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
    // we can't pass a schemeless uri to removePermission
    let uri = "https://" + aURI;
    await SpecialPowers.removePermission("https-only-load-insecure", uri);
  });
}

add_task(async function () {
  info("(0) exempt loopback addresses");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  // the request to localhost will actually fail in our test infra,
  // though the telemetry would be recorded.
  await runUpgradeTest(
    "https://localhost",
    "(0) exempt loopback addresses",
    "https://"
  );
  verifyGleanValues("(0) exempt loopback addresses", {});
});

add_task(async function () {
  info("(1) no upgrade test");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  await runUpgradeTest(
    "http://example.com?test1",
    "(1) no upgrade test",
    "http://"
  );
  verifyGleanValues("(1) no upgrade test", { noUpgrade: 1 });
});

add_task(async function () {
  info("(2) already https test");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  await runUpgradeTest(
    "https://example.com?test2",
    "(2) already https test",
    "https://"
  );

  verifyGleanValues("(2) already https test", { alreadyHTTPS: 1 });
});

add_task(async function () {
  info("(2b) already https test all prefs true");

  await setPrefsAndResetFog(
    true /* aHTTPSOnlyPref */,
    true /* aHTTPSFirstPref */,
    true /* aSchemeLessPref */
  );

  await runUpgradeTest(
    "https://example.com?test2b",
    "(2b) already https test all prefs true",
    "https://"
  );

  verifyGleanValues("(2b) already https test all prefs true", {
    alreadyHTTPS: 1,
  });
});

add_task(async function () {
  info("(3) hsts");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  // we need to setup hsts first
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, HSTS_SITE);
    await loaded;
  });

  // now we reset glean again
  Services.fog.testResetFOG();

  await runUpgradeTest("http://example.com?test3", "(3) hsts", "https://");

  verifyGleanValues("(3) hsts", { hsts: 1 });

  // finally we need to reset hsts
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, HSTS_SITE + "?reset");
    await loaded;
  });

  info("(3b) hsts with all prefs true");

  await setPrefsAndResetFog(
    true /* aHTTPSOnlyPref */,
    true /* aHTTPSFirstPref */,
    true /* aSchemeLessPref */
  );

  // we need to setup hsts first
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, HSTS_SITE);
    await loaded;
  });

  // now we reset glean again
  Services.fog.testResetFOG();

  await runUpgradeTest(
    "http://example.com?test3b",
    "(3b) hsts with all prefs true",
    "https://"
  );

  verifyGleanValues("(3b) hsts with all prefs true", { hsts: 1 });

  // finally we need to reset the hsts host
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, HSTS_SITE + "?reset");
    await loaded;
  });
});

add_task(async function () {
  info("(4) https-only upgrade");

  await setPrefsAndResetFog(
    true /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  await runUpgradeTest(
    "http://example.com?test4",
    "(4) https-only upgrade",
    "https://"
  );

  verifyGleanValues("(4) https-only upgrade", { httpsOnlyUpgrade: 1 });

  info("(4b) https-only upgrade downgrade");

  await setPrefsAndResetFog(
    true /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  // We specifically want a insecure url here that will fail to upgrade
  let uri = "http://untrusted.example.com:80";
  let desc = "(4b) https-only upgrade downgrade";
  let assertErrorPageStartsWith = "https://";
  let assertDowngradedURLStartsWith = "http://";

  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.waitForErrorPage(browser);
    BrowserTestUtils.startLoadingURIString(browser, uri);
    await loaded;

    await SpecialPowers.spawn(
      browser,
      [desc, assertErrorPageStartsWith],
      async function (desc, assertErrorPageStartsWith) {
        ok(
          content.document.location.href.startsWith(assertErrorPageStartsWith),
          desc
        );
      }
    );

    const downGradeLoaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      null,
      true
    );

    // click the 'continue to insecure page' button
    await SpecialPowers.spawn(browser, [], async function () {
      let openInsecureButton = content.document.getElementById("openInsecure");
      Assert.notEqual(
        openInsecureButton,
        null,
        "openInsecureButton should exist."
      );
      info("Waiting for openInsecureButton to be enabled.");
      function callback() {
        if (!openInsecureButton.inert) {
          observer.disconnect();
          content.requestAnimationFrame(() => {
            content.requestAnimationFrame(() => {
              openInsecureButton.click();
            });
          });
        }
      }
      const observer = new content.MutationObserver(callback);
      observer.observe(openInsecureButton, { attributeFilter: ["inert"] });
      callback();
    });

    await downGradeLoaded;

    await SpecialPowers.spawn(
      browser,
      [desc, assertDowngradedURLStartsWith],
      async function (desc, assertDowngradedURLStartsWith) {
        ok(
          content.document.location.href.startsWith(
            assertDowngradedURLStartsWith
          ),
          desc
        );
      }
    );
    await SpecialPowers.removePermission("https-only-load-insecure", uri);
  });

  verifyGleanValues("(4b) https-only upgrade downgrade", {
    httpsOnlyUpgrade: 1,
    httpsOnlyUpgradeDowngrade: 1,
  });
});

add_task(async function () {
  info("(5) https-first upgrade");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    true /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  await runUpgradeTest(
    "http://example.com?test5",
    "(5) https-first upgrade",
    "https://"
  );

  verifyGleanValues("(5) https-first upgrade", { httpsFirstUpgrade: 1 });

  info("(5b) https-first upgrade downgrade");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    true /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  await runUpgradeTest(
    NO_HTTPS_SUPPORT_SITE + "?test5b",
    "(5b) https-first upgrade downgrade",
    "http://"
  );

  verifyGleanValues("(5) https-first upgrade", {
    httpsFirstUpgrade: 1,
    httpsFirstUpgradeDowngrade: 1,
  });
});

add_task(async function () {
  info("(6) schemeless https-first upgrade");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    true /* aSchemeLessPref */
  );

  await runSchemelessTest(
    "example.com?test6",
    "(6) schemeless https-first upgrade",
    "https://"
  );

  verifyGleanValues("(6) schemeless https-first upgrade", {
    httpsFirstSchemelessUpgrade: 1,
  });

  info("(6b) schemeless https-first upgrade downgrade");

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    false /* aHTTPSFirstPref */,
    true /* aSchemeLessPref */
  );

  await runSchemelessTest(
    NO_HTTPS_SUPPORT_SITE_SCHEMELESS + "?test6b",
    "(6) schemeless https-first upgrade downgrade",
    "http://"
  );

  verifyGleanValues("(6b) schemeless https-first upgrade downgrade", {
    httpsFirstSchemelessUpgrade: 1,
    httpsFirstSchemelessUpgradeDowngrade: 1,
  });
});

add_task(async function () {
  info("(7) https-rr upgrade");

  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", false],
      ["dom.security.https_first", false],
      ["dom.security.https_first_schemeless", false],
      ["network.dns.force_use_https_rr", true],
      ["network.dns.mock_HTTPS_RR_domain", "example.org"],
    ],
  });

  await runUpgradeTest(
    "http://example.org",
    "(7) https-rr upgrade",
    "https://"
  );

  verifyGleanValues("(7) https-rr upgrade", { httpsRR: 1 });
});

add_task(async function () {
  info("(8) upgrade/downgrade/reload");
  // This test performs an upgrade-downgrade and then reloads
  // the document which then triggers an upgrade_exception.

  await setPrefsAndResetFog(
    false /* aHTTPSOnlyPref */,
    true /* aHTTPSFirstPref */,
    false /* aSchemeLessPref */
  );

  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    // First, perform the upgrade/downgrade
    const upgradeDowngradeLoaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      null,
      true
    );
    BrowserTestUtils.startLoadingURIString(
      browser,
      NO_HTTPS_SUPPORT_SITE + "?test8"
    );
    await upgradeDowngradeLoaded;

    await SpecialPowers.spawn(browser, [], async function () {
      ok(
        content.document.location.href.startsWith("http://"),
        "(8) upgrade/downgrade/reload"
      );
    });

    // Before reloading the doc we have to reset the fog
    Services.fog.testResetFOG();

    const reloadLoaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      null,
      true
    );

    await SpecialPowers.spawn(browser, [], async function () {
      content.location.reload();
    });

    await reloadLoaded;

    await SpecialPowers.spawn(browser, [], async function () {
      ok(
        content.document.location.href.startsWith("http://"),
        "(8) upgrade/downgrade/reload"
      );
    });
  });

  verifyGleanValues("(8) upgrade/downgrade/reload", { upgradeException: 1 });
});
