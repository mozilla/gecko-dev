// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ "ReaderParent" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils", "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITour", "resource:///modules/UITour.jsm");

const gStringBundle = Services.strings.createBundle("chrome://global/locale/aboutReader.properties");

var ReaderParent = {
  _readerModeInfoPanelOpen: false,

  MESSAGES: [
    "Reader:ArticleGet",
    "Reader:FaviconRequest",
    "Reader:UpdateReaderButton",
  ],

  init: function() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:ArticleGet":
        this._getArticle(message.data.url, message.target).then((article) => {
          // Make sure the target browser is still alive before trying to send data back.
          if (message.target.messageManager) {
            message.target.messageManager.sendAsyncMessage("Reader:ArticleData", { article: article });
          }
        }, e => {
          if (e && e.newURL) {
            // Make sure the target browser is still alive before trying to send data back.
            if (message.target.messageManager) {
              message.target.messageManager.sendAsyncMessage("Reader:ArticleData", { newURL: e.newURL });
            }
          }
        });
        break;

      case "Reader:FaviconRequest": {
        if (message.target.messageManager) {
          let faviconUrl = PlacesUtils.promiseFaviconLinkUrl(message.data.url);
          faviconUrl.then(function onResolution(favicon) {
            message.target.messageManager.sendAsyncMessage("Reader:FaviconReturn", {
              url: message.data.url,
              faviconUrl: favicon.path.replace(/^favicon:/, "")
            })
          },
          function onRejection(reason) {
            Cu.reportError("Error requesting favicon URL for about:reader content: " + reason);
          }).catch(Cu.reportError);
        }
        break;
      }

      case "Reader:UpdateReaderButton": {
        let browser = message.target;
        if (message.data && message.data.isArticle !== undefined) {
          browser.isArticle = message.data.isArticle;
        }
        this.updateReaderButton(browser);
        break;
      }
    }
  },

  updateReaderButton: function(browser) {
    let win = browser.ownerGlobal;
    if (browser != win.gBrowser.selectedBrowser) {
      return;
    }

    let button = win.document.getElementById("reader-mode-button");
    let command = win.document.getElementById("View:ReaderView");
    let key = win.document.getElementById("toggleReaderMode");
    if (browser.currentURI.spec.startsWith("about:reader")) {
      button.setAttribute("readeractive", true);
      button.hidden = false;
      let closeText = gStringBundle.GetStringFromName("readerView.close");
      button.setAttribute("tooltiptext", closeText);
      command.setAttribute("label", closeText);
      command.setAttribute("hidden", false);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.close.accesskey"));
      key.setAttribute("disabled", false);
    } else {
      button.removeAttribute("readeractive");
      button.hidden = !browser.isArticle;
      let enterText = gStringBundle.GetStringFromName("readerView.enter");
      button.setAttribute("tooltiptext", enterText);
      command.setAttribute("label", enterText);
      command.setAttribute("hidden", !browser.isArticle);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.enter.accesskey"));
      key.setAttribute("disabled", !browser.isArticle);
    }

    let currentUriHost = browser.currentURI && browser.currentURI.asciiHost;
    if (browser.isArticle &&
        !Services.prefs.getBoolPref("browser.reader.detectedFirstArticle") &&
        currentUriHost && !currentUriHost.endsWith("mozilla.org")) {
      this.showReaderModeInfoPanel(browser);
      Services.prefs.setBoolPref("browser.reader.detectedFirstArticle", true);
      this._readerModeInfoPanelOpen = true;
    } else if (this._readerModeInfoPanelOpen) {
      if (UITour.isInfoOnTarget(win, "readerMode-urlBar")) {
        UITour.hideInfo(win);
      }
      this._readerModeInfoPanelOpen = false;
    }
  },

  forceShowReaderIcon: function(browser) {
    browser.isArticle = true;
    this.updateReaderButton(browser);
  },

  buttonClick(event) {
    if (event.button != 0) {
      return;
    }
    this.toggleReaderMode(event);
  },

  toggleReaderMode: function(event) {
    let win = event.target.ownerGlobal;
    let browser = win.gBrowser.selectedBrowser;
    browser.messageManager.sendAsyncMessage("Reader:ToggleReaderMode");
  },

  /**
   * Shows an info panel from the UITour for Reader Mode.
   *
   * @param browser The <browser> that the tour should be started for.
   */
  showReaderModeInfoPanel(browser) {
    let win = browser.ownerGlobal;
    let targetPromise = UITour.getTarget(win, "readerMode-urlBar");
    targetPromise.then(target => {
      let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
      let icon = "chrome://browser/skin/";
      if (win.devicePixelRatio > 1) {
        icon += "reader-tour@2x.png";
      } else {
        icon += "reader-tour.png";
      }
      UITour.showInfo(win, target,
                      browserBundle.GetStringFromName("readingList.promo.firstUse.readerView.title"),
                      browserBundle.GetStringFromName("readingList.promo.firstUse.readerView.body"),
                      icon);
    });
  },

  /**
   * Gets an article for a given URL. This method will download and parse a document.
   *
   * @param url The article URL.
   * @param browser The browser where the article is currently loaded.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  _getArticle: Task.async(function* (url, browser) {
    return yield ReaderMode.downloadAndParseDocument(url).catch(e => {
      if (e && e.newURL) {
        // Pass up the error so we can navigate the browser in question to the new URL:
        throw e;
      }
      Cu.reportError("Error downloading and parsing document: " + e);
      return null;
    });
  })
};
