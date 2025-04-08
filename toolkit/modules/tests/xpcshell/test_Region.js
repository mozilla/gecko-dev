"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const { Region } = ChromeUtils.importESModule(
  "resource://gre/modules/Region.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  RegionTestUtils: "resource://testing-common/RegionTestUtils.sys.mjs",
});

const INTERVAL_PREF = "browser.region.update.interval";

const RESPONSE_DELAY = 500;
const RESPONSE_TIMEOUT = 100;

const histogram = Services.telemetry.getHistogramById(
  "SEARCH_SERVICE_COUNTRY_FETCH_RESULT"
);

// Region.sys.mjs will call init() on startup and sent a background
// task to fetch the region, ensure we have completed this before
// running the rest of the tests.
add_task(async function test_startup() {
  RegionTestUtils.setNetworkRegion("UK");
  await checkTelemetry(Region.TELEMETRY.SUCCESS);
  await cleanup();
});

add_task(async function test_basic() {
  let srv = useHttpServer(RegionTestUtils.REGION_URL_PREF);
  srv.registerPathHandler("/", (req, res) => {
    res.setStatusLine("1.1", 200, "OK");
    send(res, { country_code: "UK" });
  });
  // start to listen the notification
  let updateRegion = TestUtils.topicObserved("browser-region-updated");
  await Region._fetchRegion();
  let [subject] = await updateRegion;

  Assert.ok(true, "Region fetch should succeed");
  Assert.equal(Region.home, "UK", "Region fetch should return correct result");
  Assert.equal(
    subject,
    Region.home,
    "Notification should be sent with the correct region"
  );

  await cleanup(srv);
});

add_task(async function test_invalid_url() {
  histogram.clear();
  Services.prefs.setIntPref("browser.region.retry-timeout", 0);
  Services.prefs.setCharPref(
    RegionTestUtils.REGION_URL_PREF,
    "http://localhost:0"
  );
  let result = await Region._fetchRegion();
  Assert.ok(!result, "Should return no result");
  await checkTelemetry(Region.TELEMETRY.NO_RESULT);
});

add_task(async function test_invalid_json() {
  histogram.clear();
  Services.prefs.setCharPref(
    RegionTestUtils.REGION_URL_PREF,
    'data:application/json,{"country_code"'
  );
  let result = await Region._fetchRegion();
  Assert.ok(!result, "Should return no result");
  await checkTelemetry(Region.TELEMETRY.NO_RESULT);
});

add_task(async function test_timeout() {
  histogram.clear();
  Services.prefs.setIntPref("browser.region.retry-timeout", 0);
  Services.prefs.setIntPref("browser.region.timeout", RESPONSE_TIMEOUT);
  let srv = useHttpServer(RegionTestUtils.REGION_URL_PREF);
  srv.registerPathHandler("/", (req, res) => {
    res.processAsync();
    do_timeout(RESPONSE_DELAY, () => {
      send(res, { country_code: "UK" });
      res.finish();
    });
  });

  let result = await Region._fetchRegion();
  Assert.equal(result, null, "Region fetch should return null");

  await checkTelemetry(Region.TELEMETRY.TIMEOUT);
  await cleanup(srv);
});

add_task(async function test_location() {
  let location = { location: { lat: -1, lng: 1 }, accuracy: 100 };
  let srv = useHttpServer("geo.provider.network.url");
  srv.registerPathHandler("/", (req, res) => {
    res.setStatusLine("1.1", 200, "OK");
    send(res, location);
  });

  let result = await Region._getLocation();
  Assert.ok(true, "Region fetch should succeed");
  Assert.deepEqual(result, location, "Location is returned");

  await cleanup(srv);
});

add_task(async function test_update() {
  Region._home = null;
  RegionTestUtils.setNetworkRegion("FR");
  await Region._fetchRegion();
  Assert.equal(Region.home, "FR", "Should have correct region");
  RegionTestUtils.setNetworkRegion("DE");
  await Region._fetchRegion();
  Assert.equal(Region.home, "FR", "Shouldnt have changed yet");
  // Thie first fetchRegion will set the prefs to determine when
  // to update the home region, we need to do 2 fetchRegions to test
  // it isnt updating when it shouldnt.
  await Region._fetchRegion();
  Assert.equal(Region.home, "FR", "Shouldnt have changed yet again");
  Services.prefs.setIntPref(INTERVAL_PREF, 1);
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 1100));
  await Region._fetchRegion();
  Assert.equal(Region.home, "DE", "Should have updated now");

  await cleanup();
});

