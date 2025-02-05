"use strict";

// Test search html navigation using keys <ENTER> and <SHIFT>+<ENTER> as well
// as using buttons <prev> and <next> updates the search label.

const TEST_URL = URL_ROOT + "doc_markup_update-on-navigtion_1.html";

const STATES = {
  keys: [
    { shiftKey: false, expectedValue: "1 of 3" },
    { shiftKey: false, expectedValue: "2 of 3" },
    { shiftKey: true, expectedValue: "1 of 3" },
    { shiftKey: true, expectedValue: "3 of 3" },
  ],
  mouse: [
    { next: true, expectedValue: "1 of 3" },
    { next: true, expectedValue: "2 of 3" },
    { next: false, expectedValue: "1 of 3" },
    { next: false, expectedValue: "3 of 3" },
  ],
};

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Starting test...");
  const {
    searchBox,
    searchResultsLabel,
    searchPrevButton,
    searchNextButton,
    searchClearButton,
    search,
    searchResultsContainer,
    searchNavigationContainer,
  } = inspector;

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  info("Checking search label and navigation buttons are not displayed");
  ok(
    searchResultsContainer.hidden,
    "The search label and navigation buttons are not visible"
  );

  info("Searching for 'o'");
  searchBox.value = "o";

  info("Navigating using keys");
  await navigateUsingKeys(inspector, searchResultsLabel);

  info("Navigating using mouse");
  await navigateUsingMouse(
    inspector,
    searchResultsLabel,
    searchNextButton,
    searchPrevButton
  );

  info("Clearing the text");
  await clearText(
    search,
    searchClearButton,
    searchResultsLabel,
    searchResultsContainer
  );

  info("Searching for 'mozilla' (not-present)");
  searchBox.value = "mozilla";
  await findNotPresentString(
    inspector,
    searchResultsLabel,
    searchResultsContainer,
    searchNavigationContainer
  );
});

async function navigateUsingKeys(inspector, searchResultsLabel) {
  for (const { expectedValue, shiftKey } of STATES.keys) {
    const onSearchProcessingDone =
      inspector.searchSuggestions.once("processing-done");
    const onSearchResult = inspector.search.once("search-result");

    info(`Pressing ${shiftKey ? "<SHIFT>+" : ""}<ENTER> key.`);
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey }, inspector.panelWin);

    info("Waiting for results");
    await onSearchResult;

    info("Waiting for search query to complete");
    await onSearchProcessingDone;

    is(
      searchResultsLabel.textContent,
      expectedValue,
      "The search label shows correct values."
    );
  }
}

async function navigateUsingMouse(
  inspector,
  searchResultsLabel,
  nextBtn,
  prevBtn
) {
  for (const { next, expectedValue } of STATES.mouse) {
    const onSearchResult = inspector.search.once("search-result");

    info(`Clicking ${next ? "<NEXT>" : "<PREVIOUS>"} button`);
    EventUtils.sendMouseEvent({ type: "click" }, next ? nextBtn : prevBtn);

    info("Waiting for results");
    await onSearchResult;

    is(
      searchResultsLabel.textContent,
      expectedValue,
      "The search label shows correct values."
    );
  }
}

async function clearText(
  search,
  searchClearButton,
  searchResultsLabel,
  searchResultsContainer
) {
  const onSearchCleared = search.once("search-cleared");
  EventUtils.sendMouseEvent({ type: "click" }, searchClearButton);

  info("Waiting for search to clear");
  await onSearchCleared;

  is(
    searchResultsLabel.textContent,
    "",
    "The search label shows correct value."
  );

  info("Checking search label and navigation buttons are not displayed");
  ok(
    searchResultsContainer.hidden,
    "The search label and navigation buttons are not visible"
  );
}

async function findNotPresentString(
  inspector,
  searchResultsLabel,
  searchResultsContainer,
  searchNavigationContainer
) {
  const onSearchProcessingDone =
    inspector.searchSuggestions.once("processing-done");
  const onSearchResult = inspector.search.once("search-result");

  info(`Pressing <ENTER> key`);
  EventUtils.synthesizeKey("VK_RETURN", {}, inspector.panelWin);

  info("Waiting for results");
  await onSearchResult;

  info("Waiting for search query to complete");
  await onSearchProcessingDone;

  is(
    searchResultsLabel.textContent,
    "No matches",
    "The search label shows correct values."
  );

  info("Checking search label is displayed");
  ok(!searchResultsContainer.hidden, "The search label is visible");
  info("Checking navigation buttons are not displayed");
  ok(
    searchNavigationContainer.hidden,
    "The navigation buttons are not visible"
  );
}
