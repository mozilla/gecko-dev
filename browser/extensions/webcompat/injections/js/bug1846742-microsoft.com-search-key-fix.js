/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * www.microsoft.com - Pressing enter on search suggestions does nothing
 *
 * When pressing enter on a search suggestion, nothing happens in Firefox
 * due to key-event interop issues. We listen for enter keypresses and
 * click the element as a work-around.
 */

console.info(
  "A simulated click is issued on search suggestions when enter is pressed on them for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1846742 for details."
);

document.addEventListener("keydown", ({ target, key }) => {
  if (key == "Enter" && target.matches(".m-auto-suggest a.f-product")) {
    target.click();
  }
});
