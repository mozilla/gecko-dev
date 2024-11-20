/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1918341 - Testing the cookie category of the protection panel shows the
 *               tracker domains who access cookies when the tracker cookie
 *               blocking is disabled.
 */

const TEST_PAGE =
  "https://example.net/browser/browser/base/content/test/protectionsUI/trackerCookiePage.html";

add_setup(async function () {
  await UrlClassifierTestUtils.addTestTrackers();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["network.cookie.cookieBehavior.trackerCookieBlocking", false],
    ],
  });

  registerCleanupFunction(() => {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

add_task(async function testTrackerCookiesSubView() {
  // Open a tab which embeds a third-party tracker iframe which sets a cookie.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: TEST_PAGE,
    gBrowser,
  });

  await openProtectionsPanel();

  let categoryItem = document.getElementById(
    "protections-popup-category-cookies"
  );

  // Explicitly waiting for the category item becoming visible.
  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.isVisible(categoryItem);
  });

  ok(
    BrowserTestUtils.isVisible(categoryItem),
    "Cookie category item is visible"
  );
  let cookiesView = document.getElementById("protections-popup-cookiesView");
  let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  let listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, `We have 1 item in the list`);

  let label = listItems[0].querySelector(".protections-popup-list-host-label");
  is(
    label.value,
    "https://itisatracker.org",
    "Has an item for itisatracker.org"
  );

  await closeProtectionsPanel();

  // Fetch a non-tracker third party cookie. Ensure it's not shown in the
  // cookie subview.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    await content.fetch(
      "https://example.com/browser/browser/base/content/test/protectionsUI/cookieServer.sjs?type=partitioned",
      { credentials: "include" }
    );
  });

  await openProtectionsPanel();

  viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, `We still have 1 item in the list`);

  await closeProtectionsPanel();

  // Fetch a third party cookie from another tracker. Ensure it's shown in the
  // cookie subview.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    await content.fetch(
      "https://tracking.example.org/browser/browser/base/content/test/protectionsUI/cookieServer.sjs?type=partitioned",
      { credentials: "include" }
    );
  });

  await openProtectionsPanel();

  viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 2, `We have 2 items in the list`);

  label = listItems[0].querySelector(".protections-popup-list-host-label");
  is(
    label.value,
    "https://itisatracker.org",
    "Has an item for itisatracker.org"
  );

  label = listItems[1].querySelector(".protections-popup-list-host-label");
  is(
    label.value,
    "https://tracking.example.org",
    "Has an item for tracking.example.org"
  );

  await closeProtectionsPanel();

  BrowserTestUtils.removeTab(tab);
});
