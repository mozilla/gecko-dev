function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let query = new URLSearchParams(request.queryString);

  let setState = query.get("setState");
  if (setState == "cookie-server") {
    let cookieHeader = "foo=bar;";

    if (query.get("isThirdParty") === "true") {
      // If we're in the third-party context request a partitioned cookies
      // for compatibility with CHIPS / 3rd party cookies being blocked by
      // default.
      cookieHeader += "SameSite=None; Secure; Partitioned;";
    }

    response.setHeader("Set-Cookie", cookieHeader);
  }

  let statusCode = 200;
  let statusCodeQuery = query.get("statusCode");
  if (statusCodeQuery) {
    statusCode = Number.parseInt(statusCodeQuery);

    // Server side redirect.
    if (statusCode == 301 || statusCode == 302) {
      response.setStatusLine("1.1", statusCode, "Found");
      response.setHeader("Location", query.get("target"), false);
      return;
    }
  }

  // No redirect.
  response.setStatusLine("1.1", statusCode, "OK");
  response.write(JSON.stringify(Object.fromEntries(query), null, 2));
}
