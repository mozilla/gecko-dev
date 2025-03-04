/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1943536 - Tests that verify third-party trackers cannot trigger the
 *  opener and openerWithUserInteraction heuristics.
 */

const TEST_TRACKER_DOMAIN = "https://itisatracker.org/";
const TEST_TRACK_TOP_PAGE = TEST_TRACKER_DOMAIN + TEST_PATH + "page.html";

async function cleanup() {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      resolve()
    );
  });
}

/**
 * A helper function to check the storage access permission.
 *
 * @param {string} top - The top level domain.
 * @param {string} third - The third party domain.
 * @param {boolean} set - Whether the storage access should be set.
 */
async function testStorageAccessPermission(top, third, set) {
  let expected = set
    ? Services.perms.ALLOW_ACTION
    : Services.perms.UNKNOWN_ACTION;

  let result = await SpecialPowers.testPermission(
    `3rdPartyStorage^${third}`,
    expected,
    top
  );

  ok(result, `The storage access ${set ? "should" : "shouldn't"} be set`);
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(async _ => {
    await cleanup();
    await UrlClassifierTestUtils.cleanupTestTrackers();
  });

  await cleanup();
});

add_task(async function test_opener_heuristic_exclude_trackers() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.restrict3rdpartystorage.heuristic.window_open", true],
      [
        "privacy.restrict3rdpartystorage.heuristic.exclude_third_party_trackers",
        true,
      ],
    ],
  });

  info("Creating a new tab.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  info("Creating a third-party tracking iframe in the tab.");
  let trackingBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_TRACK_TOP_PAGE],
    async trackingPage => {
      let ifr = content.document.createElement("iframe");
      ifr.src = trackingPage;

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
      });

      return ifr.browsingContext;
    }
  );

  info("Call window.open() in the third-party tracking iframe.");
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  await SpecialPowers.spawn(trackingBC, [TEST_TOP_PAGE_7], async domain => {
    content.open(domain);
  });

  let openedTab = await tabOpenPromise;
  BrowserTestUtils.removeTab(openedTab);

  info(
    "Verify that the opener heuristic is not triggered when a third-party tracking context opens a window."
  );
  await testStorageAccessPermission(
    TEST_TOP_PAGE,
    TEST_TRACKER_DOMAIN.slice(0, -1),
    false
  );

  info(
    "Call window.open() in the first-party context to open a third-party tracking URL."
  );
  tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_TRACK_TOP_PAGE],
    async domain => {
      content.open(domain);
    }
  );

  openedTab = await tabOpenPromise;
  BrowserTestUtils.removeTab(openedTab);

  info(
    "Verify that the opener heuristic is not triggered when opening a third-party tracking URL."
  );
  await testStorageAccessPermission(
    TEST_TOP_PAGE,
    TEST_TRACKER_DOMAIN.slice(0, -1),
    false
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_opener_heuristic_first_party_trackers() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.restrict3rdpartystorage.heuristic.window_open", true],
      [
        "privacy.restrict3rdpartystorage.heuristic.exclude_third_party_trackers",
        true,
      ],
    ],
  });

  info("Creating a new tab with a first-party tracker.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TRACK_TOP_PAGE
  );

  info("Call window.open() in the first-party context to open a URL.");
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_TOP_PAGE],
    async domain => {
      content.open(domain);
    }
  );

  let openedTab = await tabOpenPromise;
  BrowserTestUtils.removeTab(openedTab);

  info(
    "Verify that a first-party tracker is allowed to trigger opener heuristic."
  );
  await testStorageAccessPermission(
    TEST_TRACKER_DOMAIN,
    TEST_DOMAIN.slice(0, -1),
    true
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(
  async function test_opener_user_interaction_heuristic_exclude_trackers() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.restrict3rdpartystorage.heuristic.window_open", false],
        [
          "privacy.restrict3rdpartystorage.heuristic.opened_window_after_interaction",
          true,
        ],
        [
          "privacy.restrict3rdpartystorage.heuristic.exclude_third_party_trackers",
          true,
        ],
      ],
    });

    info("Creating a new tab.");
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_TOP_PAGE
    );

    info(
      "Call window.open() in the first-party context to open a third-party tracking URL."
    );
    let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser);

    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [TEST_TRACK_TOP_PAGE],
      async domain => {
        content.open(domain);
      }
    );

    let openedTab = await tabOpenPromise;

    info("Trigger the user interaction in the tracking popup.");
    await SpecialPowers.spawn(openedTab.linkedBrowser, [], () => {
      content.document.userInteractionForTesting();
    });

    info("Close the tracking popup.");
    BrowserTestUtils.removeTab(openedTab);

    info("Verify that the user interaction heuristic is not triggered.");
    await testStorageAccessPermission(
      TEST_TOP_PAGE,
      TEST_TRACKER_DOMAIN.slice(0, -1),
      false
    );

    info("Removing the tab");
    BrowserTestUtils.removeTab(tab);
  }
);
