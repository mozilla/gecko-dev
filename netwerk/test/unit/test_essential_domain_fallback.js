/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function waitForNotificationPromise(notification) {
  return new Promise(resolve => {
    function observer(aSubject, _aTopic, _aData) {
      info(aSubject);
      Services.obs.removeObserver(observer, notification);
      resolve(aSubject);
    }
    Services.obs.addObserver(observer, notification);
  });
}

function openChannelPromise(url, options = { loadUsingSystemPrincipal: true }) {
  let uri = Services.io.newURI(url);
  options.uri = uri;
  let chan = NetUtil.newChannel(options);
  let flags = CL_ALLOW_UNKNOWN_CL;
  if (options.expectFailure) {
    flags |= CL_EXPECT_FAILURE;
  }
  return new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req, buf) => resolve({ req, buf }), null, flags)
    );
  });
}

let backupServer;
const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

add_setup(async function setup() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "../unit/http2-ca.pem", "CTu,u,u");
  Services.prefs.setBoolPref("network.essential_domains_fallback", true);

  backupServer = new NodeHTTPSServer();
  await backupServer.start();
  registerCleanupFunction(async () => {
    await backupServer.stop();
  });
  await backupServer.registerPathHandler("/stuff", (req, res) => {
    res.end("Good stuff");
  });

  // Just so we can simulate mapping the default HTTPS port and domain.
  Services.prefs.setStringPref(
    "network.socket.forcePort",
    `443=${backupServer.port()}`
  );

  Services.io.addEssentialDomainMapping("aus5.mozilla.org", "foo.example.com");

  let ncs = Cc[
    "@mozilla.org/network/network-connectivity-service;1"
  ].getService(Ci.nsINetworkConnectivityService);
  ncs.IPv4 = Ci.nsINetworkConnectivityService.OK;
});

add_task(async function test_fallback_on_dns_failure() {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  Services.dns.clearCache(true);
  override.addIPOverride("aus5.mozilla.org", "N/A");
  override.addIPOverride("foo.example.com", "127.0.0.1");

  let { buf } = await openChannelPromise("https://aus5.mozilla.org/stuff");
  equal(buf, "Good stuff");
});

add_task(async function test_fallback_on_tls_failure() {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  Services.dns.clearCache(true);

  override.clearOverrides();
  override.addIPOverride("aus5.mozilla.org", "127.0.0.1");
  override.addIPOverride("foo.example.com", "127.0.0.1");
  let { buf } = await openChannelPromise("https://aus5.mozilla.org/stuff");
  equal(buf, "Good stuff");
});

add_task(async function test_no_fallback_with_content_principal() {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  Services.dns.clearCache(true);

  let { req } = await openChannelPromise("https://aus5.mozilla.org/stuff", {
    loadingPrincipal: Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI("https://aus5.mozilla.org"),
      {}
    ),
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    expectFailure: true,
  });
  // "NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SECURITY, SSL_ERROR_BAD_CERT_DOMAIN)"
  equal(req.status, 0x805a2ff4);
});

add_task(async function test_fallback_on_connection_failure() {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  Services.dns.clearCache(true);

  Services.prefs.setIntPref("network.http.connection-timeout", 1);
  override.clearOverrides();
  override.addIPOverride("aus5.mozilla.org", "169.254.200.200"); // Local IP that doesn't exist.
  override.addIPOverride("foo.example.com", "127.0.0.1");

  await new Promise(resolve => do_timeout(100, resolve));
  let chanNotif = waitForNotificationPromise("httpchannel-fallback");
  let chanPromise = openChannelPromise("https://aus5.mozilla.org/stuff");
  let chan = await chanNotif;
  equal(chan.status, Cr.NS_ERROR_NET_TIMEOUT);
  let { buf } = await chanPromise;
  equal(buf, "Good stuff");
});
