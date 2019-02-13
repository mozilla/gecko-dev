/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Outstanding requests should be rejected when the connection aborts
 * unexpectedly.
 */

let protocol = devtools.require("devtools/server/protocol");
let {method, Arg, Option, RetVal} = protocol;
let events = devtools.require("sdk/event/core");

function simpleHello() {
  return {
    from: "root",
    applicationType: "xpcshell-tests",
    traits: [],
  }
}

let RootActor = protocol.ActorClass({
  typeName: "root",
  initialize: function(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    // Root actor owns itself.
    this.manage(this);
    this.actorID = "root";
    this.sequence = 0;
  },

  sayHello: simpleHello,

  simpleReturn: method(function() {
    return this.sequence++;
  }, {
    response: { value: RetVal() },
  })
});

let RootFront = protocol.FrontClass(RootActor, {
  initialize: function(client) {
    this.actorID = "root";
    protocol.Front.prototype.initialize.call(this, client);
    // Root owns itself.
    this.manage(this);
  }
});

function run_test() {
  DebuggerServer.createRootActor = RootActor;
  DebuggerServer.init();

  let trace = connectPipeTracing();
  let client = new DebuggerClient(trace);
  let rootClient;

  client.connect((applicationType, traits) => {
    rootClient = RootFront(client);

    rootClient.simpleReturn().then(() => {
      ok(false, "Connection was aborted, request shouldn't resolve");
      do_test_finished();
    }, () => {
      ok(true, "Connection was aborted, request rejected correctly");
      do_test_finished();
    });

    trace.close();
  });

  do_test_pending();
}
