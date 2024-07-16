function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  let origin = "http://example.net";
  if (aRequest.hasHeader("Origin")) {
    origin = aRequest.getHeader("Origin");
  }
  aResponse.setHeader("Access-Control-Allow-Origin", origin);
  aResponse.setHeader("Access-Control-Allow-Credentials", "true");
  let cookie = "";
  if (aRequest.hasHeader("Cookie")) {
    cookie = aRequest.getHeader("Cookie");
  }
  aResponse.write("cookie:" + cookie);

  if (aRequest.queryString) {
    aResponse.setHeader("Set-Cookie", "foopy=" + aRequest.queryString);
  }
}
