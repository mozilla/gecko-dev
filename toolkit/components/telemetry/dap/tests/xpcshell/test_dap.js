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

let received = false;
let server;
let server_addr;

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
  received = true;
  response.setStatusLine(request.httpVersion, 200);
}

add_setup(async function () {
  do_get_profile();
  Services.fog.initializeFOG();

  // Set up a mock server to represent the DAP endpoints.
  server = new HttpServer();
  server.registerPrefixHandler("/leader_endpoint/tasks/", uploadHandler);
  server.start(-1);

  const orig_leader = Services.prefs.getStringPref(PREF_LEADER);
  const orig_helper = Services.prefs.getStringPref(PREF_HELPER);
  const i = server.identity;
  server_addr = i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort;
  Services.prefs.setStringPref(PREF_LEADER, server_addr + "/leader_endpoint");
  Services.prefs.setStringPref(PREF_HELPER, server_addr + "/helper_endpoint");
  registerCleanupFunction(() => {
    Services.prefs.setStringPref(PREF_LEADER, orig_leader);
    Services.prefs.setStringPref(PREF_HELPER, orig_helper);

    return new Promise(resolve => {
      server.stop(resolve);
    });
  });
});

add_task(async function testVerificationTask() {
  Services.fog.testResetFOG();

  await lazy.DAPTelemetrySender.sendTestReports(tasks, 5000);

  Assert.ok(received, "Report upload successful.");
});

add_task(async function testNetworkError() {
  Services.fog.testResetFOG();
  Services.prefs.setStringPref(PREF_LEADER, server_addr + "/invalid-endpoint");

  let thrownErr;
  try {
    await lazy.DAPTelemetrySender.sendTestReports(tasks, 5000);
  } catch (e) {
    thrownErr = e;
  }

  Assert.ok(thrownErr.message.startsWith("Sending failed."));
});
