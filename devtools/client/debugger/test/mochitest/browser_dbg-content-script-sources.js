/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the content scripts are listed in the source tree.

"use strict";

add_task(async function () {
  const extension = await installAndStartContentScriptExtension();

  let dbg = await initDebugger("doc-content-script-sources.html");
  ok(
    !sourceExists(dbg, "content_script.js"),
    "The extension content script isn't reported to the frontend and isn't in the reducer"
  );

  info("But the content script isn't visible by default");
  await waitForSourcesInSourceTree(dbg, [
    "doc-content-script-sources.html",
    "doc-strict.html",
  ]);

  info("Enable the content script setting");
  await toggleSourcesTreeSettingsMenuItem(dbg, {
    className: ".debugger-settings-menu-item-show-content-scripts",
    isChecked: false,
  });

  info("The extension content script should now be visible in the source tree");
  await waitForSourcesInSourceTree(dbg, [
    "doc-content-script-sources.html",
    "doc-strict.html",
    "content_script.js",
  ]);

  await waitForSources(dbg, "content_script.js");
  await selectSource(dbg, "content_script.js");
  await closeTab(dbg, "content_script.js");

  const sourceTreeThreadLabels = [
    ...findAllElements(dbg, "sourceTreeThreads"),
  ].map(el => {
    return el.textContent;
  });
  // Verify that the content script is below the target of the frame it was executed against
  Assert.deepEqual(
    sourceTreeThreadLabels,
    ["Main Thread", "Test content script extension", "Debugger test page"],
    "The threads are correctly ordered"
  );

  // Destroy the toolbox and repeat the test in a new toolbox
  // and ensures that the content script is still listed.
  await dbg.toolbox.destroy();

  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "jsdebugger");
  dbg = createDebuggerContext(toolbox);
  await waitForSources(dbg, "content_script.js");
  await selectSource(dbg, "content_script.js");

  await addBreakpoint(dbg, "content_script.js", 2);

  for (let i = 1; i < 3; i++) {
    info(`Reloading tab (${i} time)`);
    gBrowser.reloadTab(gBrowser.selectedTab);
    await waitForPaused(dbg);
    await waitForSelectedSource(dbg, "content_script.js");

    await waitFor(
      () => findElementWithSelector(dbg, ".sources-list .focused"),
      "Source is focused"
    );
    await assertPausedAtSourceAndLine(
      dbg,
      findSource(dbg, "content_script.js").id,
      2
    );
    await resume(dbg);
  }

  await resume(dbg);
  assertNotPaused(dbg);

  await closeTab(dbg, "content_script.js");

  is(
    dbg.selectors.getAllThreads().length,
    2,
    "Ensure that we only get the main thread and the content script thread"
  );

  await extension.unload();
  await waitFor(
    () => dbg.selectors.getAllThreads().length == 1,
    "After unloading the add-on, the content script thread is removed"
  );
});
