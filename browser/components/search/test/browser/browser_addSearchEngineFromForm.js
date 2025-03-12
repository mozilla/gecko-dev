/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTS = [
  {
    action: "/search",
    method: "GET",
    charset: "UTF-8",
    fields: [
      { name: "q", value: "Some initial value", main: true },
      { name: "utf8✓", value: "✓", hidden: true },
    ],
    submission: "kitten",
    expected: "https://example.org/search?q=kitten&utf8%E2%9C%93=%E2%9C%93",
  },
  {
    action: "/search",
    method: "POST",
    charset: "UTF-8",
    fields: [
      { name: "q", value: "Some initial value", main: true },
      { name: "utf8✓", value: "✓", hidden: true },
    ],
    submission: "kitten",
    expected: "https://example.org/search",
    expectedPost: "q=kitten&utf8%E2%9C%93=%E2%9C%93",
  },
  {
    action: "/search",
    method: "GET",
    charset: "windows-1252",
    fields: [
      { name: "q", main: true },
      { name: "cb", checked: true, value: "true", type: "checkbox" },
      { name: "cb2", checked: false, value: "true", type: "checkbox" },
    ],
    submission: "caff\u00E8+",
    expected: "https://example.org/search?q=caff%E8%2B&cb=true",
  },
  {
    action: "/search",
    method: "POST",
    charset: "windows-1252",
    fields: [
      { name: "q", main: true },
      { name: "foo", value: "bar" },
    ],
    submission: "caff\u00E8+",
    expected: "https://example.org/search",
    expectedPost: "q=caff%E8%2B&foo=bar",
  },
];

const URL_UTF_8 =
  "https://example.org/browser/browser/components/search/test/browser/test.html";
const URL_WINDOWS1252 =
  "https://example.org/browser/browser/components/search/test/browser/test_windows1252.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2.engineAliasRefresh", true]],
  });
});

async function addEngine(browser, selector, name, alias) {
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let addEngineItem = document.getElementById("context-add-engine");

  let contextMenuPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  info("Opening context menu.");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu", button: 2 },
    browser
  );
  await contextMenuPromise;
  let dialogLoaded = TestUtils.topicObserved("subdialog-loaded");
  info("Clicking add engine.");
  contextMenu.activateItem(addEngineItem);
  let [dialogWin] = await dialogLoaded;
  await window.gDialogBox.dialog._dialogReady;
  info("Dialog opened.");
  Assert.equal(
    dialogWin.document.getElementById("titleContainer").style.display,
    "",
    "Adjustable title is displayed."
  );

  fillTextField("engineName", name, dialogWin);
  fillTextField("engineAlias", alias, dialogWin);

  info("Saving engine.");
  let promiseAdded = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ADDED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
  await window.gDialogBox.dialog._closingPromise;
  await promiseAdded;
  return Services.search.getEngineByName(name);
}

async function createForm({ action, method, fields }) {
  let doc = content.document;
  doc.querySelector("form")?.remove();
  let form = doc.createElement("form");
  form.method = method;
  form.action = action;

  for (let fieldInfo of fields) {
    let input = doc.createElement("input");
    input.value = fieldInfo.value ?? "";
    input.type = fieldInfo.type ?? "text";

    if (fieldInfo.checked) {
      input.checked = "true";
    }

    input.name = fieldInfo.name;
    if (fieldInfo.main) {
      input.id = "mainInput";
    }
    if (fieldInfo.hidden) {
      input.hidden = true;
    }
    form.appendChild(input);
  }

  doc.body.appendChild(form);
}

async function navigateToCharset(charset) {
  if (charset.toUpperCase() == "UTF-8") {
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser,
      false,
      URL_UTF_8
    );
    BrowserTestUtils.startLoadingURIString(gBrowser, URL_UTF_8);
    await browserLoadedPromise;
  } else if (charset.toLowerCase() == "windows-1252") {
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser,
      false,
      URL_WINDOWS1252
    );
    BrowserTestUtils.startLoadingURIString(gBrowser, URL_WINDOWS1252);
    await browserLoadedPromise;
  } else {
    throw new Error();
  }
}

add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browser = tab.linkedBrowser;

  for (let args of TESTS) {
    await navigateToCharset(args.charset);
    await SpecialPowers.spawn(browser, [args], createForm);
    let engine = await addEngine(browser, "#mainInput", "My Engine", "alias");
    Assert.ok(!!engine, "Engine was installed.");
    Assert.equal(
      engine.id,
      (await Services.search.getEngineByAlias("alias"))?.id,
      "Engine has correct alias."
    );

    Assert.equal(engine.wrappedJSObject.queryCharset, args.charset);
    let submission = engine.getSubmission(args.submission);
    Assert.equal(
      submission.uri.spec,
      args.expected,
      "Submission URI is correct"
    );
    Assert.equal(
      decodePostData(submission.postData, args.charset),
      args.expectedPost,
      "Submission post data is correct."
    );

    await Services.search.removeEngine(engine);
  }

  // Let the dialog fully close. Otherwise, the tab cannot be closed properly.
  await TestUtils.waitForCondition(
    () => !document.documentElement.hasAttribute("window-modal-open")
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

function decodePostData(postData, charset) {
  if (!postData) {
    return undefined;
  }
  const binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  binaryStream.setInputStream(postData);
  const available = binaryStream.available();
  const buffer = new ArrayBuffer(available);
  binaryStream.readArrayBuffer(available, buffer);
  return new TextDecoder(charset).decode(buffer);
}
