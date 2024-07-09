"use strict";

let { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

async function handleRequest(request, response) {
  response.processAsync();
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.scheme === "https") {
    await new Promise(r => setTimeout(r, 5000));
    response.write("HTTPS is slow!");
    response.finish();
  } else {
    response.write("HTTP is fast!");
    response.finish();
  }
}
