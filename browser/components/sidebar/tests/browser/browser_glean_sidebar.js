/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(10);

const lazy = {};

const TAB_DIRECTION_PREF = "sidebar.verticalTabs";

ChromeUtils.defineESModuleGetters(lazy, {
  TabsSetupFlowManager:
    "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
});

add_setup(async () => {
  SidebarController.init();
  await TestUtils.waitForTick();
});
registerCleanupFunction(() => {
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[0]);
  }
});

function getExpectedVersionString() {
  return SidebarController.sidebarRevampEnabled ? "new" : "old";
}

add_task(async function test_metrics_initialized() {
  const metrics = ["displaySettings", "positionSettings", "tabsLayout"];
  for (const metric of metrics) {
    Assert.notEqual(
      Glean.sidebar[metric].testGetValue(),
      null,
      `${metric} is initialized.`
    );
  }
  Services.fog.testResetFOG();
});

add_task(async function test_sidebar_expand() {
  await SpecialPowers.pushPrefEnv({
    set: [[TAB_DIRECTION_PREF, true]],
  });
  // Vertical tabs are expanded by default
  await SidebarController.initializeUIState({ launcherExpanded: false });

  info("Expand the sidebar.");
  EventUtils.synthesizeMouseAtCenter(SidebarController.toolbarButton, {});
  await TestUtils.waitForCondition(
    () => SidebarController.sidebarMain.expanded,
    "Sidebar is expanded."
  );

  info("Collapse the sidebar.");
  EventUtils.synthesizeMouseAtCenter(SidebarController.toolbarButton, {});
  await TestUtils.waitForCondition(
    () => !SidebarController.sidebarMain.expanded,
    "Sidebar is collapsed."
  );

  const events = Glean.sidebar.expand.testGetValue();
  Assert.equal(events?.length, 2, "Two events were reported.");

  await SpecialPowers.popPrefEnv();
});

async function testSidebarToggle(commandID, gleanEvent, otherCommandID) {
  info(`Load the ${commandID} panel.`);
  await SidebarController.show(commandID);

  let events = gleanEvent.testGetValue();
  Assert.equal(events?.length, 1, "One event was reported.");
  Assert.equal(
    events[0].extra.opened,
    "true",
    "Event indicates that the panel was opened."
  );

  if (otherCommandID) {
    info(`Load the ${otherCommandID} panel.`);
    await SidebarController.show(otherCommandID);
  } else {
    info(`Unload the ${commandID} panel.`);
    SidebarController.hide();
  }

  events = gleanEvent.testGetValue();
  Assert.equal(events?.length, 2, "Two events were reported.");
  Assert.equal(
    events[1].extra.opened,
    "false",
    "Event indicates that the panel was closed."
  );
  if (otherCommandID) {
    SidebarController.hide();
  }
}

add_task(async function test_history_sidebar_toggle() {
  const gleanEvent = Glean.history.sidebarToggle;
  await testSidebarToggle(
    "viewHistorySidebar",
    gleanEvent,
    "viewBookmarksSidebar"
  );
  for (const { extra } of gleanEvent.testGetValue()) {
    Assert.equal(
      extra.version,
      getExpectedVersionString(),
      "Event has the correct sidebar version."
    );
  }
});

async function test_synced_tabs_sidebar_toggle(revampEnabled) {
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.revamp", revampEnabled]],
  });
  await testSidebarToggle("viewTabsSidebar", Glean.syncedTabs.sidebarToggle);
  let events = Glean.syncedTabs.sidebarToggle.testGetValue();
  for (const { extra } of events) {
    Assert.equal(
      extra.synced_tabs_loaded,
      "false",
      "Event indicates that synced tabs aren't loaded yet."
    );
    Assert.equal(
      extra.version,
      getExpectedVersionString(),
      "Event has the correct sidebar version."
    );
  }

  // Repeat test while simulating synced tabs loaded state.
  Services.fog.testResetFOG();
  const sandbox = sinon.createSandbox();
  if (revampEnabled) {
    sandbox.stub(lazy.TabsSetupFlowManager, "uiStateIndex").value(4);
  } else {
    sandbox.stub(lazy.UIState, "get").returns({ status: "signed_in" });
  }

  await testSidebarToggle("viewTabsSidebar", Glean.syncedTabs.sidebarToggle);
  events = Glean.syncedTabs.sidebarToggle.testGetValue();
  for (const { extra } of events) {
    Assert.equal(
      extra.synced_tabs_loaded,
      "true",
      "Event indicates that synced tabs are now loaded."
    );
    Assert.equal(
      extra.version,
      getExpectedVersionString(),
      "Event has the correct sidebar version."
    );
  }

  sandbox.restore();
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
}

