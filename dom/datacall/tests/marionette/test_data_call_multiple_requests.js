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

startTestCommon(function() {
  let origApnSettings, dataCall1, dataCall2;

  return verifyInitialState()
  .then(() => getDataApnSettings())
  .then(value => {
    origApnSettings = value;
  })
  .then(() => setDataApnSettings(TEST_APN_SETTINGS))
  .then(() => requestDataCall("ims"))
  .then(aDataCall => {
    dataCall1 = aDataCall;
  })
  .then(() => dataCall1.addHostRoute(TEST_HOST_ROUTE))
  .then(() => verifyHostRoute(TEST_HOST_ROUTE, dataCall1.name, true))
  // Request a second data call on the same type.
  .then(() => requestDataCall("ims"))
  .then(aDataCall => {
    dataCall2 = aDataCall;
  })
  // Adding same host route on different data calls.
  .then(() => dataCall2.addHostRoute(TEST_HOST_ROUTE))
  .then(() => verifyHostRoute(TEST_HOST_ROUTE, dataCall2.name, true))
  // Removing one of the host routes, should not affect the other same host
  // route.
  .then(() => dataCall2.removeHostRoute(TEST_HOST_ROUTE))
  .then(() => verifyHostRoute(TEST_HOST_ROUTE, dataCall1.name, true))
  // All routes removed, should be cleared now.
  .then(() => dataCall1.removeHostRoute(TEST_HOST_ROUTE))
  .then(() => verifyHostRoute(TEST_HOST_ROUTE, dataCall1.name, false))
  // Releasing one of the data call, should not affect the other of the same
  // type.
  .then(() => releaseDataCall(dataCall2))
  .then(() => verifyDataCallAttributes(dataCall1, true))
  .then(() => releaseDataCall(dataCall1))
  .then(() => verifyDataCallAttributes(dataCall1, false))
  .then(() => {
    if (origApnSettings) {
      return setDataApnSettings(origApnSettings);
    }
  });
}, ["settings-read", "settings-write", "settings-api-read", "settings-api-write"]);
