/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gClient;
var gThreadClient;

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

// Test that closures can be inspected.

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-closures");

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-closures",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_object_grip();
                           });
  });
  do_test_pending();
}

function test_object_grip() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const person = packet.frame.environment.bindings.variables.person;

    Assert.equal(person.value.class, "Object");

    const personClient = gThreadClient.pauseGrip(person.value);
    personClient.getPrototypeAndProperties(response => {
      Assert.equal(response.ownProperties.getName.value.class, "Function");

      Assert.equal(response.ownProperties.getAge.value.class, "Function");

      Assert.equal(response.ownProperties.getFoo.value.class, "Function");

      const getNameClient = gThreadClient.pauseGrip(response.ownProperties.getName.value);
      const getAgeClient = gThreadClient.pauseGrip(response.ownProperties.getAge.value);
      const getFooClient = gThreadClient.pauseGrip(response.ownProperties.getFoo.value);
      getNameClient.getScope(response => {
        Assert.equal(response.scope.bindings.arguments[0].name.value, "Bob");

        getAgeClient.getScope(response => {
          Assert.equal(response.scope.bindings.arguments[1].age.value, 58);

          getFooClient.getScope(response => {
            Assert.equal(response.scope.bindings.variables.foo.value, 10);

            gThreadClient.resume(() => finishClient(gClient));
          });
        });
      });
    });
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    var PersonFactory = function (name, age) {
      var foo = 10;
      return {
        getName: function () { return name; },
        getAge: function () { return age; },
        getFoo: function () { foo = Date.now(); return foo; }
      };
    };
    var person = new PersonFactory("Bob", 58);
    debugger;
  } + ")()");
  /* eslint-enable */
}
