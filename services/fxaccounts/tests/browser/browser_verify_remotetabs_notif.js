/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Observers } = ChromeUtils.importESModule(
  "resource://services-common/observers.sys.mjs"
);

// Since we show the brand name (Nightly, Beta) in the notification
// we need to fetch the localization so the test doesn't break when going
// through the trains
const l10n = new Localization(["branding/brand.ftl", "browser/accounts.ftl"]);

// URL that opens up when a user clicks the "close tab" notification
const NOTIFICATION_CLICKED_URL = "about:firefoxview#recentlyclosed";

add_task(async function test_closetab_notification() {
  const URL_TO_CLOSE = "about:mozilla";
  let payload = [
    {
      urls: [URL_TO_CLOSE],
      sender: {
        deviceName: "device-1",
      },
    },
  ];
  info("Test verify receiving a close tab command will show a notification");

  // Get the expected notification text we'll show the user
  const [expectedTitle, expectedBody] = await l10n.formatValues([
    {
      id: "account-tabs-closed-remotely",
      args: { closedCount: 1 },
    },
    { id: "account-view-recently-closed-tabs" },
  ]);

  // This will also immediately invoke the "alertclickcallback" in addition to
  // the usual alertshow and alertfinished
  setupMockAlertsService({
    title: expectedTitle,
    body: expectedBody,
  });

  // Open a tab with the same url we'll be expecting from the close tab command payload
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL_TO_CLOSE);
  let tabClosedPromise = BrowserTestUtils.waitForTabClosing(tab);
  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  // Send the notify, which will kick off the closing of the tab and notification
  Observers.notify("fxaccounts:commands:close-uri", payload);
  Assert.ok("Close notification sent");

  // Wait for the tab to close
  await tabClosedPromise;
  Assert.ok("Tab successfully closed");

  // Test the tab that opened for "clicking" the notification shows
  // the recently closed list
  let notifTab = await waitForTabPromise;
  Assert.equal(
    notifTab.linkedBrowser.currentURI.spec,
    NOTIFICATION_CLICKED_URL
  );

  // Cleanup the tab
  BrowserTestUtils.removeTab(notifTab);
});

add_task(async function test_closetab_multiple_urls_notification() {
  const URLS_TO_CLOSE = ["about:mozilla", "about:about"];
  let payload = [
    {
      urls: URLS_TO_CLOSE,
      sender: {
        deviceName: "device-1",
      },
    },
  ];
  info(
    "Test verify receiving multiple close tabs command will show the proper notification"
  );

  // Get the expected notification text we'll show the user
  const [expectedTitle, expectedBody] = await l10n.formatValues([
    {
      id: "account-tabs-closed-remotely",
      args: { closedCount: 2 },
    },
    { id: "account-view-recently-closed-tabs" },
  ]);

  // This will also immediately invoke the "alertclickcallback" in addition to
  // the usual alertshow and alertfinished
  setupMockAlertsService({
    title: expectedTitle,
    body: expectedBody,
  });
  // Open multiple tabs to test we can close both and have the correct
  // notification text
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    URLS_TO_CLOSE[0]
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    URLS_TO_CLOSE[1]
  );
  // We want to make sure multiple tabs were closed
  let tabClosedPromise = Promise.all([
    BrowserTestUtils.waitForTabClosing(tab1),
    BrowserTestUtils.waitForTabClosing(tab2),
  ]);
  let waitForTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  // Send the notify, which will kick off the closing of the tab and notification
  Observers.notify("fxaccounts:commands:close-uri", payload);
  Assert.ok("Close notification sent");

  await tabClosedPromise;
  Assert.ok("Multiple tabs successfully closed");

  // Test the click after the notification
  let notifTab = await waitForTabPromise;
  Assert.equal(
    notifTab.linkedBrowser.currentURI.spec,
    NOTIFICATION_CLICKED_URL
  );

  // Cleanup the tab
  BrowserTestUtils.removeTab(notifTab);
});
