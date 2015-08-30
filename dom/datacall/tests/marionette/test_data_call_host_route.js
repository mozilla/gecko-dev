/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function verifyInitialState() {
  log("Verifying initial state.");

  // Data should be off and registration home before starting any test.
  return Promise.resolve()
    .then(getDataEnabled)
    .then(function(aResult) {
      is(aResult, false, "Data must be off by default.")
    });
}

function testAddHostRoute(aDataCall, aHost) {
  log('= testAddHostRoute =');

  if (!aDataCall) {
    return Promise.reject("aDataCall not avaiable.")
  }

  return aDataCall.addHostRoute(aHost)
    .then(() => verifyHostRoute(aHost, aDataCall.name, true));
}

function testRemoveHostRoute(aDataCall, aHost) {
  log('= testRemoveHostRoute =');

  if (!aDataCall) {
    return Promise.reject("aDataCall not avaiable.")
  }

  return aDataCall.removeHostRoute(aHost)
    .then(() => verifyHostRoute(aHost, aDataCall.name, false));
}

startTestCommon(function() {
  let origApnSettings;
  let dataCall;

  return verifyInitialState()
    .then(() => getDataApnSettings())
    .then(value => {
      origApnSettings = value;
    })
    .then(() => setDataApnSettings(TEST_APN_SETTINGS))
    .then(() => requestDataCall("mms"))
    .then(aDataCall => {
      ok(aDataCall, "mms data call.");
      dataCall = aDataCall;
    })
    .then(() => testAddHostRoute(dataCall, TEST_HOST_ROUTE))
    .then(() => testRemoveHostRoute(dataCall, TEST_HOST_ROUTE))
    .then(() => releaseDataCall(dataCall))
    // Restore apn settings.
    .then(() => {
      if (origApnSettings) {
        return setDataApnSettings(origApnSettings);
      }
    });
}, ["settings-read", "settings-write", "settings-api-read", "settings-api-write"]);
