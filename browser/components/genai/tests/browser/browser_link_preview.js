/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LinkPreview } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreview.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const TEST_LINK_URL = "https://example.com";

/**
 * Tests that the Link Preview feature is correctly triggered when the Alt key is pressed.
 *
 * This test performs the following steps:
 * 1. Enables the Link Preview feature via the preference `"browser.ml.linkPreview.enabled"`.
 * 2. Creates and dispatches a `keydown` event with the `altKey` property set to `true`.
 * 3. Sets an over link using `XULBrowserWindow.setOverLink`.
 * 4. Verifies that the `_maybeLinkPreview` method of `LinkPreview` is called with the correct window.
 */
add_task(async function test_link_preview_with_alt_key_event() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.enabled", true]],
  });

  let stub = sinon.stub(LinkPreview, "_maybeLinkPreview");

  let keydownEvent = new KeyboardEvent("keydown", {
    bubbles: true,
    cancelable: true,
    altKey: true,
  });
  window.dispatchEvent(keydownEvent);

  XULBrowserWindow.setOverLink(TEST_LINK_URL, {});

  ok(
    stub.calledWith(window),
    "_maybeLinkPreview was called with the correct window"
  );

  stub.restore();
  Services.prefs.clearUserPref("browser.ml.linkPreview.enabled");
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
  });
  window.dispatchEvent(keydownEvent);

  XULBrowserWindow.setOverLink(TEST_LINK_URL, {});

  ok(
    !stub.called,
    "_maybeLinkPreview should not be called when the feature is disabled"
  );

  stub.restore();
  Services.prefs.clearUserPref("browser.ml.linkPreview.enabled");
});
