/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests turning non-url-looking values typed in the input field into proper URLs.
 */

requestLongerTimeout(2);

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

const CANONIZE_MODIFIERS =
  AppConstants.platform == "macosx" ? { metaKey: true } : { ctrlKey: true };
const MODIFIER_KEY =
  AppConstants.platform == "macosx" ? "VK_META" : "VK_CONTROL";

add_task(async function checkCanonizeWorks() {
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });

  // We do not want schemeless HTTPS-First interfering with this test,
  // that interaction is already tested in dom/security/test/https-first/browser_schemeless.js
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first_schemeless", false]],
  });

  let defaultEngine = await Services.search.getDefault();
  let testcases = [
    ["example", "https://www.example.com/", CANONIZE_MODIFIERS],
    // Check that a direct load is not overwritten by a previous canonization.
    ["http://example.com/test/", "http://example.com/test/", {}],
    ["ex-ample", "https://www.ex-ample.com/", CANONIZE_MODIFIERS],
    ["  example ", "https://www.example.com/", CANONIZE_MODIFIERS],
    [" example/foo ", "https://www.example.com/foo", CANONIZE_MODIFIERS],
    [
      " example/foo bar ",
      "https://www.example.com/foo%20bar",
      CANONIZE_MODIFIERS,
    ],
    ["example.net", "http://example.net/", CANONIZE_MODIFIERS],
    ["http://example", "http://example/", CANONIZE_MODIFIERS],
    ["example:8080", "http://example:8080/", CANONIZE_MODIFIERS],
    ["ex-ample.foo", "http://ex-ample.foo/", CANONIZE_MODIFIERS],
    ["example.foo/bar ", "http://example.foo/bar", CANONIZE_MODIFIERS],
    ["1.1.1.1", "http://1.1.1.1/", CANONIZE_MODIFIERS],
    ["ftp.example.bar", "http://ftp.example.bar/", CANONIZE_MODIFIERS],
    [
      "ex ample",
      defaultEngine.getSubmission("ex ample", null, "keyword").uri.spec,
      CANONIZE_MODIFIERS,
    ],
  ];

  // Disable autoFill for this test, since it could mess up the results.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.autoFill", false],
      ["browser.urlbar.ctrlCanonizesURLs", true],
    ],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();

  for (let [inputValue, expectedURL, options] of testcases) {
    info(`Testing input string: "${inputValue}" - expected: "${expectedURL}"`);
    let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
      expectedURL,
      win.gBrowser.selectedBrowser
    );
    let promiseStopped = BrowserTestUtils.browserStopped(
      win.gBrowser.selectedBrowser,
      undefined,
      true
    );
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    await UrlbarTestUtils.inputIntoURLBar(win, inputValue);
    EventUtils.synthesizeKey("KEY_Enter", options, win);
    await Promise.all([promiseLoad, promiseStopped]);
  }

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function checkPrefTurnsOffCanonize() {
  // Add a dummy search engine to avoid hitting the network.
  await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
    setAsDefault: true,
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();

  // Ensure we don't end up loading something in the current tab becuase it's empty:
  let initialTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    opening: "about:mozilla",
  });
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.ctrlCanonizesURLs", false]],
  });

  let newURL = "http://mochi.test:8888/?terms=example";
  let promiseLoaded = BrowserTestUtils.waitForNewTab(win.gBrowser);

  win.gURLBar.focus();
  win.gURLBar.selectionStart = win.gURLBar.selectionEnd =
    win.gURLBar.value.length;
  win.gURLBar.value = "exampl";
  EventUtils.sendString("e", win);
  EventUtils.synthesizeKey("KEY_Enter", CANONIZE_MODIFIERS, win);

  await promiseLoaded;

  Assert.equal(
    initialTab.linkedBrowser.currentURI.spec,
    "about:mozilla",
    "Original tab shouldn't have navigated"
  );
  Assert.equal(
    win.gBrowser.selectedBrowser.currentURI.spec,
    newURL,
    "New tab should have navigated"
  );

  while (win.gBrowser.tabs.length > 1) {
    win.gBrowser.removeTab(win.gBrowser.selectedTab, { animate: false });
  }

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function autofill() {
  // Re-enable autofill and canonization.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.autoFill", true],
      ["browser.urlbar.ctrlCanonizesURLs", true],
    ],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();

  // Quantumbar automatically disables autofill when the old search string
  // starts with the new search string, so to make sure that doesn't happen and
  // that earlier tests don't conflict with this one, start a new search for
  // some other string.
  win.gURLBar.select();
  EventUtils.sendString("blah", win);

  // Add a visit that will be autofilled.
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([
    {
      uri: "https://example.com/",
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);

  let testcases = [
    ["ex", "https://www.ex.com/", CANONIZE_MODIFIERS],
    // Check that a direct load is not overwritten by a previous canonization.
    ["ex", "https://example.com/", {}],
    // search alias
    ["@goo", "https://www.goo.com/", CANONIZE_MODIFIERS],
  ];

  for (let [inputValue, expectedURL, options] of testcases) {
    let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
      expectedURL,
      win.gBrowser.selectedBrowser
    );
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    win.gURLBar.select();
    let autofillPromise = BrowserTestUtils.waitForEvent(
      win.gURLBar.inputField,
      "select"
    );
    EventUtils.sendString(inputValue, win);
    await autofillPromise;
    EventUtils.synthesizeKey("KEY_Enter", options, win);
    await promiseLoad;

    // Here again, make sure autofill isn't disabled for the next search.  See
    // the comment above.
    win.gURLBar.select();
    EventUtils.sendString("blah", win);
  }

  await PlacesUtils.history.clear();

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function () {
  info(
    "Test whether canonization is disabled until the modifier key is releasing if the key was used to paste text into urlbar"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.ctrlCanonizesURLs", true]],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();

  info("Paste the word to the urlbar");
  const testWord = "example";
  simulatePastingToUrlbar(testWord, win);
  is(win.gURLBar.value, testWord, "Paste the test word correctly");

  info("Send enter key while pressing the canonization modifier");
  EventUtils.synthesizeKey("VK_RETURN", CANONIZE_MODIFIERS, win);
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  is(
    win.gBrowser.selectedBrowser.documentURI.spec,
    `http://mochi.test:8888/?terms=${testWord}`,
    "The loaded url is not canonized"
  );
  EventUtils.synthesizeKey(MODIFIER_KEY, { type: "keyup" }, win);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function () {
  info(
    "Test whether canonization is enabled again after releasing the modifier key"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.ctrlCanonizesURLs", true]],
  });

  const win = await BrowserTestUtils.openNewBrowserWindow();

  info("Paste the word to the urlbar");
  const testWord = "example";
  simulatePastingToUrlbar(testWord, win);
  is(win.gURLBar.value, testWord, "Paste the test word correctly");

  info("Release the canonization modifier before typing Enter key");
  EventUtils.synthesizeKey(MODIFIER_KEY, { type: "keyup" }, win);

  info("Send enter key with the canonization modifier");
  const onLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    `https://www.${testWord}.com/`,
    win.gBrowser.selectedBrowser
  );
  const onStop = BrowserTestUtils.browserStopped(
    win.gBrowser.selectedBrowser,
    undefined,
    true
  );
  EventUtils.synthesizeKey("VK_RETURN", CANONIZE_MODIFIERS, win);

  await Promise.all([onLoad, onStop]);
  info("The loaded url is canonized");

  await BrowserTestUtils.closeWindow(win);
});

function simulatePastingToUrlbar(text, win) {
  win.gURLBar.focus();

  const keyForPaste = win.document
    .getElementById("key_paste")
    .getAttribute("key")
    .toLowerCase();
  EventUtils.synthesizeKey(
    keyForPaste,
    AppConstants.platform == "macosx"
      ? { type: "keydown", metaKey: true }
      : { type: "keydown", ctrlKey: true },
    win
  );

  win.gURLBar.select();
  EventUtils.sendString(text, win);
}
