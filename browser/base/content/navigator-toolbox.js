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

    navigatorToolbox.addEventListener("command", event => {
      let element = event.target.closest(`
        #firefox-view-button,
        #content-analysis-indicator,
        #bookmarks-toolbar-button,
        #PlacesToolbar,
        #import-button,
        #bookmarks-menu-button,
        #BMB_bookmarksPopup,
        #BMB_viewBookmarksSidebar,
        #BMB_searchBookmarks,
        #BMB_viewBookmarksToolbar`);
      if (!element) {
        return;
      }

      switch (element.id) {
        case "firefox-view-button":
          FirefoxViewHandler.openTab();
          break;

        case "content-analysis-indicator":
          ContentAnalysis.showPanel(element, PanelUI);
          break;

        case "bookmarks-toolbar-button":
          PlacesToolbarHelper.onPlaceholderCommand();
          break;

        case "PlacesToolbar":
          BookmarksEventHandler.onCommand(event);
          break;

        case "import-button":
          MigrationUtils.showMigrationWizard(window, {
            entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.BOOKMARKS_TOOLBAR,
          });
          break;

        case "bookmarks-menu-button":
          BookmarkingUI.onCommand(event);
          break;

        case "BMB_bookmarksPopup":
          BookmarksEventHandler.onCommand(event);
          break;

        case "BMB_viewBookmarksSidebar":
          SidebarController.toggle("viewBookmarksSidebar");
          break;

        case "BMB_searchBookmarks":
          PlacesCommandHook.searchBookmarks();
          break;

        case "BMB_viewBookmarksToolbar":
          BookmarkingUI.toggleBookmarksToolbar("bookmarks-widget");
          break;

        default:
          throw new Error(`Missing case for #${element.id}`);
      }
    });
  },
  { once: true }
);
