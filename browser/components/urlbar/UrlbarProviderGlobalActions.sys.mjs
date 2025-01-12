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

const SCOTCH_BONNET_PREF = "scotchBonnet.enableOverride";
const ACTIONS_PREF = "secondaryActions.featureGate";
const QUICK_ACTIONS_PREF = "suggest.quickactions";

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
  // A Map of the last queried actions.
  #actions = new Map();

  get name() {
    return "UrlbarProviderGlobalActions";
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  isActive() {
    return (
      (lazy.UrlbarPrefs.get(SCOTCH_BONNET_PREF) ||
        lazy.UrlbarPrefs.get(ACTIONS_PREF)) &&
      lazy.UrlbarPrefs.get(QUICK_ACTIONS_PREF)
    );
  }

  async startQuery(queryContext, addCallback) {
    this.#actions.clear();

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
          this.#actions.set(action.key, action);
        }
      }
    }

    if (!this.#actions.size) {
      return;
    }

    let showOnboardingLabel =
      lazy.UrlbarPrefs.get(TIMES_TO_SHOW_PREF) >
      lazy.UrlbarPrefs.get(TIMES_SHOWN_PREF);

    let results = [...this.#actions.keys()];

    let query = results.includes("matched-contextual-search")
      ? ""
      : queryContext.searchString;

    let payload = {
      results,
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
    result.suggestedIndex = SUGGESTED_INDEX;
    addCallback(this, result);
  }

  onSelection(result, element) {
    let key = element.dataset.action;
    this.#actions.get(key).onSelection?.(result, element);
  }

  onEngagement(queryContext, controller, details) {
    let key = details.element.dataset.action;
    let options = this.#actions.get(key).onPick(queryContext, controller);
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
    let children = result.payload.results.map((key, i) => {
      let action = this.#actions.get(key);
      let style;
      if (action.dataset?.style) {
        style = "";
        for (let [prop, val] of Object.entries(action.dataset.style)) {
          style += `${prop}: ${val};`;
        }
      }
      return {
        name: `button-${i}`,
        tag: "span",
        classList: ["urlbarView-action-btn"],
        attributes: {
          style,
          inputLength: result.payload.inputLength,
          "data-action": key,
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
          },
        ],
      };
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
    result.payload.results.forEach((key, i) => {
      let action = this.#actions.get(key);
      viewUpdate[`label-${i}`] = {
        l10n: { id: action.l10nId, args: action.l10nArgs, cacheable: true },
      };
    });
    return viewUpdate;
  }
}

export var UrlbarProviderGlobalActions = new ProviderGlobalActions();
