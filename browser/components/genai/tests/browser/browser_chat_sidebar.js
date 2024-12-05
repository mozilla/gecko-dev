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
  Services.fog.testResetFOG();
  await SidebarController.show("viewGenaiChatSidebar");

  const { document, browserPromise } = SidebarController.browser.contentWindow;
  const label = await TestUtils.waitForCondition(() =>
    document.querySelector("label:has(.localhost)")
  );
  Assert.ok(label, "Got a provider");
  let events =
    Glean.genaiChatbot.onboardingProviderChoiceDisplayed.testGetValue();
  Assert.equal(events.length, 1, "Displayed onboarding once");
  Assert.equal(events[0].extra.provider, "none", "Opened with no provider");
  Assert.equal(events[0].extra.step, "1", "First step");
  const browser = await browserPromise;
  Assert.equal(browser.currentURI.spec, "about:blank", "Nothing loaded yet");

  label.click();

  await TestUtils.waitForCondition(
    () => browser.currentURI.spec != "about:blank",
    "Should have previewed provider"
  );

  Assert.notEqual(
    document.querySelector(":has(> .selected) [style]").style.maxHeight,
    "0px",
    "Selected provider expanded"
  );
  Assert.ok(browser.currentURI.spec, "Provider previewed");

  const pickButton = await TestUtils.waitForCondition(() =>
    document.querySelector(".chat_pick .primary:not([disabled])")
  );
  Assert.ok(pickButton, "Got button to activate provider");
  events = Glean.genaiChatbot.onboardingProviderSelection.testGetValue();
  Assert.equal(events.length, 1, "Selected one provider");
  Assert.equal(events[0].extra.provider, "localhost", "Picked localhost");
  Assert.equal(events[0].extra.step, "1", "First step");

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
  events = Glean.genaiChatbot.onboardingContinue.testGetValue();
  Assert.equal(events.length, 1, "Continued once");
  Assert.equal(
    events[0].extra.provider,
    "localhost",
    "Continued with localhost"
  );
  Assert.equal(events[0].extra.step, "1", "First step");
  events = await TestUtils.waitForCondition(() =>
    Glean.genaiChatbot.onboardingTextHighlightDisplayed.testGetValue()
  );
  Assert.equal(events.length, 1, "Displayed highlight once");
  Assert.equal(
    events[0].extra.provider,
    "localhost",
    "Continued with localhost"
  );
  Assert.equal(events[0].extra.step, "2", "Second step");

  Services.prefs.clearUserPref("browser.ml.chat.provider");
  startButton.click();

  const noOnboarding = await TestUtils.waitForCondition(
    () => !document.getElementById("multi-stage-message-root")
  );
  Assert.ok(noOnboarding, "Onboarding container went away");
  events = Glean.genaiChatbot.onboardingFinish.testGetValue();
  Assert.equal(events.length, 1, "Finished once");
  Assert.equal(
    events[0].extra.provider,
    "localhost",
    "Finished with localhost"
  );
  Assert.equal(events[0].extra.step, "2", "Second step");

  SidebarController.hide();
});

/**
 * Check that more options menu renders
 */
add_task(async function test_sidebar_menu() {
  Services.fog.testResetFOG();
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
  let events = Glean.genaiChatbot.sidebarMoreMenuDisplay.testGetValue();
  Assert.equal(events.length, 1, "Displayed menu once");
  Assert.equal(
    events[0].extra.provider,
    "custom",
    "Opened with custom provider"
  );

  // Disable shortcuts via menu
  items[2].click();
  const shown = BrowserTestUtils.waitForEvent(popup, "popupshown");
  Services.prefs.clearUserPref("browser.ml.chat.provider");
  button.click();
  await shown;

  items = popup.querySelectorAll("menuitem");
  Assert.ok(!items[1].hasAttribute("checked"), "Shortcuts not shown");
  Assert.ok(items[2].hasAttribute("checked"), "Shortcuts hidden");
  events = Glean.genaiChatbot.sidebarMoreMenuClick.testGetValue();
  Assert.equal(events.length, 1, "Clicked menu once");
  Assert.equal(
    events[0].extra.action,
    "hide_shortcuts",
    "Hide shortcuts clicked"
  );
  Assert.equal(events[0].extra.provider, "custom", "Still custom provider");
  Assert.equal(
    Glean.genaiChatbot.sidebarMoreMenuDisplay.testGetValue().length,
    2,
    "Opened second time"
  );

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

/**
 * Check that keyboard shortcut toggles and enables chatbot
 */
add_task(async function test_keyboard_shortcut() {
  const key = document.getElementById("viewGenaiChatSidebarKb");
  const enabled = "browser.ml.chat.enabled";
  await SpecialPowers.pushPrefEnv({ set: [[enabled, false]] });

  key.doCommand();

  Assert.ok(Services.prefs.getBoolPref(enabled), "Enabled with keyboard");
  Assert.ok(SidebarController.isOpen, "Opened chatbot with keyboard");

  key.doCommand();

  Assert.ok(!SidebarController.isOpen, "Closed chatbot with keyboard");
  const events = Glean.genaiChatbot.keyboardShortcut.testGetValue();
  Assert.equal(events.length, 2, "Got 2 keyboard events");
  Assert.equal(events[0].extra.enabled, "false", "Initially disabled");
  Assert.equal(events[0].extra.sidebar, "", "Initially closed");
  Assert.equal(events[1].extra.enabled, "true", "Already enabled");
  Assert.equal(
    events[1].extra.sidebar,
    "viewGenaiChatSidebar",
    "Already opened"
  );
});
