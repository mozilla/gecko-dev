"use strict";

const BODY = `
function err(s) {
  dump("ERROR: " + s + "\\n");
  throw new Error(s);
}

function checkNumClients(actual, expected) {
  if (actual != expected) {
    let s = 'Expected ' + expected + ' clients, found ' + actual;
    err(s);
  }
}

var updateCount = 0;

registration.onupdatefound = (e) => {
  clients.matchAll().then((clients) => {
    switch (updateCount) {
      case 0: checkNumClients(clients.length, 0); break;
      case 1: checkNumClients(clients.length, 1); break;
      default: err("Too many updates, sorry."); break;
    }
    updateCount++;

    if (clients.length) {
      clients[0].postMessage("updatefound");
    }
  });
}
`;

function handleRequest(request, response) {
  // This header is necessary for making this script able to be loaded.
  response.setHeader("Content-Type", "application/javascript");

  var body = "/* " + Date.now() + " */\n" + BODY;
  response.write(body);
}
