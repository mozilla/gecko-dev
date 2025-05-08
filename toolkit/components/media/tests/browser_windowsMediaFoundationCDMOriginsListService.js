/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global add_task */
"use strict";

/**
 * This test verifies that nsIWindowsMediaFoundationCDMOriginsListService
 * correctly triggers its callback and that the origin entries are propagated
 * as expected. It also checks that the result of calling
 * requestMediaKeySystemAccess for PlayReady is influenced by these entries.
 */

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

const COLLECTION_NAME = "mfcdm-origins-list";
const gRemoteSettings = RemoteSettings(COLLECTION_NAME);
const gOriginsListService = Cc[
  "@mozilla.org/media/wmfcdm-origins-list;1"
].getService(Ci.nsIWindowsMediaFoundationCDMOriginsListService);
const gEntries = [
  {
    origin: "example.com",
    allowed: false,
  },
  {
    origin: "example.org",
    allowed: true,
  },
];

add_task(async function testOriginsListServiceCallback() {
  info(`Simulate sending initial entries update to RemoteSettings`);
  await updateRemoteSettings(gEntries);

  info(`Waiting for the callback to be triggered`);
  let waitUpdate = TestUtils.topicObserved("updated-entries");
  let callback = {
    onOriginsListLoaded(entries) {
      Assert.equal(entries.length, gEntries.length, "Entry count matches");
      for (let expected of gEntries) {
        let found = false;
        for (let i = 0; i < entries.length; i++) {
          let e = entries.queryElementAt(i, Ci.nsIOriginStatusEntry);
          if (
            e.origin === expected.origin &&
            e.status === (expected.allowed ? 1 : 0)
          ) {
            found = true;
            break;
          }
        }
        Assert.ok(
          found,
          `Expected origin (${expected.origin}/${expected.allowed}) found`
        );
      }
      Services.obs.notifyObservers(null, "updated-entries");
    },
  };
  gOriginsListService.setCallback(callback);
  await waitUpdate;

  info(
    `Simulate adding another new record which should trigger callback again`
  );
  waitUpdate = TestUtils.topicObserved("updated-entries");
  gEntries.push({
    origin: "example.net",
    allowed: false,
  });
  await Promise.all([
    TestUtils.topicObserved("updated-entries"),
    updateRemoteSettings(gEntries),
  ]);

  info(
    `Remove old callback, and set a new callback, which should be triggered as well`
  );
  waitUpdate = TestUtils.topicObserved("updated-entries");
  gOriginsListService.removeCallback(callback);
  gOriginsListService.setCallback(callback);
  await waitUpdate;
});

// This task follows the entries set in the previous task.
add_task(async function testCheckCreateKeySystemAccess() {
  info(`Set pre-requirement prefs`);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.wmf.media-engine.enabled", 2],
      // Our mock CDM doesn't implement 'IsTypeSupportedEx()', only 'IsTypeSupported()'
      ["media.eme.playready.istypesupportedex", false],
      ["media.eme.wmf.use-mock-cdm-for-external-cdms", true],
    ],
  });

  info(`Open a new tab and navigate`);
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser);

  // Test both preference values to control the default behavior when an origin
  // is not explicitly listed in the entries.
  // 3 : enabled allowed by default via Remote Settings
  // 4 : enabled blocked by default via Remote Settings
  const originFilterPrefs = [3, 4];
  for (let prefValue of originFilterPrefs) {
    const isDefaultAllowed = prefValue == 3;
    info(`Testing default behavior: ${isDefaultAllowed ? "allow" : "block"}`);
    await SpecialPowers.pushPrefEnv({
      set: [["media.eme.mfcdm.origin-filter.enabled", prefValue]],
    });

    const notSpecifiedDomains = [
      // Sub-domain, should follow the main domain result
      {
        origin: "test1.example.com",
        allowed: false,
      },
      {
        origin: "test1.example.org",
        allowed: true,
      },
      // Not specified and not a subdomain, follow the default result
      {
        origin: "example.onion",
        allowed: isDefaultAllowed,
      },
    ];

    for (let entry of gEntries.concat(notSpecifiedDomains)) {
      info(`Navigate to ${entry.origin}`);
      BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, entry.origin);
      await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

      info(`Creating a key system access, which should be ${entry.allowed}`);
      await checkCreateKeySystemAccess({
        tab,
        isAllowed: entry.allowed,
      });
    }
  }

  info(`Remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

// Below are helper functions
function updateRemoteSettings(entries) {
  return gRemoteSettings.emit("sync", {
    data: {
      current: entries,
      created: [],
      updated: [],
      deleted: [],
    },
  });
}

function checkCreateKeySystemAccess({ tab, isAllowed } = {}) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [isAllowed],
    async isAllowed_ => {
      const keySystem = "com.microsoft.playready.recommendation";
      const config = [
        {
          videoCapabilities: [
            {
              contentType: 'video/mp4; codecs="avc1.42E01E"',
            },
          ],
          initDataTypes: ["cenc"],
          sessionTypes: ["temporary"],
        },
      ];
      try {
        info("Attemping to create a key system access");
        await content.navigator.requestMediaKeySystemAccess(keySystem, config);
        ok(isAllowed_, "Created a key system access");
      } catch (e) {
        ok(!isAllowed_, "Not allowed to create a key system access");
      }
    }
  );
}
