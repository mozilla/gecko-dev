/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_DOMAIN_A = "example.com";
const ORIGIN_A = `https://${BASE_DOMAIN_A}`;
const ORIGIN_A_HTTP = `http://${BASE_DOMAIN_A}`;
const ORIGIN_A_SUB = `https://test1.${BASE_DOMAIN_A}`;

const CONTAINER_PRINCIPAL_A =
  Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(ORIGIN_A),
    { userContextId: 2 }
  );
const CONTAINER_PRINCIPAL_A_SUB =
  Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(ORIGIN_A_SUB),
    { userContextId: 2 }
  );

const BASE_DOMAIN_B = "example.org";
const ORIGIN_B = `https://${BASE_DOMAIN_B}`;
const ORIGIN_B_HTTP = `http://${BASE_DOMAIN_B}`;
const ORIGIN_B_SUB = `https://test1.${BASE_DOMAIN_B}`;

const TEST_ROOT_DIR = getRootDirectory(gTestPath);

// Stylesheets are cached per process, so we need to keep tabs open for the
// duration of a test.
let tabs = {};

function getTestURLForOrigin(origin) {
  return (
    TEST_ROOT_DIR.replace("chrome://mochitests/content", origin) +
    "file_css_cache.html"
  );
}

async function testCached(origin, isCached) {
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin);
  let url = getTestURLForOrigin(principal.originNoSuffix);

  let numParsed;

  let tab = tabs[principal.origin];
  let loadedPromise;
  if (!tab) {
    info("Creating new tab for " + url);
    tab = gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId: principal.originAttributes.userContextId,
    });
    gBrowser.selectedTab = tab;
    loadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    tabs[principal.origin] = tab;
  } else {
    loadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    tab.linkedBrowser.reload();
  }
  await loadedPromise;

  numParsed = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return SpecialPowers.getDOMWindowUtils(content).parsedStyleSheets;
  });

  // Stylesheets is cached if numParsed is 0.
  is(!numParsed, isCached, `${origin} is${isCached ? " " : " not "}cached`);
}

async function addTestTabs() {
  await testCached(ORIGIN_A, false);
  await testCached(ORIGIN_A_SUB, false);
  await testCached(ORIGIN_A_HTTP, false);
  await testCached(ORIGIN_B, false);
  await testCached(ORIGIN_B_SUB, false);
  await testCached(ORIGIN_B_HTTP, false);
  // Add more entries for ORIGIN_A but set a different user context.
  await testCached(CONTAINER_PRINCIPAL_A.origin, false);
  // Add another entry for ORIGIN_A but set a different user context.
  await testCached(CONTAINER_PRINCIPAL_A_SUB.origin, false);

  // Test that the cache has been populated.
  await testCached(ORIGIN_A, true);
  await testCached(ORIGIN_A_SUB, true);
  await testCached(ORIGIN_A_HTTP, true);
  await testCached(ORIGIN_B, true);
  await testCached(ORIGIN_B_SUB, true);
  await testCached(ORIGIN_B_HTTP, true);
  await testCached(CONTAINER_PRINCIPAL_A.origin, true);
  await testCached(CONTAINER_PRINCIPAL_A_SUB.origin, true);
}

async function cleanupTestTabs() {
  Object.values(tabs).forEach(BrowserTestUtils.removeTab);
  tabs = {};
}

add_task(async function test_deleteByPrincipal() {
  await SpecialPowers.setBoolPref("dom.security.https_first", false);
  await addTestTabs();

  // Clear data for content principal of A
  info("Clearing cache for principal " + ORIGIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(ORIGIN_A),
      false,
      Ci.nsIClearDataService.CLEAR_CSS_CACHE,
      resolve
    );
  });

  // Only the cache entry for ORIGIN_A should have been cleared.
  await testCached(ORIGIN_A, false);
  await testCached(ORIGIN_A_SUB, true);
  await testCached(ORIGIN_A_HTTP, true);
  await testCached(ORIGIN_B, true);
  await testCached(ORIGIN_B_SUB, true);
  await testCached(ORIGIN_B_HTTP, true);
  // User context 2 not cleared because we clear by exact principal.
  await testCached(CONTAINER_PRINCIPAL_A.origin, true);
  await testCached(CONTAINER_PRINCIPAL_A_SUB.origin, true);

  // Cleanup
  cleanupTestTabs();
  ChromeUtils.clearStyleSheetCache();
});

add_task(async function test_deleteBySite() {
  await addTestTabs();

  // Clear data for base domain of A.
  info("Clearing cache for (schemeless) site " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromSite(
      BASE_DOMAIN_A,
      {},
      false,
      Ci.nsIClearDataService.CLEAR_CSS_CACHE,
      resolve
    );
  });

  // All entries for A should have been cleared.
  await testCached(ORIGIN_A, false);
  await testCached(ORIGIN_A_SUB, false);
  await testCached(ORIGIN_A_HTTP, false);
  // User context 2 also cleared because we passed the wildcard
  // OriginAttributesPattern {} above.
  await testCached(CONTAINER_PRINCIPAL_A.origin, false);
  await testCached(CONTAINER_PRINCIPAL_A_SUB.origin, false);
  // Entries for B should still exist.
  await testCached(ORIGIN_B, true);
  await testCached(ORIGIN_B_SUB, true);
  await testCached(ORIGIN_B_HTTP, true);

  // Cleanup
  cleanupTestTabs();
  ChromeUtils.clearStyleSheetCache();
});

add_task(async function test_deleteBySite_oa_pattern() {
  await addTestTabs();

  // Clear data for site A.
  info("Clearing cache for (schemeless) site+pattern " + BASE_DOMAIN_A);
  await new Promise(resolve => {
    Services.clearData.deleteDataFromSite(
      BASE_DOMAIN_A,
      { userContextId: CONTAINER_PRINCIPAL_A.originAttributes.userContextId },
      false,
      Ci.nsIClearDataService.CLEAR_CSS_CACHE,
      resolve
    );
  });

  // Normal entries should not have been cleared.
  await testCached(ORIGIN_A, true);
  await testCached(ORIGIN_A_SUB, true);
  await testCached(ORIGIN_A_HTTP, true);

  // Container entries should have been cleared because we have targeted them with the pattern.
  await testCached(CONTAINER_PRINCIPAL_A.origin, false);
  await testCached(CONTAINER_PRINCIPAL_A_SUB.origin, false);

  // Entries for unrelated site B should still exist.
  await testCached(ORIGIN_B, true);
  await testCached(ORIGIN_B_SUB, true);
  await testCached(ORIGIN_B_HTTP, true);

  // Cleanup
  cleanupTestTabs();
  ChromeUtils.clearStyleSheetCache();
});
