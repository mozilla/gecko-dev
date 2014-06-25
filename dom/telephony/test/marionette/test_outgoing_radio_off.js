/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let connection;

function setRadioEnabled(enabled, callback) {
  let request  = connection.setRadioEnabled(enabled);
  let desiredRadioState = enabled ? 'enabled' : 'disabled';

  let pending = ['onradiostatechange', 'onsuccess'];
  let done = callback;

  connection.onradiostatechange = function() {
    let state = connection.radioState;
    log("Received 'radiostatechange' event, radioState: " + state);

    if (state == desiredRadioState) {
      gReceivedPending('onradiostatechange', pending, done);
    }
  };

  request.onsuccess = function onsuccess() {
    gReceivedPending('onsuccess', pending, done);
  };

  request.onerror = function onerror() {
    ok(false, "setRadioEnabled should be ok");
  };
}

function dial(number) {
  // Verify initial state before dial.
  ok(telephony);
  is(telephony.active, null);
  ok(telephony.calls);
  is(telephony.calls.length, 0);

  log("Make an outgoing call.");

  telephony.dial(number).then(null, cause => {
    log("Received promise 'reject'");

    is(telephony.active, null);
    is(telephony.calls.length, 0);
    is(cause, "RadioNotAvailable");

    emulator.runWithCallback("gsm list", function(result) {
      log("Initial call list: " + result);

      setRadioEnabled(true, cleanUp);
    });
  });
}

function cleanUp() {
  finish();
}

startTestWithPermissions(['mobileconnection'], function() {
  connection = navigator.mozMobileConnections[0];
  ok(connection instanceof MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  setRadioEnabled(false, function() {
    dial("0912345678");
  });
});
