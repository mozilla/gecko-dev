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

function checkOrWaitForDataStateChanged(aDataCall, aExpectedState) {
  if (aDataCall.state == aExpectedState) {
    log("Data call state is already " + aDataCall.state);
    return;
  }

  return waitForTargetEvent(aDataCall, "statechange", function match() {
    log("Data call state is now " + aDataCall.state);
    return aDataCall.state === aExpectedState;
  });
}

function testDataStateDisconnected(aDataCall) {
  log('= testDataStateDisconnected =');

  let promises = [];
  promises.push(checkOrWaitForDataStateChanged(aDataCall, "disconnected"));
  promises.push(setEmulatorVoiceDataStateAndWait("data", "unregistered"));

  return Promise.all(promises)
    .then(() => verifyDataCallAttributes(aDataCall, false))
    .then(() => aDataCall.addHostRoute(TEST_HOST_ROUTE))
    .then(() => {
      ok(false, "Should not success on state disconnected.");
    }, aReason => {
      ok(true, "Expected error on state disconnected.");
    })
    .then(() => aDataCall.removeHostRoute(TEST_HOST_ROUTE))
    .then(() => {
      ok(false, "Should not success on state disconnected.");
    }, aReason => {
      ok(true, "Expected error on state disconnected.");
    });
}

function testDataStateConnected(aDataCall) {
  log('= testDataStateConnected =');

  let promises = [];
  promises.push(checkOrWaitForDataStateChanged(aDataCall, "connected"));
  promises.push(setEmulatorVoiceDataStateAndWait("data", "home"));

  return Promise.all(promises)
    .then(() => verifyDataCallAttributes(aDataCall, true));
}

function testDataStateUnavailable(aDataCall) {
  log('= testDataStateUnavailable =');

  let promises = [];
  promises.push(checkOrWaitForDataStateChanged(aDataCall, "unavailable"));
  promises.push(setRadioEnabledAndWait(false));

  return Promise.all(promises)
    .then(() => verifyDataCallAttributes(aDataCall, false))
    .then(() => aDataCall.addHostRoute(TEST_HOST_ROUTE))
    .then(() => {
      ok(false, "Should not success on state unavailable.");
    }, aReason => {
      ok(true, "Expected error on state unavailable.");
    })
    .then(() => aDataCall.removeHostRoute(TEST_HOST_ROUTE))
    .then(() => {
      ok(false, "Should not success on state unavailable.");
    }, aReason => {
      ok(true, "Expected error on state unavailable.");
    })
    // Restore radio state.
    .then(() => setRadioEnabledAndWait(true));
}

startTestCommon(function() {
  let origApnSettings, dataCall;

  return verifyInitialState()
  .then(() => getDataApnSettings())
  .then(value => {
    origApnSettings = value;
  })
  .then(() => setDataApnSettings(TEST_APN_SETTINGS))
  .then(() => requestDataCall("supl"))
  .then(aDataCall => {
    dataCall = aDataCall;
  })
  // Change data registration and wait for data state events.
  .then(() => testDataStateDisconnected(dataCall))
  .then(() => testDataStateConnected(dataCall))
  // Set radio state off and wait for 'unavailable' state event.
  .then(() => testDataStateUnavailable(dataCall))
  .then(() => releaseDataCall(dataCall))
  .then(() => {
    if (origApnSettings) {
      return setDataApnSettings(origApnSettings);
    }
  });
}, ["settings-read", "settings-write", "settings-api-read", "settings-api-write"]);
