/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);
const mockNetwork = Cc[
  "@mozilla.org/network/mock-network-controller;1"
].getService(Ci.nsIMockNetworkLayerController);

let h3Port;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

function setup() {
  h3Port = Services.env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  Services.prefs.setBoolPref("network.socket.attach_mock_network_layer", true);
}

setup();
registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
  Services.prefs.clearUserPref(
    "network.dns.echconfig.fallback_to_origin_when_all_failed"
  );
  Services.prefs.clearUserPref("network.dns.httpssvc.reset_exclustion_list");
  Services.prefs.clearUserPref("network.http.http3.enable");
  Services.prefs.clearUserPref(
    "network.dns.httpssvc.http3_fast_fallback_timeout"
  );
  Services.prefs.clearUserPref(
    "network.http.http3.alt-svc-mapping-for-testing"
  );
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.http.http3.use_nspr_for_io");
  Services.prefs.clearUserPref("network.dns.preferIPv6");
  Services.prefs.clearUserPref(
    "network.http.move_to_pending_list_after_network_change"
  );
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
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

// Test the failure of an HTTP/3 connection when a network change occurs,
// specifically ensuring that blocking UDP I/O leads to the expected
// connection failure.
add_task(async function test_h3_with_network_change_fail() {
  Services.prefs.setBoolPref("network.http.http3.use_nspr_for_io", true);
  Services.prefs.setBoolPref(
    "network.http.move_to_pending_list_after_network_change",
    false
  );
  await http3_setup_tests("h3");

  let chan = makeChan(`https://foo.example.com:${h3Port}`);
  let [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.protocolVersion, "h3");

  // Blocking the UDP I/O to simulate a network change.
  let addr = mockNetwork.createScriptableNetAddr("127.0.0.1", h3Port);
  mockNetwork.blockUDPAddrIO(addr);
  registerCleanupFunction(async () => {
    mockNetwork.clearBlockedUDPAddr();
  });

  Services.obs.notifyObservers(null, "network:link-status-changed", "changed");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  // This request will fail because the HTTP/3 connection is reused.
  chan = makeChan(`https://foo.example.com:${h3Port}`);
  [req] = await channelOpenPromise(chan, CL_EXPECT_FAILURE);
});

add_task(async function test_h3_with_network_change_success() {
  Services.prefs.setBoolPref("network.http.http3.use_nspr_for_io", true);
  Services.prefs.setBoolPref(
    "network.http.move_to_pending_list_after_network_change",
    true
  );

  h3Port = await create_h3_server();
  Services.prefs.setCharPref(
    "network.http.http3.alt-svc-mapping-for-testing",
    `foo.example.com;h3=:${h3Port}`
  );

  let chan = makeChan(`https://foo.example.com:${h3Port}`);
  let [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.protocolVersion, "h3");

  let addr = mockNetwork.createScriptableNetAddr("127.0.0.1", h3Port);
  mockNetwork.blockUDPAddrIO(addr);
  registerCleanupFunction(async () => {
    mockNetwork.clearBlockedUDPAddr();
  });

  Services.obs.notifyObservers(null, "network:link-status-changed", "changed");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  // Make sure the host name can be resolved to an IPv6 address, since
  // 127.0.0.1 is blocked.
  Services.prefs.setBoolPref("network.dns.preferIPv6", true);
  Services.prefs.setBoolPref("network.dns.disableIPv6", false);
  Services.dns.clearCache(true);

  chan = makeChan(`https://foo.example.com:${h3Port}`);
  [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  Assert.equal(req.protocolVersion, "h3");
});
