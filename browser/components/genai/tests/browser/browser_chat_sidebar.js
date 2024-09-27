/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

// Used in multiple tests for loading a page in the sidebar
const TEST_CHAT_PROVIDER_URL = "http://mochi.test:8888/";

/**
 * Check that chat sidebar renders
 */
add_task(async function test_sidebar_render() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", TEST_CHAT_PROVIDER_URL]],
  });

  await SidebarController.show("viewGenaiChatSidebar");

  const provider =
    SidebarController.browser.contentWindow.document.getElementById("provider");
  Assert.ok(provider, "Rendered provider select");

  SidebarController.hide();
});

/**
 * Check that chat sidebar renders providers
 */
add_task(async function test_sidebar_providers() {
  const countVisible = () =>
    [
      ...SidebarController.browser.contentWindow.document.getElementById(
        "provider"
      ).options,
    ].filter(option => !option.hidden && option.value).length;

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", ""]],
  });
  await SidebarController.show("viewGenaiChatSidebar");

  const origCount = countVisible();
  Assert.equal(origCount, 5, "Rendered expected number of provider options");

  SidebarController.hide();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.hideLocalhost", false]],
  });
  await SidebarController.show("viewGenaiChatSidebar");

  Assert.equal(countVisible(), origCount + 1, "Added localhost option");

  SidebarController.hide();
});

/**
 * Check that onboarding renders
 */
add_task(async function test_sidebar_onboarding() {
  await SidebarController.show("viewGenaiChatSidebar");

  const { document } = SidebarController.browser.contentWindow;
  const label = await TestUtils.waitForCondition(() =>
    document.querySelector("label:has(.localhost)")
  );
  Assert.ok(label, "Got a provider");
  label.click();

  const pickButton = await TestUtils.waitForCondition(() =>
    document.querySelector(".chat_pick .primary:not([disabled])")
  );
  Assert.ok(pickButton, "Got button to activate provider");

  pickButton.click();

  const startButton = await TestUtils.waitForCondition(() =>
    document.querySelector(".chat_suggest .primary")
  );
  Assert.ok(startButton, "Got button to start");
  Assert.equal(
    Services.prefs.getStringPref("browser.ml.chat.provider"),
    "http://localhost:8080",
    "Provider pref changed during onboarding"
  );

  Services.prefs.clearUserPref("browser.ml.chat.provider");
  startButton.click();

  const noOnboarding = await TestUtils.waitForCondition(
    () => !document.getElementById("multi-stage-message-root")
  );
  Assert.ok(noOnboarding, "Onboarding container went away");

  SidebarController.hide();
});

/**
 * Check that more options menu renders
 */
add_task(async function test_sidebar_menu() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", "http://mochi.test:8888"]],
  });

  await SidebarController.show("viewGenaiChatSidebar");

  const button =
    SidebarController.browser.contentWindow.document.getElementById(
      "header-more"
    );
  Assert.ok(button, "Rendered more menu button");

  button.click();
  const popup = await TestUtils.waitForCondition(() =>
    document.getElementById("chatbot-menupopup")
  );

  Assert.ok(popup, "Menu popup created");
  let items = popup.querySelectorAll("menuitem");
  Assert.equal(items.length, 4, "Items added to menu");
  Assert.ok(items[1].hasAttribute("checked"), "Shortcuts shown");
  Assert.ok(!items[2].hasAttribute("checked"), "Shortcuts not hidden");

  // Disable shortcuts via menu
  items[2].click();
  const shown = BrowserTestUtils.waitForEvent(popup, "popupshown");
  Services.prefs.clearUserPref("browser.ml.chat.provider");
  button.click();
  await shown;

  items = popup.querySelectorAll("menuitem");
  Assert.ok(!items[1].hasAttribute("checked"), "Shortcuts not shown");
  Assert.ok(items[2].hasAttribute("checked"), "Shortcuts hidden");

  Services.prefs.clearUserPref("browser.ml.chat.shortcuts");
  const hidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  popup.hidePopup();
  await hidden;
  SidebarController.hide();
});

/**
 * Check that places doesn't get history entries from embedded provider browser
 */
add_task(async function test_sidebar_no_history() {
  // Earlier test opened sidebar with this test provider
  Assert.ok(
    !(await PlacesTestUtils.isPageInDB(TEST_CHAT_PROVIDER_URL)),
    "Earlier test with provider from test_sidebar_render is not in history"
  );
});
