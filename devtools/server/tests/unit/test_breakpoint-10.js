/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that setting a breakpoint in a line with multiple entry points
 * triggers no matter which entry point we reach.
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
}

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-stack", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_child_breakpoint();
    });
  });
}

function test_child_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    let location = { line: gDebuggee.line0 + 3 };

    source.setBreakpoint(location, function (aResponse, bpClient) {
      // actualLocation is not returned when breakpoints don't skip forward.
      do_check_eq(aResponse.actualLocation, undefined);

      gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
        // Check the return value.
        do_check_eq(aPacket.type, "paused");
        do_check_eq(aPacket.why.type, "breakpoint");
        do_check_eq(aPacket.why.actors[0], bpClient.actor);
        // Check that the breakpoint worked.
        do_check_eq(gDebuggee.i, 0);

        gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
          // Check the return value.
          do_check_eq(aPacket.type, "paused");
          do_check_eq(aPacket.why.type, "breakpoint");
          do_check_eq(aPacket.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          do_check_eq(gDebuggee.i, 1);

          // Remove the breakpoint.
          bpClient.remove(function (aResponse) {
            gThreadClient.resume(function () {
              gClient.close().then(gCallback);
            });
          });
        });

        // Continue until the breakpoint is hit again.
        gThreadClient.resume();

      });
      // Continue until the breakpoint is hit.
      gThreadClient.resume();

    });

  });


  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "debugger;\n" +                      // line0 + 1
                   "var a, i = 0;\n" +                  // line0 + 2
                   "for (i = 1; i <= 2; i++) {\n" +     // line0 + 3
                   "  a = i;\n" +                       // line0 + 4
                   "}\n",                               // line0 + 5
                   gDebuggee);
}
