/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

add_task(async function test_scheme_modification() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", true],
      ["dom.security.https_first_schemeless", true],
    ],
  });

  await BrowserTestUtils.withNewTab("example.net", async function (browser) {
    is(browser.currentURI.schemeIs("https"), true, "Do upgrade schemeless");

    {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "example.org",
      });
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        null
      );
      EventUtils.synthesizeKey("KEY_Enter");
      await onLoad;

      is(browser.currentURI.schemeIs("https"), true, "Do upgrade schemeless");
    }

    {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        value: "http://example.com",
      });
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        null
      );
      EventUtils.synthesizeKey("KEY_Enter");
      await onLoad;

      is(
        browser.currentURI.schemeIs("http"),
        true,
        "Do not upgrade a scheme of http"
      );
    }

    {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "example.com",
      });
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        null
      );
      EventUtils.synthesizeKey("KEY_Enter");
      await onLoad;

      is(
        browser.currentURI.schemeIs("http"),
        true,
        "Do not upgrade schemeless inputs after we have an exception"
      );
    }
  });
});
