/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-disable @microsoft/sdl/no-insecure-url */

function handleRequest(request, response) {
  if (request.scheme === "https") {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader(
      "Location",
      "http://example.com/browser/dom/security/test/https-first/file_schemeless_redirect.sjs",
      false
    );
  }
}
