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
 * Test whitespace before a hidden element at the end of a block.
 */
addAccessibleTask(
  `<div id="container"><span>a</span> <span id="b" hidden>b</span></div>`,
  async function testBeforeHiddenElementAtEnd(browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "container");
    testAccessibleTree(container, {
      role: ROLE_SECTION,
      children: [{ role: ROLE_TEXT_LEAF, name: "a" }],
    });

    info("Showing b");
    let reordered = waitForEvent(EVENT_REORDER, container);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("b").hidden = false;
    });
    await reordered;
    testAccessibleTree(container, {
      role: ROLE_SECTION,
      children: [
        { role: ROLE_TEXT_LEAF, name: "a" },
        { role: ROLE_TEXT_LEAF, name: " " },
        { role: ROLE_TEXT_LEAF, name: "b" },
      ],
    });

    info("Hiding b");
    reordered = waitForEvent(EVENT_REORDER, container);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("b").hidden = true;
    });
    await reordered;
    testAccessibleTree(container, {
      role: ROLE_SECTION,
      children: [{ role: ROLE_TEXT_LEAF, name: "a" }],
    });
  },
  { chrome: true, topLevel: true }
);
