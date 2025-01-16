/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "l10n", () => {
  return new Localization(["browser/recentlyClosed.ftl"], true);
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "closedTabsFromAllWindowsEnabled",
  "browser.sessionstore.closedTabsFromAllWindows"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "closedTabsFromClosedWindowsEnabled",
  "browser.sessionstore.closedTabsFromClosedWindows"
);

/**
 * @param {Window} win
 * @returns {Map<string, TabGroupStateData>}
 *   Map of closed tab groups keyed by tab group ID
 */
function getClosedTabGroupsById(win) {
  const closedTabGroups = lazy.SessionStore.getClosedTabGroups({
    sourceWindow: win,
    closedTabsFromClosedWindows: false,
  });
  const closedTabGroupsById = new Map();
  closedTabGroups.forEach(tabGroup =>
    closedTabGroupsById.set(tabGroup.id, tabGroup)
  );
  return closedTabGroupsById;
}

export var RecentlyClosedTabsAndWindowsMenuUtils = {
  /**
   * Builds up a document fragment of UI items for the recently closed tabs.
   * @param   {Window} aWindow
   *          The window that the tabs were closed in.
   * @param   {"menuitem"|"toolbarbutton"} aTagName
   *          The tag name that will be used when creating the UI items.
   * @returns {DocumentFragment} A document fragment with UI items for each recently closed tab.
   */
  getTabsFragment(aWindow, aTagName) {
    let doc = aWindow.document;
    const isPrivate = lazy.PrivateBrowsingUtils.isWindowPrivate(aWindow);
    const fragment = doc.createDocumentFragment();
    let isEmpty = true;

    if (
      lazy.SessionStore.getClosedTabCount({
        sourceWindow: aWindow,
        closedTabsFromClosedWindows: false,
      })
    ) {
      isEmpty = false;
      const browserWindows = lazy.closedTabsFromAllWindowsEnabled
        ? lazy.SessionStore.getWindows(aWindow)
        : [aWindow];

      for (const win of browserWindows) {
        const closedTabs = lazy.SessionStore.getClosedTabDataForWindow(win);
        const closedTabGroupsById = getClosedTabGroupsById(win);

        let currentGroupId = null;

        closedTabs.forEach((tab, index) => {
          let { groupId } = tab.state;
          if (groupId && closedTabGroupsById.has(groupId)) {
            if (groupId != currentGroupId) {
              // This is the first tab in a new group. Push all the tabs into the menu
              if (aTagName == "menuitem") {
                createTabGroupSubmenu(
                  closedTabGroupsById.get(groupId),
                  index,
                  win,
                  doc,
                  fragment
                );
              } else {
                createTabGroupSubpanel(
                  closedTabGroupsById.get(groupId),
                  index,
                  win,
                  doc,
                  fragment
                );
              }

              currentGroupId = groupId;
            } else {
              // We have already seen this group. Ignore.
            }
          } else {
            createEntry(aTagName, false, index, tab, doc, tab.title, fragment);
            currentGroupId = null;
          }
        });
      }
    }

    // TODO Bug 1932941: Session support for closed tab groups in closed windows
    if (
      !isPrivate &&
      lazy.closedTabsFromClosedWindowsEnabled &&
      lazy.SessionStore.getClosedTabCountFromClosedWindows()
    ) {
      isEmpty = false;
      const closedTabs = lazy.SessionStore.getClosedTabDataFromClosedWindows();
      for (let i = 0; i < closedTabs.length; i++) {
        createEntry(
          aTagName,
          false,
          i,
          closedTabs[i],
          doc,
          closedTabs[i].title,
          fragment
        );
      }
    }

    if (!isEmpty) {
      createRestoreAllEntry(
        doc,
        fragment,
        false,
        aTagName == "menuitem"
          ? "recently-closed-menu-reopen-all-tabs"
          : "recently-closed-panel-reopen-all-tabs",
        aTagName
      );
    }
    return fragment;
  },

  /**
   * Builds up a document fragment of UI items for the recently closed windows.
   * @param   {Window} aWindow
   *          A window that can be used to create the elements and document fragment.
   * @param   {"menuitem"|"toolbarbutton"} aTagName
   *          The tag name that will be used when creating the UI items.
   * @returns {DocumentFragment} A document fragment with UI items for each recently closed window.
   */
  getWindowsFragment(aWindow, aTagName) {
    let closedWindowData = lazy.SessionStore.getClosedWindowData();
    let doc = aWindow.document;
    let fragment = doc.createDocumentFragment();
    if (closedWindowData.length) {
      for (let i = 0; i < closedWindowData.length; i++) {
        const { selected, tabs, title } = closedWindowData[i];
        const selectedTab = tabs[selected - 1];
        if (selectedTab) {
          const menuLabel = lazy.l10n.formatValueSync(
            "recently-closed-undo-close-window-label",
            { tabCount: tabs.length - 1, winTitle: title }
          );
          createEntry(aTagName, true, i, selectedTab, doc, menuLabel, fragment);
        }
      }

      createRestoreAllEntry(
        doc,
        fragment,
        true,
        aTagName == "menuitem"
          ? "recently-closed-menu-reopen-all-windows"
          : "recently-closed-panel-reopen-all-windows",
        aTagName
      );
    }
    return fragment;
  },

  /**
   * Handle a command event to re-open all closed tabs
   * @param aEvent
   *        The command event when the user clicks the restore all menu item
   */
  onRestoreAllTabsCommand(aEvent) {
    const currentWindow = aEvent.target.ownerGlobal;
    const browserWindows = lazy.closedTabsFromAllWindowsEnabled
      ? lazy.SessionStore.getWindows(currentWindow)
      : [currentWindow];
    for (const sourceWindow of browserWindows) {
      let count = lazy.SessionStore.getClosedTabCountForWindow(sourceWindow);
      while (--count >= 0) {
        lazy.SessionStore.undoCloseTab(sourceWindow, 0, currentWindow);
      }
    }
    if (lazy.closedTabsFromClosedWindowsEnabled) {
      for (let tabData of lazy.SessionStore.getClosedTabDataFromClosedWindows()) {
        lazy.SessionStore.undoClosedTabFromClosedWindow(
          { sourceClosedId: tabData.sourceClosedId },
          tabData.closedId,
          currentWindow
        );
      }
    }
  },

  /**
   * Handle a command event to re-open all closed windows
   * @param aEvent
   *        The command event when the user clicks the restore all menu item
   */
  onRestoreAllWindowsCommand() {
    const closedData = lazy.SessionStore.getClosedWindowData();
    for (const { closedId } of closedData) {
      lazy.SessionStore.undoCloseById(closedId);
    }
  },

  /**
   * Re-open a closed tab and put it to the end of the tab strip.
   * Used for a middle click.
   * @param aEvent
   *        The event when the user clicks the menu item
   */
  _undoCloseMiddleClick(aEvent) {
    if (aEvent.button != 1) {
      return;
    }
    if (aEvent.originalTarget.hasAttribute("source-closed-id")) {
      lazy.SessionStore.undoClosedTabFromClosedWindow(
        {
          sourceClosedId:
            aEvent.originalTarget.getAttribute("source-closed-id"),
        },
        aEvent.originalTarget.getAttribute("value")
      );
    } else {
      aEvent.view.undoCloseTab(
        aEvent.originalTarget.getAttribute("value"),
        aEvent.originalTarget.getAttribute("source-window-id")
      );
    }
    aEvent.view.gBrowser.moveTabToEnd();
    let ancestorPanel = aEvent.target.closest("panel");
    if (ancestorPanel) {
      ancestorPanel.hidePopup();
    }
  },
};

