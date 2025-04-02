/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB = `data:text/html,
  <html>
    <body>
      <input id="input1" onblur="alert(1)">
      <input id="input2">
    </body>
  </html>`;

function waitForInputFocused(tab, inputId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [inputId], async inputId => {
    let input = content.document.getElementById(inputId);
    let inputFocused = new Promise(r => {
      input.addEventListener("focus", function () {
        r();
      });
    });
    content.document.getElementById(inputId).focus();
    await inputFocused;
  });
}

add_task(async function test_focusSwitchNestedEventLoop() {
  waitForExplicitFinish();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TAB, true);

  await waitForInputFocused(tab, "input1");

  let promptPromise = BrowserTestUtils.promiseAlertDialogOpen();

  let input2FocusedPromise = waitForInputFocused(tab, "input2");

  let prompt = await promptPromise;
  let dialog = prompt.document.querySelector("dialog");
  dialog.cancelDialog();

  // Although we asked to focus "input2" early, the focus event will
  // only be fired after the alert is closed, so we wait it here.
  await input2FocusedPromise;

  let input2BlurredPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      let input = content.document.getElementById("input2");

      let inputBlurred = new Promise(r => {
        input.addEventListener("blur", function () {
          r();
        });
      });
      await inputBlurred;
    }
  );

  await waitForInputFocused(tab, "input1");

  // In a buggy Firefox build, this promise never resolves.
  await input2BlurredPromise;
  ok(true, "input2 is now correctly blurred");
  BrowserTestUtils.removeTab(tab);
});
