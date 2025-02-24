/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that jump to definition works in preview popups

"use strict";

const TEST_URL =
  "data:text/html," +
  encodeURIComponent(`<script>
  function main() {
    b();
    debugger;
  }
  function b() {}
  const o = { b };
</script>`);

const TEST_URL_ONE_LINE =
  "data:text/html," +
  encodeURIComponent(
    `<script>function main() {b();debugger;}function b() {}</script>`
  );

add_task(async function testJumpToDefinition() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL, TEST_URL);

  invokeInTab("main");
  await waitForPaused(dbg);

  info("Hovers over 'this' token to display the preview.");
  await tryHovering(dbg, 3, 5, "previewPopup");

  info("Wait for the 'b' function to be previewed");
  await waitForAllElements(dbg, "previewPopupObjectFunction", 1);

  info("Click on the function to jump to its location");
  await clickElement(dbg, "previewPopupObjectFunctionJumpToDefinition");

  await waitForSelectedLocation(dbg, 6, 12);
});

add_task(async function testJumpToDefinitionInPrettyPrintedSource() {
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_URL_ONE_LINE,
    TEST_URL_ONE_LINE
  );

  await selectSource(dbg, TEST_URL_ONE_LINE);
  await prettyPrint(dbg);

  invokeInTab("main");
  await waitForPaused(dbg);

  info("Hovers over 'this' token to display the preview.");
  await tryHovering(dbg, 3, 3, "previewPopup");

  info("Wait for the 'b' function to be previewed");
  await waitForAllElements(dbg, "previewPopupObjectFunction", 1);

  info("Click on the function to jump to its location");
  await clickElement(dbg, "previewPopupObjectFunctionJumpToDefinition");

  await waitForSelectedLocation(dbg, 6, 10);
});

add_task(async function testJumpToDefinitionOfObjectProperty() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL, TEST_URL);

  invokeInTab("main");
  await waitForPaused(dbg);

  info("Hovers over 'o' token to display the preview.");
  await tryHovering(dbg, 7, 9, "previewPopup");

  info("Wait for the 'b' function to be previewed");
  await waitForAllElements(dbg, "previewPopupObjectFunction", 1);

  info("Click on the function to jump to its location");
  await clickElement(dbg, "previewPopupObjectFunctionJumpToDefinition");

  await waitForSelectedLocation(dbg, 6, 12);
});
