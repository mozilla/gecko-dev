/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1895789 to standarize contextmenu helpers in BrowserTestUtils
async function openContextMenu() {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  const promise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouse(
    null,
    0,
    0,
    { type: "contextmenu" },
    gBrowser.selectedBrowser
  );
  await promise;
}

async function hideContextMenu() {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  const promise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  await promise;
}

/**
 * Check that the chat context menu is hidden by default
 */
add_task(async function test_hidden_menu() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await openContextMenu();
    Assert.ok(
      document.getElementById("context-ask-chat").hidden,
      "Ask chat menu is hidden"
    );
    await hideContextMenu();
  });
});

/**
 * Check that chat context menu is shown with appropriate prefs set
 */
add_task(async function test_menu_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.enabled", true],
      ["browser.ml.chat.provider", "http://localhost:8080"],
    ],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await openContextMenu();
    Assert.ok(
      !document.getElementById("context-ask-chat").hidden,
      "Ask chat menu is shown"
    );
    await hideContextMenu();
  });
});

/**
 * Check tab behavior of chat menu items without sidebar pref
 */
add_task(async function test_open_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.enabled", true],
      ["browser.ml.chat.provider", "http://localhost:8080"],
      ["browser.ml.chat.sidebar", false],
    ],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    const origTabs = gBrowser.tabs.length;
    await openContextMenu();
    await BrowserTestUtils.switchTab(gBrowser, () =>
      document.getElementById("context-ask-chat").getItemAtIndex(0).click()
    );
    await hideContextMenu();

    Assert.equal(gBrowser.tabs.length, origTabs + 1, "Chat opened tabs");
    Assert.ok(!SidebarController.isOpen, "Chat did not open sidebar");
    gBrowser.removeTab(gBrowser.selectedTab);
  });
});

/**
 * Check sidebar behavior of chat menu items with sidebar pref
 */
add_task(async function test_open_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.enabled", true],
      ["browser.ml.chat.provider", "http://localhost:8080"],
      ["browser.ml.chat.sidebar", true],
    ],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    const origTabs = gBrowser.tabs.length;
    await openContextMenu();
    document.getElementById("context-ask-chat").getItemAtIndex(0).click();
    await hideContextMenu();

    Assert.equal(gBrowser.tabs.length, origTabs, "Chat did not open tab");
    Assert.ok(SidebarController.isOpen, "Chat opened sidebar");
    SidebarController.hide();
  });
});
