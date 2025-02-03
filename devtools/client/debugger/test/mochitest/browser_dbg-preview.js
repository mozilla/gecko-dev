/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test hovering on an object, which will show a popup and on a
// simple value, which will show a tooltip.

"use strict";

// Showing/hiding the preview tooltip can be slow as we wait for CodeMirror scroll...
requestLongerTimeout(2);

add_task(async function () {
  const dbg = await initDebugger("doc-preview.html", "preview.js");

  await testPreviews(dbg, "testInline", [
    { line: 17, column: 16, expression: "prop", result: 2 },
  ]);

  await selectSource(dbg, "preview.js");

  await testPreviews(dbg, "empties", [
    { line: 6, column: 9, expression: "a", result: '""' },
    { line: 7, column: 9, expression: "b", result: "false" },
    { line: 8, column: 9, expression: "c", result: "undefined" },
    { line: 9, column: 9, expression: "d", result: "null" },
  ]);

  await testPreviews(dbg, "objects", [
    { line: 27, column: 10, expression: "empty", result: "Object" },
    { line: 28, column: 22, expression: "foo", result: 1 },
  ]);

  await testPreviews(dbg, "smalls", [
    { line: 14, column: 9, expression: "a", result: '"..."' },
    { line: 15, column: 9, expression: "b", result: "true" },
    { line: 16, column: 9, expression: "c", result: "1" },
    {
      line: 17,
      column: 9,
      expression: "d",
      fields: [["length", "0"]],
    },
  ]);

  await testPreviews(dbg, "classPreview", [
    { line: 50, column: 20, expression: "x", result: 1 },
    { line: 50, column: 29, expression: "#privateVar", result: 2 },
    {
      line: 50,
      column: 47,
      expression: "#privateStatic",
      fields: [
        ["first", `"a"`],
        ["second", `"b"`],
      ],
    },
    {
      line: 51,
      column: 26,
      expression: "this",
      fields: [
        ["x", "1"],
        ["#privateVar", "2"],
      ],
    },
    { line: 51, column: 39, expression: "#privateVar", result: 2 },
  ]);

  await testPreviews(dbg, "multipleTokens", [
    { line: 81, column: 4, expression: "foo", result: "Object" },
    { line: 81, column: 11, expression: "blip", result: "Object" },
    { line: 82, column: 8, expression: "bar", result: "Object" },
    { line: 84, column: 16, expression: "boom", result: `0` },
  ]);

  await testPreviews(dbg, "thisProperties", [
    { line: 96, column: 13, expression: "myProperty", result: "Object" },
    { line: 96, column: 23, expression: "x", result: "this-myProperty-x" },
    {
      line: 98,
      column: 13,
      expression: "propertyName",
      result: "myProperty",
    },
    {
      line: 98,
      column: 26,
      expression: "y",
      result: "this-myProperty-y",
    },
    {
      line: 99,
      column: 14,
      expression: "propertyName",
      result: "myProperty",
    },
    {
      line: 99,
      column: 28,
      expression: "z",
      result: "this-myProperty-z",
    },
  ]);

  await testPreviews(dbg, "valueOfExpression", [
    { line: 107, column: 6, expression: "value", result: "foo" },
  ]);

  // javascript.options.experimental.explicit_resource_management is set to true, but it's
  // only supported on Nightly at the moment, so only check for SuppressedError if
  // they're supported.
  if (AppConstants.ENABLE_EXPLICIT_RESOURCE_MANAGEMENT) {
    info("Check that preview works in a script with `using` keyword");

    const onPaused = waitForPaused(dbg);
    dbg.commands.scriptCommand.execute(
      `
      {
        using erm = {
          [Symbol.dispose]() {},
          foo: 42
        };
        console.log(erm.foo);
        debugger;
      }`,
      {}
    );

    await onPaused;
    await assertPreviews(dbg, [
      // assignment
      { line: 3, column: 16, expression: "erm", result: "Object" },
      { line: 7, column: 26, expression: "foo", result: "42" },
    ]);
    await resume(dbg);
  }

  await selectSource(dbg, "preview.js");
  info(
    "Check that closing the preview tooltip doesn't release the underlying object actor"
  );
  invokeInTab("classPreview");
  await waitForPaused(dbg);
  info("Display popup a first time and hide it");
  await assertPreviews(dbg, [
    {
      line: 60,
      column: 7,
      expression: "y",
      fields: [["hello", "{…}"]],
    },
  ]);

  info("Display the popup again and try to expand a property");
  const { element: popupEl, tokenEl } = await tryHovering(
    dbg,
    60,
    7,
    "previewPopup"
  );
  const nodes = popupEl.querySelectorAll(".preview-popup .node");
  const initialNodesLength = nodes.length;
  nodes[1].querySelector(".theme-twisty").click();
  await waitFor(
    () =>
      popupEl.querySelectorAll(".preview-popup .node").length >
      initialNodesLength
  );
  ok(true, `"hello" was expanded`);
  await closePreviewForToken(dbg, tokenEl, "popup");
  await resume(dbg);
});

async function testPreviews(dbg, fnName, previews) {
  invokeInTab(fnName);
  await waitForPaused(dbg);

  await assertPreviews(dbg, previews);
  await resume(dbg);

  info(`Ran tests for ${fnName}`);
}
