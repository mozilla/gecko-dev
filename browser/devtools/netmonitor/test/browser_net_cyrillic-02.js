/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if cyrillic text is rendered correctly in the source editor
 * when loaded directly from an HTML page.
 */

function test() {
  initNetMonitor(CYRILLIC_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, Editor, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 1).then(() => {
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
        "GET", CYRILLIC_URL, {
          status: 200,
          statusText: "OK"
        });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.getElementById("details-pane-toggle"));
      EventUtils.sendMouseEvent({ type: "mousedown" },
        document.querySelectorAll("#details-pane tab")[3]);

      let RESPONSE_BODY_DISPLAYED = aMonitor.panelWin.EVENTS.RESPONSE_BODY_DISPLAYED;
      waitFor(aMonitor.panelWin, RESPONSE_BODY_DISPLAYED).then(() =>
        NetMonitorView.editor("#response-content-textarea")
      ).then((aEditor) => {
        is(aEditor.getText().indexOf("\u044F"), 486, // я
          "The text shown in the source editor is incorrect.");
        is(aEditor.getMode(), Editor.modes.html,
          "The mode active in the source editor is incorrect.");

        teardown(aMonitor).then(finish);
      });
    });

    aDebuggee.location.reload();
  });
}
