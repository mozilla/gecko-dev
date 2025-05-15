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
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
  TabMetrics: "moz-src:///browser/components/tabbrowser/TabMetrics.sys.mjs",
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
      Services.prefs.getBoolPref("browser.tabs.groups.enabled") &&
      (!queryContext.restrictSource ||
        queryContext.restrictSource == lazy.UrlbarUtils.RESULT_SOURCE.TABS) &&
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
    let results = [];
    let i = 0;

    for (let group of window.gBrowser.getAllTabGroups({
      sortByLastSeenActive: true,
    })) {
      if (
        group.ownerGlobal == window &&
        window.gBrowser.selectedTab.group == group
      ) {
        // This group is already the active group, so don't offer switching to it.
        continue;
      }
      if (!this.#matches(group.label, queryContext)) {
        continue;
      }
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

    if (queryContext.isPrivate) {
      // Tab groups can't be saved or reopened in private windows.
      return results;
    }

    for (let savedGroup of lazy.SessionStore.getSavedTabGroups()) {
      if (!this.#matches(savedGroup.name, queryContext)) {
        continue;
      }
      results.push(
        this.#makeResult({
          key: `tabgroup-${i++}`,
          l10nId: "urlbar-result-action-open-saved-tabgroup",
          l10nArgs: { group: savedGroup.name },
          onPick: (_queryContext, _controller) => {
            let group = lazy.SessionStore.openSavedTabGroup(
              savedGroup.id,
              window,
              {
                source: lazy.TabMetrics.METRIC_SOURCE.SUGGEST,
              }
            );
            this.#switchToGroup(group);
          },
          color: savedGroup.color,
        })
      );
    }

    return results;
  }

  #matches(groupName, queryContext) {
    groupName = groupName.toLowerCase();
    if (queryContext.trimmedLowerCaseSearchString.length == 1) {
      return groupName.startsWith(queryContext.trimmedLowerCaseSearchString);
    }
    return queryContext.tokens.every(token =>
      groupName.includes(token.lowerCaseValue)
    );
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
