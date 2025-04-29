/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test aria-keyshortcuts
 */
addAccessibleTask(
  `
  <div id="btn" role="button" aria-keyshortcuts="Alt+Shift+f">bar</div>
  `,
  (browser, accDoc) => {
    let btn = getNativeInterface(accDoc, "btn");
    is(
      btn.getAttributeValue("AXKeyShortcutsValue"),
      "Alt+Shift+f",
      "aria-keyshortcuts value is correct"
    );
  }
);
