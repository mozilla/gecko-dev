/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that returns all the available
 * global actions for a query.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

// Default icon shown for actions if no custom one is provided.
const DEFAULT_ICON = "chrome://global/skin/icons/settings.svg";
const DYNAMIC_TYPE_NAME = "actions";

// The suggestion index of the actions row within the urlbar results.
const SUGGESTED_INDEX = 1;
const SUGGESTED_INDEX_TABS_MODE = 0;

const SCOTCH_BONNET_PREF = "scotchBonnet.enableOverride";
const ACTIONS_PREF = "secondaryActions.featureGate";
const QUICK_ACTIONS_PREF = "suggest.quickactions";
const MAX_ACTIONS_PREF = "secondaryActions.maxActionsShown";

// Prefs relating to the onboarding label shown to new users.
const TIMES_TO_SHOW_PREF = "quickactions.timesToShowOnboardingLabel";
const TIMES_SHOWN_PREF = "quickactions.timesShownOnboardingLabel";

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

import { ActionsProviderQuickActions } from "resource:///modules/ActionsProviderQuickActions.sys.mjs";
import { ActionsProviderContextualSearch } from "resource:///modules/ActionsProviderContextualSearch.sys.mjs";
import { ActionsProviderTabGroups } from "resource:///modules/ActionsProviderTabGroups.sys.mjs";

let globalActionsProviders = [
  ActionsProviderContextualSearch,
  ActionsProviderQuickActions,
  ActionsProviderTabGroups,
];

/**
 * A provider that lets the user view all available global actions for a query.
 */
class ProviderGlobalActions extends UrlbarProvider {
  get name() {
    return "UrlbarProviderGlobalActions";
  }

  /**
   * @returns {Values<typeof UrlbarUtils.PROVIDER_TYPE>}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  async isActive() {
    return (
      (lazy.UrlbarPrefs.get(SCOTCH_BONNET_PREF) ||
        lazy.UrlbarPrefs.get(ACTIONS_PREF)) &&
      lazy.UrlbarPrefs.get(QUICK_ACTIONS_PREF)
    );
  }

  async startQuery(queryContext, addCallback) {
    let actionsResults = [];
    let searchModeEngine = "";

    for (let provider of globalActionsProviders) {
      if (provider.isActive(queryContext)) {
        for (let action of (await provider.queryActions(queryContext)) || []) {
          if (action.engine && !searchModeEngine) {
            searchModeEngine = action.engine;
          } else if (action.engine) {
            // We only allow one action that provides an engine search mode.
            continue;
          }
          action.providerName = provider.name;
          actionsResults.push(action);
        }
      }
    }

    if (!actionsResults.length) {
      return;
    }

    if (actionsResults.length > lazy.UrlbarPrefs.get(MAX_ACTIONS_PREF)) {
      actionsResults.length = lazy.UrlbarPrefs.get(MAX_ACTIONS_PREF);
    }

    let showOnboardingLabel =
      lazy.UrlbarPrefs.get(TIMES_TO_SHOW_PREF) >
      lazy.UrlbarPrefs.get(TIMES_SHOWN_PREF);

    let query = actionsResults.some(a => a.key == "matched-contextual-search")
      ? ""
      : queryContext.searchString;

    let payload = {
      actionsResults,
      dynamicType: DYNAMIC_TYPE_NAME,
      inputLength: queryContext.searchString.length,
      input: query,
      showOnboardingLabel,
      query,
    };

    if (searchModeEngine) {
      payload.providesSearchMode = true;
      payload.engine = searchModeEngine;
    }

    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.ACTIONS,
      payload
    );
    result.suggestedIndex =
      queryContext.restrictSource == UrlbarUtils.RESULT_SOURCE.TABS
        ? SUGGESTED_INDEX_TABS_MODE
        : SUGGESTED_INDEX;
    addCallback(this, result);
  }

  onSelection(result, element) {
    let key = element.dataset.action;
    let action = result.payload.actionsResults.find(a => a.key == key);
    action.onSelection?.(result, element);
  }

  onEngagement(queryContext, controller, details) {
    let key = details.element.dataset.action;
    let action = details.result.payload.actionsResults.find(a => a.key == key);
    let options = action.onPick(queryContext, controller);
    if (options?.focusContent) {
      details.element.ownerGlobal.gBrowser.selectedBrowser.focus();
    }
    controller.view.close();
  }

  onSearchSessionEnd(queryContext, controller, details) {
    let showOnboardingLabel = queryContext.results?.find(
      r => r.providerName == this.name
    )?.payload.showOnboardingLabel;
    if (showOnboardingLabel) {
      lazy.UrlbarPrefs.set(
        TIMES_SHOWN_PREF,
        lazy.UrlbarPrefs.get(TIMES_SHOWN_PREF) + 1
      );
    }
    for (let provider of globalActionsProviders) {
      provider.onSearchSessionEnd?.(queryContext, controller, details);
    }
  }

  getViewTemplate(result) {
    let children = result.payload.actionsResults.map((action, i) => {
      let btn = {
        name: `button-${i}`,
        tag: "span",
        classList: ["urlbarView-action-btn"],
        attributes: {
          inputLength: result.payload.inputLength,
          "data-action": action.key,
          role: "button",
        },
        children: [
          {
            tag: "img",
            attributes: {
              src: action.icon || DEFAULT_ICON,
            },
          },
          {
            name: `label-${i}`,
            tag: "span",
            classList: ["urlbarView-action-btn-label"],
          },
        ],
      };

      if (action.dataset?.style) {
        let style = "";
        for (let [prop, val] of Object.entries(action.dataset.style)) {
          style += `${prop}: ${val};`;
        }
        btn.attributes.style = style;
      }

      if (action.dataset?.providesSearchMode) {
        btn.attributes["data-provides-searchmode"] = "true";
      }

      return btn;
    });

    if (result.payload.showOnboardingLabel) {
      children.unshift({
        name: "press-tab-label",
        tag: "span",
        classList: ["urlbarView-press-tab-label"],
      });
    }

    return { children };
  }

  getViewUpdate(result) {
    let viewUpdate = {};
    if (result.payload.showOnboardingLabel) {
      viewUpdate["press-tab-label"] = {
        l10n: { id: "press-tab-label", cacheable: true },
      };
    }
    result.payload.actionsResults.forEach((action, i) => {
      viewUpdate[`label-${i}`] = {
        l10n: { id: action.l10nId, args: action.l10nArgs, cacheable: true },
      };
    });
    return viewUpdate;
  }
}

export var UrlbarProviderGlobalActions = new ProviderGlobalActions();
