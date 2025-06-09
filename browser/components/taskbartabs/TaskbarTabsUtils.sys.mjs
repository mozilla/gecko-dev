/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const TaskbarTabsUtils = {
  /**
   * Checks if Taskbar Tabs has been enabled.
   *
   * @returns {bool} `true` if the Taskbar Tabs pref is enabled.
   */
  isEnabled() {
    let pref = "browser.taskbarTabs.enabled";
    return Services.prefs.getBoolPref(pref, false);
  },

  /**
   * Returns a folder to store profile-specific Taskbar Tabs files.
   *
   * @returns {nsIFile} Folder to store Taskbar Tabs files.
   */
  getTaskbarTabsFolder() {
    // Construct the path `[Profile]/taskbartabs/`.
    let folder = Services.dirsvc.get("ProfD", Ci.nsIFile);
    folder.append("taskbartabs");
    return folder;
  },

  /**
   * Checks if the window is a Taskbar Tabs window.
   *
   * @param {Window} aWin - The window to inspect.
   * @returns {bool} true if the window is a Taskbar Tabs window.
   */
  isTaskbarTabWindow(aWin) {
    return aWin.document.documentElement.hasAttribute("taskbartab");
  },

  /**
   * Retrieves the Taskbar Tabs ID for the window.
   *
   * @param {DOMWindow} aWin - The window to retrieve the Taskbar Tabs ID.
   * @returns {string} The Taskbar Tabs ID for the window.
   */
  getTaskbarTabIdFromWindow(aWin) {
    return aWin.document.documentElement.getAttribute("taskbartab");
  },
};
