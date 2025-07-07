/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that searching for nodes using the selector-search input expands and
// selects the right nodes in the markup-view, even when those nodes are deeply
// nested (and therefore not attached yet when the markup-view is initialized).

const TEST_URL = URL_ROOT + "doc_markup_search.html";
const DEVTOOLS_SEARCH_HIGHLIGHT_NAME = "devtools-search";

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  let container = await getContainerForSelector("em", inspector, true);
  ok(!container, "The <em> tag isn't present yet in the markup-view");

  // Searching for the innermost element first makes sure that the inspector
  // back-end is able to attach the resulting node to the tree it knows at the
  // moment. When the inspector is started, the <body> is the default selected
  // node, and only the parents up to the ROOT are known, and its direct
  // children.
  info("searching for the innermost child: <em>");
  await searchFor("em", inspector);

  container = await getContainerForSelector("em", inspector);
  ok(container, "The <em> tag is now imported in the markup-view");

  let nodeFront = await getNodeFront("em", inspector);
  is(
    inspector.selection.nodeFront,
    nodeFront,
    "The <em> tag is the currently selected node"
  );
  ok(
    inspector.markup.win.CSS.highlights.has(DEVTOOLS_SEARCH_HIGHLIGHT_NAME),
    `"${DEVTOOLS_SEARCH_HIGHLIGHT_NAME}" CSS highlight does exist`
  );

  checkHighlightedSearchResults(inspector, [
    // Opening tag
    "em",
    // Closing tag
    "em",
  ]);

  info("searching for other nodes too");
  for (const node of ["span", "li", "ul"]) {
    await searchFor(node, inspector);

    nodeFront = await getNodeFront(node, inspector);
    is(
      inspector.selection.nodeFront,
      nodeFront,
      "The <" + node + "> tag is the currently selected node"
    );
    // We still get 2 Ranges on those items: even if only the opening tag is visible (because
    // the elements are expanded to show their children), a closing tag is actually
    // rendered and hidden in CSS.
    checkHighlightedSearchResults(inspector, [node, node]);
  }

  await searchFor("BUTT", inspector);
  is(
    inspector.selection.nodeFront,
    await getNodeFront(".Buttons", inspector),
    "The section.Buttons element is selected"
  );
  // Selected node markup: <section class="Buttons">
  checkHighlightedSearchResults(inspector, ["Butt"]);

  await searchFor("BUT", inspector);
  is(
    inspector.selection.nodeFront,
    await getNodeFront(".Buttons", inspector),
    "The section.Buttons element is selected"
  );
  checkHighlightedSearchResults(inspector, ["But"]);

  let onSearchResult = inspector.search.once("search-result");
  inspector.searchNextButton.click();
  info("Waiting for results");
  await onSearchResult;

  is(
    inspector.selection.nodeFront,
    await getNodeFront(`button[type="button"]`, inspector),
    `The button[type="button"] element is selected`
  );
  // Selected node markup: <button type="button" class="Button">OK</button>
  checkHighlightedSearchResults(inspector, [
    // opening tag (`<button`)
    "but",
    // class attribute (`class="Button"`)
    // Attributes are re-ordered in the markup view, that's wy this is coming before
    // the result for the type attribute.
    "But",
    // type attribute (`type="button"`)
    "but",
    // closing tag (`</button`)
    "but",
  ]);

  onSearchResult = inspector.search.once("search-result");
  inspector.searchNextButton.click();
  info("Waiting for results");
  await onSearchResult;

  is(
    inspector.selection.nodeFront,
    await getNodeFront(`section.Buttons > p`, inspector),
    `The p element is selected`
  );
  // Selected node markup: <p>Click the button</p>
  checkHighlightedSearchResults(inspector, ["but"]);

  const onSearchCleared = inspector.once("search-cleared");
  inspector.searchClearButton.click();
  info("Waiting for search to clear");
  await onSearchCleared;

  checkHighlightedSearchResults(inspector, []);

  await searchFor("TALLTOPMATCH", inspector);
  const talltopNodeFront = await getNodeFront("section.talltop", inspector);
  const talltopNodeFrontChildren =
    await inspector.walker.children(talltopNodeFront);
  is(
    inspector.selection.nodeFront,
    talltopNodeFrontChildren.nodes[0],
    `The section.talltop text node is selected`
  );
  checkHighlightedSearchResults(inspector, ["TALLTOPMATCH"]);

  await searchFor("TALLBOTTOMMATCH", inspector);
  const tallbottomNodeFront = await getNodeFront(
    "section.tallbottom",
    inspector
  );
  const tallbottomNodeFrontChildren =
    await inspector.walker.children(tallbottomNodeFront);
  is(
    inspector.selection.nodeFront,
    tallbottomNodeFrontChildren.nodes[0],
    `The section.tallbottom text node is selected`
  );
  checkHighlightedSearchResults(inspector, ["TALLBOTTOMMATCH"]);

  await searchFor("OVERFLOWSMATCH", inspector);
  const overflowsNodeFront = await getNodeFront("section.overflows", inspector);
  const overflowsNodeFrontChildren =
    await inspector.walker.children(overflowsNodeFront);
  is(
    inspector.selection.nodeFront,
    overflowsNodeFrontChildren.nodes[0],
    "The section.overflows text node is selected"
  );
  checkHighlightedSearchResults(inspector, ["OVERFLOWSMATCH"]);

  info(
    "Check that matching node with non-visible search result are still being scrolled to"
  );
  // Scroll to top to make sure the node isn't in view at first
  const markupViewContainer = inspector.markup.win.document.documentElement;
  markupViewContainer.scrollTop = 0;
  markupViewContainer.scrollLeft = 0;

  const croppedAttributeContainer = await getContainerForSelector(
    "section#cropped-attribute",
    inspector
  );
  let croppedAttributeContainerRect =
    croppedAttributeContainer.elt.getBoundingClientRect();

  ok(
    croppedAttributeContainerRect.y < 0 ||
      croppedAttributeContainerRect.y > markupViewContainer.clientHeight,
    "section#cropped-attribute container is not into view before searching for a match in its attributes"
  );

  await searchFor("croppedvalue", inspector);
  is(
    inspector.selection.nodeFront,
    await getNodeFront("section#cropped-attribute", inspector),
    "The section#cropped-attribute element is selected"
  );
  checkHighlightedSearchResults(inspector, []);
  // Check that node visible after it was selected
  croppedAttributeContainerRect =
    croppedAttributeContainer.elt.getBoundingClientRect();

  Assert.greaterOrEqual(
    croppedAttributeContainerRect.y,
    0,
    `Node with cropped attributes is not above visible viewport`
  );
  Assert.less(
    croppedAttributeContainerRect.y,
    markupViewContainer.clientHeight,
    `Node with cropped attributes is not below visible viewport`
  );

  // Sanity check to make sure the markup view does overflow in both axes. We need to
  // wait after the search is done as their text node is only revealed when cycling through
  // search results.
  Assert.greater(
    markupViewContainer.scrollHeight,
    markupViewContainer.clientHeight,
    "Markup view overflows vertically"
  );
  Assert.greater(
    markupViewContainer.scrollWidth,
    markupViewContainer.clientWidth,
    "Markup view overflows horizontally"
  );
});

