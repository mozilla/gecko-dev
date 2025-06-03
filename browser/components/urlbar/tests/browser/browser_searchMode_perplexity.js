/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Verify perplexity enters search mode and the PREF perplexity.hasBeenInSearchmode
 * is correctly set to true after entering search mode.
 */

"use strict";

const CONFIG = [
  {
    recordType: "engine",
    // The identifier must be different from the perplexity engine in search-config-v2.
    // Otherwise, SearchService won’t install the engine if the ID is the same.
    identifier: "Perplexity-2",
    base: {
      name: "Perplexity",
      urls: {
        search: {
          base: "https://example.com",
          searchTermParamName: "q",
        },
        suggestions: {
          base: "https://example.com",
          method: "GET",
          searchTermParamName: "search",
        },
      },
      aliases: ["Perplexity"],
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
];

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.scotchBonnet.enableOverride", true],
      ["browser.urlbar.perplexity.hasBeenInSearchMode", false],
    ],
  });
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG);
});

add_task(async function test_perplexity_has_been_in_search_mode() {
  Assert.equal(
    Services.prefs.getBoolPref("browser.urlbar.perplexity.hasBeenInSearchMode"),
    false,
    "Perplexity.hasBeenInSearchMode PREF should initially be false"
  );

  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
  info("Press on the Perplexity menu button and enter search mode");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector("menuitem[label=Perplexity]").click();
  await popupHidden;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Perplexity",
    entry: "searchbutton",
    source: 3,
  });
  Assert.equal(
    Services.prefs.getBoolPref("browser.urlbar.perplexity.hasBeenInSearchMode"),
    true,
    "Perplexity.hasBeenInSearchMode PREF should be true after being in search mode."
  );
});
