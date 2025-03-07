/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

async function openCreatePanel(tabgroupPanel, tab) {
  let panelShown = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "shown");
  gBrowser.addTabGroup([tab], {
    color: "cyan",
    isUserCreated: true,
  });
  await panelShown;
}

async function setup(enableSmartTab = true) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.groups.enabled", true],
      ["browser.tabs.groups.smart.enabled", enableSmartTab],
    ],
  });
  sinon
    .stub(SmartTabGroupingManager.prototype, "generateGroupLabels")
    .returns("");
  sinon
    .stub(SmartTabGroupingManager.prototype, "smartTabGroupingForGroup")
    .resolves([]);
  sinon.stub(SmartTabGroupingManager.prototype, "getEngineConfigs").resolves({
    "text2text-generation": { modelRevision: "v0.3.4" },
    "feature-extraction": { modelRevision: "v0.1.0" },
  });

  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const cleanup = () => {
    sinon.restore();
    BrowserTestUtils.removeTab(tab);
    Services.fog.testResetFOG();
  };
  return { tab, cleanup };
}

add_task(async function test_saving_ml_suggested_label_telemetry() {
  let { tab, cleanup } = await setup();
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  let nameField = tabgroupPanel.querySelector("#tab-group-name");

  await openCreatePanel(tabgroupPanel, tab);
  nameField.focus();
  nameField.value = "Random ML Suggested Label"; // user label matching suggested label
  tabgroupEditor.mlLabel = "Random ML Suggested Label"; // suggested label
  tabgroupPanel.querySelector("#tab-group-editor-button-create").click();
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  await panelHidden;

  const events = Glean.browserMlInteraction.smartTabTopic.testGetValue();
  Assert.equal(events.length, 1, "Should create a label event");
  Assert.equal(events[0].extra.action, "save", "Save button was clicked");
  Assert.equal(
    events[0].extra.num_tabs_in_group,
    "1",
    "Number of tabs in group"
  );
  Assert.equal(events[0].extra.ml_label_length, "25", "Suggested ML Label");
  Assert.equal(
    events[0].extra.user_label_length,
    "25",
    "User input was the same as ml suggested label"
  );
  Assert.equal(
    events[0].extra.model_revision,
    "v0.3.4",
    "Model revision should be present"
  );
  cleanup();
});

add_task(async function test_cancel_ml_suggested_label_telemetry() {
  let { tab, cleanup } = await setup();
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  let nameField = tabgroupPanel.querySelector("#tab-group-name");

  await openCreatePanel(tabgroupPanel, tab);
  nameField.focus();
  nameField.value = "Random ML Suggested Label"; // user label matching suggested label
  tabgroupEditor.mlLabel = "Random ML Suggested Label"; // suggested label
  tabgroupPanel.querySelector("#tab-group-editor-button-cancel").click(); // cancel
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  await panelHidden;

  const events = Glean.browserMlInteraction.smartTabTopic.testGetValue();
  Assert.equal(events.length, 1, "Should create a label event");
  Assert.equal(
    events[0].extra.action,
    "cancel",
    "Should save label even if cancel button was clicked"
  );
  Assert.equal(
    events[0].extra.num_tabs_in_group,
    "1",
    "Number of tabs in group"
  );
  Assert.equal(events[0].extra.ml_label_length, "25", "Suggested ML Label");
  Assert.equal(
    events[0].extra.user_label_length,
    "25",
    "User input was the same as ml suggested label"
  );
  Assert.equal(
    events[0].extra.model_revision,
    "v0.3.4",
    "Model revision should be present"
  );
  cleanup();
});

