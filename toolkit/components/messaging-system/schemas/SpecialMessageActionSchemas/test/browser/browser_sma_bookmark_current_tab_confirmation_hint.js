/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_BOOKMARK_CURRENT_TAB_WITH_CONFIRMATION_HINT() {
  await SpecialPowers.pushPrefEnv({
    clear: [["browser.bookmarks.editDialog.confirmationHintShowCount"]],
  });

  const action = {
    type: "BOOKMARK_CURRENT_TAB",
    data: { shouldHideDialog: true, shouldHideConfirmationHint: false },
  };
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let confirmationPopupShown = BrowserTestUtils.waitForPopupEvent(
    ConfirmationHint._panel,
    "shown"
  );
  let confirmationPopupHidden = BrowserTestUtils.waitForPopupEvent(
    ConfirmationHint._panel,
    "hidden"
  );
  await SMATestUtils.validateAction(action);
  await SpecialMessageActions.handleAction(action, gBrowser);

  ConfirmationHint._panel.hidePopup();
  ConfirmationHint._reset();
  await confirmationPopupHidden;
  await confirmationPopupShown;

  Assert.ok(confirmationPopupShown, "Bookmark confirmation hint shown");

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.bookmarks.eraseEverything();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_BOOKMARK_CURRENT_TAB_WITHOUT_CONFIRMATION_HINT() {
  const action = {
    type: "BOOKMARK_CURRENT_TAB",
    data: { shouldHideDialog: true, shouldHideConfirmationHint: true },
  };
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  const showConfirmationHintSpy = sandbox.spy(StarUI, "showConfirmation");
  const bookmarkCurrentTabSpy = sandbox.spy(
    SpecialMessageActions,
    "bookmarkCurrentTab"
  );

  await SMATestUtils.validateAction(action);
  await SpecialMessageActions.handleAction(action, gBrowser);

  await BrowserTestUtils.waitForCondition(
    () => bookmarkCurrentTabSpy.calledOnce,
    "Expected 'bookmarkCurrentTab' to be called once"
  );

  // Since we know that the confirmation hint should be shown as part of the bookmark current tab action,
  // we know that showConfirmation() was not called by this point
  await BrowserTestUtils.waitForCondition(
    () => showConfirmationHintSpy.notCalled,
    "Expected 'showConfirmation' to not be called"
  );

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.bookmarks.eraseEverything();
});
