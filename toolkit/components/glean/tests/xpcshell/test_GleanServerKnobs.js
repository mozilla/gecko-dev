/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(function test_setup() {
  // Give FOG a temp profile to init within.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in
  // the pre-init queue.
  Services.fog.initializeFOG();
});

add_task(function test_fog_metrics_disabled_remotely() {
  // Set a cheesy string in the test metric. This should record because the
  // metric has `disabled: false` by default.
  const str1 = "a cheesy string!";
  Glean.testOnly.cheesyString.set(str1);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Create and set a feature configuration that disables the test metric.
  const feature_config = {
    metrics_enabled: {
      "test_only.cheesy_string": false,
    },
  };
  Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

  // Attempt to set another cheesy string in the test metric. This should not
  // record because of the override to the metric's default value in the
  // feature configuration.
  const str2 = "another cheesy string!";
  Glean.testOnly.cheesyString.set(str2);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Reset everything so it doesn't interfere with other tests.
  Services.fog.testResetFOG();
});

add_task(function test_fog_multiple_metrics_disabled_remotely() {
  // Set some test metrics. This should record because the metrics are
  // `disabled: false` by default.
  const str1 = "yet another a cheesy string!";
  Glean.testOnly.cheesyString.set(str1);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));
  const qty1 = 42;
  Glean.testOnly.meaningOfLife.set(qty1);
  Assert.equal(qty1, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Create and set a feature configuration that disables multiple test
  // metrics.
  var feature_config = {
    metrics_enabled: {
      "test_only.cheesy_string": false,
      "test_only.meaning_of_life": false,
    },
  };
  Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

  // Attempt to set the metrics again. This should not record because of the
  // override to the metrics' default value in the feature configuration.
  const str2 = "another cheesy string v2!";
  Glean.testOnly.cheesyString.set(str2);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));
  const qty2 = 52;
  Glean.testOnly.meaningOfLife.set(qty2);
  Assert.equal(qty1, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Change the feature configuration to re-enable the `cheesy_string` metric.
  feature_config = {
    metrics_enabled: {
      "test_only.cheesy_string": true,
      "test_only.meaning_of_life": false,
    },
  };
  Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

  // Attempt to set the metrics again. This should only record `cheesy_string`
  // because of the most recent feature configuration.
  const str3 = "another cheesy string v3!";
  Glean.testOnly.cheesyString.set(str3);
  Assert.equal(str3, Glean.testOnly.cheesyString.testGetValue("test-ping"));
  const qty3 = 62;
  Glean.testOnly.meaningOfLife.set(qty3);
  Assert.equal(qty1, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Reset everything so it doesn't interfere with other tests.
  Services.fog.testResetFOG();

  // Set some final metrics. This should record in both metrics because they
  // are both `disabled: false` by default.
  const str4 = "another a cheesy string v4";
  Glean.testOnly.cheesyString.set(str4);
  Assert.equal(str4, Glean.testOnly.cheesyString.testGetValue("test-ping"));
  const qty4 = 72;
  Glean.testOnly.meaningOfLife.set(qty4);
  Assert.equal(qty4, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));
});

add_task(function test_fog_metrics_feature_config_api_handles_null_values() {
  // Set a cheesy string in the test metric. This should record because the
  // metric has `disabled: false` by default.
  const str1 = "a cheesy string!";
  Glean.testOnly.cheesyString.set(str1);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Create and set a feature configuration that disables the test metric.
  const feature_config = {
    metrics_enabled: {
      "test_only.cheesy_string": false,
    },
  };
  Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

  // Attempt to set another cheesy string in the test metric. This should not
  // record because of the override to the metric's default value in the
  // feature configuration.
  const str2 = "another cheesy string v2";
  Glean.testOnly.cheesyString.set(str2);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Set the configuration to `null`.
  Services.fog.applyServerKnobsConfig(null);

  // Attempt to set another cheesy string in the test metric. This should now
  // record because `null` doesn't change already existing configuration.
  Glean.testOnly.cheesyString.set(str2);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Set the configuration to `""` to replicate getting an empty string from
  // Nimbus.
  Services.fog.applyServerKnobsConfig("");

  // Attempt to set another cheesy string in the test metric. This should now
  // record again because `""` doesn't change already existing configuration.
  const str3 = "another cheesy string v3";
  Glean.testOnly.cheesyString.set(str3);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));
});

