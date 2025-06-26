"use strict";

/* import-globals-from trr_common.js */

// Allow telemetry probes which may otherwise be disabled for some
// applications (e.g. Thunderbird).
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

let trrServer;
add_setup(async function setup() {
  trr_test_setup();

  registerCleanupFunction(async () => {
    if (trrServer) {
      await trrServer.stop();
    }
    trr_clear_prefs();
  });
});

async function createAndStartServer() {
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
}

add_task(async function test_idle_telemetry() {
  await createAndStartServer();
  Services.dns.clearCache(true);
  Services.fog.testResetFOG();
  await new TRRDNSListener("testdomain.com", { expectedAnswer: "5.5.5.5" });

  let timeout = 2; // 2 seconds
  await new Promise(resolve => do_timeout(timeout * 1000, resolve));

  // gracefully shut down the server
  await trrServer.execute(`
      global.sessions.forEach(session => {
        session.close();
      });
      global.server.close()
    `);

  // Small timeout to make sure telemetry gets recorded
  await new Promise(resolve => do_timeout(1000, resolve));
  // Kill the server.
  await trrServer.stop();

  let distr = await Glean.network.trrIdleCloseTimeH2.other.testGetValue();
  Assert.equal(distr.count, 1, "just one connection being killed");
  Assert.greaterOrEqual(
    distr.sum,
    timeout * 1000000000,
    "should be slightly longer than the timeout. Note timeout is in microseconds"
  );
  Assert.less(
    distr.sum,
    timeout * 1000000000 * 1.2,
    "Shouldn't be much longer than the timeout."
  );

  // Test again, but this time kill connections. No idle connection telemetry should be recorded.
  await createAndStartServer();
  Services.dns.clearCache(true);
  Services.fog.testResetFOG();
  await new TRRDNSListener("testdomain.com", { expectedAnswer: "5.5.5.5" });

  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  // Small timeout to make sure telemetry gets recorded
  await new Promise(resolve => do_timeout(1000, resolve));
  // No telemetry should be recorded, since this is not a clean shutdown.
  Assert.equal(
    await Glean.network.trrIdleCloseTimeH2.other.testGetValue(),
    null
  );
});

// No http3 server on Android 🙁
add_task(
  { skip_if: () => AppConstants.platform == "android" },
  async function test_idle_telemetry_http3() {
    let h3port = await create_h3_server();
    Assert.ok(Number.isInteger(h3port));

    // Disable the second DoH request for looking up AAAA record.
    Services.prefs.setBoolPref("network.dns.disableIPv6", true);
    Services.prefs.setCharPref(
      "network.trr.uri",
      `https://foo.example.com/closeafter1000ms`
    );
    Services.prefs.setCharPref(
      "network.http.http3.alt-svc-mapping-for-testing",
      `foo.example.com;h3=:${h3port}`
    );
    Services.prefs.setIntPref(
      "network.trr.mode",
      Ci.nsIDNSService.MODE_TRRONLY
    );

    Services.dns.clearCache(true);
    Services.fog.testResetFOG();
    await new TRRDNSListener("testdomain2.com", { expectedSuccess: false });

    let timeout = 1;
    // Gets closed after 1 second, but wait one more.
    await new Promise(resolve => do_timeout((timeout + 1) * 1000, resolve));
    let distr = await Glean.network.trrIdleCloseTimeH3.other.testGetValue();
    Assert.equal(distr.count, 1, "just one connection being killed");
    Assert.greaterOrEqual(
      distr.sum,
      timeout * 1000000000,
      "should be slightly longer than the timeout. Note timeout is in microseconds"
    );
    Assert.less(
      distr.sum,
      timeout * 1000000000 * 1.2,
      "Shouldn't be much longer than the timeout."
    );
  }
);
