/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the a function call's stack is properly displayed in the UI.
 */

// Force the old debugger UI since it's directly used (see Bug 1301705)
Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", false);
registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

function* ifTestingSupported() {
  let { target, panel } = yield initCanvasDebuggerFrontend(SIMPLE_CANVAS_DEEP_STACK_URL);
  let { window, $, $all, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([recordingFinished, callListPopulated]);

  let callItem = CallsListView.getItemAtIndex(2);
  let locationLink = $(".call-item-location", callItem.target);

  is($(".call-item-stack", callItem.target), null,
    "There should be no stack container available yet for the draw call.");

  let callStackDisplayed = once(window, EVENTS.CALL_STACK_DISPLAYED);
  EventUtils.sendMouseEvent({ type: "mousedown" }, locationLink, window);
  yield callStackDisplayed;

  isnot($(".call-item-stack", callItem.target), null,
    "There should be a stack container available now for the draw call.");
  // We may have more than 4 functions, depending on whether async
  // stacks are available.
  ok($all(".call-item-stack-fn", callItem.target).length >= 4,
     "There should be at least 4 functions on the stack for the draw call.");

  ok($all(".call-item-stack-fn-name", callItem.target)[0].getAttribute("value")
    .includes("C()"),
    "The first function on the stack has the correct name.");
  ok($all(".call-item-stack-fn-name", callItem.target)[1].getAttribute("value")
    .includes("B()"),
    "The second function on the stack has the correct name.");
  ok($all(".call-item-stack-fn-name", callItem.target)[2].getAttribute("value")
    .includes("A()"),
    "The third function on the stack has the correct name.");
  ok($all(".call-item-stack-fn-name", callItem.target)[3].getAttribute("value")
    .includes("drawRect()"),
    "The fourth function on the stack has the correct name.");

  is($all(".call-item-stack-fn-location", callItem.target)[0].getAttribute("value"),
    "doc_simple-canvas-deep-stack.html:26",
    "The first function on the stack has the correct location.");
  is($all(".call-item-stack-fn-location", callItem.target)[1].getAttribute("value"),
    "doc_simple-canvas-deep-stack.html:28",
    "The second function on the stack has the correct location.");
  is($all(".call-item-stack-fn-location", callItem.target)[2].getAttribute("value"),
    "doc_simple-canvas-deep-stack.html:30",
    "The third function on the stack has the correct location.");
  is($all(".call-item-stack-fn-location", callItem.target)[3].getAttribute("value"),
    "doc_simple-canvas-deep-stack.html:35",
    "The fourth function on the stack has the correct location.");

  let jumpedToSource = once(window, EVENTS.SOURCE_SHOWN_IN_JS_DEBUGGER);
  EventUtils.sendMouseEvent({ type: "mousedown" }, $(".call-item-stack-fn-location", callItem.target));
  yield jumpedToSource;

  let toolbox = yield gDevTools.getToolbox(target);
  let { panelWin: { DebuggerView: view } } = toolbox.getPanel("jsdebugger");

  is(view.Sources.selectedValue, getSourceActor(view.Sources, SIMPLE_CANVAS_DEEP_STACK_URL),
    "The expected source was shown in the debugger.");
  is(view.editor.getCursor().line, 25,
    "The expected source line is highlighted in the debugger.");

  yield teardown(panel);
  finish();
}
