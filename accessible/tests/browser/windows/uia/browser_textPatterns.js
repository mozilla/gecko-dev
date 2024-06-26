/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test the Text pattern's DocumentRange property. This also tests where the
 * Text pattern is exposed.
 */
addUiaTask(
  `
<div><input id="input" value="input"></div>
<textarea id="textarea">textarea</textarea>
<div id="contentEditable" contenteditable><p>content</p><p>editable</p></div>
<a id="link" href="#">link</a>
  `,
  async function testTextDocumentRange() {
    await definePyVar("doc", `getDocUia()`);
    await definePyVar("pattern", `getUiaPattern(doc, "Text")`);
    ok(await runPython(`bool(pattern)`), "doc has Text pattern");
    // The IA2 -> UIA proxy adds spaces between elements that don't exist.
    if (gIsUiaEnabled) {
      is(
        await runPython(`pattern.DocumentRange.GetText(-1)`),
        "inputtextareacontenteditablelink",
        "document DocumentRange Text correct"
      );
    }

    await assignPyVarToUiaWithId("input");
    await definePyVar("pattern", `getUiaPattern(input, "Text")`);
    ok(await runPython(`bool(pattern)`), "input has Text pattern");
    is(
      await runPython(`pattern.DocumentRange.GetText(-1)`),
      "input",
      "input DocumentRange Text correct"
    );

    await assignPyVarToUiaWithId("textarea");
    await definePyVar("pattern", `getUiaPattern(textarea, "Text")`);
    ok(await runPython(`bool(pattern)`), "textarea has Text pattern");
    is(
      await runPython(`pattern.DocumentRange.GetText(-1)`),
      "textarea",
      "textarea DocumentRange Text correct"
    );

    // The UIA -> IA2 proxy doesn't expose the Text pattern on contentEditables
    // without role="textbox".
    if (gIsUiaEnabled) {
      await assignPyVarToUiaWithId("contentEditable");
      await definePyVar("pattern", `getUiaPattern(contentEditable, "Text")`);
      ok(await runPython(`bool(pattern)`), "contentEditable has Text pattern");
      is(
        await runPython(`pattern.DocumentRange.GetText(-1)`),
        "contenteditable",
        "contentEditable DocumentRange Text correct"
      );
    }

    await testPatternAbsent("link", "Text");
  }
);

/**
 * Test the TextRange pattern's GetText method.
 */
addUiaTask(
  `<div id="editable" contenteditable role="textbox">a <span>b</span>`,
  async function testTextRangeGetText() {
    await runPython(`
      doc = getDocUia()
      editable = findUiaByDomId(doc, "editable")
      text = getUiaPattern(editable, "Text")
      global range
      range = text.DocumentRange
    `);
    is(await runPython(`range.GetText(-1)`), "a b", "GetText(-1) correct");
    is(await runPython(`range.GetText(0)`), "", "GetText(0) correct");
    is(await runPython(`range.GetText(1)`), "a", "GetText(1) correct");
    is(await runPython(`range.GetText(2)`), "a ", "GetText(2) correct");
    is(await runPython(`range.GetText(3)`), "a b", "GetText(3) correct");
    is(await runPython(`range.GetText(4)`), "a b", "GetText(4) correct");
  }
);
