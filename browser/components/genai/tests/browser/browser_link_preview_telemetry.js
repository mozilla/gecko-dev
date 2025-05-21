/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LinkPreview } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreview.sys.mjs"
);

// Import the model for mocking
const { LinkPreviewModel } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreviewModel.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async function test_default_telemetry() {
  Services.fog.testResetFOG();

  Assert.equal(
    Glean.genaiLinkpreview.cardAiConsent.testGetValue(),
    null,
    "No cardAiConsent events"
  );

  Assert.equal(
    Glean.genaiLinkpreview.keyPointsToggle.testGetValue(),
    null,
    "No keyPointsToggle events"
  );
});

/**
 * Test that AI consent telemetry is recorded when a user clicks "continue" in the opt-in UI
 */
add_task(async function test_link_preview_ai_consent_continue_ui_interaction() {
  Services.fog.testResetFOG();

  // Setup preferences for a clean state
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.optin", false],
      ["browser.ml.linkPreview.collapsed", false],
    ],
  });

  let events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
  Assert.equal(events.length, 1, "One deny opt-in event recorded");

  const generateStub = sinon.stub(LinkPreviewModel, "generateTextAI");

  const READABLE_PAGE_URL =
    "https://example.com/browser/browser/components/genai/tests/browser/data/readableEn.html";

  LinkPreview.keyboardComboActive = true;
  XULBrowserWindow.setOverLink(READABLE_PAGE_URL, {});

  const panel = await TestUtils.waitForCondition(() =>
    document.getElementById("link-preview-panel")
  );
  await BrowserTestUtils.waitForEvent(panel, "popupshown");

  const card = panel.querySelector("link-preview-card");
  ok(card, "card created for link preview");

  const modelOptinElement = await TestUtils.waitForCondition(() => {
    if (card.shadowRoot) {
      return card.shadowRoot.querySelector("model-optin");
    }
    return null;
  }, "Waiting for model-optin element");

  ok(modelOptinElement, "model-optin element is present");

  // Simulate user clicking "Continue" in the opt-in UI
  const optinConfirmEvent = new CustomEvent("MlModelOptinConfirm", {
    bubbles: true,
    composed: true,
  });
  modelOptinElement.dispatchEvent(optinConfirmEvent);

  events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
  Assert.equal(events.length, 2, "Two cardAiConsent events recorded");
  Assert.equal(events[1].extra.option, "continue", "Continue option recorded");

  is(
    Services.prefs.getBoolPref("browser.ml.linkPreview.optin"),
    true,
    "optin preference should be true after confirming"
  );
  is(
    Services.prefs.getBoolPref("browser.ml.linkPreview.collapsed"),
    false,
    "collapsed preference should remain false after confirming"
  );

  // Clean up
  panel.remove();
  generateStub.restore();
  LinkPreview.keyboardComboActive = false;
});

/**
 * Test that AI consent telemetry is recorded when a user clicks "cancel" in the opt-in UI
 */
add_task(async function test_link_preview_ai_consent_cancel_ui_interaction() {
  Services.fog.testResetFOG();

  // Setup preferences for a clean state
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.optin", false],
      ["browser.ml.linkPreview.collapsed", false],
    ],
  });

  let events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
  Assert.equal(events.length, 1, "One deny opt-in event recorded");

  const generateStub = sinon.stub(LinkPreviewModel, "generateTextAI");

  const READABLE_PAGE_URL =
    "https://example.com/browser/browser/components/genai/tests/browser/data/readableEn.html";

  LinkPreview.keyboardComboActive = true;
  XULBrowserWindow.setOverLink(READABLE_PAGE_URL, {});

  const panel = await TestUtils.waitForCondition(() =>
    document.getElementById("link-preview-panel")
  );
  await BrowserTestUtils.waitForEvent(panel, "popupshown");

  const card = panel.querySelector("link-preview-card");
  ok(card, "card created for link preview");

  const modelOptinElement = await TestUtils.waitForCondition(() => {
    if (card.shadowRoot) {
      return card.shadowRoot.querySelector("model-optin");
    }
    return null;
  }, "Waiting for model-optin element");

  ok(modelOptinElement, "model-optin element is present");

  // Simulate user clicking "Cancel" in the opt-in UI
  const optinDenyEvent = new CustomEvent("MlModelOptinDeny", {
    bubbles: true,
    composed: true,
  });
  modelOptinElement.dispatchEvent(optinDenyEvent);

  events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
  Assert.equal(events.length, 2, "Two cardAiConsent events recorded");
  Assert.equal(events[1].extra.option, "cancel", "Cancel option recorded");

  is(
    Services.prefs.getBoolPref("browser.ml.linkPreview.optin"),
    false,
    "optin preference should remain false after denying"
  );
  is(
    Services.prefs.getBoolPref("browser.ml.linkPreview.collapsed"),
    true,
    "collapsed preference should be true after denying"
  );

  // Clean up
  panel.remove();
  generateStub.restore();
  LinkPreview.keyboardComboActive = false;
});

