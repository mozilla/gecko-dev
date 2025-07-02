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
});

async function searchFor(selector, inspector) {
  const onNewNodeFront = inspector.selection.once("new-node-front");

  searchUsingSelectorSearch(selector, inspector);

  await onNewNodeFront;
  await inspector.once("inspector-updated");
}

function checkHighlightedSearchResults(inspector, expectedHighlights) {
  Assert.deepEqual(
    [
      ...inspector.markup.win.CSS.highlights
        .get(DEVTOOLS_SEARCH_HIGHLIGHT_NAME)
        .values(),
    ].map(range => range.toString()),
    expectedHighlights
  );
}
