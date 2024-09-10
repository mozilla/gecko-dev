/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const network = Glean.network;

const page_probes = [
  network.tlsHandshake,
  network.dnsStart,
  network.dnsEnd,
  network.tcpConnection,
  network.openToFirstSent,
  network.firstSentToLastReceived,
  network.openToFirstReceived,
  network.completeLoad,
  network.completeLoadNet,
];

const sub_probes = [
  network.subTlsHandshake,
  network.subDnsStart,
  network.subDnsEnd,
  network.subTcpConnection,
  network.subOpenToFirstSent,
  network.subFirstSentToLastReceived,
  network.subOpenToFirstReceived,
  network.subCompleteLoad,
  network.subCompleteLoadNet,
];

add_setup(function test_setup() {
  Services.fog.initializeFOG();
  Services.prefs.setIntPref("network.trr.mode", 2);
});

registerCleanupFunction(() => {
  Services.fog.testResetFOG();
  Services.prefs.clearUserPref("network.trr.mode");
});

async function test_loadgroup_glean_http2(page, probes) {
  let loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(
    Ci.nsILoadGroup
  );
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );

  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  Services.fog.testResetFOG();

  let server = new NodeHTTP2Server();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });

  let loadListener = {
    onStartRequest: () => {},
    onStopRequest: () => {
      for (const probe of probes) {
        const result = probe.testGetValue();
        // Should have non-zero count, sum, and first value
        Assert.less(0, result.count);
        Assert.less(0, result.sum);
        Assert.less(0, Object.values(result.values)[0]);
      }
    },
    QueryInterface: ChromeUtils.generateQI([
      "nsIRequestObserver",
      "nsISupportsWeakReference",
    ]),
  };
  loadGroup.groupObserver = loadListener;

  await server.registerPathHandler("/", (req, resp) => {
    resp.writeHead(200);
    resp.end("done");
  });
  let chan = NetUtil.newChannel({
    uri: `https://localhost:${server.port()}`,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);

  if (page) {
    loadGroup.defaultLoadRequest = chan;
  } else {
    // Set mDefaultLoadRequest to a dummy channel when testing sub
    await server.registerPathHandler("/dummy", (req, resp) => {
      resp.writeHead(404);
      resp.end();
    });
    let dummy = NetUtil.newChannel({
      uri: `https://localhost:${server.port()}/dummy`,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    loadGroup.defaultLoadRequest = dummy;
  }

  loadGroup.addRequest(chan, null);
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  loadGroup.removeRequest(chan, null, Cr.NS_OK);
}

add_task(async function test_loadgroup_glean_http2_page() {
  await test_loadgroup_glean_http2(true, page_probes);
});

add_task(async function test_loadgroup_glean_http2_sub() {
  await test_loadgroup_glean_http2(false, sub_probes);
});
