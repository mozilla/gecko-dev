/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the selector-search input proposes ids and classes even when . and
// # is missing, but that this only occurs when the query is one word (no
// selector combination)

// The various states of the inspector: [key, suggestions array]
// [
//  what key to press,
//  suggestions  [
//    suggestion1,
//    suggestion2,
//    …
//  ]
// ]
const KEY_STATES = [
  ["s", ["span", ".span", "#span"]],
  ["p", ["span", ".span", "#span"]],
  ["a", ["span", ".span", "#span"]],
  ["n", ["span", ".span", "#span"]],
  [" ", ["span div"]],
  // mixed tag/class/id suggestions only work for the first word
  ["d", ["span div"]],
  ["VK_BACK_SPACE", ["span div"]],
  ["VK_BACK_SPACE", ["span", ".span", "#span"]],
  ["VK_BACK_SPACE", ["span", ".span", "#span"]],
  ["VK_BACK_SPACE", ["span", ".span", "#span"]],
  ["VK_BACK_SPACE", ["span", ".span", "#span"]],
  ["VK_BACK_SPACE", []],
  // Test that mixed tags, classes and ids are grouped by types, sorted by
  // count and alphabetical order
  ["b", ["body", "button", ".ba", ".bb", ".bc", "#ba", "#bb", "#bc"]],
];

const TEST_URL = `<span class="span" id="span">
                    <div class="div" id="div"></div>
                  </span>
                  <button class="ba bc" id="bc"></button>
                  <button class="bb bc" id="bb"></button>
                  <button class="bc" id="ba"></button>`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURI(TEST_URL)
  );

  const searchBox = inspector.panelWin.document.getElementById(
    "inspector-searchbox"
  );
  const popup = inspector.searchSuggestions.searchPopup;

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  for (const [key, expectedSuggestions] of KEY_STATES) {
    info(
      "pressing key " +
        key +
        " to get suggestions " +
        JSON.stringify(expectedSuggestions)
    );

    const onCommand = once(searchBox, "input", true);
    const onSearchProcessingDone =
      inspector.searchSuggestions.once("processing-done");
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    await onCommand;

    info("Waiting for the suggestions to be retrieved");
    await onSearchProcessingDone;

    const actualSuggestions = Array.from(popup.list.querySelectorAll("li")).map(
      li => li.textContent
    );
    is(
      popup.isOpen ? actualSuggestions.length : 0,
      expectedSuggestions.length,
      "There are expected number of suggestions"
    );

    for (let i = 0; i < expectedSuggestions.length; i++) {
      is(
        actualSuggestions[i],
        expectedSuggestions[i],
        "The suggestion at " + i + "th index is correct."
      );
    }
  }
});
