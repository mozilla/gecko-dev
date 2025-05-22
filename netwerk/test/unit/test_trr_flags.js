"use strict";

/* import-globals-from trr_common.js */

let trrServer;
add_setup(async function setup() {
  trr_test_setup();

  registerCleanupFunction(async () => {
    if (trrServer) {
      await trrServer.stop();
    }
    trr_clear_prefs();
  });
  Services.prefs.setBoolPref("network.dns.always_ai_canonname", true);
});

add_task(async function test_canonical_flag() {
  trrServer = new TRRServer();
  await trrServer.start();

  await trrServer.registerDoHAnswers("testdomain.com", "A", {
    answers: [
      {
        name: "testdomain.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "5.5.5.5",
      },
    ],
  });

  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  // Disable backup connection
  Services.prefs.setBoolPref("network.dns.disableIPv6", true);

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);

  await new TRRDNSListener("testdomain.com", {
    expectedAnswer: "5.5.5.5",
    flags: Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
  });
  let reqCount = await trrServer.requestCount("testdomain.com", "A");

  await trrServer.registerDoHAnswers("testdomain.com", "A", {
    answers: [
      {
        name: "testdomain.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.1.1.1",
      },
    ],
  });

  // Expect to get cached entry
  await new TRRDNSListener("testdomain.com", {
    expectedAnswer: "5.5.5.5",
    flags: Ci.nsIDNSService.RESOLVE_SPECULATE,
  });
  Assert.equal(
    reqCount,
    await trrServer.requestCount("testdomain.com", "A"),
    "no new request should be made"
  );
});
