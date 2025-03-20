/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { BrowserUtils } from "resource://gre/modules/BrowserUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const APPLE_COPY_LINK = "com.apple.share.CopyLink.invite";

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

    let shareURL = insertAfterEl.nextElementSibling;
    if (!shareURL?.matches(".share-tab-url-item")) {
      shareURL = this.#createShareURLMenuItem(insertAfterEl);
    }

    shareURL.browserToShare = Cu.getWeakReference(browser);
    if (AppConstants.platform != "macosx") {
      // On macOS, we keep the item enabled and handle enabled state
      // inside the menupopup.
      // Everywhere else, we disable the item, as there's no submenu.
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
    if (AppConstants.platform != "win" && AppConstants.platform != "macosx") {
      shareURL = this.#buildCopyLinkItem(document);
    } else {
      if (AppConstants.platform == "win") {
        shareURL = this.#buildShareURLItem(document);
      } else if (AppConstants.platform == "macosx") {
        shareURL = this.#buildShareURLMenu(document);
      }
      let l10nID =
        menu.id == "tabContextMenu"
          ? "tab-context-share-url"
          : "menu-file-share-url";
      document.l10n.setAttributes(shareURL, l10nID);
    }
    shareURL.classList.add("share-tab-url-item");

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
   * Return a menuitem that only copies the link. Useful for
   * OSes where we do not yet have full share support, like Linux.
   *
   * We currently also use this on macOS because for some reason Apple does not
   * provide the share service option for this.
   */
  #buildCopyLinkItem(document) {
    let shareURLMenuItem = document.createXULElement("menuitem");
    document.l10n.setAttributes(shareURLMenuItem, "menu-share-copy-link");
    shareURLMenuItem.classList.add("share-copy-link");

    if (AppConstants.platform == "macosx") {
      shareURLMenuItem.classList.add("menuitem-iconic");
      shareURLMenuItem.setAttribute(
        "image",
        "chrome://global/skin/icons/link.svg"
      );
    } else {
      // On macOS the command handling happens by virtue of the submenu
      // command event listener.
      shareURLMenuItem.addEventListener("command", this);
    }
    return shareURLMenuItem;
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

    // Apple seems reluctant to provide copy link as a feature. Add it at the
    // start if it's not there.
    if (!services.some(s => s.name == APPLE_COPY_LINK)) {
      let item = this.#buildCopyLinkItem(document);
      if (!shouldEnable) {
        item.setAttribute("disabled", "true");
      }
      menuPopup.appendChild(item);
    }

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

    if (event.target.classList.contains("share-copy-link")) {
      BrowserUtils.copyLink(currentURI, titleToShare);
    } else if (AppConstants.platform == "win") {
      lazy.WindowsUIUtils.shareUrl(currentURI, titleToShare);
    } else {
      // On macOSX platforms
      let shareName = event.target.getAttribute("share-name");
      if (shareName) {
        lazy.MacSharingService.shareUrl(shareName, currentURI, titleToShare);
      }
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
