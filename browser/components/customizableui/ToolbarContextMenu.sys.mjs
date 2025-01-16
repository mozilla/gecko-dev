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

/**
 * Various events handlers to set the state of the toolbar-context-menu popup,
 * as well as to handle some commands from that popup.
 */
export var ToolbarContextMenu = {
  /**
   * Makes visible the "autohide the downloads button" checkbox in the popup
   * in the event that the downloads button was context clicked. Otherwise,
   * hides that checkbox.
   *
   * This method also sets the checkbox state depending on the current user
   * configuration for hiding the downloads button.
   *
   * @param {Element} popup
   *   The toolbar-context-menu element for a window.
   */
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

  /**
   * Handler for the toolbar-context-autohide-downloads-button command event
   * that is fired when the checkbox for autohiding the downloads button is
   * changed. This method does the work of updating the internal preference
   * state for auto-hiding the downloads button.
   *
   * @param {CommandEvent} event
   */
  onDownloadsAutoHideChange(event) {
    let autoHide = event.target.getAttribute("checked") == "true";
    Services.prefs.setBoolPref("browser.download.autohideButton", autoHide);
  },

  /**
   * Makes visible the "always open downloads panel" checkbox in the popup
   * in the event that the downloads button was context clicked. Otherwise,
   * hides that checkbox.
   *
   * This method also sets the checkbox state depending on the current user
   * configuration for always showing the panel.
   *
   * @param {Element} popup
   *   The toolbar-context-menu element for a window.
   */
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

  /**
   * Handler for the toolbar-context-always-open-downloads-panel command event
   * that is fired when the checkbox for always showing the downloads panel is
   * changed. This method does the work of updating the internal preference
   * state for always showing the downloads panel.
   *
   * @param {CommandEvent} event
   */
  onDownloadsAlwaysOpenPanelChange(event) {
    let alwaysOpen = event.target.getAttribute("checked") == "true";
    Services.prefs.setBoolPref("browser.download.alwaysOpenPanel", alwaysOpen);
  },

  /**
   * This is called when a menupopup for configuring toolbars fires its
   * popupshowing event. There are multiple such menupopups, and this logic
   * tries to work for all of them. This method will insert menuitems into
   * the popup to allow for controlling the toolbars within the browser
   * toolbox.
   *
   * @param {Event} aEvent
   *   The popupshowing event for the menupopup.
   * @param {DOMNode} aInsertPoint
   *   The point within the menupopup to insert the controls for each toolbar.
   */
  onViewToolbarsPopupShowing(aEvent, aInsertPoint) {
    var popup = aEvent.target;
    let window = popup.ownerGlobal;
    let {
      document,
      BookmarkingUI,
      MozXULElement,
      onViewToolbarCommand,
      showFullScreenViewContextMenuItems,
      gBrowser,
      CustomizationHandler,
      gNavToolbox,
    } = window;

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

    if (aInsertPoint) {
      aInsertPoint.hidden = isVerticalTabStripMenu;
    }
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
      (toolbarItem?.localName.includes("separator") ||
        toolbarItem?.localName.includes("spring") ||
        toolbarItem?.localName.includes("spacer") ||
        toolbarItem?.id.startsWith("customizableui-special"))
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
      let closedCount = lazy.SessionStore.getLastClosedTabCount(window);
      document
        .getElementById("History:UndoCloseTab")
        .setAttribute("disabled", closedCount == 0);
      document.l10n.setArgs(
        document.getElementById("toolbar-context-undoCloseTab"),
        { tabCount: closedCount }
      );
      return;
    }

    let movable =
      toolbarItem?.id && lazy.CustomizableUI.isWidgetRemovable(toolbarItem);
    if (movable) {
      if (lazy.CustomizableUI.isSpecialWidget(toolbarItem.id)) {
        moveToPanel.setAttribute("disabled", true);
      } else {
        moveToPanel.removeAttribute("disabled");
      }
      removeFromToolbar.removeAttribute("disabled");
    } else {
      removeFromToolbar.setAttribute("disabled", true);
      moveToPanel.setAttribute("disabled", true);
    }
  },

  /**
   * Given an opened menupopup, returns the triggerNode that opened that
   * menupopup. If customize mode is enabled, this will return the unwrapped
   * underlying triggerNode, rather than the customize mode wrapper around it.
   *
   * @param {DOMNode} popup
   *   The menupopup to get the unwrapped trigger node for.
   * @returns {DOMNode}
   *   The underlying trigger node that opened the menupopup.
   */
  _getUnwrappedTriggerNode(popup) {
    // Toolbar buttons are wrapped in customize mode. Unwrap if necessary.
    let { triggerNode } = popup;
    let { gCustomizeMode } = popup.ownerGlobal;
    if (triggerNode && gCustomizeMode.isWrappedToolbarItem(triggerNode)) {
      return triggerNode.firstElementChild;
    }
    return triggerNode;
  },

  /**
   * For an opened menupopup, if the triggerNode was provided by an extension,
   * returns the extension ID. Otherwise, return the empty string.
   *
   * @param {DOMNode} popup
   *   The menupopup that was opened.
   * @returns {string}
   *   The ID of the extension that provided the triggerNode, or the empty
   *   string if the triggerNode was not provided by an extension.
   */
  _getExtensionId(popup) {
    let node = this._getUnwrappedTriggerNode(popup);
    return node && node.getAttribute("data-extensionid");
  },

  /**
   * For an opened menupopup, if the triggerNode was provided by an extension,
   * returns the widget ID of the triggerNode. Otherwise, return the empty
   * string.
   *
   * @param {DOMNode} popup
   *   The menupopup that was opened.
   * @returns {string}
   *   The ID of the extension-provided widget that was the triggerNode, or the
   *   empty string if the trigger node was not provided by an extension
   *   widget.
   */
  _getWidgetId(popup) {
    let node = this._getUnwrappedTriggerNode(popup);
    return node?.closest(".unified-extensions-item")?.id;
  },

  /**
   * Updates the toolbar context menu to show the right state if an
   * extension-provided widget acted as the triggerNode. This will, for example,
   * show or hide items for managing the underlying addon.
   *
   * @param {DOMNode} popup
   *   The menupopup for the toolbar context menu.
   * @param {Event} event
   *   The popupshowing event for the menupopup.
   * @returns {Promise<undefined>}
   *   Resolves once the menupopup state has been set.
   */
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

  /**
   * Handler for the context menu item for removing an extension.
   *
   * @param {DOMNode} popup
   *   The menupopup that triggered the extension removal.
   * @returns {Promise<undefined>}
   *   Resolves when the extension has been removed.
   */
  async removeExtensionForContextAction(popup) {
    let { BrowserAddonUI } = popup.ownerGlobal;

    let id = this._getExtensionId(popup);
    await BrowserAddonUI.removeAddon(id, "browserAction");
  },

  /**
   * Handler for the context menu item for issuing a report on an extension.
   *
   * @param {DOMNode} popup
   *   The menupopup that triggered the extension report.
   * @param {string} reportEntryPoint
   *   A string describing the UI entrypoint for the report.
   * @returns {Promise<undefined>}
   *   Resolves when the extension has been removed.
   */
  async reportExtensionForContextAction(popup, reportEntryPoint) {
    let { BrowserAddonUI } = popup.ownerGlobal;
    let id = this._getExtensionId(popup);
    await BrowserAddonUI.reportAddon(id, reportEntryPoint);
  },

  /**
   * Handler for the context menu item for managing an extension.
   *
   * @param {DOMNode} popup
   *   The menupopup that triggered extension management.
   * @returns {Promise<undefined>}
   *   Resolves when the extension's about:addons management page has been
   *   opened.
   */
  async openAboutAddonsForContextAction(popup) {
    let { BrowserAddonUI } = popup.ownerGlobal;
    let id = this._getExtensionId(popup);
    await BrowserAddonUI.manageAddon(id, "browserAction");
  },
};
