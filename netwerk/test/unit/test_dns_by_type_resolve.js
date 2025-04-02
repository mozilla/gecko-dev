/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let h2Port;

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_setup(async function setup() {
  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  trr_test_setup();
  registerCleanupFunction(() => {
    trr_clear_prefs();
  });

  if (mozinfo.socketprocess_networking) {
    Services.dns; // Needed to trigger socket process.
    await TestUtils.waitForCondition(() => Services.io.socketProcessLaunched);
  }

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
});

let test_answer = "bXkgdm9pY2UgaXMgbXkgcGFzc3dvcmQ=";

add_task(async function testTXTResolve() {
  // use the h2 server as DOH provider
  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/doh"
  );

  let { inRecord } = await new TRRDNSListener("_esni.example.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_TXT,
  });

  let answer = inRecord
    .QueryInterface(Ci.nsIDNSTXTRecord)
    .getRecordsAsOneString();
  Assert.equal(answer, test_answer, "got correct answer");
});
