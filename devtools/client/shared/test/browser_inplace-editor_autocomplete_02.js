/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

const AutocompletePopup = require("resource://devtools/client/shared/autocomplete-popup.js");
const {
  InplaceEditor,
} = require("resource://devtools/client/shared/inplace-editor.js");
loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor autocomplete popup for CSS values suggestions.
// Using a mocked list of CSS properties to avoid test failures linked to
// engine changes (new property, removed property, ...).

const mockValues = {
  "background-image": [
    "linear-gradient",
    "radial-gradient",
    "repeating-radial-gradient",
  ],
  color: ["blue", "red", "rgb"],
  display: ["block", "flex", "inline", "inline-block", "none"],
  "grid-template-areas": ["unset", "inherits"],
};

add_task(async function () {
  await addTab(
    "data:text/html;charset=utf-8," + "inplace editor CSS value autocomplete"
  );
  const { host, win, doc } = await createHost();

  info("Test for css property completion");
  await createEditorAndRunCompletionTest(doc, win, "display", [
    ["b", "block", -1, []],
    ["VK_BACK_SPACE", "b", -1, []],
    ["VK_BACK_SPACE", "", -1, []],
    ["i", "inline", 0, ["inline", "inline-block"]],
    ["VK_DOWN", "inline-block", 1, ["inline", "inline-block"]],
    ["VK_DOWN", "inline", 0, ["inline", "inline-block"]],
    ["VK_LEFT", "inline", -1, []],
    // Shift + navigation key shouldn't trigger autocomplete (Bug 1184538)
    [{ key: "VK_UP", shiftKey: true }, "inline", -1, [], null, false, true],
    [{ key: "VK_DOWN", shiftKey: true }, "inline", -1, [], null, false, true],
    [{ key: "VK_LEFT", shiftKey: true }, "inline", -1, [], null, false, true],
    [{ key: "VK_RIGHT", shiftKey: true }, "inline", -1, [], null, false, true],
    [{ key: "VK_HOME", shiftKey: true }, "inline", -1, [], null, false, true],
    [{ key: "VK_END", shiftKey: true }, "inline", -1, [], null, false, true],
    // "Select All" keyboard shortcut shouldn't trigger autocomplete
    [
      {
        key: "a",
        [AppConstants.platform == "macosx" ? "metaKey" : "ctrlKey"]: true,
      },
      "inline",
      -1,
      [],
      null,
      false,
      true,
    ],
  ]);

  info("Test for css property completion after css comment");
  await createEditorAndRunCompletionTest(doc, win, "display", [
    ["/", "/", -1, []],
    ["*", "/*", -1, []],
    // "/*/" is still an unclosed comment
    ["/", "/*/", -1, []],
    [" ", "/*/ ", -1, []],
    ["h", "/*/ h", -1, []],
    ["i", "/*/ hi", -1, []],
    [" ", "/*/ hi ", -1, []],
    ["*", "/*/ hi *", -1, []],
    // note that !important is not displayed here
    ["/", "/*/ hi */block", 0, mockValues.display],
    ["b", "/*/ hi */block", -1, []],
    ["VK_BACK_SPACE", "/*/ hi */b", -1, []],
    ["VK_BACK_SPACE", "/*/ hi */", -1, []],
    ["i", "/*/ hi */inline", 0, ["inline", "inline-block"]],
    ["VK_DOWN", "/*/ hi */inline-block", 1, ["inline", "inline-block"]],
    ["VK_DOWN", "/*/ hi */inline", 0, ["inline", "inline-block"]],
    ["VK_LEFT", "/*/ hi */inline", -1, []],
  ]);

  info("Test that !important is only added when we want");
  await createEditorAndRunCompletionTest(doc, win, "display", [
    // !important doesn't get displayed if there's only whitespace before the cursor
    [" ", " block", 0, mockValues.display],
    // !important doesn't get displayed if there's no meaningful char before
    ["!", " !", -1, []],
    ["i", " !i", -1, []],
    ["m", " !im", -1, []],
    // deleting `!im`
    ["VK_BACK_SPACE", " !i", -1, []],
    ["VK_BACK_SPACE", " !", -1, []],
    ["VK_BACK_SPACE", " ", -1, []],
    ["b", " block", -1, []],
    ["VK_RIGHT", " block", -1, []],
    // !important is displayed after a space
    [" ", " block block", 0, [...mockValues.display, "!important"]],
    [" ", " block  block", 0, [...mockValues.display, "!important"]],
    ["!", " block  !important", -1, []],
    ["VK_BACK_SPACE", " block  !", -1, []],
    ["VK_BACK_SPACE", " block  ", -1, []],
    ["VK_LEFT", " block  ", -1, []],
    // !important is displayed even if there is a space after the cursor
    [" ", " block  block ", 0, [...mockValues.display, "!important"]],
    // Add a char that doesn't match anything, and place the cursor before it
    ["x", " block  x ", -1, []],
    ["VK_LEFT", " block  x ", -1, []],
    // cursor is now between block and x: block | x (where | is the cursor)
    ["VK_LEFT", " block  x ", -1, []],
    // trigger the autocomplete, and check that !important is not in it
    [" ", " block  block x ", 0, mockValues.display],
    // cancel autocomplete
    ["VK_BACK_SPACE", " block   x ", -1, []],
    // Move to the end
    ...Array.from({ length: 3 }).map(() => ["VK_RIGHT", " block   x ", -1, []]),
    // Autocomplete !important
    ["!", " block   x !important", -1, []],
    // Accept autocomplete
    ["VK_RIGHT", " block   x !important", -1, []],
    // Check that no autocomplete appears when adding a space after !important
    [" ", " block   x !important ", -1, []],
    // And that we don't try to autocomplete another !important keyword
    ["!", " block   x !important !", -1, []],
  ]);

  info("Test for css property completion in gradient function");
  await createEditorAndRunCompletionTest(doc, win, "background-image", [
    [
      `r`,
      `radial-gradient`,
      0,
      ["radial-gradient", "repeating-radial-gradient"],
    ],
    // accept the completion
    [`VK_RIGHT`, `radial-gradient`, -1, []],
    // entering the opening bracket opens the autocomplete poup
    [`(`, `radial-gradient(blue)`, 0, ["blue", "red", "rgb"]],
    // the list gets properly filtered
    [`r`, `radial-gradient(red)`, 0, ["red", "rgb"]],
    // accept selected value
    [`VK_RIGHT`, `radial-gradient(red)`, -1, []],
    // entering a comma does trigger the autocomplete
    [`,`, `radial-gradient(red,blue)`, 0, ["blue", "red", "rgb"]],
    // entering a numerical value makes the autocomplete go away
    [`1`, `radial-gradient(red,1)`, -1, []],
    [`0`, `radial-gradient(red,10)`, -1, []],
    [`%`, `radial-gradient(red,10%)`, -1, []],
    // entering a space after the numerical value does trigger the autocomplete
    [` `, `radial-gradient(red,10% blue)`, 0, ["blue", "red", "rgb"]],
    [`r`, `radial-gradient(red,10% red)`, 0, ["red", "rgb"]],
    [`g`, `radial-gradient(red,10% rgb)`, -1, []],
    [`VK_RIGHT`, `radial-gradient(red,10% rgb)`, -1, []],
    // there is no autocomplete for "rgb()"
    [`(`, `radial-gradient(red,10% rgb())`, -1, []],
    [`0`, `radial-gradient(red,10% rgb(0))`, -1, []],
    [`,`, `radial-gradient(red,10% rgb(0,))`, -1, []],
    [`1`, `radial-gradient(red,10% rgb(0,1))`, -1, []],
    [`,`, `radial-gradient(red,10% rgb(0,1,))`, -1, []],
    [`2`, `radial-gradient(red,10% rgb(0,1,2))`, -1, []],
    // entering the closing parenthesis for rgb does not add a new one,
    // but reuse the one that was automatically inserted
    [`)`, `radial-gradient(red,10% rgb(0,1,2))`, -1, []],
    // entering a comma does trigger the autocomplete again after rgb() is closed
    [
      `,`,
      `radial-gradient(red,10% rgb(0,1,2),blue)`,
      0,
      ["blue", "red", "rgb"],
    ],
    // cancel adding a new argument in radial-gradient
    ["VK_BACK_SPACE", "radial-gradient(red,10% rgb(0,1,2),)", -1, []],
    ["VK_BACK_SPACE", "radial-gradient(red,10% rgb(0,1,2))", -1, []],
    // entering the closing parenthesis for radial-gradient does not add a new one,
    // but reuse the one that was automatically inserted
    [")", "radial-gradient(red,10% rgb(0,1,2))", -1, []],
    // entering a comma does trigger the autocomplete for the property again
    [
      ",",
      "radial-gradient(red,10% rgb(0,1,2)),linear-gradient",
      0,
      // we don't show !important here
      ["linear-gradient", "radial-gradient", "repeating-radial-gradient"],
    ],
    // don't go further, we covered everything
  ]);

  info("Test for no completion in string value");
  await createEditorAndRunCompletionTest(doc, win, "grid-template-areas", [
    [`"`, `"`, -1, 0],
    [`a`, `"a`, -1, 0],
    [` `, `"a `, -1, 0],
    [`.`, `"a .`, -1, 0],
    [`"`, `"a ."`, -1, 0],
  ]);

  host.destroy();
  gBrowser.removeCurrentTab();
});

/**
 *
 * @param {Document} doc
 * @param {Window} win
 * @param {String} property - The property name
 * @param {Array} testData - The data passed to helper_inplace_editor.js `testComplation`.
 *         format :
 *         [
 *           what key to press,
 *           expected input box value after keypress,
 *           selected suggestion index (-1 if popup is hidden),
 *           number of suggestions in the popup (0 if popup is hidden), or the array of items label
 *         ]
 */
async function createEditorAndRunCompletionTest(doc, win, property, testData) {
  const xulDocument = win.top.document;
  const popup = new AutocompletePopup(xulDocument, { autoSelect: true });

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
          name: property,
        },
        cssProperties: {
          getNames: () => Object.keys(mockValues),
          getValues: propertyName => mockValues[propertyName] || [],
        },
        done: resolve,
        popup,
      },
      doc
    );
  });
  popup.destroy();
}
