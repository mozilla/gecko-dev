/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

document.addEventListener(
  "DOMContentLoaded",
  () => {
    const IS_WEBEXT_PANELS =
      document.documentElement.id === "webextpanels-window";

    const contextMenuPopup = document.getElementById("contentAreaContextMenu");
    contextMenuPopup.addEventListener("popupshowing", event => {
      if (event.target != contextMenuPopup) {
        return;
      }

      // eslint-disable-next-line no-global-assign
      gContextMenu = new nsContextMenu(contextMenuPopup, event.shiftKey);
      if (!gContextMenu.shouldDisplay) {
        event.preventDefault();
        return;
      }

      if (!IS_WEBEXT_PANELS) {
        updateEditUIVisibility();
      }
    });
    contextMenuPopup.addEventListener("popuphiding", event => {
      if (event.target != contextMenuPopup) {
        return;
      }

      gContextMenu.hiding(contextMenuPopup);
      // eslint-disable-next-line no-global-assign
      gContextMenu = null;
      if (!IS_WEBEXT_PANELS) {
        updateEditUIVisibility();
      }
    });
  },
  { once: true }
);
