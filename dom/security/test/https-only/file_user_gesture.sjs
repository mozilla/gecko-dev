/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString === "redirect") {
    response.setStatusLine(request.httpVersion, 302, "Moved");
    response.setHeader(
      "Location",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      `http://${request.host}${request.path}`,
      false
    );
    return;
  }

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write(`<button id="directButton" onclick="location.href='http://${request.host}${request.path}'" type="button">direct</button>
<button id="redirectButton" onclick="location.href='http://${request.host}${request.path}?redirect'" type="button">redirect</button> `);
}
