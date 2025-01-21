/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-disable @microsoft/sdl/no-insecure-url */

// Test that schemeless HTTPS-First doesn't try to upgrade redirected loads that
// orginially did originate from the address bar (Bug 1937386). If it would
// upgrade those loads, that would result in a false NS_ERROR_REDIRECT_LOOP
// error.

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

const TEST_SERVER =
  "example.com/browser/dom/security/test/https-first/file_schemeless_redirect.sjs";

add_task(async function test_schemeless_redirect() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", false],
      ["dom.security.https_first_schemeless", true],
    ],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SERVER,
    });
    EventUtils.synthesizeKey("KEY_Enter");
    await BrowserTestUtils.waitForBrowserStateChange(
      browser,
      null,
      (aStateFlags, aStatus) => {
        if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)) {
          return false;
        }

        is(
          browser.currentURI.spec,
          "http://" + TEST_SERVER,
          "We should land on the downgraded page"
        );
        is(aStatus, Cr.NS_OK, "Browser should stop loading with status NS_OK");
        return true;
      }
    );

    finish();
  });
});
