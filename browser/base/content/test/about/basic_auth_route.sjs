/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", 'Basic realm="foo"', false);
}
