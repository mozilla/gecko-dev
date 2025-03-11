/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

// Test for Bug 835896
// WalkerSearch specific tests.  This is to make sure search results are
// coming back as expected.
// See also test_inspector-search-front.html.

add_task(async function () {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-search-data.html"
  );

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[walker.actorID]],
    async function (actorID) {
      const { require } = ChromeUtils.importESModule(
        "resource://devtools/shared/loader/Loader.sys.mjs"
      );
      const {
        DevToolsServer,
      } = require("resource://devtools/server/devtools-server.js");
      const {
        DocumentWalker: _documentWalker,
      } = require("resource://devtools/server/actors/inspector/document-walker.js");

      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      actorID = String(actorID);
      const walkerActor = DevToolsServer.searchAllConnectionsForActor(actorID);
      const walkerSearch = walkerActor.walkerSearch;
      const {
        WalkerSearch,
        WalkerIndex,
      } = require("resource://devtools/server/actors/utils/walker-search.js");

      info("Testing basic index APIs exist.");
      const index = new WalkerIndex(walkerActor);
      Assert.greater(
        index.data.size,
        0,
        "public index is filled after getting"
      );

      index.clearIndex();
      ok(!index._data, "private index is empty after clearing");
      Assert.greater(
        index.data.size,
        0,
        "public index is filled after getting"
      );

      index.destroy();

      info("Testing basic search APIs exist.");

      ok(walkerSearch, "walker search exists on the WalkerActor");
      ok(walkerSearch.search, "walker search has `search` method");
      ok(walkerSearch.index, "walker search has `index` property");
      is(
        walkerSearch.walker,
        walkerActor,
        "referencing the correct WalkerActor"
      );

      const walkerSearch2 = new WalkerSearch(walkerActor);
      ok(walkerSearch2, "a new search instance can be created");
      ok(walkerSearch2.search, "new search instance has `search` method");
      ok(walkerSearch2.index, "new search instance has `index` property");
      isnot(
        walkerSearch2,
        walkerSearch,
        "new search instance differs from the WalkerActor's"
      );

      walkerSearch2.destroy();

      info("Testing search with an empty query.");
      let results = walkerSearch.search("");
      is(results.length, 0, "No results when searching for ''");

      results = walkerSearch.search(null);
      is(results.length, 0, "No results when searching for null");

      results = walkerSearch.search(undefined);
      is(results.length, 0, "No results when searching for undefined");

      results = walkerSearch.search(10);
      is(results.length, 0, "No results when searching for 10");

      const inspectee = content.document;
      const testData = [
        {
          desc: "Search for tag with one result.",
          search: "body",
          expected: [{ node: inspectee.body, type: "tag" }],
        },
        {
          desc: "Search for tag with multiple results",
          search: "h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "tag" },
            { node: inspectee.querySelectorAll("h2")[1], type: "tag" },
            { node: inspectee.querySelectorAll("h2")[2], type: "tag" },
          ],
        },
        {
          desc: "Search for selector with multiple results",
          search: "body > h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[1], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[2], type: "selector" },
          ],
        },
        {
          desc: "Search for selector with multiple results",
          search: ":root h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[1], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[2], type: "selector" },
          ],
        },
        {
          desc: "Search for selector with multiple results",
          search: "* h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[1], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[2], type: "selector" },
          ],
        },
        {
          desc: "Search with multiple matches in a single tag expecting a single result",
          search: "💩",
          expected: [
            { node: inspectee.getElementById("💩"), type: "attributeValue" },
          ],
        },
        {
          desc: "Search for attributeName=attributeValue pairs without quotation marks",
          search: "id=arrows",
          expected: [
            { node: inspectee.getElementById("arrows"), type: "attributeName" },
          ],
        },
        {
          desc: "Search for attributeName=attributeValue pairs with quotation marks",
          search: 'id="arrows"',
          expected: [
            { node: inspectee.getElementById("arrows"), type: "attributeName" },
          ],
        },
        {
          desc: "Search for attributeName=attributeValue pairs with partial quotation marks",
          search: 'id="arr',
          expected: [
            { node: inspectee.getElementById("arrows"), type: "attributeName" },
          ],
        },
        {
          desc: `Search for unmatched attributeName="attr"`,
          search: 'id="arr"',
          expected: [],
        },
        {
          desc: "Search for attributeName=",
          search: "id=",
          expected: [
            { node: inspectee.getElementById("pseudo"), type: "attributeName" },
            { node: inspectee.getElementById("arrows"), type: "attributeName" },
            { node: inspectee.getElementById("💩"), type: "attributeName" },
          ],
        },
        {
          desc: "Search for =attributeValue",
          search: "=arr",
          expected: [
            {
              node: inspectee.getElementById("arrows"),
              type: "attributeValue",
            },
          ],
        },
        {
          desc: `Search for ="attributeValue`,
          search: `="arr`,
          expected: [
            {
              node: inspectee.getElementById("arrows"),
              type: "attributeValue",
            },
          ],
        },
        {
          desc: `Search for ="attributeValue"`,
          search: `="arrows"`,
          expected: [
            {
              node: inspectee.getElementById("arrows"),
              type: "attributeValue",
            },
          ],
        },
        {
          desc: `Search for unmatched ="attributeValue"`,
          search: `="arr"`,
          expected: [],
        },
        {
          desc: "Search that has tag and text results",
          search: "h1",
          expected: [
            { node: inspectee.querySelector("h1"), type: "tag" },
            {
              node: inspectee.querySelector("h1 + p").childNodes[0],
              type: "text",
            },
            {
              node: inspectee.querySelector("h1 + p > strong").childNodes[0],
              type: "text",
            },
          ],
        },
        {
          desc: "Search for XPath with one result",
          search: "//strong",
          expected: [
            { node: inspectee.querySelector("strong"), type: "xpath" },
          ],
        },
        {
          desc: "Search for XPath with multiple results",
          search: "//h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "xpath" },
            { node: inspectee.querySelectorAll("h2")[1], type: "xpath" },
            { node: inspectee.querySelectorAll("h2")[2], type: "xpath" },
          ],
        },
        {
          desc: "Search for XPath via containing text",
          search: "//*[contains(text(), 'p tag')]",
          expected: [{ node: inspectee.querySelector("p"), type: "xpath" }],
        },
        {
          desc: "Search for XPath via strict equal text",
          search: "//*[text()='Heading 1']",
          expected: [
            { node: inspectee.querySelector("h1#pseudo"), type: "xpath" },
          ],
        },
        {
          desc: "Search for XPath matching text node",
          search: "//strong/text()",
          expected: [
            {
              node: inspectee.querySelector("strong").firstChild,
              type: "xpath",
            },
          ],
        },
        {
          desc: "Search using XPath grouping expression",
          search: "(//*)[2]",
          expected: [{ node: inspectee.querySelector("head"), type: "xpath" }],
        },
        {
          desc: "Search using XPath function",
          search: "id('arrows')",
          expected: [
            { node: inspectee.querySelector("#arrows"), type: "xpath" },
          ],
        },
      ];

      const assertSearchResults = (searchResults, expectedResults, msg) => {
        is(
          searchResults.length,
          expectedResults.length,
          `${msg} - got expected number of results`
        );
        if (searchResults.length === expectedResults.length) {
          searchResults.forEach((result, i) => {
            const { type, node } = expectedResults[i];
            is(result.type, type, `${msg} - result #${i} type`);
            if (result.node != node) {
              const displayNode = el => {
                return `<${el.nodeName.toLowerCase()}${el.id ? "#" + el.id : ""}>`;
              };
              ok(
                false,
                `${msg} - result #${i} unexpected node: Got ${displayNode(result.node)}, expected ${displayNode(node)}`
              );
            }
          });
        }
      };

      for (const { desc, search, expected } of testData) {
        info("Running test: " + desc);
        results = walkerSearch.search(search);
        assertSearchResults(
          results,
          expected,
          "Search returns correct results with '" + search + "'"
        );
      }

      info("Testing ::before and ::after element matching");

      const beforeElt = new _documentWalker(
        inspectee.querySelector("#pseudo"),
        inspectee.defaultView
      ).firstChild();
      const afterElt = new _documentWalker(
        inspectee.querySelector("#pseudo"),
        inspectee.defaultView
      ).lastChild();
      const styleText = inspectee.querySelector("style").childNodes[0];

      // ::before
      results = walkerSearch.search("::before");
      assertSearchResults(
        results,
        [{ node: beforeElt, type: "tag" }],
        "Tag search works for pseudo element"
      );

      results = walkerSearch.search("_moz_generated_content_before");
      is(results.length, 0, "No results for anon tag name");

      results = walkerSearch.search("before element");
      assertSearchResults(
        results,
        [
          { node: styleText, type: "text" },
          { node: beforeElt, type: "text" },
        ],
        "Text search works for pseudo element"
      );

      // ::after
      results = walkerSearch.search("::after");
      assertSearchResults(
        results,
        [{ node: afterElt, type: "tag" }],
        "Tag search works for pseudo element"
      );

      results = walkerSearch.search("_moz_generated_content_after");
      is(results.length, 0, "No results for anon tag name");

      results = walkerSearch.search("after element");
      assertSearchResults(
        results,
        [
          { node: styleText, type: "text" },
          { node: afterElt, type: "text" },
        ],
        "Text search works for pseudo element"
      );

      info("Testing search before and after a mutation.");
      const expected = [
        { node: inspectee.querySelectorAll("h3")[0], type: "tag" },
        { node: inspectee.querySelectorAll("h3")[1], type: "tag" },
        { node: inspectee.querySelectorAll("h3")[2], type: "tag" },
      ];

      results = walkerSearch.search("h3");
      assertSearchResults(results, expected, "Search works with tag results");

      function mutateDocumentAndWaitForMutation(mutationFn) {
        // eslint-disable-next-line new-cap
        return new Promise(resolve => {
          info("Listening to markup mutation on the inspectee");
          const observer = new inspectee.defaultView.MutationObserver(resolve);
          observer.observe(inspectee, { childList: true, subtree: true });
          mutationFn();
        });
      }
      await mutateDocumentAndWaitForMutation(() => {
        expected[0].node.remove();
      });

      results = walkerSearch.search("h3");
      assertSearchResults(
        results,
        [expected[1], expected[2]],
        "Results are updated after removal"
      );

      // eslint-disable-next-line new-cap
      await new Promise(resolve => {
        info("Waiting for a mutation to happen");
        const observer = new inspectee.defaultView.MutationObserver(() => {
          resolve();
        });
        observer.observe(inspectee, { attributes: true, subtree: true });
        inspectee.body.setAttribute("h3", "true");
      });

      results = walkerSearch.search("h3");
      assertSearchResults(
        results,
        [
          { node: inspectee.body, type: "attributeName" },
          expected[1],
          expected[2],
        ],
        "Results are updated after addition"
      );

      // eslint-disable-next-line new-cap
      await new Promise(resolve => {
        info("Waiting for a mutation to happen");
        const observer = new inspectee.defaultView.MutationObserver(() => {
          resolve();
        });
        observer.observe(inspectee, {
          attributes: true,
          childList: true,
          subtree: true,
        });
        inspectee.body.removeAttribute("h3");
        expected[1].node.remove();
        expected[2].node.remove();
      });

      results = walkerSearch.search("h3");
      is(results.length, 0, "Results are updated after removal");
    }
  );
});
