function handleRequest(request, response) {
  if (request.queryString == "reset") {
    setState("counter", "0");
    setState("log", "");

    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/text", false);
    const body = "reset";
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  if (request.queryString == "log") {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/text", false);
    const body = getState("log");
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  setState("log", getState("log") + "," + request.queryString);

  const query = {};
  for (const segment of request.queryString.split("&")) {
    const [name, value] = segment.split("=");
    query[name] = value;
  }

  if ("redirect" in query) {
    response.setStatusLine(request.httpVersion, 302, "Found");
    if (query.redirect == "cacheable") {
      response.setHeader("Cache-Control", "max-age=10000", false);
    } else {
      response.setHeader("Cache-Control", "no-cache", false);
    }
    response.setHeader(
      "Location",
      `redirect_server.sjs?script=${query.script}`,
      false
    );
    return;
  }

  let counter = parseInt(getState("counter"));
  setState("counter", (counter + 1).toString());

  response.setStatusLine(request.httpVersion, 200, "OK");
  if (query.script == "cacheable") {
    response.setHeader("Cache-Control", "max-age=10000", false);
  } else {
    response.setHeader("Cache-Control", "no-cache", false);
  }
  response.setHeader("Content-Type", "text/javascript", false);
  const body = `
document.body.setAttribute("counter", "${counter}");
`;
  response.bodyOutputStream.write(body, body.length);
}
