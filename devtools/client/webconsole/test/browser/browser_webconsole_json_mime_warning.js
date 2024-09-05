/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that <script> loads with JSON MIME types produce a warning.
// See Bug 1916351.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-json-mime.html";
const MIME_WARNING_MSG =
  "The script from “https://example.com/browser/devtools/client/webconsole/test/browser/test-json-mime.json” was loaded even though its MIME type (“application/json”) is not a valid JavaScript MIME type.";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => findWarningMessage(hud, MIME_WARNING_MSG), "", 100);
  ok(true, "JSON MIME type warning displayed");
});
