/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const URL_EXAMPLE =
  "https://example.com/browser/netwerk/cookie/test/browser/cookie-set-dom.html";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref(
    "network.cookie.cookieBehavior.optInPartitioning"
  );
  Services.prefs.clearUserPref("network.cookie.chips.partitionLimitEnabled");
  Services.prefs.clearUserPref(
    "network.cookie.chips.partitionLimitByteCapacity"
  );
  Services.prefs.clearUserPref("network.cookie.CHIPS.enabled");
  Services.prefs.clearUserPref("network.cookie.chips.partitionLimitDryRun");
  Services.cookies.removeAll();
});

// enable chips and chips partition limit
add_setup(async () => {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 5);
  Services.prefs.setBoolPref(
    "network.cookie.cookieBehavior.optInPartitioning",
    true
  );
  Services.prefs.setBoolPref("network.cookie.CHIPS.enabled", true);
  Services.prefs.setBoolPref(
    "network.cookie.chips.partitionLimitEnabled",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.chips.partitionLimitDryRun",
    false
  );
});

const PATH = "/browser/netwerk/cookie/test/browser/";
const PATH_EMPTY = PATH + "file_empty.html";

const FIRST_PARTY = "example.com";
const THIRD_PARTY = "example.org";

const URL_DOCUMENT_FIRSTPARTY = "https://" + FIRST_PARTY + PATH_EMPTY;
const URL_DOCUMENT_THIRDPARTY = "https://" + THIRD_PARTY + PATH_EMPTY;

const COOKIE_PARTITIONED =
  "cookie=partitioned; Partitioned; Secure; SameSite=None;";
const COOKIE_UNPARTITIONED = "cookie=unpartitioned; Secure; SameSite=None;";

function createOriginAttributes(partitionKey) {
  return JSON.stringify({
    firstPartyDomain: "",
    geckoViewSessionContextId: "",
    inIsolatedMozBrowser: false,
    partitionKey,
    privateBrowsingId: 0,
    userContextId: 0,
  });
}

function createPartitionKey(url) {
  let uri = NetUtil.newURI(url);
  return `(${uri.scheme},${uri.host})`;
}

function createSameSiteForeignPartitionKey(url) {
  let uri = NetUtil.newURI(url);
  return `(${uri.scheme},${uri.host},f)`;
}

// OriginAttributes used to access partitioned and unpartitioned cookie jars
// in all tests.
const partitionedOAs = createOriginAttributes(
  createPartitionKey(URL_DOCUMENT_FIRSTPARTY)
);
const partitionedSameSiteForeignOAs = createOriginAttributes(
  createSameSiteForeignPartitionKey(URL_DOCUMENT_FIRSTPARTY)
);
const unpartitionedOAs = createOriginAttributes("");

// cookie bytes >= 4013
var staticCount = 0;
function uniqueLargeCookie(partitioned) {
  ++staticCount;
  var cookie = "largecookie"
    .concat(staticCount)
    .concat("=")
    .concat("1234567890".repeat(400))
    .concat("; Secure; SameSite=None;");
  if (partitioned) {
    return cookie.concat("; Partitioned");
  }
  return cookie;
}

async function setCookieViaDomOnChild(browser, cookie) {
  await SpecialPowers.spawn(browser, [cookie], cookie => {
    content.document.cookie = cookie;
  });
}

// child-side document sets are byte-limited
add_task(async function test_chips_limit_document_first_party_child() {
  const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Set partitioned and unpartitioned cookie from document child-side
  // 10240 * 1.2 -> 12288
  // fourth will cause purge (> 12288 Bytes)
  // and we will purge down to the soft maximum (quota) (10240 Bytes)
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(true));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(true));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(true));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(true));

  await setCookieViaDomOnChild(browser, uniqueLargeCookie(false));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(false));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(false));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(false));

  // Get cookies from partitioned jar
  let partitioned = Services.cookies.getCookiesWithOriginAttributes(
    partitionedOAs,
    FIRST_PARTY
  );

  // Get cookies from unpartitioned jar
  let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
    unpartitionedOAs,
    FIRST_PARTY
  );

  // check that partitioned cookies are purged at maximum
  // check that unpartitioned cookies are not
  Assert.equal(partitioned.length, 2);
  Assert.equal(unpartitioned.length, 4);

  // Cleanup
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

