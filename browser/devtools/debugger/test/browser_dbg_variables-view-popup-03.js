/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the inspected indentifier is highlighted.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.start());
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Inspect variable.
    yield openVarPopup(panel, { line: 15, ch: 12 });

    ok(bubble.contentsShown(),
      "The variable should register as being shown.");
    ok(!bubble._tooltip.isEmpty(),
      "The variable inspection popup isn't empty.");
    ok(bubble._markedText,
      "There's some marked text in the editor.");
    ok(bubble._markedText.clear,
      "The marked text in the editor can be cleared.");

    yield hideVarPopup(panel);

    ok(!bubble.contentsShown(),
      "The variable should register as being hidden.");
    ok(bubble._tooltip.isEmpty(),
      "The variable inspection popup is now empty.");
    ok(!bubble._markedText,
      "The marked text in the editor was removed.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
