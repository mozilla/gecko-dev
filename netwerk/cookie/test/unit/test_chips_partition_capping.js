/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref(
    "network.cookieJarSettings.unblocked_for_testing"
  );
  Services.prefs.clearUserPref("network.cookie.CHIPS.enabled");
  Services.prefs.clearUserPref("network.cookie.chips.partitionLimitEnabled");
  Services.prefs.clearUserPref(
    "network.cookie.chips.partitionLimitByteCapacity"
  );
  Services.prefs.clearUserPref("network.cookie.chips.partitionLimitDryRun");

  Services.cookies.removeAll();
});

// enable chips and chips partition limit
add_setup(async () => {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 5);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
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

  // FOG needs a profile directory to put its data in.
  do_get_profile();
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

function headerify(cookie, index, partitioned) {
  let maxAge = 9000 + index; // use index so cookies can be ordered by age
  // there is no way to test insecure purging first with a partitioned cookie without `Secure`
  // these cookies would be immediately rejected
  let mostHeaders = `; Max-Age=${maxAge}; SameSite=None; Secure`;
  let temp = cookie.concat(mostHeaders);
  if (partitioned) {
    temp = temp.concat("; Partitioned");
  }
  return temp;
}

async function checkReportedOverflow(expected) {
  let reported =
    await Glean.networking.cookieChipsPartitionLimitOverflow.testGetValue();
  if (expected == 0) {
    Assert.equal(reported, null);
    return;
  }
  // verify the telemetry by summing, but the individual reports are not aggregated
  Assert.equal(reported.sum, expected);
}

function channelMaybePartitioned(uri, partition) {
  let channel = NetUtil.newChannel({
    uri,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
    loadingPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  if (partition) {
    let cookieJarSettings = Cc[
      "@mozilla.org/cookieJarSettings;1"
    ].createInstance(Ci.nsICookieJarSettings);
    cookieJarSettings.initWithURI(uri, false);
    channel.loadInfo.cookieJarSettings = cookieJarSettings;
  }
  return channel;
}

const BYTE_LIMIT = 10240;
const BYTE_LIMIT_WITH_BUFFER = BYTE_LIMIT * 1.2; // 12288
const BYTES_PER_COOKIE = 100;
const COOKIES_TO_SET_COUNT = 122;
const COOKIES_TO_SET_BYTES = COOKIES_TO_SET_COUNT * BYTES_PER_COOKIE; // 12200

// set many cookies at 100 Bytes each
// partition maximum is 10KiB or 10240B (only for partitioned cookies)
// so after this function is called any partitioned cookie (100B) will exceed
function setManyCookies(uri, channel, partitioned) {
  let cookieString = "";
  let cookieNames = [];
  for (let i = 0; i < COOKIES_TO_SET_COUNT; i++) {
    let name = "c" + i.toString();
    let value =
      i + "_".repeat(BYTES_PER_COOKIE - i.toString().length - name.length);
    let cookie = name + "=" + value;
    cookieNames.push(name);

    Services.cookies.setCookieStringFromHttp(
      uri,
      headerify(cookie, i, partitioned),
      channel
    );

    // prep the expected value
    cookieString += cookie;
    if (i < COOKIES_TO_SET_COUNT - 1) {
      cookieString += "; ";
    }
  }

  return { cookieString, cookieNames };
}

// unpartitioned cookies should not be purged
add_task(async function test_chips_limit_parent_http_unpartitioned() {
  let baseDomain = "example.org";
  let uri = NetUtil.newURI("https://" + baseDomain + "/");
  let channel = channelMaybePartitioned(uri, baseDomain, false);

  let expected = setManyCookies(uri, channel, false);
  expected.cookieNames.push("exceeded");

  // pre-condition: check that all got added as expected
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected.cookieString);
  await checkReportedOverflow(0);

  // unpartitioned cookies, no limit here
  let cookie = "exceeded".concat("=").concat("x".repeat(240));
  Services.cookies.setCookieStringFromHttp(
    uri,
    headerify(cookie, COOKIES_TO_SET_COUNT, false), // use count for uniqueness
    channel
  );

  await checkReportedOverflow(0);

  // extract cookie names from string and compare to expected values
  let second = Services.cookies.getCookieStringFromHttp(uri, channel);
  let cookies = second.split("; ");
  for (let i = 0; i < cookies.length; i++) {
    cookies[i] = cookies[i].substr(0, cookies[i].indexOf("="));
  }
  Assert.deepEqual(cookies, expected.cookieNames);
  Assert.equal(cookies.length, expected.cookieNames.length);
  Services.cookies.removeAll();
  Services.fog.testResetFOG();
});

// parent http partition cookies exceeding capacity should purge in FIFO manner
add_task(async function test_chips_limit_parent_http_partitioned() {
  let baseDomain = "example.org";
  let uri = NetUtil.newURI("https://" + baseDomain + "/");
  let channel = channelMaybePartitioned(uri, baseDomain, true);

  let expected = setManyCookies(uri, channel, true);
  expected.cookieNames.push("exceeded");
  // with the buffer quite a few cookies will be purged
  for (let i = 0; i < 23; i++) {
    expected.cookieNames.shift();
  }

  // pre-condition: check that all got added as expected
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected.cookieString);
  await checkReportedOverflow(0); // no reporting until over the hard cap

  // adding 248 Bytes has excess of 208Bytes (3 cookies will be purged (FIFO))
  let cookie = "exceeded".concat("=").concat("x".repeat(240));
  let cookieNameValueLen = cookie.length - 1;
  Services.cookies.setCookieStringFromHttp(
    uri,
    headerify(cookie, COOKIES_TO_SET_COUNT, true), // use count for uniqueness
    channel
  );

  let expectedOverflow =
    COOKIES_TO_SET_BYTES + cookieNameValueLen - BYTE_LIMIT_WITH_BUFFER;
  await checkReportedOverflow(expectedOverflow);

  // extract cookie names from string and compare to expected values
  let second = Services.cookies.getCookieStringFromHttp(uri, channel);
  let cookies = second.split("; ");
  for (let i = 0; i < cookies.length; i++) {
    cookies[i] = cookies[i].substr(0, cookies[i].indexOf("="));
  }
  Assert.deepEqual(cookies, expected.cookieNames);
  Assert.equal(cookies.length, expected.cookieNames.length);
  Services.cookies.removeAll();
  Services.fog.testResetFOG();
});

// partition limit should still work for cookie overwrites
add_task(async function test_chips_limit_overwrites_can_purge() {
  let baseDomain = "example.org";
  let uri = NetUtil.newURI("https://" + baseDomain + "/");
  let channel = channelMaybePartitioned(uri, baseDomain, true);

  let expected = setManyCookies(uri, channel, true);
  for (let i = 0; i < 22; i++) {
    expected.cookieNames.shift();
  }

  // pre-condition: check that all got added as expected
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected.cookieString);
  await checkReportedOverflow(0);

  // cookie which already exists also triggers purge
  // 244 (new cookie) - 100 (existing cookie) -> 144 newly added bytes
  // So we are in excess by 104 bytes, means 2 cookies need purging (FIFO)
  let cookie = "c101".concat("=").concat("x".repeat(240)); // 244
  let cookieNameValueLen = cookie.length - 1;
  Services.cookies.setCookieStringFromHttp(
    uri,
    headerify(cookie, COOKIES_TO_SET_COUNT, true), // use count for uniqueness
    channel
  );

  let expectedOverflow =
    COOKIES_TO_SET_BYTES +
    cookieNameValueLen -
    BYTES_PER_COOKIE -
    BYTE_LIMIT_WITH_BUFFER;
  await checkReportedOverflow(expectedOverflow);

  // extract cookie names from string and compare to expected values
  let second = Services.cookies.getCookieStringFromHttp(uri, channel);
  let cookies = second.split("; ");
  for (let i = 0; i < cookies.length; i++) {
    cookies[i] = cookies[i].substr(0, cookies[i].indexOf("="));
  }
  Assert.deepEqual(cookies, expected.cookieNames);
  Assert.equal(cookies.length, expected.cookieNames.length);
  Services.cookies.removeAll();
  Services.fog.testResetFOG();
});

