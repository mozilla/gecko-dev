/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  if (request.scheme === "https") {
    // this file is only available over http!
    response.setHeader("Cache-Control", "no-cache", false);
    response.setStatusLine("1.1", 404, "Not Found");
    response.write("This is the HTTPS response... - No such file here!");
  } else {
    response.processAsync();
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Disposition", "attachment; filename=file.txt");
    response.setHeader("Content-Type", "text/plain");
    response.write("File contents!\n");
    response.finish();
  }
}
