/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MockFilePicker = SpecialPowers.MockFilePicker;

add_setup(async function () {
  MockFilePicker.init(window.browsingContext);

  registerCleanupFunction(async () => {
    MockFilePicker.cleanup();
  });
});

add_task(async function test_save_image_canvas() {
  await BrowserTestUtils.withNewTab(
    "data:text/html;charset=utf-8,<canvas></canvas>",
    async browser => {
      let menu = document.getElementById("contentAreaContextMenu");
      let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      let filePicker = waitForFilePicker();

      BrowserTestUtils.synthesizeMouseAtCenter(
        "canvas",
        { type: "contextmenu", button: 2 },
        browser
      );
      await popupShown;

      menu.activateItem(menu.querySelector("#context-saveimage"));

      await filePicker;
    }
  );
});

function waitForFilePicker() {
  return new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      MockFilePicker.showCallback = null;
      ok(true, "Saw the file picker");

      resolve();
    };
  });
}
