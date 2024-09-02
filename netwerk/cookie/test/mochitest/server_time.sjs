function handleRequest(aRequest, aResponse) {
  let parts = aRequest.queryString.split("&");

  aResponse.setStatusLine(aRequest.httpVersion, 200);

  aResponse.setHeader(
    "Set-Cookie",
    `${parts[0]}=foo; path=/; sameSite=none; secure; expires=${new Date(
      parseInt(parts[2], 10)
    ).toGMTString()}`,
    false
  );

  if (parts[1] != "") {
    aResponse.setHeader("Date", new Date(parseInt(parts[1], 10)).toGMTString());
  }

  aResponse.setHeader("Content-Type", "text/plain", false);
  aResponse.write("Hello world!");
}
