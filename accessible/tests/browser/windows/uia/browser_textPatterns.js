/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable camelcase */
const SupportedTextSelection_None = 0;
const SupportedTextSelection_Multiple = 2;
/* eslint-enable camelcase */

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

    // The IA2 -> UIA proxy doesn't expose the Text pattern on contentEditables
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

/**
 * Test the TextRange pattern's Clone method.
 */
addUiaTask(
  `<input id="input" type="text" value="testing">`,
  async function testTextRangeClone() {
    await runPython(`
      doc = getDocUia()
      input = findUiaByDomId(doc, "input")
      text = getUiaPattern(input, "Text")
      global origRange
      origRange = text.DocumentRange
    `);
    is(
      await runPython(`origRange.GetText(-1)`),
      "testing",
      "origRange text correct"
    );
    await runPython(`
      global clonedRange
      clonedRange = origRange.Clone()
    `);
    is(
      await runPython(`clonedRange.GetText(-1)`),
      "testing",
      "clonedRange text correct"
    );

    // Test that modifying clonedRange doesn't impact origRange.
    info("Collapsing clonedRange to start");
    await runPython(
      `clonedRange.MoveEndpointByRange(TextPatternRangeEndpoint_End, clonedRange, TextPatternRangeEndpoint_Start)`
    );
    is(
      await runPython(`clonedRange.GetText(-1)`),
      "",
      "clonedRange text correct"
    );
    is(
      await runPython(`origRange.GetText(-1)`),
      "testing",
      "origRange text correct"
    );
  }
);

/**
 * Test the TextRange pattern's Compare method.
 */
addUiaTask(
  `<input id="input" type="text" value="testing">`,
  async function testTextRangeCompare() {
    await runPython(`
      doc = getDocUia()
      input = findUiaByDomId(doc, "input")
      text = getUiaPattern(input, "Text")
      global range1, range2
      range1 = text.DocumentRange
      range2 = text.DocumentRange
    `);
    ok(
      await runPython(`range1.Compare(range2)`),
      "range1 Compare range2 correct"
    );
    ok(
      await runPython(`range2.Compare(range1)`),
      "range2 Compare range1 correct"
    );
    info("Collapsing range2 to start");
    await runPython(
      `range2.MoveEndpointByRange(TextPatternRangeEndpoint_End, range2, TextPatternRangeEndpoint_Start)`
    );
    ok(
      !(await runPython(`range1.Compare(range2)`)),
      "range1 Compare range2 correct"
    );
    ok(
      !(await runPython(`range2.Compare(range1)`)),
      "range2 Compare range1 correct"
    );
  }
);

/**
 * Test the TextRange pattern's CompareEndpoints method.
 */
addUiaTask(
  `
<p>before</p>
<div><input id="input" type="text" value="input"></div>
<p>after</p>
  `,
  async function testTextRangeCompareEndpoints() {
    await runPython(`
      global doc, range1, range2
      doc = getDocUia()
      input = findUiaByDomId(doc, "input")
      text = getUiaPattern(input, "Text")
      range1 = text.DocumentRange
      range2 = text.DocumentRange
    `);
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_Start, range1, TextPatternRangeEndpoint_Start)`
      ),
      0,
      "Compare range1 start to range1 start correct"
    );
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_End, range1, TextPatternRangeEndpoint_End)`
      ),
      0,
      "Compare range1 end to range1 end correct"
    );
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_Start, range1, TextPatternRangeEndpoint_End)`
      ),
      -1,
      "Compare range1 start to range1 end correct"
    );
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_End, range1, TextPatternRangeEndpoint_Start)`
      ),
      1,
      "Compare range1 end to range1 start correct"
    );
    // Compare different ranges.
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_Start, range2, TextPatternRangeEndpoint_Start)`
      ),
      0,
      "Compare range1 start to range2 start correct"
    );
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_End, range2, TextPatternRangeEndpoint_End)`
      ),
      0,
      "Compare range1 end to range2 end correct"
    );
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_Start, range2, TextPatternRangeEndpoint_End)`
      ),
      -1,
      "Compare range1 start to range2 end correct"
    );
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_End, range2, TextPatternRangeEndpoint_Start)`
      ),
      1,
      "Compare range1 end to range2 start correct"
    );
    // Compare ranges created using different elements.
    await definePyVar("range3", `getUiaPattern(doc, "Text").DocumentRange`);
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_Start, range3, TextPatternRangeEndpoint_Start)`
      ),
      1,
      "Compare range1 start to range3 start correct"
    );
    is(
      await runPython(
        `range1.CompareEndpoints(TextPatternRangeEndpoint_End, range3, TextPatternRangeEndpoint_End)`
      ),
      -1,
      "Compare range1 end to range3 end correct"
    );
  }
);

/**
 * Test the TextRange pattern's ExpandToEnclosingUnit method.
 */
addUiaTask(
  `
