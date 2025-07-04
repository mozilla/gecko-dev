/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests the search bar retains previous query on re-opening.
add_task(async function testSearchRetainsPreviousQuery() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const {
    selectors: { getActiveSearch },
  } = dbg;
  await selectSource(dbg, "simple1.js");

  // Open search bar
  pressKey(dbg, "fileSearch");
  await waitFor(() => getActiveSearch() === "file");
  is(getActiveSearch(), "file");

  // Type a search query
  type(dbg, "con");
  await waitForSearchState(dbg);
  is(findElement(dbg, "fileSearchInput").value, "con");

  // Close the search bar
  pressKey(dbg, "Escape");
  await waitFor(() => getActiveSearch() === null);
  is(getActiveSearch(), null);

  // Re-open search bar
  pressKey(dbg, "fileSearch");
  await waitFor(() => getActiveSearch() === "file");
  is(getActiveSearch(), "file");

  // Test for the retained query
  is(findElement(dbg, "fileSearchInput").value, "con");
});

// Tests that the search results are updated when switching between sources
add_task(async function testSearchUpdatesResultsOnSourceChanges() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js", "long.js");
  const {
    selectors: { getActiveSearch },
  } = dbg;

  await selectSource(dbg, "simple1.js");

  // Open search bar
  pressKey(dbg, "fileSearch");
  await waitFor(() => getActiveSearch() === "file");
  is(getActiveSearch(), "file");

  // Type a search query
  const searchQuery = "this";
  type(dbg, searchQuery);
  await waitForSearchState(dbg);

  await waitUntil(
    () => findElement(dbg, "fileSearchSummary").innerText !== "No results found"
  );

  is(
    findElement(dbg, "fileSearchSummary").innerText,
    "1 of 3 results",
    `There are 3 results found for search query "${searchQuery}" in simple1.js`
  );
  is(
    findElement(dbg, "fileSearchInput").value,
    searchQuery,
    `The search input value matches the search query "${searchQuery}"`
  );

  info("Switch to long.js");
  await selectSource(dbg, "long.js");

  await waitUntil(
    () => findElement(dbg, "fileSearchSummary")?.innerText == "1 of 23 results"
  );
  ok(
    true,
    `There are 23 results found for search query "${searchQuery}" in long.js`
  );
  is(
    findElement(dbg, "fileSearchInput").value,
    searchQuery,
    `The search input value still matches the search query "${searchQuery}"`
  );
});
