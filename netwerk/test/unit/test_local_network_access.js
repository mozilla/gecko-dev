"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function makeChannel(url) {
  let uri2 = NetUtil.newURI(url);
  // by default system principal is used, which cannot be used for permission based tests
  // because the default system principal has all permissions
  var principal = Services.scriptSecurityManager.createContentPrincipal(
    uri2,
    {}
  );
  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  }).QueryInterface(Ci.nsIHttpChannel);
}

var ChannelCreationObserver = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
  observe(aSubject, aTopic) {
    if (aTopic == "http-on-opening-request") {
      var chan = aSubject.QueryInterface(Ci.nsIHttpChannel);
      if (chan.URI.spec.includes("test_lna_social_tracker")) {
        chan.loadInfo.triggeringThirdPartyClassificationFlags =
          Ci.nsIClassifiedChannel.CLASSIFIED_ANY_SOCIAL_TRACKING;
      } else if (chan.URI.spec.includes("test_lna_basic_tracker")) {
        chan.loadInfo.triggeringThirdPartyClassificationFlags =
          Ci.nsIClassifiedChannel.CLASSIFIED_ANY_BASIC_TRACKING;
      } else if (chan.URI.spec.includes("test_lna_content_tracker")) {
        chan.loadInfo.triggeringThirdPartyClassificationFlags =
          Ci.nsIClassifiedChannel.CLASSIFIED_TRACKING_CONTENT;
      }
    }
  },
};

ChromeUtils.defineLazyGetter(this, "H1_URL", function () {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

ChromeUtils.defineLazyGetter(this, "H2_URL", function () {
  return "https://localhost:" + server.port();
});

let httpServer = null;
let server = new NodeHTTP2Server();
function pathHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  let body = "success";
  response.bodyOutputStream.write(body, body.length);
}

add_setup(async () => {
  Services.prefs.setBoolPref("network.lna.block_trackers", true);
  Services.obs.addObserver(ChannelCreationObserver, "http-on-opening-request");
  // H1 Server
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/test_lna", pathHandler);
  httpServer.start(-1);

  // H2 Server
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  await server.start();
  registerCleanupFunction(async () => {
    try {
      await server.stop();
      await httpServer.stop();
    } catch (e) {
      // Ignore errors during cleanup
      info("Error during cleanup:", e);
    }
  });
  await server.registerPathHandler("/test_lna", (req, resp) => {
    let content = `ok`;
    resp.writeHead(200, {
      "Content-Type": "text/plain",
      "Content-Length": `${content.length}`,
    });
    resp.end(content);
  });
});

add_task(async function lna_blocking_tests() {
  // Array of test cases:
  // [blockingEnabled, ipAddressSpace, urlSuffix, expectedStatus, port]
  const testCases = [
    [
      true,
      Ci.nsILoadInfo.Public,
      "/test_lna",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H1_URL,
    ],
    [
      true,
      Ci.nsILoadInfo.Private,
      "/test_lna",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H1_URL,
    ],
    [true, Ci.nsILoadInfo.Local, "/test_lna", Cr.NS_OK, H1_URL],
    [false, Ci.nsILoadInfo.Public, "/test_lna", Cr.NS_OK, H1_URL],
    [false, Ci.nsILoadInfo.Private, "/test_lna", Cr.NS_OK, H1_URL],
    [false, Ci.nsILoadInfo.Local, "/test_lna", Cr.NS_OK, H1_URL],
    [
      true,
      Ci.nsILoadInfo.Public,
      "/test_lna",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H2_URL,
    ],
    [
      true,
      Ci.nsILoadInfo.Private,
      "/test_lna",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H2_URL,
    ],
    [true, Ci.nsILoadInfo.Local, "/test_lna", Cr.NS_OK, H2_URL],
    [false, Ci.nsILoadInfo.Public, "/test_lna", Cr.NS_OK, H2_URL],
    [false, Ci.nsILoadInfo.Private, "/test_lna", Cr.NS_OK, H2_URL],
    [false, Ci.nsILoadInfo.Local, "/test_lna", Cr.NS_OK, H2_URL],
    // Test cases for local network access from trackers
    // NO LNA then request should not be blocked
    [false, Ci.nsILoadInfo.Local, "/test_lna_basic_tracker", Cr.NS_OK, H2_URL],
    [false, Ci.nsILoadInfo.Local, "/test_lna_social_tracker", Cr.NS_OK, H2_URL],
    [
      false,
      Ci.nsILoadInfo.Local,
      "/test_lna_content_tracker",
      Cr.NS_OK,
      H2_URL,
    ],
    [
      false,
      Ci.nsILoadInfo.Public,
      "/test_lna_basic_tracker",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H2_URL,
    ],
    [
      false,
      Ci.nsILoadInfo.Public,
      "/test_lna_social_tracker",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H2_URL,
    ],
    [
      false,
      Ci.nsILoadInfo.Public,
      "/test_lna_content_tracker",
      Cr.NS_OK,
      H2_URL,
    ],
    [
      false,
      Ci.nsILoadInfo.Private,
      "/test_lna_basic_tracker",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H2_URL,
    ],
    [
      false,
      Ci.nsILoadInfo.Private,
      "/test_lna_social_tracker",
      Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
      H2_URL,
    ],
    [
      false,
      Ci.nsILoadInfo.Private,
      "/test_lna_content_tracker",
      Cr.NS_OK,
      H2_URL,
    ],
  ];

  for (let [blocking, space, suffix, expectedStatus, url] of testCases) {
    info(`do_test ${url}${suffix}, ${space} -> ${expectedStatus}`);

    Services.prefs.setBoolPref("network.lna.blocking", blocking);

    let chan = makeChannel(url + suffix);
    chan.loadInfo.parentIpAddressSpace = space;

    let expectFailure = expectedStatus !== Cr.NS_OK ? CL_EXPECT_FAILURE : 0;

    await new Promise(resolve => {
      chan.asyncOpen(new ChannelListener(resolve, null, expectFailure));
    });

    Assert.equal(chan.status, expectedStatus);
    if (expectedStatus === Cr.NS_OK) {
      Assert.equal(chan.protocolVersion, url === H1_URL ? "http/1.1" : "h2");
    }
  }
});
