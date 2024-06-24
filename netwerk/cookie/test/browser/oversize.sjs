function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  const maxBytesPerCookie = 4096;
  const maxBytesPerAttribute = 1024;

  aResponse.setHeader(
    "Set-Cookie",
    "a=" + Array(maxBytesPerCookie + 1).join("x"),
    true
  );
  aResponse.setHeader(
    "Set-Cookie",
    "b=c; path=/" + Array(maxBytesPerAttribute + 1).join("x"),
    true
  );
  aResponse.setHeader(
    "Set-Cookie",
    "c=42; domain=" + Array(maxBytesPerAttribute + 1).join("x") + ".net",
    true
  );
  aResponse.setHeader(
    "Set-Cookie",
    "d=42; max-age=" + Array(maxBytesPerAttribute + 2).join("x"),
    true
  );
}