add_task(function test_fog_metrics_disabled_reset_fog_behavior() {
  // Set a cheesy string in the test metric. This should record because the
  // metric has `disabled: false` by default.
  const str1 = "a cheesy string!";
  Glean.testOnly.cheesyString.set(str1);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Create and set a feature configuration that disables the test metric.
  const feature_config = {
    metrics_enabled: {
      "test_only.cheesy_string": false,
    },
  };
  Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

  // Attempt to set another cheesy string in the test metric. This should not
  // record because of the override to the metric's default value in the
  // feature configuration.
  const str2 = "another cheesy string!";
  Glean.testOnly.cheesyString.set(str2);
  Assert.equal(str1, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Now reset FOG to ensure that the feature configuration is also reset.
  Services.fog.testResetFOG();

  // Attempt to set the string again in the test metric. This should now
  // record normally because we reset FOG.
  Glean.testOnly.cheesyString.set(str2);
  Assert.equal(str2, Glean.testOnly.cheesyString.testGetValue("test-ping"));

  // Reset everything so it doesn't interfere with other tests.
  Services.fog.testResetFOG();
});

add_task(function test_fog_metrics_disabled_ping() {
  // This test should work equally for full builds and artifact builds.

  Assert.ok("disabledPing" in GleanPings);

  // This metric's ping is disabled and does not collect data.
  Glean.testOnly.disabledCounter.add(1);
  Assert.equal(undefined, Glean.testOnly.disabledCounter.testGetValue());

  // Create and set a feature configuration that enables the test ping.
  let feature_config = {
    pings_enabled: {
      "disabled-ping": true,
    },
  };
  Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

  Glean.testOnly.disabledCounter.add(2);
  Assert.equal(2, Glean.testOnly.disabledCounter.testGetValue());

  // Will be submitted. We can observe that data is cleared afterwards
  GleanPings.disabledPing.submit();
  Assert.equal(undefined, Glean.testOnly.disabledCounter.testGetValue());

  feature_config = {
    pings_enabled: {
      "disabled-ping": false,
    },
  };
  Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

  Glean.testOnly.disabledCounter.add(3);
  Assert.equal(undefined, Glean.testOnly.disabledCounter.testGetValue());

  // Reset everything so it doesn't interfere with other tests.
  Services.fog.testResetFOG();
});

add_task(
  function test_fog_metrics_collection_disabled_pings_cannot_be_serverknobs_controlled() {
    // This test should work equally for full builds and artifact builds.
    Assert.ok("collectionDisabledPing" in GleanPings);

    // This metric's ping is disabled and does not collect data.
    Glean.testOnly.collectionDisabledCounter.add(1);
    Assert.equal(
      undefined,
      Glean.testOnly.collectionDisabledCounter.testGetValue()
    );

    // Create and set a feature configuration that would enable the test ping.
    // However it uses `collection-enabled=false` and cannot be controlled through server knobs.
    let feature_config = {
      pings_enabled: {
        "collection-disabled-ping": true,
      },
    };
    Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

    Glean.testOnly.collectionDisabledCounter.add(2);
    Assert.equal(
      undefined,
      Glean.testOnly.collectionDisabledCounter.testGetValue()
    );

    GleanPings.collectionDisabledPing.setEnabled(true);
    Glean.testOnly.collectionDisabledCounter.add(3);
    Assert.equal(3, Glean.testOnly.collectionDisabledCounter.testGetValue());

    // Will be submitted. We can observe that data is cleared afterwards.
    GleanPings.collectionDisabledPing.submit();
    Assert.equal(
      undefined,
      Glean.testOnly.collectionDisabledCounter.testGetValue()
    );

    // It uses `collectin-enabled=false` and cannot be controlled through server knobs,
    // not even for turning it off.
    feature_config = {
      pings_enabled: {
        "collection-disabled-ping": false,
      },
    };
    Services.fog.applyServerKnobsConfig(JSON.stringify(feature_config));

    Glean.testOnly.collectionDisabledCounter.add(4);
    Assert.equal(4, Glean.testOnly.collectionDisabledCounter.testGetValue());

    // Reset everything so it doesn't interfere with other tests.
    Services.fog.testResetFOG();
  }
);
