/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */
// withReviewCheckerSidebar calls SpecialPowers.spawn, which injects
// ContentTaskUtils in the scope of the callback. Eslint doesn't know about
// that.
/* global ContentTaskUtils */

const NON_PDP_PAGE = "about:about";

const HAS_SEEN_PREF = "browser.shopping.experience2023.newPositionCard.hasSeen";
const SIDEBAR_POSITION_START_PREF = "sidebar.position_start";
const SHOW_KEEP_MESSAGE_PREF =
  "browser.shopping.experience2023.showKeepSidebarClosedMessage";

/**
 * Checks that the notification card is visible. Closes RC afterwards.
 */
async function testNotificationCardThenCloseRC() {
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

    Assert.ok(
      shoppingContainer.newPositionNotificationCardEl,
      "new-position-notification-card is visible"
    );
    Assert.ok(shoppingContainer.closeButtonEl, "RC close button is visible");

    shoppingContainer.closeButtonEl.click();

    await shoppingContainer.updateComplete;
  });
}

/**
 * Checks that the notification card is visible before closing the RC panel.
 * Then, dismisses the notification card, checks card visibility, and closes
 * the RC panel.
 */
async function testDismissNotificationThenCloseRC() {
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
      "showNewPositionCard is still true"
    );
    Assert.ok(
      !shoppingContainer.showingKeepClosedMessage,
      "showingKeepClosedMessage is still false"
    );
    Assert.ok(
      !shoppingContainer.keepClosedMessageBarEl,
      "'Keep closed' message is still not visible"
    );

    let card = shoppingContainer.newPositionNotificationCardEl;
    Assert.ok(card, "new-position-notification-card is visible");
    Assert.ok(card.dismissButtonEl, "Card has the dismiss button");
    Assert.ok(shoppingContainer.closeButtonEl, "RC close button is visible");

    let cardVisibilityPromise = ContentTaskUtils.waitForCondition(() => {
      card = shoppingContainer.newPositionNotificationCardEl;
      return !card;
    }, "Card is no longer visible");

    card.dismissButtonEl.click();
    await cardVisibilityPromise;

    Assert.ok(
      !shoppingContainer.newPositionNotificationCardEl,
      "Notification card is no longer visible"
    );

    // Now try clicking the close button to see if we render the "Keep closed" message
    shoppingContainer.closeButtonEl.click();
  });
}

/**
 * Checks that the keep closed message is visible in RC.
 */
async function testKeepClosedIsVisible() {
  await withReviewCheckerSidebar(async _args => {
    let shoppingContainer = await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("shopping-container")?.wrappedJSObject,
      "Review Checker is loaded."
    );

    let keepClosedVisibilityPromise = ContentTaskUtils.waitForCondition(() => {
      let keepClosedMessage = shoppingContainer.keepClosedMessageBarEl;
      return keepClosedMessage;
    }, "'Keep closed' is visible'");

    await shoppingContainer.updateComplete;
    await keepClosedVisibilityPromise;

    Assert.ok(
      shoppingContainer.showingKeepClosedMessage,
      "showingKeepClosedMessage is now true"
    );
    Assert.ok(
      shoppingContainer.keepClosedMessageBarEl,
      "'Keep closed' message is now visible"
    );
  });
}

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["browser.shopping.experience2023.enabled", false],
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
 * Tests that the 'Keep closed' message does not appear when the notification card
 * is set to be displayed.
 */
add_task(
  async function test_do_not_show_keep_closed_if_notification_card_visible() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [HAS_SEEN_PREF, false],
        [SIDEBAR_POSITION_START_PREF, true],
        [SHOW_KEEP_MESSAGE_PREF, true],
        // Set to minimum closed counts met, to speed up testing
        ["browser.shopping.experience2023.sidebarClosedCount", 4],
      ],
    });
    await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async _browser => {
      await SidebarController.show("viewReviewCheckerSidebar");
      info("Waiting for sidebar to update.");
      await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

      let hasSeenPrefUpdated = TestUtils.waitForPrefChange(HAS_SEEN_PREF);

      await testNotificationCardThenCloseRC();

      Assert.ok(
        !SidebarController.isOpen,
        "Sidebar is closed without a problem"
      );

      await hasSeenPrefUpdated;

      let hasSeen = Services.prefs.getBoolPref(HAS_SEEN_PREF);
      Assert.ok(
        hasSeen,
        "browser.shopping.experience2023.newPositionCard.hasSeen is true after closing RC"
      );
    });

    await SpecialPowers.popPrefEnv();
  }
);

