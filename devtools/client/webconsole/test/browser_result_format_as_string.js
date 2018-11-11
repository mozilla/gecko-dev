/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure that JS eval result are properly formatted as strings.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-result-format-as-string.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput(true);

  let msg = yield execute(hud, "document.querySelector('p')");

  is(hud.outputNode.textContent.indexOf("bug772506_content"), -1,
     "no content element found");
  ok(!hud.outputNode.querySelector("#foobar"), "no #foobar element found");

  ok(msg, "eval output node found");
  is(msg.textContent.indexOf("<div>"), -1,
     "<div> string is not displayed");
  isnot(msg.textContent.indexOf("<p>"), -1,
        "<p> string is displayed");

  EventUtils.synthesizeMouseAtCenter(msg, {type: "mousemove"});
  ok(!gBrowser._bug772506, "no content variable");
});

function execute(hud, str) {
  let deferred = promise.defer();
  hud.jsterm.execute(str, deferred.resolve);
  return deferred.promise;
}
