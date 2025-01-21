/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.revamp", true]],
  });
});

registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_close_bookmarks_panel() {
  let sidebarBox = document.getElementById("sidebar-box");
  let sidebar = document.getElementById("sidebar");
  ok(sidebarBox.hidden, "The sidebar should be hidden");

  await SidebarController.show("viewBookmarksSidebar");
  ok(!sidebarBox.hidden, "The sidebar is shown");

  sidebar.contentDocument.getElementById("sidebar-panel-close").click();
  ok(sidebarBox.hidden, "The sidebar should be hidden");
});
