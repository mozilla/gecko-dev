"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column-in-gcd-script.js");

function run_test() {
  return (async function() {
    do_test_pending();

    const global = testGlobal("test");
    loadSubScriptWithOptions(SOURCE_URL, {target: global, ignoreCache: true});
    Cu.forceGC(); Cu.forceGC(); Cu.forceGC();

    const { createRootActor } = require("xpcshell-test/testactors");
    DebuggerServer.setRootActor(createRootActor);
    DebuggerServer.init(() => true);
    DebuggerServer.addTestGlobal(global);
    const client = new DebuggerClient(DebuggerServer.connectPipe());
    await connect(client);

    const { tabs } = await listTabs(client);
    const tab = findTab(tabs, "test");
    const [, targetFront] = await attachTarget(client, tab);
    const [, threadClient] = await attachThread(targetFront);
    await resume(threadClient);

    const { sources } = await getSources(threadClient);
    const source = findSource(sources, SOURCE_URL);
    const sourceClient = threadClient.source(source);

    const location = { line: 6, column: 17 };
    let [packet, breakpointClient] = await setBreakpoint(sourceClient, location);
    Assert.ok(packet.isPending);
    Assert.equal(false, "actualLocation" in packet);

    packet = await executeOnNextTickAndWaitForPause(function() {
      reload(targetFront).then(function() {
        loadSubScriptWithOptions(SOURCE_URL, {target: global, ignoreCache: true});
      });
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
    Assert.equal(where.column, location.column);
    const variables = frame.environment.bindings.variables;
    Assert.equal(variables.a.value, 1);
    Assert.equal(variables.b.value.type, "undefined");
    Assert.equal(variables.c.value.type, "undefined");
    await resume(threadClient);

    await close(client);
    do_test_finished();
  })();
}
