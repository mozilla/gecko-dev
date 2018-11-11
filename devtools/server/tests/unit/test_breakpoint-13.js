/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that execution doesn't pause twice while stepping, when encountering
 * either a breakpoint or a debugger statement.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-stack", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_breakpoint();
                           });
  });
}

function test_simple_breakpoint() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const source = gThreadClient.source(packet.frame.where.source);
    const location = { line: gDebuggee.line0 + 2 };

    source.setBreakpoint(location).then(async function([response, bpClient]) {
      const testCallbacks = [
        function(packet) {
          // Check that the stepping worked.
          Assert.equal(packet.frame.where.line, gDebuggee.line0 + 5);
          Assert.equal(packet.why.type, "resumeLimit");
        },
        function(packet) {
          // Entered the foo function call frame.
          Assert.equal(packet.frame.where.line, location.line);
          Assert.notEqual(packet.why.type, "breakpoint");
          Assert.equal(packet.why.type, "resumeLimit");
        },
        function(packet) {
          // At the end of the foo function call frame.
          Assert.equal(packet.frame.where.line, gDebuggee.line0 + 3);
          Assert.notEqual(packet.why.type, "breakpoint");
          Assert.equal(packet.why.type, "resumeLimit");
        },
        function(packet) {
          // Check that the breakpoint wasn't the reason for this pause, but
          // that the frame is about to be popped while stepping.
          Assert.equal(packet.frame.where.line, gDebuggee.line0 + 3);
          Assert.notEqual(packet.why.type, "breakpoint");
          Assert.equal(packet.why.type, "resumeLimit");
          Assert.equal(packet.why.frameFinished.return.type, "undefined");
        },
        function(packet) {
          // Check that the debugger statement wasn't the reason for this pause.
          Assert.equal(gDebuggee.a, 1);
          Assert.equal(gDebuggee.b, undefined);
          Assert.equal(packet.frame.where.line, gDebuggee.line0 + 6);
          Assert.notEqual(packet.why.type, "debuggerStatement");
          Assert.equal(packet.why.type, "resumeLimit");
          Assert.equal(packet.poppedFrames.length, 1);
        },
        function(packet) {
          // Check that the debugger statement wasn't the reason for this pause.
          Assert.equal(packet.frame.where.line, gDebuggee.line0 + 7);
          Assert.notEqual(packet.why.type, "debuggerStatement");
          Assert.equal(packet.why.type, "resumeLimit");
        },
      ];

      for (const callback of testCallbacks) {
        const waiter = waitForPause(gThreadClient);
        gThreadClient.stepIn();
        const packet = await waiter;
        callback(packet);
      }

      // Remove the breakpoint and finish.
      const waiter = waitForPause(gThreadClient);
      gThreadClient.stepIn();
      await waiter;
      bpClient.remove(() => gThreadClient.resume(() => gClient.close().then(gCallback)));
    });
  });

  /* eslint-disable */
  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "function foo() {\n" + // line0 + 1
                   "  this.a = 1;\n" +    // line0 + 2 <-- Breakpoint is set here.
                   "}\n" +                // line0 + 3
                   "debugger;\n" +        // line0 + 4
                   "foo();\n" +           // line0 + 5
                   "debugger;\n" +        // line0 + 6
                   "var b = 2;\n",        // line0 + 7
                   gDebuggee);
  /* eslint-enable */
}
