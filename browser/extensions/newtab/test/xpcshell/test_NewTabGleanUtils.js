/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test file contains tests for NewTabGleanUtils functionality.
 * It tests the registration of metrics and pings for Glean telemetry in the newtab context.
 */

ChromeUtils.defineESModuleGetters(this, {
  NewTabGleanUtils: "resource://newtab/lib/NewTabGleanUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const TEST_RESOURCE_URI = "resource://test.json";

function test_setup() {
  const sandbox = sinon.createSandbox();
  // bug 1954203 - use a builtin ping name to avoid bug with testResetFOG
  Services.fog.testResetFOG();

  sandbox.stub(NewTabGleanUtils, "readJSON").returns("{}");
  return sandbox;
}

/**
 * Test case: Empty metrics/pings file
 * Verifies that no registration occurs when the metrics file is empty
 */
add_task(async function test_registerMetricsAndPings_emptyFile() {
  const sandbox = test_setup();
  // Test with empty data
  let result =
    await NewTabGleanUtils.registerMetricsAndPings(TEST_RESOURCE_URI);
  Assert.ok(!result, "No metrics or ping registration for empty file");

  sandbox.restore();
});

/**
 * Test case: Invalid metrics/pings format
 * Verifies that registration fails when the metrics file contains invalid format
 * with missing required metric and pings properties
 */
add_task(async function test_registerMetricsAndPings_invalidFormat() {
  const sandbox = test_setup();

  // Test with invalid metrics and pings format
  NewTabGleanUtils.readJSON.returns({
    pings: {
      ping1: { include_client_id: true },
      ping2: { include_client_id: false },
    },
    metrics: {
      category: {
        metric1: { type: "counter" },
        metric2: { type: "string" },
      },
    },
  });

  let result =
    await NewTabGleanUtils.registerMetricsAndPings(TEST_RESOURCE_URI);
  Assert.ok(!result, "Registration failure for invalid metric and ping format");

  sandbox.restore();
});

/**
 * Test case: Valid metrics/pings format
 * Verifies successful registration with properly formatted metrics and pings
 * Tests with all required fields and correct property names
 */
add_task(async function test_registerMetricsAndPings_validFormat() {
  const sandbox = test_setup();

  // Test with valid metrics and pings format
  NewTabGleanUtils.readJSON.returns({
    pings: {
      newtab_ping: {
        includeClientId: false,
        sendIfEmpty: false,
        preciseTimestamps: true,
        includeInfoSections: true,
        enabled: true,
        schedulesPings: [],
        reasonCodes: [],
        followsCollectionEnabled: true,
        uploaderCapabilities: [],
      },
    },
    metrics: {
      newtab_category: {
        metric1: {
          type: "text",
          description: "test-description",
          lifetime: "ping",
          pings: ["newtab"],
          disabled: false,
        },
      },
    },
  });

  let result =
    await NewTabGleanUtils.registerMetricsAndPings(TEST_RESOURCE_URI);
  Assert.ok(result, "Registration success for valid metric and ping formats");

  sandbox.restore();
});

/**
 * Test case: Optional metric description
 * Verifies that metric registration succeeds even when description is missing
 * Tests that description is not a required field for metric registration
 */
add_task(
  async function test_registerMetricsAndPings_metricDescriptionOptional() {
    const sandbox = test_setup();
    NewTabGleanUtils.readJSON.returns({
      metrics: {
        newtab: {
          metric2: {
            type: "text",
            lifetime: "ping",
            pings: ["newtab"],
            disabled: false,
          },
        },
      },
    });

    let result =
      await NewTabGleanUtils.registerMetricsAndPings(TEST_RESOURCE_URI);
    Assert.ok(
      result,
      `Registration success for metric2 with missing description.
      Description property not required in JSON for metric registration`
    );

    sandbox.restore();
  }
);

/**
 * Test case: Metric registration telemetry
 * Verifies that telemetry is properly recorded for both successful and failed metric registrations
 * Tests with valid and invalid metric options
 */
add_task(async function test_registerMetricIfNeeded_telemetrySent() {
  const validOptions = {
    name: "metric1",
    type: "text",
    category: "test",
    pings: ["newtab"],
    lifetime: "ping",
    disabled: false,
  };

  NewTabGleanUtils.registerMetricIfNeeded(validOptions);

  // Check instrumented telemetry records success
  Assert.ok(
    Glean.newtab.metricRegistered.metric1.testGetValue(),
    "Glean metricRegistered telemetry sent with value as true"
  );

  const invalidOptions = {
    name: "metric2",
    type: "text",
    category: "test",
    pings: ["newtab"],
    disabled: false,
  };

  // Throws when required lifetime property missing
  Assert.throws(
    () => NewTabGleanUtils.registerMetricIfNeeded(invalidOptions),
    /Failure while registering metrics metric2/,
    "Throws when metric registration fails due to missing lifetime param"
  );

  // Check instrumented telemetry records failure
  Assert.ok(
    Glean.newtab.metricRegistered.metric2.testGetValue() === false,
    "Glean metricRegistered telemetry sent with value as false"
  );
});

/**
 * Test case: Ping registration telemetry
 * Verifies that telemetry is properly recorded for both successful and failed ping registrations
 * Tests with valid and invalid ping options
 */
add_task(async function test_registerPingIfNeeded_telemetrySent() {
  const validOptions = {
    name: "ping1",
    includeClientId: false,
    sendIfEmpty: false,
    preciseTimestamps: true,
    includeInfoSections: true,
    enabled: true,
    schedulesPings: [],
    reasonCodes: [],
    followsCollectionEnabled: true,
    uploaderCapabilities: [],
  };

  NewTabGleanUtils.registerPingIfNeeded(validOptions);
  // Check instrumented telemetry records success for registered ping
  Assert.ok(
    Glean.newtab.pingRegistered.ping1.testGetValue(),
    "Glean pingRegistered telemetry sent with value as true for ping1"
  );

  const invalidOptions = {
    name: "ping2",
    includeClientId: false,
    sendIfEmpty: false,
    preciseTimestamps: true,
    includeInfoSections: true,
    enabled: true,
    schedulesPings: [],
    reasonCodes: [],
    followsCollectionEnabled: true,
  };

  // And test methods throw appropriately
  Assert.throws(
    () => NewTabGleanUtils.registerPingIfNeeded(invalidOptions),
    /Failure while registering ping ping2/,
    "Throws when ping registration fails due to missing uploaderCapabilities param"
  );

  // Check instrumented telemetry records failure
  Assert.ok(
    Glean.newtab.pingRegistered.ping2.testGetValue() === false,
    "Glean pingRegistered telemetry sent with value as false for ping2"
  );
});

/**
 * Test case: Event metric registration
 * Verifies proper registration and recording of event metrics
 * Tests both basic event metrics and those with extra arguments
 */
add_task(async function test_registerMetricIfNeeded_eventMetrics() {
  const options = {
    name: "event1",
    category: "test_category",
    type: "event",
    pings: ["events"],
    lifetime: "ping",
    disabled: false,
  };

  NewTabGleanUtils.registerMetricIfNeeded(options);

  // Check instrumented telemetry records success
  Assert.ok(
    Glean.newtab.metricRegistered.event1.testGetValue(),
    "Glean metricRegistered telemetry sent with value as true"
  );

  const optionsWithExtra = {
    name: "event2",
    category: "test_category",
    type: "event",
    pings: ["events"],
    lifetime: "ping",
    disabled: false,
    extraArgs: {
      allowed_extra_keys: ["extra1", "extra2"],
    },
  };

  NewTabGleanUtils.registerMetricIfNeeded(optionsWithExtra);

  // Check instrumented telemetry records success
  Assert.ok(
    Glean.newtab.metricRegistered.event2.testGetValue(),
    "Glean metricRegistered telemetry sent with value as true"
  );

  let extra = { extra1: "extra1 value", extra2: "extra2 value" };
  Glean.testCategory.event2.record(extra);

  let events = Glean.testCategory.event2.testGetValue();
  Assert.equal(1, events.length, "Events recorded count");
});

/**
 * Test case: Non-event metric registration
 * Verifies that non-event metrics cannot use the record method
 * Tests that appropriate errors are thrown when trying to record non-event metrics
 */
add_task(async function test_registerMetricIfNeeded_nonEventMetrics() {
  const options = {
    name: "event3",
    type: "text",
    category: "test_category1",
    pings: ["events"],
    lifetime: "ping",
    disabled: false,
    extraArgs: {
      allowed_extra_keys: ["extra1", "extra2"],
    },
  };

  NewTabGleanUtils.registerMetricIfNeeded(options);

  // Check instrumented telemetry records success
  Assert.ok(
    Glean.newtab.metricRegistered.event3.testGetValue(),
    "Glean metricRegistered telemetry sent with value as true"
  );

  let extra = { extra1: "extra1 value", extra2: "extra2 value" };

  // And test methods throw appropriately
  Assert.throws(
    () => Glean.testCategory1.event3.record(extra),
    /TypeError/,
    "Throws when using record for non event type"
  );

  Glean.testCategory1.event3.set("Test");

  Assert.equal(
    "Test",
    Glean.testCategory1.event3.testGetValue(),
    "Success when using set to send data of type text"
  );
});
