/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/text.js */
/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

const boldAttrs = { "font-weight": "700" };
const highlightAttrs = { mark: "true" };
const fragmentAttrs = highlightAttrs;
const spellingAttrs = { invalid: "spelling" };
const grammarAttrs = { invalid: "grammar" };
const snippet = `
<p id="first">The first phrase.</p>
<p id="second">The <i>second <b>phrase.</b></i></p>
`;

/**
 * Returns a promise that resolves once the attribute ranges match. If
 * shouldWaitForEvent is true, we first wait for a text attribute change event.
 */
async function waitForTextAttrRanges(
  acc,
  ranges,
  attrs,
  shouldWaitForEvent = true
) {
  if (shouldWaitForEvent) {
    await waitForEvent(EVENT_TEXT_ATTRIBUTE_CHANGED);
  }
  await untilCacheOk(
    () => textAttrRangesMatch(acc, ranges, attrs),
    `Attr ranges match: ${JSON.stringify(ranges)}`
  );
}

/**
 * Test a text fragment within a single node.
 */
addAccessibleTask(
  snippet,
  async function testTextFragmentSingleNode(browser, docAcc) {
    const first = findAccessibleChildByID(docAcc, "first");
    ok(
      textAttrRangesMatch(
        first,
        [
          [4, 16], // "first phrase"
        ],
        fragmentAttrs
      ),
      "first attr ranges correct"
    );
    const second = findAccessibleChildByID(docAcc, "second");
    ok(
      textAttrRangesMatch(second, [], fragmentAttrs),
      "second attr ranges correct"
    );
  },
  { chrome: true, topLevel: true, urlSuffix: "#:~:text=first%20phrase" }
);

/**
 * Test a text fragment crossing nodes.
 */
addAccessibleTask(
  snippet,
  async function testTextFragmentCrossNode(browser, docAcc) {
    const first = findAccessibleChildByID(docAcc, "first");
    ok(
      textAttrRangesMatch(first, [], fragmentAttrs),
      "first attr ranges correct"
    );
    const second = findAccessibleChildByID(docAcc, "second");
    ok(
      textAttrRangesMatch(
        second,
        [
          // This run is split because of the bolded word.
          [4, 11], // "second "
          [11, 17], // "phrase"
        ],
        fragmentAttrs
      ),
      "second attr ranges correct"
    );
    // Ensure bold is still exposed in the presence of a fragment.
    testTextAttrs(
      second,
      11,
      { ...fragmentAttrs, ...boldAttrs },
      {},
      11,
      17,
      true
    ); // "phrase"
    testTextAttrs(second, 17, boldAttrs, {}, 17, 18, true); // "."
  },
  { chrome: true, topLevel: true, urlSuffix: "#:~:text=second%20phrase" }
);

/**
 * Test scrolling to a text fragment on the same page. This also tests that the
 * scrolling start event is fired.
 */
add_task(async function testTextFragmentSamePage() {
  // We use add_task here because we need to verify that an
  // event is fired, but it might be fired before document load complete, so we
  // could miss it if we used addAccessibleTask.
  const docUrl = snippetToURL(snippet);
  const initialUrl = docUrl + "#:~:text=first%20phrase";
  let scrolled = waitForEvent(
    EVENT_SCROLLING_START,
    event =>
      event.accessible.role == ROLE_TEXT_LEAF &&
      getAccessibleDOMNodeID(event.accessible.parent) == "first"
  );
  await BrowserTestUtils.withNewTab(initialUrl, async function (browser) {
    info("Waiting for scroll to first");
    const first = (await scrolled).accessible.parent;
    info("Checking ranges");
    await waitForTextAttrRanges(
      first,
      [
        [4, 16], // "first phrase"
      ],
      fragmentAttrs,
      false
    );
    const second = first.nextSibling;
    await waitForTextAttrRanges(second, [], fragmentAttrs, false);

    info("Navigating to second");
    // The text fragment begins with the text "second", which is the second
    // child of the `second` Accessible.
    scrolled = waitForEvent(EVENT_SCROLLING_START, second.getChildAt(1));
    let rangeCheck = waitForTextAttrRanges(
      second,
      [
        [4, 11], // "second "
        [11, 17], // "phrase"
      ],
      fragmentAttrs,
      true
    );
    await invokeContentTask(browser, [], () => {
      content.location.hash = "#:~:text=second%20phrase";
    });
    await scrolled;
    info("Checking ranges");
    await rangeCheck;
    // XXX DOM should probably remove the highlight from "first phrase" since
    // we've navigated to "second phrase". For now, this test expects the
    // current DOM behaviour: "first" is still highlighted.
    await waitForTextAttrRanges(
      first,
      [
        [4, 16], // "first phrase"
      ],
      fragmentAttrs,
      false
    );
  });
});

