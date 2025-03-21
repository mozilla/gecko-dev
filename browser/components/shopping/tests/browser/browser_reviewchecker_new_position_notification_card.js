/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

const CONTENT_PAGE = "https://example.com";
const NON_PDP_PAGE = "about:about";

async function testMoveToRight() {
  await withReviewCheckerSidebar(async _args => {
    let card;
    let shoppingContainer = await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("shopping-container")?.wrappedJSObject,
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

    card = shoppingContainer.newPositionNotificationCardEl;
    Assert.ok(card, "new-position-notification-card is visible");
    Assert.ok(card.moveRightButtonEl, "Card has 'Move right' button");

    let buttonChangePromise = ContentTaskUtils.waitForCondition(() => {
      card = shoppingContainer.newPositionNotificationCardEl;
      return !!card.moveLeftButtonEl;
    }, "Button changed to 'Move to left'");

    const { TestUtils } = ChromeUtils.importESModule(
      "resource://testing-common/TestUtils.sys.mjs"
    );
    let positionStartPrefUpdated = TestUtils.waitForPrefChange(
      "sidebar.position_start"
    );

    card.moveRightButtonEl.click();

    await card.updateComplete;
    await buttonChangePromise;
    await positionStartPrefUpdated;

    card = shoppingContainer.newPositionNotificationCardEl;
    Assert.ok(card.moveLeftButtonEl, "Card has 'Move to left' button");
    Assert.ok(
      !card.moveRightButtonEl,
      "Card no longer has 'Move to right' button"
    );
  });
}

async function testMoveToLeft() {
  await withReviewCheckerSidebar(async _args => {
    let card;
    let shoppingContainer = await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("shopping-container")?.wrappedJSObject,
      "Review Checker is loaded."
    );

    let buttonChangePromise = ContentTaskUtils.waitForCondition(() => {
      card = shoppingContainer.newPositionNotificationCardEl;
      return !!card.moveRightButtonEl;
    }, "Button changed to 'Move to right'");

    card = shoppingContainer.newPositionNotificationCardEl;
    card.moveLeftButtonEl.click();

    await card.updateComplete;
    await buttonChangePromise;

    card = shoppingContainer.newPositionNotificationCardEl;
    Assert.ok(
      !card.moveLeftButtonEl,
      "Card no longer has 'Move to left' button"
    );
    Assert.ok(card.moveRightButtonEl, "Card has 'Move to right' button");
  });
}

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["browser.shopping.experience2023.shoppingSidebar", false],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
      ["sidebar.main.tools", "aichat,reviewchecker,syncedtabs,history"],
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  registerCleanupFunction(async () => {
    SidebarController.hide();
  });
});

/**
 * Tests that the new position notification card is visible after auto open on a product page
 * and is correctly rendered. Also tests that the card is not visible on non PDPs after the
 * initial render.
 */
add_task(async function test_new_position_notification_card_visibility() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.newPositionCard.hasSeen", false],
      ["sidebar.position_start", true],
      ["browser.shopping.experience2023.autoClose.userEnabled", false],
    ],
  });
  /* First, load a non PDP so that we can then make RC auto open once we
   * navigate to an actual PDP. Make sure the card is not visible on a non PDP. */
  await BrowserTestUtils.withNewTab(NON_PDP_PAGE, async _browser => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(NON_PDP_PAGE);

    await withReviewCheckerSidebar(async () => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );
      await shoppingContainer.updateComplete;
      Assert.ok(
        !shoppingContainer.showNewPositionCard,
        "New position notification card is not visible"
      );
    });

    // Add a new tab
    let newProductTab = BrowserTestUtils.addTab(
      gBrowser,
      OTHER_PRODUCT_TEST_URL
    );
    let newProductBrowser = newProductTab.linkedBrowser;
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      newProductBrowser,
      false,
      OTHER_PRODUCT_TEST_URL
    );
    await browserLoadedPromise;

    // Hide the sidebar.
    info("Hiding the sidebar");
    SidebarController.hide();

    let shownPromise = BrowserTestUtils.waitForEvent(window, "SidebarShown");

    info("Switching tabs now");
    await BrowserTestUtils.switchTab(gBrowser, newProductTab);

    Assert.ok(true, "Browser is loaded");

    info("Waiting for shown");
    await shownPromise;

    await TestUtils.waitForTick();

    Assert.ok(SidebarController.isOpen, "Sidebar is open now");

    await withReviewCheckerSidebar(async _args => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
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

      let card = shoppingContainer.newPositionNotificationCardEl;

      Assert.ok(card, "new-position-notification-card is visible");
      Assert.ok(card.imgEl, "Card has image");
      Assert.ok(card.settingsLinkEl, "Card has settings link");
      Assert.ok(card.moveRightButtonEl, "Card has 'Move right' button");
      Assert.ok(card.dismissButtonEl, "Card has dismiss button");
    });

    // Load a non PDP for the new tab
    BrowserTestUtils.startLoadingURIString(newProductBrowser, NON_PDP_PAGE);
    browserLoadedPromise = BrowserTestUtils.browserLoaded(
      newProductBrowser,
      false,
      NON_PDP_PAGE
    );
    await browserLoadedPromise;

    await withReviewCheckerSidebar(async _args => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );

      let card = shoppingContainer.newPositionNotificationCardEl;
      Assert.ok(!card, "Card is no longer visible");
    });

    let hasSeen = Services.prefs.getBoolPref(
      "browser.shopping.experience2023.newPositionCard.hasSeen"
    );

    // We check that the pref is still false here since there is no further user action.
    Assert.ok(
      !hasSeen,
      "browser.shopping.experience2023.newPositionCard.hasSeen == false"
    );

    await BrowserTestUtils.removeTab(newProductTab);
  });
  SidebarController.hide();
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the sidebar changes position after pressing the Move left or Move right button on
 * the new position notification card.
 */
