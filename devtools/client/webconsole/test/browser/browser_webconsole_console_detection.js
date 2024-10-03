/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the console doesn't trigger any function in the page when logging the message

"use strict";

const TEST_URI = `data:text/html,<script>function logMessages() {
      /* Check if page functions can be called by console previewers */
      const date = new Date(2024, 0, 1);
      date.getTime = () => {
        return 42;
      };
      Date.prototype.getTime = date.getTime;
      console.log("date", date);

      const regexp = /foo/m;
      regexp.toString = () => {
        return "24";
      };
      console.log("regexp", regexp);
  }</script>`;

add_task(async function () {
  await addTab(TEST_URI);

  info("Open the console");
  const hud = await openConsole();

  // Only log messages *after* the console is opened as it may not trigger the same codepath for cached messages
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.logMessages();
  });

  const dateMessage = await waitFor(() => findConsoleAPIMessage(hud, "date"));
  Assert.stringContains(dateMessage.textContent, "Jan 01 2024");

  const regExpMessage = await waitFor(() =>
    findConsoleAPIMessage(hud, "regexp")
  );
  Assert.stringContains(regExpMessage.textContent, "/foo/m");

  await closeTabAndToolbox();
});
