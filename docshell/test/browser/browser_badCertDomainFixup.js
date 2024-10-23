/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test checks if we are correctly fixing https URLs by prefixing
// with www. when we encounter a SSL_ERROR_BAD_CERT_DOMAIN error.
// For example, https://example.com -> https://www.example.com.

async function verifyErrorPage(errorPageURL, feltPrivacy = false) {
  let certErrorLoaded = BrowserTestUtils.waitForErrorPage(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, errorPageURL);
  await certErrorLoaded;

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [feltPrivacy],
    async isFeltPrivacy => {
      let ec;
      if (isFeltPrivacy) {
        let netErrorCard =
          content.document.querySelector("net-error-card").wrappedJSObject;
        await netErrorCard.getUpdateComplete();
        netErrorCard.advancedButton.click();
        await ContentTaskUtils.waitForCondition(() => {
          return (ec = netErrorCard.errorCode);
        }, "Error code has been set inside the net-error-card advanced panel");

        is(
          ec.textContent.split(" ").at(-1),
          "SSL_ERROR_BAD_CERT_DOMAIN",
          "Correct error code is shown"
        );
      } else {
        await ContentTaskUtils.waitForCondition(() => {
          ec = content.document.getElementById("errorCode");
          return ec.textContent;
        }, "Error code has been set inside the advanced button panel");
        is(
          ec.textContent,
          "SSL_ERROR_BAD_CERT_DOMAIN",
          "Correct error code is shown"
        );
      }
    }
  );
}

// Turn off the pref and ensure that we show the error page as expected.
add_task(async function testNoFixupDisabledByPref() {
  for (let feltPrivacyEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["security.bad_cert_domain_error.url_fix_enabled", false],
        ["security.certerrors.felt-privacy-v1", feltPrivacyEnabled],
      ],
    });
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

    await verifyErrorPage(
      "https://badcertdomain.example.com",
      feltPrivacyEnabled
    );
    await verifyErrorPage(
      "https://www.badcertdomain2.example.com",
      feltPrivacyEnabled
    );

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await SpecialPowers.popPrefEnv();
  }
});

// Test that "www." is prefixed to a https url when we encounter a bad cert domain
// error if the "www." form is included in the certificate's subjectAltNames.
add_task(async function testAddPrefixForBadCertDomain() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let loadSuccessful = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "https://www.badcertdomain.example.com/"
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    "https://badcertdomain.example.com"
  );
  await loadSuccessful;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test that we don't prefix "www." to a https url when we encounter a bad cert domain
// error under certain conditions.
add_task(async function testNoFixupCases() {
  for (let feltPrivacyEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["security.certerrors.felt-privacy-v1", feltPrivacyEnabled]],
    });
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

    // Test for when "www." form is not present in the certificate.
    await verifyErrorPage(
      "https://mismatch.badcertdomain.example.com",
      feltPrivacyEnabled
    );

    // Test that urls with IP addresses are not fixed.
    await SpecialPowers.pushPrefEnv({
      set: [["network.proxy.allow_hijacking_localhost", true]],
    });
    await verifyErrorPage("https://127.0.0.3:433", feltPrivacyEnabled);
    await SpecialPowers.popPrefEnv();

    // Test that urls with ports are not fixed.
    await verifyErrorPage(
      "https://badcertdomain.example.com:82",
      feltPrivacyEnabled
    );

    BrowserTestUtils.removeTab(gBrowser.selectedTab);

    await SpecialPowers.popPrefEnv();
  }
});

// Test removing "www." prefix if the "www."-less form is included in the
// certificate's subjectAltNames.
add_task(async function testRemovePrefixForBadCertDomain() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let loadSuccessful = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "https://badcertdomain2.example.com/"
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    "https://www.badcertdomain2.example.com"
  );
  await loadSuccessful;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
