/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that HTTPS-Only takes precedence over schemeless HTTPS-First by checking
// that the HTTPSFIRST_LOAD_INSECURE_ALLOW permission is only respected on
// schemeless inputs when HTTPS-Only is disabled.

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

function runTest(aExpectedScheme, aDesc) {
  return BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "example.com",
    });
    EventUtils.synthesizeKey("KEY_Enter");
    await loaded;

    is(browser.currentURI.scheme, aExpectedScheme, aDesc);
  });
}

add_task(async function test_schemeless() {
  Services.perms.addFromPrincipal(
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      "http://example.com"
    ),
    "https-only-load-insecure",
    Ci.nsIHttpsOnlyModePermission.HTTPSFIRST_LOAD_INSECURE_ALLOW
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", true],
      ["dom.security.https_first_schemeless", true],
    ],
  });

  await runTest(
    "http",
    "HTTPSFIRST_LOAD_INSECURE_ALLOW should apply if HTTPS-Only is disabled"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  await runTest(
    "https",
    "HTTPSFIRST_LOAD_INSECURE_ALLOW should not apply if HTTPS-Only is enabled"
  );

  Services.perms.removeAll();
});