add_task(async function test_new_position_notification_card_change_position() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.newPositionCard.hasSeen", false],
      ["sidebar.position_start", true],
    ],
  });
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async _browser => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await testMoveToRight();

    await TestUtils.waitForTick();

    let hasSeen = Services.prefs.getBoolPref(
      "browser.shopping.experience2023.newPositionCard.hasSeen"
    );
    Assert.ok(
      !hasSeen,
      "browser.shopping.experience2023.newPositionCard.hasSeen is false after reverse position"
    );

    let startPosition = Services.prefs.getBoolPref("sidebar.position_start");
    Assert.ok(!startPosition, "sidebar.position_start is false");

    await testMoveToLeft();

    await TestUtils.waitForTick();

    hasSeen = Services.prefs.getBoolPref(
      "browser.shopping.experience2023.newPositionCard.hasSeen"
    );
    Assert.ok(
      !hasSeen,
      "browser.shopping.experience2023.newPositionCard.hasSeen is still false after undoing reverse position"
    );

    startPosition = Services.prefs.getBoolPref("sidebar.position_start");
    Assert.ok(startPosition, "sidebar.position_start is now true");
  });
  SidebarController.hide();
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the new position notification card is hidden after pressing the dismiss button.
 */
add_task(async function test_new_position_notification_card_dismiss() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.newPositionCard.hasSeen", false],
      ["sidebar.position_start", true],
    ],
  });
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async _browser => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await withReviewCheckerSidebar(async _args => {
      let card;
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
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

      card = shoppingContainer.newPositionNotificationCardEl;
      Assert.ok(card, "new-position-notification-card is visible");
      Assert.ok(card.dismissButtonEl, "Card has the dismiss button");

      let cardVisibilityPromise = ContentTaskUtils.waitForCondition(() => {
        card = shoppingContainer.newPositionNotificationCardEl;
        return !card;
      }, "Card is no longer visible");

      card.dismissButtonEl.click();
      await cardVisibilityPromise;
    });

    await TestUtils.waitForTick();

    let hasSeen = Services.prefs.getBoolPref(
      "browser.shopping.experience2023.newPositionCard.hasSeen"
    );
    Assert.ok(
      hasSeen,
      "browser.shopping.experience2023.newPositionCard.hasSeen is true"
    );

    let startPosition = Services.prefs.getBoolPref("sidebar.position_start");
    Assert.ok(startPosition, "sidebar.position_start is still true");
  });
  SidebarController.hide();
  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the sidebar settings panel is displayed after pressing the settings link
 * in the new position notification card.
 */
add_task(async function test_new_position_notification_card_show_settings() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.newPositionCard.hasSeen", false],
      ["sidebar.position_start", true],
    ],
  });
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async _browser => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await withReviewCheckerSidebar(async _args => {
      let card;
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
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

      card = shoppingContainer.newPositionNotificationCardEl;
      Assert.ok(card, "new-position-notification-card is visible");
      Assert.ok(card.settingsLinkEl, "Card has the sidebar settings link");

      card.settingsLinkEl.click();
    });

    await TestUtils.waitForTick();

    Assert.equal(
      SidebarController.currentID,
      "viewCustomizeSidebar",
      "The sidebar settings panel is open"
    );

    let hasSeen = Services.prefs.getBoolPref(
      "browser.shopping.experience2023.newPositionCard.hasSeen"
    );
    Assert.ok(
      hasSeen,
      "browser.shopping.experience2023.newPositionCard.hasSeen is true"
    );

    let startPosition = Services.prefs.getBoolPref("sidebar.position_start");
    Assert.ok(startPosition, "sidebar.position_start is still true");
  });
  SidebarController.hide();
  await SpecialPowers.popPrefEnv();
});
