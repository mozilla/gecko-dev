/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that the tooltip previews are correct with wrapped editor lines.

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

httpServer.registerPathHandler(
  "/doc-wrapped-lines.html",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html>
    <html>
      <head>
        <script type="text/javascript">
        const cs1 = getComputedStyle(document.documentElement);
        const cs2 = getComputedStyle(document.documentElement);
        // This line generates a very long inline-preview which is loaded a bit later after the
        // initial positions for the page has been calculated
        function add(a,b,k){var result=a+b;return k(result)}function sub(a,b,k){var result=a-b;return k(result)}function mul(a,b,k){var result=a*b;return k(result)}function div(a,b,k){var result=a/b;return k(result)}function arithmetic(){
  add(4,4,function(a){
    sub(a,2,function(b){mul(b,3,function(c){div(c,2,function(d){console.log("arithmetic",d)})})})})};
        isNaN(cs1, cs2);
          const foo = { prop: 0 };
          const bar = Math.min(foo);
          const myVeryLongVariableNameThatMayWrap = 42;
          myVeryLongVariableNameThatMayWrap * 2;
          debugger;
        </script>
      </head>
    </html>
  `);
  }
);

const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}/`;

add_task(async function () {
  await pushPref("devtools.toolbox.footer.height", 500);
  await pushPref("devtools.debugger.ui.editor-wrapping", true);

  // Reset toolbox height and end panel size to avoid impacting other tests
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("debugger.end-panel-size");
  });

  const dbg = await initDebuggerWithAbsoluteURL(
    `${BASE_URL}doc-wrapped-lines.html`
  );
  await waitForSources(dbg, "doc-wrapped-lines.html");

  const onReloaded = reload(dbg);
  await waitForPaused(dbg);

  await assertPreviews(dbg, [
    {
      line: 13,
      column: 18,
      expression: "foo",
      fields: [["prop", "0"]],
    },
    {
      line: 14,
      column: 18,
      result: "NaN",
      expression: "bar",
    },
  ]);

  info(
    "Resize the editor until the content on line 6 => `        const cs2 = getComputedStyle(document.documentElement);` wraps"
  );
  // Use splitter to resize to make sure CodeMirror internal state is refreshed
  // in such case (CodeMirror already handle window resize on its own).
  const splitter = dbg.win.document.querySelectorAll(".splitter")[1];
  const splitterOriginalX = splitter.getBoundingClientRect().left;
  ok(splitter, "Got the splitter");

  const lineEl = findElement(dbg, "line", 6);

  const lineHeightBeforeWrap = getElementBoxQuadHeight(lineEl);
  let lineHeightAfterWrap = 0;

  info("Resize the editor width to one third of the size of the line element");
  const lineElBoundingClientRect = lineEl.getBoundingClientRect();
  await resizeSplitter(
    dbg,
    splitter,
    lineElBoundingClientRect.left + lineElBoundingClientRect.width / 3
  );

  info("Wait until the line does wrap");
  await waitFor(async () => {
    const el = findElement(dbg, "line", 6);
    lineHeightAfterWrap = getElementBoxQuadHeight(el);
    return lineHeightAfterWrap > lineHeightBeforeWrap;
  });

  info("Assert that the line wrapped");
  const EXPECTED_LINES_TO_WRAP_OVER = 3;
  is(
    Math.floor(lineHeightAfterWrap),
    Math.floor(lineHeightBeforeWrap * EXPECTED_LINES_TO_WRAP_OVER),
    "The content on line 6 to wrap over 3 lines"
  );

  info("Assert the previews still work with wrapping");
  await assertPreviews(dbg, [
    {
      line: 16,
      column: 13,
      expression: "myVeryLongVariableNameThatMayWrap",
      result: "42",
    },
  ]);

  // clearing the pref isn't enough to have consistent sizes between runs,
  // so set it back to its original position
  await resizeSplitter(dbg, splitter, splitterOriginalX);

  await resume(dbg);
  await onReloaded;

  Services.prefs.clearUserPref("debugger.end-panel-size");
  await wait(1000);
});

async function resizeSplitter(dbg, splitterEl, x) {
  EventUtils.synthesizeMouse(splitterEl, 0, 0, { type: "mousedown" }, dbg.win);
  // Move the splitter of the secondary pane to the middle of the token,
  // this should cause the token to wrap.
  EventUtils.synthesizeMouseAtPoint(
    x,
    splitterEl.getBoundingClientRect().top + 10,
    { type: "mousemove" },
    dbg.win
  );

  // Stop dragging
  EventUtils.synthesizeMouseAtCenter(splitterEl, { type: "mouseup" }, dbg.win);
}

// Calculates the height of the element for the line
function getElementBoxQuadHeight(lineEl) {
  const boxQ = lineEl.getBoxQuads()[0];
  return boxQ.p4.y - boxQ.p1.y;
}
