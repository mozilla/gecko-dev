/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that TelemetryController sends close to shutdown don't lead
// to AsyncShutdown timeouts.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const { Policy, TelemetryReportingPolicy } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryReportingPolicy.sys.mjs"
);

// Some tests in this test file can't outside desktop Firefox because of
// features that aren't included in the build.
const skipIfNotBrowser = () => ({
  skip_if: () => AppConstants.MOZ_BUILD_APP != "browser",
});

const TEST_CHANNEL = "TestChannelABC";

const PREF_MINIMUM_CHANNEL_POLICY_VERSION =
  TelemetryUtils.Preferences.MinimumPolicyVersion + ".channel-" + TEST_CHANNEL;

const ON_TRAIN_ROLLOUT_SUPPORTED_PLATFORM =
  AppConstants.platform == "linux" ||
  AppConstants.platform == "macosx" ||
  (AppConstants.platform === "win" &&
    Services.sysinfo.getProperty("hasWinPackageId", false));

const ON_TRAIN_ROLLOUT_ENABLED_PREF =
  "browser.preonboarding.onTrainRolloutEnabled";

const ON_TRAIN_ROLLOUT_POPULATION_PREF =
  "browser.preonboarding.onTrainRolloutPopulation";

const ON_TRAIN_ROLLOUT_ENROLLMENT_PREF =
  "browser.preonboarding.enrolledInOnTrainRollout";

const ON_TRAIN_TEST_RECIPE = {
  slug: "new-onboarding-experience-on-train-rollout-phase-1",
  bucketConfig: {
    count: 100,
    namespace: "firefox-desktop-preonboarding-on-train-rollout-1",
    randomizationUnit: "normandy_id",
    start: 0,
    total: 10000,
  },
  branches: [{ slug: "treatment", ratio: 100 }],
};

function fakeShowPolicyTimeout(set, clear) {
  Policy.setShowInfobarTimeout = set;
  Policy.clearShowInfobarTimeout = clear;
}

function fakeResetAcceptedPolicy() {
  Services.prefs.clearUserPref(TelemetryUtils.Preferences.AcceptedPolicyDate);
  Services.prefs.clearUserPref(
    TelemetryUtils.Preferences.AcceptedPolicyVersion
  );
}

// Fake dismissing a modal dialog.
function fakeInteractWithModal() {
  Services.obs.notifyObservers(
    null,
    "datareporting:notify-data-policy:interacted"
  );
}

function setMinimumPolicyVersion(aNewPolicyVersion) {
  const CHANNEL_NAME = UpdateUtils.getUpdateChannel(false);
  // We might have channel-dependent minimum policy versions.
  const CHANNEL_DEPENDENT_PREF =
    TelemetryUtils.Preferences.MinimumPolicyVersion +
    ".channel-" +
    CHANNEL_NAME;

  // Does the channel-dependent pref exist? If so, set its value.
  if (Services.prefs.getIntPref(CHANNEL_DEPENDENT_PREF, undefined)) {
    Services.prefs.setIntPref(CHANNEL_DEPENDENT_PREF, aNewPolicyVersion);
    return;
  }

  // We don't have a channel specific minimum, so set the common one.
  Services.prefs.setIntPref(
    TelemetryUtils.Preferences.MinimumPolicyVersion,
    aNewPolicyVersion
  );
}

function unsetMinimumPolicyVersion() {
  const CHANNEL_NAME = UpdateUtils.getUpdateChannel(false);
  // We might have channel-dependent minimum policy versions.
  const CHANNEL_DEPENDENT_PREF =
    TelemetryUtils.Preferences.MinimumPolicyVersion +
    ".channel-" +
    CHANNEL_NAME;

  // Does the channel-dependent pref exist? If so, unset it.
  if (Services.prefs.getIntPref(CHANNEL_DEPENDENT_PREF, undefined)) {
    Services.prefs.clearUserPref(CHANNEL_DEPENDENT_PREF);
  }

  // And the common one.
  Services.prefs.clearUserPref(TelemetryUtils.Preferences.MinimumPolicyVersion);
}

add_setup(async function test_setup() {
  // Addon manager needs a profile directory
  do_get_profile(true);
  await loadAddonManager(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "1.9.2"
  );
  finishAddonManagerStartup();
  fakeIntlReady();

  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  // Don't bypass the notifications in this test, we'll fake it.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.BypassNotification,
    false
  );

  TelemetryReportingPolicy.setup();
});

