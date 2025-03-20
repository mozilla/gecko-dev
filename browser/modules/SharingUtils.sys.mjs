/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { BrowserUtils } from "resource://gre/modules/BrowserUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

let lazy = {};

XPCOMUtils.defineLazyServiceGetters(lazy, {
  MacSharingService: [
    "@mozilla.org/widget/macsharingservice;1",
    "nsIMacSharingService",
  ],
  WindowsUIUtils: ["@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils"],
});

class SharingUtilsCls {
  /**
   * Updates a sharing item in a given menu, creating it if necessary.
   */
  updateShareURLMenuItem(browser, insertAfterEl) {
    if (!Services.prefs.getBoolPref("browser.menu.share_url.allow", true)) {
      return;
    }

    // We only support "share URL" on macOS and on Windows:
    if (AppConstants.platform != "macosx" && AppConstants.platform != "win") {
      return;
    }

    let shareURL = insertAfterEl.nextElementSibling;
    if (!shareURL?.matches(".share-tab-url-item")) {
      shareURL = this.#createShareURLMenuItem(insertAfterEl);
    }

    shareURL.browserToShare = Cu.getWeakReference(browser);
    if (AppConstants.platform == "win") {
      // We disable the item on Windows, as there's no submenu.
      // On macOS, we handle this inside the menupopup.
      shareURL.hidden = !BrowserUtils.getShareableURL(browser.currentURI);
    }
  }

  /**
   * Creates and returns the "Share" menu item.
   */
  #createShareURLMenuItem(insertAfterEl) {
    let menu = insertAfterEl.parentNode;
    let shareURL = null;
    let document = insertAfterEl.ownerDocument;
    if (AppConstants.platform == "win") {
      shareURL = this.#buildShareURLItem(document);
    } else if (AppConstants.platform == "macosx") {
      shareURL = this.#buildShareURLMenu(document);
    }
    shareURL.className = "share-tab-url-item";

    let l10nID =
      menu.id == "tabContextMenu"
        ? "tab-context-share-url"
        : "menu-file-share-url";
    document.l10n.setAttributes(shareURL, l10nID);

