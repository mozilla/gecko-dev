const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchUtils.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

SearchTestUtils.init(this);

let userEngine;
let extensionEngine;
let installedEngines;

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

add_setup(async function () {
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

  registerCleanupFunction(async () => {
    await Services.search.removeEngine(userEngine);
    // Extension engine is cleaned up by SearchTestUtils.
  });
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
  let win = tree.ownerGlobal;
  for (let i = 0; i < appProvidedEngines.length; i++) {
    let engine = appProvidedEngines[i];
    let isDefaultSearchEngine =
      engine.id == Services.search.defaultEngine.id ||
      engine.id == Services.search.defaultPrivateEngine.id;

    let rect = tree.getCoordsForCellItem(
      i,
      tree.columns.getNamedColumn("engineName"),
      "text"
    );
    let x = rect.x + rect.width / 2;
    let y = rect.y + rect.height / 2;

    let promise = BrowserTestUtils.waitForEvent(tree, "click");
    EventUtils.synthesizeMouse(tree.body, x, y, { clickCount: 1 }, win);
    await promise;

    Assert.equal(
      doc.querySelector("#removeEngineButton").disabled,
      isDefaultSearchEngine,
      "Remove button is in correct disabled state."
    );
  }
});
