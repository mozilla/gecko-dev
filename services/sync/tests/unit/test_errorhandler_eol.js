/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");

Cu.import("resource://testing-common/services/sync/fakeservices.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function baseHandler(eolCode, request, response, statusCode, status, body) {
  let alertBody = {
    code: eolCode,
    message: "Service is EOLed.",
    url: "http://getfirefox.com",
  };
  response.setHeader("X-Weave-Timestamp", "" + new_timestamp(), false);
  response.setHeader("X-Weave-Alert", "" + JSON.stringify(alertBody), false);
  response.setStatusLine(request.httpVersion, statusCode, status);
  response.bodyOutputStream.write(body, body.length);
}

function handler513(request, response) {
  let statusCode = 513;
  let status = "Upgrade Required";
  let body = "{}";
  baseHandler("hard-eol", request, response, statusCode, status, body);
}

function handler200(eolCode) {
  return function (request, response) {
    let statusCode = 200;
    let status = "OK";
    let body = "{\"meta\": 123456789010}";
    baseHandler(eolCode, request, response, statusCode, status, body);
  };
}

function sync_httpd_setup(infoHandler) {
  let handlers = {
    "/1.1/johndoe/info/collections": infoHandler,
  };
  return httpd_setup(handlers);
}

function* setUp(server) {
  yield configureIdentity({username: "johndoe"});
  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = server.baseURI + "/";
  new FakeCryptoService();
}

function run_test() {
  run_next_test();
}

function do_check_soft_eol(eh, start) {
  // We subtract 1000 because the stored value is in second precision.
  do_check_true(eh.earliestNextAlert >= (start + eh.MINIMUM_ALERT_INTERVAL_MSEC - 1000));
  do_check_eq("soft-eol", eh.currentAlertMode);
}
function do_check_hard_eol(eh, start) {
  // We subtract 1000 because the stored value is in second precision.
  do_check_true(eh.earliestNextAlert >= (start + eh.MINIMUM_ALERT_INTERVAL_MSEC - 1000));
  do_check_eq("hard-eol", eh.currentAlertMode);
  do_check_true(Status.eol);
}

add_identity_test(this, function* test_200_hard() {
  let eh = Service.errorHandler;
  let start = Date.now();
  let server = sync_httpd_setup(handler200("hard-eol"));
  yield setUp(server);

  let deferred = Promise.defer();
  let obs = function (subject, topic, data) {
    Svc.Obs.remove("weave:eol", obs);
    do_check_eq("hard-eol", subject.code);
    do_check_hard_eol(eh, start);
    do_check_eq(Service.scheduler.eolInterval, Service.scheduler.syncInterval);
    eh.clearServerAlerts();
    server.stop(deferred.resolve);
  };

  Svc.Obs.add("weave:eol", obs);
  Service._fetchInfo();
  Service.scheduler.adjustSyncInterval();   // As if we failed or succeeded in syncing.
  yield deferred.promise;
});

add_identity_test(this, function* test_513_hard() {
  let eh = Service.errorHandler;
  let start = Date.now();
  let server = sync_httpd_setup(handler513);
  yield setUp(server);

  let deferred = Promise.defer();
  let obs = function (subject, topic, data) {
    Svc.Obs.remove("weave:eol", obs);
    do_check_eq("hard-eol", subject.code);
    do_check_hard_eol(eh, start);
    do_check_eq(Service.scheduler.eolInterval, Service.scheduler.syncInterval);
    eh.clearServerAlerts();
    server.stop(deferred.resolve);
  };

  Svc.Obs.add("weave:eol", obs);
  try {
    Service._fetchInfo();
    Service.scheduler.adjustSyncInterval();   // As if we failed or succeeded in syncing.
  } catch (ex) {
    // Because fetchInfo will fail on a 513.
  }
  yield deferred.promise;
});

add_identity_test(this, function* test_200_soft() {
  let eh = Service.errorHandler;
  let start = Date.now();
  let server = sync_httpd_setup(handler200("soft-eol"));
  yield setUp(server);

  let deferred = Promise.defer();
  let obs = function (subject, topic, data) {
    Svc.Obs.remove("weave:eol", obs);
    do_check_eq("soft-eol", subject.code);
    do_check_soft_eol(eh, start);
    do_check_eq(Service.scheduler.singleDeviceInterval, Service.scheduler.syncInterval);
    eh.clearServerAlerts();
    server.stop(deferred.resolve);
  };

  Svc.Obs.add("weave:eol", obs);
  Service._fetchInfo();
  Service.scheduler.adjustSyncInterval();   // As if we failed or succeeded in syncing.
  yield deferred.promise;
});