<p>before</p>
<div><textarea id="textarea" cols="5">ab cd ef gh</textarea></div>
<div>after <input id="input" value="input"></div>
  `,
  async function testTextRangeExpandToEnclosingUnit() {
    info("Getting DocumentRange from textarea");
    await runPython(`
      global doc, range
      doc = getDocUia()
      textarea = findUiaByDomId(doc, "textarea")
      text = getUiaPattern(textarea, "Text")
      range = text.DocumentRange
    `);
    is(
      await runPython(`range.GetText(-1)`),
      "ab cd ef gh",
      "range text correct"
    );
    // Expand should shrink the range because it's too big.
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "a", "range text correct");
    info("Collapsing to end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End)`
    );
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    // range is now collapsed at "b".
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "b", "range text correct");
    info("Expanding to word");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Word)`);
    is(await runPython(`range.GetText(-1)`), "ab ", "range text correct");
    info("Collapsing to end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End)`
    );
    // range is now collapsed at "c".
    info("Expanding to word");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Word)`);
    is(await runPython(`range.GetText(-1)`), "cd ", "range text correct");
    info("Expanding to line");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Line)`);
    is(await runPython(`range.GetText(-1)`), "ab cd ", "range text correct");
    info("Collapsing to end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End)`
    );
    // range is now collapsed at "e".
    info("Expanding to line");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Line)`);
    // The IA2 -> UIA proxy gets most things below this wrong.
    if (!gIsUiaEnabled) {
      return;
    }
    is(await runPython(`range.GetText(-1)`), "ef gh", "range text correct");
    info("Expanding to document");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Document)`);
    is(
      await runPython(`range.GetText(-1)`),
      "beforeab cd ef ghafter input",
      "range text correct"
    );

    // Test expanding to a line which crosses elements.
    info("Getting DocumentRange from input");
    await runPython(`
      input = findUiaByDomId(doc, "input")
      text = getUiaPattern(input, "Text")
      global range
      range = text.DocumentRange
    `);
    info("Expanding to line");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Line)`);
    is(
      await runPython(`range.GetText(-1)`),
      "after input",
      "range text correct"
    );
    info("Collapsing to end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End)`
    );
    // range is now collapsed at the end of the document.
    info("Expanding to line");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Line)`);
    is(
      await runPython(`range.GetText(-1)`),
      "after input",
      "range text correct"
    );
  }
);

/**
 * Test the TextRange pattern's Move method.
 */
addUiaTask(
  `
