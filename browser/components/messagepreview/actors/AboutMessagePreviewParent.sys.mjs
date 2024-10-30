/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ASRouter } from "resource:///modules/asrouter/ASRouter.sys.mjs";
import { JsonSchema } from "resource://gre/modules/JsonSchema.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  BookmarksBarButton: "resource:///modules/asrouter/BookmarksBarButton.sys.mjs",
  CFRPageActions: "resource:///modules/asrouter/CFRPageActions.sys.mjs",
  FeatureCalloutBroker:
    "resource:///modules/asrouter/FeatureCalloutBroker.sys.mjs",
  InfoBar: "resource:///modules/asrouter/InfoBar.sys.mjs",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
  Spotlight: "resource:///modules/asrouter/Spotlight.sys.mjs",
});

const SWITCH_THEMES = {
  DARK: "firefox-compact-dark@mozilla.org",
  LIGHT: "firefox-compact-light@mozilla.org",
};

function dispatchCFRAction({ type, data }, browser) {
  if (type === "USER_ACTION") {
    lazy.SpecialMessageActions.handleAction(data, browser);
  }
}

export class AboutMessagePreviewParent extends JSWindowActorParent {
  constructor() {
    super();

    const EXISTING_THEME = Services.prefs.getStringPref(
      "extensions.activeThemeID"
    );

    this._onUnload = () => {
      lazy.AddonManager.getAddonByID(EXISTING_THEME).then(addon =>
        addon.enable()
      );
    };
  }

  didDestroy() {
    this._onUnload();
  }

  showInfoBar(message, browser) {
    lazy.InfoBar.showInfoBarMessage(browser, message, dispatchCFRAction);
  }

  showSpotlight(message, browser) {
    lazy.Spotlight.showSpotlightDialog(browser, message, () => {});
  }

  showBookmarksBarButton(message, browser) {
    lazy.BookmarksBarButton.showBookmarksBarButton(message, browser);
  }

  showCFR(message, browser) {
    lazy.CFRPageActions.forceRecommendation(
      browser,
      message,
      dispatchCFRAction
    );
  }

  showPrivateBrowsingMessage(message, browser) {
    ASRouter.forcePBWindow(browser, message);
  }

  async showFeatureCallout(message, browser) {
    // Clear the Feature Tour prefs used by some callouts, to ensure
    // the behaviour of the message is correct
    let tourPref = message.content.tour_pref_name;
    if (tourPref) {
      Services.prefs.clearUserPref(tourPref);
    }
    // For messagePreview, force the trigger && targeting to be something we can show.
    message.trigger = { id: "nthTabClosed" };
    message.targeting = "true";
    // Check whether or not the callout is showing already, then
    // modify the anchor property of the feature callout to
    // ensure it's something we can show.
    let showing = await lazy.FeatureCalloutBroker.showFeatureCallout(
      browser,
      message
    );
    if (!showing) {
      for (const screen of message.content.screens) {
        let existingAnchors = screen.anchors;
        let fallbackAnchor = { selector: "#star-button-box" };

        if (existingAnchors[0].hasOwnProperty("arrow_position")) {
          fallbackAnchor.arrow_position = "top-center-arrow-end";
        } else {
          fallbackAnchor.panel_position = {
            anchor_attachment: "bottomcenter",
            callout_attachment: "topright",
          };
        }

        screen.anchors = [...existingAnchors, fallbackAnchor];
        console.log("ANCHORS: ", screen.anchors);
      }
      // Try showing again
      await lazy.FeatureCalloutBroker.showFeatureCallout(browser, message);
    }
  }

  async showMessage(data) {
    let message;
    try {
      message = JSON.parse(data);
    } catch (e) {
      console.error("Could not parse message", e);
      return;
    }

    const schema = await fetch(
      "chrome://browser/content/asrouter/schemas/MessagingExperiment.schema.json",
      { credentials: "omit" }
    ).then(rsp => rsp.json());

    const result = JsonSchema.validate(message, schema);
    if (!result.valid) {
      console.error(
        `Invalid message: ${JSON.stringify(result.errors, undefined, 2)}`
      );
    }

    const browser =
      this.browsingContext.topChromeWindow.gBrowser.selectedBrowser;
    switch (message.template) {
      case "infobar":
        this.showInfoBar(message, browser);
        return;
      case "spotlight":
        this.showSpotlight(message, browser);
        return;
      case "cfr_doorhanger":
        this.showCFR(message, browser);
        return;
      case "feature_callout":
        this.showFeatureCallout(message, browser);
        return;
      case "bookmarks_bar_button":
        this.showBookmarksBarButton(message, browser);
        return;
      case "pb_newtab":
        this.showPrivateBrowsingMessage(message, browser);
        return;
      default:
        console.error(`Unsupported message template ${message.template}`);
    }
  }

  receiveMessage(message) {
    const { name, data } = message;

    switch (name) {
      case "MessagePreview:SHOW_MESSAGE":
        this.showMessage(data);
        return;
      case "MessagePreview:CHANGE_THEME": {
        const theme = data.isDark ? SWITCH_THEMES.LIGHT : SWITCH_THEMES.DARK;
        lazy.AddonManager.getAddonByID(theme).then(addon => addon.enable());
        return;
      }
      default:
        console.log(`Unexpected event ${name} was not handled.`);
    }
  }
}
