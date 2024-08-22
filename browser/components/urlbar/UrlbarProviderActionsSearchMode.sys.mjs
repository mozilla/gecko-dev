/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that returns all the available
 * actions for an actions search mode.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

// Default icon shown for actions if no custom one is provided.
const DEFAULT_ICON = "chrome://global/skin/icons/settings.svg";
const DYNAMIC_TYPE_NAME = "actions";

ChromeUtils.defineESModuleGetters(lazy, {
  ActionsProviderQuickActions:
    "resource:///modules/ActionsProviderQuickActions.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

/**
 * A provider that lets the user view all available actions while in searchMode.
 */
class ProviderActionsSearchMode extends UrlbarProvider {
  get name() {
    return "UrlbarProviderActionsSearchMode";
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  isActive(queryContext) {
    return queryContext.searchMode?.source == UrlbarUtils.RESULT_SOURCE.ACTIONS;
  }

  async startQuery(queryContext, addCallback) {
    let input = queryContext.trimmedLowerCaseSearchString;
    let results = await lazy.ActionsProviderQuickActions.getActions(input);
    results.forEach(resultKey => {
      let result = new lazy.UrlbarResult(
        UrlbarUtils.RESULT_TYPE.DYNAMIC,
        UrlbarUtils.RESULT_SOURCE.ACTIONS,
        {
          key: resultKey,
          dynamicType: DYNAMIC_TYPE_NAME,
        }
      );
      addCallback(this, result);
    });
  }

  onEngagement(queryContext, controller, details) {
    lazy.ActionsProviderQuickActions.pickAction(
      queryContext,
      controller,
      details.element
    );
  }

  getViewTemplate(result) {
    let action = lazy.ActionsProviderQuickActions.getAction(result.payload.key);
    let inActive = "isActive" in action && !action.isActive();
    return {
      children: [
        {
          tag: "span",
          classList: ["urlbarView-action-btn"],
          attributes: {
            "data-action": result.payload.key,
            "data-input-length": result.payload.inputLength,
            role: inActive ? "" : "button",
            disabled: inActive,
          },
          children: [
            {
              tag: "img",
              attributes: {
                src: action.icon || DEFAULT_ICON,
              },
            },
            {
              name: `label`,
              tag: "span",
            },
          ],
        },
      ],
    };
  }

  getViewUpdate(result) {
    let action = lazy.ActionsProviderQuickActions.getAction(result.payload.key);

    return {
      label: {
        l10n: { id: action.label, cacheable: true },
      },
    };
  }
}

export var UrlbarProviderActionsSearchMode = new ProviderActionsSearchMode();
