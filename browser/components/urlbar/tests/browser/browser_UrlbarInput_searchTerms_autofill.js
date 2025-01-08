/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Checks autofill doesn't trigger when a user focuses the urlbar while search
 * terms persist. The tests search a word matching a bookmark. When a user
 * escapes persisted search by modifying the search terms, autofill should
 * be re-enabled.
 */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  let cleanup = await installPersistTestEngines();

  // Add a bookmark so that it can be autofilled.
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "https://somedomain.example/abc",
    title: "somedomain",
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
    cleanup();
  });
});

add_task(async function persist_search_and_focus_twice() {
  let { tab } = await searchWithTab("some");

  EventUtils.synthesizeMouseAtCenter(window.gURLBar.inputField, {}, window);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!details.autofill, "Not autofilling.");

  // Focusing is done twice because if there is caching of the urlbar view, we
  // want to make sure the cached version also doesn't autofill.
  gURLBar.blur();
  EventUtils.synthesizeMouseAtCenter(window.gURLBar.inputField, {}, window);
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!details.autofill, "Not autofilling.");

  BrowserTestUtils.removeTab(tab);
});

// Sanity check that disabling autofill for persisted search terms doesn't
// eliminate autofilling altogether.
add_task(async function persist_search_focus_and_modify() {
  let { tab } = await searchWithTab("some");

  EventUtils.synthesizeMouseAtCenter(window.gURLBar.inputField, {}, window);
  EventUtils.synthesizeKey("KEY_ArrowRight");
  EventUtils.synthesizeKey("d");
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill, "Is autofilling.");

  BrowserTestUtils.removeTab(tab);
});
