/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_overwrite_te() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let server = new NodeHTTP2Server();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  await server.registerPathHandler("/", (req, resp) => {
    // Set response headers
    resp.writeHead(200, { "Content-Type": "application/json" });

    // Dump headers
    resp.end(JSON.stringify(req.headers, null, 2));
  });

  let chan = NetUtil.newChannel({
    uri: `https://localhost:${server.port()}/`,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;

  let { buffer } = await new Promise(resolve => {
    function finish(r, b) {
      resolve({ req: r, buffer: b });
    }
    chan.asyncOpen(new ChannelListener(finish, null, CL_ALLOW_UNKNOWN_CL));
  });

  let json = JSON.parse(buffer);
  Assert.equal(json.te, "trailers");

  chan = NetUtil.newChannel({
    uri: `https://localhost:${server.port()}/`,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  chan.setRequestHeader("TE", "moz_no_te_trailers", false);

  ({ buffer } = await new Promise(resolve => {
    function finish(r, b) {
      resolve({ req: r, buffer: b });
    }
    chan.asyncOpen(new ChannelListener(finish, null, CL_ALLOW_UNKNOWN_CL));
  }));

  json = JSON.parse(buffer);
  Assert.equal(json.te, undefined);
});
