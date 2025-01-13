/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

const AutocompletePopup = require("resource://devtools/client/shared/autocomplete-popup.js");
const {
  InplaceEditor,
} = require("resource://devtools/client/shared/inplace-editor.js");
loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor autocomplete popup for variable suggestions.
// Using a mocked list of CSS variables to avoid test failures linked to
// engine changes (new property, removed property, ...).
// Also using a mocked list of CSS properties to avoid autocompletion when
// typing in "var"

// Used for representing the expectation of a visible color swatch
const COLORSWATCH = true;
// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    selected suggestion index (-1 if popup is hidden),
//    number of suggestions in the popup (0 if popup is hidden),
//    expected post label corresponding with the input box value,
//    boolean representing if there should be a colour swatch visible,
//  ]
const SIMPLE_TEST_DATA = [
  ["v", "v", -1, 0, null, !COLORSWATCH],
  ["a", "va", -1, 0, null, !COLORSWATCH],
  ["r", "var", -1, 0, null, !COLORSWATCH],
  ["(", "var(--abc)", 0, 9, null, !COLORSWATCH],
  ["-", "var(--abc)", 0, 9, "inherit", !COLORSWATCH],
  ["VK_BACK_SPACE", "var(-)", -1, 0, null, !COLORSWATCH],
  ["-", "var(--abc)", 0, 9, "inherit", !COLORSWATCH],
  ["VK_DOWN", "var(--def)", 1, 9, "transparent", !COLORSWATCH],
  ["VK_DOWN", "var(--ghi)", 2, 9, "#00FF00", COLORSWATCH],
  ["VK_DOWN", "var(--jkl)", 3, 9, "rgb(255, 0, 0)", COLORSWATCH],
  ["VK_DOWN", "var(--mno)", 4, 9, "hsl(120, 60%, 70%)", COLORSWATCH],
  ["VK_DOWN", "var(--pqr)", 5, 9, "BlueViolet", COLORSWATCH],
  ["VK_DOWN", "var(--stu)", 6, 9, "15px", !COLORSWATCH],
  ["VK_DOWN", "var(--vwx)", 7, 9, "rgba(255, 0, 0, 0.4)", COLORSWATCH],
  ["VK_DOWN", "var(--yz)", 8, 9, "hsla(120, 60%, 70%, 0.3)", COLORSWATCH],
  ["VK_DOWN", "var(--abc)", 0, 9, "inherit", !COLORSWATCH],
  ["VK_DOWN", "var(--def)", 1, 9, "transparent", !COLORSWATCH],
  ["VK_DOWN", "var(--ghi)", 2, 9, "#00FF00", COLORSWATCH],
  ["VK_LEFT", "var(--ghi)", -1, 0, null, !COLORSWATCH],
];

const IN_FUNCTION_TEST_DATA = [
  ["c", "c", -1, 0, null, !COLORSWATCH],
  ["a", "ca", -1, 0, null, !COLORSWATCH],
  ["l", "cal", -1, 0, null, !COLORSWATCH],
  ["c", "calc", -1, 0, null, !COLORSWATCH],
  ["(", "calc()", -1, 0, null, !COLORSWATCH],
  // include all the "simple test" steps, only wrapping the expected input value
  // inside `calc()`
  ...SIMPLE_TEST_DATA.map(data => [
    data[0],
    `calc(${data[1]})`,
    ...data.slice(2),
  ]),
];

const FALLBACK_VALUE_TEST_DATA = [
  ["v", "v", -1, 0, null, !COLORSWATCH],
  ["a", "va", -1, 0, null, !COLORSWATCH],
  ["r", "var", -1, 0, null, !COLORSWATCH],
  ["(", "var(--abc)", 0, 9, null, !COLORSWATCH],
  ["VK_RIGHT", "var(--abc)", -1, 0, null, !COLORSWATCH],
  [",", "var(--abc,wheat)", 0, 3, null, COLORSWATCH],
  ["w", "var(--abc,wheat)", 0, 2, null, COLORSWATCH],
];

const CSS_VARIABLES = [
  ["--abc", "inherit"],
  ["--def", "transparent"],
  ["--ghi", "#00FF00"],
  ["--jkl", "rgb(255, 0, 0)"],
  ["--mno", "hsl(120, 60%, 70%)"],
  ["--pqr", "BlueViolet"],
  ["--stu", "15px"],
  ["--vwx", "rgba(255, 0, 0, 0.4)"],
  ["--yz", "hsla(120, 60%, 70%, 0.3)"],
];

add_task(async function () {
  await addTab(
    "data:text/html;charset=utf-8,inplace editor CSS variable autocomplete"
  );
  const { host, doc } = await createHost();

  info("Test simple var() completion");
  await createEditorAndRunCompletionTest(doc, SIMPLE_TEST_DATA);

  info("Test var() in calc() completion");
  await createEditorAndRunCompletionTest(doc, IN_FUNCTION_TEST_DATA);

  info("Test var() fallback completion");
  await createEditorAndRunCompletionTest(doc, FALLBACK_VALUE_TEST_DATA, {
    color: ["wheat", "white", "yellow"],
  });

  host.destroy();
  gBrowser.removeCurrentTab();
});

async function createEditorAndRunCompletionTest(
  doc,
  testData,
  mockValues = {}
) {
  const popup = new AutocompletePopup(doc, { autoSelect: true });

  await new Promise(resolve => {
    createInplaceEditorAndClick(
      {
        start: async editor => {
          for (const data of testData) {
            await testCompletion(data, editor);
          }

          EventUtils.synthesizeKey("VK_RETURN", {}, editor.input.defaultView);
        },
        contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
        property: {
          name: "color",
        },
        cssProperties: {
          getNames: () => Object.keys(mockValues),
          getValues: propertyName => mockValues[propertyName] || [],
        },
        getCssVariables: () => new Map(CSS_VARIABLES),
        done: resolve,
        popup,
      },
      doc
    );
  });

  popup.destroy();
}
