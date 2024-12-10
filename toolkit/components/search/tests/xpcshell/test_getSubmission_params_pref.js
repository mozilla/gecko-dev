/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that preference parameters used in search URLs are from the
 * default branch, and that their special characters are URL encoded. */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const defaultBranch = Services.prefs.getDefaultBranch(
  SearchUtils.BROWSER_SEARCH_PREF
);
const baseURL = "https://example.com/search?";

const CONFIG = [
  {
    identifier: "preferenceEngine",
    base: {
      urls: {
        search: {
          base: "https://example.com/search",
          params: [
            {
              name: "code",
              enterpriseValue: "enterprise",
            },
            {
              name: "code",
              experimentConfig: "code",
            },
            {
              name: "test",
              experimentConfig: "test",
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
  },
];

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
});

add_task(async function test_pref_initial_value() {
  defaultBranch.setCharPref("param.code", "good&id=unique");

  // Preference params are only allowed to be modified on the user branch
  // on nightly builds. For non-nightly builds, check that modifying on the
  // normal branch doesn't work.
  if (!AppConstants.NIGHTLY_BUILD) {
    Services.prefs.setCharPref(
      SearchUtils.BROWSER_SEARCH_PREF + "param.code",
      "bad"
    );
  }

  await Services.search.init();

  const engine = Services.search.getEngineById("preferenceEngine");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "code=good%26id%3Dunique&q=foo",
    "Should have got the submission URL with the correct code"
  );

  // Now clear the user-set preference. Having a user set preference means
  // we don't get updates from the pref service of changes on the default
  // branch. Normally, this won't be an issue, since we don't expect users
  // to be playing with these prefs, and worst-case, they'll just get the
  // actual change on restart.
  Services.prefs.clearUserPref(SearchUtils.BROWSER_SEARCH_PREF + "param.code");
});

add_task(async function test_pref_updated() {
  // Update the pref without re-init nor restart.
  defaultBranch.setCharPref("param.code", "supergood&id=unique123456");

  const engine = Services.search.getEngineById("preferenceEngine");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "code=supergood%26id%3Dunique123456&q=foo",
    "Should have got the submission URL with the updated code"
  );
});

add_task(async function test_pref_cleared() {
  // Update the pref without re-init nor restart.
  // Note you can't delete a preference from the default branch.
  defaultBranch.setCharPref("param.code", "");

  let engine = Services.search.getEngineById("preferenceEngine");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "q=foo",
    "Should have just the base URL after the pref was cleared"
  );
});

add_task(async function test_pref_updated_enterprise() {
  // Set the pref to some value and enable enterprise mode at the same time.
  defaultBranch.setCharPref("param.code", "supergood&id=unique123456");
  await enableEnterprise();

  const engine = Services.search.getEngineById("preferenceEngine");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "code=enterprise&q=foo",
    "Enterprise parameter should override experiment config."
  );
});
