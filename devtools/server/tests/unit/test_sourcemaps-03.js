/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check setting breakpoints in source mapped sources.
 */

var gDebuggee;
var gClient;
var gThreadClient;

const {SourceNode} = require("source-map");

function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-source-map");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-source-map",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_source_map();
                           });
  });
  do_test_pending();
}

function testBreakpointMapping(name, callback) {
  (async function() {
    let response = await waitForPause(gThreadClient);
    Assert.equal(response.why.type, "debuggerStatement");

    const source = await getSource(gThreadClient, "http://example.com/www/js/" + name + ".js");
    response = await setBreakpoint(source, {
      // Setting the breakpoint on an empty line so that it is pushed down one
      // line and we can check the source mapped actualLocation later.
      line: 3,
    });

    // Should not slide breakpoints for sourcemapped sources
    Assert.ok(!response.actualLocation);

    await setBreakpoint(source, { line: 4 });

    // The eval will cause us to resume, then we get an unsolicited pause
    // because of our breakpoint, we resume again to finish the eval, and
    // finally receive our last pause which has the result of the client
    // evaluation.
    response = await gThreadClient.eval(null, name + "()");
    Assert.equal(response.type, "resumed");

    response = await waitForPause(gThreadClient);
    Assert.equal(response.why.type, "breakpoint");
    // Assert that we paused because of the breakpoint at the correct
    // location in the code by testing that the value of `ret` is still
    // undefined.
    Assert.equal(response.frame.environment.bindings.variables.ret.value.type,
                 "undefined");

    response = await resume(gThreadClient);

    response = await waitForPause(gThreadClient);
    Assert.equal(response.why.type, "clientEvaluated");
    Assert.equal(response.why.frameFinished.return, name);

    response = await resume(gThreadClient);

    callback();
  })();

  gDebuggee.eval("(" + function() {
    debugger;
  } + "());");
}

function test_simple_source_map() {
  const expectedSources = new Set([
    "http://example.com/www/js/a.js",
    "http://example.com/www/js/b.js",
    "http://example.com/www/js/c.js",
  ]);

  gThreadClient.addListener("newSource", function _onNewSource(event, packet) {
    expectedSources.delete(packet.source.url);
    if (expectedSources.size > 0) {
      return;
    }
    gThreadClient.removeListener("newSource", _onNewSource);

    function finish() {
      finishClient(gClient);
    }

    testBreakpointMapping("a", function() {
      testBreakpointMapping("b", function() {
        testBreakpointMapping("c", finish);
      });
    });
  });

  const a = new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() {\n"),
    new SourceNode(2, 0, "a.js", "  var ret;\n"),
    new SourceNode(3, 0, "a.js", "  // Empty line\n"),
    new SourceNode(4, 0, "a.js", "  ret = 'a';\n"),
    new SourceNode(5, 0, "a.js", "  return ret;\n"),
    new SourceNode(6, 0, "a.js", "}\n"),
  ]);
  const b = new SourceNode(null, null, null, [
    new SourceNode(1, 0, "b.js", "function b() {\n"),
    new SourceNode(2, 0, "b.js", "  var ret;\n"),
    new SourceNode(3, 0, "b.js", "  // Empty line\n"),
    new SourceNode(4, 0, "b.js", "  ret = 'b';\n"),
    new SourceNode(5, 0, "b.js", "  return ret;\n"),
    new SourceNode(6, 0, "b.js", "}\n"),
  ]);
  const c = new SourceNode(null, null, null, [
    new SourceNode(1, 0, "c.js", "function c() {\n"),
    new SourceNode(2, 0, "c.js", "  var ret;\n"),
    new SourceNode(3, 0, "c.js", "  // Empty line\n"),
    new SourceNode(4, 0, "c.js", "  ret = 'c';\n"),
    new SourceNode(5, 0, "c.js", "  return ret;\n"),
    new SourceNode(6, 0, "c.js", "}\n"),
  ]);

  let { code, map } = (new SourceNode(null, null, null, [
    a, b, c,
  ])).toStringWithSourceMap({
    file: "http://example.com/www/js/abc.js",
    sourceRoot: "http://example.com/www/js/",
  });

  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  Cu.evalInSandbox(code, gDebuggee, "1.8",
                   "http://example.com/www/js/abc.js", 1);
}