add_task(async function test_synced_tabs_sidebar_toggle_legacy() {
  await test_synced_tabs_sidebar_toggle(false);
});

add_task(async function test_synced_tabs_sidebar_toggle_revamp() {
  await test_synced_tabs_sidebar_toggle(true);
});

add_task(async function test_bookmarks_sidebar_toggle() {
  const gleanEvent = Glean.bookmarks.sidebarToggle;
  await testSidebarToggle("viewBookmarksSidebar", gleanEvent);
  for (const { extra } of gleanEvent.testGetValue()) {
    Assert.equal(
      extra.version,
      getExpectedVersionString(),
      "Event has the correct sidebar version."
    );
  }
});

add_task(async function test_extension_sidebar_toggle() {
  info("Load an extension.");
  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");

  let events = Glean.extension.sidebarToggle.testGetValue();
  Assert.equal(events.length, 1, "One event was reported.");
  const {
    extra: { opened, addon_id, addon_name, version },
  } = events[0];
  Assert.ok(opened, "Event indicates the panel was opened.");
  Assert.ok(addon_id, "Event has the extension's ID.");
  Assert.ok(addon_name, "Event has the extension's name.");
  Assert.equal(
    version,
    getExpectedVersionString(),
    "Event has the correct sidebar version."
  );

  info("Unload the extension.");
  await extension.unload();

  events = Glean.extension.sidebarToggle.testGetValue();
  Assert.equal(events?.length, 2, "Two events were reported.");
});

add_task(async function test_review_checker_sidebar_toggle() {
  const gleanEvent = Glean.shopping.sidebarToggle;
  await testSidebarToggle("viewReviewCheckerSidebar", gleanEvent);
  for (const { extra } of gleanEvent.testGetValue()) {
    Assert.equal(
      extra.version,
      getExpectedVersionString(),
      "Event has the correct sidebar version."
    );
  }
});

add_task(async function test_customize_panel_toggle() {
  await testSidebarToggle(
    "viewCustomizeSidebar",
    Glean.sidebarCustomize.panelToggle
  );
});

add_task(async function test_customize_icon_click() {
  info("Click on the gear icon.");
  const { customizeButton } = SidebarController.sidebarMain;
  EventUtils.synthesizeMouseAtCenter(customizeButton, {});

  const events = Glean.sidebarCustomize.iconClick.testGetValue();
  Assert.equal(events?.length, 1, "One event was reported.");

  SidebarController.hide();
});

async function testCustomizeToggle(commandID, gleanEvent, checked = true) {
  await SidebarController.show("viewCustomizeSidebar");
  const customizeComponent =
    SidebarController.browser.contentDocument.querySelector(
      "sidebar-customize"
    );
  const checkbox = customizeComponent.shadowRoot.querySelector(`#${commandID}`);

  info(`Toggle ${commandID}.`);
  EventUtils.synthesizeMouseAtCenter(
    checkbox,
    {},
    SidebarController.browser.contentWindow
  );
  Assert.equal(
    checkbox.checked,
    !checked,
    `Checkbox is ${checked ? "un" : ""}checked.`
  );
  let events = gleanEvent.testGetValue();
  Assert.equal(events?.length, 1, "One event was reported.");
  Assert.deepEqual(
    events[0].extra,
    { checked: `${!checked}` },
    `Event indicates that the box was ${checked ? "un" : ""}checked.`
  );

  info(`Re-toggle ${commandID}.`);
  EventUtils.synthesizeMouseAtCenter(
    checkbox,
    {},
    SidebarController.browser.contentWindow
  );
  Assert.equal(
    checkbox.checked,
    checked,
    `Checkbox is ${checked ? "" : "un"}checked.`
  );
  events = gleanEvent.testGetValue();
  Assert.equal(events?.length, 2, "Two events were reported.");
  Assert.deepEqual(
    events[1].extra,
    { checked: `${checked}` },
    `Event indicates that the box was ${checked ? "" : "un"}checked.`
  );

  SidebarController.hide();
}

add_task(async function test_customize_chatbot_enabled() {
  await SpecialPowers.pushPrefEnv({ set: [["browser.ml.chat.enabled", true]] });
  await testCustomizeToggle(
    "viewGenaiChatSidebar",
    Glean.sidebarCustomize.chatbotEnabled
  );
});

