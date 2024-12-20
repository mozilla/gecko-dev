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

  let loadIsHttps = aExpected.loadIsHttps || null;
  let loadIsHttp = aExpected.loadIsHttp || null;
  let loadIsHttpForLocalDomain = aExpected.loadIsHttpForLocalDomain || null;

  let glean = Glean.networking.httpsHttpOrLocal;
  Assert.equal(
    glean.load_is_https.testGetValue(),
    loadIsHttps,
    "verify load_is_https"
  );
  Assert.equal(
    glean.load_is_http.testGetValue(),
    loadIsHttp,
    "verify load_is_http"
  );
  Assert.equal(
    glean.load_is_http_for_local_domain.testGetValue(),
    loadIsHttpForLocalDomain,
    "verify load_is_http_for_local_domain"
  );
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

  let chan = makeChan(`http://${DOMAIN}`);
  let [req] = await channelOpenPromise(chan);
  info(
    "req.remoteAddress=" +
      req.QueryInterface(Ci.nsIHttpChannelInternal).remoteAddress
  );
  verifyGleanValues(`test ip=${ip}`, expected);

  Services.dns.clearCache(false);
  override.clearOverrides();
  mockNetwork.clearNetAddrOverrides();
  Services.obs.notifyObservers(null, "net:prune-all-connections");
}

add_task(async function test_ipv4_local() {
  Services.prefs.setBoolPref("dom.security.https_only_mode", false);
  Services.prefs.setBoolPref("dom.security.https_first", false);
  Services.prefs.setBoolPref("dom.security.https_first_schemeless", false);

  await do_test("10.0.0.1", { loadIsHttpForLocalDomain: 1 });
  await do_test("172.16.0.1", { loadIsHttpForLocalDomain: 1 });
  await do_test("192.168.0.1", { loadIsHttpForLocalDomain: 1 });
  await do_test("169.254.0.1", { loadIsHttpForLocalDomain: 1 });
  await do_test("127.0.0.1", { loadIsHttpForLocalDomain: 1 });
});

add_task(async function test_ipv6_local() {
  await do_test("::1", { loadIsHttpForLocalDomain: 1 });
  await do_test("fc00::1", { loadIsHttpForLocalDomain: 1 });
  await do_test("fe80::1", { loadIsHttpForLocalDomain: 1 });
});

add_task(async function test_http() {
  await do_test("1.1.1.1", { loadIsHttp: 1 });
});

add_task(async function test_https() {
  Services.prefs.setBoolPref("dom.security.https_only_mode", true);
  let httpsServer = new NodeHTTPSServer();
  await httpsServer.start();
  registerCleanupFunction(async () => {
    await httpsServer.stop();
  });
  await do_test("1.1.1.1", { loadIsHttps: 1 }, 443, httpsServer.port());
});
