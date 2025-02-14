function handleRequest(request, response) {
  if (request.queryString == "use-cacheable") {
    setState("cacheable", "true");

    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/text", false);
    const body = "ok";
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  if (request.queryString == "use-non-cacheable") {
    setState("cacheable", "false");

    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/text", false);
    const body = "ok";
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  let cacheable = getState("cacheable") === "true";

  response.setStatusLine(request.httpVersion, 200, "OK");
  if (cacheable) {
    response.setHeader("Cache-Control", "max-age=10000", false);
  } else {
    response.setHeader("Cache-Control", "no-cache", false);
  }
  response.setHeader("Content-Type", "text/javascript", false);

  const body = `
document.body.setAttribute("cacheable", "${cacheable}");
`;
  response.bodyOutputStream.write(body, body.length);
}
