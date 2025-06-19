/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

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

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["test.wait300msAfterTabSwitch", true]],
  });
});

/**
 * Check page menu has summarize prompt
 */
add_task(async function test_page_menu_prompt() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub(GenAI, "handleAskChat");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.provider", "http://localhost:8080"],
      ["browser.ml.chat.page", true],
    ],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await openContextMenu();
    await TestUtils.waitForCondition(
      () =>
        document.getElementById("context-ask-chat").getItemAtIndex(0)?.label ==
        "Summarize Page",
      "page prompt added"
    );
    document.getElementById("context-ask-chat").getItemAtIndex(0).click();
    await hideContextMenu();
  });

  Assert.equal(stub.callCount, 1, "one menu prompt");
  Assert.equal(stub.firstCall.args[0].id, "summarize", "summarize prompt");
  Assert.ok(stub.firstCall.args[0].badge, "new badge");

  sandbox.restore();
  SidebarController.hide();
});

/**
 * Check badge toggle by prefs
 */
add_task(async function test_toggle_new_badge() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.provider", "http://localhost:8080"],
      ["browser.ml.chat.sidebar", true],
      ["browser.ml.chat.page", true],
      ["browser.ml.chat.page.footerBadge", true],
    ],
  });

  await SidebarController.show("viewGenaiChatSidebar");

  const { document } = SidebarController.browser.contentWindow;
  const buttonContainer = document.getElementById("summarize-btn-container");
  const badge = buttonContainer.querySelector(".badge");

  Assert.notEqual(
    getComputedStyle(badge).display,
    "none",
    "new badge set visible"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.page.footerBadge", false]],
  });

  await TestUtils.waitForCondition(
    () => getComputedStyle(badge).display == "none",
    "Badge changed by css"
  );

  Assert.equal(
    getComputedStyle(badge).display,
    "none",
    "new badge set dismissed"
  );
});

/**
 * Test badge dismissal and check if summarizeCurrentPage() is executed
 */
add_task(async function test_click_summarize_button() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.page.footerBadge", true]],
  });

  const { document } = SidebarController.browser.contentWindow;
  const summarizeButton = document.getElementById("summarize-button");

  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub(
    SidebarController.browser.contentWindow,
    "summarizeCurrentPage"
  );

  summarizeButton.click();

  Assert.equal(
    Services.prefs.getBoolPref("browser.ml.chat.page.footerBadge"),
    false
  );
  Assert.equal(stub.callCount, 1);

  stub.restore();
  SidebarController.hide();
});
