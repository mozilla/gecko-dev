/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/text.js */
/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

const boldAttrs = { "font-weight": "700" };
const fragmentAttrs = { mark: "true" };
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
