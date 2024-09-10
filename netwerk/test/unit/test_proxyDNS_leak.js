/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test when socks proxy is registered, we don't try to resolve HTTPS record.
// Steps:
// 1. Use addHTTPSRecordOverride to add an override for service.com.
// 2. Add a proxy filter to use socks proxy.
// 3. Create a request to load service.com.
// 4. See if the HTTPS record is in DNS cache entries.

"use strict";

const gDashboard = Cc["@mozilla.org/network/dashboard;1"].getService(
  Ci.nsIDashboard
);
const pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();

add_task(async function setup() {
  Services.prefs.setBoolPref("network.dns.native_https_query", true);
  Services.prefs.setBoolPref("network.dns.native_https_query_win10", true);
  const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
    Ci.nsINativeDNSResolverOverride
  );

  let rawBuffer = [
    0, 0, 128, 0, 0, 0, 0, 1, 0, 0, 0, 0, 7, 115, 101, 114, 118, 105, 99, 101,
    3, 99, 111, 109, 0, 0, 65, 0, 1, 0, 0, 0, 55, 0, 13, 0, 1, 0, 0, 1, 0, 6, 2,
    104, 50, 2, 104, 51,
  ];
  override.addHTTPSRecordOverride("service.com", rawBuffer, rawBuffer.length);
  override.addIPOverride("service.com", "127.0.0.1");
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.dns.native_https_query");
    Services.prefs.clearUserPref("network.dns.native_https_query_win10");
    Services.prefs.clearUserPref("network.dns.localDomains");
    override.clearOverrides();
  });
});

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
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
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

async function isRecordFound(hostname) {
  return new Promise(resolve => {
    gDashboard.requestDNSInfo(function (data) {
      let found = false;
      for (let i = 0; i < data.entries.length; i++) {
        if (
          data.entries[i].hostname == hostname &&
          data.entries[i].type == Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC
        ) {
          found = true;
          break;
        }
      }
      resolve(found);
    });
  });
}

async function do_test_with_proxy_filter(filter) {
  pps.registerFilter(filter, 10);

  let chan = makeChan(`https://service.com/`);
  await channelOpenPromise(chan, CL_EXPECT_LATE_FAILURE | CL_ALLOW_UNKNOWN_CL);

  let found = await isRecordFound("service.com");
  pps.unregisterFilter(filter);

  return found;
}

add_task(async function test_proxyDNS_do_leak() {
  let filter = new NodeProxyFilter("socks", "localhost", 443, 0);

  let res = await do_test_with_proxy_filter(filter);

  Assert.ok(res, "Should find a DNS entry");
});

add_task(async function test_proxyDNS_dont_leak() {
  Services.dns.clearCache(false);

  let filter = new NodeProxyFilter(
    "socks",
    "localhost",
    443,
    Ci.nsIProxyInfo.TRANSPARENT_PROXY_RESOLVES_HOST
  );

  let res = await do_test_with_proxy_filter(filter);

  Assert.ok(!res, "Should not find a DNS entry");
});