// dry run mode should not purge, but still report excess via telemetry
add_task(async function test_chips_limit_dry_run_no_purge() {
  Services.prefs.setBoolPref("network.cookie.chips.partitionLimitDryRun", true);

  let baseDomain = "example.org";
  let uri = NetUtil.newURI("https://" + baseDomain + "/");
  let channel = channelMaybePartitioned(uri, baseDomain, true);
  let expected = setManyCookies(uri, channel, true);
  expected.cookieNames.push("exceeded");

  // pre-condition: check that all got added as expected
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected.cookieString);
  await checkReportedOverflow(0);

  // adding 248 Bytes has excess of 208Bytes (3 cookies will be purged (FIFO))
  let cookie = "exceeded".concat("=").concat("x".repeat(240));
  let cookieNameValueLen = cookie.length - 1;
  Services.cookies.setCookieStringFromHttp(
    uri,
    headerify(cookie, COOKIES_TO_SET_COUNT, true), // use count for uniqueness
    channel
  );
  expected.cookieString += "; ".concat(cookie);

  let expectedOverflow =
    COOKIES_TO_SET_BYTES + cookieNameValueLen - BYTE_LIMIT_WITH_BUFFER;
  await checkReportedOverflow(expectedOverflow);

  // extract cookie names from string and compare to expected values
  let second = Services.cookies.getCookieStringFromHttp(uri, channel);
  let cookies = second.split("; ");
  for (let i = 0; i < cookies.length; i++) {
    cookies[i] = cookies[i].substr(0, cookies[i].indexOf("="));
  }
  Assert.deepEqual(cookies, expected.cookieNames);
  Assert.equal(cookies.length, expected.cookieNames.length);
  Assert.equal(second, expected.cookieString);
  Services.cookies.removeAll();
  Services.fog.testResetFOG();
});

