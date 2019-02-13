/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '7f0af1bb-7e1f-4fb8-8e4a-e8de434abde3';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 150,
    retryBaseInterval: 150
  });
  run_next_test();
}

add_task(function* test_unregister_invalid_json() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  let records = [{
    channelID: '87902e90-c57e-4d18-8354-013f4a556559',
    pushEndpoint: 'https://example.org/update/1',
    scope: 'https://example.edu/page/1',
    originAttributes: '',
    version: 1
  }, {
    channelID: '057caa8f-9b99-47ff-891c-adad18ce603e',
    pushEndpoint: 'https://example.com/update/2',
    scope: 'https://example.net/page/1',
    originAttributes: '',
    version: 1
  }];
  for (let record of records) {
    yield db.put(record);
  }

  let unregisterDefer = Promise.defer();
  let unregisterDone = after(2, unregisterDefer.resolve);
  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID
          }));
        },
        onUnregister(request) {
          this.serverSendMsg(');alert(1);(');
          unregisterDone();
        }
      });
    }
  });

  // "unregister" is fire-and-forget: it's sent via _send(), not
  // _sendRequest().
  yield PushNotificationService.unregister(
    'https://example.edu/page/1', '');
  let record = yield db.getByKeyID(
    '87902e90-c57e-4d18-8354-013f4a556559');
  ok(!record, 'Failed to delete unregistered record');

  yield PushNotificationService.unregister(
    'https://example.net/page/1', '');
  record = yield db.getByKeyID(
    '057caa8f-9b99-47ff-891c-adad18ce603e');
  ok(!record,
    'Failed to delete unregistered record after receiving invalid JSON');

  yield waitForPromise(unregisterDefer.promise, DEFAULT_TIMEOUT,
    'Timed out waiting for unregister');
});
