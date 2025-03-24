/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "IdentityCredentialStorageService",
  "@mozilla.org/browser/identity-credential-storage-service;1",
  "nsIIdentityCredentialStorageService"
);

const TEST_URL = "https://example.com/";

async function disconnect() {
  let promise = content.IdentityCredential.disconnect({
    configURL:
      "https://example.net/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
    clientId: "id",
    accountHint: "example",
  });
  try {
    return await promise;
  } catch (_) {
    return undefined;
  }
}

async function disconnectEndpointFailure() {
  let promise = content.IdentityCredential.disconnect({
    configURL:
      "https://example.net/browser/dom/credentialmanagement/identity/tests/browser/server_manifest_disconnect_failure.json",
    clientId: "id",
    accountHint: "example",
  });
  try {
    return await promise;
  } catch (_) {
    return undefined;
  }
}

async function disconnectManifestFailure() {
  let promise = content.IdentityCredential.disconnect({
    configURL:
      "https://example.net/browser/dom/credentialmanagement/identity/tests/browser/server_manifest_failure.json",
    clientId: "id",
    accountHint: "example",
  });
  try {
    return await promise;
  } catch (_) {
    return undefined;
  }
}

add_task(async function test_disconnect_identity_credential() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "dom.security.credentialmanagement.identity.select_first_in_ui_lists",
        true,
      ],
      [
        "dom.security.credentialmanagement.identity.reject_delay.enabled",
        false,
      ],
    ],
  });

  const idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.net"),
    {}
  );
  const rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.com"),
    {}
  );

  // Set two accounts as registered
  IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    "disconnected",
    true,
    false
  );
  IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    "still_connected",
    true,
    false
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  await SpecialPowers.spawn(tab.linkedBrowser, [], disconnect);

  let registered = {};
  let allowLogout = {};
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    "disconnected",
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should be unregistered by disconnect.");
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    "still_connected",
    registered,
    allowLogout
  );
  Assert.ok(
    registered.value,
    "Should be still registered by disconnect if not returned by server."
  );

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_disconnect_miss_identity_credential() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "dom.security.credentialmanagement.identity.select_first_in_ui_lists",
        true,
      ],
      [
        "dom.security.credentialmanagement.identity.reject_delay.enabled",
        false,
      ],
    ],
  });

  const idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.net"),
    {}
  );
  const rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.com"),
    {}
  );

  // Set only one account as registered
  this.IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    "no_longer_connected",
    true,
    false
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  await SpecialPowers.spawn(tab.linkedBrowser, [], disconnect);

  let registered = {};
  let allowLogout = {};
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    "no_longer_connected",
    registered,
    allowLogout
  );
  Assert.ok(
    !registered.value,
    "Should not be still registered by disconnect if the disconnect missed."
  );

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_disconnect_on_disconnect_failure() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "dom.security.credentialmanagement.identity.select_first_in_ui_lists",
        true,
      ],
      [
        "dom.security.credentialmanagement.identity.reject_delay.enabled",
        false,
      ],
    ],
  });

  const idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.net"),
    {}
  );
  const rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.com"),
    {}
  );

  // Set two accounts as registered
  this.IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    "disconnected",
    true,
    false
  );
  this.IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    "not_still_connected",
    true,
    false
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  await SpecialPowers.spawn(tab.linkedBrowser, [], disconnectEndpointFailure);

  let registered = {};
  let allowLogout = {};
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    "disconnected",
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should be unregistered by disconnect.");
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    "not_still_connected",
    registered,
    allowLogout
  );
  Assert.ok(
    !registered.value,
    "Should not be still registered because disconnect failed."
  );

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_nothing_on_manifest_failure() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "dom.security.credentialmanagement.identity.select_first_in_ui_lists",
        true,
      ],
      [
        "dom.security.credentialmanagement.identity.reject_delay.enabled",
        false,
      ],
    ],
  });

  const idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.net"),
    {}
  );
  const rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.com"),
    {}
  );

  // Set two accounts as registered
  this.IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    "connected",
    true,
    false
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  await SpecialPowers.spawn(tab.linkedBrowser, [], disconnectManifestFailure);

  let registered = {};
  let allowLogout = {};
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    "connected",
    registered,
    allowLogout
  );
  Assert.ok(
    registered.value,
    "Should be registered because the disconnect failed finding a manifest"
  );

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
