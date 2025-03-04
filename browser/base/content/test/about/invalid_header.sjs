/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200);

  // x-invalid-header permits invalid chars in httpd.sys.mjs
  response.setHeader("x-invalid-header-value", "Foo\x00Bar", false);
}