add_setup(skipIfNotBrowser(), async () => {
  // Needed to interact with Nimbus.
  await ExperimentManager.onStartup();
  await ExperimentAPI.ready();
});

add_task(skipIfNotBrowser(), async function test_firstRun() {
  await Services.search.init();

  const FIRST_RUN_TIMEOUT_MSEC = 60 * 1000; // 60s
  const OTHER_RUNS_TIMEOUT_MSEC = 10 * 1000; // 10s

  Services.prefs.clearUserPref(TelemetryUtils.Preferences.FirstRun);

  let promiseTimeout = () =>
    new Promise(resolve => {
      fakeShowPolicyTimeout(
        (_callback, timeout) => resolve(timeout),
        () => {}
      );
    });
  let p, startupTimeout;

  TelemetryReportingPolicy.reset();
  p = promiseTimeout();
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  startupTimeout = await p;
  Assert.equal(
    startupTimeout,
    FIRST_RUN_TIMEOUT_MSEC,
    "The infobar display timeout should be 60s on the first run."
  );

  // Run again, and check that we actually wait only 10 seconds.
  TelemetryReportingPolicy.reset();
  p = promiseTimeout();
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  startupTimeout = await p;
  Assert.equal(
    startupTimeout,
    OTHER_RUNS_TIMEOUT_MSEC,
    "The infobar display timeout should be 10s on other runs."
  );
});

add_task(async function test_prefs() {
  TelemetryReportingPolicy.reset();

  let now = fakeNow(2009, 11, 18);

  // If the date is not valid (earlier than 2012), we don't regard the policy as accepted.
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified());
  Assert.equal(
    Services.prefs.getStringPref(
      TelemetryUtils.Preferences.AcceptedPolicyDate,
      null
    ),
    0,
    "Invalid dates should not make the policy accepted."
  );

  // Check that the notification date and version are correctly saved to the prefs.
  now = fakeNow(2012, 11, 18);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.equal(
    Services.prefs.getStringPref(
      TelemetryUtils.Preferences.AcceptedPolicyDate,
      null
    ),
    now.getTime(),
    "A valid date must correctly be saved."
  );

  // Now that user is notified, check if we are allowed to upload.
  Assert.ok(
    TelemetryReportingPolicy.canUpload(),
    "We must be able to upload after the policy is accepted."
  );

  // Disable submission and check that we're no longer allowed to upload.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.DataSubmissionEnabled,
    false
  );
  Assert.ok(
    !TelemetryReportingPolicy.canUpload(),
    "We must not be able to upload if data submission is disabled."
  );

  // Turn the submission back on.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.DataSubmissionEnabled,
    true
  );
  Assert.ok(
    TelemetryReportingPolicy.canUpload(),
    "We must be able to upload if data submission is enabled and the policy was accepted."
  );

  // Set a new minimum policy version and check that user is no longer notified.
  let newMinimum =
    Services.prefs.getIntPref(
      TelemetryUtils.Preferences.CurrentPolicyVersion,
      1
    ) + 1;
  setMinimumPolicyVersion(newMinimum);
  Assert.ok(
    !TelemetryReportingPolicy.testIsUserNotified(),
    "A greater minimum policy version must invalidate the policy and disable upload."
  );

  // Eventually accept the policy and make sure user is notified.
  Services.prefs.setIntPref(
    TelemetryUtils.Preferences.CurrentPolicyVersion,
    newMinimum
  );
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(
    TelemetryReportingPolicy.testIsUserNotified(),
    "Accepting the policy again should show the user as notified."
  );
  Assert.ok(
    TelemetryReportingPolicy.canUpload(),
    "Accepting the policy again should let us upload data."
  );

  // macOS has the app.update.channel pref locked. Check if it needs to be
  // unlocked before proceeding with the test.
  if (Services.prefs.getDefaultBranch("").prefIsLocked("app.update.channel")) {
    Services.prefs.getDefaultBranch("").unlockPref("app.update.channel");
  }

  // Set a new, per channel, minimum policy version. Start by setting a test current channel.
  Services.prefs
    .getDefaultBranch("")
    .setStringPref("app.update.channel", TEST_CHANNEL);

  // Increase and set the new minimum version, then check that we're not notified anymore.
  newMinimum++;
  Services.prefs.setIntPref(PREF_MINIMUM_CHANNEL_POLICY_VERSION, newMinimum);
  Assert.ok(
    !TelemetryReportingPolicy.testIsUserNotified(),
    "Increasing the minimum policy version should invalidate the policy."
  );

  // Eventually accept the policy and make sure user is notified.
  Services.prefs.setIntPref(
    TelemetryUtils.Preferences.CurrentPolicyVersion,
    newMinimum
  );
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(
    TelemetryReportingPolicy.testIsUserNotified(),
    "Accepting the policy again should show the user as notified."
  );
  Assert.ok(
    TelemetryReportingPolicy.canUpload(),
    "Accepting the policy again should let us upload data."
  );
});