<p>ab</p>
<textarea id="textarea">cd ef gh</textarea>
<p>ij</p>
  `,
  async function testTextRangeMove() {
    await runPython(`
      doc = getDocUia()
      textarea = findUiaByDomId(doc, "textarea")
      text = getUiaPattern(textarea, "Text")
      global range
      range = text.DocumentRange
    `);
    is(await runPython(`range.GetText(-1)`), "cd ef gh", "range text correct");
    info("Moving 1 word");
    is(
      await runPython(`range.Move(TextUnit_Word, 1)`),
      1,
      "Move return correct"
    );
    is(await runPython(`range.GetText(-1)`), "ef ", "range text correct");
    info("Moving 3 words");
    // There are only 2 words after.
    is(
      await runPython(`range.Move(TextUnit_Word, 3)`),
      2,
      "Move return correct"
    );
    // The IA2 -> UIA proxy gets most things below this wrong.
    if (!gIsUiaEnabled) {
      return;
    }
    is(await runPython(`range.GetText(-1)`), "ij", "range text correct");
    info("Moving -5 words");
    // There are only 4 words before.
    is(
      await runPython(`range.Move(TextUnit_Word, -5)`),
      -4,
      "Move return correct"
    );
    is(await runPython(`range.GetText(-1)`), "ab", "range text correct");
    info("Moving 1 word");
    is(
      await runPython(`range.Move(TextUnit_Word, 1)`),
      1,
      "Move return correct"
    );
    is(await runPython(`range.GetText(-1)`), "cd ", "range text correct");
    info("Moving 1 character");
    is(
      await runPython(`range.Move(TextUnit_Character, 1)`),
      1,
      "Move return correct"
    );
    is(await runPython(`range.GetText(-1)`), "d", "range text correct");
    // When the range is not collapsed, Move moves backward to the start of the
    // unit before moving to the requested unit.
    info("Moving -1 word");
    is(
      await runPython(`range.Move(TextUnit_Word, -1)`),
      -1,
      "Move return correct"
    );
    is(await runPython(`range.GetText(-1)`), "ab", "range text correct");
    info("Collapsing to start");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_End, range, TextPatternRangeEndpoint_Start)`
    );
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    // range is now collapsed at "a".
    info("Moving 1 word");
    is(
      await runPython(`range.Move(TextUnit_Word, 1)`),
      1,
      "Move return correct"
    );
    // range is now collapsed at "c".
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "c", "range text correct");
    info("Collapsing to end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End)`
    );
    // range is now collapsed at "d".
    // When the range is collapsed, Move does *not* first move back to the start
    // of the unit.
    info("Moving -1 word");
    is(
      await runPython(`range.Move(TextUnit_Word, -1)`),
      -1,
      "Move return correct"
    );
    // range is collapsed at "c".
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Word)`);
    is(await runPython(`range.GetText(-1)`), "cd ", "range text correct");
  }
);

/**
 * Test the TextRange pattern's MoveEndpointByRange method.
 */
addUiaTask(
  `
<p>ab</p>
<div><textarea id="textarea">cd ef gh</textarea></div>
<p>ij</p>
  `,
  async function testTextRangeMoveEndpointByRange() {
    await runPython(`
      global doc, taRange, range
      doc = getDocUia()
      textarea = findUiaByDomId(doc, "textarea")
      text = getUiaPattern(textarea, "Text")
      taRange = text.DocumentRange
      range = text.DocumentRange
    `);
    is(await runPython(`range.GetText(-1)`), "cd ef gh", "range text correct");
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "c", "range text correct");
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End)`
      ),
      -1,
      "start < end"
    );
    info("Moving end to start");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_End, range, TextPatternRangeEndpoint_Start)`
    );
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End)`
      ),
      0,
      "start == end"
    );
    info("Moving range end to textarea end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_End, taRange, TextPatternRangeEndpoint_End)`
    );
    is(await runPython(`range.GetText(-1)`), "cd ef gh", "range text correct");
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "c", "range text correct");
    info("Moving range start to textarea end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_Start, taRange, TextPatternRangeEndpoint_End)`
    );
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_Start, taRange, TextPatternRangeEndpoint_End)`
      ),
      0,
      "range start == textarea end"
    );
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_End, taRange, TextPatternRangeEndpoint_End)`
      ),
      0,
      "range end == textarea end"
    );
    info("Moving range end to textarea start");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_End, taRange, TextPatternRangeEndpoint_Start)`
    );
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_Start, taRange, TextPatternRangeEndpoint_Start)`
      ),
      0,
      "range start == textarea start"
    );
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_End, taRange, TextPatternRangeEndpoint_Start)`
      ),
      0,
      "range end == textarea start"
    );
    await definePyVar("docRange", `getUiaPattern(doc, "Text").DocumentRange`);
    info("Moving range start to document start");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_Start, docRange, TextPatternRangeEndpoint_Start)`
    );
    info("Moving range end to document end");
    await runPython(
      `range.MoveEndpointByRange(TextPatternRangeEndpoint_End, docRange, TextPatternRangeEndpoint_End)`
    );
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_Start, docRange, TextPatternRangeEndpoint_Start)`
      ),
      0,
      "range start == document start"
    );
    is(
      await runPython(
        `range.CompareEndpoints(TextPatternRangeEndpoint_End, docRange, TextPatternRangeEndpoint_End)`
      ),
      0,
      "range end == document end"
    );
  }
);

/**
 * Test the TextRange pattern's MoveEndpointByUnit method.
 */
addUiaTask(
  `
