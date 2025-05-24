/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the selector-search input proposes pseudo elements

const TEST_URL = `<h1>Hello</h1>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURI(TEST_URL)
  );

  const TESTS = [
    {
      input: ":",
      // we don't want to test the exact items that are suggested, as the test would fail
      // when new pseudo are added in the platform.
      // Only includes some items that should be suggested (`included`),
      // and some that should not be (`notIncluded`)
      suggestions: {
        included: [":active", ":empty", ":focus"],
        notIncluded: ["::selection", "::marker"],
      },
      inputAfterAcceptingSuggestion: ":active",
    },
    {
      // For now we don't support searching for pseudo element (Bug 1097991),
      // so the list should be empty
      input: "::",
      suggestions: {
        included: [],
      },
    },
    {
      input: "h1:",
      suggestions: {
        included: ["h1:active", "h1:empty", "h1:focus"],
        notIncluded: ["h1::selection", "h1::marker"],
      },
      inputAfterAcceptingSuggestion: "h1:active",
    },
    {
      input: "h1:not",
      suggestions: {
        included: ["h1:not("],
        notIncluded: ["h1:nth-child("],
      },
      inputAfterAcceptingSuggestion: "h1:not(",
    },
    {
      input: "h1:empty:",
      suggestions: {
        included: ["h1:empty:active", "h1:empty:empty", "h1:empty:focus"],
        notIncluded: ["h1::selection", "h1::marker"],
      },
      inputAfterAcceptingSuggestion: "h1:empty:active",
    },
    {
      input: "h1:empty:no",
      suggestions: {
        included: ["h1:empty:not("],
        notIncluded: ["h1:empty:nth-child("],
      },
      inputAfterAcceptingSuggestion: "h1:empty:not(",
    },
    {
      input: "body > h1:",
      suggestions: {
        included: ["body > h1:active", "body > h1:empty", "body > h1:focus"],
        notIncluded: ["body > h1::selection", "body > h1::marker"],
      },
      inputAfterAcceptingSuggestion: "body > h1:active",
    },
    {
      input: "body > h1:no",
      suggestions: {
        included: ["body > h1:not("],
        notIncluded: ["body > h1:nth-child("],
      },
      inputAfterAcceptingSuggestion: "body > h1:not(",
    },
  ];

  info("Focus the search box");
  await focusSearchBoxUsingShortcut(inspector.panelWin);

  const searchInputEl = inspector.panelWin.document.getElementById(
    "inspector-searchbox"
  );
  const { searchPopup } = inspector.searchSuggestions;

  for (const { input, suggestions, inputAfterAcceptingSuggestion } of TESTS) {
    info(`Checking suggestions for "${input}"`);

    const onPopupOpened = searchPopup.once("popup-opened");
    // the query for getting suggestions is not throttled and is fired for every char
    // being typed, so we avoid using EventUtils.sendString for the whole input to avoid
    // dealing with multiple events. Instead, put the value directly in the input, and only
    // type the last char.
    const onProcessingDone =
      inspector.searchSuggestions.once("processing-done");
    searchInputEl.value = input.substring(0, input.length - 1);
    EventUtils.sendChar(input.at(-1), inspector.panelWin);
    info("Wait for search query to complete");
    await onProcessingDone;

    const actualSuggestions = Array.from(
      searchPopup.list.querySelectorAll("li")
    ).map(li => li.textContent);

    if (!suggestions.included.length) {
      const res = await Promise.race([
        onPopupOpened,
        wait(1000).then(() => "TIMEOUT"),
      ]);
      is(res, "TIMEOUT", "popup did not open");
    } else {
      await onPopupOpened;
      ok(true, "suggestions popup opened");
    }

    for (const expectedLabel of suggestions.included) {
      ok(
        actualSuggestions.some(s => s === expectedLabel),
        `"${expectedLabel}" is in the list of suggestions for "${input}" (full list: ${JSON.stringify(actualSuggestions)})`
      );
    }

    for (const unexpectedLabel of suggestions.notIncluded || []) {
      ok(
        !actualSuggestions.some(s => s === unexpectedLabel),
        `"${unexpectedLabel}" is not in the list of suggestions for "${input}"`
      );
    }

    if (inputAfterAcceptingSuggestion) {
      info("Press tab to fill the search input with the first suggestion");
      const onSuggestionAccepted =
        inspector.searchSuggestions.once("processing-done");
      const onPopupClosed = searchPopup.once("popup-closed");
      EventUtils.synthesizeKey("VK_TAB", {}, inspector.panelWin);
      await onSuggestionAccepted;
      await onPopupClosed;

      is(
        searchInputEl.value,
        inputAfterAcceptingSuggestion,
        "input has expected value after accepting suggestion"
      );
    }

    info("Clear the input");
    const onSearchCleared = inspector.search.once("search-cleared");
    const onEmptySearchSuggestionProcessingDone =
      inspector.searchSuggestions.once("processing-done");

    // select the whole input and hit backspace to clear it
    searchInputEl.setSelectionRange(0, searchInputEl.value.length);
    EventUtils.synthesizeKey("VK_BACK_SPACE", {}, inspector.panelWin);
    await onSearchCleared;
    await onEmptySearchSuggestionProcessingDone;
  }
});
