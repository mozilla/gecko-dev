/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests pretty-printing of HTML file with windows line-breaks (\r\n).

"use strict";

requestLongerTimeout(2);

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");

httpServer.registerPathHandler("/doc_line_breaks.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(
    `TEST with line breaks\r\n<script>(function(){\r\n})('test')</script>`
  );
});

const TEST_URL = `http://localhost:${httpServer.identity.primaryPort}/doc_line_breaks.html`;

add_task(async function () {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL);

  await selectSource(dbg, "doc_line_breaks.html");
  clickElement(dbg, "prettyPrintButton");

  await waitForSelectedSource(dbg, "doc_line_breaks.html:formatted");
  const prettyPrintedSource = findSourceContent(
    dbg,
    "doc_line_breaks.html:formatted"
  );
  ok(prettyPrintedSource, "Pretty-printed source exists");

  info("Check that the HTML file was pretty-printed as expected");
  is(
    prettyPrintedSource.value,
    "TEST with line breaks\n<script>\n(function () {\n}) ('test')\n</script>",
    "HTML file is pretty printed as expected"
  );
});
