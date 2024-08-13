/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const gInitializedWindows = new WeakSet();

export var PopupBlockerObserver = {
  handleEvent(event) {
    switch (event.type) {
      case "DOMUpdateBlockedPopups":
        this.onDOMUpdateBlockedPopups(event);
        break;
      case "command":
        this.onCommand(event);
        break;
      case "popupshowing":
        this.fillPopupList(event);
        break;
      case "popuphiding":
        this.onPopupHiding(event);
        break;
    }
  },

  onCommand(event) {
    if (event.target.hasAttribute("popupReportIndex")) {
      PopupBlockerObserver.showBlockedPopup(event);
      return;
    }

    switch (event.target.id) {
      case "blockedPopupAllowSite":
        this.toggleAllowPopupsForSite(event);
        break;
      case "blockedPopupEdit":
        this.editPopupSettings(event);
        break;
      case "blockedPopupDontShowMessage":
        this.dontShowMessage(event);
        break;
    }
  },

  ensureInitializedForWindow(win) {
    if (gInitializedWindows.has(win)) {
      return;
    }
    gInitializedWindows.add(win);
    let popup = win.document.getElementById("blockedPopupOptions");
    popup.addEventListener("command", this);
    popup.addEventListener("popupshowing", this);
    popup.addEventListener("popuphiding", this);
  },

  async onDOMUpdateBlockedPopups(aEvent) {
    let window = aEvent.originalTarget.ownerGlobal;
    let { gBrowser, gPermissionPanel } = window;
    if (aEvent.originalTarget != gBrowser.selectedBrowser) {
      return;
    }
    gPermissionPanel.refreshPermissionIcons();

    let popupCount =
      gBrowser.selectedBrowser.popupBlocker.getBlockedPopupCount();

    if (!popupCount) {
      // Hide the notification box (if it's visible).
      let notificationBox = gBrowser.getNotificationBox();
      let notification =
        notificationBox.getNotificationWithValue("popup-blocked");
      if (notification) {
        notificationBox.removeNotification(notification, false);
      }
      return;
    }

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (gBrowser.selectedBrowser.popupBlocker.shouldShowNotification) {
      this.ensureInitializedForWindow(window);
      if (Services.prefs.getBoolPref("privacy.popups.showBrowserMessage")) {
        const label = {
          "l10n-id":
            popupCount < this.maxReportedPopups
              ? "popup-warning-message"
              : "popup-warning-exceeded-message",
          "l10n-args": { popupCount },
        };

        let notificationBox = gBrowser.getNotificationBox();
        let notification =
          notificationBox.getNotificationWithValue("popup-blocked") ||
          (await this.notificationPromise);
        if (notification) {
          notification.label = label;
        } else {
          const image = "chrome://browser/skin/notification-icons/popup.svg";
          const priority = notificationBox.PRIORITY_INFO_MEDIUM;
          try {
            this.notificationPromise = notificationBox.appendNotification(
              "popup-blocked",
              { label, image, priority },
              [
                {
                  "l10n-id": "popup-warning-button",
                  popup: "blockedPopupOptions",
                  callback: null,
                },
              ]
            );
            await this.notificationPromise;
          } catch (err) {
            console.warn(err);
          } finally {
            this.notificationPromise = null;
          }
        }
      }

      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      gBrowser.selectedBrowser.popupBlocker.didShowNotification();
    }
  },

  toggleAllowPopupsForSite(aEvent) {
    let window = aEvent.originalTarget.ownerGlobal;
    let { gBrowser } = window;
    var pm = Services.perms;
    var shouldBlock = aEvent.target.getAttribute("block") == "true";
    var perm = shouldBlock ? pm.DENY_ACTION : pm.ALLOW_ACTION;
    pm.addFromPrincipal(gBrowser.contentPrincipal, "popup", perm);

    if (!shouldBlock) {
      gBrowser.selectedBrowser.popupBlocker.unblockAllPopups();
    }

    gBrowser.getNotificationBox().removeCurrentNotification();
  },

  fillPopupList(aEvent) {
    let window = aEvent.originalTarget.ownerGlobal;
    let { gBrowser, document } = window;
    // XXXben - rather than using |currentURI| here, which breaks down on multi-framed sites
    //          we should really walk the blockedPopups and create a list of "allow for <host>"
    //          menuitems for the common subset of hosts present in the report, this will
    //          make us frame-safe.
    //
    // XXXjst - Note that when this is fixed to work with multi-framed sites,
    //          also back out the fix for bug 343772 where
    //          nsGlobalWindow::CheckOpenAllow() was changed to also
    //          check if the top window's location is allow-listed.
    let browser = gBrowser.selectedBrowser;
    var uriOrPrincipal = browser.contentPrincipal.isContentPrincipal
      ? browser.contentPrincipal
      : browser.currentURI;
    var blockedPopupAllowSite = document.getElementById(
      "blockedPopupAllowSite"
    );
    try {
      blockedPopupAllowSite.removeAttribute("hidden");
      let uriHost = uriOrPrincipal.asciiHost
        ? uriOrPrincipal.host
        : uriOrPrincipal.spec;
      var pm = Services.perms;
      if (
        pm.testPermissionFromPrincipal(browser.contentPrincipal, "popup") ==
        pm.ALLOW_ACTION
      ) {
        // Offer an item to block popups for this site, if an allow-list entry exists
        // already for it.
        document.l10n.setAttributes(
          blockedPopupAllowSite,
          "popups-infobar-block",
          { uriHost }
        );
        blockedPopupAllowSite.setAttribute("block", "true");
      } else {
        // Offer an item to allow popups for this site
        document.l10n.setAttributes(
          blockedPopupAllowSite,
          "popups-infobar-allow",
          { uriHost }
        );
        blockedPopupAllowSite.removeAttribute("block");
      }
    } catch (e) {
      blockedPopupAllowSite.hidden = true;
    }

    let blockedPopupDontShowMessage = document.getElementById(
      "blockedPopupDontShowMessage"
    );
    let showMessage = Services.prefs.getBoolPref(
      "privacy.popups.showBrowserMessage"
    );
    blockedPopupDontShowMessage.setAttribute("checked", !showMessage);

    let blockedPopupsSeparator = document.getElementById(
      "blockedPopupsSeparator"
    );
    blockedPopupsSeparator.hidden = true;

    browser.popupBlocker.getBlockedPopups().then(blockedPopups => {
      let foundUsablePopupURI = false;
      if (blockedPopups) {
        for (let i = 0; i < blockedPopups.length; i++) {
          let blockedPopup = blockedPopups[i];

          // popupWindowURI will be null if the file picker popup is blocked.
          // xxxdz this should make the option say "Show file picker" and do it (Bug 590306)
          if (!blockedPopup.popupWindowURISpec) {
            continue;
          }

          var popupURIspec = blockedPopup.popupWindowURISpec;

          // Sometimes the popup URI that we get back from the blockedPopup
          // isn't useful (for instance, netscape.com's popup URI ends up
          // being "http://www.netscape.com", which isn't really the URI of
          // the popup they're trying to show).  This isn't going to be
          // useful to the user, so we won't create a menu item for it.
          if (
            popupURIspec == "" ||
            popupURIspec == "about:blank" ||
            popupURIspec == "<self>" ||
            popupURIspec == uriOrPrincipal.spec
          ) {
            continue;
          }

          // Because of the short-circuit above, we may end up in a situation
          // in which we don't have any usable popup addresses to show in
          // the menu, and therefore we shouldn't show the separator.  However,
          // since we got past the short-circuit, we must've found at least
          // one usable popup URI and thus we'll turn on the separator later.
          foundUsablePopupURI = true;

          var menuitem = document.createXULElement("menuitem");
          document.l10n.setAttributes(menuitem, "popup-show-popup-menuitem", {
            popupURI: popupURIspec,
          });
          menuitem.setAttribute("popupReportIndex", i);
          menuitem.setAttribute(
            "popupInnerWindowId",
            blockedPopup.innerWindowId
          );
          menuitem.browsingContext = blockedPopup.browsingContext;
          menuitem.popupReportBrowser = browser;
          aEvent.target.appendChild(menuitem);
        }
      }

      // Show the separator if we added any
      // showable popup addresses to the menu.
      if (foundUsablePopupURI) {
        blockedPopupsSeparator.removeAttribute("hidden");
      }
    }, null);
  },

  onPopupHiding(aEvent) {
    let item = aEvent.target.lastElementChild;
    while (item && item.id != "blockedPopupsSeparator") {
      let next = item.previousElementSibling;
      item.remove();
      item = next;
    }
  },

  showBlockedPopup(aEvent) {
    let target = aEvent.target;
    let browsingContext = target.browsingContext;
    let innerWindowId = target.getAttribute("popupInnerWindowId");
    let popupReportIndex = target.getAttribute("popupReportIndex");
    let browser = target.popupReportBrowser;
    browser.popupBlocker.unblockPopup(
      browsingContext,
      innerWindowId,
      popupReportIndex
    );
  },

  editPopupSettings(aEvent) {
    let window = aEvent.originalTarget.ownerGlobal;
    let { openPreferences } = window;
    openPreferences("privacy-permissions-block-popups");
  },

  dontShowMessage(aEvent) {
    let window = aEvent.originalTarget.ownerGlobal;
    let { gBrowser } = window;
    var showMessage = Services.prefs.getBoolPref(
      "privacy.popups.showBrowserMessage"
    );
    Services.prefs.setBoolPref(
      "privacy.popups.showBrowserMessage",
      !showMessage
    );
    gBrowser.getNotificationBox().removeCurrentNotification();
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  PopupBlockerObserver,
  "maxReportedPopups",
  "privacy.popups.maxReported"
);