<p>ab</p>
<textarea id="textarea">cd ef gh</textarea>
<p>ij</p>
  `,
  async function testTextRangeMoveEndpointByUnit() {
    await runPython(`
      doc = getDocUia()
      textarea = findUiaByDomId(doc, "textarea")
      text = getUiaPattern(textarea, "Text")
      global range
      range = text.DocumentRange
    `);
    is(await runPython(`range.GetText(-1)`), "cd ef gh", "range text correct");
    info("Moving end -1 word");
    is(
      await runPython(
        `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Word, -1)`
      ),
      -1,
      "MoveEndpointByUnit return correct"
    );
    is(await runPython(`range.GetText(-1)`), "cd ef ", "range text correct");
    info("Moving end -4 words");
    // There are only 3 words before.
    is(
      await runPython(
        `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Word, -4)`
      ),
      -3,
      "MoveEndpointByUnit return correct"
    );
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    info("Moving start 1 word");
    is(
      await runPython(
        `range.MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Word, 1)`
      ),
      1,
      "MoveEndpointByUnit return correct"
    );
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    info("Moving end 1 character");
    is(
      await runPython(
        `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1)`
      ),
      1,
      "MoveEndpointByUnit return correct"
    );
    is(await runPython(`range.GetText(-1)`), "c", "range text correct");
    info("Moving start 5 words");
    // There are only 4 word boundaries after.
    is(
      await runPython(
        `range.MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Word, 5)`
      ),
      4,
      "MoveEndpointByUnit return correct"
    );
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    info("Moving end -1 word");
    is(
      await runPython(
        `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Word, -1)`
      ),
      -1,
      "MoveEndpointByUnit return correct"
    );
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    info("Moving end 1 character");
    is(
      await runPython(
        `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1)`
      ),
      1,
      "MoveEndpointByUnit return correct"
    );
    is(await runPython(`range.GetText(-1)`), "i", "range text correct");
  }
);

/**
 * Test the Text pattern's SupportedTextSelection property.
 */
addUiaTask(
  `
