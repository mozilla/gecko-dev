"use strict";

function handleRequest(request, response) {
  const { queryString } = request;
  if (queryString === "setup") {
    setState("cache-counter", "1");
    setState("etag", `"${Date.now()}-${Math.round(Math.random() * 100)}"`);
    response.setHeader("Content-Type", "text/html");
    response.write("OK");
    return;
  }

  const Etag = getState("etag");
  const IfNoneMatch = request.hasHeader("If-None-Match")
    ? request.getHeader("If-None-Match")
    : "";

  const counter = getState("cache-counter") || 1;
  const page = "<script>var jsValue = '" + counter + "';</script>" + counter;

  setState("cache-counter", "" + (parseInt(counter, 10) + 1));

  response.setHeader("Etag", Etag, false);

  if (IfNoneMatch === Etag) {
    response.setStatusLine(request.httpVersion, "304", "Not Modified");
  } else {
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.setHeader("Content-Length", page.length + "", false);
    response.write(page);
  }
}
