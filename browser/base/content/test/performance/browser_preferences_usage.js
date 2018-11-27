/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const DEFAULT_PROCESS_COUNT = Services.prefs.getDefaultBranch(null).getIntPref("dom.ipc.processCount");

/**
 * A test that checks whether any preference getter from the given list
 * of stats was called more often than the max parameter.
 *
 * @param {Array}  stats - an array of [prefName, accessCount] tuples
 * @param {Number} max - the maximum number of times any of the prefs should
 *                 have been called.
 * @param {Object} whitelist (optional) - an object that defines
 *                 prefs that should be exempt from checking the
 *                 maximum access. It looks like the following:
 *
 *                 pref_name: {
 *                   min: [Number] the minimum amount of times this should have
 *                                 been called (to avoid keeping around dead items)
 *                   max: [Number] the maximum amount of times this should have
 *                                 been called (to avoid this creeping up further)
 *                 }
 */
function checkPrefGetters(stats, max, whitelist = {}) {
  let getterStats = Object
    .entries(stats)
    .sort(([, val1], [, val2]) => val2 - val1);

  // Clone the whitelist to be able to delete entries to check if we
  // forgot any later on.
  whitelist = Object.assign({}, whitelist);

  for (let [pref, count] of getterStats) {
    let whitelistItem = whitelist[pref];
    if (!whitelistItem) {
      Assert.lessOrEqual(count, max, `${pref} should not be accessed more than ${max} times.`);
    } else {
      // Still record how much this pref was accessed even if we don't do any real assertions.
      if (!whitelistItem.min && !whitelistItem.max) {
        info(`${pref} should not be accessed more than ${max} times and was accessed ${count} times.`);
      }

      if (whitelistItem.min) {
        Assert.lessOrEqual(whitelistItem.min, count,
          `Whitelist item ${pref} should be accessed at least ${whitelistItem.min} times.`);
      }
      if (whitelistItem.max) {
        Assert.lessOrEqual(count, whitelistItem.max,
          `Whitelist item ${pref} should be accessed at most ${whitelistItem.max} times.`);
      }
      delete whitelist[pref];
    }
  }

  // This pref will be accessed by mozJSComponentLoader when loading modules,
  // which fails TV runs since they run the test multiple times without restarting.
  // We just ignore this pref, since it's for testing only anyway.
  if (whitelist["browser.startup.record"]) {
    delete whitelist["browser.startup.record"];
  }

  let remainingWhitelist = Object.keys(whitelist);
  is(remainingWhitelist.length, 0, `Should have checked all whitelist items. Remaining: ${remainingWhitelist}`);
}

/**
 * A helper function to read preference access data
 * using the Services.prefs.readStats() function.
 */
function getPreferenceStats() {
  let stats = {};
  Services.prefs.readStats((key, value) => stats[key] = value);
  return stats;
}

add_task(async function debug_only() {
  ok(AppConstants.DEBUG, "You need to run this test on a debug build.");
});

// Just checks how many prefs were accessed during startup.
add_task(async function startup() {
  let max = 40;

  let whitelist = {
    "browser.startup.record": {
      min: 200,
      max: 350,
    },
    "layout.css.prefixes.webkit": {
      min: 135,
      max: 170,
    },
    "layout.css.dpi": {
      min: 45,
      max: 81,
    },
    "network.loadinfo.skip_type_assertion": {
      // This is accessed in debug only.
    },
    "extensions.getAddons.cache.enabled": {
      min: 4,
      max: 55,
    },
    "chrome.override_package.global": {
      min: 0,
      max: 50,
    },
  };

  let startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService().wrappedJSObject;
  await startupRecorder.done;

  ok(startupRecorder.data.prefStats, "startupRecorder has prefStats");

  checkPrefGetters(startupRecorder.data.prefStats, max, whitelist);
});

// This opens 10 tabs and checks pref getters.
add_task(async function open_10_tabs() {
  // This is somewhat arbitrary. When we had a default of 4 content processes
  // the value was 15. We need to scale it as we increase the number of
  // content processes so we approximate with 4 * process_count.
  const max = 4 * DEFAULT_PROCESS_COUNT;

  let whitelist = {
    "layout.css.dpi": {
      max: 35,
    },
    "browser.zoom.full": {
      min: 10,
      max: 25,
    },
    "browser.startup.record": {
      max: 20,
    },
    "browser.tabs.remote.logSwitchTiming": {
      max: 25,
    },
    "network.loadinfo.skip_type_assertion": {
      // This is accessed in debug only.
    },
    "toolkit.cosmeticAnimations.enabled": {
      min: 5,
      max: 20,
    },
  };

  Services.prefs.resetStats();

  let tabs = [];
  while (tabs.length < 10) {
    tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com", true, true));
  }

  for (let tab of tabs) {
    await BrowserTestUtils.removeTab(tab);
  }

  checkPrefGetters(getPreferenceStats(), max, whitelist);
});

// This navigates to 50 sites and checks pref getters.
add_task(async function navigate_around() {
  let max = 40;

  let whitelist = {
    "browser.zoom.full": {
      min: 100,
      max: 110,
    },
    "network.loadinfo.skip_type_assertion": {
      // This is accessed in debug only.
    },
    "toolkit.cosmeticAnimations.enabled": {
      min: 45,
      max: 55,
    },
  };

  Services.prefs.resetStats();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com", true, true);

  let urls = ["http://example.com", "https://example.com", "http://example.org", "https://example.org"];

  for (let i = 0; i < 50; i++) {
    let url = urls[i % urls.length];
    info(`Navigating to ${url}...`);
    await BrowserTestUtils.loadURI(tab.linkedBrowser, url);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    info(`Loaded ${url}.`);
  }

  await BrowserTestUtils.removeTab(tab);

  checkPrefGetters(getPreferenceStats(), max, whitelist);
});
