/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test the debugger popup previews with bucketed arrays.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-preview.html", "preview.js");

  await selectSource(dbg, "preview.js");

  invokeInTab("largeArray");
  await waitForPaused(dbg);
  const { element: popupEl, tokenEl } = await tryHovering(
    dbg,
    34,
    10,
    "previewPopup"
  );

  info("Wait for top level node to expand and child nodes to load");
  await waitUntil(
    () => popupEl.querySelectorAll(".preview-popup .node").length > 1
  );

  const oiNodes = Array.from(popupEl.querySelectorAll(".preview-popup .node"));

  const displayedPropertyNames = oiNodes.map(
    oiNode => oiNode.querySelector(".object-label")?.textContent
  );
  Assert.deepEqual(displayedPropertyNames, [
    null, // No property name is displayed for the root node
    "[0…99]",
    "[100…100]",
    "length",
    "<prototype>",
  ]);
  const node = oiNodes.find(
    oiNode => oiNode.querySelector(".object-label")?.textContent === "length"
  );
  if (!node) {
    ok(false, `The "length" property is not displayed in the popup`);
  } else {
    is(
      node.querySelector(".objectBox").textContent,
      "101",
      `The "length" property has the expected value`
    );
  }
  await closePreviewForToken(dbg, tokenEl, "popup");

  await resume(dbg);
});
