/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LinkPreview } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreview.sys.mjs"
);
const { LinkPreviewModel } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreviewModel.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const TEST_LINK_URL = "https://example.com";

/**
 * Tests that the Link Preview feature is correctly triggered when the Shift+Alt
 * key is pressed.
 *
 * This test performs the following steps:
 * 1. Enables the Link Preview feature via the preference `"browser.ml.linkPreview.enabled"`.
 * 2. Creates and dispatches a `keydown` event with the `altKey` property set to `true`.
 * 3. Sets an over link using `XULBrowserWindow.setOverLink`.
 * 4. Verifies that the `_maybeLinkPreview` method of `LinkPreview` is called with the correct window.
 */
add_task(async function test_link_preview_with_shift_alt_key_event() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.enabled", true]],
  });

  let stub = sinon.stub(LinkPreview, "_maybeLinkPreview");

  ok(!LinkPreview.keyboardComboActive, "not yet active");

  window.dispatchEvent(
    new KeyboardEvent("keydown", {
      bubbles: true,
      cancelable: true,
      altKey: true,
    })
  );

  ok(!LinkPreview.keyboardComboActive, "just alt insufficient");

  window.dispatchEvent(
    new KeyboardEvent("keydown", {
      bubbles: true,
      cancelable: true,
      shiftKey: true,
    })
  );

  ok(!LinkPreview.keyboardComboActive, "just shift insufficient");

  window.dispatchEvent(
    new KeyboardEvent("keydown", {
      bubbles: true,
      cancelable: true,
      altKey: true,
      ctrlKey: true,
      shiftKey: true,
    })
  );

  ok(!LinkPreview.keyboardComboActive, "extra control also not active");

  let keydownEvent = new KeyboardEvent("keydown", {
    bubbles: true,
    cancelable: true,
    altKey: true,
    shiftKey: true,
  });
  window.dispatchEvent(keydownEvent);

  ok(LinkPreview.keyboardComboActive, "shift+alt active");

  XULBrowserWindow.setOverLink(TEST_LINK_URL, {});

  ok(
    stub.calledWith(window),
    "_maybeLinkPreview was called with the correct window"
  );

  stub.restore();
  Services.prefs.clearUserPref("browser.ml.linkPreview.enabled");
  LinkPreview.keyboardComboActive = false;
});

/**
 * Tests that no event is dispatched when the Link Preview feature is disabled, even if the Alt key is pressed.
 *
 * This test performs the following steps:
 * 1. Disables the Link Preview feature via the preference `"browser.ml.linkPreview.enabled"`.
 * 2. Creates and dispatches a `keydown` event with the `altKey` property set to `true`.
 * 3. Sets an over link using `XULBrowserWindow.setOverLink`.
 * 4. Verifies that the `_maybeLinkPreview` method of `LinkPreview` is not called.
 */
add_task(async function test_no_event_triggered_when_disabled_with_alt_key() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.enabled", false]],
  });

  let stub = sinon.stub(LinkPreview, "_maybeLinkPreview");

  let keydownEvent = new KeyboardEvent("keydown", {
    bubbles: true,
    cancelable: true,
    altKey: true,
    shiftKey: true,
  });
  window.dispatchEvent(keydownEvent);

  XULBrowserWindow.setOverLink(TEST_LINK_URL, {});

  ok(
    !stub.called,
    "_maybeLinkPreview should not be called when the feature is disabled"
  );

  stub.restore();
  Services.prefs.clearUserPref("browser.ml.linkPreview.enabled");
  LinkPreview.keyboardComboActive = false;
});

/**
 * Test that page data is fetched.
 */
add_task(async function test_fetch_page_data() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.enabled", true]],
  });
  const actor =
    window.browsingContext.currentWindowContext.getActor("LinkPreview");
  const result = await actor.fetchPageData(
    "https://example.com/browser/toolkit/components/reader/tests/browser/readerModeArticle.html"
  );

  ok(result.article, "article should be populated");
  is(result.article.byline, "by Jane Doe", "byline should be correct");
  is(result.article.title, "Article title", "title should be correct");
  ok(result.metaInfo, "metaInfo should be populated");
  is(
    result.metaInfo.description,
    "This is the article description.",
    "description should be correct"
  );
  is(result.metaInfo["html:title"], "Article title", "title should be correct");
});

