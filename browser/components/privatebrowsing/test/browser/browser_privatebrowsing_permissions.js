/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that permissions don't persist across private browsing sessions by
// opening a new pbm window, gaining a notification permission, reopening the
// pbm window, and then checking the notification permission.

const TEST_URL = "https://example.com";
const TEST_PRINCIPAL = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(TEST_URL),
  { privateBrowsingId: 1 }
);

async function checkNotificationPermission(tab, isAllowExpected) {
  // Check permission manager
  let permission = Services.perms.testExactPermissionFromPrincipal(
    TEST_PRINCIPAL,
    "desktop-notification"
  );
  is(
    permission,
    isAllowExpected
      ? Services.perms.ALLOW_ACTION
      : Services.perms.UNKNOWN_ACTION,
    `Permission ${isAllowExpected ? "should" : "should not"} exist in permission manager`
  );

  // Check Notification API directly
  await SpecialPowers.spawn(tab, [isAllowExpected], _isAllowExpected => {
    is(
      content.Notification.permission,
      _isAllowExpected ? "granted" : "default",
      `The notification API ${_isAllowExpected ? "should" : "should not"} allow notification`
    );
  });
}

add_task(async () => {
  // Explicit user interaction would make this test more complicated than
  // nescessary
  await SpecialPowers.pushPrefEnv({
    set: [["dom.webnotifications.requireuserinteraction", false]],
  });

  // Create a private browsing window
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let privateTab = privateWindow.gBrowser.selectedBrowser;

  info("Checking permissions before test");
  await checkNotificationPermission(privateTab, false);

  // Open test site
  BrowserTestUtils.startLoadingURIString(privateTab, TEST_URL);
  await BrowserTestUtils.browserLoaded(privateTab);

  // Gain "notification" permission on test site in private tab
  let popupShown = BrowserTestUtils.waitForEvent(
    privateWindow.PopupNotifications.panel,
    "popupshown"
  );
  await SpecialPowers.spawn(privateTab, [], () => {
    content.Notification.requestPermission();
  });
  await popupShown;

  let notification = privateWindow.PopupNotifications.panel.firstElementChild;
  let popupHidden = BrowserTestUtils.waitForEvent(
    privateWindow.PopupNotifications.panel,
    "popuphidden"
  );
  notification.button.click();
  await popupHidden;

  info("Checking permissions after permission was granted");
  await checkNotificationPermission(privateTab, true);

  // Close private window
  await BrowserTestUtils.closeWindow(privateWindow);

  // Open new private window
  privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  privateTab = privateWindow.gBrowser.selectedBrowser;

  // Open example site again
  BrowserTestUtils.startLoadingURIString(privateTab, TEST_URL);
  await BrowserTestUtils.browserLoaded(privateTab);

  info("Checking permissions after pbm window got reopened");
  await checkNotificationPermission(privateTab, false);

  // Close private window again
  await BrowserTestUtils.closeWindow(privateWindow);
});
