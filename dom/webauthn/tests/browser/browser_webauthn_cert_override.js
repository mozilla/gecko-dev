/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_virtual_authenticator();

let expectSecurityError = expectError("Security");

async function test_webauthn_with_cert_override(
  aTestDomain,
  aExpectSecurityError
) {
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);

  let testURL = "https://" + aTestDomain;
  let certErrorLoaded;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, testURL);
      let browser = gBrowser.selectedBrowser;
      certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );
  info("Waiting for cert error page.");
  await certErrorLoaded;

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("Adding certificate error override.");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let doc = content.document;
    let exceptionButton = doc.getElementById("exceptionDialogButton");
    exceptionButton.click();
  });

  info("Waiting for page load.");
  await loaded;

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let doc = content.document;
    ok(
      !doc.documentURI.startsWith("about:certerror"),
      "Exception has been added."
    );
  });

  let makeCredPromise = promiseWebAuthnMakeCredential(tab, "none", "preferred");
  if (aExpectSecurityError) {
    await makeCredPromise.then(arrivingHereIsBad).catch(expectSecurityError);
    ok(
      true,
      "Calling navigator.credentials.create() results in a security error"
    );
  } else {
    await makeCredPromise.catch(arrivingHereIsBad);
    ok(true, "Calling navigator.credentials.create() is allowed");
  }

  let getAssertionPromise = promiseWebAuthnGetAssertionDiscoverable(tab);
  if (aExpectSecurityError) {
    await getAssertionPromise
      .then(arrivingHereIsBad)
      .catch(expectSecurityError);
    ok(true, "Calling navigator.credentials.get() results in a security error");
  } else {
    await getAssertionPromise.catch(arrivingHereIsBad);
    ok(true, "Calling navigator.credentials.get() results in a security error");
  }

  certOverrideService.clearValidityOverride(aTestDomain, -1, {});

  loaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  BrowserCommands.reloadSkipCache();
  await loaded;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(() => test_webauthn_with_cert_override("expired.example.com", false));
add_task(() => test_webauthn_with_cert_override("untrusted.example.com", true));
add_task(() =>
  test_webauthn_with_cert_override("no-subject-alt-name.example.com", true)
);
