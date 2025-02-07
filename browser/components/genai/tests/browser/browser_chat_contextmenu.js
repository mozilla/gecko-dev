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
    set: [["browser.ml.chat.provider", "http://localhost:8080"]],
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
 * Check that the remove option resets provider
 */
add_task(async function test_remove_option() {
  Services.fog.testResetFOG();
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await openContextMenu();
    Assert.ok(
      Services.prefs.getStringPref("browser.ml.chat.provider"),
      "Provider is set"
    );

    const menu = document.getElementById("context-ask-chat");
    menu.getItemAtIndex(menu.itemCount - 1).click();
    await hideContextMenu();

    Assert.equal(
      Services.prefs.getStringPref("browser.ml.chat.provider"),
      "",
      "Provider reset"
    );

    const events = Glean.genaiChatbot.contextmenuRemove.testGetValue();
    Assert.equal(events.length, 1, "One remove event recorded");
    Assert.equal(events[0].extra.provider, "localhost", "Provider recorded");
  });
});

/**
 * Check tab behavior of chat menu items without sidebar pref
 */
add_task(async function test_open_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [
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
  Services.fog.testResetFOG();
  await SpecialPowers.pushPrefEnv({
    set: [
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

  const events = Glean.genaiChatbot.contextmenuPromptClick.testGetValue();
  Assert.equal(events.length, 1, "One context menu click");
  Assert.equal(events[0].extra.prompt, "summarize", "Picked summarize");
  Assert.equal(events[0].extra.provider, "localhost", "With localhost");
  Assert.equal(events[0].extra.selection, "0", "No selection");
});

/**
 * Check modified prompts record as custom
 */
add_task(async function test_custom_prompt() {
  Services.fog.testResetFOG();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.prompts.0", JSON.stringify({ id: "prompt" })],
      ["browser.ml.chat.provider", "http://localhost:8080"],
      ["browser.ml.chat.sidebar", true],
    ],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await openContextMenu();
    document.getElementById("context-ask-chat").getItemAtIndex(0).click();
    await hideContextMenu();
    SidebarController.hide();
  });

  Assert.equal(
    Glean.genaiChatbot.contextmenuPromptClick.testGetValue()[0].extra.prompt,
    "custom",
    "Custom id replaced with 'custom'"
  );
});
