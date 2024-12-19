/* This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
  ASRouter: "resource:///modules/asrouter/ASRouter.sys.mjs",
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

export const BookmarksBarButton = {
  async showBookmarksBarButton(browser, message) {
    const { label, action, logo } = message.content;
    let { gBrowser } = browser.ownerGlobal;
    const featureId = "fxms_bmb_button";
    const widgetId = "fxms-bmb-button";
    const supportedActions = ["OPEN_URL", "SET_PREF", "MULTI_ACTION"];

    const fxmsBookmarksBarBtn = {
      id: widgetId,
      l10nId: label?.string_id,
      label: label?.raw,
      tooltiptext: label?.tooltiptext,
      defaultArea: lazy.CustomizableUI.AREA_BOOKMARKS,
      type: "button",

      handleExperimentUpdate() {
        const value = lazy.NimbusFeatures[featureId].getAllVariables() || {};

        if (!Object.keys(value).length) {
          lazy.CustomizableUI.removeWidgetFromArea(widgetId);
        }
      },

      onCreated(aNode) {
        // This surface is for first-run experiments only
        // Once the button is removed by the user or experiment unenrollment, it cannot be added again
        lazy.NimbusFeatures[featureId].onUpdate(this.handleExperimentUpdate);
        aNode.className = `bookmark-item chromeclass-toolbar-additional`;
        if (logo?.imageURL) {
          aNode.style.listStyleImage = `url(${logo.imageURL})`;
        }

        lazy.BrowserUsageTelemetry.recordWidgetChange(
          widgetId,
          lazy.CustomizableUI.AREA_BOOKMARKS,
          "create"
        );
        lazy.ASRouter.addImpression(message);
      },

      onCommand() {
        // Click telemetry is handled in BrowserUsageTelemetry, see
        // _recordCommand()
        if (supportedActions.includes(action.type)) {
          switch (action.type) {
            case "OPEN_URL":
            case "SET_PREF":
              lazy.SpecialMessageActions.handleAction(action, gBrowser);
              break;
            case "MULTI_ACTION":
              if (
                action.data.actions.every(iAction =>
                  supportedActions.includes(iAction.type)
                )
              ) {
                lazy.SpecialMessageActions.handleAction(action, gBrowser);
                break;
              }
          }
        }

        if (action.navigate || action.dismiss) {
          lazy.CustomizableUI.destroyWidget(widgetId);
        }
      },

      onWidgetRemoved() {
        lazy.CustomizableUI.destroyWidget(widgetId);
      },

      onDestroyed() {
        lazy.CustomizableUI.removeListener(this);
        lazy.BrowserUsageTelemetry.recordWidgetChange(
          widgetId,
          null,
          "destroy"
        );
      },
    };

    lazy.CustomizableUI.addListener(fxmsBookmarksBarBtn);
    lazy.CustomizableUI.createWidget(fxmsBookmarksBarBtn);
  },
};