async function searchFor(selector, inspector) {
  const onNewNodeFront = inspector.selection.once("new-node-front");

  searchUsingSelectorSearch(selector, inspector);

  await onNewNodeFront;
  await inspector.once("inspector-updated");
}

function checkHighlightedSearchResults(inspector, expectedHighlights) {
  const searchInputValue = getSelectorSearchBox(inspector).value;

  info(`Checking highlights for "${searchInputValue}" search`);
  const devtoolsHighlights = [
    ...inspector.markup.win.CSS.highlights
      .get(DEVTOOLS_SEARCH_HIGHLIGHT_NAME)
      .values(),
  ];
  Assert.deepEqual(
    devtoolsHighlights.map(range => range.toString()),
    expectedHighlights,
    `Got expected highlights for "${searchInputValue}"`
  );

  if (expectedHighlights.length) {
    const markupViewContainer = inspector.markup.win.document.documentElement;
    info(
      `Check that we scrolled so the first highlighted range for "${searchInputValue}" is visible`
    );
    const [rect] = devtoolsHighlights[0].getClientRects();
    const { x, y } = rect;

    Assert.greaterOrEqual(
      y,
      0,
      `First "${searchInputValue}" match not above visible viewport`
    );
    Assert.less(
      y,
      markupViewContainer.clientHeight,
      `First "${searchInputValue}" match not below visible viewport`
    );
    Assert.greaterOrEqual(
      x,
      0,
      `First "${searchInputValue}" match not before the "left border" of the visible viewport`
    );
    Assert.less(
      x,
      markupViewContainer.clientWidth,
      `First "${searchInputValue}" match not after the "right border" of the visible viewport`
    );
  }
}
