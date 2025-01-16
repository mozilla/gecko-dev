/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  // let's enjoy the amazing CSP setting
  response.setHeader(
    "Content-Security-Policy",
    "frame-ancestors 'self'",
    false
  );

  // let's avoid caching issues
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Cache-Control", "no-cache", false);

  // everything is fine - no needs to worry :)
  response.setStatusLine(request.httpVersion, 200);
}
