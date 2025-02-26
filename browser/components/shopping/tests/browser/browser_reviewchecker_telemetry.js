/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

"use strict";

const CONTENT_PAGE = "https://example.com";

ChromeUtils.defineESModuleGetters(this, {
  ContentTaskUtils: "resource://testing-common/ContentTaskUtils.sys.mjs",
});

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["browser.shopping.experience2023.shoppingSidebar", false],
      ["sidebar.main.tools", "aichat,reviewchecker,syncedtabs,history"],
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  registerCleanupFunction(async () => {
    SidebarController.hide();
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
