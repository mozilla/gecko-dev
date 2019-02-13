/**
 * Test enabling/disabling the entire ReadingList feature via the
 * browser.readinglist.enabled preference.
 */

function checkRLState() {
  let enabled = RLUtils.enabled;
  info("Checking ReadingList UI is " + (enabled ? "enabled" : "disabled"));

  let sidebarBroadcaster = document.getElementById("readingListSidebar");
  let sidebarMenuitem = document.getElementById("menu_readingListSidebar");

  let bookmarksMenubarItem = document.getElementById("menu_readingList");
  let bookmarksMenubarSeparator = document.getElementById("menu_readingListSeparator");

  if (enabled) {
    Assert.notEqual(sidebarBroadcaster.getAttribute("hidden"), "true",
                    "Sidebar broadcaster should not be hidden");
    Assert.notEqual(sidebarMenuitem.getAttribute("hidden"), "true",
                    "Sidebar menuitem should be visible");

    // Currently disabled on OSX.
    if (bookmarksMenubarItem) {
      Assert.notEqual(bookmarksMenubarItem.getAttribute("hidden"), "true",
                      "RL bookmarks submenu in menubar should not be hidden");
      Assert.notEqual(sidebarMenuitem.getAttribute("hidden"), "true",
                      "RL bookmarks separator in menubar should be visible");
    }
  } else {
    Assert.equal(sidebarBroadcaster.getAttribute("hidden"), "true",
                 "Sidebar broadcaster should be hidden");
    Assert.equal(sidebarMenuitem.getAttribute("hidden"), "true",
                 "Sidebar menuitem should be hidden");
    Assert.equal(ReadingListUI.isSidebarOpen, false,
                 "ReadingListUI should not think sidebar is open");

    // Currently disabled on OSX.
    if (bookmarksMenubarItem) {
      Assert.equal(bookmarksMenubarItem.getAttribute("hidden"), "true",
                      "RL bookmarks submenu in menubar should not be hidden");
      Assert.equal(sidebarMenuitem.getAttribute("hidden"), "true",
                      "RL bookmarks separator in menubar should be visible");
    }
  }

  if (!enabled) {
    Assert.equal(SidebarUI.isOpen, false, "Sidebar should not be open");
  }
}

add_task(function*() {
  info("Start with ReadingList disabled");
  RLUtils.enabled = false;
  checkRLState();
  info("Enabling ReadingList");
  RLUtils.enabled = true;
  checkRLState();

  info("Opening ReadingList sidebar");
  yield ReadingListUI.showSidebar();
  Assert.ok(SidebarUI.isOpen, "Sidebar should be open");
  Assert.equal(SidebarUI.currentID, "readingListSidebar", "Sidebar should have ReadingList loaded");

  info("Disabling ReadingList");
  RLUtils.enabled = false;
  Assert.ok(!SidebarUI.isOpen, "Sidebar should be closed");
  checkRLState();
});
