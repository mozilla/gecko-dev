/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1122064 - make sure that scripts introduced via onNewScripts
 * properly populate the `ScriptStore` with all there nested child
 * scripts, so you can set breakpoints on deeply nested scripts
 */

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  // Populate the `ScriptStore` so that we only test that the script
  // is added through `onNewScript`
  await getSources(threadClient);

  let packet = await executeOnNextTickAndWaitForPause(() => {
    evalCode(debuggee);
  }, client);
  const source = threadClient.source(packet.frame.where.source);
  const location = {
    line: debuggee.line0 + 8,
  };

  const [res, bpClient] = await setBreakpoint(source, location);
  ok(!res.error);

  await resume(threadClient);
  packet = await waitForPause(client);
  Assert.equal(packet.type, "paused");
  Assert.equal(packet.why.type, "breakpoint");
  Assert.equal(packet.why.actors[0], bpClient.actor);
  Assert.equal(packet.frame.where.source.actor, source.actor);
  Assert.equal(packet.frame.where.line, location.line);

  await resume(threadClient);
}));

/* eslint-disable */
function evalCode(debuggee) {
  // Start a new script
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n(" + function () {
      debugger;
      var a = (function () {
        return (function () {
          return (function () {
            return (function () {
              return (function () {
                var x = 10; // This line gets a breakpoint
                return 1;
              })();
            })();
          })();
        })();
      })();
    } + ")()",
    debuggee
  );
}
