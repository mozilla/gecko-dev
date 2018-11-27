/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that we can set breakpoints in columns, not just lines.
 */

add_task(threadClientTest(({ threadClient, debuggee, client }) => {
  return new Promise(resolve => {
    // Debugger statement
    client.addOneTimeListener("paused", function(event, packet) {
      const source = threadClient.source(packet.frame.where.source);
      const location = {
        line: debuggee.line0 + 1,
        column: 55,
      };
      let timesBreakpointHit = 0;

      source.setBreakpoint(location).then(function([response, bpClient]) {
        threadClient.addListener("paused", function onPaused(event, packet) {
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.why.type, "breakpoint");
          Assert.equal(packet.why.actors[0], bpClient.actor);
          Assert.equal(packet.frame.where.source.actor, source.actor);
          Assert.equal(packet.frame.where.line, location.line);
          Assert.equal(packet.frame.where.column, location.column);

          Assert.equal(debuggee.acc, timesBreakpointHit);
          Assert.equal(packet.frame.environment.bindings.variables.i.value,
                       timesBreakpointHit);

          if (++timesBreakpointHit === 3) {
            threadClient.removeListener("paused", onPaused);
            bpClient.remove(function(response) {
              threadClient.resume(resolve);
            });
          } else {
            threadClient.resume();
          }
        });

        // Continue until the breakpoint is hit.
        threadClient.resume();
      });
    });

    /* eslint-disable */
    Cu.evalInSandbox(
      "var line0 = Error().lineNumber;\n" +
      "(function () { debugger; this.acc = 0; for (var i = 0; i < 3; i++) this.acc++; }());",
      debuggee
    );
    /* eslint-enable */
  });
}));
