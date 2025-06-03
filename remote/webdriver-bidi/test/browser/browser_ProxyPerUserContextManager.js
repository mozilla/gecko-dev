/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ContextualIdentityListener } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/listeners/ContextualIdentityListener.sys.mjs"
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
const { Proxy } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs"
);

const { ProxyPerUserContextManager } = ChromeUtils.importESModule(
  "chrome://remote/content/webdriver-bidi/ProxyPerUserContextManager.sys.mjs"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["network.proxy.allow_hijacking_localhost", true]],
  });
});

add_task(async function test_manual_http_proxy_per_user_context() {
  const [serverURL, proxyURL] = createHTTPProxy();

  const proxyManager = new ProxyPerUserContextManager();

  const userContext = ContextualIdentityService.create("test_name");
  const { userContextId } = userContext;

  const tabInUserContext = BrowserTestUtils.addTab(gBrowser, serverURL, {
    userContextId,
  });
  const browserInUserContext = tabInUserContext.linkedBrowser;

  info("Set up a manual proxy for user context");
  const proxyConfiguration = Proxy.fromJSON({
    proxyType: "manual",
    httpProxy: proxyURL,
  });
  proxyManager.addConfiguration(userContextId, proxyConfiguration);

  info("Verify that navigation request in user context is proxied");
  await isPageProxied(browserInUserContext, serverURL);

  const tabInDefaultUserContext = BrowserTestUtils.addTab(gBrowser, serverURL);
  const browserInDefaultUserContext = tabInDefaultUserContext.linkedBrowser;

  info("Verify that navigation request in default user context is not proxied");
  await isPageNotProxied(browserInDefaultUserContext, serverURL);

  info("Destroy proxy manager");
  proxyManager.destroy();

  info("Verify that navigation request in user context is not proxied");
  await isPageNotProxied(browserInUserContext, serverURL);

  BrowserTestUtils.removeTab(tabInUserContext);
  ContextualIdentityService.remove(userContextId);
  BrowserTestUtils.removeTab(tabInDefaultUserContext);
});

add_task(async function test_manual_http_proxy_per_user_context_with_noProxy() {
  const [serverURL, proxyURL] = createHTTPProxy();

  const proxyManager = new ProxyPerUserContextManager();

  const userContext = ContextualIdentityService.create("test_name");
  const { userContextId } = userContext;

  const tabInUserContext = BrowserTestUtils.addTab(gBrowser, serverURL, {
    userContextId,
  });
  const browserInUserContext = tabInUserContext.linkedBrowser;

  info("Set up a manual proxy for user context with `noProxy` field");
  const proxyConfiguration = Proxy.fromJSON({
    proxyType: "manual",
    httpProxy: proxyURL,
    noProxy: ["localhost"],
  });
  proxyManager.addConfiguration(userContextId, proxyConfiguration);

  info("Verify that navigation request is not proxied");
  await isPageNotProxied(browserInUserContext, serverURL);

  proxyManager.destroy();

  BrowserTestUtils.removeTab(tabInUserContext);
  ContextualIdentityService.remove(userContextId);
});

add_task(async function test_override_manual_http_proxy_per_user_context() {
  const [serverURL, proxyURL] = createHTTPProxy();

  const proxyManager = new ProxyPerUserContextManager();

  const userContext = ContextualIdentityService.create("test_name");
  const { userContextId } = userContext;

  const tabInUserContext = BrowserTestUtils.addTab(gBrowser, serverURL, {
    userContextId,
  });
  const browserInUserContext = tabInUserContext.linkedBrowser;

  info("Set up a manual proxy for user context");
  const proxyConfiguration = Proxy.fromJSON({
    proxyType: "manual",
    httpProxy: proxyURL,
  });
  proxyManager.addConfiguration(userContextId, proxyConfiguration);

  info("Verify that navigation request is proxied");
  await isPageProxied(browserInUserContext, serverURL);

  info("Set a new proxy configuration for user context with direct proxy");
  const newProxyConfiguration = Proxy.fromJSON({
    proxyType: "direct",
  });
  proxyManager.addConfiguration(userContextId, newProxyConfiguration);

  info("Verify that navigation request is not proxied");
  await isPageNotProxied(browserInUserContext, serverURL);

  proxyManager.destroy();

  BrowserTestUtils.removeTab(tabInUserContext);
  ContextualIdentityService.remove(userContextId);
});

