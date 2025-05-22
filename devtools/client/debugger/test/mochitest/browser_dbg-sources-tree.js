/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests how directories that contain only one child which is a directory
// are merged with their child in the sources tree and separated again
// when another child is added (Bug 1967248).

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-nested-sources.html");

  await waitForSources(dbg, "deep-nested-source.js");
  await selectSource(dbg, "deep-nested-source.js");

  checkSourceTree(dbg, [
    ["Main Thread", 1],
    ["example.com", 2],
    ["browser/devtools/client/debugger/test/mochitest/examples", 3],
    ["nested/deep", 4],
    ["deep-nested-source.js", 5],
    ["doc-nested-sources.html", 4],
  ]);

  invokeInTab("loadScript");
  await waitForSources(dbg, "nested-source.js");

  checkSourceTree(dbg, [
    ["Main Thread", 1],
    ["example.com", 2],
    ["browser/devtools/client/debugger/test/mochitest/examples", 3],
    ["nested", 4],
    ["deep", 5],
    ["deep-nested-source.js", 6],
    ["nested-source.js", 5],
    ["doc-nested-sources.html", 4],
  ]);
});

function checkSourceTree(dbg, expected) {
  const treeNodes = getDisplayedSourceTree(dbg);
  is(
    expected.length,
    treeNodes.length,
    "The source tree has the expected number of nodes"
  );

  for (let i = 0; i < expected.length; i++) {
    const node = treeNodes[i];
    const [label, level] = expected[i];
    is(
      node.querySelector(".label").textContent,
      label,
      `The node has the expected label`
    );
    is(Number(node.ariaLevel), level, `The node has the expected level`);
  }
}
