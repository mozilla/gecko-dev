function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  aResponse.setHeader("Set-Cookie", " ; path=/; secure", true);
}
