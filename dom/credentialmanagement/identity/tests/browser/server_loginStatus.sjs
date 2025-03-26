/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  if (params.has("status")) {
    response.setHeader("Set-Login", params.get("status"), false);
  }
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Location", "/", false);
  response.setStatusLine(request.httpVersion, "307", "Temporary Redirect");
  response.write("<!DOCTYPE html>");
}
