function handleRequest(request, response) {
  if (request.queryString == "slow") {
    // Do not send the response header at all.
    response.seizePower();
  } else {
    response.setHeader("Content-Type", "text/plain", false);
    response.write("hey");
  }
}
