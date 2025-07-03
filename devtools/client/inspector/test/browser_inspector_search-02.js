/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that searching for combining selectors using the inspector search
// field produces correct suggestions.

const TEST_URL = URL_ROOT + "doc_inspector_search-suggestions.html";

// See head.js `checkMarkupSearchSuggestions` function
const TEST_DATA = [
  {
    key: "d",
    value: "d",
    suggestions: ["div", "#d1", "#d2"],
  },
  {
    key: "i",
    value: "di",
    suggestions: ["div"],
  },
  {
    key: "v",
    value: "div",
    suggestions: [],
  },
  {
    key: " ",
    value: "div ",
    suggestions: ["div div", "div span"],
  },
  {
    key: ">",
    value: "div >",
    suggestions: ["div >div", "div >span"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "div ",
    suggestions: ["div div", "div span"],
  },
  {
    key: "+",
    value: "div +",
    suggestions: ["div +span"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "div ",
    suggestions: ["div div", "div span"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "div",
    suggestions: [],
  },
  {
    key: "VK_BACK_SPACE",
    value: "di",
    suggestions: ["div"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "d",
    suggestions: ["div", "#d1", "#d2"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "",
    suggestions: [],
  },
  {
    key: "p",
    value: "p",
    suggestions: ["p", "#p1", "#p2", "#p3"],
  },
  {
    key: " ",
    value: "p ",
    suggestions: ["p strong"],
  },
  {
    key: "+",
    value: "p +",
    suggestions: ["p +button", "p +footer", "p +p"],
  },
  {
    key: "b",
    value: "p +b",
    suggestions: ["p +button"],
  },
  {
    key: "u",
    value: "p +bu",
    suggestions: ["p +button"],
  },
  {
    key: "t",
    value: "p +but",
    suggestions: ["p +button"],
  },
  {
    key: "t",
    value: "p +butt",
    suggestions: ["p +button"],
  },
  {
    key: "o",
    value: "p +butto",
    suggestions: ["p +button"],
  },
  {
    key: "n",
    value: "p +button",
    suggestions: [],
  },
  {
    key: "+",
    value: "p +button+",
    suggestions: ["p +button+p"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "p +button",
    suggestions: [],
  },
  {
    key: "~",
    value: "p +button~",
    suggestions: ["p +button~footer", "p +button~p"],
  },
  {
    key: "f",
    value: "p +button~f",
    suggestions: ["p +button~footer"],
  },
];

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);
  await checkMarkupSearchSuggestions(inspector, TEST_DATA);
});

add_task(async function testEscape() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  // Get in a state where the suggestions popup is displayed
  await checkMarkupSearchSuggestions(inspector, [
    { key: "d", value: "d", suggestions: ["div", "#d1", "#d2"] },
  ]);

  const searchBox = inspector.searchBox;
  const popup = inspector.searchSuggestions.searchPopup;

  ok(popup.isOpen, `The suggestions popup is open`);
  is(searchBox.value, "d", "search input has expected value");

  info("Check that Esc closes the suggestions popup");
  const onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, inspector.panelWin);
  await onPopupClose;
  ok(!popup.isOpen, `The suggestions popup is closed`);
  is(searchBox.value, "d", "The search input value didn't changed");

  info("Check that Esc clears the input when the popup isn't opened");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, inspector.panelWin);
  is(searchBox.value, "", "The search input was cleared");
});