/**
 * @param {Element} element
 * @param {TabGroupStateData} tabGroup
 */
function setTabGroupColorProperties(element, tabGroup) {
  element.style.setProperty(
    "--tab-group-color",
    `var(--tab-group-color-${tabGroup.color})`
  );
  element.style.setProperty(
    "--tab-group-color-invert",
    `var(--tab-group-color-${tabGroup.color}-invert)`
  );
  element.style.setProperty(
    "--tab-group-color-pale",
    `var(--tab-group-color-${tabGroup.color}-pale)`
  );
}

/**
 * Creates a `menuitem` for the tab group that will expand to a newly
 * created submenu of the tab group's tab contents when selected.
 *
 * @param {TabGroupStateData} aTabGroup
 *        Session store state for the closed tab group.
 * @param {number} aIndex
 *        The index of the first tab in the tab group, relative to the tab strip.
 * @param {Window} aSourceWindow
 *        The source window of the closed tab group.
 * @param {Document} aDocument
 *        A document object that can be used to create the entry.
 * @param {DocumentFragment} aFragment
 *        The DOM fragment that the created entry will be in.
 */
function createTabGroupSubmenu(
  aTabGroup,
  aIndex,
  aSourceWindow,
  aDocument,
  aFragment
) {
  let element = aDocument.createXULElement("menu");
  if (aTabGroup.name) {
    element.setAttribute("label", aTabGroup.name);
  } else {
    aDocument.l10n.setAttributes(element, "tab-context-unnamed-group");
  }

  element.classList.add("menu-iconic", "tab-group-icon-closed");
  setTabGroupColorProperties(element, aTabGroup);

  let menuPopup = aDocument.createXULElement("menupopup");

  aTabGroup.tabs.forEach(tab => {
    createEntry(
      "menuitem",
      false,
      aIndex,
      tab,
      aDocument,
      tab.title,
      menuPopup
    );
    aIndex++;
  });

  menuPopup.appendChild(aDocument.createXULElement("menuseparator"));

  let reopenTabGroupItem = aDocument.createXULElement("menuitem");
  aDocument.l10n.setAttributes(
    reopenTabGroupItem,
    "tab-context-reopen-tab-group"
  );
  reopenTabGroupItem.addEventListener("command", () => {
    lazy.SessionStore.undoCloseTabGroup(aSourceWindow, aTabGroup.id);
  });
  menuPopup.appendChild(reopenTabGroupItem);

  element.appendChild(menuPopup);
  aFragment.appendChild(element);
}

