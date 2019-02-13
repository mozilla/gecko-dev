/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = 'b2546987-4f63-49b1-99f7-739cd3c40e44';
const channelID = '35a820f7-d7dd-43b3-af21-d65352212ae3';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  disableServiceWorkerEvents(
    'https://example.com/storage-error'
  );
  run_next_test();
}

add_task(function* test_register_rollback() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  let handshakes = 0;
  let registers = 0;
  let unregisterDefer = Promise.defer();
  PushServiceWebSocket._generateID = () => channelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db: makeStub(db, {
      put(prev, record) {
        return Promise.reject('universe has imploded');
      }
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          handshakes++;
          equal(request.uaid, userAgentID, 'Handshake: wrong device ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID
          }));
        },
        onRegister(request) {
          equal(request.channelID, channelID, 'Register: wrong channel ID');
          registers++;
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            uaid: userAgentID,
            channelID,
            pushEndpoint: 'https://example.com/update/rollback'
          }));
        },
        onUnregister(request) {
          equal(request.channelID, channelID, 'Unregister: wrong channel ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'unregister',
            status: 200,
            channelID
          }));
          unregisterDefer.resolve();
        }
      });
    }
  });

  // Should return a rejected promise if storage fails.
  yield rejects(
    PushNotificationService.register('https://example.com/storage-error',
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    function(error) {
      return error == 'universe has imploded';
    },
    'Wrong error for unregister database failure'
  );

  // Should send an out-of-band unregister request.
  yield waitForPromise(unregisterDefer.promise, DEFAULT_TIMEOUT,
    'Unregister request timed out');
  equal(handshakes, 1, 'Wrong handshake count');
  equal(registers, 1, 'Wrong register count');
});
