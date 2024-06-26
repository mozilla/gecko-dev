/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

document
  .getElementById("placesCommands")
  .addEventListener("commandupdate", () => {
    PlacesUIUtils.updateCommands(window);
  });

document.getElementById("placesCommands").addEventListener("command", event => {
  const cmd = event.target.id;
  switch (cmd) {
    case "Browser:ShowAllBookmarks":
      PlacesCommandHook.showPlacesOrganizer("UnfiledBookmarks");
      break;
    case "Browser:ShowAllHistory":
      PlacesCommandHook.showPlacesOrganizer("History");
      break;

    case "placesCmd_open":
    case "placesCmd_open:window":
    case "placesCmd_open:privatewindow":
    case "placesCmd_open:tab":
    case "placesCmd_new:bookmark":
    case "placesCmd_new:folder":
    case "placesCmd_new:separator":
    case "placesCmd_show:info":
    case "placesCmd_sortBy:name":
    case "placesCmd_deleteDataHost":
    case "placesCmd_createBookmark":
    case "placesCmd_cut":
    case "placesCmd_copy":
    case "placesCmd_paste":
    case "placesCmd_delete":
    case "placesCmd_showInFolder":
      PlacesUIUtils.doCommand(window, cmd);
      break;
  }
});
