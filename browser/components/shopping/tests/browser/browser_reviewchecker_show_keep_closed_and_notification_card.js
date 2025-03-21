/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

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

    Assert.ok(
      !shoppingContainer.showingKeepClosedMessage,
      "showingKeepClosedMessage is false"
    );
    Assert.ok(
      !shoppingContainer.keepClosedMessageBarEl,
      "'Keep closed' message is not visible"
    );
  });
}

async function testDismissNotificationThenCheckKeepClosed() {
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

async function testKeepClosedAfterNotificationDismissed() {
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
 * Tests that the 'Keep closed' message does not appear when the notification card
 * is set to be displayed. Only show the message once the notification card is dismissed.
 */
add_task(
  async function test_do_not_show_keep_closed_until_notification_dismissed() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.shopping.experience2023.newPositionCard.hasSeen", false],
        ["sidebar.position_start", true],
        ["browser.shopping.experience2023.showKeepSidebarClosedMessage", true],
        // Set to minimum closed counts met, to speed up testing
        ["browser.shopping.experience2023.sidebarClosedCount", 4],
      ],
    });
    await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async _browser => {
      await SidebarController.show("viewReviewCheckerSidebar");
      info("Waiting for sidebar to update.");
      await reviewCheckerSidebarUpdated(PRODUCT_TEST_URL);

      await testNotificationCardThenCloseRC();

      Assert.ok(
        !SidebarController.isOpen,
        "Sidebar is closed without a problem"
      );

      let hasSeen = Services.prefs.getBoolPref(
        "browser.shopping.experience2023.newPositionCard.hasSeen"
      );
      Assert.ok(
        !hasSeen,
        "browser.shopping.experience2023.newPositionCard.hasSeen is still false"
      );

      // Let's open another tab, to force auto open and show the notification card again
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

      let shownPromise = BrowserTestUtils.waitForEvent(window, "SidebarShown");

      info("Switching tabs now");
      await BrowserTestUtils.switchTab(gBrowser, newProductTab);

      Assert.ok(true, "Browser is loaded");

      info("Waiting for shown");
      await shownPromise;
      await TestUtils.waitForTick();

      Assert.ok(SidebarController.isOpen, "Sidebar is open now");

      // Now, check content again to dismiss the notification and verify "Keep closed"
      await testDismissNotificationThenCheckKeepClosed();

      Assert.ok(SidebarController.isOpen, "Sidebar is still open");

      hasSeen = Services.prefs.getBoolPref(
        "browser.shopping.experience2023.newPositionCard.hasSeen"
      );
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
        await testKeepClosedAfterNotificationDismissed();
      }

      await BrowserTestUtils.removeTab(newProductTab);
    });

    SidebarController.hide();
    await SpecialPowers.popPrefEnv();
  }
);
