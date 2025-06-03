/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

let trrServer;
let server;
let preflightCache;

add_setup(async function () {
  trr_test_setup();

  trrServer = new TRRServer();
  await trrServer.start();

  await trrServer.registerDoHAnswers("alt1.example.com", "A", {
    answers: [
      {
        name: "alt1.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });
  await trrServer.registerDoHAnswers("alt2.example.com", "A", {
    answers: [
      {
        name: "alt2.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  preflightCache = Cc["@mozilla.org/network/cors-preflight-cache;1"].getService(
    Ci.nsICORSPreflightCache
  );

  server = new NodeHTTP2Server();
  await server.start();
  await server.registerPathHandler("/", (req, resp) => {
    resp.setHeader("Access-Control-Allow-Methods", "PUT");
    resp.setHeader("Access-Control-Allow-Origin", "https://example.org");
    resp.writeHead(200);
    resp.end(global.server_name);
  });

  registerCleanupFunction(async () => {
    trr_clear_prefs();
    if (trrServer) {
      await trrServer.stop();
    }
    await server.stop();
  });
});

function createCORSRequest(corsURI) {
  let uri = NetUtil.newURI(corsURI);
  let principal = Services.scriptSecurityManager.createContentPrincipal(uri, {
    firstPartyDomain: "https://example.org",
  });
  let channel = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  }).QueryInterface(Ci.nsIHttpChannel);
  channel.requestMethod = "PUT";
  let triggeringPrincipal =
    Services.scriptSecurityManager.createContentPrincipal(
      NetUtil.newURI("https://example.org/"),
      {
        firstPartyDomain: "https://example.org",
      }
    );

  channel.loadInfo.setTriggeringPrincipalForTesting(triggeringPrincipal);
  return [channel, principal];
}

async function runCORSTest({
  uri,
  expectedPreflightCount,
  beforeSecondRequest = () => {},
}) {
  let preflightRequestCount = 0;

  const observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic) {
      aSubject = aSubject.QueryInterface(Ci.nsIHttpChannel);
      if (aTopic === "http-on-before-connect" && aSubject.URI.spec === uri) {
        dump("aSubject.requestMethod:" + aSubject.requestMethod + "\n");
        if (aSubject.requestMethod === "OPTIONS") {
          preflightRequestCount++;
        }
      }
    },
  };

  Services.obs.addObserver(observer, "http-on-before-connect");

  // First request
  let [channel, principal] = createCORSRequest(uri);
  await channelOpenPromise(channel, CL_ALLOW_UNKNOWN_CL);

  let entries = preflightCache.getEntries(principal);
  Assert.equal(entries.length, 1);
  Assert.equal(entries[0].URI.asciiSpec, uri);

  await beforeSecondRequest();

  // Second request
  let [secondChannel] = createCORSRequest(uri);
  await channelOpenPromise(secondChannel, CL_ALLOW_UNKNOWN_CL);

  Assert.equal(preflightRequestCount, expectedPreflightCount);

  Services.obs.removeObserver(observer, "http-on-before-connect");
}

add_task(async function test_cors_with_valid_dns_cache() {
  const corsURI = `https://alt1.example.com:${server.port()}/`;
  await runCORSTest({
    uri: corsURI,
    expectedPreflightCount: 1,
  });
});

add_task(async function test_cors_without_valid_dns_cache() {
  const corsURI = `https://alt2.example.com:${server.port()}/`;
  Services.dns.clearCache(true); // clear before first request
  await runCORSTest({
    uri: corsURI,
    expectedPreflightCount: 2,
    beforeSecondRequest: async () => {
      Services.dns.clearCache(true);
    },
  });
});

add_task(async function test_cors_with_dns_cache_changed() {
  const corsURI = `https://alt2.example.com:${server.port()}/`;
  Services.dns.clearCache(true); // clear before first request
  await runCORSTest({
    uri: corsURI,
    expectedPreflightCount: 2,
    beforeSecondRequest: async () => {
      Services.dns.clearCache(true);
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 500));

      await trrServer.registerDoHAnswers("alt2.example.com", "A", {
        answers: [
          {
            name: "alt2.example.com",
            ttl: 55,
            type: "A",
            flush: false,
            data: "127.0.0.1",
          },
          {
            name: "alt2.example.com",
            ttl: 55,
            type: "A",
            flush: false,
            data: "127.0.0.3",
          },
        ],
      });

      const oa = {
        firstPartyDomain: "https://example.org",
      };
      await new TRRDNSListener("alt2.example.com", {
        expectedAnswer: "127.0.0.1",
        originAttributes: oa,
      });
    },
  });
});
