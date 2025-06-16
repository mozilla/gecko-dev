/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kWidgetId = "taskbar-tabs-button";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

let lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  TaskbarTabs: "resource:///modules/taskbartabs/TaskbarTabs.sys.mjs",
  TaskbarTabsUtils: "resource:///modules/taskbartabs/TaskbarTabsUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "TaskbarTabs",
    maxLogLevel: "Warn",
  });
});

/**
 * Object which handles Taskbar Tabs page actions.
 */
export const TaskbarTabsPageAction = {
  // Set of tabs currently being processed due to a page action event.
  _processingTabs: new Set(),

  /**
   * Connects a listener to the Taskbar Tabs page action.
   *
   * @param {DOMWindow} aWindow - The browser window.
   */
  init(aWindow) {
    let taskbarTabsEnabled = lazy.TaskbarTabsUtils.isEnabled();
    let isPopupWindow = !aWindow.toolbar.visible;
    // WARNING: If we ever enable private browsing in Taskbar Tabs, we need to
    // revisit pin code which assumes we're not in a private context.
    let isPrivate = lazy.PrivateBrowsingUtils.isWindowPrivate(aWindow);

    if (
      !taskbarTabsEnabled ||
      isPopupWindow ||
      isPrivate ||
      AppConstants.platform != "win"
    ) {
      lazy.logConsole.info("Not initializing Taskbar Tabs Page Action.");
      return;
    }

    lazy.logConsole.info("Initializing Taskbar Tabs Page Action.");

    let taskbarTabsButton = aWindow.document.getElementById(kWidgetId);
    taskbarTabsButton.addEventListener("click", this, true);

    taskbarTabsButton.hidden = false;
  },

  /**
   * Handles the clicking of the page action button associated with Taskbar
   * Tabs.
   *
   * @param {Event} aEvent - The event triggered by the Taskbar Tabs page
   * action.
   * @returns {Promise} Resolves once the event has been handled.
   */
  async handleEvent(aEvent) {
    let window = aEvent.target.ownerGlobal;
    let currentTab = window.gBrowser.selectedTab;

    if (this._processingTabs.has(currentTab)) {
      // Button was clicked before last input finished processing for the tab,
      // discard to avoid racing. Don't bother buffering input - clicking
      // repeatedly before input is processed is not meaningful.
      lazy.logConsole.debug(
        `Page Action still processing for tab, dropping input.`
      );
      return;
    }
    lazy.logConsole.debug(`Blocking Page Action input for tab.`);
    this._processingTabs.add(currentTab);

    try {
      let isTaskbarTabWindow = lazy.TaskbarTabsUtils.isTaskbarTabWindow(window);

      if (!isTaskbarTabWindow) {
        lazy.logConsole.info("Opening new Taskbar Tab via Page Action.");

        // Move tab to a Taskbar Tabs window.
        let browser = currentTab.linkedBrowser;
        let url = browser.currentURI;
        let userContextId =
          browser.contentPrincipal.originAttributes.userContextId;

        let taskbarTab = await lazy.TaskbarTabs.findOrCreateTaskbarTab(
          url,
          userContextId
        );

        await lazy.TaskbarTabs.replaceTabWithWindow(taskbarTab, currentTab);
      } else {
        lazy.logConsole.info("Closing Taskbar Tab via Page Action.");

        // Move tab to a regular browser window.
        let id = lazy.TaskbarTabsUtils.getTaskbarTabIdFromWindow(window);

        await lazy.TaskbarTabs.ejectWindow(window);

        if (!(await lazy.TaskbarTabs.getCountForId(id))) {
          lazy.logConsole.info("Uninstalling Taskbar Tab via Page Action.");
          await lazy.TaskbarTabs.removeTaskbarTab(id);
        }
      }
    } finally {
      lazy.logConsole.debug(`Unblocking Page Action input for tab.`);
      this._processingTabs.delete(currentTab);
    }
  },
};
