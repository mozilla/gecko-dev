"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/javascript", false);
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.write(`
console.log('module loaded')
export function f() {}
`);
}
