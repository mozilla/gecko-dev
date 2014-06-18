/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

function setRadioEnabled(enabled) {
  log("Set radio enabled: " + enabled + ".");

  let desiredRadioState = enabled ? 'enabled' : 'disabled';
  let deferred = Promise.defer();
  let connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  connection.onradiostatechange = function() {
    let state = connection.radioState;
    log("Received 'radiostatechange' event, radioState: " + state);

    // We are waiting for 'desiredRadioState.' Ignore any transient state.
    if (state === desiredRadioState) {
      connection.onradiostatechange = null;
      deferred.resolve();
    }
  };
  connection.setRadioEnabled(enabled);

  return deferred.promise;
}

startTestWithPermissions(['mobileconnection'], function() {
  let outCall;
  setRadioEnabled(false)
    .then(() => gDial("112"))
    .then(call => { outCall = call; })
    .then(() => gRemoteAnswer(outCall))
    .then(() => gDelay(1000))  // See Bug 1018051 for the purpose of the delay.
    .then(() => gRemoteHangUp(outCall))
    .then(null, () => {
      ok(false, "promise rejects during test.");
    })
    .then(finish);
});
