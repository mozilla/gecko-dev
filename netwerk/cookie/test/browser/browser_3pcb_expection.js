/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const COLLECTION_NAME = "third-party-cookie-blocking-exempt-urls";
const PREF_NAME = "network.cookie.cookieBehavior.optInPartitioning.skip_list";

const FIRST_PARTY_DOMAIN = "example.com";
const THIRD_PARTY_DOMAIN = "example.org";
const ANOTHER_THIRD_PARTY_DOMAIN = "example.net";

const FIRST_PARTY_SITE = `https://${FIRST_PARTY_DOMAIN}`;
const THIRD_PARTY_SITE = `https://${THIRD_PARTY_DOMAIN}`;
const ANOTHER_THIRD_PARTY_SITE = `https://${ANOTHER_THIRD_PARTY_DOMAIN}`;

const FIRST_PARTY_URL = `${FIRST_PARTY_SITE}/${TEST_PATH}/file_empty.html`;
const THIRD_PARTY_URL = `${THIRD_PARTY_SITE}/${TEST_PATH}/file_empty.html`;

// RemoteSettings collection db.
let db;

/**
 * Dispatch a RemoteSettings "sync" event.
 * @param {Object} data - The event's data payload.
 * @param {Object} [data.created] - Records that were created.
 * @param {Object} [data.updated] - Records that were updated.
 * @param {Object} [data.deleted] - Records that were removed.
 */
async function remoteSettingsSync({ created, updated, deleted }) {
  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: {
      created,
      updated,
      deleted,
    },
  });
}

/**
 * Compare two string arrays ignoring order.
 * @param {string[]} arr1 - The first array.
 * @param {string[]} arr2 - The second array.
 * @returns {boolean} - Whether the arrays match.
 */
const strArrayMatches = (arr1, arr2) =>
  arr1.length === arr2.length &&
  arr1.sort().every((value, index) => value === arr2.sort()[index]);

/**
 * Wait until the 3pcb allow-list matches the expected state.
 * @param {string[]} allowedSiteHosts - (Unordered) host list to match.
 */
async function waitForAllowListState(expected) {
  // Ensure the site host exception list has been imported correctly.
  await BrowserTestUtils.waitForCondition(() => {
    return strArrayMatches(Services.cookies.testGet3PCBExceptions(), expected);
  }, "Waiting for exceptions to be imported.");
  Assert.deepEqual(
    Services.cookies.testGet3PCBExceptions().sort(),
    expected.sort(),
    "Imported the correct site host exceptions"
  );
}

/**
 * A helper function to create the iframe and the nested ABA iframe.
 * @param {Browser} browser The browser where the testing is performed.
 * @param {string} firstPartyURL The first party URL.
 * @param {string} thirdPartyURL The third party URL.
 * @returns {Promise} A promise that resolves to the iframe browsing context
 *                    and the ABA iframe browsing context.
 */
async function createNestedIframes(browser, firstPartyURL, thirdPartyURL) {
  return SpecialPowers.spawn(
    browser,
    [firstPartyURL, thirdPartyURL],
    async (firstPartyURL, thirdPartyURL) => {
      let iframe = content.document.createElement("iframe");
      iframe.src = thirdPartyURL;

      await new Promise(resolve => {
        iframe.onload = resolve;
        content.document.body.appendChild(iframe);
      });

      let ABABC = await SpecialPowers.spawn(
        iframe,
        [firstPartyURL],
        async firstPartyURL => {
          let iframe = content.document.createElement("iframe");
          iframe.src = firstPartyURL;

          await new Promise(resolve => {
            iframe.onload = resolve;
            content.document.body.appendChild(iframe);
          });

          return iframe.browsingContext;
        }
      );

      return { iframeBC: iframe.browsingContext, ABABC };
    }
  );
}

/**
 * A helper function to set third-party cookies in the third-party iframe and
 * the ABA iframe.
 *
 * @param {Browser} browser The browser where the testing is performed.
 * @param {CanonicalBrowsingContext} iframeBC The iframe browsing context.
 * @param {CanonicalBrowsingContext} ABAABC The ABA browsing context.
 */
