/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_setup(async function () {
  registerCleanupFunction(async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
        resolve()
      );
    });
  });
});

AntiTracking._createTask({
  name: "Block tracker cookies with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when tracker cookie blocking is enabled",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    document.cookie = "name=value; SameSite=None; Secure; Partitioned";
    is(document.cookie, "", "Document cookie is blocked");
    await fetch("server.sjs?partitioned")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-not-present", "We should not have HTTP cookies");
      });
    await fetch("server.sjs?checkonly")
      .then(r => r.text())
      .then(text => {
        is(
          text,
          "cookie-not-present",
          "We should still not have HTTP cookies after setting them via HTTP"
        );
      });
    is(
      document.cookie,
      "",
      "Document cookie is still blocked after setting via HTTP"
    );
  },
  extraPrefs: [["network.cookie.cookieBehavior.trackerCookieBlocking", true]],
  expectedBlockingNotifications: [
    Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
  ],
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

AntiTracking._createTask({
  name: "Block tracker storage with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when tracker cookie blocking is enabled",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    try {
      localStorage.foo = 42;
      ok(false, "LocalStorage cannot be used!");
    } catch (e) {
      ok(true, "LocalStorage cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  extraPrefs: [["network.cookie.cookieBehavior.trackerCookieBlocking", true]],
  expectedBlockingNotifications: [
    Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
  ],
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

AntiTracking._createTask({
  name: "Block tracker indexedDB with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when tracker cookie blocking is enabled",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    try {
      indexedDB.open("test", "1");
      ok(false, "IDB should be blocked");
    } catch (e) {
      ok(true, "IDB should be blocked");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
  extraPrefs: [["network.cookie.cookieBehavior.trackerCookieBlocking", true]],
  expectedBlockingNotifications: [
    Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
  ],
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

AntiTracking._createTask({
  name: "Allow tracker cookies with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when tracker cookie blocking is disabled",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    await fetch("server.sjs?partitioned")
      .then(r => r.text())
      .then(text => {
        is(
          text,
          "cookie-not-present",
          "We should not have seen HTTP cookie at beginning"
        );
      });

    ok(document.cookie.includes("foopy=1"), "Some cookies for me from http");

    await fetch("server.sjs?checkonly")
      .then(r => r.text())
      .then(text => {
        is(
          text,
          "cookie-present",
          "We should have HTTP cookies after setting them via HTTP"
        );
      });

    document.cookie = "name=value; SameSite=None; Secure; Partitioned";
    ok(document.cookie.includes("name=value"), "Some cookies for me");
  },
  extraPrefs: [["network.cookie.cookieBehavior.trackerCookieBlocking", false]],
  expectedPartitioningNotifications: [
    Ci.nsIWebProgressListener.STATE_COOKIES_PARTITIONED_TRACKER,
  ],
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

AntiTracking._createTask({
  name: "Allow tracker storage with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when tracker cookie blocking is disabled",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    localStorage.foo = 42;
    ok(true, "LocalStorage is allowed");
  },
  extraPrefs: [["network.cookie.cookieBehavior.trackerCookieBlocking", false]],
  expectedPartitioningNotifications: [
    Ci.nsIWebProgressListener.STATE_COOKIES_PARTITIONED_TRACKER,
  ],
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

AntiTracking._createTask({
  name: "Allow tracker indexedDB with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when tracker cookie blocking is disabled",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    indexedDB.open("test", "1");
    ok(true, "IDB should be allowed");
  },
  extraPrefs: [["network.cookie.cookieBehavior.trackerCookieBlocking", false]],
  expectedPartitioningNotifications: [
    Ci.nsIWebProgressListener.STATE_COOKIES_PARTITIONED_TRACKER,
  ],
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});
