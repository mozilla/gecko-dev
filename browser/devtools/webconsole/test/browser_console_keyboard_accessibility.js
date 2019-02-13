/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that basic keyboard shortcuts work in the web console.

"use strict";

let test = asyncTest(function*() {
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  ok(hud, "Web Console opened");

  info("dump some spew into the console for scrolling");
  hud.jsterm.execute("(function() { for (var i = 0; i < 100; i++) { " +
                     "console.log('foobarz' + i);" +
                     "}})();");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarz99",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  let currentPosition = hud.outputNode.parentNode.scrollTop;
  let bottom = currentPosition;

  EventUtils.synthesizeKey("VK_PAGE_UP", {});
  isnot(hud.outputNode.parentNode.scrollTop, currentPosition, "scroll position changed after page up");

  currentPosition = hud.outputNode.parentNode.scrollTop;
  EventUtils.synthesizeKey("VK_PAGE_DOWN", {});
  ok(hud.outputNode.parentNode.scrollTop > currentPosition, "scroll position now at bottom");

  EventUtils.synthesizeKey("VK_HOME", {});
  is(hud.outputNode.parentNode.scrollTop, 0, "scroll position now at top");

  EventUtils.synthesizeKey("VK_END", {});

  let scrollTop = hud.outputNode.parentNode.scrollTop;
  ok(scrollTop > 0 && Math.abs(scrollTop - bottom) <= 5,
     "scroll position now at bottom");

  info("try ctrl-l to clear output");
  executeSoon(() => { EventUtils.synthesizeKey("l", { ctrlKey: true }); });
  yield hud.jsterm.once("messages-cleared");

  is(hud.outputNode.textContent.indexOf("foobarz1"), -1, "output cleared");
  is(hud.jsterm.inputNode.getAttribute("focused"), "true",
     "jsterm input is focused");

  info("try ctrl-f to focus filter");
  EventUtils.synthesizeKey("F", { accelKey: true });
  ok(!hud.jsterm.inputNode.getAttribute("focused"),
     "jsterm input is not focused");
  is(hud.ui.filterBox.getAttribute("focused"), "true",
     "filter input is focused");

  if (Services.appinfo.OS == "Darwin") {
    ok(hud.ui.getFilterState("network"), "network category is enabled");
    EventUtils.synthesizeKey("t", { ctrlKey: true });
    ok(!hud.ui.getFilterState("network"), "accesskey for Network works");
    EventUtils.synthesizeKey("t", { ctrlKey: true });
    ok(hud.ui.getFilterState("network"), "accesskey for Network works (again)");
  }
  else {
    EventUtils.synthesizeKey("N", { altKey: true });
    let net = hud.ui.document.querySelector("toolbarbutton[category=net]");
    is(hud.ui.document.activeElement, net,
       "accesskey for Network category focuses the Net button");
  }
});
