/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

document.addEventListener(
  "DOMContentLoaded",
  () => {
    const navigatorToolbox = document.getElementById("navigator-toolbox");

    navigatorToolbox.addEventListener("popupshowing", event => {
      switch (event.target.id) {
        case "PlacesChevronPopup":
          document
            .getElementById("PlacesToolbar")
            ._placesView._onChevronPopupShowing(event);
          break;

        case "BMB_bookmarksPopup":
          BookmarkingUI.onPopupShowing(event);
        // fall-through
        case "BMB_bookmarksToolbarPopup":
        case "BMB_unsortedBookmarksPopup":
        case "BMB_mobileBookmarksPopup":
          if (!event.target.parentNode._placesView) {
            let placeMap = {
              BMB_bookmarksPopup: PlacesUtils.bookmarks.menuGuid,
              BMB_bookmarksToolbarPopup: PlacesUtils.bookmarks.toolbarGuid,
              BMB_unsortedBookmarksPopup: PlacesUtils.bookmarks.unfiledGuid,
              BMB_mobileBookmarksPopup: PlacesUtils.bookmarks.mobileGuid,
            };
            new PlacesMenu(event, `place:parent=${placeMap[event.target.id]}`);
          }
          break;
      }
    });
  },
  { once: true }
);
