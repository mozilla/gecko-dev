/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");

function run_test()
{
  // Should get an exception if we try to interact with DebuggerServer
  // before we initialize it...
  check_except(function() {
    DebuggerServer.createListener();
  });
  check_except(DebuggerServer.closeAllListeners);
  check_except(DebuggerServer.connectPipe);

  // Allow incoming connections.
  DebuggerServer.init();

  // These should still fail because we haven't added a createRootActor
  // implementation yet.
  check_except(function() {
    DebuggerServer.createListener();
  });
  check_except(DebuggerServer.closeAllListeners);
  check_except(DebuggerServer.connectPipe);

  DebuggerServer.registerModule("xpcshell-test/testactors");

  // Now they should work.
  DebuggerServer.createListener();
  DebuggerServer.closeAllListeners();

  // Make sure we got the test's root actor all set up.
  let client1 = DebuggerServer.connectPipe();
  client1.hooks = {
    onPacket: function(aPacket1) {
      do_check_eq(aPacket1.from, "root");
      do_check_eq(aPacket1.applicationType, "xpcshell-tests");

      // Spin up a second connection, make sure it has its own root
      // actor.
      let client2 = DebuggerServer.connectPipe();
      client2.hooks = {
        onPacket: function(aPacket2) {
          do_check_eq(aPacket2.from, "root");
          do_check_neq(aPacket1.testConnectionPrefix,
                       aPacket2.testConnectionPrefix);
          client2.close();
        },
        onClosed: function(aResult) {
          client1.close();
        },
      };
      client2.ready();
    },

    onClosed: function(aResult) {
      do_test_finished();
    },
  };

  client1.ready();
  do_test_pending();
}
