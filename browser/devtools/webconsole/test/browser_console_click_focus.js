/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the input field is focused when the console is opened.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testInputFocus, false);
}

function testInputFocus() {
  browser.removeEventListener("DOMContentLoaded", testInputFocus, false);

  openConsole().then((hud) => {
    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "Dolske Digs Bacon",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    }).then(([result]) => {
      let msg = [...result.matched][0];
      let outputItem = msg.querySelector(".body");
      ok(outputItem, "found a logged message");
      let inputNode = hud.jsterm.inputNode;
      ok(inputNode.getAttribute("focused"), "input node is focused, first");

      let lostFocus = () => {
        inputNode.removeEventListener("blur", lostFocus);
        info("input node lost focus");
      }

      inputNode.addEventListener("blur", lostFocus);

      browser.ownerDocument.getElementById("urlbar").click();

      ok(!inputNode.getAttribute("focused"), "input node is not focused");

      EventUtils.sendMouseEvent({type: "click"}, hud.outputNode);

      ok(inputNode.getAttribute("focused"), "input node is focused, second time")

      // test click-drags are not focusing the input element.
      EventUtils.sendMouseEvent({type: "mousedown", clientX: 3, clientY: 4},
        outputItem);
      EventUtils.sendMouseEvent({type: "click", clientX: 15, clientY: 5},
        outputItem);

      executeSoon(() => {
        todo(!inputNode.getAttribute("focused"), "input node is not focused after drag");
        finishTest();
      });
    });
  });
}
