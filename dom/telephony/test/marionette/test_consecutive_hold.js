/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

const inNumber = "5555552222";
const inInfo = gInCallStrPool(inNumber);
let inCall;

startTest(function() {
  Promise.resolve()

    // Incoming
    .then(() => gRemoteDial(inNumber))
    .then(call => inCall = call)
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.incoming]))

    // Answer
    .then(() => gAnswer(inCall))
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))

    // Disable the Hold function of the emulator, then hold the active call,
    // where the hold request will fail and the call should stay active.
    .then(() => emulator.runCmd("gsm disable hold"))
    .then(() => {
      let waitingPromise = gWaitForNamedStateEvent(inCall, "holding");
      let requestPromise = inCall.hold()
        .then(() => ok(false, "This hold request should be rejected."),
              () => log("This hold request is rejected as expected."));

      return Promise.all([waitingPromise, requestPromise]);
    })
    .then(() => gCheckAll(inCall, [inCall], "", [], [inInfo.active]))

    // Enable the Hold function of the emulator, then hold the active call,
    // where the hold request should succeed and the call should become held.
    .then(() => emulator.runCmd("gsm enable hold"))
    .then(() => {
      let waitingPromise = gWaitForNamedStateEvent(inCall, "held");
      let requestPromise = inCall.hold()
        .then(() => log("This hold request is resolved as expected."),
              () => ok(false, "This hold request should be resolved."));

      return Promise.all([waitingPromise, requestPromise]);
    })
    .then(() => gCheckAll(null, [inCall], "", [], [inInfo.held]))

    // Hang up the call
    .then(() => gHangUp(inCall))
    .then(() => gCheckAll(null, [], "", [], []))

    // Clean Up
    .catch(error => ok(false, "Promise reject: " + error))
    .then(() => emulator.runCmd("gsm enable hold"))
    .then(finish);
});
