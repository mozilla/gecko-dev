/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  response.setHeader("Content-Type", "application/octet-stream", false);
}
