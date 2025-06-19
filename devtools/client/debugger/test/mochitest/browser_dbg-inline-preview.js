/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test checking inline preview feature

"use strict";

add_task(async function testInlinePreviews() {
  await pushPref("devtools.debugger.features.inline-preview", true);
  await pushPref("javascript.options.experimental.import_attributes", true);

  const dbg = await initDebugger(
    "doc-inline-preview.html",
    "inline-preview.js"
  );
  await selectSource(dbg, "inline-preview.js");

  // Reload the page to trigger the pause at the debugger statement
  // in the block on line 66.
  const onReload = reload(dbg);
  await waitForPaused(dbg);

  info("Check that debugger is paused in the block scope");
  await assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "inline-preview.js").id,
    66
  );

  await assertInlinePreviews(
    dbg,
    [
      { previews: [{ identifier: "x:", value: "1" }], line: 63 },
      { previews: [{ identifier: "x:", value: "2" }], line: 65 },
    ],
    // `block` is passed as the function name here because
    // we are testing a block scope
    "block"
  );

  await resume(dbg);

  await assertInlinePreviews(
    dbg,
    [
      { previews: [{ identifier: "x:", value: "1" }], line: 63 },
      {
        previews: [{ identifier: "dict:", value: 'Object { hello: "world" }' }],
        line: 68,
      },
      { previews: [{ identifier: "key:", value: '"hello"' }], line: 69 },
    ],
    "block"
  );

  await resume(dbg);
  await onReload;

  await invokeFunctionAndAssertInlinePreview(dbg, "checkValues", [
    { previews: [{ identifier: "a:", value: '""' }], line: 2 },
    { previews: [{ identifier: "b:", value: "false" }], line: 3 },
    { previews: [{ identifier: "c:", value: "undefined" }], line: 4 },
    { previews: [{ identifier: "d:", value: "null" }], line: 5 },
    { previews: [{ identifier: "e:", value: "Array []" }], line: 6 },
    { previews: [{ identifier: "f:", value: "Object { }" }], line: 7 },
    {
      previews: [{ identifier: "reg:", value: "/^\\p{RGI_Emoji}$/v" }],
      line: 8,
    },
    { previews: [{ identifier: "obj:", value: "Object { foo: 1 }" }], line: 9 },
    {
      previews: [
        {
          identifier: "bs:",
          value: "Array(101) [ {…}, {…}, {…}, … ]",
        },
      ],
      line: 13,
    },
  ]);

  await invokeFunctionAndAssertInlinePreview(dbg, "columnWise", [
    { previews: [{ identifier: "a:", value: '"a"' }], line: 21 },
    { previews: [{ identifier: "b:", value: '"b"' }], line: 22 },
    { previews: [{ identifier: "c:", value: '"c"' }], line: 23 },
  ]);

  // Check that referencing an object property previews the property, not the object (bug 1599917)
  await invokeFunctionAndAssertInlinePreview(dbg, "objectProperties", [
    {
      previews: [
        {
          identifier: "obj:",
          value: 'Object { hello: "world", a: {…} }',
        },
      ],
      line: 29,
    },
    { previews: [{ identifier: "obj.hello:", value: '"world"' }], line: 30 },
    { previews: [{ identifier: "obj.a.b:", value: '"c"' }], line: 31 },
  ]);

  await invokeFunctionAndAssertInlinePreview(dbg, "classProperties", [
    {
      previews: [
        { identifier: "i:", value: "2" },
        { identifier: "this.x:", value: "1" },
      ],
      line: 43,
    },
    {
      previews: [
        { identifier: "self:", value: `Object { x: 1, #privateVar: 2 }` },
      ],
      line: 44,
    },
  ]);

  // Check inline previews for values within a module script
  await invokeFunctionAndAssertInlinePreview(dbg, "runInModule", [
    { previews: [{ identifier: "val:", value: "4" }], line: 20 },
    { previews: [{ identifier: "ids:", value: "Array [ 1, 2 ]" }], line: 21 },
  ]);

  // Checks that open in inspector button works in inline preview
  invokeInTab("btnClick");
  await assertInlinePreviews(
    dbg,
    [{ previews: [{ identifier: "btn:", value: "button" }], line: 53 }],
    "onBtnClick"
  );
  await checkInspectorIcon(dbg);

  await dbg.toolbox.selectTool("jsdebugger");

  await waitForSelectedSource(dbg, "inline-preview.js");

  // Check preview of event ( event.target should be clickable )
  // onBtnClick function in inline-preview.js
  await assertInlinePreviews(
    dbg,
    [
      {
        previews: [
          {
            identifier: "event:",
            value: "click { target: button, buttons: 0, clientX: 0, … }",
          },
        ],
        line: 58,
      },
    ],
    "onBtnClick"
  );
  await checkInspectorIcon(dbg);
  await resume(dbg);

  await dbg.toolbox.closeToolbox();
});

add_task(async function testInlinePreviewsWithExplicitResourceManagement() {
  await pushPref("devtools.debugger.features.inline-preview", true);
  // javascript.options.experimental.explicit_resource_management is set to true, but it's
  // only supported on Nightly at the moment, so only check for SuppressedError if
  // they're supported.
  if (!AppConstants.ENABLE_EXPLICIT_RESOURCE_MANAGEMENT) {
    return;
  }
  const dbg = await initDebugger("doc-inline-preview.html");

  const onPaused = waitForPaused(dbg);
  dbg.commands.scriptCommand.execute(
    `
    function explicitResourceManagement() {
      using erm = {
        [Symbol.dispose]() {},
        foo: 42
      };
      console.log(erm.foo);
      debugger;
    }; explicitResourceManagement();`,
    {}
  );
  await onPaused;

  await invokeFunctionAndAssertInlinePreview(
    dbg,
    "explicitResourceManagement",
    [
      {
        previews: [
          {
            identifier: "erm:",
            value: `Object { foo: 42, Symbol("Symbol.dispose"): Symbol.dispose() }`,
          },
        ],
        line: 3,
      },
    ]
  );

  await dbg.toolbox.closeToolbox();
});

async function invokeFunctionAndAssertInlinePreview(
  dbg,
  fnName,
  expectedInlinePreviews
) {
  invokeInTab(fnName);
  await assertInlinePreviews(dbg, expectedInlinePreviews, fnName);
  await resume(dbg);
}

async function checkInspectorIcon(dbg) {
  const node = await waitForElement(dbg, "inlinePreviewOpenInspector");

  // Ensure hovering over button highlights the node in content pane
  const view = node.ownerDocument.defaultView;
  const { toolbox } = dbg;
  const onNodeHighlight = toolbox.getHighlighter().waitForHighlighterShown();

  EventUtils.synthesizeMouseAtCenter(node, { type: "mousemove" }, view);

  info("Wait for node to be highlighted");
  const { nodeFront } = await onNodeHighlight;
  is(nodeFront.displayName, "button", "The correct node was highlighted");

  // Ensure panel changes when button is clicked
  const onInspectorPanelLoad = waitForInspectorPanelChange(dbg);
  node.click();
  await onInspectorPanelLoad;

  await resume(dbg);
}
