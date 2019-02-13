/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var httpServer = null;

XPCOMUtils.defineLazyGetter(this, "serverPort", function() {
  return httpServer.identity.primaryPort;
});

var retries = 0

function listen5xxCodeHandler(metadata, response) {
  ok(true, "Listener 5xx code");
  do_test_finished();
  retries++;
  response.setHeader("Retry-After", '1');
  response.setStatusLine(metadata.httpVersion, 500, "Retry");
}

function resubscribeHandler(metadata, response) {
  ok(true, "Ask for new subscription");
  ok(retries == 3, "Should retry 2 times.");
  do_test_finished();
  response.setHeader("Location",
                  'http://localhost:' + serverPort + '/newSubscription')
  response.setHeader("Link",
                  '</newPushEndpoint>; rel="urn:ietf:params:push", ' +
                  '</newReceiptPushEndpoint>; rel="urn:ietf:params:push:receipt"');
  response.setStatusLine(metadata.httpVersion, 201, "OK");
}

function listenSuccessHandler(metadata, response) {
  do_check_true(true, "New listener point");
  httpServer.stop(do_test_finished);
  response.setStatusLine(metadata.httpVersion, 204, "Try again");
}


httpServer = new HttpServer();
httpServer.registerPathHandler("/subscription5xxCode", listen5xxCodeHandler);
httpServer.registerPathHandler("/subscribe", resubscribeHandler);
httpServer.registerPathHandler("/newSubscription", listenSuccessHandler);
httpServer.start(-1);

function run_test() {

  do_get_profile();
  setPrefs({
    'http2.retryInterval': 1000,
    'http2.maxRetries': 2
  });
  disableServiceWorkerEvents(
    'https://example.com/page'
  );

  run_next_test();
}

add_task(function* test1() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();

  var serverURL = "http://localhost:" + httpServer.identity.primaryPort;

  let records = [{
    subscriptionUri: serverURL + '/subscription5xxCode',
    pushEndpoint: serverURL + '/pushEndpoint',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint',
    scope: 'https://example.com/page'
  }];

  for (let record of records) {
    yield db.put(record);
  }

  PushService.init({
    serverURI: serverURL + "/subscribe",
    service: PushServiceHttp2,
    db
  });

});
