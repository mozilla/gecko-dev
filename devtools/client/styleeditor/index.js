/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../toolkit/content/globalOverlay.js */
/* import-globals-from ../../../toolkit/content/editMenuOverlay.js */

window.addEventListener("load", () => {
  document
    .getElementById("sourceEditorContextMenu")
    .addEventListener("popupshowing", () => {
      goUpdateGlobalEditMenuItems();

      [
        "cmd_undo",
        "cmd_redo",
        "cmd_cut",
        "cmd_paste",
        "cmd_delete",
        "cmd_find",
        "cmd_findAgain",
      ].forEach(goUpdateCommand);
    });

  document
    .getElementById("sourceEditorCommands")
    .addEventListener("command", event => goDoCommand(event.target.id));
});