/**
 * Test custom highlight mutations.
 */
addAccessibleTask(
  `
${snippet}
<script>
  const firstText = document.getElementById("first").firstChild;
  // Highlight the word "first".
  const range1 = new Range();
  range1.setStart(firstText, 4);
  range1.setEnd(firstText, 9);
  const highlight1 = new Highlight(range1);
  CSS.highlights.set("highlight1", highlight1);
</script>
  `,
  async function testCustomHighlightMutations(browser, docAcc) {
    info("Checking initial highlight");
    const first = findAccessibleChildByID(docAcc, "first");
    ok(
      textAttrRangesMatch(
        first,
        [
          [4, 9], // "first"
        ],
        highlightAttrs
      ),
      "first attr ranges correct"
    );
    const second = findAccessibleChildByID(docAcc, "second");
    ok(
      textAttrRangesMatch(second, [], highlightAttrs),
      "second attr ranges correct"
    );

    info("Adding range2 to highlight1");
    let rangeCheck = waitForTextAttrRanges(
      first,
      [
        [0, 3], // "The "
        [4, 9], // "first"
      ],
      highlightAttrs,
      true
    );
    await invokeContentTask(browser, [], () => {
      content.firstText = content.document.getElementById("first").firstChild;
      // Highlight the word "The".
      content.range2 = new content.Range();
      content.range2.setStart(content.firstText, 0);
      content.range2.setEnd(content.firstText, 3);
      content.highlight1 = content.CSS.highlights.get("highlight1");
      content.highlight1.add(content.range2);
    });
    await rangeCheck;

    info("Adding highlight2");
    rangeCheck = waitForTextAttrRanges(
      first,
      [
        [0, 3], // "The "
        [4, 9], // "first"
        [10, 16], // "phrase"
      ],
      highlightAttrs,
      true
    );
    await invokeContentTask(browser, [], () => {
      // Highlight the word "phrase".
      const range3 = new content.Range();
      range3.setStart(content.firstText, 10);
      range3.setEnd(content.firstText, 16);
      const highlight2 = new content.Highlight(range3);
      content.CSS.highlights.set("highlight2", highlight2);
    });
    await rangeCheck;

    info("Removing range2");
    rangeCheck = waitForTextAttrRanges(
      first,
      [
        [4, 9], // "first"
        [10, 16], // "phrase"
      ],
      highlightAttrs,
      true
    );
    await invokeContentTask(browser, [], () => {
      content.highlight1.delete(content.range2);
    });
    await rangeCheck;

    info("Removing highlight1");
    rangeCheck = waitForTextAttrRanges(
      first,
      [
        [10, 16], // "phrase"
      ],
      highlightAttrs,
      true
    );
    await invokeContentTask(browser, [], () => {
      content.CSS.highlights.delete("highlight1");
    });
    await rangeCheck;
  },
  { chrome: true, topLevel: true }
);

/**
 * Test custom highlight types.
 */
addAccessibleTask(
  `
${snippet}
<script>
  const firstText = document.getElementById("first").firstChild;
  // Highlight the word "The".
  const range1 = new Range();
  range1.setStart(firstText, 0);
  range1.setEnd(firstText, 3);
  const highlight = new Highlight(range1);
  CSS.highlights.set("highlight", highlight);

  // Make the word "first" a spelling error.
  const range2 = new Range();
  range2.setStart(firstText, 4);
  range2.setEnd(firstText, 9);
  const spelling = new Highlight(range2);
  spelling.type = "spelling-error";
  CSS.highlights.set("spelling", spelling);

  // Make the word "phrase" a grammar error.
  const range3 = new Range();
  range3.setStart(firstText, 10);
  range3.setEnd(firstText, 16);
  const grammar = new Highlight(range3);
  grammar.type = "grammar-error";
  CSS.highlights.set("grammar", grammar);
</script>
  `,
  async function testCustomHighlightTypes(browser, docAcc) {
    const first = findAccessibleChildByID(docAcc, "first");
    ok(
      textAttrRangesMatch(
        first,
        [
          [0, 3], // "the"
        ],
        highlightAttrs
      ),
      "first highlight ranges correct"
    );
    ok(
      textAttrRangesMatch(
        first,
        [
          [4, 9], // "first"
        ],
        spellingAttrs
      ),
      "first spelling ranges correct"
    );
    ok(
      textAttrRangesMatch(
        first,
        [
          [10, 16], // "phrase"
        ],
        grammarAttrs
      ),
      "first grammar ranges correct"
    );
    const second = findAccessibleChildByID(docAcc, "second");
    ok(
      textAttrRangesMatch(second, [], highlightAttrs),
      "second highlight ranges correct"
    );
  },
  { chrome: true, topLevel: true }
);

