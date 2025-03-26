/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchUtils.sys.mjs"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2.engineAliasRefresh", true]],
  });
});

add_task(async function test_addEngine() {
  await openPreferencesViaOpenPreferencesAPI("search", {
    leaveOpen: true,
  });

  // Add new engine via add engine dialog.
  let doc = gBrowser.contentDocument;
  let addButton = doc.querySelector("#addEngineButton");
  let dialogWin = await openDialogWith(doc, () => addButton.click());
  Assert.equal(
    dialogWin.document.getElementById("titleContainer").style.display,
    "none",
    "Adjustable title is hidden in add engine dialog."
  );

  setName("Bugzilla", dialogWin);
  setUrl(
    "https://bugzilla.mozilla.org/buglist.cgi?quicksearch=%s&list_id=17442621",
    dialogWin
  );
  await setAlias("bz", dialogWin);

  let promiseAdded = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ADDED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
  await promiseAdded;
  Assert.ok(true, "Got added notification.");

  // Check new engine.
  let engine = Services.search.getEngineByName("Bugzilla");
  Assert.equal(engine.name, "Bugzilla", "Name is correct.");
  Assert.equal(
    engine.getSubmission("föö").uri.spec,
    "https://bugzilla.mozilla.org/buglist.cgi?quicksearch=f%C3%B6%C3%B6&list_id=17442621",
    "URL is correct and encodes search terms using utf-8."
  );
  Assert.equal(engine.alias, "bz", "Alias is correct.");

  // Clean up.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await Services.search.removeEngine(engine);
});

add_task(async function test_validation() {
  await openPreferencesViaOpenPreferencesAPI("search", {
    leaveOpen: true,
  });
  let existingEngine = await Services.search.addUserEngine({
    name: "user",
    url: "https://example.com/user?q={searchTerms}&b=ff",
    alias: "u",
  });

  let doc = gBrowser.contentDocument;
  let addButton = doc.querySelector("#addEngineButton");
  let dialogWin = await openDialogWith(doc, () => addButton.click());

  let button = dialogWin.document
    .querySelector("dialog")
    .shadowRoot.querySelector('button[dlgtype="accept"]');
  let name = dialogWin.document.getElementById("engineName");
  let url = dialogWin.document.getElementById("engineUrl");
  let alias = dialogWin.document.getElementById("engineAlias");

  Assert.ok(
    name.value == "" && url.value == "" && alias.value == "",
    "Everything is empty initially."
  );
  Assert.ok(button.disabled, "Button is disabled initially.");

  setName("Example", dialogWin);
  setUrl("https://example.com/search?q=%s", dialogWin);
  await setAlias("abc", dialogWin);
  Assert.ok(!button.disabled, "Button is enabled when everything is there.");

  // Check URL
  setUrl("", dialogWin);
  Assert.ok(button.disabled, "URL is required.");
  setUrl("javascript://%s", dialogWin);
  Assert.ok(button.disabled, "Javascript URLs are not allowed.");
  setUrl("https://example.com/search?q=%s", dialogWin);
  Assert.ok(!button.disabled, "Good URL enables the button.");

  // Check name
  setName("", dialogWin);
  Assert.ok(button.disabled, "Name is required.");
  setName(existingEngine.name, dialogWin);
  Assert.ok(button.disabled, "Existing name is not allowed.");
  setName("Example", dialogWin);
  Assert.ok(!button.disabled, "Good name enables the button.");

  // Check alias
  await setAlias("", dialogWin);
  Assert.ok(!button.disabled, "Alias is not required.");
  await setAlias(existingEngine.alias, dialogWin);
  Assert.ok(button.disabled, "Existing alias is not allowed.");
  await setAlias("abc", dialogWin);
  Assert.ok(!button.disabled, "Good alias enables the button.");

  // Clean up.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await Services.search.removeEngine(existingEngine);
});

