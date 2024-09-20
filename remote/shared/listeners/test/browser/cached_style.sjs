/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setHeader(
    "Cache-Control",
    "no-transform,public,max-age=300,s-maxage=900"
  );
  response.setHeader("Expires", "Thu, 01 Dec 2100 20:00:00 GMT");
  response.setHeader("Content-Type", "text/css", false);
  response.write("body { background-color: black; }");
}