<style>
body {
  user-select: none;
}
</style>
<input id="input">
  `,
  async function testTextSupportedTextSelection() {
    let result = await runPython(`
      global doc
      doc = getDocUia()
      input = findUiaByDomId(doc, "input")
      text = getUiaPattern(input, "Text")
      return text.SupportedTextSelection
    `);
    is(
      result,
      SupportedTextSelection_Multiple,
      "input SupportedTextSelection correct"
    );
    // The IA2 -> UIA bridge doesn't understand that text isn't selectable in
    // this document.
    if (gIsUiaEnabled) {
      is(
        await runPython(`getUiaPattern(doc, "Text").SupportedTextSelection`),
        SupportedTextSelection_None,
        "doc SupportedTextSelection correct"
      );
    }
  }
);

/**
 * Test the Text pattern's GetSelection method with the caret.
 */
addUiaTask(
  `<textarea id="textarea" cols="2">ab cd</textarea>`,
  async function testTextGetSelectionCaret(browser, docAcc) {
    await runPython(`
      doc = getDocUia()
      textarea = findUiaByDomId(doc, "textarea")
      global text
      text = getUiaPattern(textarea, "Text")
    `);
    is(await runPython(`text.GetSelection().Length`), 0, "No selection");
    info("Focusing textarea");
    const textarea = findAccessibleChildByID(docAcc, "textarea");
    let moved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    textarea.takeFocus();
    await moved;
    is(await runPython(`text.GetSelection().Length`), 1, "1 selection");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    ok(await runPython(`bool(range)`), "Got selection range 0");
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "a", "range text correct");

    info("Pressing ArrowRight");
    moved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    await moved;
    is(await runPython(`text.GetSelection().Length`), 1, "1 selection");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    ok(await runPython(`bool(range)`), "Got selection range 0");
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "b", "range text correct");

    // The IA2 -> UIA proxy doesn't handle the insertion point at the end of a
    // line correctly.
    if (!gIsUiaEnabled) {
      return;
    }

    // Test the insertion point at the end of a wrapped line.
    info("Pressing End");
    moved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_End");
    await moved;
    is(await runPython(`text.GetSelection().Length`), 1, "1 selection");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    ok(await runPython(`bool(range)`), "Got selection range 0");
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    info("Moving end 1 character");
    await runPython(
      `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1)`
    );
    is(await runPython(`range.GetText(-1)`), "c", "range text correct");
    info("Expanding to line at caret");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Line)`);
    is(await runPython(`range.GetText(-1)`), "ab ", "range text correct");

    // Test the insertion point at the end of the textarea.
    info("Pressing Ctrl+End");
    moved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_End", { ctrlKey: true });
    await moved;
    is(await runPython(`text.GetSelection().Length`), 1, "1 selection");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    ok(await runPython(`bool(range)`), "Got selection range 0");
    info("Expanding to character");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
    is(await runPython(`range.GetText(-1)`), "", "range text correct");
    info("Expanding to line");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Line)`);
    is(await runPython(`range.GetText(-1)`), "cd", "range text correct");
  }
);

/**
 * Test the Text pattern's GetSelection method with selection.
 */
addUiaTask(
  `<textarea id="textarea" cols="3">ab cd</textarea>`,
  async function testTextGetSelectionSelection(browser, docAcc) {
    await runPython(`
      doc = getDocUia()
      textarea = findUiaByDomId(doc, "textarea")
      global text
      text = getUiaPattern(textarea, "Text")
    `);
    is(await runPython(`text.GetSelection().Length`), 0, "No selection");
    info("Focusing textarea");
    const textarea = findAccessibleChildByID(docAcc, "textarea");
    let moved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    textarea.takeFocus();
    await moved;
    is(await runPython(`text.GetSelection().Length`), 1, "1 selection");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    ok(await runPython(`bool(range)`), "Got selection range 0");
    is(await runPython(`range.GetText(-1)`), "", "range text correct");

    info("Selecting ab");
    moved = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, textarea);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("textarea").setSelectionRange(0, 2);
    });
    await moved;
    is(await runPython(`text.GetSelection().Length`), 1, "1 selection");
    await definePyVar("range", `text.GetSelection().GetElement(0)`);
    ok(await runPython(`bool(range)`), "Got selection range 0");
    is(await runPython(`range.GetText(-1)`), "ab", "range text correct");

    // XXX Multiple selections aren't possible in editable text. A test for that
    // should be added in bug 1901458.
  }
);

/**
 * Test the Text pattern's TextSelectionChanged event.
 */
addUiaTask(
  `<input id="input" value="abc">`,
  async function testTextTextSelectionChanged(browser) {
    info("Focusing input");
    await setUpWaitForUiaEvent("Text_TextSelectionChanged", "input");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("input").focus();
    });
    await waitForUiaEvent();
    ok(true, "input got TextSelectionChanged event");
    info("Moving caret to b");
    await setUpWaitForUiaEvent("Text_TextSelectionChanged", "input");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("input").setSelectionRange(1, 1);
    });
    await waitForUiaEvent();
    ok(true, "input got TextSelectionChanged event");
    info("Selecting bc");
    await setUpWaitForUiaEvent("Text_TextSelectionChanged", "input");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("input").setSelectionRange(1, 3);
    });
    await waitForUiaEvent();
    ok(true, "input got TextSelectionChanged event");
  }
);

/**
 * Test the Text pattern's TextChanged event.
 */
addUiaTask(
  `<input id="input" value="abc">`,
  async function testTextTextChanged(browser) {
    info("Focusing input");
    let moved = waitForEvent(EVENT_TEXT_CARET_MOVED, "input");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("input").focus();
    });
    await moved;
    info("Deleting a");
    await setUpWaitForUiaEvent("Text_TextChanged", "input");
    await invokeContentTask(browser, [], () => {
      content.document.execCommand("forwardDelete");
    });
    await waitForUiaEvent();
    ok(true, "input got TextChanged event");
    info("Inserting a");
    await setUpWaitForUiaEvent("Text_TextChanged", "input");
    await invokeContentTask(browser, [], () => {
      content.document.execCommand("insertText", false, "a");
    });
    await waitForUiaEvent();
    ok(true, "input got TextChanged event");
  }
);

/**
 * Test the TextRange pattern's GetEnclosingElement method.
 */
addUiaTask(
  `<div id="editable" contenteditable role="textbox">ab <mark id="cdef"><span>cd</span> <a id="ef" href="/">ef</a></mark> <img id="g" src="https://example.com/a11y/accessible/tests/mochitest/moz.png" alt="g"></div>`,
  async function testTextRangeGetEnclosingElement() {
    info("Getting editable DocumentRange");
    await runPython(`
      doc = getDocUia()
      editable = findUiaByDomId(doc, "editable")
      text = getUiaPattern(editable, "Text")
      global range
      range = text.DocumentRange
    `);
    is(
      await runPython(`range.GetEnclosingElement().CurrentAutomationId`),
      "editable",
      "EnclosingElement is editable"
    );
    info("Expanding to word");
    await runPython(`range.ExpandToEnclosingUnit(TextUnit_Word)`);
    // Range is now "ab ".
    // The IA2 -> UIA proxy gets this wrong.
    if (gIsUiaEnabled) {
      is(
        await runPython(`range.GetEnclosingElement().CurrentName`),
        "ab ",
        "EnclosingElement is ab text leaf"
      );
    }
    info("Moving 1 word");
    await runPython(`range.Move(TextUnit_Word, 1)`);
    // Range is now "cd ".
    // The "cd" text leaf doesn't include the space, so the enclosing element is
    // its parent.
    is(
      await runPython(`range.GetEnclosingElement().CurrentAutomationId`),
      "cdef",
      "EnclosingElement is cdef"
    );
    info("Moving end -1 character");
    await runPython(
      `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, -1)`
    );
    // Range is now "cd".
    // The IA2 -> UIA proxy gets this wrong.
    if (gIsUiaEnabled) {
      is(
        await runPython(`range.GetEnclosingElement().CurrentName`),
        "cd",
        "EnclosingElement is cd text leaf"
      );
    }
    info("Moving 1 word");
    await runPython(`range.Move(TextUnit_Word, 1)`);
    // Range is now "ef ".
    // Neither the "ef" text leaf/link nor the "cdef" mark include the trailing
    // space, so the enclosing element is cdef's parent.
    is(
      await runPython(`range.GetEnclosingElement().CurrentAutomationId`),
      "editable",
      "EnclosingElement is editable"
    );
    info("Moving end -1 character");
    await runPython(
      `range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, -1)`
    );
    // Range is now "ef".
    is(
      await runPython(`range.GetEnclosingElement().CurrentName`),
      "ef",
      "EnclosingElement is ef text leaf"
    );
    info("Moving 1 word");
    await runPython(`range.Move(TextUnit_Word, 1)`);
    // Range is now the embedded object character for the img (g).
    // The IA2 -> UIA proxy gets this wrong.
    if (gIsUiaEnabled) {
      is(
        await runPython(`range.GetEnclosingElement().CurrentAutomationId`),
        "g",
        "EnclosingElement is g"
      );
    }
  }
);

/**
 * Test the TextRange pattern's GetChildren method.
 */
addUiaTask(
  `<div id="editable" contenteditable role="textbox">ab <span id="cdef" role="button"><span>cd</span> <a id="ef" href="/">ef</a> </span><img id="g" src="https://example.com/a11y/accessible/tests/mochitest/moz.png" alt="g"></div>`,
  async function testTextRangeGetChildren() {
    info("Getting editable DocumentRange");
    await runPython(`
      doc = getDocUia()
      editable = findUiaByDomId(doc, "editable")
      text = getUiaPattern(editable, "Text")
      global r
      r = text.DocumentRange
    `);
    await isUiaElementArray(
      `r.GetChildren()`,
      ["cdef", "g"],
      "Children are correct"
    );
    info("Expanding to word");
    await runPython(`r.ExpandToEnclosingUnit(TextUnit_Word)`);
    // Range is now "ab ".
    await isUiaElementArray(`r.GetChildren()`, [], "Children are correct");
    info("Moving 1 word");
    await runPython(`r.Move(TextUnit_Word, 1)`);
    // Range is now "cd ".
    await isUiaElementArray(`r.GetChildren()`, [], "Children are correct");
    info("Moving 1 word");
    await runPython(`r.Move(TextUnit_Word, 1)`);
    // Range is now "ef ". The range includes the link but is not completely
    // enclosed by the link.
    await isUiaElementArray(`r.GetChildren()`, ["ef"], "Children are correct");
    info("Moving end -1 character");
    await runPython(
      `r.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, -1)`
    );
    // Range is now "ef". The range encloses the link, so there are no children.
    await isUiaElementArray(`r.GetChildren()`, [], "Children are correct");
    info("Moving 1 word");
    await runPython(`r.Move(TextUnit_Word, 1)`);
    // Range is now the embedded object character for the img (g). The range is
    // completely enclosed by the image.
    // The IA2 -> UIA proxy gets this wrong.
    if (gIsUiaEnabled) {
      await isUiaElementArray(`r.GetChildren()`, [], "Children are correct");
    }
  }
);

/**
 * Test the Text pattern's RangeFromChild method.
 */
addUiaTask(
  `<div id="editable" contenteditable role="textbox">ab <mark id="cdef"><span>cd</span> <a id="ef" href="/">ef</a></mark> <img id="g" src="https://example.com/a11y/accessible/tests/mochitest/moz.png" alt="g"></div>`,
  async function testTextRangeFromChild() {
    await runPython(`
      global doc, docText, editable, edText
      doc = getDocUia()
      docText = getUiaPattern(doc, "Text")
      editable = findUiaByDomId(doc, "editable")
      edText = getUiaPattern(editable, "Text")
    `);
    is(
      await runPython(`docText.RangeFromChild(editable).GetText(-1)`),
      `ab cd ef ${kEmbedChar}`,
      "doc returned correct range for editable"
    );
    await testPythonRaises(
      `edText.RangeFromChild(editable)`,
      "editable correctly failed to return range for editable"
    );
    is(
      await runPython(`docText.RangeFromChild(editable).GetText(-1)`),
      `ab cd ef ${kEmbedChar}`,
      "doc returned correct range for editable"
    );
    let text = await runPython(`
      ab = uiaClient.RawViewWalker.GetFirstChildElement(editable)
      range = docText.RangeFromChild(ab)
      return range.GetText(-1)
    `);
    is(text, "ab ", "doc returned correct range for ab");
    text = await runPython(`
      global cdef
      cdef = findUiaByDomId(doc, "cdef")
      range = docText.RangeFromChild(cdef)
      return range.GetText(-1)
    `);
    is(text, "cd ef", "doc returned correct range for cdef");
    text = await runPython(`
      cd = uiaClient.RawViewWalker.GetFirstChildElement(cdef)
      range = docText.RangeFromChild(cd)
      return range.GetText(-1)
    `);
    is(text, "cd", "doc returned correct range for cd");
    text = await runPython(`
      global efLink
      efLink = findUiaByDomId(doc, "ef")
      range = docText.RangeFromChild(efLink)
      return range.GetText(-1)
    `);
    is(text, "ef", "doc returned correct range for ef link");
    text = await runPython(`
      efLeaf = uiaClient.RawViewWalker.GetFirstChildElement(efLink)
      range = docText.RangeFromChild(efLeaf)
      return range.GetText(-1)
    `);
    is(text, "ef", "doc returned correct range for ef leaf");
    text = await runPython(`
      g = findUiaByDomId(doc, "g")
      range = docText.RangeFromChild(g)
      return range.GetText(-1)
    `);
    is(text, kEmbedChar, "doc returned correct range for g");
  },
  // The IA2 -> UIA proxy has too many quirks/bugs here.
  { uiaEnabled: true, uiaDisabled: false }
);

/**
 * Test the Text pattern's RangeFromPoint method.
 */
addUiaTask(
  `<div id="test">a <span>b </span>c</div>`,
  async function testTextRangeFromPoint(browser, docAcc) {
    const acc = findAccessibleChildByID(docAcc, "test", [nsIAccessibleText]);
    await runPython(`
      global doc, docText
      doc = getDocUia()
      docText = getUiaPattern(doc, "Text")
    `);

    // Walk through every offset in the accessible and hit test each. Verify
    // that the returned range is empty, and that it hit the right character.
    for (let offset = 0; offset < acc.characterCount; ++offset) {
      const x = {};
      const y = {};
      acc.getCharacterExtents(offset, x, y, {}, {}, COORDTYPE_SCREEN_RELATIVE);
      await runPython(`
        global range
        range = docText.RangeFromPoint(POINT(${x.value}, ${y.value}))`);
      is(
        await runPython(`range.GetText(-1)`),
        ``,
        "doc returned correct empty range"
      );
      await runPython(`range.ExpandToEnclosingUnit(TextUnit_Character)`);
      const charAtOffset = acc.getCharacterAtOffset(offset);
      is(
        await runPython(`range.GetText(-1)`),
        `${charAtOffset}`,
        "doc returned correct range"
      );
    }

    // An arbitrary invalid point should cause an invalid argument error.
    await testPythonRaises(
      `docText.RangeFromPoint(POINT(9999999999, 9999999999))`,
      "no text leaves at invalid point"
    );
  },
  { uiaEnabled: true, uiaDisabled: true }
);

/**
 * Test the TextRange pattern's GetBoundingRectangles method.
 */
addUiaTask(
  `