async function setThirdPartyCookie(browser, iframeBC, ABABC) {
  const THIRD_PARTY_FETCH_COOKIE_URL = `${THIRD_PARTY_SITE}/${TEST_PATH}/setFetchCookie.sjs`;

  // Try to set a third-party cookie by fetching from the third-party URL.
  await SpecialPowers.spawn(
    browser,
    [THIRD_PARTY_FETCH_COOKIE_URL],
    async url => {
      await content.fetch(url, { credentials: "include" });
    }
  );

  // Set a third-party cookie in the third-party iframe.
  await SpecialPowers.spawn(iframeBC, [], async _ => {
    content.document.cookie = "thirdPartyIframe=value; SameSite=None; Secure;";
  });

  // Set a ABA cookie in the nested iframe. An ABA cookie is also considered
  // as a third-party cookie.
  await SpecialPowers.spawn(ABABC, [], async _ => {
    content.document.cookie = "ABAIframe=value; SameSite=None; Secure;";
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior.optInPartitioning", true]],
  });

  // Start with an empty RS collection.
  db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), [], { clear: true });
});

add_task(async function test_3pcb_no_exception() {
  // Clear cookies before running the test.
  Services.cookies.removeAll();

  info("Opening a new tab.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FIRST_PARTY_URL
  );
  let browser = tab.linkedBrowser;

  info("Creating iframes and setting third-party cookies.");
  let { iframeBC, ABABC } = await createNestedIframes(
    browser,
    FIRST_PARTY_URL,
    THIRD_PARTY_URL
  );
  await setThirdPartyCookie(browser, iframeBC, ABABC);

  info("Verifying cookies.");
  // Verify in the iframeBC to ensure no cookie is set.
  await SpecialPowers.spawn(iframeBC, [], async () => {
    let cookies = content.document.cookie;
    is(cookies, "", "No cookies should be set in the iframeBC");
  });

  // Verify in the nested iframe to ensure no cookie is set.
  await SpecialPowers.spawn(ABABC, [], async () => {
    let cookies = content.document.cookie;
    is(cookies, "", "No cookies should be set in the ABA iframe");
  });

  info("Clean up");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_3pcb_pref_exception() {
  // Clear cookies before running the test.
  Services.cookies.removeAll();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        PREF_NAME,
        `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE};${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
      ],
    ],
  });

  info("Opening a new tab.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FIRST_PARTY_URL
  );
  let browser = tab.linkedBrowser;

  info("Creating iframes and setting third-party cookies.");
  let { iframeBC, ABABC } = await createNestedIframes(
    browser,
    FIRST_PARTY_URL,
    THIRD_PARTY_URL
  );
  await setThirdPartyCookie(browser, iframeBC, ABABC);

  info("Verifying cookies.");
  // Verify in the iframeBC to ensure cookies are set.
  await SpecialPowers.spawn(iframeBC, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "thirdPartyFetch=value; thirdPartyIframe=value",
      "Cookies should be set in the iframeBC"
    );
  });

  // Verify in the nested ABA iframe to ensure no cookie is set.
  await SpecialPowers.spawn(ABABC, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "ABAIframe=value",
      "No cookies should be set in the ABA iframe"
    );
  });
  BrowserTestUtils.removeTab(tab);

  info("Clear exceptions and verify cookies are still valid");
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_NAME, ""]],
  });

  info("Opening the tab again.");
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FIRST_PARTY_URL);
  browser = tab.linkedBrowser;

  let { iframeBC: iframeBCNew, ABABC: ABABCNew } = await createNestedIframes(
    browser,
    FIRST_PARTY_URL,
    THIRD_PARTY_URL
  );

  await SpecialPowers.spawn(iframeBCNew, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "thirdPartyFetch=value; thirdPartyIframe=value",
      "Cookies should be set in the iframeBC"
    );
  });
  await SpecialPowers.spawn(ABABCNew, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "ABAIframe=value",
      "No cookies should be set in the ABA iframe"
    );
  });

  info("Clean up");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_3pcb_pref_wildcard_exception() {
  // Clear cookies before running the test.
  Services.cookies.removeAll();

  await SpecialPowers.pushPrefEnv({
    set: [[PREF_NAME, `*,${THIRD_PARTY_SITE};*,${FIRST_PARTY_SITE}`]],
  });

  info("Opening a new tab.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FIRST_PARTY_URL
  );
  let browser = tab.linkedBrowser;

  info("Creating iframes and setting third-party cookies.");
  let { iframeBC, ABABC } = await createNestedIframes(
    browser,
    FIRST_PARTY_URL,
    THIRD_PARTY_URL
  );
  await setThirdPartyCookie(browser, iframeBC, ABABC);

  info("Verifying cookies.");
  // Verify in the iframeBC to ensure cookies are set.
  await SpecialPowers.spawn(iframeBC, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "thirdPartyFetch=value; thirdPartyIframe=value",
      "Cookies should be set in the iframeBC"
    );
  });

  // Verify in the nested ABA iframe to ensure no cookie is set.
  await SpecialPowers.spawn(ABABC, [], async () => {
    let cookies = content.document.cookie;
    is(cookies, "ABAIframe=value", "Cookies should be set in the ABA iframe");
  });
  BrowserTestUtils.removeTab(tab);

  info("Clear exceptions and verify cookies are still valid");
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_NAME, ""]],
  });

  info("Opening the tab again.");
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FIRST_PARTY_URL);
  browser = tab.linkedBrowser;

  let { iframeBC: iframeBCNew, ABABC: ABABCNew } = await createNestedIframes(
    browser,
    FIRST_PARTY_URL,
    THIRD_PARTY_URL
  );

  await SpecialPowers.spawn(iframeBCNew, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "thirdPartyFetch=value; thirdPartyIframe=value",
      "Cookies should be set in the iframeBC"
    );
  });
  await SpecialPowers.spawn(ABABCNew, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "ABAIframe=value",
      "No cookies should be set in the ABA iframe"
    );
  });

  info("Clean up");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_3pcb_pref_exception_updates() {
  // Start with an empty pref
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_NAME, ""]],
  });

  info("Set initial pref value");
  Services.prefs.setStringPref(
    PREF_NAME,
    `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE};${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`
  );
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Update the pref exception");
  Services.prefs.setStringPref(
    PREF_NAME,
    `${FIRST_PARTY_SITE},${ANOTHER_THIRD_PARTY_SITE};${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`
  );
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${ANOTHER_THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Remove one exception");
  Services.prefs.setStringPref(
    PREF_NAME,
    `${FIRST_PARTY_SITE},${ANOTHER_THIRD_PARTY_SITE}`
  );
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${ANOTHER_THIRD_PARTY_SITE}`,
  ]);

  info("Remove all exceptions");
  Services.prefs.setStringPref(PREF_NAME, "");
  await waitForAllowListState([]);

  info("Cleanup");
  Services.prefs.clearUserPref(PREF_NAME);
});

