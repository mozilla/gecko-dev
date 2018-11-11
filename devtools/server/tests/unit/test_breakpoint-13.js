/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that execution doesn't pause twice while stepping, when encountering
 * either a breakpoint or a debugger statement.
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
      test_simple_breakpoint();
    });
  });
}

function test_simple_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    let location = { line: gDebuggee.line0 + 2 };

    source.setBreakpoint(location, Task.async(function* (aResponse, bpClient) {
      const testCallbacks = [
        function (aPacket) {
          // Check that the stepping worked.
          do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 5);
          do_check_eq(aPacket.why.type, "resumeLimit");
        },
        function (aPacket) {
          // Entered the foo function call frame.
          do_check_eq(aPacket.frame.where.line, location.line);
          do_check_neq(aPacket.why.type, "breakpoint");
          do_check_eq(aPacket.why.type, "resumeLimit");
        },
        function (aPacket) {
          // At the end of the foo function call frame.
          do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 3);
          do_check_neq(aPacket.why.type, "breakpoint");
          do_check_eq(aPacket.why.type, "resumeLimit");
        },
        function (aPacket) {
          // Check that the breakpoint wasn't the reason for this pause, but
          // that the frame is about to be popped while stepping.
          do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 3);
          do_check_neq(aPacket.why.type, "breakpoint");
          do_check_eq(aPacket.why.type, "resumeLimit");
          do_check_eq(aPacket.why.frameFinished.return.type, "undefined");
        },
        function (aPacket) {
          // The foo function call frame was just popped from the stack.
          do_check_eq(gDebuggee.a, 1);
          do_check_eq(gDebuggee.b, undefined);
          do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 5);
          do_check_eq(aPacket.why.type, "resumeLimit");
          do_check_eq(aPacket.poppedFrames.length, 1);
        },
        function (aPacket) {
          // Check that the debugger statement wasn't the reason for this pause.
          do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 6);
          do_check_neq(aPacket.why.type, "debuggerStatement");
          do_check_eq(aPacket.why.type, "resumeLimit");
        },
        function (aPacket) {
          // Check that the debugger statement wasn't the reason for this pause.
          do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 7);
          do_check_neq(aPacket.why.type, "debuggerStatement");
          do_check_eq(aPacket.why.type, "resumeLimit");
        },
      ];

      for (let callback of testCallbacks) {
        let waiter = waitForPause(gThreadClient);
        gThreadClient.stepIn();
        let packet = yield waiter;
        callback(packet);
      }

      // Remove the breakpoint and finish.
      let waiter = waitForPause(gThreadClient);
      gThreadClient.stepIn();
      yield waiter;
      bpClient.remove(() => gThreadClient.resume(() => gClient.close().then(gCallback)));
    }));
  });

  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "function foo() {\n" + // line0 + 1
                   "  this.a = 1;\n" +    // line0 + 2 <-- Breakpoint is set here.
                   "}\n" +                // line0 + 3
                   "debugger;\n" +        // line0 + 4
                   "foo();\n" +           // line0 + 5
                   "debugger;\n" +        // line0 + 6
                   "var b = 2;\n",        // line0 + 7
                   gDebuggee);
}
