/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the search engine list in about:preferences#search.

"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);
const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/search/SearchUtils.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

SearchTestUtils.init(this);

const CONFIG = [
  { identifier: "engine1" },
  { identifier: "engine2" },
  {
    identifier: "de_only_engine",
    base: {
      urls: {
        search: {
          base: "https://moz.test/",
          searchTermParamName: "search",
        },
      },
    },
    variants: [
      {
        environment: { locales: ["de"] },
      },
    ],
  },
];

let userEngine;
let extensionEngine;
let installedEngines;
let userInstalledAppEngine;

function getCellText(tree, i, cellName) {
  return tree.view.getCellText(i, tree.columns.getNamedColumn(cellName));
}

async function engine_list_test(fn) {
  let task = async () => {
    let prefs = await openPreferencesViaOpenPreferencesAPI("search", {
      leaveOpen: true,
    });
    Assert.equal(
      prefs.selectedPane,
      "paneSearch",
      "Search pane is selected by default"
    );
    let doc = gBrowser.contentDocument;
    let tree = doc.querySelector("#engineList");
    Assert.ok(
      !tree.hidden,
      "The search engine list should be visible when Search is requested"
    );
    // Scroll the treeview into view since mouse operations
    // off screen can act confusingly.
    tree.scrollIntoView();
    await fn(tree, doc);
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  };

  // Make sure the name of the passed function is used in logs.
  Object.defineProperty(task, "name", { value: fn.name });
  add_task(task);
}
async function selectEngine(tree, index) {
  let rect = tree.getCoordsForCellItem(
    index,
    tree.columns.getNamedColumn("engineName"),
    "text"
  );
  let x = rect.x + rect.width / 2;
  let y = rect.y + rect.height / 2;
  let promise = BrowserTestUtils.waitForEvent(tree, "click");
  EventUtils.synthesizeMouse(
    tree.body,
    x,
    y,
    { clickCount: 1 },
    tree.ownerGlobal
  );
  return promise;
}

add_setup(async function () {
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG);
  installedEngines = await Services.search.getAppProvidedEngines();

  await SearchTestUtils.installSearchExtension({
    keyword: ["testing", "customkeyword"],
    search_url: "https://example.com/engine1",
    search_url_get_params: "search={searchTerms}",
    name: "Extension Engine",
  });
  extensionEngine = Services.search.getEngineByName("Extension Engine");
  installedEngines.push(extensionEngine);

  userEngine = await Services.search.addUserEngine({
    name: "User Engine",
    url: "https://example.com/user?q={searchTerms}&b=ff",
    alias: "u",
  });
  installedEngines.push(userEngine);

  userInstalledAppEngine =
    await Services.search.findContextualSearchEngineByHost("moz.test");

  await Services.search.addSearchEngine(userInstalledAppEngine);
  // The added engines are removed in the last test.
});

engine_list_test(async function test_engine_list(tree) {
  let userEngineIndex = installedEngines.length - 1;
  for (let i = 0; i < installedEngines.length; i++) {
    let engine = installedEngines[i];
    Assert.equal(
      getCellText(tree, i, "engineName"),
      engine.name,
      "Search engine " + engine.name + " displayed correctly."
    );
    Assert.equal(
      tree.view.isEditable(i, tree.columns.getNamedColumn("engineName")),
      i == userEngineIndex,
      "Only user engine name is editable."
    );
  }
});

engine_list_test(async function test_change_keyword(tree) {
  let extensionEngineIndex = installedEngines.length - 2;
  Assert.equal(
    getCellText(tree, extensionEngineIndex, "engineKeyword"),
    "testing, customkeyword",
    "Internal keywords are displayed."
  );
  let rect = tree.getCoordsForCellItem(
    extensionEngineIndex,
    tree.columns.getNamedColumn("engineKeyword"),
    "text"
  );

  // Test editing keyword of extension engine because it
  // has user-defined and extension-provided keywords.
  let x = rect.x + rect.width / 2;
  let y = rect.y + rect.height / 2;
  let win = tree.ownerGlobal;

  let promise = BrowserTestUtils.waitForEvent(tree, "dblclick");
  EventUtils.synthesizeMouse(tree.body, x, y, { clickCount: 1 }, win);
  EventUtils.synthesizeMouse(tree.body, x, y, { clickCount: 2 }, win);
  await promise;

  promise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  EventUtils.sendString("newkeyword");
  EventUtils.sendKey("RETURN");
  await promise;

  Assert.equal(
    getCellText(tree, extensionEngineIndex, "engineKeyword"),
    "newkeyword, testing, customkeyword",
    "User-defined keyword was added."
  );

  // Test duplicated keywords (different capitalization).
  tree.view.setCellText(
    0,
    tree.columns.getNamedColumn("engineKeyword"),
    "keyword"
  );
  await TestUtils.waitForTick();

  let keywordBefore = getCellText(tree, 1, "engineKeyword");
  let alertSpy = sinon.spy(win, "alert");
  tree.view.setCellText(
    1,
    tree.columns.getNamedColumn("engineKeyword"),
    "Keyword"
  );
  await TestUtils.waitForTick();

  Assert.ok(alertSpy.calledOnce, "Warning was shown.");
  Assert.equal(
    getCellText(tree, 1, "engineKeyword"),
    keywordBefore,
    "Did not modify keywords."
  );
  alertSpy.restore();
});

