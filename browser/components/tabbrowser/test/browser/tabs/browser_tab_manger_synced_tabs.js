add_setup(async function () {
  const previous = PlacesUIUtils.shouldShowTabsFromOtherComputersMenuitem;

  registerCleanupFunction(async () => {
    PlacesUIUtils.shouldShowTabsFromOtherComputersMenuitem = previous;
  });

  PlacesUIUtils.shouldShowTabsFromOtherComputersMenuitem = () => true;
});

add_task(async function tab_manager_synced_tabs() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  win.gTabsPanel.init();

  let button = win.document.getElementById("alltabs-button");
  let allTabsView = win.document.getElementById("allTabsMenu-allTabsView");
  let allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
    allTabsView,
    "ViewShown"
  );
  button.click();
  await allTabsPopupShownPromise;

  let syncedTabsButton = allTabsView.querySelector("#allTabsMenu-syncedTabs");
  is(syncedTabsButton.checkVisibility(), true, "item is visible");

  let sidebarShown = BrowserTestUtils.waitForEvent(win, "SidebarShown");
  syncedTabsButton.click();
  info("Waiting for sidebar to open");
  await sidebarShown;

  is(win.SidebarController.isOpen, true, "Sidebar is open");
  is(
    win.SidebarController.currentID,
    "viewTabsSidebar",
    "Synced tabs side bar is being displayed"
  );

  await BrowserTestUtils.closeWindow(win);
});
