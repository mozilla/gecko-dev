/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DAPTelemetrySender: "resource://gre/modules/DAPTelemetrySender.sys.mjs",
});

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

const PREF_LEADER = "toolkit.telemetry.dap.leader.url";
const PREF_HELPER = "toolkit.telemetry.dap.helper.url";

const PREF_DATAUPLOAD = "datareporting.healthreport.uploadEnabled";

let server;
let server_addr;

// The dummy test server will record report sizes in this list.
let server_requests = [];

// List of testing task configurations. These are fake IDs for use in this files
// test server only.
const tasks = [
  {
    id: "QjMD4n8l_MHBoLrbCfLTFi8hC264fC59SKHPviPF0q8",
    vdaf: "sum",
    bits: 8,
    time_precision: 300,
  },
  {
    id: "52TTU9GPOA_eTiPJePk5RNauQI4EWCnzixAXe3LEz7o",
    vdaf: "sumvec",
    bits: 1,
    length: 20,
    time_precision: 300,
  },
  {
    id: "DSZGMFh26hBYXNaKvhL_N4AHA3P5lDn19on1vFPBxJM",
    vdaf: "sumvec",
    bits: 8,
    length: 20,
    time_precision: 300,
  },
  {
    id: "RnywY1X4s1vtspu6B8C1FOu_jJZhJO6V8L3PT3WepF4",
    vdaf: "sumvec",
    bits: 16,
    length: 20,
    time_precision: 300,
  },
  {
    id: "o-91EcR2kfxfAmkKPPHifXKqiH7Upm0Ilw5joB3L_pE",
    vdaf: "histogram",
    length: 30,
    time_precision: 300,
  },
];

// Expected payload sizes of DAP reports for the above tasks.
const task_report_sizes = [886, 902, 3654, 6566, 1126];

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

add_task(async function testVerificationTask() {
  server_requests = [];
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.deepEqual(
    server_requests,
    task_report_sizes,
    "Report upload successful."
  );
});

add_task(async function testNetworkError() {
  const test_leader = Services.prefs.getStringPref(PREF_LEADER);
  Services.prefs.setStringPref(PREF_LEADER, server_addr + "/invalid-endpoint");

  server_requests = [];

  let thrownErr;
  try {
    await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  } catch (e) {
    thrownErr = e;
  }

  Assert.deepEqual(server_requests, []);
  Assert.ok(thrownErr.message.startsWith("Sending failed."));

  Services.prefs.setStringPref(PREF_LEADER, test_leader);
});

add_task(async function testTelemetryToggle() {
  // Normal
  server_requests = [];
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.deepEqual(server_requests, task_report_sizes);

  // Telemetry off
  server_requests = [];
  Services.prefs.setBoolPref(PREF_DATAUPLOAD, false);
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.deepEqual(server_requests, []);

  // Normal
  server_requests = [];
  Services.prefs.clearUserPref(PREF_DATAUPLOAD);
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.deepEqual(server_requests, task_report_sizes);
});
