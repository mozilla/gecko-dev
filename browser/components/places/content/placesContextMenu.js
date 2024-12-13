/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

document.addEventListener(
  "DOMContentLoaded",
  () => {
    const placesContext = document.getElementById("placesContext");

    placesContext.addEventListener("popupshowing", event =>
      PlacesUIUtils.placesContextShowing(event)
    );
    placesContext.addEventListener("popuphiding", event =>
      PlacesUIUtils.placesContextHiding(event)
    );

    placesContext.addEventListener("command", event => {
      switch (event.target.id) {
        case "placesContext_openBookmarkContainer:tabs":
        case "placesContext_openBookmarkLinks:tabs":
        case "placesContext_openContainer:tabs":
        case "placesContext_openLinks:tabs":
          PlacesUIUtils.openSelectionInTabs(event);
          break;
      }
    });

    const containerPopup = document.getElementById(
      "placesContext_open_newcontainertab_popup"
    );
    containerPopup.addEventListener("command", event =>
      PlacesUIUtils.openInContainerTab(event)
    );
    containerPopup.addEventListener("popupshowing", event =>
      PlacesUIUtils.createContainerTabMenu(event)
    );
  },
  { once: true }
);
