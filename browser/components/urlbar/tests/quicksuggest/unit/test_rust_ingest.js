/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests ingest in the Rust backend.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  InterruptKind: "resource://gre/modules/RustSuggest.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

// These consts are copied from the update timer manager test. See
// `initUpdateTimerManager()`.
const PREF_APP_UPDATE_TIMERMINIMUMDELAY = "app.update.timerMinimumDelay";
const PREF_APP_UPDATE_TIMERFIRSTINTERVAL = "app.update.timerFirstInterval";
const MAIN_TIMER_INTERVAL = 1000; // milliseconds
const CATEGORY_UPDATE_TIMER = "update-timer";

const REMOTE_SETTINGS_SUGGESTION = QuickSuggestTestUtils.ampRemoteSettings();

add_setup(async function () {
  initUpdateTimerManager();

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: [REMOTE_SETTINGS_SUGGESTION],
      },
    ],
    prefs: [["suggest.quicksuggest.sponsored", true]],
  });
});

// The backend should ingest when it's disabled and then re-enabled.
add_task(async function disableEnable() {
  Assert.strictEqual(
    UrlbarPrefs.get("quicksuggest.rustEnabled"),
    true,
    "Sanity check: Rust pref is initially true"
  );
  Assert.strictEqual(
    QuickSuggest.rustBackend.isEnabled,
    true,
    "Sanity check: Rust backend is initially enabled"
  );

  // Disable and re-enable the backend. An ingest should start immediately
  // since ingest is done every time the backend is re-enabled.
  UrlbarPrefs.set("quicksuggest.rustEnabled", false);
  UrlbarPrefs.set("quicksuggest.rustEnabled", true);
  let { ingestPromise } = await waitForIngestStart(null);

  info("Awaiting ingest promise");
  await ingestPromise;
  info("Done awaiting ingest promise");

  await checkSuggestions();
});

// Ingestion should be performed according to the defined interval.
add_task(async function interval() {
  let { ingestPromise } = QuickSuggest.rustBackend;

  // Re-enable the backend with a small ingest interval. A new ingest will
  // immediately start.
  let intervalSecs = 1;
  UrlbarPrefs.set("quicksuggest.rustIngestIntervalSeconds", intervalSecs);
  UrlbarPrefs.set("quicksuggest.rustEnabled", false);
  UrlbarPrefs.set("quicksuggest.rustEnabled", true);
  ({ ingestPromise } = await waitForIngestStart(ingestPromise));

  info("Awaiting initial ingest promise");
  await ingestPromise;
  info("Done awaiting initial ingest promise");

  // Wait for a few ingests to happen due to the timer firing.
  for (let i = 0; i < 3; i++) {
    info("Preparing for ingest at index " + i);

    // Set a new suggestion so we can make sure ingest really happened.
    let suggestion = {
      ...REMOTE_SETTINGS_SUGGESTION,
      url: REMOTE_SETTINGS_SUGGESTION.url + "/" + i,
    };
    await QuickSuggestTestUtils.setRemoteSettingsRecords(
      [
        {
          type: "data",
          attachment: [suggestion],
        },
      ],
      // Don't force sync since the whole point here is to make sure the backend
      // ingests on its own!
      { forceSync: false }
    );

    // Wait for ingest to start and finish.
    info("Waiting for ingest to start at index " + i);
    ({ ingestPromise } = await waitForIngestStart(ingestPromise));
    info("Waiting for ingest to finish at index " + i);
    await ingestPromise;
    await checkSuggestions([suggestion]);
  }

  info("Disabling the backend");
  UrlbarPrefs.set("quicksuggest.rustEnabled", false);

  // At this point, ingests should stop with two caveats. (1) There may be one
  // ongoing ingest that started immediately after `ingestPromise` resolved in
  // the final iteration of the loop above. (2) The timer manager sometimes
  // fires our ingest timer even after it was unregistered by the backend (when
  // the backend was disabled), maybe because the interval is so small in this
  // test. These two things mean that up to two more ingests may finish now.
  // We'll simply wait for a few seconds up to two times until no new ingests
  // start.

  let waitSecs = 3 * intervalSecs;
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  let wait = () => new Promise(r => setTimeout(r, 1000 * waitSecs));

  let waitedAtEndOfLoop = false;
  for (let i = 0; i < 2; i++) {
    info(`Waiting ${waitSecs}s after disabling backend, i=${i}...`);
    await wait();

    let { ingestPromise: newIngestPromise } = QuickSuggest.rustBackend;
    if (ingestPromise == newIngestPromise) {
      info(`No new ingest started, i=${i}`);
      waitedAtEndOfLoop = true;
      break;
    }

    info(`New ingest started, now awaiting, i=${i}`);
    ingestPromise = newIngestPromise;
    await ingestPromise;
  }

  if (!waitedAtEndOfLoop) {
    info(`Waiting a final ${waitSecs}s...`);
    await wait();
  }

  // No new ingests should have started.
  Assert.equal(
    QuickSuggest.rustBackend.ingestPromise,
    ingestPromise,
    "No new ingest started after disabling the backend"
  );

  // Clean up for later tasks: Reset the interval and enable the backend again.
  UrlbarPrefs.clear("quicksuggest.rustIngestIntervalSeconds");
  UrlbarPrefs.set("quicksuggest.rustEnabled", true);
  ({ ingestPromise } = await waitForIngestStart(ingestPromise));

  info("Awaiting cleanup ingest promise");
  await ingestPromise;
  info("Done awaiting cleanup ingest promise");
});

