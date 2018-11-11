/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// A console listener so we can listen for a log message from nsSearchService.
function promiseTimezoneMessage() {
  return new Promise(resolve => {
    let listener = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIConsoleListener]),
      observe(msg) {
        if (msg.message.startsWith("getIsUS() fell back to a timezone check with the result=")) {
          Services.console.unregisterListener(listener);
          resolve(msg);
        }
      },
    };
    Services.console.registerListener(listener);
  });
}

function run_test() {
  // Here we have malformed JSON
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code"');
  Services.search.init(() => {
    ok(!Services.prefs.prefHasUserValue("browser.search.region"), "should be no region pref");
    // fetch the engines - this should not persist any prefs.
    Services.search.getEngines();
    ok(!Services.prefs.prefHasUserValue("browser.search.region"), "should be no region pref");
    // should have recorded SUCCESS_WITHOUT_DATA
    checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.SUCCESS_WITHOUT_DATA);
    // and false values for timeout and forced-sync-init.
    for (let hid of ["SEARCH_SERVICE_COUNTRY_TIMEOUT",
                     "SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT"]) {
      let histogram = Services.telemetry.getHistogramById(hid);
      let snapshot = histogram.snapshot();
      deepEqual(snapshot.values, {0: 1, 1: 0}); // boolean probe so 3 buckets, expect 1 result for |0|.
    }
    do_test_finished();
    run_next_test();
  });
  do_test_pending();
}
