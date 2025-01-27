/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let h2Port;

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

add_setup(async function setup() {
  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.dns.localDomains");
  });
});

add_task(async function test_http_1_1() {
  let chan = makeChan(
    `https://foo.example.com:${h2Port}/h11required_with_content`
  );
  let [req, resp] = await channelOpenPromise(chan);
  Assert.equal(req.protocolVersion, "h2");
  Assert.equal(resp, "ok");
});