/**
 * Creates a `toolbarbutton` for the tab group that will navigate to a newly
 * created subpanel of the tab group's tab contents when selected.
 *
 * @param {TabGroupStateData} aTabGroup
 *        Session store state for the closed tab group.
 * @param {number} aIndex
 *        The index of the first tab in the tab group, relative to the tab strip.
 * @param {Window} aSourceWindow
 *        The source window of the closed tab group.
 * @param {Document} aDocument
 *        A document object that can be used to create the entry.
 * @param {DocumentFragment} aFragment
 *        The DOM fragment that the created entry will be in.
 */
function createTabGroupSubpanel(
  aTabGroup,
  aIndex,
  aSourceWindow,
  aDocument,
  aFragment
) {
  let element = aDocument.createXULElement("toolbarbutton");
  if (aTabGroup.name) {
    element.setAttribute("label", aTabGroup.name);
  } else {
    aDocument.l10n.setAttributes(element, "tab-context-unnamed-group");
  }

  element.classList.add(
    "subviewbutton",
    "subviewbutton-iconic",
    "subviewbutton-nav",
    "tab-group-icon-closed"
  );
  element.setAttribute("closemenu", "none");
  setTabGroupColorProperties(element, aTabGroup);

  const panelviewId = `closed-tabs-tab-group-${aTabGroup.id}`;
  let panelview = aDocument.getElementById(panelviewId);

  if (panelview) {
    // panelviews get moved around the DOM by PanelMultiView, so if it still
    // exists, remove it so we can rebuild a new panelview
    panelview.remove();
  }

  panelview = aDocument.createXULElement("panelview");
  panelview.id = panelviewId;
  let panelBody = aDocument.createXULElement("vbox");
  panelBody.className = "panel-subview-body";

  aTabGroup.tabs.forEach(tab => {
    createEntry(
      "toolbarbutton",
      false,
      aIndex,
      tab,
      aDocument,
      tab.title,
      panelBody
    );
    aIndex++;
  });

  panelview.appendChild(panelBody);
  panelview.appendChild(aDocument.createXULElement("toolbarseparator"));

  let reopenTabGroupItem = aDocument.createXULElement("toolbarbutton");
  aDocument.l10n.setAttributes(
    reopenTabGroupItem,
    "tab-context-reopen-tab-group"
  );
  reopenTabGroupItem.classList.add(
    "reopentabgroupitem",
    "subviewbutton",
    "panel-subview-footer-button"
  );
  reopenTabGroupItem.addEventListener("command", () => {
    lazy.SessionStore.undoCloseTabGroup(aSourceWindow, aTabGroup.id);
  });

  panelview.appendChild(reopenTabGroupItem);

  element.addEventListener("command", () => {
    aDocument.ownerGlobal.PanelUI.showSubView(panelview.id, element);
  });

  aFragment.appendChild(panelview);
  aFragment.appendChild(element);
}

/**
 * Create a UI entry for a recently closed tab, tab group, or window.
 * @param {"menuitem"|"toolbarbutton"} aTagName
 *        the tag name that will be used when creating the UI entry
 * @param {boolean} aIsWindowsFragment
 *        whether or not this entry will represent a closed window
 * @param {number} aIndex
 *        the index of the closed tab
 * @param {TabStateData} aClosedTab
 *        the closed tab
 * @param {Document} aDocument
 *        a document that can be used to create the entry
 * @param {string} aMenuLabel
 *        the label the created entry will have
 * @param {DocumentFragment} aFragment
 *        the fragment the created entry will be in
 */
