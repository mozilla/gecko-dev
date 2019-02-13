/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// A for-of loop in Web Console code can loop over a content NodeList.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-for-of.html";

let test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  yield testForOf(hud);
});

function testForOf(hud) {
  let deferred = promise.defer();

  var jsterm = hud.jsterm;
  jsterm.execute("{ [x.tagName for (x of document.body.childNodes) if (x.nodeType === 1)].join(' '); }",
    (node) => {
      ok(/H1 DIV H2 P/.test(node.textContent),
        "for-of loop should find all top-level nodes");
      deferred.resolve();
    });

  return deferred.promise;
}