add_task(
  async function test_override_global_manual_proxy_with_direct_proxy_per_user_context() {
    const [serverURL, proxyURL] = createHTTPProxy();

    info("Set up a global manual proxy");
    const globalProxy = Proxy.fromJSON({
      proxyType: "manual",
      httpProxy: proxyURL,
    });

    globalProxy.init();

    const tabInDefaultUserContext = BrowserTestUtils.addTab(
      gBrowser,
      serverURL
    );
    const browserInDefaultUserContext = tabInDefaultUserContext.linkedBrowser;

    info("Verify that navigation request is proxied");
    await isPageProxied(browserInDefaultUserContext, serverURL);

    const proxyManager = new ProxyPerUserContextManager();

    const userContext = ContextualIdentityService.create("test_name");
    const { userContextId } = userContext;

    const tabInUserContext = BrowserTestUtils.addTab(gBrowser, serverURL, {
      userContextId,
    });
    const browserInUserContext = tabInUserContext.linkedBrowser;

    info("Set up a direct proxy for user context");
    const proxyConfiguration = Proxy.fromJSON({
      proxyType: "direct",
    });
    proxyManager.addConfiguration(userContextId, proxyConfiguration);

    info("Verify that navigation request is not proxied");
    await isPageNotProxied(browserInUserContext, serverURL);

    proxyManager.destroy();

    globalProxy.destroy();

    BrowserTestUtils.removeTab(tabInUserContext);
    ContextualIdentityService.remove(userContextId);
    BrowserTestUtils.removeTab(tabInDefaultUserContext);
  }
);

add_task(
  async function test_override_global_system_proxy_with_direct_proxy_per_user_context() {
    const [serverURL] = createHTTPProxy();

    info("Set up a global system proxy");
    const globalProxy = Proxy.fromJSON({
      proxyType: "system",
    });

    globalProxy.init();

    const proxyManager = new ProxyPerUserContextManager();

    const userContext = ContextualIdentityService.create("test_name");
    const { userContextId } = userContext;

    const tabInUserContext = BrowserTestUtils.addTab(gBrowser, serverURL, {
      userContextId,
    });
    const browserInUserContext = tabInUserContext.linkedBrowser;

    info("Set up a direct proxy for user context");
    const proxyConfiguration = Proxy.fromJSON({
      proxyType: "direct",
    });

    proxyManager.addConfiguration(userContextId, proxyConfiguration);

    info("Verify that navigation request is not proxied");
    await isPageNotProxied(browserInUserContext, serverURL);

    proxyManager.destroy();

    globalProxy.destroy();

    BrowserTestUtils.removeTab(tabInUserContext);
    ContextualIdentityService.remove(userContextId);
  }
);

add_task(
  async function test_delete_configuration_for_manual_http_proxy_per_user_context() {
    const [serverURL, proxyURL] = createHTTPProxy();

    const proxyManager = new ProxyPerUserContextManager();

    const userContext = ContextualIdentityService.create("test_name");
    const { userContextId } = userContext;

    const tabInUserContext = BrowserTestUtils.addTab(gBrowser, serverURL, {
      userContextId,
    });
    const browserInUserContext = tabInUserContext.linkedBrowser;

    info("Set up a manual proxy for user context");
    const proxyConfiguration = Proxy.fromJSON({
      proxyType: "manual",
      httpProxy: proxyURL,
    });
    proxyManager.addConfiguration(userContextId, proxyConfiguration);

    info("Verify that navigation request is proxied");
    await isPageProxied(browserInUserContext, serverURL);

    info("Delete the proxy configuration");
    proxyManager.deleteConfiguration(userContextId);

    info("Verify that navigation request is not proxied");
    await isPageNotProxied(browserInUserContext, serverURL);

    info("Try to delete the configuration again");
    proxyManager.deleteConfiguration(userContextId);

    proxyManager.destroy();

    BrowserTestUtils.removeTab(tabInUserContext);
    ContextualIdentityService.remove(userContextId);
  }
);

function createHTTPProxy() {
  const server = new HttpServer();
  server.start(-1);
  server.registerPathHandler("/", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", true);
    response.write("Not proxied");
  });

  const proxyServer = new HttpServer();
  proxyServer.start(-1);
  proxyServer.identity.add("http", "localhost", server.identity.primaryPort);
  proxyServer.registerPathHandler("/", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", true);
    response.write("Proxied");
  });

  registerCleanupFunction(() => {
    server.stop();
    proxyServer.stop();
  });

  return [
    `http://localhost:${server.identity.primaryPort}`,
    `localhost:${proxyServer.identity.primaryPort}`,
  ];
}

async function isPageProxied(browser, url) {
  await loadPage(browser, url);
  return SpecialPowers.spawn(browser, [], async () => {
    Assert.ok(
      content.document.body.textContent === "Proxied",
      "The page was proxied"
    );
  });
}

async function isPageNotProxied(browser, url) {
  await loadPage(browser, url);
  return SpecialPowers.spawn(browser, [], async () => {
    Assert.ok(
      content.document.body.textContent === "Not proxied",
      "The page was not proxied"
    );
  });
}

async function loadPage(browser, url) {
  const loadedPromise = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, url);
  return loadedPromise;
}