add_task(async function test_migratePrefs() {
  const DEPRECATED_FHR_PREFS = {
    "datareporting.policy.dataSubmissionPolicyAccepted": true,
    "datareporting.policy.dataSubmissionPolicyBypassAcceptance": true,
    "datareporting.policy.dataSubmissionPolicyResponseType": "foxyeah",
    "datareporting.policy.dataSubmissionPolicyResponseTime":
      Date.now().toString(),
  };

  // Make sure the preferences are set before setting up the policy.
  for (let name in DEPRECATED_FHR_PREFS) {
    switch (typeof DEPRECATED_FHR_PREFS[name]) {
      case "string":
        Services.prefs.setStringPref(name, DEPRECATED_FHR_PREFS[name]);
        break;
      case "number":
        Services.prefs.setIntPref(name, DEPRECATED_FHR_PREFS[name]);
        break;
      case "boolean":
        Services.prefs.setBoolPref(name, DEPRECATED_FHR_PREFS[name]);
        break;
    }
  }
  // Set up the policy.
  TelemetryReportingPolicy.reset();
  // They should have been removed by now.
  for (let name in DEPRECATED_FHR_PREFS) {
    Assert.ok(
      !Services.prefs.prefHasUserValue(name),
      name + " should have been removed."
    );
  }
});

add_task(async function test_userNotifiedOfCurrentPolicy() {
  fakeResetAcceptedPolicy();
  TelemetryReportingPolicy.reset();

  // User should be reported as not notified by default.
  Assert.ok(
    !TelemetryReportingPolicy.testIsUserNotified(),
    "The initial state should be unnotified."
  );

  // Forcing a policy version should not automatically make the user notified.
  Services.prefs.setIntPref(
    TelemetryUtils.Preferences.AcceptedPolicyVersion,
    TelemetryReportingPolicy.DEFAULT_DATAREPORTING_POLICY_VERSION
  );
  Assert.ok(
    !TelemetryReportingPolicy.testIsUserNotified(),
    "The default state of the date should have a time of 0 and it should therefore fail"
  );

  // Showing the notification bar should make the user notified.
  fakeNow(2012, 11, 11);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(
    TelemetryReportingPolicy.testIsUserNotified(),
    "Using the proper API causes user notification to report as true."
  );

  // It is assumed that later versions of the policy will incorporate previous
  // ones, therefore this should also return true.
  let newVersion =
    Services.prefs.getIntPref(
      TelemetryUtils.Preferences.CurrentPolicyVersion,
      1
    ) + 1;
  Services.prefs.setIntPref(
    TelemetryUtils.Preferences.AcceptedPolicyVersion,
    newVersion
  );
  Assert.ok(
    TelemetryReportingPolicy.testIsUserNotified(),
    "A future version of the policy should pass."
  );

  newVersion =
    Services.prefs.getIntPref(
      TelemetryUtils.Preferences.CurrentPolicyVersion,
      1
    ) - 1;
  Services.prefs.setIntPref(
    TelemetryUtils.Preferences.AcceptedPolicyVersion,
    newVersion
  );
  Assert.ok(
    !TelemetryReportingPolicy.testIsUserNotified(),
    "A previous version of the policy should fail."
  );
});

