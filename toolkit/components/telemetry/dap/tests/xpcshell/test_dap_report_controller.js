/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const { DAPReportController, Task } = ChromeUtils.importESModule(
  "resource://gre/modules/DAPReportController.sys.mjs"
);

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

const PREF_LEADER = "toolkit.telemetry.dap.leader.url";
const PREF_HELPER = "toolkit.telemetry.dap.helper.url";

let server;
let server_addr;

// The dummy test server will record report sizes in this list.
let server_requests = [];

function uploadHandler(request, response) {
  Assert.equal(
    request.getHeader("Content-Type"),
    "application/dap-report",
    "Wrong Content-Type header."
  );

  let body = new BinaryInputStream(request.bodyInputStream);
  server_requests.push(body.available());
  response.setStatusLine(request.httpVersion, 200);
}

add_setup(async function () {
  do_get_profile();
  // Set up a mock server to represent the DAP endpoints.
  server = new HttpServer();
  server.registerPrefixHandler("/leader_endpoint/tasks/", uploadHandler);
  server.start(-1);
  const i = server.identity;
  server_addr = i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort;

  Services.prefs.setStringPref(PREF_LEADER, server_addr + "/leader_endpoint");
  Services.prefs.setStringPref(PREF_HELPER, server_addr + "/helper_endpoint");
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_LEADER);
    Services.prefs.clearUserPref(PREF_HELPER);

    return new Promise(resolve => {
      server.stop(resolve);
    });
  });
});

function openDatabase() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open("SubmissionCap", 1);

    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

