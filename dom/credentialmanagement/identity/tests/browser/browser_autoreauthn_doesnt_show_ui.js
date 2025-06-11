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

add_task(async function test_auto_reauthentication_doesnt_show_ui() {
  const idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.net"),
    {}
  );
  const rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.com"),
    {}
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let unlinked = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      let promise = content.navigator.credentials.get({
        identity: {
          mode: "passive",
          providers: [
            {
              configURL:
                "https://example.net/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
              clientId: "123",
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

  ok(unlinked, "expect a result from the second request.");
  ok(unlinked.name, "expect a DOMException which must have a name.");

  // Set account as registered
  IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    "connected",
    true,
    false
  );
  Services.perms.addFromPrincipal(
    rpPrincipal,
    "credential-allow-silent-access",
    Ci.nsIPermissionManager.ALLOW_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );
  Services.perms.addFromPrincipal(
    rpPrincipal,
    "credential-allow-silent-access^" + idpPrincipal.origin,
    Ci.nsIPermissionManager.ALLOW_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  let notApprovedPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      let promise = content.navigator.credentials.get({
        identity: {
          mode: "passive",
          providers: [
            {
              configURL:
                "https://example.net/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
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

  await popupShown;
  tab.linkedBrowser.browsingContext.topChromeWindow.document
    .getElementsByClassName("popup-notification-secondary-button")[0]
    .click();

  let notApproved = await notApprovedPromise;
  ok(notApproved, "expect a result from the second request.");
  ok(notApproved.name, "expect a DOMException which must have a name.");

  let approvedAndLinked = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      let promise = content.navigator.credentials.get({
        identity: {
          mode: "passive",
          providers: [
            {
              configURL:
                "https://example.net/browser/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
              clientId: "123",
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

  is(approvedAndLinked, "result", "Result obtained!");

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