add_task(async function test_canSend() {
  const TEST_PING_TYPE = "test-ping";

  PingServer.start();
  Services.prefs.setStringPref(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );

  await TelemetryController.testReset();
  TelemetryReportingPolicy.reset();

  // User should be reported as not notified by default.
  Assert.ok(
    !TelemetryReportingPolicy.testIsUserNotified(),
    "The initial state should be unnotified."
  );

  // Assert if we receive any ping before the policy is accepted.
  PingServer.registerPingHandler(() =>
    Assert.ok(false, "Should not have received any pings now")
  );
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Reset the ping handler.
  PingServer.resetPingHandler();

  // Fake the infobar: this should also trigger the ping send task.
  TelemetryReportingPolicy.testInfobarShown();
  let ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(
    ping[0].type,
    TEST_PING_TYPE,
    "We should have received the previous ping."
  );

  // Submit another ping, to make sure it gets sent.
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Get the ping and check its type.
  ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(
    ping[0].type,
    TEST_PING_TYPE,
    "We should have received the new ping."
  );

  // Fake a restart with a pending ping.
  await TelemetryController.addPendingPing(TEST_PING_TYPE, {});
  await TelemetryController.testReset();

  // We should be immediately sending the ping out.
  ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(
    ping[0].type,
    TEST_PING_TYPE,
    "We should have received the pending ping."
  );

  // Submit another ping, to make sure it gets sent.
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Get the ping and check its type.
  ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(
    ping[0].type,
    TEST_PING_TYPE,
    "We should have received the new ping."
  );

  await PingServer.stop();
});

add_task(skipIfNotBrowser(), async function test_feature_prefs() {
  // Verify that feature values impact Gecko preferences at
  // `sessionstore-windows-restored` time, but not afterward.
  function assertPrefs(
    currentPolicyVersion,
    minimumPolicyVersion,
    firstRunURL
  ) {
    Assert.equal(
      Services.prefs.getIntPref(
        TelemetryUtils.Preferences.CurrentPolicyVersion
      ),
      currentPolicyVersion,
      "datareporting.policy.currentPolicyVersion is set"
    );

    Assert.equal(
      Services.prefs.getIntPref(
        TelemetryUtils.Preferences.MinimumPolicyVersion
      ),
      minimumPolicyVersion,
      "datareporting.policy.minimumPolicyVersion is set"
    );

    Assert.equal(
      Services.prefs.getCharPref(TelemetryUtils.Preferences.FirstRunURL),
      firstRunURL,
      "datareporting.policy.firstRunURL is set"
    );
  }

  unsetMinimumPolicyVersion();
  Services.prefs.clearUserPref(TelemetryUtils.Preferences.CurrentPolicyVersion);

  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.preonboarding.featureId,
      value: {
        enabled: true,
        currentPolicyVersion: 900,
        minimumPolicyVersion: 899,
        firstRunURL: "http://mochi.test/v900",
      },
    },
    { isRollout: false }
  );

  Assert.ok(NimbusFeatures.preonboarding.getVariable("enabled"));

  // Before `sessionstore-windows-restored`, nothing is configured.
  TelemetryReportingPolicy.reset();

  Assert.ok(
    !Services.prefs.prefHasUserValue(
      TelemetryUtils.Preferences.CurrentPolicyVersion
    ),
    "datareporting.policy.currentPolicyVersion is not set"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue(
      TelemetryUtils.Preferences.MinimumPolicyVersion
    ),
    "datareporting.policy.minimumPolicyVersion is not set"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue(TelemetryUtils.Preferences.FirstRunURL),
    "datareporting.policy.firstRunURL is not set"
  );

  // After `sessionstore-windows-restored`, values are adopted.
  await Policy.fakeSessionRestoreNotification();
  assertPrefs(900, 899, "http://mochi.test/v900");

  // Unenroll.  Values remain, for consistency while Firefox is running.
  doCleanup();
  Assert.ok(!NimbusFeatures.preonboarding.getVariable("enabled"));
  assertPrefs(900, 899, "http://mochi.test/v900");

  // Updating the Nimbus feature does nothing (without `sessionstore-windows-restored`).
  doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.preonboarding.featureId,
      value: {
        enabled: true,
        currentPolicyVersion: 901,
        minimumPolicyVersion: 900,
        firstRunURL: "http://mochi.test/v901",
      },
    },
    { isRollout: false }
  );
  Assert.ok(NimbusFeatures.preonboarding.getVariable("enabled"));
  assertPrefs(900, 899, "http://mochi.test/v900");
  doCleanup();
});

