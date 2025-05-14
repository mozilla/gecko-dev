/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const fakeAppInfo = {
  ID: "xpcshell@tests.mozilla.org",
  name: "XPCShell",
  platformVersion: "1.0",
  version: "1",
  appBuildID: "buildID-1",
  lastAppVersion: "1",
  lastAppBuildID: "buildID-1",
};

AddonTestUtils.init(this);
AddonTestUtils.updateAppInfo(fakeAppInfo);

add_setup(function () {
  Services.fog.initializeFOG();
});

async function testXPIStateWriteErrorTelemetry({
  mockError,
  expectedGleanEvent,
}) {
  Services.fog.testResetFOG();
  await promiseStartupManager();
  let XPIStates = AddonTestUtils.getXPIExports().XPIInternal.XPIStates;
  await XPIStates.save();
  // Sanity check.
  ok(!!XPIStates._jsonFile, "Expect XPIStates._jsonFile to be defined");
  info("Mock XPIStates.save failure");
  const sandbox = sinon.createSandbox();
  sandbox.stub(XPIStates._jsonFile, "saveSoon").callsFake(
    function fakeSaveSoon() {
      info(`jsonFile.saveSoon called for ${this.sanitizedBasename}\n`);
      this._saveFailureHandler(mockError);
    }.bind(XPIStates._jsonFile)
  );
  await XPIStates.save();
  Assert.deepEqual(
    Glean.addonsManager.xpistatesWriteErrors
      .testGetValue()
      .map(event => event.extra),
    [expectedGleanEvent],
    "Got the expected data in the xpistatesWriteError Glean event"
  );
  await promiseShutdownManager();
  sandbox.restore();
}

add_task(async function test_save_failure_on_new_profile() {
  Services.appinfo.lastAppVersion = null;
  const internalError = new InternalError("fake internal error");
  await testXPIStateWriteErrorTelemetry({
    mockError: internalError,
    expectedGleanEvent: {
      error_type: "InternalError",
      profile_state: "new",
    },
  });
});

add_task(async function test_toomuchrecursion_failure() {
  AddonTestUtils.updateAppInfo(fakeAppInfo);
  const errorTooMuchRecursion = new InternalError("too much recursion");
  await testXPIStateWriteErrorTelemetry({
    mockError: errorTooMuchRecursion,
    expectedGleanEvent: {
      error_type: "TooMuchRecursion",
      profile_state: "existing",
    },
  });
});

add_task(async function test_save_failure_on_app_version_changed() {
  AddonTestUtils.updateAppInfo({
    ...fakeAppInfo,
    version: "2",
  });
  const someOtherErrorName = new Error("fake error name");
  someOtherErrorName.name = "SomeOtherErrorName";
  await testXPIStateWriteErrorTelemetry({
    mockError: someOtherErrorName,
    expectedGleanEvent: {
      error_type: "SomeOtherErrorName",
      profile_state: "existingWithVersionChanged",
    },
  });
});

add_task(async function test_save_failure_on_app_buildid_changed() {
  AddonTestUtils.updateAppInfo({
    ...fakeAppInfo,
    appBuildID: "buildID-2",
  });
  const mockError = new Error("mock error");
  await testXPIStateWriteErrorTelemetry({
    mockError,
    expectedGleanEvent: {
      error_type: "Error",
      profile_state: "existingWithVersionChanged",
    },
  });
});

add_task(async function test_save_failure_on_unexpected_null_error() {
  AddonTestUtils.updateAppInfo(fakeAppInfo);
  await testXPIStateWriteErrorTelemetry({
    mockError: null,
    expectedGleanEvent: {
      error_type: "Unknown",
      profile_state: "existing",
    },
  });
});
