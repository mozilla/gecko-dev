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

add_task(
  async function test_passive_filters_out_not_connected_idp_and_leaves_a_connected() {
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
      "connected",
      true,
      false
    );

    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

    let expectFailure = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      async function () {
        let promise = content.navigator.credentials.get({
          identity: {
            mode: "passive",
            providers: [
              {
                configURL:
                  "https://example.org/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
                clientId: "browser",
                nonce: "nonce",
              },
            ],
          },
        });
        try {
          let cred = await promise;
          return cred.token;
        } catch (err) {
          return err;
        }
      }
    );

    // If this were example.org, we didn't filter it out because it was was connected
    ok(expectFailure, "expect a result from the second request.");
    ok(expectFailure.name, "expect a DOMException which must have a name.");

    let sameSiteResult = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      async function () {
        let promise = content.navigator.credentials.get({
          identity: {
            mode: "passive",
            providers: [
              {
                configURL:
                  "https://example.org/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
                clientId: "browser",
                nonce: "nonce",
              },
              {
                configURL:
                  "https://www.example.com/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
                clientId: "browser",
                nonce: "nonce",
              },
            ],
          },
        });
        try {
          let cred = await promise;
          return cred.token;
        } catch (err) {
          return err;
        }
      }
    );

    // If this is the right token, we didn't filter out an IDP because it was was connected
    is(sameSiteResult, "result", "Result obtained!");

    let connectedResult = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      async function () {
        let promise = content.navigator.credentials.get({
          identity: {
            mode: "passive",
            providers: [
              {
                configURL:
                  "https://example.org/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
                clientId: "browser",
                nonce: "nonce",
              },
              {
                configURL:
                  "https://example.net/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
                clientId: "browser",
                nonce: "nonce",
              },
            ],
          },
        });
        try {
          let cred = await promise;
          return cred.token;
        } catch (err) {
          return err;
        }
      }
    );

    // If this is the right token, we didn't filter out an IDP because it was was connected
    is(connectedResult, "result", "Result obtained!");

    // Close tabs.
    await BrowserTestUtils.removeTab(tab);
    await SpecialPowers.popPrefEnv();
  }
);
