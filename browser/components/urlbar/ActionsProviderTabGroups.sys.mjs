/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  ActionsProvider,
  ActionsResult,
} from "resource:///modules/ActionsProvider.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

const MIN_SEARCH_PREF = "tabGroups.minSearchLength";

/**
 * A provider that matches the urlbar input to built in actions.
 */
class ProviderTabGroups extends ActionsProvider {
  get name() {
    return "ActionsProviderTabGroups";
  }

  isActive(queryContext) {
    return (
      lazy.NimbusFeatures.tabGroups.getVariable("enabled") &&
      !queryContext.searchMode &&
      queryContext.trimmedSearchString.length < 50 &&
      queryContext.trimmedSearchString.length >=
        lazy.UrlbarPrefs.get(MIN_SEARCH_PREF)
    );
  }

  async queryActions(queryContext) {
    let window = lazy.BrowserWindowTracker.getTopWindow();
    if (!window) {
      // We're likely running xpcshell tests if this happens in automation.
      if (!Cu.isInAutomation) {
        console.error("Couldn't find a browser window.");
      }
      return null;
    }
    let input = queryContext.trimmedLowerCaseSearchString;
    let results = [];
    let i = 0;

    for (let group of window.gBrowser.getAllTabGroups()) {
      if (group.label.toLowerCase().startsWith(input)) {
        results.push(
          this.#makeResult({
            key: `tabgroup-${i++}`,
            l10nId: "urlbar-result-action-switch-to-tabgroup",
            l10nArgs: { group: group.label },
            onPick: (_queryContext, _controller) => {
              this.#switchToGroup(group);
            },
            color: group.color,
          })
        );
      }
    }

    for (let savedGroup of lazy.SessionStore.getSavedTabGroups()) {
      if (savedGroup.name.toLowerCase().startsWith(input)) {
        results.push(
          this.#makeResult({
            key: `tabgroup-${i++}`,
            l10nId: "urlbar-result-action-open-saved-tabgroup",
            l10nArgs: { group: savedGroup.name },
            onPick: (_queryContext, _controller) => {
              let group = lazy.SessionStore.openSavedTabGroup(savedGroup.id);
              this.#switchToGroup(group);
            },
            color: savedGroup.color,
          })
        );
      }
    }

    return results;
  }

  #makeResult({ key, l10nId, l10nArgs, onPick, color }) {
    return new ActionsResult({
      key,
      l10nId,
      l10nArgs,
      onPick,
      icon: "chrome://browser/skin/tabbrowser/tab-groups.svg",
      dataset: {
        style: {
          "--tab-group-color": `var(--tab-group-color-${color})`,
          "--tab-group-color-invert": `var(--tab-group-color-${color}-invert)`,
          "--tab-group-color-pale": `var(--tab-group-color-${color}-pale)`,
        },
      },
    });
  }

  #switchToGroup(group) {
    group.select();
    group.ownerGlobal.focus();
  }
}

export var ActionsProviderTabGroups = new ProviderTabGroups();
