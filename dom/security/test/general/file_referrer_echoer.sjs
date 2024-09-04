// Echoes the Referer header back to the client.

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  if (aRequest.hasHeader("Referer")) {
    const referrer = aRequest.getHeader("Referer");
    aResponse.setHeader("Content-Type", "text/html", false);
    aResponse.write(referrer);
  }

  aResponse.setHeader("Access-Control-Allow-Origin", "*", false);
}