/**
 * Test fetching errors.
 */
add_task(async function test_fetch_errors() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.enabled", true]],
  });
  const actor =
    window.browsingContext.currentWindowContext.getActor("LinkPreview");
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  let result = await actor.fetchPageData("http://example.com/");

  ok(result.error, "got error from fetching http");
  is(
    result.error.result,
    Cr.NS_ERROR_UNKNOWN_PROTOCOL,
    "error result should be protocol"
  );

  result = await actor.fetchPageData(
    "https://itisatrap.org/firefox/its-a-trap.html"
  );
  ok(result.error, "got error from fetching trap");
  is(
    result.error.result,
    Cr.NS_ERROR_PHISHING_URI,
    "error result should be phishing"
  );

  result = await actor.fetchPageData("https://unknown.host.example.com");
  ok(result.error, "got error from fetching host");
  is(
    result.error.result,
    Cr.NS_ERROR_UNKNOWN_HOST,
    "error result should be host"
  );
});

/**
 * Test that link preview panel is shown.
 */
add_task(async function test_link_preview_panel_shown() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.enabled", true]],
  });

  let onDownload, toResolve;
  const stub = sinon
    .stub(LinkPreviewModel, "generateTextAI")
    .callsFake(async (text, options) => {
      onDownload = options.onDownload;
      toResolve = Promise.withResolvers();
      return toResolve.promise;
    });

  const READABLE_PAGE_URL =
    "https://example.com/browser/toolkit/components/reader/tests/browser/readerModeArticle.html";

  window.dispatchEvent(
    new KeyboardEvent("keydown", {
      bubbles: true,
      cancelable: true,
      altKey: true,
      shiftKey: true,
    })
  );
  XULBrowserWindow.setOverLink(READABLE_PAGE_URL, {});

  const panel = await TestUtils.waitForCondition(() =>
    document.getElementById("link-preview-panel")
  );
  ok(panel, "Panel created for link preview");

  await BrowserTestUtils.waitForEvent(panel, "popupshown");

  is(stub.callCount, 1, "would have generated key points");

  const card = panel.querySelector("link-preview-card");
  ok(card, "card created for link preview");
  ok(card.generating, "initially marked as generating");
  ok(!card.showWait, "initially assume not waiting");
  ok(!LinkPreview.downloadingModel, "initially assume not downloading");

  onDownload(true);

  ok(card.showWait, "switched to waiting when download initiates");
  ok(LinkPreview.downloadingModel, "shared waiting for download");

  onDownload(false);

  ok(!card.showWait, "no longer waiting after download complete");
  ok(!LinkPreview.downloadingModel, "downloading updated");
  ok(card.generating, "still generating");

  toResolve.resolve();
  await LinkPreview.lastRequest;

  ok(!card.generating, "done generating");

  panel.remove();
  stub.restore();
  LinkPreview.keyboardComboActive = false;
});

/**
 * Test that link preview panel doesn't generate key points when URL is not readable.
 */
add_task(async function test_skip_keypoints_generation_if_url_not_readable() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.enabled", true]],
  });

  const generateStub = sinon.stub(LinkPreviewModel, "generateTextAI");

  window.dispatchEvent(
    new KeyboardEvent("keydown", {
      bubbles: true,
      cancelable: true,
      altKey: true,
      shiftKey: true,
    })
  );
  XULBrowserWindow.setOverLink(TEST_LINK_URL, {});

  const panel = await TestUtils.waitForCondition(() =>
    document.getElementById("link-preview-panel")
  );
  ok(panel, "Panel created for link preview");

  await BrowserTestUtils.waitForEvent(panel, "popupshown");

  is(
    generateStub.callCount,
    0,
    "generateTextAI should not be called when article has no text content"
  );

  const card = panel.querySelector("link-preview-card");
  ok(card, "card created for link preview");
  ok(!card.generating, "card should not be in generating state");
  ok(!card.showWait, "card should not be in waiting state");
  ok(!LinkPreview.downloadingModel, "downloading model flag should not be set");

  panel.remove();
  generateStub.restore();
  LinkPreview.keyboardComboActive = false;
});