add_task(async function test_customize_synced_tabs_enabled() {
  await testCustomizeToggle(
    "viewTabsSidebar",
    Glean.sidebarCustomize.syncedTabsEnabled
  );
});

add_task(async function test_customize_history_enabled() {
  await testCustomizeToggle(
    "viewHistorySidebar",
    Glean.sidebarCustomize.historyEnabled
  );
});

add_task(async function test_customize_bookmarks_enabled() {
  await testCustomizeToggle(
    "viewBookmarksSidebar",
    Glean.sidebarCustomize.bookmarksEnabled,
    false
  );
});

add_task(async function test_customize_review_checker_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.integratedSidebar", true]],
  });
  await testCustomizeToggle(
    "viewReviewCheckerSidebar",
    Glean.sidebarCustomize.shoppingReviewCheckerEnabled,
    false
  );
});

add_task(async function test_customize_extensions_clicked() {
  info("Load an extension.");
  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");

  info("Load the customize panel.");
  await SidebarController.show("viewCustomizeSidebar");
  const customizeComponent =
    SidebarController.browser.contentDocument.querySelector(
      "sidebar-customize"
    );

  info("Click on the extension link.");
  const deferredEMLoaded = Promise.withResolvers();
  Services.obs.addObserver(function observer(_, topic) {
    Services.obs.removeObserver(observer, topic);
    deferredEMLoaded.resolve();
  }, "EM-loaded");
  EventUtils.synthesizeMouseAtCenter(
    customizeComponent.extensionLinks[0],
    {},
    SidebarController.browser.contentWindow
  );
  await deferredEMLoaded.promise;

  const events = Glean.sidebarCustomize.extensionsClicked.testGetValue();
  Assert.equal(events.length, 1, "One event was reported.");

  await extension.unload();
  SidebarController.hide();
});

async function testCustomizeSetting(
  input,
  gleanEventOrMetric,
  firstExpected,
  secondExpected
) {
  await SidebarController.show("viewCustomizeSidebar");
  const { contentDocument } = SidebarController.browser;
  const component = contentDocument.querySelector("sidebar-customize");

  info(`Toggle the setting for ${input}.`);
  // Use click as the rect in synthesizeMouseAtCenter can change for the vertical tabs toggle
  component[input].click();
  await TestUtils.waitForTick();
  let value = gleanEventOrMetric.testGetValue();
  if (Array.isArray(value)) {
    Assert.equal(value.length, 1, "One event was reported.");
    Assert.deepEqual(value[0].extra, firstExpected);
  } else {
    Assert.equal(value, firstExpected);
  }

  info(`Toggle the setting for ${input}.`);
  component[input].click();
  await TestUtils.waitForTick();
  value = gleanEventOrMetric.testGetValue();
  if (Array.isArray(value)) {
    Assert.equal(value.length, 2, "Two events were reported.");
    Assert.deepEqual(value[1].extra, secondExpected);
  } else {
    Assert.equal(value, secondExpected);
  }

  SidebarController.hide();
}

