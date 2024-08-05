/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that shortcuts aren't shown by default
 */
add_task(async function test_no_shortcuts() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", "http://localhost:8080"]],
  });
  await BrowserTestUtils.withNewTab("data:text/plain,hi", async browser => {
    browser.focus();
    goDoCommand("cmd_selectAll");
    Assert.ok(
      !document.querySelector(".content-shortcuts"),
      "No shortcuts found"
    );
  });
});

/**
 * Check that shortcuts get shown on selection and open popup and sidebar
 */
add_task(async function test_show_shortcuts() {
  Services.fog.testResetFOG();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.shortcuts", true]],
  });
  await BrowserTestUtils.withNewTab("data:text/plain,hi", async browser => {
    await SimpleTest.promiseFocus(browser);
    const selectPromise = SpecialPowers.spawn(browser, [], () => {
      ContentTaskUtils.waitForCondition(() => content.getSelection());
    });
    goDoCommand("cmd_selectAll");
    await selectPromise;
    BrowserTestUtils.synthesizeMouseAtCenter(
      browser,
      { type: "mouseup" },
      browser
    );
    const shortcuts = await TestUtils.waitForCondition(() =>
      document.querySelector(".content-shortcuts[shown]")
    );
    Assert.ok(shortcuts, "Shortcuts added on select");
    let events = Glean.genaiChatbot.shortcutsDisplayed.testGetValue();
    Assert.equal(events.length, 1, "Shortcuts shown once");
    Assert.equal(events[0].extra.selection, 2, "Selected hi");

    const popup = document.getElementById("ask-chat-shortcuts");
    Assert.equal(popup.state, "closed", "Popup is closed");

    EventUtils.sendMouseEvent({ type: "mouseover" }, shortcuts);
    await BrowserTestUtils.waitForEvent(popup, "popupshown");

    Assert.equal(popup.state, "open", "Popup is open");
    events = Glean.genaiChatbot.shortcutsExpanded.testGetValue();
    Assert.equal(events.length, 1, "One shortcuts opened");
    Assert.equal(events[0].extra.selection, 2, "Selected hi");

    Assert.ok(!SidebarController.isOpen, "Sidebar is closed");
    popup.querySelector("toolbarbutton").click();

    Assert.ok(SidebarController.isOpen, "Chat opened sidebar");
    events = Glean.genaiChatbot.shortcutsPromptClick.testGetValue();
    Assert.equal(events.length, 1, "One shortcut clicked");
    Assert.equal(events[0].extra.prompt, "summarize", "Picked summarize");
    Assert.equal(events[0].extra.provider, "localhost", "With localhost");
    Assert.equal(events[0].extra.selection, 2, "Selected hi");

    SidebarController.hide();
  });
});
