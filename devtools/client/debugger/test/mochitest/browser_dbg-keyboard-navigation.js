/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that keyboard navigation into and out of debugger code editor

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");
  const doc = dbg.win.document;

  await selectSource(dbg, "simple2.js");

  await waitForElement(dbg, "codeMirror");

  info("Focus on the editor");
  findElement(dbg, "codeMirror").focus();

  is(findElement(dbg, "codeMirror"), doc.activeElement, "Editor is focused");

  info(
    "Press shift + tab to navigate out of the editor to the previous tab element"
  );
  pressKey(dbg, "ShiftTab");

  is(
    findElementWithSelector(dbg, ".command-bar-button.toggle-button.end"),
    doc.activeElement,
    "The left sidebar toggle button is focused"
  );

  info("Press tab to navigate back to the editor");
  pressKey(dbg, "Tab");

  is(
    findElement(dbg, "codeMirror"),
    doc.activeElement,
    "Editor is focused again"
  );

  info("Press tab again to navigate out of the editor to the next tab element");
  // The extra tab is needed as in CM5 there is a <textarea> which gets a focus
  pressKey(dbg, "Tab");
  pressKey(dbg, "Tab");

  is(
    findElementWithSelector(dbg, ".action.black-box"),
    doc.activeElement,
    "The ignore source button is focused"
  );
});