add_task(async function test_3pcb_rs_exception() {
  // Clear cookies before running the test.
  Services.cookies.removeAll();

  info("Import RS entries.");
  let thirdPartyEntry = await db.create({
    fpSite: FIRST_PARTY_SITE,
    tpSite: THIRD_PARTY_SITE,
  });
  let ABAEntry = await db.create({
    fpSite: FIRST_PARTY_SITE,
    tpSite: FIRST_PARTY_SITE,
  });
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ created: [thirdPartyEntry, ABAEntry] });
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Opening a new tab.");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    FIRST_PARTY_URL
  );
  let browser = tab.linkedBrowser;

  info("Creating iframes and setting third-party cookies.");
  let { iframeBC, ABABC } = await createNestedIframes(
    browser,
    FIRST_PARTY_URL,
    THIRD_PARTY_URL
  );
  await setThirdPartyCookie(browser, iframeBC, ABABC);

  info("Verifying cookies.");
  // Verify in the iframeBC to ensure cookies are set.
  await SpecialPowers.spawn(iframeBC, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "thirdPartyFetch=value; thirdPartyIframe=value",
      "Cookies should be set in the iframeBC"
    );
  });

  // Verify in the nested ABA iframe to ensure the cookie is set.
  await SpecialPowers.spawn(ABABC, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "ABAIframe=value",
      "No cookies should be set in the ABA iframe"
    );
  });
  BrowserTestUtils.removeTab(tab);

  info("Clear exceptions and verify cookies are still valid");
  await db.delete(thirdPartyEntry.id);
  await db.delete(ABAEntry.id);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({
    deleted: [thirdPartyEntry, ABAEntry],
  });
  await waitForAllowListState([]);

  info("Opening the tab again.");
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FIRST_PARTY_URL);
  browser = tab.linkedBrowser;

  let { iframeBC: iframeBCNew, ABABC: ABABCNew } = await createNestedIframes(
    browser,
    FIRST_PARTY_URL,
    THIRD_PARTY_URL
  );

  await SpecialPowers.spawn(iframeBCNew, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "thirdPartyFetch=value; thirdPartyIframe=value",
      "Cookies should be set in the iframeBC"
    );
  });
  await SpecialPowers.spawn(ABABCNew, [], async () => {
    let cookies = content.document.cookie;
    is(
      cookies,
      "ABAIframe=value",
      "No cookies should be set in the ABA iframe"
    );
  });

  info("Clean up");
  BrowserTestUtils.removeTab(tab);
  await db.clear();
  await db.importChanges({}, Date.now());
});

