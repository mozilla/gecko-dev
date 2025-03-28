/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

"use strict";

const ABOUT_ABOUT = "about:about";
const CONTENT_PAGE = "https://example.com/1";

ChromeUtils.defineESModuleGetters(this, {
  ContentTaskUtils: "resource://testing-common/ContentTaskUtils.sys.mjs",
});

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["browser.shopping.experience2023.enabled", false],
      ["sidebar.main.tools", "aichat,reviewchecker,syncedtabs,history"],
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  registerCleanupFunction(async () => {
    SidebarController.hide();
    Services.fog.testResetFOG();
    await Services.fog.testFlushAllChildren();
  });
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();
});

add_task(async function test_no_reliability_available() {
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();
  await BrowserTestUtils.withNewTab(NEEDS_ANALYSIS_TEST_URL, async () => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(NEEDS_ANALYSIS_TEST_URL);
  });

  await Services.fog.testFlushAllChildren();
  var sawPageEvents =
    Glean.shopping.surfaceNoReviewReliabilityAvailable.testGetValue();

  Assert.equal(sawPageEvents.length, 1);
  Assert.equal(sawPageEvents[0].category, "shopping");
  Assert.equal(
    sawPageEvents[0].name,
    "surface_no_review_reliability_available"
  );

  SidebarController.hide();
});

add_task(
  async function test_notification_card_impression_and_buttons_clicked() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.autoOpen.enabled", true],
        ["browser.shopping.experience2023.autoOpen.userEnabled", true],
        ["browser.shopping.experience2023.newPositionCard.hasSeen", false],
        ["sidebar.position_start", true],
      ],
    });
    Services.fog.testResetFOG();
    await Services.fog.testFlushAllChildren();
    await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async () => {
      await SidebarController.show("viewReviewCheckerSidebar");
      info("Waiting for sidebar to update.");
      await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

      await withReviewCheckerSidebar(async _args => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector("shopping-container")
              ?.wrappedJSObject,
          "Review Checker is loaded."
        );
        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.showNewPositionCard !== "undefined",
          "showNewPositionCard is set."
        );
        await shoppingContainer.updateComplete;

        Assert.ok(
          shoppingContainer.showNewPositionCard,
          "showNewPositionCard is true"
        );

        Assert.ok(
          shoppingContainer.newPositionNotificationCardEl,
          "new-position-notification-card is visible"
        );

        let card = shoppingContainer.newPositionNotificationCardEl;

        Assert.ok(card, "new-position-notification-card is visible");

        let buttonChangePromise = ContentTaskUtils.waitForCondition(() => {
          card = shoppingContainer.newPositionNotificationCardEl;
          return !!card.moveLeftButtonEl;
        }, "Button changed to 'Move to left'");

        Assert.ok(card.moveRightButtonEl, "Card has 'Move right' button");
        card.moveRightButtonEl.click();

        await buttonChangePromise;

        Assert.ok(card.moveLeftButtonEl, "Card has 'Move left' button");
        card.moveLeftButtonEl.click();

        Assert.ok(card.dismissButtonEl, "Card has dismiss button");
        card.dismissButtonEl.click();
      });
    });

    await Services.fog.testFlushAllChildren();
    let impressionEvents =
      Glean.shopping.surfaceNotificationCardImpression.testGetValue();

    Assert.equal(impressionEvents.length, 1);
    Assert.equal(impressionEvents[0].category, "shopping");
    Assert.equal(
      impressionEvents[0].name,
      "surface_notification_card_impression"
    );

    let moveRightClickedEvents =
      Glean.shopping.surfaceNotificationCardMoveRightClicked.testGetValue();
    Assert.equal(moveRightClickedEvents.length, 1);
    Assert.equal(moveRightClickedEvents[0].category, "shopping");
    Assert.equal(
      moveRightClickedEvents[0].name,
      "surface_notification_card_move_right_clicked"
    );

    let moveLeftClickedEvents =
      Glean.shopping.surfaceNotificationCardMoveLeftClicked.testGetValue();
    Assert.equal(moveLeftClickedEvents.length, 1);
    Assert.equal(moveLeftClickedEvents[0].category, "shopping");
    Assert.equal(
      moveLeftClickedEvents[0].name,
      "surface_notification_card_move_left_clicked"
    );

    let dismissClickedEvents =
      Glean.shopping.surfaceNotificationCardDismissClicked.testGetValue();
    Assert.equal(dismissClickedEvents.length, 1);
    Assert.equal(dismissClickedEvents[0].category, "shopping");
    Assert.equal(
      dismissClickedEvents[0].name,
      "surface_notification_card_dismiss_clicked"
    );

    SidebarController.hide();
  }
);

