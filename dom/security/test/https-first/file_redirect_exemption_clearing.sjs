/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable @microsoft/sdl/no-insecure-url */

// See browser_redirect_exemption_clearing.js

const SOURCE_HOST = "example.com";
const TARGET_HOST = "example.org";

function handleRequest(request, response) {
  if (request.host === SOURCE_HOST) {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader(
      "Location",
      `http://${request.scheme === "https" ? SOURCE_HOST : TARGET_HOST}${request.path}`,
      false
    );
  }
}
