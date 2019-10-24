"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const BRANCH_PREF = "trailhead.firstrun.branches";

/**
 * Sets the trailhead branch pref to the passed value.
 */
async function setTrailheadBranch(value) {
  Services.prefs.setCharPref(BRANCH_PREF, value);

  // Reset trailhead so it loads the new branch.
  Services.prefs.clearUserPref("trailhead.firstrun.didSeeAboutWelcome");
  await ASRouter.setState({ trailheadInitialized: false });
  await ASRouter.setupTrailhead();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(BRANCH_PREF);
  });
}

/**
 * Test a specific trailhead branch.
 */
async function test_trailhead_branch(
  branchName,
  expectedSelectors = [],
  unexpectedSelectors = []
) {
  await setTrailheadBranch(branchName);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    false
  );
  let browser = tab.linkedBrowser;

  await ContentTask.spawn(
    browser,
    { expectedSelectors, branchName, unexpectedSelectors },
    async ({
      expectedSelectors: expected,
      branchName: branch,
      unexpectedSelectors: unexpected,
    }) => {
      for (let selector of expected) {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector(selector),
          `Should render ${selector} in the ${branch} branch`
        );
      }
      for (let selector of unexpected) {
        ok(
          !content.document.querySelector(selector),
          `Should not render ${selector} in the ${branch} branch`
        );
      }
    }
  );

  BrowserTestUtils.removeTab(tab);
}

/**
 * Test the the various trailhead branches.
 */
add_task(async function test_trailhead_branches() {
  await test_trailhead_branch(
    "join-privacy",
    // Expected selectors:
    [
      ".trailhead.joinCohort",
      "button[data-l10n-id=onboarding-browse-privately-button]",
      "button[data-l10n-id=onboarding-tracking-protection-button2]",
      "button[data-l10n-id=onboarding-lockwise-passwords-button2]",
    ]
  );

  await test_trailhead_branch(
    "sync-supercharge",
    // Expected selectors:
    [
      ".trailhead.syncCohort",
      "button[data-l10n-id=onboarding-data-sync-button2]",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
      "button[data-l10n-id=onboarding-mobile-phone-button]",
    ]
  );

  await test_trailhead_branch(
    "modal_variant_a-supercharge",
    // Expected selectors:
    [
      ".trailhead.joinCohort",
      "p[data-l10n-id=onboarding-benefit-sync-text]",
      "p[data-l10n-id=onboarding-benefit-monitor-text]",
      "p[data-l10n-id=onboarding-benefit-lockwise-text]",
    ]
  );

  await test_trailhead_branch(
    "modal_variant_f-supercharge",
    // Expected selectors:
    [
      ".trailhead.joinCohort",
      "h3[data-l10n-id=onboarding-welcome-form-header]",
      "p[data-l10n-id=onboarding-benefit-products-text]",
      "p[data-l10n-id=onboarding-benefit-knowledge-text]",
      "p[data-l10n-id=onboarding-benefit-privacy-text]",
    ]
  );

  await test_trailhead_branch(
    "full_page_d-supercharge",
    // Expected selectors:
    [
      ".trailhead-fullpage",
      ".trailheadCard",
      "p[data-l10n-id=onboarding-benefit-products-text]",
      "button[data-l10n-id=onboarding-join-form-continue]",
      "button[data-l10n-id=onboarding-join-form-signin]",
    ]
  );

  await test_trailhead_branch(
    "full_page_e-supercharge",
    // Expected selectors:
    [
      ".fullPageCardsAtTop",
      ".trailhead-fullpage",
      ".trailheadCard",
      "p[data-l10n-id=onboarding-benefit-products-text]",
      "button[data-l10n-id=onboarding-join-form-continue]",
      "button[data-l10n-id=onboarding-join-form-signin]",
    ]
  );

  await test_trailhead_branch(
    "cards-multidevice",
    // Expected selectors:
    [
      "button[data-l10n-id=onboarding-mobile-phone-button]",
      "button[data-l10n-id=onboarding-pocket-anywhere-button]",
      "button[data-l10n-id=onboarding-send-tabs-button]",
    ],
    // Unexpected selectors:
    ["#trailheadDialog"]
  );

  await test_trailhead_branch(
    "join-payoff",
    // Expected selectors:
    [
      ".trailhead.joinCohort",
      "button[data-l10n-id=onboarding-firefox-monitor-button]",
      "button[data-l10n-id=onboarding-facebook-container-button]",
      "button[data-l10n-id=onboarding-firefox-send-button]",
    ]
  );

  await test_trailhead_branch(
    "nofirstrun",
    [],
    // Unexpected selectors:
    ["#trailheadDialog", ".trailheadCards"]
  );
});
