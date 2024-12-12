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

    for (let provider of globalActionsProviders) {
      if (provider.isActive(queryContext)) {
        for (let action of (await provider.queryActions(queryContext)) || []) {
          this.#actions.set(action.key, action);
        }
      }
    }

    if (!this.#actions.size) {
      return;
    }

    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.ACTIONS,
      {
        results: [...this.#actions.keys()],
        dynamicType: DYNAMIC_TYPE_NAME,
        inputLength: queryContext.searchString.length,
      }
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
    for (let provider of globalActionsProviders) {
      provider.onSearchSessionEnd?.(queryContext, controller, details);
    }
  }

  getViewTemplate(result) {
    return {
      children: [
        {
          name: "buttons",
          tag: "div",
          children: result.payload.results.map((key, i) => {
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
          }),
        },
      ],
    };
  }

  getViewUpdate(result) {
    let viewUpdate = {};
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
