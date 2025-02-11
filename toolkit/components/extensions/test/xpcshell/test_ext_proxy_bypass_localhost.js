"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "authManager",
  "@mozilla.org/network/http-auth-manager;1",
  "nsIHttpAuthManager"
);

const gOverride = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

let gProxyReqCount = 0;
let gServerReqCount = 0;

const server = createHttpServer();
server.identity.add("http", "localhost", server.identity.primaryPort);
server.identity.add("http", "example.com", server.identity.primaryPort);

server.registerPathHandler("/", (request, response) => {
  gServerReqCount++;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
});

const proxy = createHttpServer();

// accept proxy connections for localhost
proxy.identity.add("http", "localhost", server.identity.primaryPort);
proxy.identity.add("http", "example.com", server.identity.primaryPort);

proxy.registerPathHandler("/", (request, response) => {
  gProxyReqCount++;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
});

function getExtension(background) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "webRequest", "webRequestBlocking", "<all_urls>"],
      host_permissions: [
        `http://localhost:${server.identity.primaryPort}/*`,
        `http://localhost/*`,
      ],
    },
    background: `(${background})(${proxy.identity.primaryPort})`,
  });
}

add_task(async function test_webRequest_auth_proxy() {
  function background(port) {
    browser.webRequest.onCompleted.addListener(
      details => {
        browser.test.log(`onCompleted ${JSON.stringify(details)}\n`);

        if (details.proxyInfo) {
          browser.test.assertEq(
            "localhost",
            details.proxyInfo.host,
            "proxy host"
          );
          browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
          browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
        }

        browser.test.sendMessage(
          "requestCompleted",
          details.proxyInfo ? "proxied" : "not_proxied"
        );
      },
      { urls: ["<all_urls>"] }
    );

    // Handle the proxy request.
    browser.proxy.onRequest.addListener(
      details => {
        browser.test.log(`onRequest ${JSON.stringify(details)}`);
        return [
          {
            host: "localhost",
            port,
            type: "http",
          },
        ];
      },
      { urls: ["<all_urls>"] },
      ["requestHeaders"]
    );
  }

  let extension = getExtension(background);

  await extension.startup();

  authManager.clearAll();

  // The extension always returns the proxy info
  // The first request is made with the allow_hijacking_localhost pref set to true
  // and checks that the request got proxied.
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  let proxyCount = gProxyReqCount;
  let serverCount = gServerReqCount;
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://localhost:${server.identity.primaryPort}/`
  );
  equal(await extension.awaitMessage("requestCompleted"), "proxied");
  equal(
    gProxyReqCount,
    proxyCount + 1,
    "Should see just one extra proxy request"
  );
  equal(gServerReqCount, serverCount, "Shouldn't see any unproxied requests");
  await contentPage.close();

  // The second request doesn't allow the webextension to force the
  // proxying of a localhost request.
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", false);

  proxyCount = gProxyReqCount;
  serverCount = gServerReqCount;
  contentPage = await ExtensionTestUtils.loadContentPage(
    `http://localhost:${server.identity.primaryPort}/`
  );
  equal(await extension.awaitMessage("requestCompleted"), "not_proxied");
  equal(gProxyReqCount, proxyCount, "Shouldn't see any proxy requests");
  equal(
    gServerReqCount,
    serverCount + 1,
    "Should have one extra server request"
  );
  await contentPage.close();

  // Make sure unproxied requests to not crash because they are not local.
  gOverride.addIPOverride("example.com", "127.0.0.1");

  proxyCount = gProxyReqCount;
  serverCount = gServerReqCount;
  contentPage = await ExtensionTestUtils.loadContentPage(
    `http://example.com:${server.identity.primaryPort}/`
  );
  equal(await extension.awaitMessage("requestCompleted"), "proxied");
  equal(gProxyReqCount, proxyCount + 1, "Should be proxied");
  equal(gServerReqCount, serverCount, "No extra direct requests");
  await contentPage.close();

  // Now check that the pref is respected by addon requests too.
  Services.prefs.setCharPref("network.proxy.no_proxies_on", "example.com");

  proxyCount = gProxyReqCount;
  serverCount = gServerReqCount;
  contentPage = await ExtensionTestUtils.loadContentPage(
    `http://example.com:${server.identity.primaryPort}/`
  );
  equal(await extension.awaitMessage("requestCompleted"), "not_proxied");
  equal(gProxyReqCount, proxyCount, "Shouldn't see any proxy requests");
  equal(
    gServerReqCount,
    serverCount + 1,
    "Should have one extra server request"
  );
  await contentPage.close();

  gOverride.clearOverrides();

  await extension.unload();
});
