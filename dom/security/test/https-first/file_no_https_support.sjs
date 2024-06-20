/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine("1.1", 404, "Not Found");
  response.write("not found");
}