add_task(async function test_max_retry() {
  Region._home = null;
  let requestsSeen = 0;
  Services.prefs.setIntPref("browser.region.retry-timeout", RESPONSE_TIMEOUT);
  Services.prefs.setIntPref("browser.region.timeout", RESPONSE_TIMEOUT);
  let srv = useHttpServer(RegionTestUtils.REGION_URL_PREF);
  srv.registerPathHandler("/", (req, res) => {
    requestsSeen++;
    res.setStatusLine("1.1", 200, "OK");
    res.processAsync();
    do_timeout(RESPONSE_DELAY, res.finish.bind(res));
  });

  Region._fetchRegion();
  await TestUtils.waitForCondition(() => requestsSeen === 3);
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, RESPONSE_DELAY));

  Assert.equal(Region.home, null, "failed to fetch region");
  Assert.equal(requestsSeen, 3, "Retried 4 times");

  Region._retryCount = 0;
  await cleanup(srv);
});

add_task(async function test_retry() {
  Region._home = null;
  let requestsSeen = 0;
  Services.prefs.setIntPref("browser.region.retry-timeout", RESPONSE_TIMEOUT);
  Services.prefs.setIntPref("browser.region.timeout", RESPONSE_TIMEOUT);
  let srv = useHttpServer(RegionTestUtils.REGION_URL_PREF);
  srv.registerPathHandler("/", (req, res) => {
    res.setStatusLine("1.1", 200, "OK");
    if (++requestsSeen == 2) {
      res.setStatusLine("1.1", 200, "OK");
      send(res, { country_code: "UK" });
    } else {
      res.processAsync();
      do_timeout(RESPONSE_DELAY, res.finish.bind(res));
    }
  });

  Region._fetchRegion();
  await TestUtils.waitForCondition(() => requestsSeen === 2);
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, RESPONSE_DELAY));

  Assert.equal(Region.home, "UK", "failed to fetch region");
  Assert.equal(requestsSeen, 2, "Retried 2 times");

  await cleanup(srv);
});

add_task(async function test_timerManager() {
  RegionTestUtils.setNetworkRegion("FR");

  // Ensure the home region updates immediately, but the update
  // check will only happen once per second.
  Services.prefs.setIntPref("browser.region.update.interval", 0);
  Services.prefs.setIntPref("browser.region.update.debounce", 1);

  let region = Region.newInstance();
  await region.init();
  Assert.equal(region.home, "FR", "Should have correct initial region");

  // Updates are being debounced, these should be ignored.
  RegionTestUtils.setNetworkRegion("DE");
  await region._updateTimer();
  await region._updateTimer();
  Assert.equal(region.home, "FR", "Ignored updates to region");

  // Set the debounce interval to 0 so these updates are used.
  Services.prefs.setIntPref("browser.region.update.debounce", 0);
  RegionTestUtils.setNetworkRegion("AU");
  await region._updateTimer();
  await region._updateTimer();
  Assert.equal(region.home, "AU", "region has been updated");
  await cleanup();
});

function useHttpServer(pref) {
  let server = new HttpServer();
  server.start(-1);
  Services.prefs.setCharPref(
    pref,
    `http://localhost:${server.identity.primaryPort}/`
  );
  return server;
}

function send(res, json) {
  res.setStatusLine("1.1", 200, "OK");
  res.setHeader("content-type", "application/json", true);
  res.write(JSON.stringify(json));
}

async function cleanup(srv = null) {
  Services.prefs.clearUserPref("browser.search.region");
  if (srv) {
    await new Promise(r => srv.stop(r));
  }
}

async function checkTelemetry(aExpectedValue) {
  // Wait until there is 1 result.
  await TestUtils.waitForCondition(() => {
    let snapshot = histogram.snapshot();
    return Object.values(snapshot.values).reduce((a, b) => a + b) == 1;
  });
  let snapshot = histogram.snapshot();
  Assert.equal(snapshot.values[aExpectedValue], 1);
}