async function doOneModalFlow(version) {
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.preonboarding.featureId,
      value: {
        enabled: true,
        currentPolicyVersion: version,
        minimumPolicyVersion: version,
        firstRunURL: `http://mochi.test/v${version}`,
        // Needed to opt into the modal flow, but not actually used in this test.
        screens: [{ id: "test" }],
      },
    },
    { isRollout: false }
  );

  let displayStub = sinon.stub(Policy, "showModal").returns(true);

  // This will notify the user via a modal.
  TelemetryReportingPolicy.reset();
  await Policy.fakeSessionRestoreNotification();

  Assert.equal(displayStub.callCount, 1, "showModal is invoked");

  Assert.equal(
    TelemetryReportingPolicy.testIsUserNotified(),
    false,
    "Before interaction, the user should be reported as not notified"
  );

  let completed = false;
  let p = TelemetryReportingPolicy.ensureUserIsNotified().then(
    () => (completed = true)
  );

  Assert.equal(
    completed,
    false,
    "The notification promise should not resolve before the user interacts"
  );

  fakeInteractWithModal();

  await p;

  Assert.equal(
    completed,
    true,
    "The notification promise should resolve after user interacts"
  );

  Assert.equal(
    TelemetryReportingPolicy.testIsUserNotified(),
    true,
    "After interaction, the state should be notified."
  );

  doCleanup();

  sinon.restore();
}

add_task(
  skipIfNotBrowser(),
  async function test_modal_flow_before_notification() {
    // Test the `--first-startup` flow.  Suppose the user has not been notified.
    // Verify that when the Nimbus feature is configured, the modal branch is
    // taken, that the ensure promise waits, and that the observer notification
    // resolves the ensure promise.

    fakeResetAcceptedPolicy();
    Services.prefs.clearUserPref(TelemetryUtils.Preferences.FirstRun);

    await doOneModalFlow(900);

    // The user accepted the version from the experiment/rollout.
    Assert.equal(
      Services.prefs.getIntPref(
        TelemetryUtils.Preferences.AcceptedPolicyVersion
      ),
      900
    );
  }
);

add_task(
  skipIfNotBrowser(),
  async function test_modal_flow_after_notification() {
    // Test the existing user flow.  Suppose the user **has** been notified, but
    // is then enrolled into an experiment which configures the Nimbus feature.
    // Verify that the modal branch is taken, that the ensure promise waits, and
    // that the observer notification resolves the ensure promise.

    unsetMinimumPolicyVersion();
    Services.prefs.clearUserPref(
      TelemetryUtils.Preferences.CurrentPolicyVersion
    );

    fakeResetAcceptedPolicy();
    Services.prefs.setBoolPref(TelemetryUtils.Preferences.FirstRun, false);

    TelemetryReportingPolicy.reset();

    // Showing the notification bar should make the user notified.
    fakeNow(2012, 11, 11);
    TelemetryReportingPolicy.testInfobarShown();
    Assert.ok(
      TelemetryReportingPolicy.testIsUserNotified(),
      "User is notified after seeing the legacy infobar"
    );

    Assert.ok(
      Services.prefs.getIntPref(
        TelemetryUtils.Preferences.AcceptedPolicyVersion
      ) < 900,
      "Before, the user has not accepted experiment/rollout version"
    );

    // This resets, witnesses `sessionstore-windows-restored`, and fakes the modal flow.
    await doOneModalFlow(900);

    Assert.ok(
      TelemetryReportingPolicy.testIsUserNotified(),
      "User is notified after seeing the experiment modal"
    );

    Assert.equal(
      Services.prefs.getIntPref(
        TelemetryUtils.Preferences.AcceptedPolicyVersion
      ),
      900,
      "After, the user has accepted the experiment/rollout version."
    );
  }
);