function createEntry(
  aTagName,
  aIsWindowsFragment,
  aIndex,
  aClosedTab,
  aDocument,
  aMenuLabel,
  aFragment
) {
  let element = aDocument.createXULElement(aTagName);

  element.setAttribute("label", aMenuLabel);
  if (aClosedTab.image) {
    const iconURL = lazy.PlacesUIUtils.getImageURL(aClosedTab.image);
    element.setAttribute("image", iconURL);
  }

  if (aIsWindowsFragment) {
    element.addEventListener("command", event =>
      event.target.ownerGlobal.undoCloseWindow(aIndex)
    );
  } else if (typeof aClosedTab.sourceClosedId == "number") {
    // sourceClosedId is used to look up the closed window to remove it when the tab is restored
    let sourceClosedId = aClosedTab.sourceClosedId;
    element.setAttribute("source-closed-id", sourceClosedId);
    element.setAttribute("value", aClosedTab.closedId);
    element.addEventListener(
      "command",
      () => {
        lazy.SessionStore.undoClosedTabFromClosedWindow(
          { sourceClosedId },
          aClosedTab.closedId
        );
      },
      { once: true }
    );
  } else {
    // sourceWindowId is used to look up the closed tab entry to remove it when it is restored
    let sourceWindowId = aClosedTab.sourceWindowId;
    element.setAttribute("value", aIndex);
    element.setAttribute("source-window-id", sourceWindowId);
    element.addEventListener("command", event =>
      event.target.ownerGlobal.undoCloseTab(aIndex, sourceWindowId)
    );
  }

  if (aTagName == "menuitem") {
    element.setAttribute(
      "class",
      "menuitem-iconic bookmark-item menuitem-with-favicon"
    );
  } else if (aTagName == "toolbarbutton") {
    element.setAttribute(
      "class",
      "subviewbutton subviewbutton-iconic bookmark-item"
    );
  }

  // Set the targetURI attribute so it will be shown in tooltip.
  // SessionStore uses one-based indexes, so we need to normalize them.
  let tabData;
  tabData = aIsWindowsFragment ? aClosedTab : aClosedTab.state;
  let activeIndex = (tabData.index || tabData.entries.length) - 1;
  if (activeIndex >= 0 && tabData.entries[activeIndex]) {
    element.setAttribute("targetURI", tabData.entries[activeIndex].url);
  }

  // Windows don't open in new tabs and menuitems dispatch command events on
  // middle click, so we only need to manually handle middle clicks for
  // toolbarbuttons.
  if (!aIsWindowsFragment && aTagName != "menuitem") {
    element.addEventListener(
      "click",
      RecentlyClosedTabsAndWindowsMenuUtils._undoCloseMiddleClick
    );
  }

  if (aIndex == 0) {
    element.setAttribute(
      "key",
      aIsWindowsFragment
        ? "key_undoCloseWindow"
        : "key_restoreLastClosedTabOrWindowOrSession"
    );
  }

  aFragment.appendChild(element);
}

/**
 * Create an entry to restore all closed windows or tabs.
 * For menus, adds a menu separator and a menu item.
 * For toolbar panels, adds a toolbar button only and expects
 * CustomizableWidgets.sys.mjs to add its own separator elsewhere in the DOM
 *
 * @param {Document} aDocument
 *        a document that can be used to create the entry
 * @param {DocumentFragment} aFragment
 *        the fragment the created entry will be in
 * @param {boolean} aIsWindowsFragment
 *        whether or not this entry will represent a closed window
 * @param {string} aRestoreAllLabel
 *        which localizable string to use for the entry
 * @param {"menuitem"|"toolbarbutton"} aTagName
 *        the tag name that will be used when creating the UI entry
 */
function createRestoreAllEntry(
  aDocument,
  aFragment,
  aIsWindowsFragment,
  aRestoreAllLabel,
  aTagName
) {
  let restoreAllElements = aDocument.createXULElement(aTagName);
  restoreAllElements.classList.add("restoreallitem");

  if (aTagName == "toolbarbutton") {
    restoreAllElements.classList.add(
      "subviewbutton",
      "panel-subview-footer-button"
    );
  }

  // We cannot use aDocument.l10n.setAttributes because the menubar label is not
  // updated in time and displays a blank string (see Bug 1691553).
  restoreAllElements.setAttribute(
    "label",
    lazy.l10n.formatValueSync(aRestoreAllLabel)
  );

  restoreAllElements.addEventListener(
    "command",
    aIsWindowsFragment
      ? RecentlyClosedTabsAndWindowsMenuUtils.onRestoreAllWindowsCommand
      : RecentlyClosedTabsAndWindowsMenuUtils.onRestoreAllTabsCommand
  );

  if (aTagName == "menuitem") {
    aFragment.appendChild(aDocument.createXULElement("menuseparator"));
  }

  aFragment.appendChild(restoreAllElements);
}
