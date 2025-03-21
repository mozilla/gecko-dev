/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  "e10s/doc_treeupdate_whitespace.html",
  async function (browser, accDoc) {
    let container1 = findAccessibleChildByID(accDoc, "container1");
    let container2Parent = findAccessibleChildByID(accDoc, "container2-parent");

    let tree = {
      SECTION: [
        { GRAPHIC: [] },
        { TEXT_LEAF: [] },
        { GRAPHIC: [] },
        { TEXT_LEAF: [] },
        { GRAPHIC: [] },
      ],
    };
    testAccessibleTree(container1, tree);

    let onReorder = waitForEvent(EVENT_REORDER, "container1");
    // Remove img1 from container1
    await invokeContentTask(browser, [], () => {
      let doc = content.document;
      doc.getElementById("container1").removeChild(doc.getElementById("img1"));
    });
    await onReorder;

    tree = {
      SECTION: [{ GRAPHIC: [] }, { TEXT_LEAF: [] }, { GRAPHIC: [] }],
    };
    testAccessibleTree(container1, tree);

    tree = {
      SECTION: [{ LINK: [] }, { LINK: [{ GRAPHIC: [] }] }],
    };
    testAccessibleTree(container2Parent, tree);

    onReorder = waitForEvent(EVENT_REORDER, "container2-parent");
    // Append an img with valid src to container2
    await invokeContentTask(browser, [], () => {
      let doc = content.document;
      let img = doc.createElement("img");
      img.setAttribute(
        "src",
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        "http://example.com/a11y/accessible/tests/mochitest/moz.png"
      );
      doc.getElementById("container2").appendChild(img);
    });
    await onReorder;

    tree = {
      SECTION: [
        { LINK: [{ GRAPHIC: [] }] },
        { TEXT_LEAF: [] },
        { LINK: [{ GRAPHIC: [] }] },
      ],
    };
    testAccessibleTree(container2Parent, tree);
  },
  { iframe: true, remoteIframe: true }
);

/**
 * Test whitespace before hard and soft line breaks.
 */
addAccessibleTask(
  `
<div id="hardContainer"><span>a</span> <span id="b" hidden>b</span></div>
<div id="softContainer" style="width: 1ch; font-family: monospace;">
  <span>c</span> <span>d</span>
</div>
  `,
  async function testBeforeLineBreaks(browser, docAcc) {
    const hardContainer = findAccessibleChildByID(docAcc, "hardContainer");
    testAccessibleTree(hardContainer, {
      role: ROLE_SECTION,
      children: [{ role: ROLE_TEXT_LEAF, name: "a" }],
    });

    info("Showing b");
    let reordered = waitForEvent(EVENT_REORDER, hardContainer);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("b").hidden = false;
    });
    await reordered;
    testAccessibleTree(hardContainer, {
      role: ROLE_SECTION,
      children: [
        { role: ROLE_TEXT_LEAF, name: "a" },
        { role: ROLE_TEXT_LEAF, name: " " },
        { role: ROLE_TEXT_LEAF, name: "b" },
      ],
    });

    info("Hiding b");
    reordered = waitForEvent(EVENT_REORDER, hardContainer);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("b").hidden = true;
    });
    await reordered;
    testAccessibleTree(hardContainer, {
      role: ROLE_SECTION,
      children: [{ role: ROLE_TEXT_LEAF, name: "a" }],
    });

    const softContainer = findAccessibleChildByID(docAcc, "softContainer");
    testAccessibleTree(softContainer, {
      role: ROLE_SECTION,
      children: [
        { role: ROLE_TEXT_LEAF, name: "c" },
        { role: ROLE_TEXT_LEAF, name: " " },
        { role: ROLE_TEXT_LEAF, name: "d" },
      ],
    });
  },
  { chrome: true, topLevel: true }
);