add_task(async function test_editEngine() {
  await openPreferencesViaOpenPreferencesAPI("search", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let tree = doc.querySelector("#engineList");
  let view = tree.view.wrappedJSObject;
  let engine = await Services.search.addUserEngine({
    name: "user",
    url: "https://example.com/user?q={searchTerms}&b=ff",
    alias: "u",
  });

  // Check buttons of all search engines + local shortcuts.
  let removeButton = doc.querySelector("#removeEngineButton");
  let editButton = doc.querySelector("#editEngineButton");

  let userEngineIndex = null;
  for (let i = 0; i < tree.view.rowCount; i++) {
    view.selection.select(i);
    let selectedEngine = view.selectedEngine;
    if (selectedEngine?.isUserEngine) {
      Assert.equal(selectedEngine.name, "user", "Is the new engine.");
      Assert.ok(!removeButton.disabled, "Remove button is enabled.");
      Assert.ok(!editButton.disabled, "Edit button is enabled.");
      userEngineIndex = i;
    } else {
      Assert.ok(editButton.disabled, "Edit button is disabled.");
    }
  }

  // Check if table contains new engine without reloading.
  Assert.ok(!!userEngineIndex, "User engine is in the table.");
  view.selection.select(userEngineIndex);

  // Open the dialog.
  let dialogWin = await openDialogWith(doc, () => editButton.click());
  Assert.equal(
    dialogWin.document.getElementById("titleContainer").style.display,
    "none",
    "Adjustable title is hidden in edit engine dialog."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineName").value,
    "user",
    "Name in dialog is correct."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineUrl").value,
    "https://example.com/user?q=%s&b=ff",
    "URL in dialog is correct."
  );
  Assert.ok(
    dialogWin.document.getElementById("enginePostDataRow").hidden,
    "Post data input is hidden."
  );
  Assert.equal(
    dialogWin.document.getElementById("suggestUrl").value,
    "",
    "Suggest URL in dialog is empty."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineAlias").value,
    "u",
    "Alias in dialog is correct."
  );

  // Set new values.
  setName("Searchfox", dialogWin);
  setUrl("https://searchfox.org/mozilla-central/search?q=%s", dialogWin);
  await setAlias("sf", dialogWin);

  // Save changes to engine.
  let promiseChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED,
    3
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
  await promiseChanged;
  Assert.ok(true, "Got 3 change notifications.");

  // Open dialog again and check values.
  dialogWin = await openDialogWith(doc, () => editButton.click());
  Assert.equal(
    dialogWin.document.getElementById("engineName").value,
    "Searchfox",
    "Name in dialog reflects change."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineUrl").value,
    "https://searchfox.org/mozilla-central/search?q=%s",
    "URL in dialog reflects change."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineAlias").value,
    "sf",
    "Alias in dialog reflects change."
  );

  // Clean up.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await Services.search.removeEngine(engine);
});

add_task(async function test_editPostEngine() {
  await openPreferencesViaOpenPreferencesAPI("search", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;
  let tree = doc.querySelector("#engineList");
  let view = tree.view.wrappedJSObject;

  let formData = new FormData();
  formData.append("q", "{searchTerms}");
  let engine = await Services.search.addUserEngine({
    name: "user post",
    url: "https://example.com/user",
    formData,
    method: "POST",
    alias: "u",
  });

  let editButton = doc.querySelector("#editEngineButton");

  for (let i = 0; i < tree.view.rowCount; i++) {
    view.selection.select(i);
    let selectedEngine = view.selectedEngine;
    if (selectedEngine?.isUserEngine) {
      view.selection.select(i);
      break;
    }
  }

  // Open the dialog.
  let dialogWin = await openDialogWith(doc, () => editButton.click());
  Assert.equal(
    dialogWin.document.getElementById("engineName").value,
    "user post",
    "Name in dialog is correct."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineUrl").value,
    "https://example.com/user",
    "URL in dialog is correct."
  );
  Assert.ok(
    !dialogWin.document.getElementById("enginePostDataRow").hidden,
    "Post data input is visible."
  );
  Assert.equal(
    dialogWin.document.getElementById("enginePostData").value,
    "q=%s",
    "Post data in dialog is correct."
  );
  Assert.equal(
    dialogWin.document.getElementById("suggestUrl").value,
    "",
    "Suggest URL in dialog is empty."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineAlias").value,
    "u",
    "Alias in dialog is correct."
  );

  // Set new values.
  setName("Searchfox", dialogWin);
  setUrl("https://searchfox.org/mozilla-central/search", dialogWin);
  setPostData("q=%s&path=&case=false&regexp=false", dialogWin);
  setSuggestUrl("https://searchfox.org/suggest/%s", dialogWin);
  await setAlias("sf", dialogWin);

  // Save changes to engine.
  let promiseChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED,
    3
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
  await promiseChanged;
  Assert.ok(true, "Got 3 change notifications.");

  // Open dialog again and check values.
  dialogWin = await openDialogWith(doc, () => editButton.click());
  Assert.equal(
    dialogWin.document.getElementById("engineName").value,
    "Searchfox",
    "Name in dialog reflects change."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineUrl").value,
    "https://searchfox.org/mozilla-central/search",
    "URL in dialog reflects changes."
  );
  Assert.equal(
    dialogWin.document.getElementById("enginePostData").value,
    "q=%s&path=&case=false&regexp=false",
    "Post data in dialog reflects changes."
  );
  Assert.equal(
    dialogWin.document.getElementById("suggestUrl").value,
    "https://searchfox.org/suggest/%s",
    "Suggest URL in dialog reflects changes."
  );
  Assert.equal(
    dialogWin.document.getElementById("engineAlias").value,
    "sf",
    "Alias in dialog reflects changes."
  );

  // Check values of search engine object.
  Assert.equal(
    engine.name,
    "Searchfox",
    "Name of search engine object was updated."
  );
  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://searchfox.org/mozilla-central/search",
    "Submission URL reflects changes."
  );
  Assert.equal(
    decodePostData(submission.postData),
    "q=foo&path=&case=false&regexp=false",
    "Submission post data reflects changes"
  );
  submission = engine.getSubmission("foo", SearchUtils.URL_TYPE.SUGGEST_JSON);
  Assert.equal(
    submission.uri.spec,
    "https://searchfox.org/suggest/foo",
    "Submission URL reflects changes."
  );
  Assert.equal(submission.postData, null, "Submission URL is still GET.");
  Assert.equal(
    engine.alias,
    "sf",
    "Alias of search engine object was updated."
  );

  // Clean up.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await Services.search.removeEngine(engine);
});

async function openDialogWith(doc, fn) {
  let dialogLoaded = TestUtils.topicObserved("subdialog-loaded");
  await fn();
  let [dialogWin] = await dialogLoaded;
  await doc.ownerGlobal.gSubDialog.dialogs[0]._dialogReady;
  Assert.ok(true, "Engine dialog opened.");
  return dialogWin;
}

function setName(value, win) {
  fillTextField("engineName", value, win);
}

function setUrl(value, win) {
  fillTextField("engineUrl", value, win);
}

function setPostData(value, win) {
  fillTextField("enginePostData", value, win);
}

function setSuggestUrl(value, win) {
  fillTextField("suggestUrl", value, win);
}

async function setAlias(value, win) {
  fillTextField("engineAlias", value, win);
  await TestUtils.waitForTick();
}

function fillTextField(id, text, win) {
  let elt = win.document.getElementById(id);
  elt.focus();
  elt.select();
  EventUtils.synthesizeKey("a", { metaKey: true }, win);
  EventUtils.synthesizeKey("KEY_Backspace", {}, win);

  for (let c of text.split("")) {
    EventUtils.synthesizeKey(c, {}, win);
  }
}

function decodePostData(postData) {
  let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  binaryStream.setInputStream(postData.data);

  return binaryStream
    .readBytes(binaryStream.available())
    .replace("searchTerms", "%s");
}