add_task(
  async function test_notification_card_impression_and_sidebar_settings_clicked() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.autoOpen.enabled", true],
        ["browser.shopping.experience2023.autoOpen.userEnabled", true],
        ["browser.shopping.experience2023.newPositionCard.hasSeen", false],
        ["sidebar.position_start", true],
      ],
    });
    Services.fog.testResetFOG();
    await Services.fog.testFlushAllChildren();
    await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async () => {
      await SidebarController.show("viewReviewCheckerSidebar");
      info("Waiting for sidebar to update.");
      await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

      await withReviewCheckerSidebar(async _args => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector("shopping-container")
              ?.wrappedJSObject,
          "Review Checker is loaded."
        );
        await ContentTaskUtils.waitForCondition(
          () => typeof shoppingContainer.showNewPositionCard !== "undefined",
          "showNewPositionCard is set."
        );
        await shoppingContainer.updateComplete;

        Assert.ok(
          shoppingContainer.showNewPositionCard,
          "showNewPositionCard is true"
        );

        Assert.ok(
          shoppingContainer.newPositionNotificationCardEl,
          "new-position-notification-card is visible"
        );

        let card = shoppingContainer.newPositionNotificationCardEl;

        Assert.ok(card, "new-position-notification-card is visible");
        Assert.ok(card.settingsLinkEl, "Card has settings link");

        card.settingsLinkEl.click();
      });
    });

    await Services.fog.testFlushAllChildren();
    let impressionEvents =
      Glean.shopping.surfaceNotificationCardImpression.testGetValue();

    Assert.equal(impressionEvents.length, 1);
    Assert.equal(impressionEvents[0].category, "shopping");
    Assert.equal(
      impressionEvents[0].name,
      "surface_notification_card_impression"
    );

    let settingsClickedEvents =
      Glean.shopping.surfaceNotificationCardSidebarSettingsClicked.testGetValue();
    Assert.equal(settingsClickedEvents.length, 1);
    Assert.equal(settingsClickedEvents[0].category, "shopping");
    Assert.equal(
      settingsClickedEvents[0].name,
      "surface_notification_card_sidebar_settings_clicked"
    );

    SidebarController.hide();
  }
);

/**
 * Tests that Glean.shopping.surface_displayed is recorded as expected
 * when navigating between pages for a single tab.
 */
add_task(async function test_surface_displayed_same_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.autoOpen.userEnabled", false]],
  });
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async browser => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await Services.fog.testFlushAllChildren();

    let surfaceDisplayedEvents =
      await Glean.shopping.surfaceDisplayed.testGetValue();
    Assert.equal(
      surfaceDisplayedEvents.length,
      1,
      "There should be a surfaceDisplayed event after opening a PDP"
    );
    let pdpEvent = surfaceDisplayedEvents[0];
    verifySurfaceDisplayedEvent(pdpEvent, "true", "false");

    // Test supported site
    let loadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      CONTENT_PAGE
    );
    BrowserTestUtils.startLoadingURIString(browser, CONTENT_PAGE);
    info("Loading supported site in the same tab.");
    await loadedPromise;

    await Services.fog.testFlushAllChildren();

    surfaceDisplayedEvents =
      await Glean.shopping.surfaceDisplayed.testGetValue();
    Assert.equal(
      surfaceDisplayedEvents.length,
      2,
      "There should be a surfaceDisplayed event after opening a supported site"
    );
    let supportedSiteEvent = surfaceDisplayedEvents[1];
    verifySurfaceDisplayedEvent(supportedSiteEvent, "false", "true");

    // Test unsupported site
    loadedPromise = BrowserTestUtils.browserLoaded(browser, false, ABOUT_ABOUT);
    BrowserTestUtils.startLoadingURIString(browser, ABOUT_ABOUT);
    info("Loading unsupported site in the same tab.");
    await loadedPromise;

    await Services.fog.testFlushAllChildren();

    surfaceDisplayedEvents =
      await Glean.shopping.surfaceDisplayed.testGetValue();
    Assert.equal(
      surfaceDisplayedEvents.length,
      3,
      "There should be a surfaceDisplayed event after opening an unsupported site"
    );
    let unsupportedSiteEvent = surfaceDisplayedEvents[2];
    verifySurfaceDisplayedEvent(unsupportedSiteEvent, "false", "false");
  });

  SidebarController.hide();
});