// `SuggestStore.interrupt()` should be called on shutdown.
add_task(async function shutdown() {
  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(QuickSuggest.rustBackend._test_store, "interrupt");

  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  AsyncShutdown.profileBeforeChange._trigger();

  let calls = spy.getCalls();
  Assert.equal(
    calls.length,
    1,
    "store.interrupt() should have been called once on simulated shutdown"
  );
  Assert.deepEqual(
    calls[0].args,
    [InterruptKind.READ_WRITE],
    "store.interrupt() should have been called with InterruptKind.READ_WRITE"
  );
  Assert.ok(
    InterruptKind.READ_WRITE,
    "Sanity check: InterruptKind.READ_WRITE is defined"
  );

  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
  sandbox.restore();
});

async function waitForIngestStart(oldIngestPromise) {
  let newIngestPromise;
  await TestUtils.waitForCondition(() => {
    let { ingestPromise } = QuickSuggest.rustBackend;
    if (
      (oldIngestPromise && ingestPromise != oldIngestPromise) ||
      (!oldIngestPromise && ingestPromise)
    ) {
      newIngestPromise = ingestPromise;
      return true;
    }
    return false;
  }, "Waiting for a new ingest to start");

  Assert.equal(
    QuickSuggest.rustBackend.ingestPromise,
    newIngestPromise,
    "Sanity check: ingestPromise hasn't changed since waitForCondition returned"
  );

  // A bare promise can't be returned because it will cause the awaiting caller
  // to await that promise! We're simply trying to return the promise, which the
  // caller can later await.
  return { ingestPromise: newIngestPromise };
}

async function checkSuggestions(expected = [REMOTE_SETTINGS_SUGGESTION]) {
  let actual = await QuickSuggest.rustBackend.query("amp");
  Assert.deepEqual(
    actual.map(s => s.url),
    expected.map(s => s.url),
    "Backend should be serving the expected suggestions"
  );
}

/**
 * Sets up the update timer manager for testing: makes it fire more often,
 * removes all existing timers, and initializes it for testing. The body of this
 * function is copied from:
 * https://searchfox.org/mozilla-central/source/toolkit/components/timermanager/tests/unit/consumerNotifications.js
 */
function initUpdateTimerManager() {
  // Set the timer to fire every second
  Services.prefs.setIntPref(
    PREF_APP_UPDATE_TIMERMINIMUMDELAY,
    MAIN_TIMER_INTERVAL / 1000
  );
  Services.prefs.setIntPref(
    PREF_APP_UPDATE_TIMERFIRSTINTERVAL,
    MAIN_TIMER_INTERVAL
  );

  // Remove existing update timers to prevent them from being notified
  for (let { data: entry } of Services.catMan.enumerateCategory(
    CATEGORY_UPDATE_TIMER
  )) {
    Services.catMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, entry, false);
  }

  Cc["@mozilla.org/updates/timer-manager;1"]
    .getService(Ci.nsIUpdateTimerManager)
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "utm-test-init", "");
}
