/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Asserts that the tab key works with selected content in the input

const TEST_URI =
  "data:text/html,<!DOCTYPE html><meta charset=utf8>Testing jsterm with no input";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  info("Check that hitting Tab when input has some content");
  jsterm.focus();
  setInputValue(hud, "test content");

  info("Select all the content in the input");
  const isMacOS = Services.appinfo.OS === "Darwin";
  EventUtils.synthesizeKey("A", {
    [isMacOS ? "metaKey" : "ctrlKey"]: true,
  });

  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Tab");
  is(
    getInputValue(hud),
    "    test content",
    "input content should include spaces at the start"
  );
});
