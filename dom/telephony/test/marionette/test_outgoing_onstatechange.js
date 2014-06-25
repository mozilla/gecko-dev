/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let outgoingCall;
let outNumber = "5555551111";

function dial() {
  log("Make an outgoing call.");

  telephony.dial(outNumber).then(call => {
    outgoingCall = call;
    ok(outgoingCall);
    is(outgoingCall.id.number, outNumber);
    is(outgoingCall.state, "dialing");

    is(outgoingCall, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoingCall);

    outgoingCall.onstatechange = function statechangering(event) {
      log("Received 'onstatechange' call event.");

      is(outgoingCall, event.call);
      let expectedStates = ["dialing", "alerting"];
      ok(expectedStates.indexOf(event.call.state) != -1);

      if (event.call.state == "alerting") {
        emulator.runWithCallback("gsm list", function(result) {
          log("Call list is now: " + result);
          is(result[0], "outbound to  " + outNumber + " : ringing");
          answer();
        });
      }
    };
  });
}

function answer() {
  log("Answering the outgoing call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoingCall.onstatechange = function onstatechangeanswer(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    is(outgoingCall.state, "connected");
    is(outgoingCall, telephony.active);

    emulator.runWithCallback("gsm list", function(result) {
      log("Call list is now: " + result);
      is(result[0], "outbound to  " + outNumber + " : active");
      hold();
    });
  };
  emulator.runWithCallback("gsm accept " + outNumber);
}

function hold() {
  log("Putting the call on hold.");

  let gotHolding = false;
  outgoingCall.onstatechange = function onstatechangehold(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    if(!gotHolding){
      is(outgoingCall.state, "holding");
      gotHolding = true;
    } else {
      is(outgoingCall.state, "held");
      is(telephony.active, null);
      is(telephony.calls.length, 1);
      is(telephony.calls[0], outgoingCall);

      emulator.runWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + outNumber + " : held");
        resume();
      });
    }
  };
  outgoingCall.hold();
}

function resume() {
  log("Resuming the held call.");

  let gotResuming = false;
  outgoingCall.onstatechange = function onstatechangeresume(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    if(!gotResuming){
      is(outgoingCall.state, "resuming");
      gotResuming = true;
    } else {
      is(outgoingCall.state, "connected");
      is(telephony.active, outgoingCall);
      is(telephony.calls.length, 1);
      is(telephony.calls[0], outgoingCall);

      emulator.runWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        is(result[0], "outbound to  " + outNumber + " : active");
        hangUp();
      });
    }
  };
  outgoingCall.resume();
}

function hangUp() {
  log("Hanging up the outgoing call (local hang-up).");

  let gotDisconnecting = false;
  outgoingCall.onstatechange = function onstatechangedisconnect(event) {
    log("Received 'onstatechange' call event.");
    is(outgoingCall, event.call);
    if(!gotDisconnecting){
      is(outgoingCall.state, "disconnecting");
      gotDisconnecting = true;
    } else {
      is(outgoingCall.state, "disconnected");
      is(telephony.active, null);
      is(telephony.calls.length, 0);

      emulator.runWithCallback("gsm list", function(result) {
        log("Call list is now: " + result);
        cleanUp();
      });
    }
  };
  outgoingCall.hangUp();
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
