/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.import("resource://gre/modules/Services.jsm");

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var prefs;
var tlsProfile;
var serverURL;
var serverPort = -1;

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  serverPort = env.get("MOZHTTP2-PORT");
  do_check_neq(serverPort, null);

  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  tlsProfile = prefs.getBoolPref("network.http.spdy.enforce-tls-profile");

  // Set to allow the cert presented by our H2 server
  var oldPref = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", false);

  addCertOverride("localhost", serverPort,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  serverURL = "https://localhost:" + serverPort;

  disableServiceWorkerEvents(
    'https://example.org/1',
    'https://example.org/no_receiptEndpoint'
  );

  run_next_test();
}

add_task(function* test_pushSubscriptionSuccess() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionSuccess/subscribe",
    db
  });

  let newRecord = yield PushNotificationService.register(
    'https://example.org/1',
    ChromeUtils.originAttributesToSuffix({ appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false })
  );

  var subscriptionUri = serverURL + '/pushSubscriptionSuccesss';
  var pushEndpoint = serverURL + '/pushEndpointSuccess';
  var pushReceiptEndpoint = serverURL + '/receiptPushEndpointSuccess';
  equal(newRecord.subscriptionUri, subscriptionUri,
    'Wrong subscription ID in registration record');
  equal(newRecord.pushEndpoint, pushEndpoint,
    'Wrong push endpoint in registration record');

  equal(newRecord.pushReceiptEndpoint, pushReceiptEndpoint,
    'Wrong push endpoint receipt in registration record');
  equal(newRecord.scope, 'https://example.org/1',
    'Wrong scope in registration record');

  let record = yield db.getByKeyID(subscriptionUri);
  equal(record.subscriptionUri, subscriptionUri,
    'Wrong subscription ID in database record');
  equal(record.pushEndpoint, pushEndpoint,
    'Wrong push endpoint in database record');
  equal(record.pushReceiptEndpoint, pushReceiptEndpoint,
    'Wrong push endpoint receipt in database record');
  equal(record.scope, 'https://example.org/1',
    'Wrong scope in database record');

  db.drop().then(PushService.uninit());
});

add_task(function* test_pushSubscriptionMissingLink2() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionMissingLink2/subscribe",
    db
  });

  let newRecord = yield PushNotificationService.register(
    'https://example.org/no_receiptEndpoint',
    ChromeUtils.originAttributesToSuffix({ appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false })
  );

  var subscriptionUri = serverURL + '/subscriptionMissingLink2';
  var pushEndpoint = serverURL + '/pushEndpointMissingLink2';
  var pushReceiptEndpoint = '';
  equal(newRecord.subscriptionUri, subscriptionUri,
    'Wrong subscription ID in registration record');
  equal(newRecord.pushEndpoint, pushEndpoint,
    'Wrong push endpoint in registration record');

  equal(newRecord.pushReceiptEndpoint, pushReceiptEndpoint,
    'Wrong push endpoint receipt in registration record');
  equal(newRecord.scope, 'https://example.org/no_receiptEndpoint',
    'Wrong scope in registration record');

  let record = yield db.getByKeyID(subscriptionUri);
  equal(record.subscriptionUri, subscriptionUri,
    'Wrong subscription ID in database record');
  equal(record.pushEndpoint, pushEndpoint,
    'Wrong push endpoint in database record');
  equal(record.pushReceiptEndpoint, pushReceiptEndpoint,
    'Wrong push endpoint receipt in database record');
  equal(record.scope, 'https://example.org/no_receiptEndpoint',
    'Wrong scope in database record');
});

add_task(function* test_complete() {
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", tlsProfile);
});