/**
 * Tests that Glean.shopping.surface_displayed is recorded as expected
 * when navigating to different pages in new tabs.
 */
add_task(async function test_surface_displayed_multiple_tabs() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.autoOpen.userEnabled", false]],
  });
  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  await testSurfaceDisplayedNewTab(PRODUCT_TEST_URL, 0, {
    isProductPage: "true",
    isSupportedSite: "false",
  });
  await testSurfaceDisplayedNewTab(OTHER_PRODUCT_TEST_URL, 1, {
    isProductPage: "true",
    isSupportedSite: "false",
  });
  await testSurfaceDisplayedNewTab(CONTENT_PAGE, 2, {
    isProductPage: "false",
    isSupportedSite: "true",
  });
  await testSurfaceDisplayedNewTab(ABOUT_ABOUT, 3, {
    isProductPage: "false",
    isSupportedSite: "false",
  });

  SidebarController.hide();
});

/**
 * Test helper function that adds a new foregrounded tab, loads a page
 * with RC open, and verifies Glean.shopping.surface_displayed.
 *
 * @param {string} url
 *  The url to load
 * @param {number} eventPosition
 *  The expected position index of an event we want to test
 * @param {object} expectedObj
 *  Object of expected string values for the event, like
 *  isProductPage and isSupportedSite.
 */
async function testSurfaceDisplayedNewTab(url, eventPosition = 0, expectedObj) {
  if (!url || !expectedObj || !Object.entries(expectedObj)?.length) {
    Assert.ok(false, "There was a problem running testSurfaceDisplayedNewTab");
    return;
  }

  await SidebarController.show("viewReviewCheckerSidebar");

  let expectedIsProductPage = expectedObj.isProductPage;
  let expectedIsSupportedSite = expectedObj.isSupportedSite;

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Waiting for sidebar to update.");
  await reviewCheckerSidebarUpdated(url);

  await Services.fog.testFlushAllChildren();

  let surfaceDisplayedEvents =
    await Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(
    surfaceDisplayedEvents.length,
    eventPosition + 1,
    "Got correct number of recorded events"
  );
  let event = surfaceDisplayedEvents[eventPosition];
  verifySurfaceDisplayedEvent(
    event,
    expectedIsProductPage,
    expectedIsSupportedSite
  );

  BrowserTestUtils.removeTab(tab);
}

/**
 * Test helper function to verify surface_displayed event details.
 *
 * @param {object} surfaceDisplayedEvent
 *  An event in the surface_displayed events array
 * @param {string} expectedIsProductPageVal
 *  Expected isProductPage property value to pass the test
 * @param {string} expectedIsSupportedSiteVal
 *  Expected isSupportedSite property value to pass the test
 */
function verifySurfaceDisplayedEvent(
  surfaceDisplayedEvent,
  expectedIsProductPageVal,
  expectedIsSupportedSiteVal
) {
  Assert.equal(surfaceDisplayedEvent.category, "shopping");
  Assert.equal(surfaceDisplayedEvent.name, "surface_displayed");
  Assert.equal(
    surfaceDisplayedEvent.extra.isIntegratedSidebar,
    "true",
    "isIntegratedSidebar key is true"
  );
  Assert.equal(
    surfaceDisplayedEvent.extra.isProductPage,
    expectedIsProductPageVal,
    `isProductPage key is ${expectedIsProductPageVal}`
  );
  Assert.equal(
    surfaceDisplayedEvent.extra.isSupportedSite,
    expectedIsSupportedSiteVal,
    `isSupportedSite key is ${expectedIsSupportedSiteVal}`
  );
}
