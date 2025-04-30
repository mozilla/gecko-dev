"use strict";

// This test ensures the security delay (security.dialog_enable_delay) gets
// properly applied to the "Continue" button on the HTTPS-Only error page. It
// consists of the following checks:
// 1. Check that the button gets enabled at the right time after a new tab is
//    loaded
// 2. Check that the button gets disabled and re-enabled with the correct
//    timings on a focus loss due to a new tab being opened.
// 3. Check that the button gets enabled with the correct timings when the
//    HTTPS-Only error page is loaded through the identity pane.

// We specifically want a insecure url here that will fail to upgrade
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const TEST_URL = "http://untrusted.example.com";
const TEST_PRINCIPAL =
  Services.scriptSecurityManager.createContentPrincipalFromOrigin(TEST_URL);

function waitForEnabledButton() {
  return new Promise(resolve => {
    const button = content.document.getElementById("openInsecure");
    const observer = new content.MutationObserver(mutations => {
      for (const mutation of mutations) {
        if (
          mutation.type === "attributes" &&
          mutation.attributeName === "class" &&
          !mutation.target.classList.contains("disabled")
        ) {
          resolve();
        }
      }
    });
    observer.observe(button, { attributeFilter: ["class"] });
    ok(
      button.classList.contains("disabled"),
      "The 'Continue to HTTP Site' button should be disabled right after the error page is loaded/focused."
    );
  });
}

const specifiedDelay = Services.prefs.getIntPref(
  "security.dialog_enable_delay",
  1000
);

async function waitForEnabledButtonAndCheckTiming() {
  const startTime = Date.now();
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], waitForEnabledButton);
  const endTime = Date.now();

  const observedDelay = endTime - startTime;

  Assert.greater(
    observedDelay,
    specifiedDelay - 100,
    `The observed delay (${observedDelay}ms) should be roughly the same or greater than the delay specified in "security.dialog_enable_delay" (${specifiedDelay}ms)`
  );
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  info("Loading insecure page");
  let loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser, TEST_URL);
  await loaded;
  await waitForEnabledButtonAndCheckTiming();

  info("Opening and closing a new tab");
  let newTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
  });
  await BrowserTestUtils.removeTab(newTab);
  await waitForEnabledButtonAndCheckTiming();

  info("Loading page with exception");
  await Services.perms.addFromPrincipal(
    TEST_PRINCIPAL,
    "https-only-load-insecure",
    Ci.nsIHttpsOnlyModePermission.LOAD_INSECURE_ALLOW_SESSION
  );
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser, TEST_URL);
  await loaded;

  info("Opening identity pane");
  document.getElementById("identity-icon-box").click();
  const identityPopup = document.getElementById("identity-popup");
  ok(!!identityPopup, "Identity pane should exist");
  await BrowserTestUtils.waitForPopupEvent(identityPopup, "shown");

  info("Removing exception in identity pane");
  const menulist = document.getElementById(
    "identity-popup-security-httpsonlymode-menulist"
  );
  ok(!!menulist, "Identity pane should contain HTTPS-Only menulist");
  loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  menulist.getItemAtIndex(0).doCommand();
  await loaded;
  await waitForEnabledButtonAndCheckTiming();
});
