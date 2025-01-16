/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  // let's enjoy the amazing XFO setting
  response.setHeader("X-Frame-Options", "SAMEORIGIN");

  // let's avoid caching issues
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Cache-Control", "no-cache", false);

  // everything is fine - no needs to worry :)
  response.setStatusLine(request.httpVersion, 200);
}
