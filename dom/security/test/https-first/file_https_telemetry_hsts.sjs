/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  if (request.queryString === "reset") {
    // Reset the HSTS policy, prevent influencing other tests
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Strict-Transport-Security", "max-age=0");
    response.write("Resetting HSTS");
    return;
  }
  let hstsHeader = "max-age=60";
  response.setHeader("Strict-Transport-Security", hstsHeader);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, 200);
  response.write("<html><body>ok</body></html>");
}
