/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

let TEST_DATA = [
  // Mobile network type, expected success.
  { type: "mms", expectSuccess: true },
  { type: "supl", expectSuccess: true },
  { type: "ims", expectSuccess: true },
  { type: "dun", expectSuccess: true },
  // fota apn is not provided in TEST_APN_SETTINGS.
  { type: "fota", expectSuccess: false },
  // abcd is not a mobile network type.
  { type: "abcd", expectSuccess: false },
];

function verifyInitialState() {
  log("Verifying initial state.");

  // Data should be off and registration home before starting any test.
  return Promise.resolve()
    .then(getDataEnabled)
    .then(function(aResult) {
      is(aResult, false, "Data must be off by default.")
    });
}

function testRequestDataCall(aType, aExpectFailure) {
  log("= testRequestDataCall - type: " + aType) + " =";

  return requestDataCall(aType)
    .then(aDataCall => {
      ok(!aExpectFailure, "requestDataCall should not fail.")
      return aDataCall
    }, aErrorMsg => {
      ok(aExpectFailure, "requestDataCall expected to fail.")
    });
}

function testReleaseDataCall(aDataCall) {
  log(" = testReleaseDataCall - type: " + (aDataCall ? aDataCall.type : "") + " =");

  return releaseDataCall(aDataCall)
    .then(() => aDataCall.addHostRoute(TEST_HOST_ROUTE))
    .then(() => {
      ok(false, "Should not success on released data call.");
    }, aReason => {
      ok(true, "Expected error on released data call.");
    })
    .then(() => aDataCall.removeHostRoute(TEST_HOST_ROUTE))
    .then(() => {
      ok(false, "Should not success on released data call.");
    }, aReason => {
      ok(true, "Expected error on released data call.");
    });
}

startTestCommon(function() {
  let origApnSettings;

  return verifyInitialState()
  .then(() => getDataApnSettings())
  .then(value => {
    origApnSettings = value;
  })
  .then(() => setDataApnSettings(TEST_APN_SETTINGS))
  .then(() => {
    let promise = Promise.resolve();
    for (let i = 0; i < TEST_DATA.length; i++) {
      let entry = TEST_DATA[i];
      promise = promise.then(() => testRequestDataCall(entry.type,
                                                       !entry.expectSuccess))
        .then(aDataCall => {
          if (!entry.expectSuccess) {
            return;
          }
          return testReleaseDataCall(aDataCall);
        });
    }
    return promise;
  }).then(() => {
    if (origApnSettings) {
      return setDataApnSettings(origApnSettings);
    }
  });

}, ["settings-read", "settings-write", "settings-api-read", "settings-api-write"]);
