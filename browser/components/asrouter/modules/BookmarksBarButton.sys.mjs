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
});

export const BookmarksBarButton = {
  async showBookmarksBarButton(browser, message) {
    const { label, action } = message.content;
    let { gBrowser } = browser.ownerGlobal;
    const surfaceName = "fxms-bmb-button";
    const widgetId = message.id;

    const fxmsBookmarksBarBtn = {
      id: widgetId,
      l10nId: label?.string_id,
      label: label?.raw,
      tooltiptext: label?.tooltiptext,
      defaultArea: lazy.CustomizableUI.AREA_BOOKMARKS,
      type: "button",

      onCreated(aNode) {
        aNode.className = `bookmark-item chromeclass-toolbar-additional ${surfaceName}`;

        lazy.BrowserUsageTelemetry.recordWidgetChange(
          widgetId,
          lazy.CustomizableUI.AREA_BOOKMARKS,
          "create"
        );
        lazy.ASRouter.addImpression(message);
      },

      onCommand() {
        // Currently only supports OPEN_URL action
        if (action.type === "OPEN_URL") {
          // Click telemetry is handled in BrowserUsageTelemetry, see
          // _recordCommand()
          lazy.SpecialMessageActions.handleAction(action, gBrowser);
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
