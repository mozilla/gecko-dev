/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_URL = BASE + "browser_contextmenu_shareurl.html";

/**
 * Test the "Share" item in the tab contextmenu on Linux.
 */
add_task(async function test_contextmenu_share_linux() {
  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    await openTabContextMenu(gBrowser.selectedTab);

    let contextMenu = document.getElementById("tabContextMenu");
    let contextMenuClosedPromise = BrowserTestUtils.waitForPopupEvent(
      contextMenu,
      "hidden"
    );
    let itemCreated = contextMenu.querySelector(".share-tab-url-item");
    ok(itemCreated, "Got Share item on Linux");
    await SimpleTest.promiseClipboardChange(TEST_URL, () =>
      contextMenu.activateItem(itemCreated)
    );
    ok(true, "Copied to clipboard.");

    await contextMenuClosedPromise;
  });
});

/**
 * Helper for opening the toolbar context menu.
 */
async function openTabContextMenu(tab) {
  info("Opening tab context menu");
  let contextMenu = document.getElementById("tabContextMenu");
  let openTabContextMenuPromise = BrowserTestUtils.waitForPopupEvent(
    contextMenu,
    "shown"
  );

  EventUtils.synthesizeMouseAtCenter(tab, { type: "contextmenu" });
  await openTabContextMenuPromise;
}
