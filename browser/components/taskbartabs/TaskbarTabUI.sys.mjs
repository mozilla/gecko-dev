/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Set up Taskbar Tabs Window UI
export var TaskbarTabUI = {
  init(window) {
    let document = window.document;
    if (!document.documentElement.hasAttribute("taskbartab")) {
      return;
    }

    // Ensure tab strip is hidden
    window.TabBarVisibility.update();

    // Hide pocket button
    const saveToPocketButton = document.getElementById("save-to-pocket-button");
    if (saveToPocketButton) {
      saveToPocketButton.remove();
      document.documentElement.setAttribute("pocketdisabled", "true");
    }

    // Hide bookmark star
    document.getElementById("star-button-box").style.display = "none";

    // Hide fxa in the hamburger menu
    document.documentElement.setAttribute("fxadisabled", true);
  },
};
