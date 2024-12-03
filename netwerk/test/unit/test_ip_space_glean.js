"use strict";

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

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
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

add_setup(async function setup() {
  Services.fog.initializeFOG();
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.disable-localhost-when-offline");
    Services.prefs.clearUserPref("network.dns.use_override_as_peer_address");
    Services.prefs.clearUserPref("dom.security.https_only_mode");
    Services.prefs.clearUserPref("dom.security.https_first");
    Services.prefs.clearUserPref("dom.security.https_first_schemeless");
  });
});

function onBeforeConnect(callback) {
  Services.obs.addObserver(
    {
      observe(subject) {
        Services.obs.removeObserver(this, "http-on-before-connect");
        callback(subject.QueryInterface(Ci.nsIHttpChannel));
      },
    },
    "http-on-before-connect"
  );
}

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

async function do_test(ip, expected) {
  // Need to set this pref, so SocketTransport will always return
  // NS_ERROR_OFFLINE instead of trying to connect to a local address.
  Services.prefs.setBoolPref("network.disable-localhost-when-offline", true);
  // Set this pref so that nsHttpChannel::mPeerAddr will be assigned to the
  // override address.
  Services.prefs.setBoolPref("network.dns.use_override_as_peer_address", true);

  Services.fog.testResetFOG();

  override.addIPOverride(DOMAIN, ip);
  let chan = makeChan(`http://${DOMAIN}`);
  onBeforeConnect(chan => {
    chan.suspend();
    Promise.resolve().then(() => {
      Services.io.offline = true;
      chan.resume();
    });
  });

  await channelOpenPromise(chan, CL_EXPECT_FAILURE);
  verifyGleanValues(`test ip=${ip}`, expected);

  Services.dns.clearCache(false);
  override.clearOverrides();
  Services.io.offline = false;
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
  await do_test("1.1.1.1", { loadIsHttps: 1 });
});