/**
 * Test that telemetry events are properly recorded when a user toggles between
 * expanded and collapsed states of the link preview card. Verifies that each
 * toggle action generates the appropriate keyPointsToggle event with the correct
 * expand state value.
 */
add_task(async function test_toggle_expand_collapse_telemetry() {
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.optin", true],
      ["browser.ml.linkPreview.collapsed", false],
    ],
  });

  let events = Glean.genaiLinkpreview.keyPointsToggle.testGetValue();
  Assert.equal(events.length, 1, "One keyPointsToggle event recorded");

  const generateStub = sinon.stub(LinkPreviewModel, "generateTextAI");

  const READABLE_PAGE_URL =
    "https://example.com/browser/browser/components/genai/tests/browser/data/readableEn.html";

  LinkPreview.keyboardComboActive = true;
  XULBrowserWindow.setOverLink(READABLE_PAGE_URL, {});

  const panel = await TestUtils.waitForCondition(() =>
    document.getElementById("link-preview-panel")
  );
  await BrowserTestUtils.waitForEvent(panel, "popupshown");

  const card = panel.querySelector("link-preview-card");
  ok(card, "Card created for link preview");

  is(card.collapsed, false, "Card should start expanded");

  const keypointsHeader = card.shadowRoot.querySelector(".keypoints-header");
  ok(keypointsHeader, "Found keypoints header");
  keypointsHeader.click();

  is(
    card.collapsed,
    true,
    "Card should now be collapsed after clicking header"
  );

  events = Glean.genaiLinkpreview.keyPointsToggle.testGetValue();
  Assert.equal(events.length, 2, "Two keyPointsToggle events recorded");
  Assert.equal(
    events[1].extra.expand,
    "false",
    "expand=false recorded for collapse action"
  );

  keypointsHeader.click();

  is(
    card.collapsed,
    false,
    "Card should now be expanded after clicking header again"
  );

  events = Glean.genaiLinkpreview.keyPointsToggle.testGetValue();
  Assert.equal(events.length, 3, "Three keyPointsToggle events recorded");
  Assert.equal(
    events[2].extra.expand,
    "true",
    "expand=true recorded for expand action"
  );

  // Clean up
  panel.remove();
  generateStub.restore();
  LinkPreview.keyboardComboActive = false;
});

/**
 * Test that AI consent telemetry is recorded when a user collapses the link preview
 * in the opt-in UI, which implicitly denies consent. This verifies that collapsing
 * the card during the opt-in flow has the same effect as explicitly clicking "Cancel".
 */
add_task(
  async function test_link_preview_ai_consent_toggle_collapse_to_cancel_ui_interaction() {
    Services.fog.testResetFOG();

    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.ml.linkPreview.enabled", true],
        ["browser.ml.linkPreview.optin", false],
        ["browser.ml.linkPreview.collapsed", false],
      ],
    });

    let events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
    Assert.equal(events.length, 1, "One deny opt-in event recorded");

    const generateStub = sinon.stub(LinkPreviewModel, "generateTextAI");

    const READABLE_PAGE_URL =
      "https://example.com/browser/browser/components/genai/tests/browser/data/readableEn.html";

    LinkPreview.keyboardComboActive = true;
    XULBrowserWindow.setOverLink(READABLE_PAGE_URL, {});

    const panel = await TestUtils.waitForCondition(() =>
      document.getElementById("link-preview-panel")
    );
    await BrowserTestUtils.waitForEvent(panel, "popupshown");

    const card = panel.querySelector("link-preview-card");
    ok(card, "card created for link preview");

    const modelOptinElement = await TestUtils.waitForCondition(() => {
      if (card.shadowRoot) {
        return card.shadowRoot.querySelector("model-optin");
      }
      return null;
    }, "Waiting for model-optin element");

    ok(modelOptinElement, "model-optin element is present");

    //deny by clicking collapse
    const keypointsHeader = card.shadowRoot.querySelector(".keypoints-header");
    ok(keypointsHeader, "Found keypoints header");
    keypointsHeader.click();

    is(
      card.collapsed,
      true,
      "Card should now be collapsed after clicking header"
    );

    events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
    Assert.equal(events.length, 2, "Two cardAiConsent events recorded");
    Assert.equal(events[1].extra.option, "cancel", "Cancel option recorded");

    is(
      Services.prefs.getBoolPref("browser.ml.linkPreview.optin"),
      false,
      "optin preference should remain false after denying"
    );
    is(
      Services.prefs.getBoolPref("browser.ml.linkPreview.collapsed"),
      true,
      "collapsed preference should be true after denying"
    );

    // Clean up
    panel.remove();
    generateStub.restore();
    LinkPreview.keyboardComboActive = false;
  }
);
