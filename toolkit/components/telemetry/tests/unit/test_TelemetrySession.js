/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
/* This testcase triggers two telemetry pings.
 *
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/ClientID.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LightweightThemeManager.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);

const PING_FORMAT_VERSION = 4;
const PING_TYPE_MAIN = "main";
const PING_TYPE_SAVED_SESSION = "saved-session";

const REASON_ABORTED_SESSION = "aborted-session";
const REASON_SAVED_SESSION = "saved-session";
const REASON_SHUTDOWN = "shutdown";
const REASON_TEST_PING = "test-ping";
const REASON_DAILY = "daily";
const REASON_ENVIRONMENT_CHANGE = "environment-change";

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";

const IGNORE_HISTOGRAM = "test::ignore_me";
const IGNORE_HISTOGRAM_TO_CLONE = "MEMORY_HEAP_ALLOCATED";
const IGNORE_CLONED_HISTOGRAM = "test::ignore_me_also";
const ADDON_NAME = "Telemetry test addon";
const ADDON_HISTOGRAM = "addon-histogram";
// Add some unicode characters here to ensure that sending them works correctly.
const SHUTDOWN_TIME = 10000;
const FAILED_PROFILE_LOCK_ATTEMPTS = 2;

// Constants from prio.h for nsIFileOutputStream.init
const PR_WRONLY = 0x2;
const PR_CREATE_FILE = 0x8;
const PR_TRUNCATE = 0x20;
const RW_OWNER = parseInt("0600", 8);

const NUMBER_OF_THREADS_TO_LAUNCH = 30;
let gNumberOfThreadsLaunched = 0;

const MS_IN_ONE_HOUR  = 60 * 60 * 1000;
const MS_IN_ONE_DAY   = 24 * MS_IN_ONE_HOUR;

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_ENABLED = PREF_BRANCH + "enabled";
const PREF_SERVER = PREF_BRANCH + "server";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";
const PREF_FHR_SERVICE_ENABLED = "datareporting.healthreport.service.enabled";

const DATAREPORTING_DIR = "datareporting";
const ABORTED_PING_FILE_NAME = "aborted-session-ping";
const ABORTED_SESSION_UPDATE_INTERVAL_MS = 5 * 60 * 1000;

XPCOMUtils.defineLazyGetter(this, "DATAREPORTING_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIR);
});

let gClientID = null;

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

function truncateDateToDays(date) {
  return new Date(date.getFullYear(),
                  date.getMonth(),
                  date.getDate(),
                  0, 0, 0, 0);
}

function sendPing() {
  TelemetrySession.gatherStartup();
  if (PingServer.started) {
    TelemetrySend.setServer("http://localhost:" + PingServer.port);
    return TelemetrySession.testPing();
  } else {
    TelemetrySend.setServer("http://doesnotexist");
    return TelemetrySession.testPing();
  }
}

let clearPendingPings = Task.async(function*() {
  const pending = yield TelemetryStorage.loadPendingPingList();
  for (let p of pending) {
    yield TelemetryStorage.removePendingPing(p.id);
  }
});

function fakeGenerateUUID(sessionFunc, subsessionFunc) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  session.Policy.generateSessionUUID = sessionFunc;
  session.Policy.generateSubsessionUUID = subsessionFunc;
}

function fakeIdleNotification(topic) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  return session.TelemetryScheduler.observe(null, topic, null);
}

function setupTestData() {
  Telemetry.newHistogram(IGNORE_HISTOGRAM, "never", Telemetry.HISTOGRAM_BOOLEAN);
  Telemetry.histogramFrom(IGNORE_CLONED_HISTOGRAM, IGNORE_HISTOGRAM_TO_CLONE);
  Services.startup.interrupted = true;
  Telemetry.registerAddonHistogram(ADDON_NAME, ADDON_HISTOGRAM,
                                   Telemetry.HISTOGRAM_LINEAR,
                                   1, 5, 6);
  let h1 = Telemetry.getAddonHistogram(ADDON_NAME, ADDON_HISTOGRAM);
  h1.add(1);
  let h2 = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  h2.add();

  let k1 = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  k1.add("a");
  k1.add("a");
  k1.add("b");
}

function getSavedPingFile(basename) {
  let tmpDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let pingFile = tmpDir.clone();
  pingFile.append(basename);
  if (pingFile.exists()) {
    pingFile.remove(true);
  }
  do_register_cleanup(function () {
    try {
      pingFile.remove(true);
    } catch (e) {
    }
  });
  return pingFile;
}

function checkPingFormat(aPing, aType, aHasClientId, aHasEnvironment) {
  const MANDATORY_PING_FIELDS = [
    "type", "id", "creationDate", "version", "application", "payload"
  ];

  const APPLICATION_TEST_DATA = {
    buildId: "2007010101",
    name: APP_NAME,
    version: APP_VERSION,
    vendor: "Mozilla",
    platformVersion: PLATFORM_VERSION,
    xpcomAbi: "noarch-spidermonkey",
  };

  // Check that the ping contains all the mandatory fields.
  for (let f of MANDATORY_PING_FIELDS) {
    Assert.ok(f in aPing, f + "must be available.");
  }

  Assert.equal(aPing.type, aType, "The ping must have the correct type.");
  Assert.equal(aPing.version, PING_FORMAT_VERSION, "The ping must have the correct version.");

  // Test the application section.
  for (let f in APPLICATION_TEST_DATA) {
    Assert.equal(aPing.application[f], APPLICATION_TEST_DATA[f],
                 f + " must have the correct value.");
  }

  // We can't check the values for channel and architecture. Just make
  // sure they are in.
  Assert.ok("architecture" in aPing.application,
            "The application section must have an architecture field.");
  Assert.ok("channel" in aPing.application,
            "The application section must have a channel field.");

  // Check the clientId and environment fields, as needed.
  Assert.equal("clientId" in aPing, aHasClientId);
  Assert.equal("environment" in aPing, aHasEnvironment);
}

function checkPayloadInfo(data) {
  const ALLOWED_REASONS = [
    "environment-change", "shutdown", "daily", "saved-session", "test-ping"
  ];
  let numberCheck = arg => { return (typeof arg == "number"); };
  let positiveNumberCheck = arg => { return numberCheck(arg) && (arg >= 0); };
  let stringCheck = arg => { return (typeof arg == "string") && (arg != ""); };
  let revisionCheck = arg => {
    return (Services.appinfo.isOfficial) ? stringCheck(arg) : (typeof arg == "string");
  };
  let uuidCheck = arg => {
    return UUID_REGEX.test(arg);
  };
  let isoDateCheck = arg => {
    // We expect use of this version of the ISO format:
    // 2015-04-12T18:51:19.1+00:00
    const isoDateRegEx = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+[+-]\d{2}:\d{2}$/;
    return stringCheck(arg) && !Number.isNaN(Date.parse(arg)) &&
           isoDateRegEx.test(arg);
  };

  const EXPECTED_INFO_FIELDS_TYPES = {
    reason: stringCheck,
    revision: revisionCheck,
    timezoneOffset: numberCheck,
    sessionId: uuidCheck,
    subsessionId: uuidCheck,
    // Special cases: previousSessionId and previousSubsessionId are null on first run.
    previousSessionId: (arg) => { return (arg) ? uuidCheck(arg) : true; },
    previousSubsessionId: (arg) => { return (arg) ? uuidCheck(arg) : true; },
    subsessionCounter: positiveNumberCheck,
    profileSubsessionCounter: positiveNumberCheck,
    sessionStartDate: isoDateCheck,
    subsessionStartDate: isoDateCheck,
    subsessionLength: positiveNumberCheck,
  };

  for (let f in EXPECTED_INFO_FIELDS_TYPES) {
    Assert.ok(f in data, f + " must be available.");

    let checkFunc = EXPECTED_INFO_FIELDS_TYPES[f];
    Assert.ok(checkFunc(data[f]),
              f + " must have the correct type and valid data " + data[f]);
  }

  // Previous buildId is not mandatory.
  if (data.previousBuildId) {
    Assert.ok(stringCheck(data.previousBuildId));
  }

  Assert.ok(ALLOWED_REASONS.find(r => r == data.reason),
            "Payload must contain an allowed reason.");

  Assert.ok(Date.parse(data.subsessionStartDate) >= Date.parse(data.sessionStartDate));
  Assert.ok(data.profileSubsessionCounter >= data.subsessionCounter);
  Assert.ok(data.timezoneOffset >= -12*60, "The timezone must be in a valid range.");
  Assert.ok(data.timezoneOffset <= 12*60, "The timezone must be in a valid range.");
}

function checkPayload(payload, reason, successfulPings, savedPings) {
  Assert.ok("info" in payload, "Payload must contain an info section.");
  checkPayloadInfo(payload.info);

  Assert.ok(payload.simpleMeasurements.totalTime >= 0);
  Assert.ok(payload.simpleMeasurements.uptime >= 0);
  Assert.equal(payload.simpleMeasurements.startupInterrupted, 1);
  Assert.equal(payload.simpleMeasurements.shutdownDuration, SHUTDOWN_TIME);
  Assert.equal(payload.simpleMeasurements.savedPings, savedPings);
  Assert.ok("maximalNumberOfConcurrentThreads" in payload.simpleMeasurements);
  Assert.ok(payload.simpleMeasurements.maximalNumberOfConcurrentThreads >= gNumberOfThreadsLaunched);

  let activeTicks = payload.simpleMeasurements.activeTicks;
  Assert.ok(activeTicks >= 0);

  Assert.equal(payload.simpleMeasurements.failedProfileLockCount,
              FAILED_PROFILE_LOCK_ATTEMPTS);
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let failedProfileLocksFile = profileDirectory.clone();
  failedProfileLocksFile.append("Telemetry.FailedProfileLocks.txt");
  Assert.ok(!failedProfileLocksFile.exists());


  let isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
  if (isWindows) {
    Assert.ok(payload.simpleMeasurements.startupSessionRestoreReadBytes > 0);
    Assert.ok(payload.simpleMeasurements.startupSessionRestoreWriteBytes > 0);
  }

  const TELEMETRY_PING = "TELEMETRY_PING";
  const TELEMETRY_SUCCESS = "TELEMETRY_SUCCESS";
  const TELEMETRY_TEST_FLAG = "TELEMETRY_TEST_FLAG";
  const TELEMETRY_TEST_COUNT = "TELEMETRY_TEST_COUNT";
  const TELEMETRY_TEST_KEYED_FLAG = "TELEMETRY_TEST_KEYED_FLAG";
  const TELEMETRY_TEST_KEYED_COUNT = "TELEMETRY_TEST_KEYED_COUNT";
  const READ_SAVED_PING_SUCCESS = "READ_SAVED_PING_SUCCESS";

  if (successfulPings > 0) {
    Assert.ok(TELEMETRY_PING in payload.histograms);
  }
  Assert.ok(READ_SAVED_PING_SUCCESS in payload.histograms);
  Assert.ok(TELEMETRY_TEST_FLAG in payload.histograms);
  Assert.ok(TELEMETRY_TEST_COUNT in payload.histograms);

  let rh = Telemetry.registeredHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, []);
  for (let name of rh) {
    if (/SQLITE/.test(name) && name in payload.histograms) {
      let histogramName = ("STARTUP_" + name);
      Assert.ok(histogramName in payload.histograms, histogramName + " must be available.");
    }
  }
  Assert.ok(!(IGNORE_HISTOGRAM in payload.histograms));
  Assert.ok(!(IGNORE_CLONED_HISTOGRAM in payload.histograms));

  // Flag histograms should automagically spring to life.
  const expected_flag = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 3,
    values: {0:1, 1:0},
    sum: 0,
    sum_squares_lo: 0,
    sum_squares_hi: 0
  };
  let flag = payload.histograms[TELEMETRY_TEST_FLAG];
  Assert.equal(uneval(flag), uneval(expected_flag));

  // We should have a test count.
  const expected_count = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 4,
    values: {0:1, 1:0},
    sum: 1,
    sum_squares_lo: 1,
    sum_squares_hi: 0,
  };
  let count = payload.histograms[TELEMETRY_TEST_COUNT];
  Assert.equal(uneval(count), uneval(expected_count));

  // There should be one successful report from the previous telemetry ping.
  if (successfulPings > 0) {
    const expected_tc = {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 2,
      values: {0:2, 1:successfulPings, 2:0},
      sum: successfulPings,
      sum_squares_lo: successfulPings,
      sum_squares_hi: 0
    };
    let tc = payload.histograms[TELEMETRY_SUCCESS];
    Assert.equal(uneval(tc), uneval(expected_tc));
  }

  let h = payload.histograms[READ_SAVED_PING_SUCCESS];
  Assert.equal(h.values[0], 1);

  // The ping should include data from memory reporters.  We can't check that
  // this data is correct, because we can't control the values returned by the
  // memory reporters.  But we can at least check that the data is there.
  //
  // It's important to check for the presence of reporters with a mix of units,
  // because TelemetryController has separate logic for each one.  But we can't
  // currently check UNITS_COUNT_CUMULATIVE or UNITS_PERCENTAGE because
  // Telemetry doesn't touch a memory reporter with these units that's
  // available on all platforms.

  Assert.ok('MEMORY_JS_GC_HEAP' in payload.histograms); // UNITS_BYTES
  Assert.ok('MEMORY_JS_COMPARTMENTS_SYSTEM' in payload.histograms); // UNITS_COUNT

  // We should have included addon histograms.
  Assert.ok("addonHistograms" in payload);
  Assert.ok(ADDON_NAME in payload.addonHistograms);
  Assert.ok(ADDON_HISTOGRAM in payload.addonHistograms[ADDON_NAME]);

  Assert.ok(("mainThread" in payload.slowSQL) &&
                ("otherThreads" in payload.slowSQL));

  // Check keyed histogram payload.

  Assert.ok("keyedHistograms" in payload);
  let keyedHistograms = payload.keyedHistograms;
  Assert.ok(TELEMETRY_TEST_KEYED_FLAG in keyedHistograms);
  Assert.ok(TELEMETRY_TEST_KEYED_COUNT in keyedHistograms);

  Assert.deepEqual({}, keyedHistograms[TELEMETRY_TEST_KEYED_FLAG]);

  const expected_keyed_count = {
    "a": {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 4,
      values: {0:2, 1:0},
      sum: 2,
      sum_squares_lo: 2,
      sum_squares_hi: 0,
    },
    "b": {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 4,
      values: {0:1, 1:0},
      sum: 1,
      sum_squares_lo: 1,
      sum_squares_hi: 0,
    },
  };
  Assert.deepEqual(expected_keyed_count, keyedHistograms[TELEMETRY_TEST_KEYED_COUNT]);
}

function writeStringToFile(file, contents) {
  let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"]
                .createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
	       RW_OWNER, ostream.DEFER_OPEN);
  ostream.write(contents, contents.length);
  ostream.QueryInterface(Ci.nsISafeOutputStream).finish();
  ostream.close();
}

function write_fake_shutdown_file() {
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let file = profileDirectory.clone();
  file.append("Telemetry.ShutdownTime.txt");
  let contents = "" + SHUTDOWN_TIME;
  writeStringToFile(file, contents);
}

function write_fake_failedprofilelocks_file() {
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let file = profileDirectory.clone();
  file.append("Telemetry.FailedProfileLocks.txt");
  let contents = "" + FAILED_PROFILE_LOCK_ATTEMPTS;
  writeStringToFile(file, contents);
}

function run_test() {
  do_test_pending();

  // Addon manager needs a profile directory
  do_get_profile();
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);

  Services.prefs.setBoolPref(PREF_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);

  // Make it look like we've previously failed to lock a profile a couple times.
  write_fake_failedprofilelocks_file();

  // Make it look like we've shutdown before.
  write_fake_shutdown_file();

  let currentMaxNumberOfThreads = Telemetry.maximalNumberOfConcurrentThreads;
  do_check_true(currentMaxNumberOfThreads > 0);

  // Try to augment the maximal number of threads currently launched
  let threads = [];
  try {
    for (let i = 0; i < currentMaxNumberOfThreads + 10; ++i) {
      threads.push(Services.tm.newThread(0));
    }
  } catch (ex) {
    // If memory is too low, it is possible that not all threads will be launched.
  }
  gNumberOfThreadsLaunched = threads.length;

  do_check_true(Telemetry.maximalNumberOfConcurrentThreads >= gNumberOfThreadsLaunched);

  do_register_cleanup(function() {
    threads.forEach(function(thread) {
      thread.shutdown();
    });
  });

  Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(run_next_test));
}

add_task(function* asyncSetup() {
  yield TelemetrySession.setup();
  yield TelemetryController.setup();
  // Load the client ID from the client ID provider to check for pings sanity.
  gClientID = yield ClientID.getClientID();
});

// Ensures that expired histograms are not part of the payload.
add_task(function* test_expiredHistogram() {
  let histogram_id = "FOOBAR";
  let dummy = Telemetry.newHistogram(histogram_id, "30", Telemetry.HISTOGRAM_EXPONENTIAL, 1, 2, 3);

  dummy.add(1);

  do_check_eq(TelemetrySession.getPayload()["histograms"][histogram_id], undefined);
  do_check_eq(TelemetrySession.getPayload()["histograms"]["TELEMETRY_TEST_EXPIRED"], undefined);
});

// Checks that an invalid histogram file is deleted if TelemetryStorage fails to parse it.
add_task(function* test_runInvalidJSON() {
  let pingFile = getSavedPingFile("invalid-histograms.dat");

  writeStringToFile(pingFile, "this.is.invalid.JSON");
  do_check_true(pingFile.exists());

  yield TelemetryStorage.testLoadHistograms(pingFile);
  do_check_false(pingFile.exists());
});

// Sends a ping to a non existing server. If we remove this test, we won't get
// all the histograms we need in the main ping.
add_task(function* test_noServerPing() {
  yield sendPing();
  // We need two pings in order to make sure STARTUP_MEMORY_STORAGE_SQLIE histograms
  // are initialised. See bug 1131585.
  yield sendPing();
});

// Checks that a sent ping is correctly received by a dummy http server.
add_task(function* test_simplePing() {
  yield clearPendingPings();
  yield TelemetrySend.reset();
  PingServer.start();
  Preferences.set(PREF_SERVER, "http://localhost:" + PingServer.port);

  let now = new Date(2020, 1, 1, 12, 0, 0);
  let expectedDate = new Date(2020, 1, 1, 0, 0, 0);
  fakeNow(now);
  const monotonicStart = fakeMonotonicNow(5000);

  const expectedSessionUUID = "bd314d15-95bf-4356-b682-b6c4a8942202";
  const expectedSubsessionUUID = "3e2e5f6c-74ba-4e4d-a93f-a48af238a8c7";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);
  yield TelemetrySession.reset();

  // Session and subsession start dates are faked during TelemetrySession setup. We can
  // now fake the session duration.
  const SESSION_DURATION_IN_MINUTES = 15;
  fakeNow(new Date(2020, 1, 1, 12, SESSION_DURATION_IN_MINUTES, 0));
  fakeMonotonicNow(monotonicStart + SESSION_DURATION_IN_MINUTES * 60 * 1000);

  yield sendPing();
  let ping = yield PingServer.promiseNextPing();

  checkPingFormat(ping, PING_TYPE_MAIN, true, true);

  // Check that we get the data we expect.
  let payload = ping.payload;
  Assert.equal(payload.info.sessionId, expectedSessionUUID);
  Assert.equal(payload.info.subsessionId, expectedSubsessionUUID);
  let sessionStartDate = new Date(payload.info.sessionStartDate);
  Assert.equal(sessionStartDate.toISOString(), expectedDate.toISOString());
  let subsessionStartDate = new Date(payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());
  Assert.equal(payload.info.subsessionLength, SESSION_DURATION_IN_MINUTES * 60);

  // Restore the UUID generator so we don't mess with other tests.
  fakeGenerateUUID(generateUUID, generateUUID);
});

// Saves the current session histograms, reloads them, performs a ping
// and checks that the dummy http server received both the previously
// saved ping and the new one.
add_task(function* test_saveLoadPing() {
  // Let's start out with a defined state.
  yield clearPendingPings();
  yield TelemetryController.reset();
  PingServer.clearRequests();

  // Setup test data and trigger pings.
  setupTestData();
  yield TelemetrySession.testSavePendingPing();
  yield sendPing();

  // Get requests received by dummy server.
  const requests = yield PingServer.promiseNextRequests(2);

  for (let req of requests) {
    Assert.equal(req.getHeader("content-type"), "application/json; charset=UTF-8",
                 "The request must have the correct content-type.");
  }

  // We decode both requests to check for the |reason|.
  let pings = [for (req of requests) decodeRequestPayload(req)];

  // Check we have the correct two requests. Ordering is not guaranteed. The ping type
  // is encoded in the URL.
  if (pings[0].type != PING_TYPE_MAIN) {
    pings.reverse();
  }

  checkPingFormat(pings[0], PING_TYPE_MAIN, true, true);
  checkPayload(pings[0].payload, REASON_TEST_PING, 0, 1);
  checkPingFormat(pings[1], PING_TYPE_SAVED_SESSION, true, true);
  checkPayload(pings[1].payload, REASON_SAVED_SESSION, 0, 0);
});

add_task(function* test_checkSubsessionHistograms() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android.
    return;
  }

  let now = new Date(2020, 1, 1, 12, 0, 0);
  let expectedDate = new Date(2020, 1, 1, 0, 0, 0);
  fakeNow(now);
  yield TelemetrySession.setup();

  const COUNT_ID = "TELEMETRY_TEST_COUNT";
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const count = Telemetry.getHistogramById(COUNT_ID);
  const keyed = Telemetry.getKeyedHistogramById(KEYED_ID);
  const registeredIds =
    new Set(Telemetry.registeredHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, []));

  const stableHistograms = new Set([
    "TELEMETRY_TEST_FLAG",
    "TELEMETRY_TEST_COUNT",
    "TELEMETRY_TEST_RELEASE_OPTOUT",
    "TELEMETRY_TEST_RELEASE_OPTIN",
    "STARTUP_CRASH_DETECTED",
  ]);

  const stableKeyedHistograms = new Set([
    "TELEMETRY_TEST_KEYED_FLAG",
    "TELEMETRY_TEST_KEYED_COUNT",
    "TELEMETRY_TEST_KEYED_RELEASE_OPTIN",
    "TELEMETRY_TEST_KEYED_RELEASE_OPTOUT",
  ]);

  // Compare the two sets of histograms.
  // The "subsession" histograms should match the registered
  // "classic" histograms. However, histograms can change
  // between us collecting the different payloads, so we only
  // check for deep equality on known stable histograms.
  checkHistograms = (classic, subsession) => {
    for (let id of Object.keys(classic)) {
      if (!registeredIds.has(id)) {
        continue;
      }

      Assert.ok(id in subsession);
      if (stableHistograms.has(id)) {
        Assert.deepEqual(classic[id],
                         subsession[id]);
      } else {
        Assert.equal(classic[id].histogram_type,
                     subsession[id].histogram_type);
      }
    }
  };

  // Same as above, except for keyed histograms.
  checkKeyedHistograms = (classic, subsession) => {
    for (let id of Object.keys(classic)) {
      if (!registeredIds.has(id)) {
        continue;
      }

      Assert.ok(id in subsession);
      if (stableKeyedHistograms.has(id)) {
        Assert.deepEqual(classic[id],
                         subsession[id]);
      }
    }
  };

  // Both classic and subsession payload histograms should start the same.
  // The payloads should be identical for now except for the reason.
  count.clear();
  keyed.clear();
  let classic = TelemetrySession.getPayload();
  let subsession = TelemetrySession.getPayload("environment-change");

  Assert.equal(classic.info.reason, "gather-payload");
  Assert.equal(subsession.info.reason, "environment-change");
  Assert.ok(!(COUNT_ID in classic.histograms));
  Assert.ok(!(COUNT_ID in subsession.histograms));
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.deepEqual(classic.keyedHistograms[KEYED_ID], {});
  Assert.deepEqual(subsession.keyedHistograms[KEYED_ID], {});

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // Adding values should get picked up in both.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 1);

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // Values should still reset properly.
  count.clear();
  keyed.clear();
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(!(COUNT_ID in classic.histograms));
  Assert.ok(!(COUNT_ID in subsession.histograms));
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.deepEqual(classic.keyedHistograms[KEYED_ID], {});

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // Adding values should get picked up in both.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 1);

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // We should be able to reset only the subsession histograms.
  // First check that "snapshot and clear" still returns the old state...
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change", true);

  let subsessionStartDate = new Date(classic.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());
  subsessionStartDate = new Date(subsession.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());
  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // ... then check that the next snapshot shows the subsession
  // histograms got reset.
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 1);
  Assert.equal(subsession.histograms[COUNT_ID].sum, 0);

  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 1);
  Assert.deepEqual(subsession.keyedHistograms[KEYED_ID], {});

  // Adding values should get picked up in both again.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 2);
  Assert.equal(subsession.histograms[COUNT_ID].sum, 1);

  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 2);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 2);
  Assert.equal(subsession.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(subsession.keyedHistograms[KEYED_ID]["b"].sum, 1);
});

add_task(function* test_checkSubsessionData() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android.
    return;
  }

  // Keep track of the active ticks count if the session recorder is available.
  let sessionRecorder = TelemetryController.getSessionRecorder();
  let activeTicksAtSubsessionStart = sessionRecorder.activeTicks;
  let expectedActiveTicks = activeTicksAtSubsessionStart;

  incrementActiveTicks = () => {
    sessionRecorder.incrementActiveTicks();
    ++expectedActiveTicks;
  }

  yield TelemetrySession.reset();

  // Both classic and subsession payload data should be the same on the first subsession.
  incrementActiveTicks();
  let classic = TelemetrySession.getPayload();
  let subsession = TelemetrySession.getPayload("environment-change");
  Assert.equal(classic.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Classic pings must count active ticks since the beginning of the session.");
  Assert.equal(subsession.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Subsessions must count active ticks as classic pings on the first subsession.");

  // Start a new subsession and check that the active ticks are correctly reported.
  incrementActiveTicks();
  activeTicksAtSubsessionStart = sessionRecorder.activeTicks;
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change", true);
  Assert.equal(classic.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Classic pings must count active ticks since the beginning of the session.");
  Assert.equal(subsession.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Pings must not loose the tick count when starting a new subsession.");

  // Get a new subsession payload without clearing the subsession.
  incrementActiveTicks();
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");
  Assert.equal(classic.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Classic pings must count active ticks since the beginning of the session.");
  Assert.equal(subsession.simpleMeasurements.activeTicks,
               expectedActiveTicks - activeTicksAtSubsessionStart,
               "Subsessions must count active ticks since the last new subsession.");
});

add_task(function* test_dailyCollection() {
  if (gIsAndroid) {
    // We don't do daily collections yet on Android.
    return;
  }

  let now = new Date(2030, 1, 1, 12, 0, 0);
  let nowDay = new Date(2030, 1, 1, 0, 0, 0);
  let schedulerTickCallback = null;

  PingServer.clearRequests();

  fakeNow(now);

  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});

  // Init and check timer.
  yield clearPendingPings();
  yield TelemetrySession.setup();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);

  // Set histograms to expected state.
  const COUNT_ID = "TELEMETRY_TEST_COUNT";
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const count = Telemetry.getHistogramById(COUNT_ID);
  const keyed = Telemetry.getKeyedHistogramById(KEYED_ID);

  count.clear();
  keyed.clear();
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  keyed.add("b", 1);

  // Make sure the daily ping gets triggered.
  let expectedDate = nowDay;
  now = futureDate(nowDay, MS_IN_ONE_DAY);
  fakeNow(now);

  Assert.ok(!!schedulerTickCallback);
  // Run a scheduler tick: it should trigger the daily ping.
  yield schedulerTickCallback();

  // Collect the daily ping.
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  let subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["b"].sum, 2);

  // The daily ping is rescheduled for "tomorrow".
  expectedDate = futureDate(expectedDate, MS_IN_ONE_DAY);
  now = futureDate(now, MS_IN_ONE_DAY);
  fakeNow(now);

  // Run a scheduler tick. Trigger and collect another ping. The histograms should be reset.
  yield schedulerTickCallback();

  ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 0);
  Assert.deepEqual(ping.payload.keyedHistograms[KEYED_ID], {});

  // Trigger and collect another daily ping, with the histograms being set again.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);

  // The daily ping is rescheduled for "tomorrow".
  expectedDate = futureDate(expectedDate, MS_IN_ONE_DAY);
  now = futureDate(now, MS_IN_ONE_DAY);
  fakeNow(now);

  yield schedulerTickCallback();
  ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["b"].sum, 1);

  // Shutdown to cleanup the aborted-session if it gets created.
  yield TelemetrySession.shutdown();
});

add_task(function* test_dailyDuplication() {
  if (gIsAndroid) {
    // We don't do daily collections yet on Android.
    return;
  }

  clearPendingPings();
  PingServer.clearRequests();

  let schedulerTickCallback = null;
  let now = new Date(2030, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.setup();

  // Make sure the daily ping gets triggered at midnight.
  // We need to make sure that we trigger this after the period where we wait for
  // the user to become idle.
  let firstDailyDue = new Date(2030, 1, 2, 0, 0, 0);
  fakeNow(firstDailyDue);

  // Run a scheduler tick: it should trigger the daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Get the first daily ping.
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);

  // We don't expect to receive any other daily ping in this test, so assert if we do.
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No more daily pings should be sent/received in this test.");
  });

  // Set the current time to a bit after midnight.
  let secondDailyDue = new Date(firstDailyDue);
  secondDailyDue.setHours(0);
  secondDailyDue.setMinutes(15);
  fakeNow(secondDailyDue);

  // Run a scheduler tick: it should NOT trigger the daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Shutdown to cleanup the aborted-session if it gets created.
  yield TelemetrySession.shutdown();
  PingServer.resetPingHandler();
});

add_task(function* test_dailyOverdue() {
  if (gIsAndroid) {
    // We don't do daily collections yet on Android.
    return;
  }

  let schedulerTickCallback = null;
  let now = new Date(2030, 1, 1, 11, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.setup();

  // Skip one hour ahead: nothing should be due.
  now.setHours(now.getHours() + 1);
  fakeNow(now);

  // Assert if we receive something!
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No daily ping should be received if not overdue!.");
  });

  // This tick should not trigger any daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Restore the non asserting ping handler.
  PingServer.resetPingHandler();
  PingServer.clearRequests();

  // Simulate an overdue ping: we're not close to midnight, but the last daily ping
  // time is too long ago.
  let dailyOverdue = new Date(2030, 1, 2, 13, 0, 0);
  fakeNow(dailyOverdue);

  // Run a scheduler tick: it should trigger the daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Get the first daily ping.
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);

  // Shutdown to cleanup the aborted-session if it gets created.
  yield TelemetrySession.shutdown();
});

add_task(function* test_environmentChange() {
  if (gIsAndroid) {
    // We don't split subsessions on environment changes yet on Android.
    return;
  }

  let now = new Date(2040, 1, 1, 12, 0, 0);
  let timerCallback = null;
  let timerDelay = null;

  clearPendingPings();
  yield TelemetrySend.reset();
  PingServer.clearRequests();

  fakeNow(now);

  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);

  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, TelemetryEnvironment.RECORD_PREF_VALUE],
  ]);

  // Setup.
  yield TelemetrySession.setup();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);
  TelemetryEnvironment._watchPreferences(PREFS_TO_WATCH);

  // Set histograms to expected state.
  const COUNT_ID = "TELEMETRY_TEST_COUNT";
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const count = Telemetry.getHistogramById(COUNT_ID);
  const keyed = Telemetry.getKeyedHistogramById(KEYED_ID);

  count.clear();
  keyed.clear();
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);

  // Trigger and collect environment-change ping.
  let startDay = truncateDateToDays(now);
  now = futureDate(now, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(now);

  Preferences.set(PREF_TEST, 1);
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.environment.settings.userPrefs[PREF_TEST], undefined);
  Assert.equal(ping.payload.info.reason, REASON_ENVIRONMENT_CHANGE);
  let subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), startDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);

  // Trigger and collect another ping. The histograms should be reset.
  startDay = truncateDateToDays(now);
  now = futureDate(now, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(now);

  Preferences.set(PREF_TEST, 2);
  ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.environment.settings.userPrefs[PREF_TEST], 1);
  Assert.equal(ping.payload.info.reason, REASON_ENVIRONMENT_CHANGE);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), startDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 0);
  Assert.deepEqual(ping.payload.keyedHistograms[KEYED_ID], {});
});

add_task(function* test_savedPingsOnShutdown() {
  // On desktop, we expect both "saved-session" and "shutdown" pings. We only expect
  // the former on Android.
  const expectedPingCount = (gIsAndroid) ? 1 : 2;
  // Assure that we store the ping properly when saving sessions on shutdown.
  // We make the TelemetrySession shutdown to trigger a session save.
  const dir = TelemetryStorage.pingDirectoryPath;
  yield OS.File.removeDir(dir, {ignoreAbsent: true});
  yield OS.File.makeDir(dir);
  yield TelemetrySession.shutdown();

  PingServer.clearRequests();
  yield TelemetryController.reset();

  const pings = yield PingServer.promiseNextPings(expectedPingCount);

  for (let ping of pings) {
    Assert.ok("type" in ping);

    let expectedReason =
      (ping.type == PING_TYPE_SAVED_SESSION) ? REASON_SAVED_SESSION : REASON_SHUTDOWN;

    checkPingFormat(ping, ping.type, true, true);
    Assert.equal(ping.payload.info.reason, expectedReason);
    Assert.equal(ping.clientId, gClientID);
  }
});

add_task(function* test_savedSessionData() {
  // Create the directory which will contain the data file, if it doesn't already
  // exist.
  yield OS.File.makeDir(DATAREPORTING_PATH);
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_LOAD").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_PARSE").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").clear();

  // Write test data to the session data file.
  const dataFilePath = OS.Path.join(DATAREPORTING_PATH, "session-state.json");
  const sessionState = {
    sessionId: null,
    subsessionId: null,
    profileSubsessionCounter: 3785,
  };
  yield CommonUtils.writeJSON(sessionState, dataFilePath);

  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, TelemetryEnvironment.RECORD_PREF_VALUE],
  ]);

  // We expect one new subsession when starting TelemetrySession and one after triggering
  // an environment change.
  const expectedSubsessions = sessionState.profileSubsessionCounter + 2;
  const expectedSessionUUID = "ff602e52-47a1-b7e8-4c1a-ffffffffc87a";
  const expectedSubsessionUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);

  if (gIsAndroid) {
    // We don't support subsessions yet on Android, so skip the next checks.
    return;
  }

  // Start TelemetrySession so that it loads the session data file.
  yield TelemetrySession.reset();
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  // Watch a test preference, trigger and environment change and wait for it to propagate.
  // _watchPreferences triggers a subsession notification
  fakeNow(new Date(2050, 1, 1, 12, 0, 0));
  TelemetryEnvironment._watchPreferences(PREFS_TO_WATCH);
  let changePromise = new Promise(resolve =>
    TelemetryEnvironment.registerChangeListener("test_fake_change", resolve));
  Preferences.set(PREF_TEST, 1);
  yield changePromise;
  TelemetryEnvironment.unregisterChangeListener("test_fake_change");

  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, expectedSubsessions);
  yield TelemetrySession.shutdown();

  // Restore the UUID generator so we don't mess with other tests.
  fakeGenerateUUID(generateUUID, generateUUID);

  // Load back the serialised session data.
  let data = yield CommonUtils.readJSON(dataFilePath);
  Assert.equal(data.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(data.sessionId, expectedSessionUUID);
  Assert.equal(data.subsessionId, expectedSubsessionUUID);
});

add_task(function* test_sessionData_ShortSession() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android, so skip the next checks.
    return;
  }

  const SESSION_STATE_PATH = OS.Path.join(DATAREPORTING_PATH, "session-state.json");

  // Shut down Telemetry and remove the session state file.
  yield TelemetrySession.shutdown();
  yield OS.File.remove(SESSION_STATE_PATH, { ignoreAbsent: true });
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_LOAD").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_PARSE").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").clear();

  const expectedSessionUUID = "ff602e52-47a1-b7e8-4c1a-ffffffffc87a";
  const expectedSubsessionUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);

  // We intentionally don't wait for the setup to complete and shut down to simulate
  // short sessions. We expect the profile subsession counter to be 1.
  TelemetrySession.reset();
  yield TelemetrySession.shutdown();

  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  // Restore the UUID generation functions.
  fakeGenerateUUID(generateUUID, generateUUID);

  // Start TelemetrySession so that it loads the session data file. We expect the profile
  // subsession counter to be incremented by 1 again.
  yield TelemetrySession.reset();

  // We expect 2 profile subsession counter updates.
  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, 2);
  Assert.equal(payload.info.previousSessionId, expectedSessionUUID);
  Assert.equal(payload.info.previousSubsessionId, expectedSubsessionUUID);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);
});

add_task(function* test_invalidSessionData() {
  // Create the directory which will contain the data file, if it doesn't already
  // exist.
  yield OS.File.makeDir(DATAREPORTING_PATH);
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_LOAD").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_PARSE").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").clear();

  // Write test data to the session data file. This should fail to parse.
  const dataFilePath = OS.Path.join(DATAREPORTING_PATH, "session-state.json");
  const unparseableData = "{asdf:@äü";
  OS.File.writeAtomic(dataFilePath, unparseableData,
                      {encoding: "utf-8", tmpPath: dataFilePath + ".tmp"});

  // Start TelemetrySession so that it loads the session data file.
  yield TelemetrySession.reset();

  // The session data file should not load. Only expect the current subsession.
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  // Write test data to the session data file. This should fail validation.
  const sessionState = {
    profileSubsessionCounter: "not-a-number?",
    someOtherField: 12,
  };
  yield CommonUtils.writeJSON(sessionState, dataFilePath);

  // The session data file should not load. Only expect the current subsession.
  const expectedSubsessions = 1;
  const expectedSessionUUID = "ff602e52-47a1-b7e8-4c1a-ffffffffc87a";
  const expectedSubsessionUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);

  // Start TelemetrySession so that it loads the session data file.
  yield TelemetrySession.reset();

  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  yield TelemetrySession.shutdown();

  // Restore the UUID generator so we don't mess with other tests.
  fakeGenerateUUID(generateUUID, generateUUID);

  // Load back the serialised session data.
  let data = yield CommonUtils.readJSON(dataFilePath);
  Assert.equal(data.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(data.sessionId, expectedSessionUUID);
  Assert.equal(data.subsessionId, expectedSubsessionUUID);
});

add_task(function* test_abortedSession() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Make sure the aborted sessions directory does not exist to test its creation.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  let schedulerTickCallback = null;
  let now = new Date(2040, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.reset();

  Assert.ok((yield OS.File.exists(DATAREPORTING_PATH)),
            "Telemetry must create the aborted session directory when starting.");

  // Fake now again so that the scheduled aborted-session save takes place.
  now = futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS);
  fakeNow(now);
  // The first aborted session checkpoint must take place right after the initialisation.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();
  // Check that the aborted session is due at the correct time.
  Assert.ok((yield OS.File.exists(ABORTED_FILE)),
            "There must be an aborted session ping.");

  // This ping is not yet in the pending pings folder, so we can't access it using
  // TelemetryStorage.popPendingPings().
  let pingContent = yield OS.File.read(ABORTED_FILE, { encoding: "utf-8" });
  let abortedSessionPing = JSON.parse(pingContent);

  // Validate the ping.
  checkPingFormat(abortedSessionPing, PING_TYPE_MAIN, true, true);
  Assert.equal(abortedSessionPing.payload.info.reason, REASON_ABORTED_SESSION);

  // Trigger a another aborted-session ping and check that it overwrites the previous one.
  now = futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS);
  fakeNow(now);
  yield schedulerTickCallback();

  pingContent = yield OS.File.read(ABORTED_FILE, { encoding: "utf-8" });
  let updatedAbortedSessionPing = JSON.parse(pingContent);
  checkPingFormat(updatedAbortedSessionPing, PING_TYPE_MAIN, true, true);
  Assert.equal(updatedAbortedSessionPing.payload.info.reason, REASON_ABORTED_SESSION);
  Assert.notEqual(abortedSessionPing.id, updatedAbortedSessionPing.id);
  Assert.notEqual(abortedSessionPing.creationDate, updatedAbortedSessionPing.creationDate);

  yield TelemetrySession.shutdown();
  Assert.ok(!(yield OS.File.exists(ABORTED_FILE)),
            "No aborted session ping must be available after a shutdown.");

  // Write the ping to the aborted-session file. TelemetrySession will add it to the
  // saved pings directory when it starts.
  yield TelemetryStorage.savePingToFile(abortedSessionPing, ABORTED_FILE, false);

  yield clearPendingPings();
  PingServer.clearRequests();
  yield TelemetrySession.reset();
  yield TelemetryController.reset();

  Assert.ok(!(yield OS.File.exists(ABORTED_FILE)),
            "The aborted session ping must be removed from the aborted session ping directory.");

  // We should have received an aborted-session ping.
  const receivedPing = yield PingServer.promiseNextPing();
  Assert.equal(receivedPing.type, PING_TYPE_MAIN, "Should have the correct type");
  Assert.equal(receivedPing.payload.info.reason, REASON_ABORTED_SESSION, "Ping should have the correct reason");

  yield TelemetrySession.shutdown();
});

add_task(function* test_abortedSession_Shutdown() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  let schedulerTickCallback = null;
  let now = fakeNow(2040, 1, 1, 0, 0, 0);
  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.reset();

  Assert.ok((yield OS.File.exists(DATAREPORTING_PATH)),
            "Telemetry must create the aborted session directory when starting.");

  // Fake now again so that the scheduled aborted-session save takes place.
  now = fakeNow(futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS));
  // The first aborted session checkpoint must take place right after the initialisation.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();
  // Check that the aborted session is due at the correct time.
  Assert.ok((yield OS.File.exists(ABORTED_FILE)), "There must be an aborted session ping.");

  // Remove the aborted session file and then shut down to make sure exceptions (e.g file
  // not found) do not compromise the shutdown.
  yield OS.File.remove(ABORTED_FILE);

  yield TelemetrySession.shutdown();
});

add_task(function* test_abortedDailyCoalescing() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Make sure the aborted sessions directory does not exist to test its creation.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  let schedulerTickCallback = null;
  PingServer.clearRequests();

  let nowDate = new Date(2009, 10, 18, 0, 0, 0);
  fakeNow(nowDate);

  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.reset();

  Assert.ok((yield OS.File.exists(DATAREPORTING_PATH)),
            "Telemetry must create the aborted session directory when starting.");

  // Delay the callback around midnight so that the aborted-session ping gets merged with the
  // daily ping.
  let dailyDueDate = futureDate(nowDate, MS_IN_ONE_DAY);
  fakeNow(dailyDueDate);
  // Trigger both the daily ping and the saved-session.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();

  // Wait for the daily ping.
  let dailyPing = yield PingServer.promiseNextPing();
  Assert.equal(dailyPing.payload.info.reason, REASON_DAILY);

  // Check that an aborted session ping was also written to disk.
  Assert.ok((yield OS.File.exists(ABORTED_FILE)),
            "There must be an aborted session ping.");

  // Read aborted session ping and check that the session/subsession ids equal the
  // ones in the daily ping.
  let pingContent = yield OS.File.read(ABORTED_FILE, { encoding: "utf-8" });
  let abortedSessionPing = JSON.parse(pingContent);
  Assert.equal(abortedSessionPing.payload.info.sessionId, dailyPing.payload.info.sessionId);
  Assert.equal(abortedSessionPing.payload.info.subsessionId, dailyPing.payload.info.subsessionId);

  yield TelemetrySession.shutdown();
});

add_task(function* test_schedulerComputerSleep() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  clearPendingPings();
  yield TelemetrySend.reset();
  PingServer.clearRequests();

  // Remove any aborted-session ping from the previous tests.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  // Set a fake current date and start Telemetry.
  let nowDate = new Date(2009, 10, 18, 0, 0, 0);
  fakeNow(nowDate);
  let schedulerTickCallback = null;
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.reset();

  // Set the current time 3 days in the future at midnight, before running the callback.
  let future = futureDate(nowDate, MS_IN_ONE_DAY * 3);
  fakeNow(future);
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();

  let dailyPing = yield PingServer.promiseNextPing();
  Assert.equal(dailyPing.payload.info.reason, REASON_DAILY);

  Assert.ok((yield OS.File.exists(ABORTED_FILE)),
            "There must be an aborted session ping.");

  yield TelemetrySession.shutdown();
});

add_task(function* test_schedulerEnvironmentReschedules() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  // Reset the test preference.
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, TelemetryEnvironment.RECORD_PREF_VALUE],
  ]);

  yield clearPendingPings();
  PingServer.clearRequests();

  // Set a fake current date and start Telemetry.
  let nowDate = new Date(2060, 10, 18, 0, 0, 0);
  fakeNow(nowDate);
  let schedulerTickCallback = null;
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.reset();
  TelemetryEnvironment._watchPreferences(PREFS_TO_WATCH);

  // Set the current time at midnight.
  let future = futureDate(nowDate, MS_IN_ONE_DAY);
  fakeNow(future);

  // Trigger the environment change.
  Preferences.set(PREF_TEST, 1);

  // Wait for the environment-changed ping.
  yield PingServer.promiseNextPing();

  // We don't expect to receive any daily ping in this test, so assert if we do.
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No ping should be sent/received in this test.");
  });

  // Execute one scheduler tick. It should not trigger a daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  yield TelemetrySession.shutdown();
});

add_task(function* test_schedulerNothingDue() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Remove any aborted-session ping from the previous tests.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });
  yield clearPendingPings();

  // We don't expect to receive any ping in this test, so assert if we do.
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No ping should be sent/received in this test.");
  });

  // Set a current date/time away from midnight, so that the daily ping doesn't get
  // sent.
  let nowDate = new Date(2009, 10, 18, 11, 0, 0);
  fakeNow(nowDate);
  let schedulerTickCallback = null;
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetrySession.reset();

  // Delay the callback execution to a time when no ping should be due.
  let nothingDueDate = futureDate(nowDate, ABORTED_SESSION_UPDATE_INTERVAL_MS / 2);
  fakeNow(nothingDueDate);
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();

  // Check that no aborted session ping was written to disk.
  Assert.ok(!(yield OS.File.exists(ABORTED_FILE)));

  yield TelemetrySession.shutdown();
  PingServer.resetPingHandler();
});

add_task(function* test_pingExtendedStats() {
  const EXTENDED_PAYLOAD_FIELDS = [
    "chromeHangs", "threadHangStats", "log", "slowSQL", "fileIOReports", "lateWrites",
    "addonHistograms", "addonDetails", "UIMeasurements",
  ];

  // Disable sending extended statistics.
  Telemetry.canRecordExtended = false;

  yield clearPendingPings();
  PingServer.clearRequests();
  yield TelemetrySession.reset();
  yield sendPing();

  let ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, PING_TYPE_MAIN, true, true);

  // Check that the payload does not contain extended statistics fields.
  for (let f in EXTENDED_PAYLOAD_FIELDS) {
    Assert.ok(!(EXTENDED_PAYLOAD_FIELDS[f] in ping.payload),
              EXTENDED_PAYLOAD_FIELDS[f] + " must not be in the payload if the extended set is off.");
  }

  // We check this one separately so that we can reuse EXTENDED_PAYLOAD_FIELDS below, since
  // slowSQLStartup might not be there.
  Assert.ok(!("slowSQLStartup" in ping.payload),
            "slowSQLStartup must not be sent if the extended set is off");

  Assert.ok(!("addonManager" in ping.payload.simpleMeasurements),
            "addonManager must not be sent if the extended set is off.");
  Assert.ok(!("UITelemetry" in ping.payload.simpleMeasurements),
            "UITelemetry must not be sent if the extended set is off.");

  // Restore the preference.
  Telemetry.canRecordExtended = true;

  // Send a new ping that should contain the extended data.
  yield TelemetrySession.reset();
  yield sendPing();
  ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, PING_TYPE_MAIN, true, true);

  // Check that the payload now contains extended statistics fields.
  for (let f in EXTENDED_PAYLOAD_FIELDS) {
    Assert.ok(EXTENDED_PAYLOAD_FIELDS[f] in ping.payload,
              EXTENDED_PAYLOAD_FIELDS[f] + " must be in the payload if the extended set is on.");
  }

  Assert.ok("addonManager" in ping.payload.simpleMeasurements,
            "addonManager must be sent if the extended set is on.");
  Assert.ok("UITelemetry" in ping.payload.simpleMeasurements,
            "UITelemetry must be sent if the extended set is on.");
});

add_task(function* test_schedulerUserIdle() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const SCHEDULER_TICK_INTERVAL_MS = 5 * 60 * 1000;
  const SCHEDULER_TICK_IDLE_INTERVAL_MS = 60 * 60 * 1000;

  let now = new Date(2010, 1, 1, 11, 0, 0);
  fakeNow(now);

  let schedulerTimeout = 0;
  fakeSchedulerTimer((callback, timeout) => {
    schedulerTimeout = timeout;
  }, () => {});
  yield TelemetrySession.reset();
  yield clearPendingPings();
  PingServer.clearRequests();

  // When not idle, the scheduler should have a 5 minutes tick interval.
  Assert.equal(schedulerTimeout, SCHEDULER_TICK_INTERVAL_MS);

  // Send an "idle" notification to the scheduler.
  fakeIdleNotification("idle");

  // When idle, the scheduler should have a 1hr tick interval.
  Assert.equal(schedulerTimeout, SCHEDULER_TICK_IDLE_INTERVAL_MS);

  // Send an "active" notification to the scheduler.
  fakeIdleNotification("active");

  // When user is back active, the scheduler tick should be 5 minutes again.
  Assert.equal(schedulerTimeout, SCHEDULER_TICK_INTERVAL_MS);

  // We should not miss midnight when going to idle.
  now.setHours(23);
  now.setMinutes(50);
  fakeNow(now);
  fakeIdleNotification("idle");
  Assert.equal(schedulerTimeout, 10 * 60 * 1000);

  yield TelemetrySession.shutdown();
});

add_task(function* stopServer(){
  yield PingServer.stop();
  do_test_finished();
});
