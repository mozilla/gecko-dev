/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PromptListener } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/listeners/PromptListener.sys.mjs"
);

const BUILDER_URL = "https://example.com/document-builder.sjs?html=";

const BEFOREUNLOAD_MARKUP = `
<html>
<head>
  <script>
    window.onbeforeunload = function() {
      return true;
    };
  </script>
</head>
<body>TEST PAGE</body>
</html>
`;
const BEFOREUNLOAD_URL = BUILDER_URL + encodeURI(BEFOREUNLOAD_MARKUP);

add_task(async function test_without_curBrowser() {
  const listener = new PromptListener();
  const opened = listener.once("opened");
  const closed = listener.once("closed");

  listener.startListening();

  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.confirm('test'))`);
  const dialogWin = await dialogPromise;

  const openedEvent = await opened;

  is(openedEvent.prompt.window, dialogWin, "Received expected prompt");

  dialogWin.document.querySelector("dialog").acceptDialog();

  const closedEvent = await closed;

  is(closedEvent.detail.accepted, true, "Received correct event details");

  listener.destroy();
});

add_task(async function test_with_curBrowser() {
  const listener = new PromptListener(() => ({
    contentBrowser: gBrowser.selectedBrowser,
    window,
  }));
  const opened = listener.once("opened");
  const closed = listener.once("closed");

  listener.startListening();

  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.confirm('test'))`);
  const dialogWin = await dialogPromise;

  const openedEvent = await opened;

  is(openedEvent.prompt.window, dialogWin, "Received expected prompt");

  dialogWin.document.querySelector("dialog").acceptDialog();

  const closedEvent = await closed;

  is(closedEvent.detail.accepted, true, "Received correct event details");

  listener.destroy();
});

add_task(async function test_close_event_details() {
  let closed;
  const listener = new PromptListener();

  listener.startListening();

  for (const promptType of ["alert", "confirm", "prompt"]) {
    for (const accept of [false, true]) {
      info(`Test prompt type "${promptType}" with accept=${accept}`);

      closed = listener.once("closed");

      let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
      await createScriptNode(`setTimeout(() => window.${promptType}('foo'))`);
      const dialogWin = await dialogPromise;

      if (promptType === "prompt") {
        dialogWin.document.getElementById("loginTextbox").value = "bar";
      }

      if (accept) {
        dialogWin.document.querySelector("dialog").acceptDialog();
      } else {
        dialogWin.document.querySelector("dialog").cancelDialog();
      }

      const closedEvent = await closed;

      // Special-case prompts of type alert, which can only be accepted.
      const expectedState = promptType === "alert" && !accept ? true : accept;
      is(closedEvent.detail.accepted, expectedState, "Received expected state");

      is(
        closedEvent.detail.promptType,
        promptType,
        "Received expected prompt type"
      );

      const expectedText =
        promptType === "prompt" && accept ? "bar" : undefined;
      is(
        closedEvent.detail.userText,
        expectedText,
        "Received expected user text"
      );
    }
  }

  listener.destroy();
});

add_task(async function test_close_event_details_beforeunload() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  const listener = new PromptListener();
  listener.startListening();

  for (const leavePage of [false, true]) {
    info(`Test with leavePage=${leavePage}`);

    const tab = addTab(gBrowser, BEFOREUNLOAD_URL);
    try {
      const browser = tab.linkedBrowser;
      await BrowserTestUtils.browserLoaded(browser, false, BEFOREUNLOAD_URL);

      const closed = listener.once("closed");

      let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
      BrowserTestUtils.startLoadingURIString(browser, "about:blank");
      const dialogWin = await dialogPromise;

      if (leavePage) {
        dialogWin.document.querySelector("dialog").acceptDialog();
      } else {
        dialogWin.document.querySelector("dialog").cancelDialog();
      }

      const closedEvent = await closed;
      is(closedEvent.detail.accepted, leavePage, "Received expected state");
      is(
        closedEvent.detail.promptType,
        "beforeunload",
        "Received expected prompt type"
      );
      is(closedEvent.detail.userText, undefined, "Received expected user text");

      if (leavePage) {
        await BrowserTestUtils.browserLoaded(browser, false, "about:blank");
      }
    } finally {
      gBrowser.removeTab(tab);
    }
  }

  listener.destroy();
});

add_task(async function test_events_in_another_browser() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const selectedBrowser = win.gBrowser.selectedBrowser;
  const listener = new PromptListener(() => ({
    contentBrowser: selectedBrowser,
    window: selectedBrowser.ownerGlobal,
  }));
  const events = [];
  const onEvent = (name, data) => events.push(data);
  listener.on("opened", onEvent);
  listener.on("closed", onEvent);

  listener.startListening();

  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  await createScriptNode(`setTimeout(() => window.confirm('test'))`);
  const dialogWin = await dialogPromise;

  Assert.strictEqual(events.length, 0, "No event was received");

  dialogWin.document.querySelector("dialog").acceptDialog();

  // Wait a bit to make sure that the event didn't come.
  await new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, 500);
  });

  Assert.strictEqual(events.length, 0, "No event was received");

  listener.destroy();
  await BrowserTestUtils.closeWindow(win);
});
