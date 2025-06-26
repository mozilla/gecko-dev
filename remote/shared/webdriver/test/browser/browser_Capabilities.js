/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
const { Proxy } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs"
);

add_task(async function test_global_manual_http_proxy() {
  await SpecialPowers.pushPrefEnv({
    set: [["network.proxy.allow_hijacking_localhost", true]],
  });

  const server = new HttpServer();
  server.start(-1);
  server.registerPathHandler("/", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", true);
    response.write("Not proxied");
  });
  const SERVER_URL = `http://localhost:${server.identity.primaryPort}`;

  const proxyServer = new HttpServer();
  proxyServer.start(-1);
  proxyServer.identity.add("http", "localhost", server.identity.primaryPort);
  proxyServer.registerPathHandler("/", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", true);
    response.write("Proxied");
  });
  const PROXY_URL = `localhost:${proxyServer.identity.primaryPort}`;

  const tab = BrowserTestUtils.addTab(gBrowser, SERVER_URL);
  const browser = tab.linkedBrowser;

  info("Verify that navigation request is not proxied");
  let loadedPromise = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, SERVER_URL);
  await loadedPromise;

  await SpecialPowers.spawn(browser, [], async () => {
    Assert.ok(
      content.document.body.textContent === "Not proxied",
      "The page was not proxied"
    );
  });

  info("Set the global manual proxy");

  const globalProxy = Proxy.fromJSON({
    proxyType: "manual",
    httpProxy: PROXY_URL,
  });

  globalProxy.init();

  info("Verify that navigation request is proxied");
  loadedPromise = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, SERVER_URL);
  await loadedPromise;

  await SpecialPowers.spawn(browser, [], async () => {
    Assert.ok(
      content.document.body.textContent === "Proxied",
      "The page was proxied"
    );
  });

  info("Destroy the proxy configuration");

  globalProxy.destroy();

  info("Verify that navigation request is not proxied");
  loadedPromise = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, SERVER_URL);
  await loadedPromise;

  await SpecialPowers.spawn(browser, [], async () => {
    Assert.ok(
      content.document.body.textContent === "Not proxied",
      "The page was not proxied"
    );
  });

  BrowserTestUtils.removeTab(tab);

  server.stop();
  proxyServer.stop();
});
