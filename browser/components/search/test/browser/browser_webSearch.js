/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests `SearchUIUtils.webSearch` by pressing accel+k in various scenarios.
 * Some subtests intentionally overlap with tests in browser/components/urlbar.
 */

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

async function openBookmarksLibrary() {
  let { promise, resolve } = Promise.withResolvers();
  let library = window.openDialog(
    "chrome://browser/content/places/places.xhtml",
    "",
    "chrome,toolbar=yes,dialog=no,resizable",
    "BookmarksToolbar"
  );
  waitForFocus(() => resolve(library), library);
  return promise;
}

add_setup(async function () {
  // Install search extension to prevent connecting to the default search engine.
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });
});

add_task(async function test_urlbar() {
  Assert.ok(!gURLBar.searchMode, "Not in search mode initially.");

  let focusPromise = BrowserTestUtils.waitForEvent(gURLBar, "focus");
  EventUtils.synthesizeKey("k", { accelKey: true });
  await focusPromise;
  Assert.equal(
    document.activeElement,
    gURLBar.inputField,
    "Focused the urlbar."
  );
  Assert.ok(!!gURLBar.searchMode, "Went into search mode.");

  await UrlbarTestUtils.exitSearchMode(window);
  document.documentElement.focus();
});

add_task(async function test_searchBar() {
  let searchBar = await gCUITestUtils.addSearchBar();

  let focusPromise = BrowserTestUtils.waitForEvent(searchBar.textbox, "focus");
  EventUtils.synthesizeKey("k", { accelKey: true });
  await focusPromise;
  Assert.equal(
    document.activeElement,
    searchBar.textbox,
    "Focused the search bar."
  );

  gCUITestUtils.removeSearchBar();
  document.documentElement.focus();
});

add_task(async function test_urlbarReadOnly() {
  gURLBar.readOnly = true;

  let newWindowPromise = TestUtils.topicObserved(
    "browser-delayed-startup-finished"
  );
  EventUtils.synthesizeKey("k", { accelKey: true });
  let [newWindow] = await newWindowPromise;
  Assert.ok(true, "Opened a new window.");

  await TestUtils.waitForCondition(
    () => newWindow.document.activeElement == newWindow.gURLBar.inputField
  );
  Assert.ok(true, "Focused the urlbar of the new window.");
  Assert.ok(!!newWindow.gURLBar.searchMode, "Went into search mode.");

  gURLBar.readOnly = false;
  await BrowserTestUtils.closeWindow(newWindow);
});

add_task(async function test_popup() {
  Assert.ok(!gURLBar.searchMode, "Not in search mode initially.");
  let libraryWin = await openBookmarksLibrary();

  // Due to Bug 1953787, CTRL+K does not work inside library windows on
  // platforms other than mac, so we call SearchUIUtils.webSearch directly.
  if (AppConstants.platform == "macosx") {
    let focusPromise = BrowserTestUtils.waitForEvent(gURLBar, "focus");
    EventUtils.synthesizeKey("k", { accelKey: true }, libraryWin);
    await focusPromise;
  } else {
    SearchUIUtils.webSearch(libraryWin);
  }
  Assert.equal(
    document.activeElement,
    gURLBar.inputField,
    "Focused the urlbar."
  );
  Assert.ok(!!gURLBar.searchMode, "Went into search mode.");

  await BrowserTestUtils.closeWindow(libraryWin);
  await UrlbarTestUtils.exitSearchMode(window);
  document.documentElement.focus();
});