add_task(async function test_customize_sidebar_display() {
  await SpecialPowers.pushPrefEnv({
    set: [[TAB_DIRECTION_PREF, true]],
  });
  await testCustomizeSetting(
    "visibilityInput",
    Glean.sidebarCustomize.sidebarDisplay,
    { preference: "hide" },
    { preference: "always" }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_customize_sidebar_position() {
  await testCustomizeSetting(
    "positionInput",
    Glean.sidebarCustomize.sidebarPosition,
    { position: "right" },
    { position: "left" }
  );
});

add_task(async function test_customize_tabs_layout() {
  await testCustomizeSetting(
    "verticalTabsInput",
    Glean.sidebarCustomize.tabsLayout,
    { orientation: "vertical" },
    { orientation: "horizontal" }
  );
});

add_task(async function test_customize_firefox_settings_clicked() {
  await SidebarController.show("viewCustomizeSidebar");
  const { contentDocument, contentWindow } = SidebarController.browser;
  const component = contentDocument.querySelector("sidebar-customize");
  let settingsLink = component.shadowRoot.querySelector("#manage-settings > a");
  settingsLink.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(settingsLink, {}, contentWindow);
  const events = Glean.sidebarCustomize.firefoxSettingsClicked.testGetValue();
  Assert.equal(events.length, 1, "One event was reported.");

  SidebarController.hide();
});

add_task(async function test_sidebar_resize() {
  await SpecialPowers.pushPrefEnv({
    set: [[TAB_DIRECTION_PREF, true]],
  });
  await SidebarController.show("viewHistorySidebar");
  const originalWidth = SidebarController._box.style.width;
  SidebarController._box.style.width = "500px";
  SidebarController._splitter.dispatchEvent(new CustomEvent("command"));

  const events = await TestUtils.waitForCondition(
    () => Glean.sidebar.resize.testGetValue(),
    "Resize events are available."
  );
  Assert.ok(events[0], "One event was reported.");
  const {
    extra: { current, previous, percentage },
  } = events[0];
  Assert.ok(current, "Current width is provided.");
  Assert.ok(previous, "Previous width is provided.");
  Assert.ok(percentage, "Window percentage is provided.");

  const metric = Glean.sidebar.width.testGetValue();
  Assert.ok(metric, "Sidebar width is recorded.");

  SidebarController._box.style.width = originalWidth;
  SidebarController.hide();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_sidebar_display_settings() {
  await SpecialPowers.pushPrefEnv({
    set: [[TAB_DIRECTION_PREF, true]],
  });
  await testCustomizeSetting(
    "visibilityInput",
    Glean.sidebar.displaySettings,
    "hide",
    "always"
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_sidebar_position_settings() {
  await testCustomizeSetting(
    "positionInput",
    Glean.sidebar.positionSettings,
    "right",
    "left"
  );
});

add_task(async function test_sidebar_tabs_layout() {
  await testCustomizeSetting(
    "verticalTabsInput",
    Glean.sidebar.tabsLayout,
    "vertical",
    "horizontal"
  );
});

add_task(async function test_sidebar_position_rtl_ui() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(window, "RTL_UI").value(true);
  await SpecialPowers.pushPrefEnv({ set: [["intl.l10n.pseudo", "bidi"]] });
  Services.fog.testResetFOG();

  // When RTL is enabled, sidebar is shown on the right by default.
  // Toggle position setting to move it to the left, then back to the right.
  await testCustomizeSetting(
    "positionInput",
    Glean.sidebarCustomize.sidebarPosition,
    { position: "left" },
    { position: "right" }
  );
  await testCustomizeSetting(
    "positionInput",
    Glean.sidebar.positionSettings,
    "left",
    "right"
  );

  sandbox.restore();
  await SpecialPowers.popPrefEnv();
});

async function testIconClick(expanded) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.enabled", true],
      ["sidebar.main.tools", "aichat,syncedtabs,history,bookmarks"],
      [TAB_DIRECTION_PREF, true],
    ],
  });

  const { sidebarMain } = SidebarController;
  const gleanEvents = [
    Glean.sidebar.chatbotIconClick,
    Glean.sidebar.syncedTabsIconClick,
    Glean.sidebar.historyIconClick,
    Glean.sidebar.bookmarksIconClick,
  ];
  for (const [i, button] of Array.from(sidebarMain.toolButtons).entries()) {
    await SidebarController.initializeUIState({ launcherExpanded: expanded });

    info(`Click the icon for: ${button.getAttribute("view")}`);
    EventUtils.synthesizeMouseAtCenter(button, {});

    if (gleanEvents[i]) {
      const events = gleanEvents[i].testGetValue();
      Assert.equal(events?.length, 1, "One event was reported.");
      Assert.deepEqual(
        events?.[0].extra,
        { sidebar_open: `${expanded}` },
        `Event indicates the sidebar was ${expanded ? "expanded" : "collapsed"}.`
      );
    }
  }

  info("Load an extension.");
  const extension = ExtensionTestUtils.loadExtension({ ...extData });
  await extension.startup();
  await extension.awaitMessage("sidebar");

  await SidebarController.initializeUIState({ launcherExpanded: expanded });

  info("Click the icon for the extension.");
  const extensionButton = await TestUtils.waitForCondition(
    () => sidebarMain.extensionButtons[0],
    "Extension button is present"
  );
  EventUtils.synthesizeMouseAtCenter(extensionButton, {});

  const events = Glean.sidebar.addonIconClick.testGetValue();
  Assert.equal(events?.length, 1, "One event was reported.");
  Assert.equal(
    events?.[0].extra.sidebar_open,
    `${expanded}`,
    `Event indicates the sidebar was ${expanded ? "expanded" : "collapsed"}.`
  );
  Assert.ok(events?.[0].extra.addon_id, "Event has the extension's ID.");

  info("Unload the extension.");
  await extension.unload();

  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
}

add_task(async function test_icon_click_collapsed_sidebar() {
  await testIconClick(false);
});

add_task(async function test_icon_click_expanded_sidebar() {
  await testIconClick(true);
});
