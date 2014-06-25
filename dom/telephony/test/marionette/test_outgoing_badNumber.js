/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

let number = "****5555552368****";
let outgoing;


function dial() {
  log("Make an outgoing call to an invalid number.");

  // Note: The number is valid from the view of phone and the call could be
  // dialed out successfully. However, it will later receive the BadNumberError
  // from network side.
  telephony.dial(number).then(call => {
    outgoing = call;
    ok(outgoing);
    is(outgoing.id.number, number);
    is(outgoing.state, "dialing");

    is(outgoing, telephony.active);
    is(telephony.calls.length, 1);
    is(telephony.calls[0], outgoing);

    outgoing.onerror = function onerror(event) {
      log("Received 'error' event.");
      is(event.call, outgoing);
      ok(event.call.error);
      is(event.call.error.name, "BadNumberError");

      emulator.runWithCallback("gsm list", function(result) {
        log("Initial call list: " + result);
        cleanUp();
      });
    };
  });
}

function cleanUp() {
  finish();
}

startTest(function() {
  dial();
});
