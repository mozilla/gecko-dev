"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column-minified.js");

async function run_test() {
  do_test_pending();
  const { createRootActor } = require("xpcshell-test/testactors");
  DebuggerServer.setRootActor(createRootActor);
  DebuggerServer.init(() => true);
  const global = createTestGlobal("test");
  DebuggerServer.addTestGlobal(global);

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await connect(client);

  const { tabs } = await listTabs(client);
  const tab = findTab(tabs, "test");
  const [, targetFront] = await attachTarget(client, tab);
  const [, threadClient] = await attachThread(targetFront);
  await resume(threadClient);

  const promise = waitForNewSource(threadClient, SOURCE_URL);
  loadSubScript(SOURCE_URL, global);
  const { source } = await promise;
  const sourceClient = threadClient.source(source);

  // Pause inside of the nested function so we can make sure that we don't
  // add any other breakpoints at other places on this line.
  const location = { line: 3, column: 81 };
  let [packet, breakpointClient] = await setBreakpoint(
    sourceClient,
    location
  );

  Assert.ok(!packet.isPending);
  Assert.equal(false, "actualLocation" in packet);

  packet = await executeOnNextTickAndWaitForPause(function() {
    Cu.evalInSandbox("f()", global);
  }, client);

  Assert.equal(packet.type, "paused");
  const why = packet.why;
  Assert.equal(why.type, "breakpoint");
  Assert.equal(why.actors.length, 1);
  Assert.equal(why.actors[0], breakpointClient.actor);

  const frame = packet.frame;
  const where = frame.where;
  Assert.equal(where.source.actor, source.actor);
  Assert.equal(where.line, location.line);
  Assert.equal(where.column, 81);

  const variables = frame.environment.bindings.variables;
  Assert.equal(variables.a.value, 1);
  Assert.equal(variables.b.value, 2);
  Assert.equal(variables.c.value, 3);

  await resume(threadClient);
  await close(client);
  do_test_finished();
}