add_task(async function test_3pcb_rs_exception_updates() {
  info("Create the third-party entry and the ABA entry.");
  let thirdPartyEntry = await db.create({
    fpSite: FIRST_PARTY_SITE,
    tpSite: THIRD_PARTY_SITE,
  });
  let ABAEntry = await db.create({
    fpSite: FIRST_PARTY_SITE,
    tpSite: FIRST_PARTY_SITE,
  });
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ created: [thirdPartyEntry, ABAEntry] });
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Update third-party entry with a different third-party site.");
  let thirdPartyEntryUpdated = { ...thirdPartyEntry };
  thirdPartyEntryUpdated.tpSite = ANOTHER_THIRD_PARTY_SITE;
  await db.update(thirdPartyEntry);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({
    updated: [{ old: thirdPartyEntry, new: thirdPartyEntryUpdated }],
  });
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${ANOTHER_THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Create another entry and remove the ABA entry.");
  let anotherThirdPartyEntry = await db.create({
    fpSite: ANOTHER_THIRD_PARTY_SITE,
    tpSite: THIRD_PARTY_SITE,
  });
  await db.delete(ABAEntry.id);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({
    created: [anotherThirdPartyEntry],
    deleted: [ABAEntry],
  });
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${ANOTHER_THIRD_PARTY_SITE}`,
    `${ANOTHER_THIRD_PARTY_SITE},${THIRD_PARTY_SITE}`,
  ]);

  info("Remove all RS entries.");
  await db.delete(thirdPartyEntryUpdated.id);
  await db.delete(anotherThirdPartyEntry.id);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({
    deleted: [thirdPartyEntryUpdated, anotherThirdPartyEntry],
  });
  await waitForAllowListState([]);

  info("Clean up");
  await db.clear();
  await db.importChanges({}, Date.now());
});

add_task(async function test_3pcb_rs_precedence_over_pref() {
  info("Create the third-party entry and the ABA entry.");
  let thirdPartyEntry = await db.create({
    fpSite: FIRST_PARTY_SITE,
    tpSite: THIRD_PARTY_SITE,
  });
  let ABAEntry = await db.create({
    fpSite: FIRST_PARTY_SITE,
    tpSite: FIRST_PARTY_SITE,
  });
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ created: [thirdPartyEntry, ABAEntry] });
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Set the duplicate pref exception.");
  // Verify that we don't introduce duplicate exceptions if we set the same
  // exception via pref.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        PREF_NAME,
        `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE};${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
      ],
    ],
  });
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Remove the pref exception.");
  // Verify that the RS exception is still there even if we remove the same
  // exception via pref.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_NAME, ""]],
  });
  await waitForAllowListState([
    `${FIRST_PARTY_SITE},${THIRD_PARTY_SITE}`,
    `${FIRST_PARTY_SITE},${FIRST_PARTY_SITE}`,
  ]);

  info("Clean up");
  await db.delete(thirdPartyEntry.id);
  await db.delete(ABAEntry.id);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({
    deleted: [thirdPartyEntry, ABAEntry],
  });
  await waitForAllowListState([]);
  await db.clear();
  await db.importChanges({}, Date.now());
});