function countRecords(store) {
  return new Promise((resolve, reject) => {
    const request = store.count();
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

async function getReportCount(db) {
  try {
    const tx = db.transaction("reports", "readonly");
    const store = tx.objectStore("reports");
    const count = await countRecords(store);
    await tx.done;
    return count;
  } catch (err) {
    if (err.name === "NotFoundError") {
      console.error("Object store 'reports' not found");
    } else {
      console.error("IndexedDB access error:", err);
    }
    // Cannot return 0 since 0 is a valid result.
    return Number.MAX_SAFE_INTEGER;
  }
}

async function getFreqCapCount(db) {
  try {
    const tx = db.transaction("freq_caps", "readonly");
    const store = tx.objectStore("freq_caps");
    const count = await countRecords(store);
    await tx.done;
    return count;
  } catch (err) {
    if (err.name === "NotFoundError") {
      console.error("Object store 'freq_caps' not found");
    } else {
      console.error("IndexedDB access error:", err);
    }
    // Cannot return 0 since 0 is a valid result.
    return Number.MAX_SAFE_INTEGER;
  }
}

add_task(async function testSupportedVdafs() {
  server_requests = [];
  const histogram_task = new Task({
    taskId: "QjMD4n8l_MHBoLrbCfLTFi8hC264fC59SKHPviPF0q8",
    vdaf: "histogram",
    length: 30,
    defaultMeasurement: 0,
  });
  const sumvec_task = new Task({
    taskId: "52TTU9GPOA_eTiPJePk5RNauQI4EWCnzixAXe3LEz7o",
    vdaf: "sumvec",
    length: 20,
    bits: 8,
    defaultMeasurement: [
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    ],
  });
  const sum_task = new Task({
    taskId: "RnywY1X4s1vtspu6B8C1FOu_jJZhJO6V8L3PT3WepF4",
    vdaf: "sum",
    bits: 8,
    defaultMeasurement: 0,
  });

  let tasks = {};
  tasks[histogram_task._taskId] = histogram_task;
  tasks[sumvec_task._taskId] = sumvec_task;
  tasks[sum_task._taskId] = sum_task;

  let fakeTimestamp = new Date("1970-01-01T00:00:00Z").getTime();
  let dapReportContoller = new DAPReportController({
    tasks,
    options: {
      windowDays: 7,
      submissionIntervalMins: 240,
    },
    DateNowFn: () => fakeTimestamp,
  });

  await dapReportContoller.startTimedSubmission();

  // The detailed test validations are only performed for histogram_task,
  // The other tasks verify the submission of the default measurement.

  // Initial state validation
  const db = await openDatabase();
  let numRecords = await getReportCount(db);
  Assert.ok(numRecords == 0, "Should be no pending reports");

  // Record a measurment and validate pending report
  await dapReportContoller.recordMeasurement(histogram_task._taskId, 1);
  numRecords = await getReportCount(db);
  Assert.ok(numRecords == 1, "Should be 1 pending report");

  let report = await dapReportContoller.getReportToSubmit(
    histogram_task._taskId
  );
  let expectedReport = {
    taskId: histogram_task._taskId,
    vdaf: histogram_task._vdaf,
    bits: undefined,
    length: histogram_task._length,
    measurement: 1,
  };
  Assert.deepEqual(
    report,
    expectedReport,
    "Pending report should match recorded task and measurement"
  );

  // Submit the measurement
  await dapReportContoller.submit(1000, "unit-test");

  // State validation
  numRecords = await getReportCount(db);
  Assert.ok(numRecords == 0, "Should be 0 pending reports after submit");

  // Verify submission capping is active
  numRecords = await getFreqCapCount(db);
  Assert.ok(
    numRecords == 1,
    "Frequency capping should indicate a report has been submitted"
  );
  let freqCap = await dapReportContoller.getFreqCap(histogram_task._taskId);
  let expectedFreqCap = {
    taskId: histogram_task._taskId,
    nextReset: fakeTimestamp + 7 * 24 * 60 * 60 * 1000,
  };
  Assert.deepEqual(
    freqCap,
    expectedFreqCap,
    "Cap should indicate nextReset time."
  );

  // Record another measurement
  await dapReportContoller.recordMeasurement(histogram_task._taskId, 2);
  numRecords = await getReportCount(db);
  Assert.ok(
    numRecords == 0,
    "Should be 0 pending reports due to submission with freq cap window"
  );

  // Advance now() by more than windowDays ( 7 days + 1 ms)
  fakeTimestamp = fakeTimestamp + 7 * 24 * 60 * 60 * 1000 + 1;

  // Record another measurement and validate pending report
  await dapReportContoller.recordMeasurement(histogram_task._taskId, 3);

  numRecords = await getReportCount(db);
  Assert.ok(numRecords == 1, "Should be 1 pending report");
  report = await dapReportContoller.getReportToSubmit(histogram_task._taskId);
  expectedReport = {
    taskId: histogram_task._taskId,
    vdaf: histogram_task._vdaf,
    bits: undefined,
    length: histogram_task._length,
    measurement: 3,
  };
  Assert.deepEqual(
    report,
    expectedReport,
    "Pending report should match recorded task and measurement"
  );

  // Submit the measurement
  await dapReportContoller.submit(1000, "unit-test");

  numRecords = await getReportCount(db);
  Assert.ok(
    numRecords == 0,
    "Should be 0 pending reports since now() is beyond the freq cap window"
  );

  // Verify submission capping is active
  numRecords = await getFreqCapCount(db);
  Assert.ok(numRecords == 1, "Should be 1 cap entry");
  freqCap = await dapReportContoller.getFreqCap(histogram_task._taskId);
  expectedFreqCap = {
    taskId: histogram_task._taskId,
    nextReset: fakeTimestamp + 7 * 24 * 60 * 60 * 1000,
  };
  Assert.deepEqual(
    freqCap,
    expectedFreqCap,
    "Cap should indicate nextReset time."
  );

  // Stop submission timer and cleanup.
  await dapReportContoller.cleanup(30 * 1000, "stop collector");

  //Verify IndexDB Cleanup after unenrollment
  numRecords = await getFreqCapCount(db);
  Assert.ok(numRecords == 0, "Should be 0 freq cap entries after cleanup");

  //Report
  numRecords = await getReportCount(db);
  Assert.ok(numRecords == 0, "Should be 0 pending reports after cleanup");

  // There are 4 server requests per task (the sizes match the vdaf used):
  // 1.  The first request is submitted on enrollment
  // 2.  The second request is force submitted
  // 3.  The third request is force submitted
  // 4.  The last request is submitted on unenrollment
  Assert.deepEqual(
    server_requests.sort((a, b) => a - b),
    [886, 886, 886, 886, 1126, 1126, 1126, 1126, 3654, 3654, 3654, 3654],
    "Should have one report on dapReportContoller create, second for recorded measurement, third on cleanup"
  );
});
