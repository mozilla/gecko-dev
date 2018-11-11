/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that event listeners are properly fetched even if one of the listeners
 * don't have a Debugger.Source object (bug 942899).
 *
 * This test is skipped on debug and e10s builds for following reasons:
 *  - debug: requiring sdk/tabs causes memory leaks when new windows are opened
 *    in tests executed after this one. Bug 1142597.
 *  - e10s: tab.attach is not e10s safe and only works when add-on compatibility
 *    shims are in place. Bug 1146603.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-01.html";

function test() {
  Task.spawn(function* () {
    let tab = yield addTab(TAB_URL);

    // Create a sandboxed content script the Add-on SDK way. Inspired by bug
    // 1145996.
    let tabs = require("sdk/tabs");
    let sdkTab = [...tabs].find(tab => tab.url === TAB_URL);
    ok(sdkTab, "Add-on SDK found the loaded tab.");

    info("Attaching an event handler via add-on sdk content scripts.");
    let worker = sdkTab.attach({
      contentScript: "document.body.addEventListener('click', e => alert(e))",
      onError: ok.bind(this, false)
    });

    let options = {
      source: TAB_URL,
      line: 1
    };
    let [,, panel, win] = yield initDebugger(tab, options);
    let dbg = panel.panelWin;
    let controller = dbg.DebuggerController;
    let constants = dbg.require("./content/constants");
    let actions = dbg.require("./content/actions/event-listeners");
    let fetched = waitForDispatch(panel, constants.FETCH_EVENT_LISTENERS);

    info("Scheduling event listener fetch.");
    controller.dispatch(actions.fetchEventListeners());

    info("Waiting for updated event listeners to arrive.");
    yield fetched;

    ok(true, "The listener update did not hang.");
    closeDebuggerAndFinish(panel);
  });
}
