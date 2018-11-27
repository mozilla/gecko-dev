/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that a breakpoint or a debugger statement cause execution to pause even
 * in a stepped-over function.
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", function(event, packet) {
      const source = threadClient.source(packet.frame.where.source);
      const location = { line: debuggee.line0 + 2 };

      source.setBreakpoint(location).then(async function([response, bpClient]) {
        const testCallbacks = [
          function(packet) {
            // Check that the stepping worked.
            Assert.equal(packet.frame.where.line, debuggee.line0 + 5);
            Assert.equal(packet.why.type, "resumeLimit");
          },
          function(packet) {
            // Reached the breakpoint.
            Assert.equal(packet.frame.where.line, location.line);
            Assert.equal(packet.why.type, "breakpoint");
            Assert.notEqual(packet.why.type, "resumeLimit");
          },
          function(packet) {
            // Stepped to the closing brace of the function.
            Assert.equal(packet.frame.where.line, debuggee.line0 + 3);
            Assert.equal(packet.why.type, "resumeLimit");
          },
          function(packet) {
            // The frame is about to be popped while stepping.
            Assert.equal(packet.frame.where.line, debuggee.line0 + 3);
            Assert.notEqual(packet.why.type, "breakpoint");
            Assert.equal(packet.why.type, "resumeLimit");
            Assert.equal(packet.why.frameFinished.return.type, "undefined");
          },
          function(packet) {
            // Check that the debugger statement wasn't the reason for this pause.
            Assert.equal(debuggee.a, 1);
            Assert.equal(debuggee.b, undefined);
            Assert.equal(packet.frame.where.line, debuggee.line0 + 6);
            Assert.notEqual(packet.why.type, "debuggerStatement");
            Assert.equal(packet.why.type, "resumeLimit");
            Assert.equal(packet.poppedFrames.length, 1);
          },
          function(packet) {
            // Check that the debugger statement wasn't the reason for this pause.
            Assert.equal(packet.frame.where.line, debuggee.line0 + 7);
            Assert.notEqual(packet.why.type, "debuggerStatement");
            Assert.equal(packet.why.type, "resumeLimit");
          },
        ];

        for (const callback of testCallbacks) {
          const waiter = waitForPause(threadClient);
          threadClient.stepOver();
          const packet = await waiter;
          callback(packet);
        }

        // Remove the breakpoint and finish.
        const waiter = waitForPause(threadClient);
        threadClient.stepOver();
        await waiter;
        bpClient.remove(() => threadClient.resume(resolve));
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
                     debuggee);
    /* eslint-enable */
  });
}));
