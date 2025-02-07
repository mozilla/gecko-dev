/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that special characters in the user input are cleared correctly
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1945716

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Web Console test for Bug 1945716";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const expression = `𝐀𝐁𝐂😀`;
  setInputValue(hud, expression);
  EventUtils.synthesizeKey("KEY_Backspace");
  is(getInputValue(hud), `𝐀𝐁𝐂`, "The emoji character is deleted correctly");
});
