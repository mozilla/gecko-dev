/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LinkPreview } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreview.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { LinkPreviewModel } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreviewModel.sys.mjs"
);

const TEST_LINK_URL_EN =
  "https://example.com/browser/browser/components/genai/tests/browser/data/readableEn.html";
const TEST_LINK_URL_FR =
  "https://example.com/browser/browser/components/genai/tests/browser/data/readableFr.html";

function clearOverlink() {
  // Clear the state by setting it to the FR URL
  XULBrowserWindow.setOverLink(TEST_LINK_URL_FR);
}

async function waitForPanelOpen(
  message = "waiting for onboarding panel to open"
) {
  return await TestUtils.waitForCondition(() => {
    const panel = document.getElementById("link-preview-panel");
    return panel?.state == "open" ? panel : null;
  }, message);
}

/**
 * Tests that the onboarding panel is shown when preferences indicate it should be shown.
 */
add_task(async function test_show_onboarding_close_button() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.onboardingTimes", ""],
    ],
  });

  XULBrowserWindow.setOverLink(TEST_LINK_URL_EN);

  const panel = await waitForPanelOpen();
  ok(panel, "Panel created for onboarding");

  const onboarding_card = panel.querySelector("link-preview-card-onboarding");
  ok(onboarding_card, "onboarding element is present");
  const ogcard = onboarding_card.shadowRoot.querySelector(
    ".og-card.onboarding"
  );
  ok(ogcard, "ogcard element is present");

  const closeButton = onboarding_card.shadowRoot.querySelector(
    "#onboarding-close-button"
  );
  ok(closeButton, "close button is present");
  closeButton.click();

  ok(
    Services.prefs.prefHasUserValue("browser.ml.linkPreview.onboardingTimes"),
    "onboardingTimes was set"
  );

  // Cleanup
  panel.remove();
});

add_task(async function test_try_it_now_button() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.onboardingTimes", ""],
    ],
  });

  clearOverlink();
  XULBrowserWindow.setOverLink(TEST_LINK_URL_EN);

  const panel = await waitForPanelOpen();
  ok(panel, "Panel created for onboarding");

  const onboarding_card = panel.querySelector("link-preview-card-onboarding");
  ok(onboarding_card, "onboarding element is present");

  const tryItNowButton = onboarding_card.shadowRoot.querySelector(
    "moz-button[data-l10n-id='link-preview-onboarding-button']"
  );
  ok(tryItNowButton, "Try it now button is present");
  tryItNowButton.click();

  is(
    Services.prefs.getBoolPref("browser.ml.linkPreview.collapsed"),
    false,
    "collapsed pref should be set to false after completing onboarding"
  );

  ok(
    Services.prefs.prefHasUserValue("browser.ml.linkPreview.onboardingTimes"),
    "onboardingTimes was set"
  );

  // Cleanup
  panel.remove();
});

/**
 * Test that onboarding is shown when under max show frequency
 */
add_task(async function test_onboarding_max_show_frequency() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.onboardingTimes", "121"],
      ["browser.ml.linkPreview.onboardingMaxShowFreq", 2],
    ],
  });

  ok(
    LinkPreview.showOnboarding,
    "Should show onboarding when under max frequency"
  );

  clearOverlink();
  XULBrowserWindow.setOverLink(TEST_LINK_URL_EN);

  let panel = await TestUtils.waitForCondition(() =>
    document.getElementById("link-preview-panel")
  );
  ok(panel, "Panel created for onboarding under max frequency");

  let onboarding_card = panel.querySelector("link-preview-card-onboarding");
  ok(onboarding_card, "Card created for onboarding under max frequency");

  panel.remove();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.onboardingTimes", "121,12212"],
      ["browser.ml.linkPreview.onboardingMaxShowFreq", 2],
    ],
  });

  ok(
    !LinkPreview.showOnboarding,
    "Should not show onboarding when max frequency is reached"
  );

  clearOverlink();
  XULBrowserWindow.setOverLink(TEST_LINK_URL_EN);

  panel = document.getElementById("link-preview-panel");
  is(panel, null, "Panel should not be created when max frequency is reached");
});

/**
 * Test that onboarding respects cooldown period
 */
add_task(async function test_onboarding_cooldown_period() {
  // // Test initial setup - no timestamps, should show onboarding
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.onboardingTimes", ""],
      ["browser.ml.linkPreview.onboardingMaxShowFreq", 2],
      [
        "browser.ml.linkPreview.onboardingCooldownPeriodMS",
        7 * 24 * 60 * 60 * 1000,
      ], // 7 days cooldown
    ],
  });

  // Initially should show onboarding since no previous timestamps
  ok(
    LinkPreview.showOnboarding,
    "Should show onboarding initially with empty timestamps"
  );

  // Set the first timestamp (now) which means we just showed onboarding
  const now = Date.now();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.onboardingTimes", `${now}`]],
  });

  // Should not show onboarding now since we just showed it (within cooldown period)
  ok(
    !LinkPreview.showOnboarding,
    "Should not show onboarding immediately after showing it once"
  );

  // Change cooldown to 5 days and test with a 6-day-old timestamp
  const eightDaysAgo = now - 8 * 24 * 60 * 60 * 1000;
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.onboardingTimes", `${eightDaysAgo}`],
      ["browser.ml.linkPreview.onboardingMaxShowFreq", 2],
    ],
  });

  // Should show onboarding since last time was 6 days ago, outside the 5-day cooldown
  ok(
    LinkPreview.showOnboarding,
    "Should show onboarding after cooldown period has passed"
  );
});

/**
 * Test that using link preview (not via onboarding) sets the onboardingTimes
 * preference to effectively disable future onboarding prompts.
 */
add_task(async function test_onboarding_times_set_on_preview_usage() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.linkPreview.enabled", true],
      ["browser.ml.linkPreview.onboardingMaxShowFreq", 2],
      ["browser.ml.linkPreview.onboardingTimes", ""],
    ],
  });

  const generateStub = sinon.stub(LinkPreviewModel, "generateTextAI");

  const READABLE_PAGE_URL =
    "https://example.com/browser/browser/components/genai/tests/browser/data/readableEn.html";

  LinkPreview.keyboardComboActive = "shift";
  XULBrowserWindow.setOverLink(READABLE_PAGE_URL, {});

  const panel = await waitForPanelOpen();
  ok(panel, "Panel created for link preview");

  // Check the preference value
  // renderLinkPreviewPanel sets this to "0,0" when onboardingMaxShowFreq is 2
  // and source is not "onboarding".
  is(
    Services.prefs.getCharPref("browser.ml.linkPreview.onboardingTimes"),
    "0,0",
    "onboardingTimes should be set to '0,0' after link preview usage"
  );

  // Cleanup
  panel.remove();
  generateStub.restore();
  LinkPreview.keyboardComboActive = false;
  Services.prefs.clearUserPref("browser.ml.linkPreview.onboardingTimes");
});
