/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test checks that objects which are not extensible report themselves as
 * such.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test()
{
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
};

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-grips", aServer);
  gDebuggee.eval(function stopMe(arg1, arg2, arg3, arg4) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-grips", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let [f, s, ne, e] = aPacket.frame.arguments;
    let [fClient, sClient, neClient, eClient] = aPacket.frame.arguments.map(
      a => gThreadClient.pauseGrip(a));

    do_check_false(f.extensible);
    do_check_false(fClient.isExtensible);

    do_check_false(s.extensible);
    do_check_false(sClient.isExtensible);

    do_check_false(ne.extensible);
    do_check_false(neClient.isExtensible);

    do_check_true(e.extensible);
    do_check_true(eClient.isExtensible);

    gThreadClient.resume(_ => {
      gClient.close(gCallback);
    });
  });

  gDebuggee.eval("(" + function () {
    let f = {};
    Object.freeze(f);
    let s = {};
    Object.seal(s);
    let ne = {};
    Object.preventExtensions(ne);
    stopMe(f, s, ne, {});
  } + "())");
}

