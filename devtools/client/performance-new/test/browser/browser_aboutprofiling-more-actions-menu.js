/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function waitForClipboard() {
  return waitUntil(() => navigator.clipboard.readText());
}

// Before starting the test, let's find out which features are selected from the
// firefox-platform preset.
const supportedFeatures = Services.profiler.GetFeatures();
const { presets } = ChromeUtils.importESModule(
  "resource://devtools/client/performance-new/shared/background.sys.mjs"
);
// Depending on the environment, not all features are present. So that the test
// works for all environments, we need to compute this value instead of
// hardcoding it.
const featuresForFirefoxPlatformPreset = presets[
  "firefox-platform"
].features.filter(feature => supportedFeatures.includes(feature));
const featuresForFirefoxPlatformPresetAsString =
  featuresForFirefoxPlatformPreset.join(",");

add_task(async function test() {
  info("Test the more actions button in about:profiling.");
  info(
    "Set the preference devtools.performance.aboutprofiling.has-developer-options to true"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.performance.aboutprofiling.has-developer-options", true]],
  });

  await withAboutProfiling(async (document, browser) => {
    info("Make sure the firefox-platform preset is checked for consistency.");
    const firefoxPlatformLabel = await getElementFromDocumentByText(
      document,
      AppConstants.MOZ_APP_NAME
    );
    EventUtils.synthesizeMouseAtCenter(
      firefoxPlatformLabel,
      {},
      browser.contentWindow
    );
    is(
      Services.prefs.getCharPref("devtools.performance.recording.preset", ""),
      "firefox-platform",
      'The preset "firefox-platform" is selected.'
    );

    info("Test that there is a button to show a menu with more actions.");
    const moreActionsButton = document.querySelector("moz-button");
    ok(moreActionsButton, "There is a button.");
    ok(moreActionsButton.shadowRoot, "The button contains a shadowDom.");

    // Make sure we have an accessible name
    is(
      moreActionsButton.shadowRoot.querySelector("button").title,
      "More actions",
      "Test that the more actions button has a title"
    );

    info("Test that the button is clickable");
    // The second argument is the event object. By passing an empty object, this
    // tells the utility function to generate a mousedown then a mouseup, that
    // is a click.
    EventUtils.synthesizeMouseAtCenter(
      moreActionsButton,
      {},
      browser.contentWindow
    );
    let item = await getElementFromDocumentByText(
      document,
      "with startup profiling"
    );
    ok(item, "The item to restart with startup profiling has been displayed");
    // Skipping clicking on the item, we don't want to actually restart firefox
    // during the test. But most of the code is common with the below use case
    // "copy environment variables".

    info("Will copy environment variables for startup profiling");
    SpecialPowers.cleanupAllClipboard();
    item = await getElementFromDocumentByText(
      document,
      "Copy environment variables"
    );
    ok(
      item,
      "The item to copy environment variables for startup profiling is present in the menu"
    );

    EventUtils.synthesizeMouseAtCenter(item, {}, browser.contentWindow);
    is(
      await waitForClipboard(),
      `MOZ_PROFILER_STARTUP='1' MOZ_PROFILER_STARTUP_INTERVAL='1' MOZ_PROFILER_STARTUP_ENTRIES='134217728' MOZ_PROFILER_STARTUP_FEATURES='${featuresForFirefoxPlatformPresetAsString}' MOZ_PROFILER_STARTUP_FILTERS='GeckoMain,Compositor,Renderer,SwComposite,DOM Worker'`,
      "The clipboard contains the environment variables suitable for startup profiling."
    );

    info("Will copy parameters for performance tests profiling");
    SpecialPowers.cleanupAllClipboard();
    EventUtils.synthesizeMouseAtCenter(
      moreActionsButton,
      {},
      browser.contentWindow
    );
    item = await getElementFromDocumentByText(document, "performance tests");
    ok(
      item,
      "The item to copy the parameters to performance tests is present in the menu"
    );
    EventUtils.synthesizeMouseAtCenter(item, {}, browser.contentWindow);

    is(
      await waitForClipboard(),
      `--gecko-profile --gecko-profile-interval 1 --gecko-profile-entries 134217728 --gecko-profile-features '${featuresForFirefoxPlatformPresetAsString}' --gecko-profile-threads 'GeckoMain,Compositor,Renderer,SwComposite,DOM Worker'`,
      "The clipboard contains the parameters suitable for performance tests."
    );
    SpecialPowers.cleanupAllClipboard();

    // With the preference set to false, the items aren't present
    info(
      "Set the preference devtools.performance.aboutprofiling.has-developer-options to false"
    );
    await SpecialPowers.pushPrefEnv({
      set: [
        ["devtools.performance.aboutprofiling.has-developer-options", false],
      ],
    });
    EventUtils.synthesizeMouseAtCenter(
      moreActionsButton,
      {},
      browser.contentWindow
    );
    await getElementFromDocumentByText(document, "with startup profiling");
    // The item that's always present is now displayed
    ok(
      !maybeGetElementFromDocumentByText(
        document,
        "Copy environment variables"
      ),
      "The item to copy environment variables is not present."
    );
    ok(
      !maybeGetElementFromDocumentByText(document, "performance tests"),
      "the item to copy the parameters for performance tests is not present."
    );
  });
});
