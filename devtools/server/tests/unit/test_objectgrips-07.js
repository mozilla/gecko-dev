/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that objects which are not extensible report themselves as
 * such.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function run_test() {
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-grips", server);
  gDebuggee.eval(function stopMe(arg1, arg2, arg3, arg4) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_object_grip();
                           });
  });
}

function test_object_grip() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const [f, s, ne, e] = packet.frame.arguments;
    const [fClient, sClient, neClient, eClient] = packet.frame.arguments.map(
      a => gThreadClient.pauseGrip(a));

    Assert.ok(!f.extensible);
    Assert.ok(!fClient.isExtensible);

    Assert.ok(!s.extensible);
    Assert.ok(!sClient.isExtensible);

    Assert.ok(!ne.extensible);
    Assert.ok(!neClient.isExtensible);

    Assert.ok(e.extensible);
    Assert.ok(eClient.isExtensible);

    gThreadClient.resume(_ => {
      gClient.close().then(gCallback);
    });
  });

  /* eslint-disable no-undef */
  gDebuggee.eval("(" + function() {
    const f = {};
    Object.freeze(f);
    const s = {};
    Object.seal(s);
    const ne = {};
    Object.preventExtensions(ne);
    stopMe(f, s, ne, {});
  } + "())");
  /* eslint-enable no-undef */
}
