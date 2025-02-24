/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  const dbg = await initDebugger(
    "doc-sourcemaps3.html",
    "bundle.js",
    "sorted.js",
    "test.js"
  );

  const sortedSrc = findSource(dbg, "sorted.js");
  const bundleSrc = findSource(dbg, "bundle.js");

  info("Check that override context menu item is disabled for original files");
  await selectSource(dbg, sortedSrc);
  let waitForPopup = waitForContextMenu(dbg);
  rightClickEl(dbg, findSourceNodeWithText(dbg, "sorted.js"));
  let popup = await waitForPopup;
  await assertContextMenuItemDisabled(dbg, "#node-menu-overrides", true);
  await closeContextMenu(dbg, popup);

  info("Check that override context menu item is enabled for generated files");
  await selectSource(dbg, bundleSrc);
  waitForPopup = waitForContextMenu(dbg);
  rightClickEl(dbg, findSourceNodeWithText(dbg, "bundle.js"));
  popup = await waitForPopup;
  await assertContextMenuItemDisabled(dbg, "#node-menu-overrides", false);
  await closeContextMenu(dbg, popup);
});
