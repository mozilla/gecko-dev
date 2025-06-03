/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pressing Escape after changing the width or height will revert the change.

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, async function ({ ui }) {
  const [widthInput, heightInput] = ui.toolWindow.document.querySelectorAll(
    ".text-input.viewport-dimension-input"
  );
  checkInputCancel(ui, widthInput);
  checkInputCancel(ui, heightInput);
});

function checkInputCancel(ui, input) {
  const { Simulate } = ui.toolWindow.require(
    "resource://devtools/client/shared/vendor/react-dom-test-utils.js"
  );
  const value = parseInt(input.value, 10);

  input.focus();
  input.value = value + 100;
  Simulate.change(input);
  EventUtils.synthesizeKey("KEY_Escape", {}, ui.toolWindow);

  is(
    parseInt(input.value, 10),
    value,
    "Input value should be reverted after canceling edit."
  );
}
