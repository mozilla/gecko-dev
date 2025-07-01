/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  actionTypes: "resource://newtab/common/Actions.mjs",
  SmartShortcutsFeed: "resource://newtab/lib/SmartShortcutsFeed.sys.mjs",
});

const PREF_SYSTEM_SHORTCUTS_PERSONALIZATION =
  "discoverystream.shortcuts.personalization.enabled";

add_task(async function test_construction() {
  let feed = new SmartShortcutsFeed();

  feed.store = {
    getState() {
      return this.state;
    },
    state: {
      Prefs: {
        values: {
          [PREF_SYSTEM_SHORTCUTS_PERSONALIZATION]: false,
        },
      },
    },
  };

  info("SmartShortcutsFeed constructor should create initial values");

  Assert.ok(feed, "Could construct a SmartShortcutsFeed");
  Assert.ok(!feed.loaded, "SmartShortcutsFeed is not loaded");
  Assert.ok(!feed.isEnabled());
});

add_task(async function test_onAction_INIT() {
  let feed = new SmartShortcutsFeed();

  feed.store = {
    getState() {
      return this.state;
    },
    state: {
      Prefs: {
        values: {
          [PREF_SYSTEM_SHORTCUTS_PERSONALIZATION]: true,
        },
      },
    },
  };

  info("SmartShortcutsFeed.onAction INIT should set loaded");

  await feed.onAction({
    type: actionTypes.INIT,
  });

  Assert.ok(feed.loaded);
});

add_task(async function test_isEnabled() {
  let feed = new SmartShortcutsFeed();

  feed.store = {
    getState() {
      return this.state;
    },
    state: {
      Prefs: {
        values: {
          [PREF_SYSTEM_SHORTCUTS_PERSONALIZATION]: true,
        },
      },
    },
  };

  info("SmartShortcutsFeed should be enabled");
  Assert.ok(feed.isEnabled());
});
