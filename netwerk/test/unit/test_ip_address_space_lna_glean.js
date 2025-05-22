"use strict";

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);
const mockNetwork = Cc[
  "@mozilla.org/network/mock-network-controller;1"
].getService(Ci.nsIMockNetworkLayerController);
const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

const DOMAIN = "example.org";

function makeChan(url, expected) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);

  if (
    expected.PublicToPrivateHttp !== undefined ||
    expected.PublicToLocalHttp !== undefined ||
    expected.PublicToPublicHttp !== undefined ||
    expected.PublicToPrivateHttps !== undefined ||
    expected.PublicToLocalHttps !== undefined ||
    expected.PublicToPublicHttp !== undefined
  ) {
    chan.loadInfo.parentIpAddressSpace = Ci.nsILoadInfo.Public;
  } else if (
    expected.PrivateToLocalHttp !== undefined ||
    expected.PrivateToPrivateHttp !== undefined ||
    expected.PrivateToLocalHttps !== undefined ||
    expected.PrivateToPrivateHttps !== undefined
  ) {
    chan.loadInfo.parentIpAddressSpace = Ci.nsILoadInfo.Private;
  }
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
      certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
        false
      );
    }
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      true
    );
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

let server;

add_setup(async function setup() {
  Services.prefs.setBoolPref("network.socket.attach_mock_network_layer", true);

  Services.fog.initializeFOG();

  server = new NodeHTTPServer();
  await server.start();
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.disable-localhost-when-offline");
    Services.prefs.clearUserPref("network.dns.use_override_as_peer_address");
    Services.prefs.clearUserPref("dom.security.https_only_mode");
    Services.prefs.clearUserPref("dom.security.https_first");
    Services.prefs.clearUserPref("dom.security.https_first_schemeless");
    Services.prefs.clearUserPref("network.socket.attach_mock_network_layer");
    await server.stop();
  });
});

function verifyGleanValues(aDescription, aExpected) {
  info(aDescription);

  let privateToLocalHttp = aExpected.PrivateToLocalHttp || null;
  let publicToPrivateHttp = aExpected.PublicToPrivateHttp || null;
  let publicToLocalHttp = aExpected.PublicToLocalHttp || null;
  let privateToLocalHttps = aExpected.PrivateToLocalHttps || null;
  let publicToPrivateHttps = aExpected.PublicToPrivateHttps || null;
  let publicToLocalHttps = aExpected.PublicToLocalHttps || null;

  let glean = Glean.networking.localNetworkAccess;
  Assert.equal(
    glean.private_to_local_http.testGetValue(),
    privateToLocalHttp,
    "verify private_to_local_http"
  );
  Assert.equal(
    glean.public_to_private_http.testGetValue(),
    publicToPrivateHttp,
    "verify public_to_private_http"
  );
  Assert.equal(
    glean.public_to_local_http.testGetValue(),
    publicToLocalHttp,
    "verify public_to_local_http"
  );
  Assert.equal(
    glean.private_to_local_https.testGetValue(),
    privateToLocalHttps,
    "verify private_to_local_http"
  );
  Assert.equal(
    glean.public_to_private_https.testGetValue(),
    publicToPrivateHttps,
    "verify public_to_private_http"
  );
  Assert.equal(
    glean.public_to_local_https.testGetValue(),
    publicToLocalHttps,
    "verify public_to_local_http"
  );

  Assert.equal(
    glean.public_to_local_https.testGetValue(),
    publicToLocalHttps,
    "verify public_to_local_http"
  );

  if (
    privateToLocalHttp ||
    publicToPrivateHttp ||
    publicToLocalHttp ||
    privateToLocalHttps ||
    publicToPrivateHttps ||
    publicToLocalHttps
  ) {
    Assert.equal(
      glean.success.testGetValue(),
      1,
      "verify local_network_access_success"
    );
    // XXX (sunil) add test for local_network_access_failure cases
  }
}

async function do_test(ip, expected, srcPort, dstPort) {
  Services.fog.testResetFOG();

  override.addIPOverride(DOMAIN, ip);
  let fromAddr = mockNetwork.createScriptableNetAddr(ip, srcPort ?? 80);
  let toAddr = mockNetwork.createScriptableNetAddr(
    fromAddr.family == Ci.nsINetAddr.FAMILY_INET ? "127.0.0.1" : "::1",
    dstPort ?? server.port()
  );

  mockNetwork.addNetAddrOverride(fromAddr, toAddr);

  info(`do_test ${ip}, ${fromAddr} -> ${toAddr}`);

  let chan = makeChan(`http://${DOMAIN}`, expected);
  let [req] = await channelOpenPromise(chan);

  info(
    "req.remoteAddress=" +
      req.QueryInterface(Ci.nsIHttpChannelInternal).remoteAddress
  );

  if (expected.PublicToPrivateHttp) {
    Assert.equal(chan.loadInfo.ipAddressSpace, Ci.nsILoadInfo.Private);
  } else if (expected.PrivateToLocalHttp || expected.PrivateToLocalHttp) {
    Assert.equal(chan.loadInfo.ipAddressSpace, Ci.nsILoadInfo.Local);
  }

  verifyGleanValues(`test ip=${ip}`, expected);

  Services.dns.clearCache(false);
  override.clearOverrides();
  mockNetwork.clearNetAddrOverrides();
  Services.obs.notifyObservers(null, "net:prune-all-connections");
}

add_task(async function test_lna_http() {
  Services.prefs.setBoolPref("dom.security.https_only_mode", false);
  Services.prefs.setBoolPref("dom.security.https_first", false);
  Services.prefs.setBoolPref("dom.security.https_first_schemeless", false);

  await do_test("10.0.0.1", { PublicToPrivateHttp: 1 });

  // NO LNA access do not increment
  await do_test("10.0.0.1", { PrivateToPrivateHttp: 0 });
  await do_test("2.2.2.2", { PublicToPublicHttp: 0 });

  await do_test("100.64.0.1", { PublicToPrivateHttp: 1 });
  await do_test("127.0.0.1", { PublicToLocalHttp: 1 });
  await do_test("127.0.0.1", { PrivateToLocalHttp: 1 });
  if (AppConstants.platform != "android") {
    await do_test("::1", { PrivateToLocalHttp: 1 });
  }
});

add_task(async function test_lna_https() {
  Services.prefs.setBoolPref("dom.security.https_only_mode", true);
  let httpsServer = new NodeHTTPSServer();
  await httpsServer.start();
  registerCleanupFunction(async () => {
    await httpsServer.stop();
  });
  await do_test(
    "10.0.0.1",
    { PublicToPrivateHttps: 1 },
    443,
    httpsServer.port()
  );

  // NO LNA access do not increment
  await do_test(
    "10.0.0.1",
    { PrivateToPrivateHttps: 0 },
    443,
    httpsServer.port()
  );
  await do_test(
    "2.2.2.2",
    { PublicToPublicHttps: 0 },
    443,
    httpsServer.port(),
    true
  );

  await do_test(
    "100.64.0.1",
    { PublicToPrivateHttps: 1 },
    443,
    httpsServer.port()
  );
  await do_test(
    "127.0.0.1",
    { PublicToLocalHttps: 1 },
    443,
    httpsServer.port()
  );
  await do_test(
    "127.0.0.1",
    { PrivateToLocalHttps: 1 },
    443,
    httpsServer.port()
  );
  if (AppConstants.platform != "android") {
    await do_test("::1", { PrivateToLocalHttps: 1 }, 443, httpsServer.port());
  }
});
