/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Make sure that setting a breakpoint in a line with bytecodes in multiple
 * scripts, sets the breakpoint in all of them (bug 793214).
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", function(event, packet) {
      const source = threadClient.source(packet.frame.where.source);
      const location = { line: debuggee.line0 + 2 };

      source.setBreakpoint(location).then(function([response, bpClient]) {
        // actualLocation is not returned when breakpoints don't skip forward.
        Assert.equal(response.actualLocation, undefined);

        threadClient.addOneTimeListener("paused", function(event, packet) {
          // Check the return value.
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.why.type, "breakpoint");
          Assert.equal(packet.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          Assert.equal(debuggee.a, undefined);

          threadClient.addOneTimeListener("paused", function(event, packet) {
            // Check the return value.
            Assert.equal(packet.type, "paused");
            Assert.equal(packet.why.type, "breakpoint");
            Assert.equal(packet.why.actors[0], bpClient.actor);
            // Check that the breakpoint worked.
            Assert.equal(debuggee.a.b, 1);
            Assert.equal(debuggee.res, undefined);

            // Remove the breakpoint.
            bpClient.remove(function(response) {
              threadClient.resume(resolve);
            });
          });

          // Continue until the breakpoint is hit again.
          threadClient.resume();
        });
        // Continue until the breakpoint is hit.
        threadClient.resume();
      });
    });

    /* eslint-disable */
    Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                     "debugger;\n" +                      // line0 + 1
                     "var a = { b: 1, f: function() { return 2; } };\n" + // line0+2
                     "var res = a.f();\n",               // line0 + 3
                     debuggee);
    /* eslint-enable */
  });
}));
