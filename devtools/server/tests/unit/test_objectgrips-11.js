/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we get the magic properties on Error objects.

var gDebuggee;
var gClient;
var gThreadClient;

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_object_grip();
                           });
  });
  do_test_pending();
}

function test_object_grip() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const args = packet.frame.arguments;

    const objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getOwnPropertyNames(function(response) {
      const opn = response.ownPropertyNames;
      Assert.equal(opn.length, 4);
      opn.sort();
      Assert.equal(opn[0], "columnNumber");
      Assert.equal(opn[1], "fileName");
      Assert.equal(opn[2], "lineNumber");
      Assert.equal(opn[3], "message");

      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });

  gDebuggee.eval("stopMe(new TypeError('error message text'))");
}

