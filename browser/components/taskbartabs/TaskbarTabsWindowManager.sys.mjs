/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const kTaskbarTabsWindowFeatures =
  "titlebar,close,toolbar,location,personalbar=no,status,menubar=no,resizable,minimizable,scrollbars";

let lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  TaskbarTabsUtils: "resource:///modules/taskbartabs/TaskbarTabsUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  WinTaskbar: ["@mozilla.org/windows-taskbar;1", "nsIWinTaskbar"],
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "TaskbarTabs",
    maxLogLevel: "Warn",
  });
});

/**
 * Manager for the lifetimes of Taskbar Tab windows.
 */
export class TaskbarTabsWindowManager {
  // Count of active taskbar tabs associated to an ID.
  #tabIdCount = new Map();
  // Map from the tab browser permanent key to originating window ID.
  #tabOriginMap = new WeakMap();

  /**
   * Moves an existing browser tab into a Taskbar Tab.
   *
   * @param {TaskbarTab} aTaskbarTab - The Taskbar Tab to replace the window with.
   * @param {string} aTaskbarTab.id - ID of the Taskbar Tab.
   * @param {MozTabbrowserTab} aTab - The tab to adopt as a Taskbar Tab.
   * @returns {Promise} Resolves once the tab replacing window has openend.
   */
  async replaceTabWithWindow({ id }, aTab) {
    let originWindow = aTab.ownerGlobal;

    // Save the parent window of this tab, so we can revert back if needed.
    let tabId = getTabId(aTab);
    let windowId = getWindowId(originWindow);

    let extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
    extraOptions.setPropertyAsAString("taskbartab", id);

    let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    args.appendElement(aTab);
    args.appendElement(extraOptions);

    await this.#openWindow(id, args);

    this.#tabOriginMap.set(tabId, windowId);
  }

  /**
   * Opens a new Taskbar Tab Window.
   *
   * @param {TaskbarTab} aTaskbarTab - The Taskbar Tab to open.
   * @returns {Promise} Resolves once the window has opened.
   */
  async openWindow(aTaskbarTab) {
    let url = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    url.data = aTaskbarTab.startUrl;

    let extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
    extraOptions.setPropertyAsAString("taskbartab", aTaskbarTab.id);

    let userContextId = Cc["@mozilla.org/supports-PRUint32;1"].createInstance(
      Ci.nsISupportsPRUint32
    );
    userContextId.data = aTaskbarTab.userContextId;

    let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    args.appendElement(url);
    args.appendElement(extraOptions);
    args.appendElement(null);
    args.appendElement(null);
    args.appendElement(undefined);
    args.appendElement(userContextId);
    args.appendElement(null);
    args.appendElement(null);
    args.appendElement(Services.scriptSecurityManager.getSystemPrincipal());

    await this.#openWindow(aTaskbarTab.id, args);
  }

  /**
   * Handles common window opening behavior for Taskbar Tabs.
   *
   * @param {string} aId - ID of the Taskbar Tab to use as the window AUMID.
   * @param {nsIMutableArray} aArgs - `args` to pass to the opening window.
   * @returns {Promise} Resolves once window has opened and tab count has been
   *                    incremented.
   */
  async #openWindow(aId, aArgs) {
    let win = await lazy.BrowserWindowTracker.promiseOpenWindow({
      args: aArgs,
      features: kTaskbarTabsWindowFeatures,
      all: false,
    });

    lazy.WinTaskbar.setGroupIdForWindow(win, aId);
    win.focus();

    let tabIdCount = this.#tabIdCount.get(aId) ?? 0;
    this.#tabIdCount.set(aId, ++tabIdCount);
  }

  /**
   * Reverts a web app to a tab in a regular Firefox window. We will try to use
   * the window the taskbar tab originated from, if that's not avaliable, we
   * will use the most recently active window. If no window is avalaible, a new
   * one will be opened.
   *
   * @param {DOMWindow} aWindow - A Tasbkar Tab window.
   */
  async ejectWindow(aWindow) {
    lazy.logConsole.info("Ejecting window from Taskbar Tabs.");

    let taskbarTabId = lazy.TaskbarTabsUtils.getTaskbarTabIdFromWindow(aWindow);
    if (!taskbarTabId) {
      throw new Error("No Taskbar Tab ID found on window.");
    } else {
      lazy.logConsole.debug(`Taskbar Tab ID is ${taskbarTabId}`);
    }

    let windowList = lazy.BrowserWindowTracker.getOrderedWindows({
      private: false,
    });

    // A Taskbar Tab should only contain one tab, but iterate over the browser's
    // tabs just in case one snuck in.
    for (const tab of aWindow.gBrowser.tabs) {
      let tabId = getTabId(tab);
      let originWindowId = this.#tabOriginMap.get(tabId);

      let win =
        // Find the originating window for the Taskbar Tab if it still exists.
        windowList.find(window => {
          let windowId = getWindowId(window);
          let matching = windowId === originWindowId;
          if (matching) {
            lazy.logConsole.debug(
              `Ejecting into originating window: ${windowId}`
            );
          }
          return matching;
        });

      if (!win) {
        // Otherwise the most recent non-Taskbar Tabs window interacted with.
        win = lazy.BrowserWindowTracker.getTopWindow({
          private: false,
        });

        if (win) {
          lazy.logConsole.debug(`Ejecting into top window.`);
        }
      }

      if (win) {
        // Set this tab to the last tab position of the window.
        win.gBrowser.adoptTab(tab, {
          tabIndex: win.gBrowser.openTabs.length,
          selectTab: true,
        });
      } else {
        lazy.logConsole.debug(
          "No originating or existing browser window found, ejecting into newly created window."
        );
        win = await lazy.BrowserWindowTracker.promiseOpenWindow({ args: tab });
      }

      win.focus();

      this.#tabOriginMap.delete(tabId);
      let tabIdCount = this.#tabIdCount.get(taskbarTabId);
      if (tabIdCount > 0) {
        this.#tabIdCount.set(taskbarTabId, --tabIdCount);
      } else {
        lazy.logConsole.error("Tab count should have been greater than 0.");
      }
    }
  }

  /**
   * Returns a count of the current windows associated to a Taskbar Tab.
   *
   * @param {string} aId - The Taskbar Tab ID.
   * @returns {integer} Count of windows associated to the Taskbar Tab ID.
   */
  getCountForId(aId) {
    return this.#tabIdCount.get(aId) ?? 0;
  }
}

/**
 * Retrieves the browser tab's ID.
 *
 * @param {MozTabbrowserTab} aTab - Tab to retrieve the ID from.
 * @returns {object} The permanent key identifying the tab.
 */
function getTabId(aTab) {
  return aTab.permanentKey;
}

/**
 * Retrieves the window ID.
 *
 * @param {DOMWindow} aWindow
 * @returns {string} A unique string identifying the window.
 */
function getWindowId(aWindow) {
  return aWindow.docShell.outerWindowID;
}