    menu.insertBefore(shareURL, insertAfterEl.nextSibling);
    return shareURL;
  }

  /**
   * Returns a menu item specifically for accessing Windows sharing services.
   */
  #buildShareURLItem(document) {
    let shareURLMenuItem = document.createXULElement("menuitem");
    shareURLMenuItem.addEventListener("command", this);
    return shareURLMenuItem;
  }

  /**
   * Returns a menu specifically for accessing macOSx sharing services .
   */
  #buildShareURLMenu(document) {
    let menu = document.createXULElement("menu");
    let menuPopup = document.createXULElement("menupopup");
    menuPopup.addEventListener("popupshowing", this);
    menu.appendChild(menuPopup);
    return menu;
  }

  /**
   * Get the sharing data for a given DOM node.
   */
  getDataToShare(node) {
    let browser = node.browserToShare?.get();
    let urlToShare = null;
    let titleToShare = null;

    if (browser) {
      let maybeToShare = BrowserUtils.getShareableURL(browser.currentURI);
      if (maybeToShare) {
        urlToShare = maybeToShare;
        titleToShare = browser.contentTitle;
      }
    }
    return { urlToShare, titleToShare };
  }

  /**
   * Populates the "Share" menupopup on macOSx.
   */
  initializeShareURLPopup(menuPopup) {
    if (AppConstants.platform != "macosx") {
      return;
    }

    // Empty menupopup
    while (menuPopup.firstChild) {
      menuPopup.firstChild.remove();
    }

    let document = menuPopup.ownerDocument;
    let { gURLBar } = menuPopup.ownerGlobal;

    let { urlToShare } = this.getDataToShare(menuPopup.parentNode);

    // If we can't share the current URL, we display the items disabled,
    // but enable the "more..." item at the bottom, to allow the user to
    // change sharing preferences in the system dialog.
    let shouldEnable = !!urlToShare;
    if (!urlToShare) {
      // Fake it so we can ask the sharing service for services:
      urlToShare = Services.io.newURI("https://mozilla.org/");
    }

    let currentURI = gURLBar.makeURIReadable(urlToShare).displaySpec;
    let services = lazy.MacSharingService.getSharingProviders(currentURI);

    services.forEach(share => {
      let item = document.createXULElement("menuitem");
      item.classList.add("menuitem-iconic");
      item.setAttribute("label", share.menuItemTitle);
      item.setAttribute("share-name", share.name);
      item.setAttribute("image", share.image);
      if (!shouldEnable) {
        item.setAttribute("disabled", "true");
      }
      menuPopup.appendChild(item);
    });
    menuPopup.appendChild(document.createXULElement("menuseparator"));
    let moreItem = document.createXULElement("menuitem");
    document.l10n.setAttributes(moreItem, "menu-share-more");
    moreItem.classList.add("menuitem-iconic", "share-more-button");
    menuPopup.appendChild(moreItem);

    menuPopup.addEventListener("command", this);
    menuPopup.parentNode
      .closest("menupopup")
      .addEventListener("popuphiding", this);
    menuPopup.setAttribute("data-initialized", true);
  }

  onShareURLCommand(event) {
    // Only call sharing services for the "Share" menu item. These services
    // are accessed from a submenu popup for MacOS or the "Share" menu item
    // for Windows. Use .closest() as a hack to find either the item itself
    // or a parent with the right class.
    let target = event.target.closest(".share-tab-url-item");
    if (!target) {
      return;
    }
    let { gURLBar } = target.ownerGlobal;

    // urlToShare/titleToShare may be null, in which case only the "more"
    // item is enabled, so handle that case first:
    if (event.target.classList.contains("share-more-button")) {
      lazy.MacSharingService.openSharingPreferences();
      return;
    }

    let { urlToShare, titleToShare } = this.getDataToShare(target);
    let currentURI = gURLBar.makeURIReadable(urlToShare).displaySpec;

    if (AppConstants.platform == "win") {
      lazy.WindowsUIUtils.shareUrl(currentURI, titleToShare);
      return;
    }

    // On macOSX platforms
    let shareName = event.target.getAttribute("share-name");

    if (shareName) {
      lazy.MacSharingService.shareUrl(shareName, currentURI, titleToShare);
    }
  }

  onPopupHiding(event) {
    // We don't want to rebuild the contents of the "Share" menupopup if only its submenu is
    // hidden. So bail if this isn't the top menupopup in the DOM tree:
    if (event.target.parentNode.closest("menupopup")) {
      return;
    }
    // Otherwise, clear its "data-initialized" attribute.
    let menupopup = event.target.querySelector(
      ".share-tab-url-item"
    )?.menupopup;
    menupopup?.removeAttribute("data-initialized");

    event.target.removeEventListener("popuphiding", this);
  }

  onPopupShowing(event) {
    if (!event.target.hasAttribute("data-initialized")) {
      this.initializeShareURLPopup(event.target);
    }
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "command":
        this.onShareURLCommand(aEvent);
        break;
      case "popuphiding":
        this.onPopupHiding(aEvent);
        break;
      case "popupshowing":
        this.onPopupShowing(aEvent);
        break;
    }
  }

  testOnlyMockUIUtils(mock) {
    if (!Cu.isInAutomation) {
      throw new Error("Can only mock utils in automation.");
    }
    // eslint-disable-next-line mozilla/valid-lazy
    Object.defineProperty(lazy, "WindowsUIUtils", {
      get() {
        if (mock) {
          return mock;
        }
        return Cc["@mozilla.org/windows-ui-utils;1"].getService(
          Ci.nsIWindowsUIUtils
        );
      },
    });
  }
}

export let SharingUtils = new SharingUtilsCls();
