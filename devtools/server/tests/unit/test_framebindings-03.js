/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* strict mode code may not contain 'with' statements */
/* eslint-disable strict */

/**
 * Check a |with| frame actor's bindings.
 */

var gDebuggee;
var gClient;
var gThreadClient;

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const env = packet.frame.environment;
    Assert.notEqual(env, undefined);

    const parentEnv = env.parent;
    Assert.notEqual(parentEnv, undefined);

    const bindings = parentEnv.bindings;
    const args = bindings.arguments;
    const vars = bindings.variables;
    Assert.equal(args.length, 1);
    Assert.equal(args[0].number.value, 10);
    Assert.equal(vars.r.value, 10);
    Assert.equal(vars.a.value, Math.PI * 100);
    Assert.equal(vars.arguments.value.class, "Arguments");
    Assert.ok(!!vars.arguments.value.actor);

    const objClient = gThreadClient.pauseGrip(env.object);
    objClient.getPrototypeAndProperties(function(response) {
      Assert.equal(response.ownProperties.PI.value, Math.PI);
      Assert.equal(response.ownProperties.cos.value.type, "object");
      Assert.equal(response.ownProperties.cos.value.class, "Function");
      Assert.ok(!!response.ownProperties.cos.value.actor);

      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(number) {
      var a;
      var r = number;
      with (Math) {
        a = PI * r * r;
        debugger;
      }
    }
    stopMe(10);
  } + ")()");
  /* eslint-enable */
}
