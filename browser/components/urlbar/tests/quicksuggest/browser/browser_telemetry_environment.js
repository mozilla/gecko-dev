/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests Suggest telemetry in `TelemetryEnvironment`.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
});

add_setup(async function () {
  await QuickSuggestTestUtils.ensureQuickSuggestInit();
});

// Toggles the `suggest.quicksuggest.nonsponsored` pref.
add_task(function nonsponsoredToggled() {
  doToggleTest("suggest.quicksuggest.nonsponsored");
});

// Toggles the `suggest.quicksuggest.sponsored` pref.
add_task(async function sponsoredToggled() {
  doToggleTest("suggest.quicksuggest.sponsored");
});

// Toggles the `quicksuggest.dataCollection.enabled` pref.
add_task(async function dataCollectionToggled() {
  doToggleTest("quicksuggest.dataCollection.enabled");
});

function doToggleTest(pref) {
  let enabled = UrlbarPrefs.get(pref);
  Assert.equal(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar." + pref
    ],
    enabled,
    "Initial value of pref should be correct in TelemetryEnvironment: " + pref
  );

  for (let i = 0; i < 2; i++) {
    enabled = !enabled;
    UrlbarPrefs.set(pref, enabled);
    Assert.equal(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        "browser.urlbar." + pref
      ],
      enabled,
      "Pref should be correct in TelemetryEnvironment: " + pref
    );
  }
}

// Simulates the race on startup between telemetry environment initialization
// and the initial update of the Suggest scenario. After startup is done,
// telemetry environment should record the correct values for startup prefs.
add_task(async function telemetryEnvironmentOnStartup() {
  await QuickSuggestTestUtils.setScenario(null);

  // Restart telemetry environment so we know it's watching its default set of
  // prefs.
  await TelemetryEnvironment.testCleanRestart().onInitialized();

  // Get the prefs that UrlbarPrefs sets when the Suggest scenario is updated on
  // startup. They're the union of the prefs exposed in the UI and the prefs
  // that are set on the default branch per scenario.
  let prefs = [
    ...new Set([
      ...Object.values(UrlbarPrefs.FIREFOX_SUGGEST_UI_PREFS_BY_VARIABLE),
      ...Object.values(UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS)
        .map(valuesByPrefName => Object.keys(valuesByPrefName))
        .flat(),
    ]),
  ];

  // Not all of the prefs are recorded in telemetry environment. Filter in the
  // ones that are.
  prefs = prefs.filter(
    p =>
      `browser.urlbar.${p}` in
      TelemetryEnvironment.currentEnvironment.settings.userPrefs
  );

  info("Got startup prefs: " + JSON.stringify(prefs));

  // Sanity check the expected prefs. This isn't strictly necessary since we
  // programmatically get the prefs above, but it's an extra layer of defense,
  // for example in case we accidentally filtered out some expected prefs above.
  // If this fails, you might have added a startup pref but didn't update this
  // array here.
  Assert.deepEqual(
    prefs.sort(),
    [
      "quicksuggest.dataCollection.enabled",
      "suggest.quicksuggest.nonsponsored",
      "suggest.quicksuggest.sponsored",
    ],
    "Expected startup prefs"
  );

  // Make sure the prefs don't have user values that would mask the default
  // values.
  for (let p of prefs) {
    UrlbarPrefs.clear(p);
  }

  // Build a map of default values.
  let defaultValues = Object.fromEntries(
    prefs.map(p => [p, UrlbarPrefs.get(p)])
  );

  // Now simulate startup. Restart telemetry environment but don't wait for it
  // to finish before calling `updateFirefoxSuggestScenario()`. This simulates
  // startup where telemetry environment's initialization races the intial
  // update of the Suggest scenario.
  let environmentInitPromise =
    TelemetryEnvironment.testCleanRestart().onInitialized();

  // Update the scenario and force the startup prefs to take on values that are
  // the inverse of what they are now.
  await UrlbarPrefs.updateFirefoxSuggestScenario({
    isStartup: true,
    scenario: "online",
    defaultPrefs: {
      online: Object.fromEntries(
        Object.entries(defaultValues).map(([p, value]) => [p, !value])
      ),
    },
  });

  // At this point telemetry environment should be done initializing since
  // `updateFirefoxSuggestScenario()` waits for it, but await our promise now.
  await environmentInitPromise;

  // TelemetryEnvironment should have cached the new values.
  for (let [p, value] of Object.entries(defaultValues)) {
    let expected = !value;
    Assert.strictEqual(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        `browser.urlbar.${p}`
      ],
      expected,
      `Check 1: ${p} is ${expected} in TelemetryEnvironment`
    );
  }

  // Simulate another startup and set all prefs back to their original default
  // values.
  environmentInitPromise =
    TelemetryEnvironment.testCleanRestart().onInitialized();

  await UrlbarPrefs.updateFirefoxSuggestScenario({
    isStartup: true,
    scenario: "online",
    defaultPrefs: {
      online: defaultValues,
    },
  });

  await environmentInitPromise;

  // TelemetryEnvironment should have cached the new (original) values.
  for (let [p, value] of Object.entries(defaultValues)) {
    let expected = value;
    Assert.strictEqual(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        `browser.urlbar.${p}`
      ],
      expected,
      `Check 2: ${p} is ${expected} in TelemetryEnvironment`
    );
  }

  await TelemetryEnvironment.testCleanRestart().onInitialized();
});
