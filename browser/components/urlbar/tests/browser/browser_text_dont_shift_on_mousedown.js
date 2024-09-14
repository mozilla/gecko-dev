/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that focusing the urlbar with mousedown doesn't shift the text until
 * mouseup, as that may cause unexpected text selections.
 */

var gDefaultEngine;

add_setup(async function () {
  let cleanup = await installPersistTestEngines();
  gDefaultEngine = Services.search.getEngineByName("Example");
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    cleanup();
  });
});

add_task(async function test_loaded_page() {
  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    let originalX = gURLBar.inputField.getBoundingClientRect().x;
    let finalX = getElementXBetweenMouseDownAndUp(gURLBar.inputField);
    Assert.equal(originalX, finalX, "The input field didn't move");
  });
});

add_task(async function test_invalid_proxystate() {
  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    gURLBar.value = "modifiedText";
    let originalX = gURLBar.inputField.getBoundingClientRect().x;
    let finalX = getElementXBetweenMouseDownAndUp(gURLBar.inputField);
    Assert.equal(originalX, finalX, "The input field didn't move");
  });
});

add_task(async function test_persistedSearchTerms() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  await BrowserTestUtils.withNewTab("https://example.com/", async browser => {
    const searchTerms = "pizza";
    let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(
      gDefaultEngine,
      searchTerms
    );
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      expectedSearchUrl
    );
    gURLBar.focus();
    gURLBar.value = searchTerms;
    EventUtils.synthesizeKey("KEY_Enter");
    await browserLoadedPromise;
    Assert.equal(gURLBar.value, searchTerms, "Search was persisted");
    Assert.ok(!gURLBar.focused, "Urlbar is not focused");

    let originalX = gURLBar.inputField.getBoundingClientRect().x;
    let finalX = getElementXBetweenMouseDownAndUp(gURLBar.inputField);
    Assert.equal(originalX, finalX, "The input field didn't move");
  });
});

add_task(async function test_dedicatedSearchButton() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
  await BrowserTestUtils.withNewTab("https://example.com/", async () => {
    let originalX = gURLBar.inputField.getBoundingClientRect().x;
    let finalX = getElementXBetweenMouseDownAndUp(gURLBar.inputField);
    Assert.equal(originalX, finalX, "The input field didn't move");
  });
});

function getElementXBetweenMouseDownAndUp(element) {
  EventUtils.synthesizeMouse(
    gURLBar.inputField,
    10,
    10,
    { type: "mousedown" },
    element.getOwnerGlobal
  );
  try {
    return element.getBoundingClientRect().x;
  } finally {
    EventUtils.synthesizeMouse(
      gURLBar.inputField,
      10,
      10,
      { type: "mouseup" },
      element.getOwnerGlobal
    );
  }
}