/**
 * Test overlapping custom highlights.
 */
addAccessibleTask(
  `
${snippet}
<script>
  const firstText = document.getElementById("first").firstChild;
  // Make the word "The" both a highlight and a spelling error.
  const range1 = new Range();
  range1.setStart(firstText, 0);
  range1.setEnd(firstText, 3);
  const highlight1 = new Highlight(range1);
  CSS.highlights.set("highlight1", highlight1);
  const spelling = new Highlight(range1);
  spelling.type = "spelling-error";
  CSS.highlights.set("spelling", spelling);

  // Highlight the word "first".
  const range2 = new Range();
  range2.setStart(firstText, 4);
  range2.setEnd(firstText, 9);
  highlight1.add(range2);
  // Make "fir" a spelling error.
  const range3 = new Range();
  range3.setStart(firstText, 4);
  range3.setEnd(firstText, 7);
  spelling.add(range3);
  // Make "rst" a spelling error.
  const range4 = new Range();
  range4.setStart(firstText, 6);
  range4.setEnd(firstText, 9);
  spelling.add(range4);

  // Highlight the word "phrase".
  const range5 = new Range();
  range5.setStart(firstText, 10);
  range5.setEnd(firstText, 16);
  highlight1.add(range5);
  // Make "ras" a spelling error.
  const range6 = new Range();
  range6.setStart(firstText, 12);
  range6.setEnd(firstText, 15);
  spelling.add(range6);

  const secondText = document.querySelector("#second i").firstChild;
  // Highlight the word "second".
  const range7 = new Range();
  range7.setStart(secondText, 0);
  range7.setEnd(secondText, 6);
  highlight1.add(range7);
  // Make "sec" a spelling error.
  const range8 = new Range();
  range8.setStart(secondText, 0);
  range8.setEnd(secondText, 3);
  spelling.add(range8);
  // Make "nd" a spelling error.
  const range9 = new Range();
  range9.setStart(secondText, 4);
  range9.setEnd(secondText, 6);
  spelling.add(range9);

  const phrase2Text = document.querySelector("#second b").firstChild;
  // Highlight the word "phrase".
  const range10 = new Range();
  range10.setStart(phrase2Text, 0);
  range10.setEnd(phrase2Text, 6);
  highlight1.add(range10);
  // Highlight "ras" using a different Highlight.
  const range11 = new Range();
  range11.setStart(phrase2Text, 2);
  range11.setEnd(phrase2Text, 5);
  const highlight2 = new Highlight(range11);
  CSS.highlights.set("highlight2", highlight2);
</script>
  `,
  async function testCustomHighlightOverlapping(browser, docAcc) {
    const first = findAccessibleChildByID(docAcc, "first");
    ok(
      textAttrRangesMatch(
        first,
        [
          [0, 3], // "the"
          [4, 6], // "fi"
          [6, 7], // "r"
          [7, 9], // "st"
          [10, 12], // "ph"
          [12, 15], // "ras"
          [15, 16], // "e"
        ],
        highlightAttrs
      ),
      "first highlight ranges correct"
    );
    ok(
      textAttrRangesMatch(
        first,
        [
          [0, 3], // "the"
          [4, 6], // "fi"
          [6, 7], // "r"
          [7, 9], // "st"
          [12, 15], // "ras"
        ],
        spellingAttrs
      ),
      "first spelling ranges correct"
    );
    const second = findAccessibleChildByID(docAcc, "second");
    ok(
      textAttrRangesMatch(
        second,
        [
          [4, 7], // "sec"
          [7, 8], // "o"
          [8, 10], // "nd"
          [11, 13], // "ph"
          [13, 16], // "ras"
          [16, 17], // "e"
        ],
        highlightAttrs
      ),
      "second highlight ranges correct"
    );
    ok(
      textAttrRangesMatch(
        second,
        [
          [4, 7], // "sec"
          [8, 10], // "nd"
        ],
        spellingAttrs
      ),
      "second spelling ranges correct"
    );
  },
  { chrome: true, topLevel: true }
);
