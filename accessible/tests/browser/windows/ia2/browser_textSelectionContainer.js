/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../mochitest/text.js */

loadScripts({ name: "text.js", dir: MOCHITESTS_DIR });

function checkSelection(root, ranges) {
  const selRanges = root.selectionRanges;
  is(selRanges.length, ranges.length, "Correct number of selection ranges");
  for (let r = 0; r < ranges.length; ++r) {
    const selRange = selRanges.queryElementAt(r, nsIAccessibleTextRange);
    testTextRange(selRange, root.id, ...ranges[r]);
  }
}

/**
 * Test IAccessibleTextSelectionContainer::setSelections.
 */
addAccessibleTask(
  `<p id="p">ab<a id="link" href="/">cd</a>ef<img id="img" src="https://example.com/a11y/accessible/tests/mochitest/moz.png" alt="g"></p>`,
  async function testSetSelections(browser, docAcc) {
    docAcc.QueryInterface(nsIAccessibleText);
    await runPython(`
      global doc, docSel, p, link
      doc = getDocIa2()
      docSel = doc.QueryInterface(IAccessibleTextSelectionContainer)
      p = findIa2ByDomId(doc, "p").QueryInterface(IAccessibleText)
      link = findIa2ByDomId(doc, "link").QueryInterface(IAccessibleText)
    `);

    info("Selecting ab, end at link 0");
    const p = findAccessibleChildByID(docAcc, "p");
    let selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, p);
    await runPython(`
      docSel.setSelections(1, byref(IA2TextSelection(p, 0, link, 0, False)))
    `);
    await selected;
    const link = findAccessibleChildByID(docAcc, "link");
    checkSelection(docAcc, [[p, 0, link, 0]]);

    info("Selecting bc");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, p);
    await runPython(`
      docSel.setSelections(1, byref(IA2TextSelection(p, 1, link, 1, False)))
    `);
    await selected;
    checkSelection(docAcc, [[p, 1, link, 1]]);

    info("Selecting ab, end before link");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, p);
    await runPython(`
      docSel.setSelections(1, byref(IA2TextSelection(p, 0, p, 2, False)))
    `);
    await selected;
    checkSelection(docAcc, [[p, 0, link, 0]]);

    info("Selecting de");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, p);
    await runPython(`
      docSel.setSelections(1, byref(IA2TextSelection(link, 1, p, 4, False)))
    `);
    await selected;
    checkSelection(docAcc, [[link, 1, p, 4]]);

    info("Selecting f");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, p);
    await runPython(`
      docSel.setSelections(1, byref(IA2TextSelection(p, 4, p, 5, False)))
    `);
    await selected;
    checkSelection(docAcc, [[p, 4, p, 5]]);
    // DOM treats an end point of (img, 0) as including the image. Ensure we
    // used a DOM child index.
    await invokeContentTask(browser, [], () => {
      const sel = content.getSelection();
      is(sel.focusNode.id, "p", "DOM selection focus node correct");
      is(sel.focusOffset, 3, "DOM selection focus offset correct");
    });

    info("Selecting fg");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, p);
    await runPython(`
      docSel.setSelections(1, byref(IA2TextSelection(p, 4, p, 6, False)))
    `);
    await selected;
    checkSelection(docAcc, [[p, 4, p, 6]]);

    info("Selecting g");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, p);
    await runPython(`
      docSel.setSelections(1, byref(IA2TextSelection(p, 5, p, 6, False)))
    `);
    await selected;
    checkSelection(docAcc, [[p, 5, p, 6]]);

    info("Selecting a, c");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, link);
    await runPython(`
      docSel.setSelections(2, (IA2TextSelection * 2)(
        IA2TextSelection(p, 0, p, 1, False),
        IA2TextSelection(link, 0, link, 1, False)
      ))
    `);
    await selected;
    checkSelection(docAcc, [
      [p, 0, p, 1],
      [link, 0, link, 1],
    ]);

    info("Clearing selection");
    selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, docAcc);
    await runPython(`
      docSel.setSelections(0, byref(IA2TextSelection()))
    `);
    await selected;
    checkSelection(docAcc, []);
  }
);