const getOnTrainRolloutModalStub = async ({
  shouldEnroll,
  isFirstRun,
  isEnrolled,
}) => {
  Services.prefs.setBoolPref(ON_TRAIN_ROLLOUT_ENABLED_PREF, true);
  Services.prefs.setIntPref(
    ON_TRAIN_ROLLOUT_POPULATION_PREF,
    ON_TRAIN_TEST_RECIPE.bucketConfig.count
  );
  Services.prefs.setBoolPref(ON_TRAIN_ROLLOUT_ENROLLMENT_PREF, isEnrolled);
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FirstRun, isFirstRun);

  const testIDs = await ExperimentManager.generateTestIds(ON_TRAIN_TEST_RECIPE);
  let experimentId = shouldEnroll ? testIDs.treatment : testIDs.notInExperiment;
  sinon.stub(ClientEnvironment, "userId").get(() => experimentId);
  let modalStub = sinon.stub(Policy, "showModal").returns(true);

  fakeResetAcceptedPolicy();
  TelemetryReportingPolicy.reset();
  let p = Policy.delayedSetup();
  Policy.fakeSessionRestoreNotification();
  fakeInteractWithModal();
  await p;

  const doCleanup = () => {
    sinon.restore();
    fakeResetAcceptedPolicy();
    Services.prefs.clearUserPref(ON_TRAIN_ROLLOUT_ENABLED_PREF);
    Services.prefs.clearUserPref(ON_TRAIN_ROLLOUT_POPULATION_PREF);
    Services.prefs.clearUserPref(ON_TRAIN_ROLLOUT_ENROLLMENT_PREF);
  };

  return { modalStub, doCleanup };
};

add_task(
  skipIfNotBrowser(),
  async function test_onTrainRollout_configuration_supportedOS_should_enroll() {
    if (!ON_TRAIN_ROLLOUT_SUPPORTED_PLATFORM) {
      info(
        "Skipping supported OS test because current platform is not Linux, Mac, or Win MSIX"
      );
      return;
    }

    const { modalStub, doCleanup } = await getOnTrainRolloutModalStub({
      shouldEnroll: true,
      isFirstRun: true,
      isEnrolled: false,
    });

    Assert.equal(
      modalStub.callCount,
      1,
      "showModal is invoked once if enrolled in rollout"
    );

    doCleanup();
  }
);

add_task(
  skipIfNotBrowser(),
  async function test_onTrainRollout_configuration_supportedOS_should_not_enroll() {
    if (!ON_TRAIN_ROLLOUT_SUPPORTED_PLATFORM) {
      info(
        "Skipping supported OS test because current platform is not Linux, Mac, or Win MSIX"
      );
      return;
    }

    const { modalStub, doCleanup } = await getOnTrainRolloutModalStub({
      shouldEnroll: false,
      isFirstRun: true,
      isEnrolled: false,
    });

    Assert.equal(
      modalStub.callCount,
      0,
      "showModal is not invoked if not enrolled in rollout"
    );

    doCleanup();
  }
);

add_task(
  skipIfNotBrowser(),
  async function test_onTrainRollout_configuration_unsupportedOS() {
    if (ON_TRAIN_ROLLOUT_SUPPORTED_PLATFORM) {
      info(
        "Skipping unsupported OS test because current platform is supported"
      );
      return;
    }

    const { modalStub, doCleanup } = await getOnTrainRolloutModalStub({
      shouldEnroll: true,
      isFirstRun: true,
      isEnrolled: false,
    });

    Assert.equal(
      modalStub.callCount,
      0,
      "showModal is not invoked on unsupported OS even if on-train rollouts are enabled and user would otherwise be enrolled"
    );

    doCleanup();
  }
);

add_task(
  skipIfNotBrowser(),
  async function test_onTrainRollout_subsequent_startup_after_enrolled() {
    if (!ON_TRAIN_ROLLOUT_SUPPORTED_PLATFORM) {
      info(
        "Skipping supported OS test because current platform is not Linux, Mac, or Win MSIX"
      );
      return;
    }

    const { modalStub, doCleanup } = await getOnTrainRolloutModalStub({
      shouldEnroll: true,
      isFirstRun: false,
      isEnrolled: true,
    });

    Assert.equal(
      modalStub.callCount,
      1,
      "showModal is invoked on subsequent startup if user was enrolled on first startup but did not interact with modal"
    );

    doCleanup();
  }
);

add_task(
  skipIfNotBrowser(),
  async function test_onTrainRollout_subsequent_startup_not_enrolled() {
    if (!ON_TRAIN_ROLLOUT_SUPPORTED_PLATFORM) {
      info(
        "Skipping supported OS test because current platform is not Linux, Mac, or Win MSIX"
      );
      return;
    }

    const { modalStub, doCleanup } = await getOnTrainRolloutModalStub({
      shouldEnroll: true,
      isFirstRun: false,
      isEnrolled: false,
    });

    Assert.equal(
      modalStub.callCount,
      0,
      "showModal is not invoked on subsequent startup if user was not enrolled on first startup"
    );

    doCleanup();
  }
);