// chips partitioned 3rd party cookies are subject to chips partition limit
add_task(async function test_chips_limit_third_party() {
  const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Spawn document bc
  await SpecialPowers.spawn(browser, [URL_DOCUMENT_THIRDPARTY], async url => {
    let ifr = content.document.createElement("iframe");
    let loadPromise = ContentTaskUtils.waitForEvent(ifr, "load");
    ifr.src = url;
    content.document.body.appendChild(ifr);
    await loadPromise;

    // Spawn iframe bc
    await SpecialPowers.spawn(await ifr.browsingContext, [], async () => {
      function largeCookie(partitioned, index) {
        var cookie = "largecookie"
          .concat(index)
          .concat("=")
          .concat("1234567890".repeat(400))
          .concat("; Secure; SameSite=None;");
        if (partitioned) {
          return cookie.concat("; Partitioned");
        }
        return cookie;
      }
      content.document.cookie = largeCookie(true, 0);
      content.document.cookie = largeCookie(true, 1);
      content.document.cookie = largeCookie(true, 2);
      content.document.cookie = largeCookie(true, 3);
      content.document.cookie = largeCookie(false, 0);
      content.document.cookie = largeCookie(false, 1);
      content.document.cookie = largeCookie(false, 2);
      content.document.cookie = largeCookie(false, 3);
    });
  });

  // Get cookies from partitioned jar
  let partitioned = Services.cookies.getCookiesWithOriginAttributes(
    partitionedOAs,
    THIRD_PARTY
  );
  // Get cookies from unpartitioned jar
  let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
    unpartitionedOAs,
    THIRD_PARTY
  );

  // third party partitioned cookies are subject to the byte limit
  // while unpartitioned are blocked
  Assert.equal(partitioned.length, 2);
  Assert.equal(unpartitioned.length, 0);

  // Cleanup
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

// same-site foreign partitioned cookies are subject to the limit
// This is ABA scenario: A contains B iframe, which contains A iframe
add_task(async function test_chips_limit_samesite_foreign() {
  const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // content process (top-level)
  await SpecialPowers.spawn(
    browser,
    [URL_DOCUMENT_THIRDPARTY, URL_DOCUMENT_FIRSTPARTY],
    async (url, urlInner) => {
      let ifr = content.document.createElement("iframe");
      let loadPromise = ContentTaskUtils.waitForEvent(ifr, "load");
      ifr.src = url;
      content.document.body.appendChild(ifr);
      await loadPromise;

      // Spawn iframe bc (third-party to top-level)
      await SpecialPowers.spawn(
        await ifr.browsingContext,
        [urlInner],
        async urlInner => {
          let ifr = content.document.createElement("iframe");
          let loadPromise = ContentTaskUtils.waitForEvent(ifr, "load");
          ifr.src = urlInner;
          content.document.body.appendChild(ifr);
          await loadPromise;

          // spawn inner iframe (same-site as top-level)
          await SpecialPowers.spawn(await ifr.browsingContext, [], async () => {
            function largeCookie(partitioned, index) {
              var cookie = "largecookie"
                .concat(index)
                .concat("=")
                .concat("1234567890".repeat(400))
                .concat("; Secure; SameSite=None;");
              if (partitioned) {
                return cookie.concat("; Partitioned");
              }
              return cookie;
            }
            content.document.cookie = largeCookie(true, 0);
            content.document.cookie = largeCookie(true, 1);
            content.document.cookie = largeCookie(true, 2);
            content.document.cookie = largeCookie(true, 3);
            content.document.cookie = largeCookie(false, 0);
            content.document.cookie = largeCookie(false, 1);
            content.document.cookie = largeCookie(false, 2);
            content.document.cookie = largeCookie(false, 3);
          });
        }
      );
    }
  );

  // Get cookies from partitioned jar
  let partitioned = Services.cookies.getCookiesWithOriginAttributes(
    partitionedSameSiteForeignOAs,
    FIRST_PARTY
  );
  // Get cookies from unpartitioned jar
  let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
    unpartitionedOAs, // no such thing as unpartitioned with foreign bit
    FIRST_PARTY
  );

  // same-site foreign partitioned cookies are subject to the byte limit
  // unpartitioned are blocked
  Assert.equal(partitioned.length, 2);
  Assert.equal(unpartitioned.length, 0);

  // Cleanup
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

// reducing the byte limit pref affects the amount of cookie bytes we trigger at
add_task(async function test_chips_limit_change_byte_limit() {
  Services.prefs.setIntPref(
    "network.cookie.chips.partitionLimitByteCapacity",
    5120 // half the default
  );
  const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // two cookies of this size will exceed the new limit
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(true));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(true));
  // unpartitioned will be unaffected
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(false));
  await setCookieViaDomOnChild(browser, uniqueLargeCookie(false));

  // Get cookies from partitioned jar
  let partitioned = Services.cookies.getCookiesWithOriginAttributes(
    partitionedOAs,
    FIRST_PARTY
  );

  // Get cookies from unpartitioned jar
  let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
    unpartitionedOAs,
    FIRST_PARTY
  );

  // check that partitioned cookies are purged at maximum
  // check that unpartitioned cookies are not
  Assert.equal(partitioned.length, 1);
  Assert.equal(unpartitioned.length, 2);

  // Cleanup
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});