<div id="test"><p id="line1">abc</p><p id="line2">d</p><p id="line3"></p></div>
<div id="offscreen" style="position:absolute; left:200vw;">xyz</div>
  `,
  async function testTextRangeGetBoundingRectangles(browser, docAcc) {
    const line1 = findAccessibleChildByID(docAcc, "line1", [nsIAccessibleText]);
    const line2 = findAccessibleChildByID(docAcc, "line2", [nsIAccessibleText]);

    const lineRects = await runPython(`
      global doc, docText, testAcc, range
      doc = getDocUia()
      docText = getUiaPattern(doc, "Text")
      testAcc = findUiaByDomId(doc, "test")
      range = docText.RangeFromChild(testAcc)
      return range.GetBoundingRectangles()
    `);

    is(lineRects.length, 8, "GetBoundingRectangles returned two rectangles");
    const firstLineRect = [
      lineRects[0],
      lineRects[1],
      lineRects[2],
      lineRects[3],
    ];
    const secondLineRect = [
      lineRects[4],
      lineRects[5],
      lineRects[6],
      lineRects[7],
    ];
    testTextBounds(line1, 0, -1, firstLineRect, COORDTYPE_SCREEN_RELATIVE);
    testTextBounds(line2, 0, -1, secondLineRect, COORDTYPE_SCREEN_RELATIVE);
    // line3 has no rectangle - GetBoundingRectangles shouldn't return anything for empty lines.

    // GetBoundingRectangles shouldn't return anything for offscreen lines.
    const offscreenRects = await runPython(`
      global offscreenAcc, range
      offscreenAcc = findUiaByDomId(doc, "offscreen")
      range = docText.RangeFromChild(offscreenAcc)
      return range.GetBoundingRectangles()
    `);
    is(
      offscreenRects.length,
      0,
      "GetBoundingRectangles returned no rectangles"
    );
  },
  { uiaEnabled: true, uiaDisabled: true, chrome: true }
);
