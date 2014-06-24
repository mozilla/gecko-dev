/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const DEFAULT_ECC_LIST = "112,911";

function setEccListProperty(list) {
  log("Set property ril.ecclist: " + list);

  let deferred = Promise.defer();
  try {
    emulator.runShellCmd(["setprop","ril.ecclist", list]).then(function() {
      deferred.resolve(list);
    });
  } catch (e) {
    deferred.reject(e);
  }
  return deferred.promise;
}

function getEccListProperty() {
  log("Get property ril.ecclist.");

  let deferred = Promise.defer();
  try {
    emulator.runShellCmd(["getprop","ril.ecclist"]).then(function(aResult) {
      let list = !aResult.length ? "" : aResult[0];
      deferred.resolve(list);
    });
  } catch (e) {
    deferred.reject(e);
  }
  return deferred.promise;
}

/**
 * Convenient helper to compare a TelephonyCall and a received call event.
 */
function checkEventCallState(event, call, state) {
  is(call, event.call, "event.call");
  is(call.state, state, "call state");
}

/**
 * Make an outgoing call.
 *
 * @param number
 *        A string.
 * @param serviceId [optional]
 *        Identification of a service. 0 is set as default.
 * @return A deferred promise.
 */
function dial(number, serviceId) {
  serviceId = typeof serviceId !== "undefined" ? serviceId : 0;
  log("Make an outgoing call: " + number + ", serviceId: " + serviceId);

  let deferred = Promise.defer();

  telephony.dial(number, serviceId).then(call => {
    ok(call);
    is(call.number, number);
    is(call.state, "dialing");
    is(call.serviceId, serviceId);

    call.onalerting = function onalerting(event) {
      call.onalerting = null;
      log("Received 'onalerting' call event.");
      checkEventCallState(event, call, "alerting");
      deferred.resolve(call);
    };
  }, cause => {
    deferred.reject(cause);
  });

  return deferred.promise;
}

/**
 * Remote party answers the call.
 *
 * @param call
 *        A TelephonyCall object.
 * @return A deferred promise.
 */
function remoteAnswer(call) {
  log("Remote answering the call.");

  let deferred = Promise.defer();

  call.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    call.onconnected = null;
    checkEventCallState(event, call, "connected");
    deferred.resolve(call);
  };
  emulator.runCmd("gsm accept " + call.number);

  return deferred.promise;
}

/**
 * Remote party hangs up the call.
 *
 * @param call
 *        A TelephonyCall object.
 * @return A deferred promise.
 */
function remoteHangUp(call) {
  log("Remote hanging up the call.");

  let deferred = Promise.defer();

  call.ondisconnected = function ondisconnected(event) {
    log("Received 'disconnected' call event.");
    call.ondisconnected = null;
    checkEventCallState(event, call, "disconnected");
    deferred.resolve(call);
  };
  emulator.runCmd("gsm cancel " + call.number);

  return deferred.promise;
}

function testEmergencyLabel(number, list) {
  if (!list) {
    list = DEFAULT_ECC_LIST;
  }
  let index = list.split(",").indexOf(number);
  let emergency = index != -1;
  log("= testEmergencyLabel = " + number + " should be " +
      (emergency ? "emergency" : "normal") + " call");

  let outCall;

  return dial(number)
    .then(call => { outCall = call; })
    .then(() => {
      is(outCall.emergency, emergency, "emergency result should be correct");
    })
    .then(() => remoteAnswer(outCall))
    .then(() => {
      is(outCall.emergency, emergency, "emergency result should be correct");
    })
    .then(() => remoteHangUp(outCall));
}

startTest(function() {
  let origEccList;
  let eccList;

  getEccListProperty()
    .then(list => {
      origEccList = eccList = list;
    })
    .then(() => testEmergencyLabel("112", eccList))
    .then(() => testEmergencyLabel("911", eccList))
    .then(() => testEmergencyLabel("0912345678", eccList))
    .then(() => testEmergencyLabel("777", eccList))
    .then(() => {
      eccList = "777,119";
      return setEccListProperty(eccList);
    })
    .then(() => testEmergencyLabel("777", eccList))
    .then(() => testEmergencyLabel("119", eccList))
    .then(() => testEmergencyLabel("112", eccList))
    .then(() => setEccListProperty(origEccList))
    .then(null, error => {
      ok(false, 'promise rejects during test: ' + error);
    })
    .then(finish);
});
