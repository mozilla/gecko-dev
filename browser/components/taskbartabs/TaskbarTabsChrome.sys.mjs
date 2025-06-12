/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Set up Taskbar Tabs Window Chrome.
export const TaskbarTabsChrome = {
  /**
   * Modifies the window chrome for Taskbar Tabs styling (mostly hiding elements).
   *
   * @param {Window} aWindow
   */
  init(aWindow) {
    let document = aWindow.document;
    if (!document.documentElement.hasAttribute("taskbartab")) {
      return;
    }

    // Ensure tab strip is hidden
    aWindow.TabBarVisibility.update();

    // Hide bookmark star
    document.getElementById("star-button-box").style.display = "none";

    // Hide fxa in the hamburger menu
    document.documentElement.setAttribute("fxadisabled", true);
  },
};
