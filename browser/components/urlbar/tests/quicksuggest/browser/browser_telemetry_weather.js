/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for weather suggestions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

const suggestion_type = "weather";
const match_type = "firefox-suggest";
const index = 1;
const position = index + 1;

const { TELEMETRY_SCALARS: WEATHER_SCALARS } = UrlbarProviderWeather;
const { WEATHER_SUGGESTION: suggestion, WEATHER_RS_DATA } = MerinoTestUtils;

// Trying to avoid timeouts in TV mode.
requestLongerTimeout(3);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Make sure quick actions are disabled because showing them in the top
      // sites view interferes with this test.
      ["browser.urlbar.suggest.quickactions", false],
    ],
  });

  await setUpTelemetryTest({
    remoteSettingsRecords: [
      {
        type: "weather",
        weather: WEATHER_RS_DATA,
      },
    ],
  });
  await MerinoTestUtils.initWeather();
});

add_tasks_with_rust(async function () {
  let rustEnabled = UrlbarPrefs.get("quicksuggest.rustEnabled");
  await doTelemetryTest({
    index,
    suggestion,
    providerName: rustEnabled
      ? UrlbarProviderQuickSuggest.name
      : UrlbarProviderWeather.name,
    showSuggestion: async () => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: MerinoTestUtils.WEATHER_KEYWORD,
      });
    },
    teardown: async () => {
      // Picking the block button sets this pref to false and disables weather
      // suggestions. We need to flip it back to true and wait for the
      // suggestion to be fetched again before continuing to the next selectable
      // test. The view also also stay open, so close it afterward.
      if (!UrlbarPrefs.get("suggest.weather")) {
        await UrlbarTestUtils.promisePopupClose(window);
        gURLBar.handleRevert();
        UrlbarPrefs.clear("suggest.weather");

        // Wait for keywords to be re-synced from remote settings.
        await QuickSuggestTestUtils.forceSync();
        await QuickSuggest.weather.fetchPromise;
      }
    },
    // impression-only
    impressionOnly: {
      scalars: rustEnabled
        ? {}
        : {
            [WEATHER_SCALARS.IMPRESSION]: position,
          },
      event: {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "impression_only",
        extra: {
          suggestion_type,
          match_type,
          position: position.toString(),
        },
      },
    },
    // click
    click: {
      scalars: rustEnabled
        ? {}
        : {
            [WEATHER_SCALARS.IMPRESSION]: position,
            [WEATHER_SCALARS.CLICK]: position,
          },
      event: {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "click",
        extra: {
          suggestion_type,
          match_type,
          position: position.toString(),
        },
      },
    },
    commands: [
      // not relevant
      {
        command: [
          "[data-l10n-id=firefox-suggest-command-dont-show-this]",
          "not_relevant",
        ],
        scalars: rustEnabled
          ? {}
          : {
              [WEATHER_SCALARS.IMPRESSION]: position,
            },
        event: {
          category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
          method: "engagement",
          object: "other",
          extra: {
            suggestion_type,
            match_type,
            position: position.toString(),
          },
        },
      },
    ],
  });
});