add_task(async function test_chips_limit_chips_off() {
  Services.prefs.setBoolPref(
    "network.cookie.chips.partitionLimitDryRun",
    false
  );
  Services.prefs.setBoolPref("network.cookie.CHIPS.enabled", false);

  let baseDomain = "example.org";
  let uri = NetUtil.newURI("https://" + baseDomain + "/");
  let channel = channelMaybePartitioned(uri, baseDomain, true);
  let expected = setManyCookies(uri, channel, true);
  expected.cookieNames.push("exceeded");

  // pre-condition: check that all got added as expected
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected.cookieString);
  await checkReportedOverflow(0);

  // shouldn't trigger purge when CHIPS disabled
  let cookie = "exceeded".concat("=").concat("x".repeat(240));
  Services.cookies.setCookieStringFromHttp(
    uri,
    headerify(cookie, COOKIES_TO_SET_COUNT, true), // use count for uniqueness
    channel
  );
  expected.cookieString += "; ".concat(cookie);

  await checkReportedOverflow(0);

  // extract cookie names from string and compare to expected values
  let second = Services.cookies.getCookieStringFromHttp(uri, channel);
  let cookies = second.split("; ");
  for (let i = 0; i < cookies.length; i++) {
    cookies[i] = cookies[i].substr(0, cookies[i].indexOf("="));
  }
  Assert.deepEqual(cookies, expected.cookieNames);
  Assert.equal(cookies.length, expected.cookieNames.length);
  Assert.equal(second, expected.cookieString);
  Services.cookies.removeAll();
  Services.fog.testResetFOG();
});

add_task(async function test_chips_limit_chips_limit_off() {
  Services.prefs.setBoolPref(
    "network.cookie.chips.partitionLimitDryRun",
    false
  );
  Services.prefs.setBoolPref("network.cookie.CHIPS.enabled", true);

  Services.prefs.setBoolPref(
    "network.cookie.chips.partitionLimitEnabled",
    false
  );

  let baseDomain = "example.org";
  let uri = NetUtil.newURI("https://" + baseDomain + "/");
  let channel = channelMaybePartitioned(uri, baseDomain, true);
  let expected = setManyCookies(uri, channel, true);
  expected.cookieNames.push("exceeded");

  // pre-condition: check that all got added as expected
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected.cookieString);
  await checkReportedOverflow(0);

  // shouldn't trigger purge when CHIPS limit disabled
  let cookie = "exceeded".concat("=").concat("x".repeat(240));
  Services.cookies.setCookieStringFromHttp(
    uri,
    headerify(cookie, COOKIES_TO_SET_COUNT, true), // use count for uniqueness
    channel
  );
  expected.cookieString += "; ".concat(cookie);

  await checkReportedOverflow(0);

  // extract cookie names from string and compare to expected values
  let second = Services.cookies.getCookieStringFromHttp(uri, channel);
  let cookies = second.split("; ");
  for (let i = 0; i < cookies.length; i++) {
    cookies[i] = cookies[i].substr(0, cookies[i].indexOf("="));
  }
  Assert.deepEqual(cookies, expected.cookieNames);
  Assert.equal(cookies.length, expected.cookieNames.length);
  Assert.equal(second, expected.cookieString);
  Services.cookies.removeAll();
  Services.fog.testResetFOG();
});

// non-chips-partitioned cookies do not trigger the limit
add_task(async function test_chips_limit_non_chips_partitioned() {
  // channel is partitioned, not cookie header -> non-chips-partition cookies
  let baseDomain = "example.org";
  let uri = NetUtil.newURI("https://" + baseDomain + "/");
  let channel = channelMaybePartitioned(uri, baseDomain, true);
  let expected = setManyCookies(uri, channel, false);
  expected.cookieNames.push("exceeded");

  // pre-condition: check that all got added as expected
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected.cookieString);
  await checkReportedOverflow(0);

  // non-chips-partitioned cookies, should also have no limit
  let cookie = "exceeded".concat("=").concat("x".repeat(240));
  Services.cookies.setCookieStringFromHttp(
    uri,
    headerify(cookie, COOKIES_TO_SET_COUNT, false), // use count for uniqueness
    channel
  );

  await checkReportedOverflow(0);

  // extract cookie names from string and compare to expected values
  let second = Services.cookies.getCookieStringFromHttp(uri, channel);
  let cookies = second.split("; ");
  for (let i = 0; i < cookies.length; i++) {
    cookies[i] = cookies[i].substr(0, cookies[i].indexOf("="));
  }
  Assert.deepEqual(cookies, expected.cookieNames);
  Assert.equal(cookies.length, expected.cookieNames.length);
  Services.cookies.removeAll();
  Services.fog.testResetFOG();
});
