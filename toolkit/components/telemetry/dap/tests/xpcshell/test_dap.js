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
let server_requests = 0;

const tasks = [
  {
    // this is testing task 1
    id: "QjMD4n8l_MHBoLrbCfLTFi8hC264fC59SKHPviPF0q8",
    leader_endpoint: null,
    helper_endpoint: null,
    time_precision: 300,
    measurement_type: "u8",
  },
  {
    // this is testing task 2
    id: "DSZGMFh26hBYXNaKvhL_N4AHA3P5lDn19on1vFPBxJM",
    leader_endpoint: null,
    helper_endpoint: null,
    time_precision: 300,
    measurement_type: "vecu8",
  },
];

function uploadHandler(request, response) {
  Assert.equal(
    request.getHeader("Content-Type"),
    "application/dap-report",
    "Wrong Content-Type header."
  );

  let body = new BinaryInputStream(request.bodyInputStream);
  console.log(body.available());
  Assert.equal(
    true,
    body.available() == 886 || body.available() == 3654,
    "Wrong request body size."
  );

  server_requests += 1;

  response.setStatusLine(request.httpVersion, 200);
}

add_setup(async function () {
  do_get_profile();
  Services.fog.initializeFOG();

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
  Services.fog.testResetFOG();

  server_requests = 0;
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.equal(server_requests, tasks.length, "Report upload successful.");
});

add_task(async function testNetworkError() {
  Services.fog.testResetFOG();

  const test_leader = Services.prefs.getStringPref(PREF_LEADER);
  Services.prefs.setStringPref(PREF_LEADER, server_addr + "/invalid-endpoint");

  let thrownErr;
  try {
    await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  } catch (e) {
    thrownErr = e;
  }

  Assert.ok(thrownErr.message.startsWith("Sending failed."));

  Services.prefs.setStringPref(PREF_LEADER, test_leader);
});

add_task(async function testTelemetryToggle() {
  Services.fog.testResetFOG();

  // Normal
  server_requests = 0;
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.equal(server_requests, tasks.length);

  // Telemetry off
  server_requests = 0;
  Services.prefs.setBoolPref(PREF_DATAUPLOAD, false);
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.equal(server_requests, 0);

  // Normal
  server_requests = 0;
  Services.prefs.clearUserPref(PREF_DATAUPLOAD);
  await lazy.DAPTelemetrySender.sendTestReports(tasks, { timeout: 5000 });
  Assert.equal(server_requests, tasks.length);
});
