/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "5555552368";
let outgoing;

// Second Telephony instance created for monitoring events.
let listener;

function dial() {
  log("Make an outgoing call.");

    telephony.dial(number);

    // Create the second Telephony instance.
    let ifr = document.createElement("iframe");
    ifr.onload = function() {
      listener = ifr.contentWindow.navigator.mozTelephony;
      ok(listener, "A new telephony instance on iframe");

      listener.oncallschanged = function oncallschanged(event) {
        log("Received 'callschanged' call event.");

        if (!listener.calls.length) {
          // We might receive more than one callschanged event. That's because
          // Telephony API guarantees at least one callschanged event is fired
          // to notify the calls array is loaded. In this test case, we are
          // waiting for calls.length becoming 1.
          return;
        }

        listener.oncallschanged = null;

        is(listener.calls.length, 1);
        outgoing = listener.calls[0];
        ok(outgoing);
        is(outgoing.number, number);
        is(outgoing, listener.active);
        answer();
      };
    };
    document.body.appendChild(ifr);
}

function answer() {
  log("Answering the outgoing call.");

  // We get no "connecting" event when the remote party answers the call.

  outgoing.onconnected = function onconnected(event) {
    log("Received 'connected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "connected");

    is(outgoing, listener.active);

    emulator.run("gsm list", function(result) {
      log("Call list (after 'connected' event) is now: " + result);
      is(result[0], "outbound to  " + number + " : active");
      is(result[1], "OK");
      hangUp();
    });
  };
  emulator.run("gsm accept " + number);
}

function hangUp() {
  log("Hanging up the outgoing call.");

  outgoing.ondisconnected = function(event) {
    log("Received 'disconnected' call event.");
    is(outgoing, event.call);
    is(outgoing.state, "disconnected");
    ok(!listener.active);

    emulator.run("gsm list", function(result) {
      log("Call list (after 'connected' event) is now: " + result);
      is(result[0], "OK");
      cleanUp();
    });
  };
  emulator.run("gsm cancel " + number);
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
