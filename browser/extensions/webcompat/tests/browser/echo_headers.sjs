/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript sts=2 sw=2 et tw=80: */
"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html", false);

  let headers = {};
  let enumerator = request.headers;
  while (enumerator.hasMoreElements()) {
    let header = enumerator.getNext().data;
    headers[header.toLowerCase()] = request.getHeader(header);
  }

  response.write(
    `<!DOCTYPE html><script>headers = ${JSON.stringify(headers)}</script>`
  );
}
