/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

ChromeUtils.defineESModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
});

add_task(async function test_expiredScalar() {
  const EXPIRED_SCALAR = "telemetry.test.expired";
  const EXPIRED_KEYED_SCALAR = "telemetry.test.keyed_expired";
  const UNEXPIRED_SCALAR = "telemetry.test.unexpired";

  Telemetry.clearScalars();

  // Try to set the expired scalar to some value. We will not be recording the value,
  // but we shouldn't throw.
  Glean.testOnly.expired.add(11715);
  Glean.testOnly.keyedExpired.some_key.add(11715);

  // The unexpired scalar has an expiration version, but far away in the future.
  const expectedValue = 11716;
  Glean.testOnly.unexpired.add(expectedValue);

  // Get a snapshot of the scalars.
  const scalars = TelemetryTestUtils.getProcessScalars("parent");
  const keyedScalars = TelemetryTestUtils.getProcessScalars("parent");

  Assert.ok(
    !(EXPIRED_SCALAR in scalars),
    "The expired scalar must not be persisted."
  );
  Assert.equal(
    scalars[UNEXPIRED_SCALAR],
    expectedValue,
    "The unexpired scalar must be persisted with the correct value."
  );
  Assert.ok(
    !(EXPIRED_KEYED_SCALAR in keyedScalars),
    "The expired keyed scalar must not be persisted."
  );
});

add_task(async function test_scalarRecording() {
  const OPTIN_SCALAR = "telemetry.test.release_optin";
  const OPTOUT_SCALAR = "telemetry.test.release_optout";

  let checkValue = (scalarName, expectedValue) => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent");
    Assert.equal(
      scalars[scalarName],
      expectedValue,
      scalarName + " must contain the expected value."
    );
  };

  let checkNotSerialized = scalarName => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent");
    Assert.ok(!(scalarName in scalars), scalarName + " was not recorded.");
  };

  Telemetry.canRecordBase = false;
  Telemetry.canRecordExtended = false;
  Telemetry.clearScalars();

  // Check that no scalar is recorded if both base and extended recording are off.
  Glean.testOnly.releaseOptout.add(3);
  Glean.testOnly.releaseOptin.add(3);
  checkNotSerialized(OPTOUT_SCALAR);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that opt-out scalars are recorded, while opt-in are not.
  Telemetry.canRecordBase = true;
  Glean.testOnly.releaseOptout.add(3);
  Glean.testOnly.releaseOptin.add(3);
  checkValue(OPTOUT_SCALAR, 3);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that both opt-out and opt-in scalars are recorded.
  Telemetry.canRecordExtended = true;
  Glean.testOnly.releaseOptout.add(5);
  Glean.testOnly.releaseOptin.add(6);
  checkValue(OPTOUT_SCALAR, 8);
  checkValue(OPTIN_SCALAR, 6);
});

add_task(async function test_keyedScalarRecording() {
  const OPTIN_SCALAR = "telemetry.test.keyed_release_optin";
  const OPTOUT_SCALAR = "telemetry.test.keyed_release_optout";
  const testKey = "policy_key";

  let checkValue = (scalarName, expectedValue) => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    Assert.equal(
      scalars[scalarName][testKey],
      expectedValue,
      scalarName + " must contain the expected value."
    );
  };

  let checkNotSerialized = scalarName => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    Assert.ok(!(scalarName in scalars), scalarName + " was not recorded.");
  };

  Telemetry.canRecordBase = false;
  Telemetry.canRecordExtended = false;
  Telemetry.clearScalars();

  // Check that no scalar is recorded if both base and extended recording are off.
  Glean.testOnly.keyedReleaseOptout[testKey].add(3);
  Glean.testOnly.keyedReleaseOptin[testKey].add(3);
  checkNotSerialized(OPTOUT_SCALAR);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that opt-out scalars are recorded, while opt-in are not.
  Telemetry.canRecordBase = true;
  Glean.testOnly.keyedReleaseOptout[testKey].add(3);
  Glean.testOnly.keyedReleaseOptin[testKey].add(3);
  checkValue(OPTOUT_SCALAR, 3);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that both opt-out and opt-in scalars are recorded.
  Telemetry.canRecordExtended = true;
  Glean.testOnly.keyedReleaseOptout[testKey].add(5);
  Glean.testOnly.keyedReleaseOptin[testKey].add(6);
  checkValue(OPTOUT_SCALAR, 8);
  checkValue(OPTIN_SCALAR, 6);
});

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_productSpecificScalar() {
    const DEFAULT_PRODUCT_SCALAR = "telemetry.test.default_products";
    const DESKTOP_ONLY_SCALAR = "telemetry.test.desktop_only";
    const MULTIPRODUCT_SCALAR = "telemetry.test.multiproduct";
    const MOBILE_ONLY_SCALAR = "telemetry.test.mobile_only";
    const MOBILE_ONLY_KEYED_SCALAR = "telemetry.test.keyed_mobile_only";

    Telemetry.clearScalars();

    // Try to set the desktop scalars
    let expectedValue = 11714;
    Glean.testOnly.defaultProducts.add(expectedValue);
    Glean.testOnly.desktopOnly.add(expectedValue);
    Glean.testOnly.multiproduct.add(expectedValue);

    // Try to set the mobile-only scalar to some value. We will not be recording the value,
    // but we shouldn't throw.
    let expectedKey = "some_key";
    Glean.testOnly.mobileOnly.add(11715);
    Glean.testOnly.keyedMobileOnly[expectedKey].add(11715);

    // Get a snapshot of the scalars.
    const scalars = TelemetryTestUtils.getProcessScalars("parent");
    const keyedScalars = TelemetryTestUtils.getProcessScalars("parent");

    Assert.equal(
      scalars[DEFAULT_PRODUCT_SCALAR],
      expectedValue,
      "The default platfomrs scalar must contain the right value"
    );
    Assert.equal(
      scalars[DESKTOP_ONLY_SCALAR],
      expectedValue,
      "The desktop-only scalar must contain the right value"
    );
    Assert.equal(
      scalars[MULTIPRODUCT_SCALAR],
      expectedValue,
      "The multiproduct scalar must contain the right value"
    );

    Assert.ok(
      !(MOBILE_ONLY_SCALAR in scalars),
      "The mobile-only scalar must not be persisted."
    );
    Assert.ok(
      !(MOBILE_ONLY_KEYED_SCALAR in keyedScalars),
      "The mobile-only keyed scalar must not be persisted."
    );
  }
);
