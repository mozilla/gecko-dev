/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests re-opening pretty printed tabs on load

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-minified.html", "math.min.js");

  await selectSourceFromSourceTree(dbg, "math.min.js");
  assertSourceIcon(dbg, "math.min.js", "javascript");
  clickElement(dbg, "prettyPrintButton");
  await waitForSource(dbg, "math.min.js:formatted");

  await waitFor(() => findElement(dbg, "sourceTabs").children.length == 2);
  const [prettyTab, originalTab] = findElement(dbg, "sourceTabs").children;
  ok(
    prettyTab.querySelector(".source-icon.img.prettyPrint"),
    "Pretty printed tab has the pretty-print icon"
  );
  ok(
    !originalTab.querySelector(".source-icon.img.prettyPrint"),
    "original tab does not have the pretty-print icon"
  );

  // Test reloading the debugger
  await waitForSelectedSource(dbg, "math.min.js:formatted");
  await reload(dbg);

  await waitForSelectedSource(dbg, "math.min.js:formatted");
  ok(true, "Pretty printed source is selected on reload");

  // Select any other source
  await selectSourceFromSourceTree(dbg, "doc-minified.html");

  // Ensure that we can re-select the pretty printed source from the Source Tree
  await selectSourceFromSourceTree(dbg, "math.min.js");
  await waitForSelectedSource(dbg, "math.min.js:formatted");
  assertSourceIcon(dbg, "math.min.js", "prettyPrint");
  const tab = findElement(dbg, "activeTab");
  ok(tab.querySelector(".img.prettyPrint"));

  const focusedTreeElement = findElementWithSelector(
    dbg,
    ".sources-list .focused .label"
  );
  is(
    focusedTreeElement.textContent.trim(),
    "math.min.js",
    "Pretty printed source is selected in tree"
  );
});