add_task(async function test_saving_ml_suggested_tabs() {
  let { tab, cleanup } = await setup();
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  let nameField = tabgroupPanel.querySelector("#tab-group-name");

  await openCreatePanel(tabgroupPanel, tab);
  // add the name only
  nameField.focus();
  nameField.value = "Random ML Suggested Label"; // user label matching suggested label
  // click on the suggest button
  tabgroupPanel.querySelector("#tab-group-suggestion-button").click();
  tabgroupEditor.hasSuggestedMlTabs = true;
  tabgroupPanel.querySelector("#tab-group-create-suggestions-button").click();
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  await panelHidden;
  const events = Glean.browserMlInteraction.smartTabSuggest.testGetValue();
  Assert.equal(events.length, 1, "Should create a sugggest event");
  Assert.equal(events[0].extra.action, "save", "Save button was clicked");
  Assert.equal(
    events[0].extra.num_tabs_in_window,
    "2",
    "Number of tabs in window"
  );
  Assert.equal(
    events[0].extra.num_tabs_in_group,
    "1",
    "Number of tabs in the group"
  );
  Assert.equal(
    events[0].extra.num_tabs_suggested,
    "0",
    "No tabs were suggested"
  );
  Assert.equal(events[0].extra.num_tabs_approved, "0", "No tabs were approved");
  Assert.equal(events[0].extra.num_tabs_removed, "0", "No tabs were removed");
  Assert.equal(
    events[0].extra.model_revision,
    "v0.1.0",
    "Model revision should be present"
  );
  cleanup();
});

add_task(async function test_saving_ml_suggested_tabs_with_ml_label() {
  let { tab, cleanup } = await setup();
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  let nameField = tabgroupPanel.querySelector("#tab-group-name");

  await openCreatePanel(tabgroupPanel, tab);
  // add the label
  nameField.focus();
  nameField.value = "Random ML Suggested Label"; // user label matching suggested label
  tabgroupEditor.mlLabel = "Random ML Suggested Label"; // suggested label
  // click on the suggest button
  tabgroupPanel.querySelector("#tab-group-suggestion-button").click();
  tabgroupEditor.hasSuggestedMlTabs = true;
  tabgroupPanel.querySelector("#tab-group-create-suggestions-button").click();
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  await panelHidden;

  const labelEvent = Glean.browserMlInteraction.smartTabTopic.testGetValue();
  const suggestEvent =
    Glean.browserMlInteraction.smartTabSuggest.testGetValue();
  Assert.equal(labelEvent.length, 1, "Should create label event");
  Assert.equal(suggestEvent.length, 1, "Should create suggest event");
  Assert.equal(labelEvent[0].extra.action, "save", "Save button was clicked");
  Assert.equal(suggestEvent[0].extra.action, "save", "Save button was clicked");
  cleanup();
});

add_task(async function test_canceling_ml_suggested_tabs_with_ml_label() {
  let { tab, cleanup } = await setup();
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  let nameField = tabgroupPanel.querySelector("#tab-group-name");

  await openCreatePanel(tabgroupPanel, tab);
  // add the label
  nameField.focus();
  nameField.value = "Random ML Suggested Label"; // user label matching suggested label
  tabgroupEditor.mlLabel = "Random ML Suggested Label"; // suggested label
  // click on the suggest button
  tabgroupPanel.querySelector("#tab-group-suggestion-button").click();
  tabgroupEditor.hasSuggestedMlTabs = true;
  tabgroupPanel.querySelector("#tab-group-cancel-suggestions-button").click(); // cancel
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  await panelHidden;

  const labelEvent = Glean.browserMlInteraction.smartTabTopic.testGetValue();
  const suggestEvent =
    Glean.browserMlInteraction.smartTabSuggest.testGetValue();
  Assert.equal(labelEvent.length, 1, "Should create label event");
  Assert.equal(suggestEvent.length, 1, "Should create suggest event");
  Assert.equal(
    labelEvent[0].extra.action,
    "cancel",
    "Cancel button was clicked"
  );
  Assert.equal(
    suggestEvent[0].extra.action,
    "cancel",
    "cancel button was clicked"
  );
  cleanup();
});

add_task(async function test_pref_off_should_not_create_events() {
  let { tab, cleanup } = await setup(false);
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  let nameField = tabgroupPanel.querySelector("#tab-group-name");

  await openCreatePanel(tabgroupPanel, tab);
  nameField.focus();
  nameField.value = "Random ML Suggested Label"; // user label matching suggested label
  tabgroupEditor.mlLabel = "Random ML Suggested Label"; // suggested label
  tabgroupPanel.querySelector("#tab-group-editor-button-create").click();
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  tabgroupPanel.querySelector("#tab-group-editor-button-create").click(); // save
  await panelHidden;

  Assert.equal(
    Glean.browserMlInteraction.smartTabTopic.testGetValue() ?? "none",
    "none",
    "No event if the feature is off"
  );
  cleanup();
});
