/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that ObjectFront.prototype.getDefinitionSite and the "definitionSite"
// request work properly.

var gDebuggee;
var gClient;
var gThreadFront;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  Cu.evalInSandbox(
    function stopMe() {
      debugger;
    }.toString(),
    gDebuggee
  );

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips", function(
      response,
      targetFront,
      threadFront
    ) {
      gThreadFront = threadFront;
      add_pause_listener();
    });
  });
  do_test_pending();
}

function add_pause_listener() {
  gThreadFront.once("paused", function(packet) {
    const [funcGrip, objGrip] = packet.frame.arguments;
    const func = gThreadFront.pauseGrip(funcGrip);
    const obj = gThreadFront.pauseGrip(objGrip);
    test_definition_site(func, obj);
  });

  eval_code();
}

function eval_code() {
  Cu.evalInSandbox(
    [
      "this.line0 = Error().lineNumber;",
      "function f() {}",
      "stopMe(f, {});",
    ].join("\n"),
    gDebuggee
  );
}

async function test_definition_site(func, obj) {
  const response = await func.getDefinitionSite();
  Assert.ok(!response.error);
  Assert.equal(response.source.url, getFilePath("test_objectgrips-13.js"));
  Assert.equal(response.line, gDebuggee.line0 + 1);
  Assert.equal(response.column, 0);

  test_bad_definition_site(obj);
}

function test_bad_definition_site(obj) {
  try {
    obj._client.request("definitionSite", () => Assert.ok(false));
  } catch (e) {
    gThreadFront.resume().then(() => finishClient(gClient));
  }
}
