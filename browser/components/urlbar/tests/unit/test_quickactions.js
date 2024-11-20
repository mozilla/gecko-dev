/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ActionsProviderQuickActions:
    "resource:///modules/ActionsProviderQuickActions.sys.mjs",
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.sys.mjs",
});

add_setup(async () => {
  UrlbarPrefs.set("secondaryActions.featureGate", true);

  ActionsProviderQuickActions.addAction("newaction", {
    commands: ["newaction"],
  });

  registerCleanupFunction(async () => {
    UrlbarPrefs.clear("secondaryActions.featureGate");
    ActionsProviderQuickActions.removeAction("newaction");
  });
});

add_task(async function nomatch() {
  let context = createContext("this doesnt match", {});
  let results = await ActionsProviderQuickActions.queryActions(context);
  Assert.ok(results === null, "there were no matches");
});

add_task(async function quickactions_match() {
  let context = createContext("new", {});
  let results = await ActionsProviderQuickActions.queryActions(context);
  Assert.ok(results[0].key == "newaction", "Matched the new action");
});

add_task(async function duplicate_matches() {
  ActionsProviderQuickActions.addAction("testaction", {
    commands: ["testaction", "test"],
  });

  let context = createContext("test", {});
  let results = await ActionsProviderQuickActions.queryActions(context);

  Assert.ok(results[0].key == "testaction", "Matched the test action");

  ActionsProviderQuickActions.removeAction("testaction");
});

add_task(async function remove_action() {
  ActionsProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
  });
  ActionsProviderQuickActions.removeAction("testaction");

  let context = createContext("test", {});
  let result = await ActionsProviderQuickActions.queryActions(context);

  Assert.ok(result === null, "there were no matches");
});

add_task(async function minimum_search_string() {
  let searchString = "newa";
  for (let minimumSearchString of [3]) {
    info(`Setting 'minimumSearchString' to ${minimumSearchString}`);
    UrlbarPrefs.set("quickactions.minimumSearchString", minimumSearchString);
    for (let i = 1; i < 4; i++) {
      let context = createContext(searchString.substring(0, i), {});
      let result = await ActionsProviderQuickActions.queryActions(context);
      let isActive = ActionsProviderQuickActions.isActive(context);

      if (i >= minimumSearchString) {
        Assert.ok(result[0].key == "newaction", "Matched the new action");
        Assert.equal(isActive, true, "Provider is active");
      } else {
        Assert.equal(isActive, false, "Provider is not active");
      }
    }
  }
  UrlbarPrefs.clear("quickactions.minimumSearchString");
});

add_task(async function interventions_disabled() {
  let context = createContext("test", { isPrivate: false });
  Assert.ok(
    !UrlbarProviderInterventions.isActive(context),
    "Urlbar interventions are disabled when actions are enabled"
  );
});
