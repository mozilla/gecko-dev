function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader(
    "Set-Cookie",
    "thirdPartyFetch=value; SameSite=None; Secure;",
    false
  );
  response.setHeader(
    "Access-Control-Allow-Origin",
    "https://example.com",
    false
  );
  response.setHeader("Access-Control-Allow-Credentials", "true", false);
  response.setHeader("Access-Control-Allow-Methods", "GET", false);
}
