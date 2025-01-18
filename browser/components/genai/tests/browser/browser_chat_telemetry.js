/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that chat set default telemetry
 */
add_task(async function test_default_telemetry() {
  // These metrics rely on startup init, which is skipped in repeat verify
  Assert.equal(
    Glean.genaiChatbot.enabled.testGetValue() ?? true,
    true,
    "Default enabled for test"
  );
  Assert.equal(
    Glean.genaiChatbot.provider.testGetValue() ?? "none",
    "none",
    "Default no provider for test"
  );
  Assert.equal(
    Glean.genaiChatbot.shortcuts.testGetValue() ?? true,
    true,
    "Default shortcuts for test"
  );
  Assert.equal(
    Glean.genaiChatbot.shortcutsCustom.testGetValue() ?? true,
    true,
    "Default shortcuts_custom for test"
  );
  Assert.equal(
    Glean.genaiChatbot.sidebar.testGetValue() ?? true,
    true,
    "Default sidebar for test"
  );

  Services.fog.testResetFOG();
  await SidebarController.show("viewGenaiChatSidebar");
  SidebarController.hide();

  const events = Glean.genaiChatbot.sidebarToggle.testGetValue();
  const sidebarVersion = SidebarController.sidebarRevampEnabled ? "new" : "old";
  Assert.equal(events.length, 2, "Sidebar toggled twice");
  Assert.equal(events[0].extra.opened, "true", "First opened");
  Assert.equal(events[0].extra.provider, "none", "No provider");
  Assert.equal(events[0].extra.reason, "load", "Page loaded");
  Assert.equal(events[0].extra.version, sidebarVersion, "Correct version");
  Assert.equal(events[1].extra.opened, "false", "Second not opened");
  Assert.equal(events[1].extra.provider, "none", "Still no provider");
  Assert.equal(events[1].extra.reason, "unload", "Page unloaded");
  Assert.equal(events[1].extra.version, sidebarVersion, "Correct version");

  Assert.equal(
    Glean.genaiChatbot.experimentCheckboxClick.testGetValue(),
    null,
    "No experiment events"
  );
  Assert.equal(
    Glean.genaiChatbot.providerChange.testGetValue(),
    null,
    "No provider change events"
  );
  Assert.equal(
    Glean.genaiChatbot.contextmenuPromptClick.testGetValue(),
    null,
    "No context menu events"
  );
});

/**
 * Check that telemetry detects changes
 */
add_task(async function test_telemetry_change() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.hideLocalhost", false],
      ["browser.ml.chat.provider", "http://mochi.test:8888"],
    ],
  });
  await SidebarController.show("viewGenaiChatSidebar");
  SidebarController.browser.focus();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", "http://localhost:8080"]],
  });

  Assert.equal(
    Glean.genaiChatbot.provider.testGetValue(),
    "localhost",
    "Metric switched to localhost"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", ""]],
  });
  SidebarController.hide();

  Assert.equal(
    Glean.genaiChatbot.provider.testGetValue(),
    "none",
    "Metric switched to none"
  );
  const events = Glean.genaiChatbot.providerChange.testGetValue();
  Assert.equal(events.length, 2, "Two provider change");
  Assert.equal(events[0].extra.previous, "custom", "From custom");
  Assert.equal(events[0].extra.current, "localhost", "To localhost");
  Assert.equal(events[0].extra.surface, "panel", "Using sidebar panel");
  Assert.equal(events[1].extra.previous, "localhost", "From localhost");
  Assert.equal(events[1].extra.current, "none", "To none");
  Assert.equal(events[1].extra.surface, "panel", "Using sidebar panel");
});