engine_list_test(async function test_rename_engines(tree) {
  // Test editing name of user search engine because
  // only the names of user engines can be edited.
  let userEngineIndex = installedEngines.length - 1;
  let rect = tree.getCoordsForCellItem(
    userEngineIndex,
    tree.columns.getNamedColumn("engineName"),
    "text"
  );
  let x = rect.x + rect.width / 2;
  let y = rect.y + rect.height / 2;
  let win = tree.ownerGlobal;

  let promise = BrowserTestUtils.waitForEvent(tree, "dblclick");
  EventUtils.synthesizeMouse(tree.body, x, y, { clickCount: 1 }, win);
  EventUtils.synthesizeMouse(tree.body, x, y, { clickCount: 2 }, win);
  await promise;

  EventUtils.sendString("User Engine 2");
  promise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  EventUtils.sendKey("RETURN");
  await promise;

  Assert.equal(userEngine.name, "User Engine 2", "Engine was renamed.");

  // Avoid duplicated engine names.
  let alertSpy = sinon.spy(win, "alert");
  tree.view.setCellText(
    userEngineIndex,
    tree.columns.getNamedColumn("engineName"),
    "Extension Engine"
  );
  await TestUtils.waitForTick();

  Assert.ok(alertSpy.calledOnce, "Warning was shown.");
  Assert.equal(
    getCellText(tree, userEngineIndex, "engineName"),
    "User Engine 2",
    "Name was not modified."
  );

  alertSpy.restore();
});

engine_list_test(async function test_remove_button_disabled_state(tree, doc) {
  let appProvidedEngines = await Services.search.getAppProvidedEngines();
  for (let i = 0; i < appProvidedEngines.length; i++) {
    let engine = appProvidedEngines[i];
    let isDefaultSearchEngine =
      engine.id == Services.search.defaultEngine.id ||
      engine.id == Services.search.defaultPrivateEngine.id;

    await selectEngine(tree, i);
    Assert.equal(
      doc.querySelector("#removeEngineButton").disabled,
      isDefaultSearchEngine,
      "Remove button is in correct disabled state."
    );
  }
});

engine_list_test(async function test_remove_button(tree, doc) {
  let win = tree.ownerGlobal;
  let alertSpy = sinon.stub(win, "alert");

  info("Removing user engine.");
  let userEngineIndex = installedEngines.findIndex(e => e.id == userEngine.id);
  await selectEngine(tree, userEngineIndex);

  let promptPromise = PromptTestUtils.handleNextPrompt(
    gBrowser.selectedBrowser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT },
    { buttonNumClick: 0 } // 0 = cancel, 1 = remove
  );
  let removedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.REMOVED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  doc.querySelector("#removeEngineButton").click();
  await promptPromise;
  let removedEngine = await removedPromise;
  Assert.equal(
    removedEngine.id,
    userEngine.id,
    "User engine was removed after a prompt."
  );

  // Re-fetch the engines since removing the user engine changed it.
  installedEngines = await Services.search.getEngines();

  info("Removing extension engine.");
  let extensionEngineIndex = installedEngines.findIndex(
    e => e.id == extensionEngine.id
  );
  await selectEngine(tree, extensionEngineIndex);

  doc.querySelector("#removeEngineButton").click();
  await TestUtils.waitForCondition(() => alertSpy.calledOnce);
  Assert.ok(true, "Alert is shown when attempting to remove extension engine.");

  info("Removing user installed app engine.");
  let index = installedEngines.findIndex(
    e => e.id == userInstalledAppEngine.id
  );

  await selectEngine(tree, index);

  promptPromise = PromptTestUtils.handleNextPrompt(
    gBrowser.selectedBrowser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT },
    { buttonNumClick: 0 } // 0 = cancel, 1 = remove
  );
  removedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.REMOVED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  doc.querySelector("#removeEngineButton").click();
  await promptPromise;
  removedEngine = await removedPromise;
  Assert.equal(
    removedEngine.id,
    userInstalledAppEngine.id,
    "User installed app engine was removed after a prompt."
  );

  info("Removing (last) app provided engine.");
  let appProvidedEngines = await Services.search.getAppProvidedEngines();
  let lastAppEngine = appProvidedEngines[appProvidedEngines.length - 1];
  let lastAppEngineIndex = installedEngines.findIndex(
    e => e.id == lastAppEngine.id
  );
  await selectEngine(tree, lastAppEngineIndex);

  doc.querySelector("#removeEngineButton").click();
  removedEngine = await SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.REMOVED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  Assert.equal(
    removedEngine.id,
    lastAppEngine.id,
    "Last app provided engine was removed without a prompt."
  );

  // Cleanup.
  alertSpy.restore();
  let updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  doc.getElementById("restoreDefaultSearchEngines").click();
  await updatedPromise;
  // The user engine is purposefully not re-added.
  // The extension engine is removed automatically on cleanup.
});