/**
 * Tests that the 'Keep closed' message is shown after dismissing the notification card.
 */
add_task(
  async function test_do_not_show_keep_closed_until_notification_dismissed() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [HAS_SEEN_PREF, false],
        [SIDEBAR_POSITION_START_PREF, true],
        [SHOW_KEEP_MESSAGE_PREF, true],
        // Set to minimum closed counts met, to speed up testing
        ["browser.shopping.experience2023.sidebarClosedCount", 4],
      ],
    });
    await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async _browser => {
      await SidebarController.show("viewReviewCheckerSidebar");
      info("Waiting for sidebar to update.");
      await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

      // Now, check content again to dismiss the notification and verify "Keep closed"
      await testDismissNotificationThenCloseRC();

      Assert.ok(SidebarController.isOpen, "Sidebar is still open");

      let hasSeen = Services.prefs.getBoolPref(HAS_SEEN_PREF);
      Assert.ok(
        hasSeen,
        "browser.shopping.experience2023.newPositionCard.hasSeen is now true"
      );

      if (!SidebarController.isOpen) {
        Assert.ok(
          false,
          "Asserting false. Cannot test 'Keep closed' message if sidebar is closed"
        );
      } else {
        Assert.ok(true, "Sidebar is not closed yet");
        await testKeepClosedIsVisible();
      }
    });

    SidebarController.hide();
    await SpecialPowers.popPrefEnv();
  }
);

add_task(async function test_keep_closed_message_not_visible_non_pdp() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [HAS_SEEN_PREF, true],
      [SHOW_KEEP_MESSAGE_PREF, true],
      // Set to minimum closed counts met, to speed up testing
      ["browser.shopping.experience2023.sidebarClosedCount", 4],
    ],
  });
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async _browser => {
    await SidebarController.show("viewReviewCheckerSidebar");
    info("Waiting for sidebar to update.");
    await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

    await TestUtils.waitForTick();

    Assert.ok(SidebarController.isOpen, "Sidebar is open now");

    await withReviewCheckerSidebar(async _args => {
      let shoppingContainer = await ContentTaskUtils.waitForCondition(
        () =>
          content.document.querySelector("shopping-container")?.wrappedJSObject,
        "Review Checker is loaded."
      );

      await shoppingContainer.updateComplete;
      info("Shopping container update complete");

      shoppingContainer.closeButtonEl.click();

      await ContentTaskUtils.waitForCondition(
        () => shoppingContainer.keepClosedMessageBarEl,
        "Keep closed message is visible"
      );
    });

    let nonPDPTab = BrowserTestUtils.addTab(gBrowser, NON_PDP_PAGE);
    let nonPDPBrowser = nonPDPTab.linkedBrowser;
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      nonPDPBrowser,
      false,
      NON_PDP_PAGE
    );
    await browserLoadedPromise;

    info("Switching tabs now");
    await BrowserTestUtils.switchTab(gBrowser, nonPDPTab);

    Assert.ok(true, "Browser is loaded");
    await SidebarController.show("viewReviewCheckerSidebar");

    await withReviewCheckerSidebar(
      async showKeepClosedMessagePref => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () =>
            content.document.querySelector("shopping-container")
              ?.wrappedJSObject,
          "Review Checker is loaded."
        );

        await shoppingContainer.updateComplete;

        Assert.ok(
          !shoppingContainer.keepClosedMessageBarEl,
          "'Keep closed' message is not visible before close button click"
        );

        shoppingContainer.closeButtonEl.click();

        Assert.ok(
          !shoppingContainer.keepClosedMessageBarEl,
          "'Keep closed' message is not visible after close button click"
        );

        let showKeepSidebarClosedMessage = Services.prefs.getBoolPref(
          showKeepClosedMessagePref
        );

        Assert.ok(
          showKeepSidebarClosedMessage,
          "browser.shopping.experience2023.showKeepSidebarClosedMessage is true"
        );
      },
      [SHOW_KEEP_MESSAGE_PREF]
    );

    await BrowserTestUtils.removeTab(nonPDPTab);
  });
  SidebarController.hide();
  await SpecialPowers.popPrefEnv();
});
