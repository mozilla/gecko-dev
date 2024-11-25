/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that the buttons are ordered as expected, and that the
 * "Close current tab" button actually closes the current tab.
 */
add_task(async function test_close_current_tab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  async function observer(subject) {
    let dialogElement = subject.document.getElementById("commonDialog");
    let buttons = Array.from(
      dialogElement.buttonBox.getElementsByTagName("button")
    );

    // button reordering only happens on Unix
    if (AppConstants.XP_UNIX) {
      is(
        buttons[2].label,
        dialogElement.getButton("cancel").label,
        "Cancel button should be at position 2"
      );
    }

    let closePromise = BrowserTestUtils.waitForTabClosing(tab);
    dialogElement.getButton("extra1").click();
    await closePromise;

    is(
      gBrowser.tabs.find(t => t === tab),
      undefined,
      "Tab should no longer be open"
    );
  }

  Services.obs.addObserver(observer, "common-dialog-loaded");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.warnOnQuitShortcut", true],
      ["browser.warnOnQuit", true],
    ],
  });

  // triggers quit-application-requested
  canQuitApplication(undefined, "shortcut");

  Services.obs.removeObserver(observer, "common-dialog-loaded");
});
