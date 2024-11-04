/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function oneEscapeFocusContentNonEmptyUrlbar() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "https://example.com" },
    async function () {
      Assert.equal(
        gURLBar.untrimmedValue,
        "https://example.com/",
        "URL bar value should have an address"
      );

      let focusBrowserPromise = BrowserTestUtils.waitForEvent(
        gBrowser.selectedBrowser,
        "focus"
      );
      gURLBar.focus();
      EventUtils.synthesizeKey("KEY_Escape");
      info("waiting for content document focus");
      await focusBrowserPromise;

      Assert.equal(
        document.activeElement,
        gBrowser.selectedBrowser,
        "Content document should be focused"
      );
    }
  );
});

add_task(async function oneEscapeFocusContentBlankPage() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:home" },
    async function () {
      // Test that pressing Esc will focus the document when no text was typed
      // in the urlbar

      is(gURLBar.value, "", "URL bar value should be empty");

      let focusBrowserPromise = BrowserTestUtils.waitForEvent(
        gBrowser.selectedBrowser,
        "focus"
      );
      gURLBar.focus();
      EventUtils.synthesizeKey("KEY_Escape");
      info("waiting for content document focus");
      await focusBrowserPromise;
      Assert.equal(
        document.activeElement,
        gBrowser.selectedBrowser,
        "Content document should be focused"
      );
    }
  );
});

add_task(async function threeEscapeFocusContentDocumentNonEmptyUrlbar() {
  registerCleanupFunction(PlacesUtils.history.clear);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com",
    },
    async function () {
      let originalValue = gURLBar.value;

      // Test that repeatedly pressing Esc after typing in the url bar will
      // 1. close results panel
      // 2. reset / undo previously typed text
      // 3. focus content document
      //
      await UrlbarTestUtils.promisePopupOpen(window, () =>
        UrlbarTestUtils.inputIntoURLBar(window, "hello")
      );
      is(
        gURLBar.value,
        "hello",
        "URL bar value should match after sending a key"
      );
      // assert results panel is open
      Assert.equal(
        UrlbarTestUtils.isPopupOpen(window),
        true,
        "Popup should be open"
      );

      // 1. first escape press should close panel
      EventUtils.synthesizeKey("KEY_Escape");

      // assert results panel is closed
      Assert.equal(
        UrlbarTestUtils.isPopupOpen(window),
        false,
        "Popup shouldn't be open"
      );

      // assert urlbar is still focused
      Assert.equal(
        document.activeElement,
        gURLBar.inputField,
        "URL Bar should be focused"
      );

      // 2. second escape press should undo typed text
      EventUtils.synthesizeKey("KEY_Escape");

      is(
        gURLBar.value,
        originalValue,
        "URL bar value should be reset after escape"
      );
      Assert.equal(
        document.activeElement,
        gURLBar.inputField,
        "URL Bar should still be focused"
      );

      let focusBrowserPromise = BrowserTestUtils.waitForEvent(
        gBrowser.selectedBrowser,
        "focus"
      );
      // 3. third escape press should focus content document
      EventUtils.synthesizeKey("KEY_Escape");
      info("waiting for content document focus");
      await focusBrowserPromise;

      Assert.equal(
        document.activeElement,
        gBrowser.selectedBrowser,
        "Content document should be focused"
      );
    }
  );
});

add_task(async function testDisabledFocusContentDocumentOnEsc() {
  // Test that pressing Esc will not focus the document when the preference
  // is unset
  Preferences.set("browser.urlbar.focusContentDocumentOnEsc", false);

  let focusUrlPromise = BrowserTestUtils.waitForEvent(
    gURLBar.inputField,
    "focus"
  );
  gURLBar.focus();
  await focusUrlPromise;

  EventUtils.synthesizeKey("KEY_Escape");

  // assert results panel is closed
  Assert.equal(
    UrlbarTestUtils.isPopupOpen(window),
    false,
    "Popup shouldn't be open"
  );

  Assert.equal(
    document.activeElement,
    gURLBar.inputField,
    "URL Bar should be focused"
  );

  // cleanup
  Preferences.set("browser.urlbar.focusContentDocumentOnEsc", true);
});
