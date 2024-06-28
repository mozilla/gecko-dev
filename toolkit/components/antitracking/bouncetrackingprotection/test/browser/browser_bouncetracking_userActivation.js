/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that user activation is correctly recorded for BTP.
 */

function assertNoUserActivationHosts() {
  is(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    0,
    "Should have no user activation hosts."
  );
}

function assertHasUserActivationForHost(siteHost) {
  let userActivationHosts = bounceTrackingProtection.testGetUserActivationHosts(
    {}
  );
  is(userActivationHosts.length, 1, "Should have one user activation.");
  is(
    userActivationHosts[0].siteHost,
    siteHost,
    `User activation host is ${siteHost}.`
  );
}

function getURL(origin) {
  return getBaseUrl(origin) + "file_start.html";
}

/**
 * Inserts an iframe element and resolves once the iframe has loaded.
 * @param {*} browserOrBrowsingContext - Browser or BrowsingContext to insert the iframe into.
 * @param {string} url - URL to load in the iframe.
 * @returns {Promise<BrowsingContext>} Promise which resolves to the iframe's
 * BrowsingContext.
 */
function insertIframeAndWaitForLoad(browserOrBrowsingContext, url) {
  return SpecialPowers.spawn(browserOrBrowsingContext, [url], async url => {
    let iframe = content.document.createElement("iframe");
    iframe.src = url;
    content.document.body.appendChild(iframe);
    // Wait for it to load.
    await ContentTaskUtils.waitForEvent(iframe, "load");

    return iframe.browsingContext;
  });
}

/**
 * Runs a test that spawns an iframe, interacts with it, and checks the BTP user
 * activation state.
 * @param {boolean} useIframeSameSite - Whether the iframe interacted with
 * should be same or cross-site to the top-level window.
 */
async function runIframeTest(useIframeSameSite) {
  assertNoUserActivationHosts();

  await BrowserTestUtils.withNewTab(
    getURL("https://example.com"),
    async browser => {
      let iframeOrigin = useIframeSameSite
        ? "https://example.com"
        : "https://example.org";
      let iframeBC = await insertIframeAndWaitForLoad(
        browser,
        getURL(iframeOrigin)
      );
      info("Interact with the iframe.");
      await BrowserTestUtils.synthesizeMouseAtPoint(1, 1, {}, iframeBC);
    }
  );

  info("Should have user activation for the top level window only.");
  assertHasUserActivationForHost("example.com");
  bounceTrackingProtection.clearAll();
}

add_setup(function () {
  bounceTrackingProtection.clearAll();
});

add_task(async function test_top() {
  assertNoUserActivationHosts();

  await BrowserTestUtils.withNewTab(
    getURL("https://example.com"),
    async browser => {
      info("Interact with the top level window.");
      await BrowserTestUtils.synthesizeMouseAtPoint(1, 1, {}, browser);
    }
  );

  info("Should have user activation for the top level window.");
  assertHasUserActivationForHost("example.com");
  bounceTrackingProtection.clearAll();
});

add_task(async function test_iframe_same_site() {
  await runIframeTest(true);
});

add_task(async function test_iframe_cross_site() {
  await runIframeTest(false);
});

add_task(async function test_iframe_cross_site_nested() {
  assertNoUserActivationHosts();

  await BrowserTestUtils.withNewTab(
    getURL("https://example.com"),
    async browser => {
      let iframeOrigin = "https://example.org";

      info("Create a nested iframe.");
      let iframeBC = await insertIframeAndWaitForLoad(
        browser,
        getURL(iframeOrigin)
      );
      let iframeBCNested = await insertIframeAndWaitForLoad(
        iframeBC,
        getURL(iframeOrigin)
      );

      info("Interact with the nested iframe.");
      await BrowserTestUtils.synthesizeMouseAtPoint(1, 1, {}, iframeBCNested);
    }
  );

  info("Should have user activation for the top level window only.");
  assertHasUserActivationForHost("example.com");
  bounceTrackingProtection.clearAll();
});
