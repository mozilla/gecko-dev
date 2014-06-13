/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("mobileconnection", true, document);

let Promise = SpecialPowers.Cu.import("resource://gre/modules/Promise.jsm").Promise;

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let connection;
let ifr = document.createElement("iframe");
ifr.onload = function() {
  connection = ifr.contentWindow.navigator.mozMobileConnections[0];

  ok(connection instanceof ifr.contentWindow.MozMobileConnection,
     "connection is instanceof " + connection.constructor);

  startTest();
};
document.body.appendChild(ifr);

function receivedPending(received, pending, nextAction) {
  let index = pending.indexOf(received);
  if (index != -1) {
    pending.splice(index, 1);
  }
  if (pending.length === 0) {
    nextAction();
  }
}

function setRadioEnabled(enabled, transientState, finalState) {
  log("setRadioEnabled to " + enabled);

  let deferred = Promise.defer();
  let done = function() {
    deferred.resolve();
  };

  let pending = ["onradiostatechange", "onsuccess"];

  let receivedTransient = false;
  connection.onradiostatechange = function() {
    let state = connection.radioState;
    log("Received 'radiostatechange' event, radioState: " + state);

    if (state == transientState) {
      receivedTransient = true;
    } else if (state == finalState) {
      ok(receivedTransient);
      receivedPending("onradiostatechange", pending, done);
    }
  };

  let req = connection.setRadioEnabled(enabled);

  req.onsuccess = function() {
    log("setRadioEnabled success");
    receivedPending("onsuccess", pending, done);
  };

  req.onerror = function() {
    ok(false, "setRadioEnabled should not fail");
    deferred.reject();
  };

  return deferred.promise;
}

function setClir(aMode) {
  let deferred = Promise.defer();

  ok(true, "setClir(" + aMode + ")");
  let req = connection.setCallingLineIdRestriction(aMode);

  req.onsuccess = function(aEvent) {
    deferred.resolve(aEvent);
  };

  req.onerror = function(aEvent) {
    deferred.reject(aEvent);
  };

  return deferred.promise;
}

function getClir() {
  let deferred = Promise.defer();

  ok(true, "getClir");
  let req = connection.getCallingLineIdRestriction();

  req.onsuccess = function(aEvent) {
    deferred.resolve(aEvent);
  };

  req.onerror = function(aEvent) {
    deferred.reject(aEvent);
  };

  return deferred.promise;
}

function testSetClirOnRadioOff(aMode) {
  log("testSetClirOnRadioOff (set to mode: " + aMode + ")");
  return Promise.resolve()
    .then(() => setClir(aMode))
    .then(() => {
      ok(false, "shouldn't resolve");
    }, (evt) => {
      is(evt.target.error.name, "RadioNotAvailable");
    });
}

function testGetClirOnRadioOff() {
  log("testGetClirOnRadioOff");
  return Promise.resolve()
    .then(() => getClir())
    .then(() => {
      ok(false, "shouldn't resolve");
    }, (evt) => {
      is(evt.target.error.name, "RadioNotAvailable");
    });
}

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}

// Test case.
function startTest() {
  setRadioEnabled(false, "disabling", "disabled")
    .then(() => testSetClirOnRadioOff(0))
    .then(() => testGetClirOnRadioOff())
    // Restore radio state.
    .then(() => setRadioEnabled(true, "enabling", "enabled"),
          () => setRadioEnabled(true, "enabling", "enabled"))
    .then(cleanUp);
}
