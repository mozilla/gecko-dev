/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that thread-lifetime grips last past a resume.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_thread_lifetime();
                           });
  });
  do_test_pending();
}

function test_thread_lifetime() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const pauseGrip = packet.frame.arguments[0];

    // Create a thread-lifetime actor for this object.
    gClient.request({ to: pauseGrip.actor, type: "threadGrip" }, function(response) {
      // Successful promotion won't return an error.
      Assert.equal(response.error, undefined);
      gThreadClient.addOneTimeListener("paused", function(event, packet) {
        // Verify that the promoted actor is returned again.
        Assert.equal(pauseGrip.actor, packet.frame.arguments[0].actor);
        // Now that we've resumed, release the thread-lifetime grip.
        gClient.release(pauseGrip.actor, function(response) {
          gClient.request(
            {to: pauseGrip.actor, type: "bogusRequest"}, function(response) {
              Assert.equal(response.error, "noSuchActor");
              gThreadClient.resume(function(response) {
                finishClient(gClient);
              });
            });
        });
      });
      gThreadClient.resume();
    });
  });

  gDebuggee.eval("(" + function() {
    function stopMe(arg1) {
      debugger;
      debugger;
    }
    stopMe({obj: true});
  } + ")()");
}
