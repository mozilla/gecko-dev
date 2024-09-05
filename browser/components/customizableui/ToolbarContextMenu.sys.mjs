/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  ExtensionsUI: "resource:///modules/ExtensionsUI.sys.mjs",
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gAlwaysOpenPanel",
  "browser.download.alwaysOpenPanel",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gAddonAbuseReportEnabled",
  "extensions.abuseReport.enabled",
  false
);

export var ToolbarContextMenu = {
  updateDownloadsAutoHide(popup) {
    let { document, DownloadsButton } = popup.ownerGlobal;
    let checkbox = document.getElementById(
      "toolbar-context-autohide-downloads-button"
    );
    let isDownloads =
      popup.triggerNode &&
      ["downloads-button", "wrapper-downloads-button"].includes(
        popup.triggerNode.id
      );
    checkbox.hidden = !isDownloads;
    if (DownloadsButton.autoHideDownloadsButton) {
      checkbox.setAttribute("checked", "true");
    } else {
      checkbox.removeAttribute("checked");
    }
  },

  onDownloadsAutoHideChange(event) {
    let autoHide = event.target.getAttribute("checked") == "true";
    Services.prefs.setBoolPref("browser.download.autohideButton", autoHide);
  },

  updateDownloadsAlwaysOpenPanel(popup) {
    let { document } = popup.ownerGlobal;
    let separator = document.getElementById(
      "toolbarDownloadsAnchorMenuSeparator"
    );
    let checkbox = document.getElementById(
      "toolbar-context-always-open-downloads-panel"
    );
    let isDownloads =
      popup.triggerNode &&
      ["downloads-button", "wrapper-downloads-button"].includes(
        popup.triggerNode.id
      );
    separator.hidden = checkbox.hidden = !isDownloads;
    lazy.gAlwaysOpenPanel
      ? checkbox.setAttribute("checked", "true")
      : checkbox.removeAttribute("checked");
  },

  onDownloadsAlwaysOpenPanelChange(event) {
    let alwaysOpen = event.target.getAttribute("checked") == "true";
    Services.prefs.setBoolPref("browser.download.alwaysOpenPanel", alwaysOpen);
  },

  onViewToolbarsPopupShowing(aEvent, aInsertPoint) {
    var popup = aEvent.target;
    let {
      document,
      BookmarkingUI,
      MozXULElement,
      onViewToolbarCommand,
      showFullScreenViewContextMenuItems,
      gBrowser,
      CustomizationHandler,
      gNavToolbox,
    } = popup.ownerGlobal;

    // triggerNode can be a nested child element of a toolbaritem.
    let toolbarItem = popup.triggerNode;
    while (toolbarItem) {
      let localName = toolbarItem.localName;
      if (localName == "toolbar") {
        toolbarItem = null;
        break;
      }
      if (localName == "toolbarpaletteitem") {
        toolbarItem = toolbarItem.firstElementChild;
        break;
      }
      if (localName == "menupopup") {
        aEvent.preventDefault();
        aEvent.stopPropagation();
        return;
      }
      let parent = toolbarItem.parentElement;
      if (parent) {
        if (
          parent.classList.contains("customization-target") ||
          parent.getAttribute("overflowfortoolbar") || // Needs to work in the overflow list as well.
          parent.localName == "toolbarpaletteitem" ||
          parent.localName == "toolbar" ||
          parent.id == "vertical-tabs"
        ) {
          break;
        }
      }
      toolbarItem = parent;
    }

    // Empty the menu
    for (var i = popup.children.length - 1; i >= 0; --i) {
      var deadItem = popup.children[i];
      if (deadItem.hasAttribute("toolbarId")) {
        popup.removeChild(deadItem);
      }
    }

    let showTabStripItems = toolbarItem?.id == "tabbrowser-tabs";
    let isVerticalTabStripMenu =
      showTabStripItems && toolbarItem.parentElement.id == "vertical-tabs";

    aInsertPoint.hidden = isVerticalTabStripMenu;
    document.getElementById("toolbar-context-customize").hidden =
      isVerticalTabStripMenu;

    if (!isVerticalTabStripMenu) {
      MozXULElement.insertFTLIfNeeded("browser/toolbarContextMenu.ftl");
      let firstMenuItem = aInsertPoint || popup.firstElementChild;
      let toolbarNodes = gNavToolbox.querySelectorAll("toolbar");
      for (let toolbar of toolbarNodes) {
        if (!toolbar.hasAttribute("toolbarname")) {
          continue;
        }

        if (toolbar.id == "PersonalToolbar") {
          let menu = BookmarkingUI.buildBookmarksToolbarSubmenu(toolbar);
          popup.insertBefore(menu, firstMenuItem);
        } else {
          let menuItem = document.createXULElement("menuitem");
          menuItem.setAttribute("id", "toggle_" + toolbar.id);
          menuItem.setAttribute("toolbarId", toolbar.id);
          menuItem.setAttribute("type", "checkbox");
          menuItem.setAttribute("label", toolbar.getAttribute("toolbarname"));
          let hidingAttribute =
            toolbar.getAttribute("type") == "menubar"
              ? "autohide"
              : "collapsed";
          menuItem.setAttribute(
            "checked",
            toolbar.getAttribute(hidingAttribute) != "true"
          );
          menuItem.setAttribute("accesskey", toolbar.getAttribute("accesskey"));
          if (popup.id != "toolbar-context-menu") {
            menuItem.setAttribute("key", toolbar.getAttribute("key"));
          }

          popup.insertBefore(menuItem, firstMenuItem);
          menuItem.addEventListener("command", onViewToolbarCommand);
        }
      }
    }

    let moveToPanel = popup.querySelector(".customize-context-moveToPanel");
    let removeFromToolbar = popup.querySelector(
      ".customize-context-removeFromToolbar"
    );
    // Show/hide fullscreen context menu items and set the
    // autohide item's checked state to mirror the autohide pref.
    showFullScreenViewContextMenuItems(popup);
    // View -> Toolbars menu doesn't have the moveToPanel or removeFromToolbar items.
    if (!moveToPanel || !removeFromToolbar) {
      return;
    }

    for (let node of popup.querySelectorAll(
      'menuitem[contexttype="toolbaritem"]'
    )) {
      node.hidden = showTabStripItems;
    }

    for (let node of popup.querySelectorAll('menuitem[contexttype="tabbar"]')) {
      node.hidden = !showTabStripItems;
    }

    document
      .getElementById("toolbar-context-menu")
      .querySelectorAll("[data-lazy-l10n-id]")
      .forEach(el => {
        el.setAttribute("data-l10n-id", el.getAttribute("data-lazy-l10n-id"));
        el.removeAttribute("data-lazy-l10n-id");
      });

    // The "normal" toolbar items menu separator is hidden because it's unused
    // when hiding the "moveToPanel" and "removeFromToolbar" items on flexible
    // space items. But we need to ensure its hidden state is reset in the case
    // the context menu is subsequently opened on a non-flexible space item.
    let menuSeparator = document.getElementById("toolbarItemsMenuSeparator");
    menuSeparator.hidden = false;

    document.getElementById("toolbarNavigatorItemsMenuSeparator").hidden =
      !showTabStripItems;

    if (
      !CustomizationHandler.isCustomizing() &&
      lazy.CustomizableUI.isSpecialWidget(toolbarItem?.id || "")
    ) {
      moveToPanel.hidden = true;
      removeFromToolbar.hidden = true;
      menuSeparator.hidden = !showTabStripItems;
    }

    if (showTabStripItems) {
      let multipleTabsSelected = !!gBrowser.multiSelectedTabsCount;
      document.getElementById("toolbar-context-bookmarkSelectedTabs").hidden =
        !multipleTabsSelected;
      document.getElementById("toolbar-context-bookmarkSelectedTab").hidden =
        multipleTabsSelected;
      document.getElementById("toolbar-context-reloadSelectedTabs").hidden =
        !multipleTabsSelected;
      document.getElementById("toolbar-context-reloadSelectedTab").hidden =
        multipleTabsSelected;
      document.getElementById("toolbar-context-selectAllTabs").disabled =
        gBrowser.allTabsSelected();
      document.getElementById("toolbar-context-undoCloseTab").disabled =
        lazy.SessionStore.getClosedTabCount() == 0;
      return;
    }

    // fxms-bmb-button is a Firefox Messaging System Bookmarks bar button.
    let removable = !toolbarItem?.classList?.contains("fxms-bmb-button");
    let movable =
      toolbarItem?.id &&
      removable &&
      !toolbarItem?.classList?.contains("fxms-bmb-button") &&
      lazy.CustomizableUI.isWidgetRemovable(toolbarItem);
    if (movable) {
      if (lazy.CustomizableUI.isSpecialWidget(toolbarItem.id)) {
        moveToPanel.setAttribute("disabled", true);
      } else {
        moveToPanel.removeAttribute("disabled");
      }
      removeFromToolbar.removeAttribute("disabled");
    } else {
      if (removable) {
        removeFromToolbar.setAttribute("disabled", true);
      }
      moveToPanel.setAttribute("disabled", true);
    }
  },

  _getUnwrappedTriggerNode(popup) {
    // Toolbar buttons are wrapped in customize mode. Unwrap if necessary.
    let { triggerNode } = popup;
    let { gCustomizeMode } = popup.ownerGlobal;
    if (triggerNode && gCustomizeMode.isWrappedToolbarItem(triggerNode)) {
      return triggerNode.firstElementChild;
    }
    return triggerNode;
  },

  _getExtensionId(popup) {
    let node = this._getUnwrappedTriggerNode(popup);
    return node && node.getAttribute("data-extensionid");
  },

  _getWidgetId(popup) {
    let node = this._getUnwrappedTriggerNode(popup);
    return node?.closest(".unified-extensions-item")?.id;
  },

  async updateExtension(popup, event) {
    let removeExtension = popup.querySelector(
      ".customize-context-removeExtension"
    );
    let manageExtension = popup.querySelector(
      ".customize-context-manageExtension"
    );
    let reportExtension = popup.querySelector(
      ".customize-context-reportExtension"
    );
    let pinToToolbar = popup.querySelector(".customize-context-pinToToolbar");
    let separator = reportExtension.nextElementSibling;
    let id = this._getExtensionId(popup);
    let addon = id && (await lazy.AddonManager.getAddonByID(id));

    for (let element of [removeExtension, manageExtension, separator]) {
      element.hidden = !addon;
    }

    if (pinToToolbar) {
      pinToToolbar.hidden = !addon;
    }

    reportExtension.hidden = !addon || !lazy.gAddonAbuseReportEnabled;

    if (addon) {
      popup.querySelector(".customize-context-moveToPanel").hidden = true;
      popup.querySelector(".customize-context-removeFromToolbar").hidden = true;

      if (pinToToolbar) {
        let widgetId = this._getWidgetId(popup);
        if (widgetId) {
          let area = lazy.CustomizableUI.getPlacementOfWidget(widgetId).area;
          let inToolbar = area != lazy.CustomizableUI.AREA_ADDONS;
          pinToToolbar.setAttribute("checked", inToolbar);
        }
      }

      removeExtension.disabled = !(
        addon.permissions & lazy.AddonManager.PERM_CAN_UNINSTALL
      );

      if (event?.target?.id === "toolbar-context-menu") {
        lazy.ExtensionsUI.originControlsMenu(popup, id);
      }
    }
  },

  async removeExtensionForContextAction(popup) {
    let { BrowserAddonUI } = popup.ownerGlobal;

    let id = this._getExtensionId(popup);
    await BrowserAddonUI.removeAddon(id, "browserAction");
  },

  async reportExtensionForContextAction(popup, reportEntryPoint) {
    let { BrowserAddonUI } = popup.ownerGlobal;
    let id = this._getExtensionId(popup);
    await BrowserAddonUI.reportAddon(id, reportEntryPoint);
  },

  async openAboutAddonsForContextAction(popup) {
    let { BrowserAddonUI } = popup.ownerGlobal;
    let id = this._getExtensionId(popup);
    await BrowserAddonUI.manageAddon(id, "browserAction");
  },
};
