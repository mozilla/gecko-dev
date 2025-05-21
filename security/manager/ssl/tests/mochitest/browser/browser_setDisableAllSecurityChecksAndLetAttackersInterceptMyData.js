/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ContextualIdentityListener } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/listeners/ContextualIdentityListener.sys.mjs"
);

const PAGE_URL = "https://example.com/";
const PAGE_WITH_EXPIRED_CERT = "https://expired.example.com/";

add_task(async function test_disable_all_security_checks_globally() {
  const certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  const browser = gBrowser.selectedBrowser;

  info("Loading and waiting for the cert error");
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(browser);
  BrowserTestUtils.startLoadingURIString(browser, PAGE_WITH_EXPIRED_CERT);
  await errorPageLoaded;

  Assert.ok(true, "Error page is loaded");

  info("Disable security checks");
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  const loadedPromise = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, PAGE_WITH_EXPIRED_CERT);
  await loadedPromise;
  Assert.ok(true, "Normal page is loaded");

  info("Enable security checks");
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );

  errorPageLoaded = BrowserTestUtils.waitForErrorPage(browser);
  BrowserTestUtils.startLoadingURIString(browser, PAGE_WITH_EXPIRED_CERT);
  await errorPageLoaded;
  Assert.ok(true, "Error page is loaded");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_disable_all_security_checks_for_user_context() {
  const certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);

  const userContext = ContextualIdentityService.create("test_name");
  const { userContextId } = userContext;

  const tabInUserContext = BrowserTestUtils.addTab(gBrowser, PAGE_URL, {
    userContextId,
  });
  const browserInUserContext = tabInUserContext.linkedBrowser;

  info("Loading and waiting for the cert error in user context");
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(browserInUserContext);
  BrowserTestUtils.startLoadingURIString(
    browserInUserContext,
    PAGE_WITH_EXPIRED_CERT
  );
  await errorPageLoaded;

  Assert.ok(true, "Error page is loaded");

  info("Disable security checks for user context");
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyDataForUserContext(
    userContextId,
    true
  );

  let loadedPromise = BrowserTestUtils.browserLoaded(browserInUserContext);
  BrowserTestUtils.startLoadingURIString(
    browserInUserContext,
    PAGE_WITH_EXPIRED_CERT
  );
  await loadedPromise;
  Assert.ok(true, "Normal page is loaded in user context");

  const tabInDefaultUserContext = BrowserTestUtils.addTab(gBrowser, PAGE_URL);
  const browserInDefaultUserContext = tabInDefaultUserContext.linkedBrowser;

  info("Loading and waiting for the cert error in default user context");
  errorPageLoaded = BrowserTestUtils.waitForErrorPage(
    browserInDefaultUserContext
  );
  BrowserTestUtils.startLoadingURIString(
    browserInDefaultUserContext,
    PAGE_WITH_EXPIRED_CERT
  );
  await errorPageLoaded;
  Assert.ok(true, "Error page is loaded");

  BrowserTestUtils.removeTab(tabInUserContext);
  BrowserTestUtils.removeTab(tabInDefaultUserContext);
  ContextualIdentityService.remove(userContextId);
});

add_task(
  async function test_disable_all_security_checks_globally_enable_for_user_context() {
    const certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);

    const tabInDefaultUserContext = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      PAGE_URL
    );
    const browser = gBrowser.selectedBrowser;

    info("Disable security checks");
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      true
    );

    let loadedPromise = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.startLoadingURIString(browser, PAGE_WITH_EXPIRED_CERT);
    await loadedPromise;
    Assert.ok(true, "Normal page is loaded");

    const userContext = ContextualIdentityService.create("test_name");
    const { userContextId } = userContext;

    const tabInUserContext = BrowserTestUtils.addTab(gBrowser, PAGE_URL, {
      userContextId,
    });
    const browserInUserContext = tabInUserContext.linkedBrowser;

    info("Enable security checks for user context");
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyDataForUserContext(
      userContextId,
      false
    );

    info("Loading and waiting for the cert error in user context");
    const errorPageLoaded =
      BrowserTestUtils.waitForErrorPage(browserInUserContext);
    BrowserTestUtils.startLoadingURIString(
      browserInUserContext,
      PAGE_WITH_EXPIRED_CERT
    );
    await errorPageLoaded;

    Assert.ok(true, "Error page is loaded");

    BrowserTestUtils.removeTab(tabInDefaultUserContext);
    BrowserTestUtils.removeTab(tabInUserContext);
    ContextualIdentityService.remove(userContextId);

    info("Reenable security checks");
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
  }
);

add_task(
  async function test_reset_disable_all_security_checks_for_user_context() {
    const certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);

    const userContext = ContextualIdentityService.create("test_name");
    const { userContextId } = userContext;

    const tabInUserContext = BrowserTestUtils.addTab(gBrowser, PAGE_URL, {
      userContextId,
    });
    const browserInUserContext = tabInUserContext.linkedBrowser;

    info("Loading and waiting for the cert error in user context");
    let errorPageLoaded =
      BrowserTestUtils.waitForErrorPage(browserInUserContext);
    BrowserTestUtils.startLoadingURIString(
      browserInUserContext,
      PAGE_WITH_EXPIRED_CERT
    );
    await errorPageLoaded;

    Assert.ok(true, "Error page is loaded");

    info("Disable security checks for user context");
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyDataForUserContext(
      userContextId,
      true
    );

    let loadedPromise = BrowserTestUtils.browserLoaded(browserInUserContext);
    BrowserTestUtils.startLoadingURIString(
      browserInUserContext,
      PAGE_WITH_EXPIRED_CERT
    );
    await loadedPromise;
    Assert.ok(true, "Normal page is loaded in user context");

    info("Reset disable security checks for user context");
    certOverrideService.resetDisableAllSecurityChecksAndLetAttackersInterceptMyDataForUserContext(
      userContextId
    );

    errorPageLoaded = BrowserTestUtils.waitForErrorPage(browserInUserContext);
    BrowserTestUtils.startLoadingURIString(
      browserInUserContext,
      PAGE_WITH_EXPIRED_CERT
    );
    await errorPageLoaded;

    Assert.ok(true, "Error page is loaded");

    BrowserTestUtils.removeTab(tabInUserContext);
    ContextualIdentityService.remove(userContextId);
  }
);
