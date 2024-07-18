function handleRequest(request, response) {
  if (request.queryString == "reset") {
    setState("counter", "0");

    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/text", false);
    const body = "reset";
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  let counter = parseInt(getState("counter"));
  setState("counter", (counter + 1).toString());

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setHeader("Content-Type", "text/javascript", false);
  const body = `
document.body.setAttribute("counter", "${counter}");
`;
  response.bodyOutputStream.write(body, body.length);
}
