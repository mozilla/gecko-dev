"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);
ChromeUtils.import("resource://normandy/lib/CleanupManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);

// Save ourselves some typing
const {withMockExperiments} = PreferenceExperiments;
const DefaultPreferences = new Preferences({defaultBranch: true});
const startupPrefs = "app.normandy.startupExperimentPrefs";

function experimentFactory(attrs) {
  return Object.assign({
    name: "fakename",
    branch: "fakebranch",
    expired: false,
    lastSeen: new Date().toJSON(),
    preferenceName: "fake.preference",
    preferenceValue: "fakevalue",
    preferenceType: "string",
    previousPreferenceValue: "oldfakevalue",
    preferenceBranchType: "default",
    experimentType: "exp",
  }, attrs);
}

// clearAllExperimentStorage
decorate_task(
  withMockExperiments([experimentFactory({ name: "test" })]),
  async function(experiments) {
    ok(await PreferenceExperiments.has("test"), "Mock experiment is detected.");
    await PreferenceExperiments.clearAllExperimentStorage();
    ok(
      !(await PreferenceExperiments.has("test")),
      "clearAllExperimentStorage removed all stored experiments",
    );
  }
);

// start should throw if an experiment with the given name already exists
decorate_task(
  withMockExperiments([experimentFactory({ name: "test" })]),
  withSendEventStub,
  async function(experiments, sendEventStub) {
    await Assert.rejects(
      PreferenceExperiments.start({
        name: "test",
        branch: "branch",
        preferenceName: "fake.preference",
        preferenceValue: "value",
        preferenceType: "string",
        preferenceBranchType: "default",
      }),
      /test.*already exists/,
      "start threw an error due to a conflicting experiment name",
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["enrollFailed", "preference_study", "test", {reason: "name-conflict"}]],
      "event should be sent for failure",
    );
  }
);

// start should throw if an experiment for the given preference is active
decorate_task(
  withMockExperiments([experimentFactory({ name: "test", preferenceName: "fake.preference" })]),
  withSendEventStub,
  async function(experiments, sendEventStub) {
    await Assert.rejects(
      PreferenceExperiments.start({
        name: "different",
        branch: "branch",
        preferenceName: "fake.preference",
        preferenceValue: "value",
        preferenceType: "string",
        preferenceBranchType: "default",
      }),
      /another.*is currently active/i,
      "start threw an error due to an active experiment for the given preference",
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["enrollFailed", "preference_study", "different", {reason: "pref-conflict"}]],
      "event should be sent for failure",
    );
  }
);

// start should throw if an invalid preferenceBranchType is given
decorate_task(
  withMockExperiments(),
  withSendEventStub,
  async function(experiments, sendEventStub) {
    await Assert.rejects(
      PreferenceExperiments.start({
        name: "test",
        branch: "branch",
        preferenceName: "fake.preference",
        preferenceValue: "value",
        preferenceType: "string",
        preferenceBranchType: "invalid",
      }),
      /invalid value for preferenceBranchType: invalid/i,
      "start threw an error due to an invalid preference branch type",
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["enrollFailed", "preference_study", "test", {reason: "invalid-branch"}]],
      "event should be sent for failure",
    );
  }
);

// start should save experiment data, modify the preference, and register a
// watcher.
decorate_task(
  withMockExperiments(),
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  withSendEventStub,
  async function testStart(experiments, mockPreferences, startObserverStub, sendEventStub) {
    mockPreferences.set("fake.preference", "oldvalue", "default");
    mockPreferences.set("fake.preference", "uservalue", "user");

    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "newvalue",
      preferenceBranchType: "default",
      preferenceType: "string",
    });
    ok(await PreferenceExperiments.get("test"), "start saved the experiment");
    ok(
      startObserverStub.calledWith("test", "fake.preference", "string", "newvalue"),
      "start registered an observer",
    );

    const expectedExperiment = {
      name: "test",
      branch: "branch",
      expired: false,
      preferenceName: "fake.preference",
      preferenceValue: "newvalue",
      preferenceType: "string",
      previousPreferenceValue: "oldvalue",
      preferenceBranchType: "default",
    };
    const experiment = {};
    const actualExperiment = await PreferenceExperiments.get("test");
    Object.keys(expectedExperiment).forEach(key => experiment[key] = actualExperiment[key]);
    Assert.deepEqual(experiment, expectedExperiment, "start saved the experiment");

    is(
      DefaultPreferences.get("fake.preference"),
      "newvalue",
      "start modified the default preference",
    );
    is(
      Preferences.get("fake.preference"),
      "uservalue",
      "start did not modify the user preference",
    );
    is(
      Preferences.get(`${startupPrefs}.fake.preference`),
      "newvalue",
      "start saved the experiment value to the startup prefs tree",
    );
  },
);

// start should modify the user preference for the user branch type
decorate_task(
  withMockExperiments(),
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  async function(experiments, mockPreferences, startObserver) {
    mockPreferences.set("fake.preference", "olddefaultvalue", "default");
    mockPreferences.set("fake.preference", "oldvalue", "user");

    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "newvalue",
      preferenceType: "string",
      preferenceBranchType: "user",
    });
    ok(
      startObserver.calledWith("test", "fake.preference", "string", "newvalue"),
      "start registered an observer",
    );

    const expectedExperiment = {
      name: "test",
      branch: "branch",
      expired: false,
      preferenceName: "fake.preference",
      preferenceValue: "newvalue",
      preferenceType: "string",
      previousPreferenceValue: "oldvalue",
      preferenceBranchType: "user",
    };

    const experiment = {};
    const actualExperiment = await PreferenceExperiments.get("test");
    Object.keys(expectedExperiment).forEach(key => experiment[key] = actualExperiment[key]);
    Assert.deepEqual(experiment, expectedExperiment, "start saved the experiment");

    Assert.notEqual(
      DefaultPreferences.get("fake.preference"),
      "newvalue",
      "start did not modify the default preference",
    );
    is(Preferences.get("fake.preference"), "newvalue", "start modified the user preference");
  }
);

// start should detect if a new preference value type matches the previous value type
decorate_task(
  withMockPreferences,
  withSendEventStub,
  async function(mockPreferences, sendEventStub) {
    mockPreferences.set("fake.type_preference", "oldvalue");

    await Assert.rejects(
      PreferenceExperiments.start({
        name: "test",
        branch: "branch",
        preferenceName: "fake.type_preference",
        preferenceBranchType: "user",
        preferenceValue: 12345,
        preferenceType: "integer",
      }),
      /previous preference value is of type/i,
      "start threw error for incompatible preference type"
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["enrollFailed", "preference_study", "test", {reason: "invalid-type"}]],
      "event should be sent for failure",
    );
  }
);

// startObserver should throw if an observer for the experiment is already
// active.
decorate_task(
  withMockExperiments(),
  async function() {
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "newvalue");
    Assert.throws(
      () => PreferenceExperiments.startObserver("test", "another.fake", "string", "othervalue"),
      /observer.*is already active/i,
      "startObserver threw due to a conflicting active observer",
    );
    PreferenceExperiments.stopAllObservers();
  }
);

// startObserver should register an observer that calls stop when a preference
// changes from its experimental value.
decorate_task(
  withMockExperiments(),
  withMockPreferences,
  async function(mockExperiments, mockPreferences) {
    const tests = [
      ["string", "startvalue", "experimentvalue", "newvalue"],
      ["boolean", false, true, false],
      ["integer", 1, 2, 42],
    ];

    for (const [type, startvalue, experimentvalue, newvalue] of tests) {
      const stop = sinon.stub(PreferenceExperiments, "stop");
      mockPreferences.set("fake.preference" + type, startvalue);

      // NOTE: startObserver does not modify the pref
      PreferenceExperiments.startObserver("test" + type, "fake.preference" + type, type, experimentvalue);

      // Setting it to the experimental value should not trigger the call.
      mockPreferences.set("fake.preference" + type, experimentvalue);
      ok(!stop.called, "Changing to the experimental pref value did not trigger the observer");

      // Setting it to something different should trigger the call.
      mockPreferences.set("fake.preference" + type, newvalue);
      ok(stop.called, "Changing to a different value triggered the observer");

      PreferenceExperiments.stopAllObservers();
      stop.restore();
    }
  }
);

decorate_task(
  withMockExperiments(),
  async function testHasObserver() {
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentValue");

    ok(await PreferenceExperiments.hasObserver("test"), "hasObserver should detect active observers");
    ok(
      !(await PreferenceExperiments.hasObserver("missing")),
      "hasObserver shouldn't detect inactive observers",
    );

    PreferenceExperiments.stopAllObservers();
  }
);

// stopObserver should throw if there is no observer active for it to stop.
decorate_task(
  withMockExperiments(),
  async function() {
    Assert.throws(
      () => PreferenceExperiments.stopObserver("neveractive", "another.fake", "othervalue"),
      /no observer.*found/i,
      "stopObserver threw because there was not matching active observer",
    );
  }
);

// stopObserver should cancel an active observer.
decorate_task(
  withMockExperiments(),
  withMockPreferences,
  async function(mockExperiments, mockPreferences) {
    const stop = sinon.stub(PreferenceExperiments, "stop");
    mockPreferences.set("fake.preference", "startvalue");

    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
    PreferenceExperiments.stopObserver("test");

    // Setting the preference now that the observer is stopped should not call
    // stop.
    mockPreferences.set("fake.preference", "newvalue");
    ok(!stop.called, "stopObserver successfully removed the observer");

    // Now that the observer is stopped, start should be able to start a new one
    // without throwing.
    try {
      PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
    } catch (err) {
      ok(false, "startObserver did not throw an error for an observer that was already stopped");
    }

    PreferenceExperiments.stopAllObservers();
    stop.restore();
  }
);

// stopAllObservers
decorate_task(
  withMockExperiments(),
  withMockPreferences,
  async function(mockExperiments, mockPreferences) {
    const stop = sinon.stub(PreferenceExperiments, "stop");
    mockPreferences.set("fake.preference", "startvalue");
    mockPreferences.set("other.fake.preference", "startvalue");

    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
    PreferenceExperiments.startObserver("test2", "other.fake.preference", "string", "experimentvalue");
    PreferenceExperiments.stopAllObservers();

    // Setting the preference now that the observers are stopped should not call
    // stop.
    mockPreferences.set("fake.preference", "newvalue");
    mockPreferences.set("other.fake.preference", "newvalue");
    ok(!stop.called, "stopAllObservers successfully removed all observers");

    // Now that the observers are stopped, start should be able to start new
    // observers without throwing.
    try {
      PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
      PreferenceExperiments.startObserver("test2", "other.fake.preference", "string", "experimentvalue");
    } catch (err) {
      ok(false, "startObserver did not throw an error for an observer that was already stopped");
    }

    PreferenceExperiments.stopAllObservers();
    stop.restore();
  }
);

// markLastSeen should throw if it can't find a matching experiment
decorate_task(
  withMockExperiments(),
  async function() {
    await Assert.rejects(
      PreferenceExperiments.markLastSeen("neveractive"),
      /could not find/i,
      "markLastSeen threw because there was not a matching experiment",
    );
  }
);

// markLastSeen should update the lastSeen date
const oldDate = new Date(1988, 10, 1).toJSON();
decorate_task(
  withMockExperiments([experimentFactory({ name: "test", lastSeen: oldDate })]),
  async function([experiment]) {
    await PreferenceExperiments.markLastSeen("test");
    Assert.notEqual(
      experiment.lastSeen,
      oldDate,
      "markLastSeen updated the experiment lastSeen date",
    );
  }
);

// stop should throw if an experiment with the given name doesn't exist
decorate_task(
  withMockExperiments(),
  withSendEventStub,
  async function(experiments, sendEventStub) {
    await Assert.rejects(
      PreferenceExperiments.stop("test"),
      /could not find/i,
      "stop threw an error because there are no experiments with the given name",
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["unenrollFailed", "preference_study", "test", {reason: "does-not-exist"}]],
      "event should be sent for failure",
    );
  }
);

// stop should throw if the experiment is already expired
decorate_task(
  withMockExperiments([experimentFactory({ name: "test", expired: true })]),
  withSendEventStub,
  async function(experiments, sendEventStub) {
    await Assert.rejects(
      PreferenceExperiments.stop("test"),
      /already expired/,
      "stop threw an error because the experiment was already expired",
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["unenrollFailed", "preference_study", "test", {reason: "already-unenrolled"}]],
      "event should be sent for failure",
    );
  }
);

// stop should mark the experiment as expired, stop its observer, and revert the
// preference value.
decorate_task(
  withMockExperiments([
    experimentFactory({
      name: "test",
      expired: false,
      branch: "fakebranch",
      preferenceName: "fake.preference",
      preferenceValue: "experimentvalue",
      preferenceType: "string",
      previousPreferenceValue: "oldvalue",
      preferenceBranchType: "default",
    }),
  ]),
  withMockPreferences,
  withSpy(PreferenceExperiments, "stopObserver"),
  withSendEventStub,
  async function testStop(experiments, mockPreferences, stopObserverSpy, sendEventStub) {
    // this assertion is mostly useful for --verify test runs, to make
    // sure that tests clean up correctly.
    is(Preferences.get("fake.preference"), null, "preference should start unset");

    mockPreferences.set(`${startupPrefs}.fake.preference`, "experimentvalue", "user");
    mockPreferences.set("fake.preference", "experimentvalue", "default");
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");

    await PreferenceExperiments.stop("test", {reason: "test-reason"});
    ok(stopObserverSpy.calledWith("test"), "stop removed an observer");
    const experiment = await PreferenceExperiments.get("test");
    is(experiment.expired, true, "stop marked the experiment as expired");
    is(
      DefaultPreferences.get("fake.preference"),
      "oldvalue",
      "stop reverted the preference to its previous value",
    );
    ok(
      !Services.prefs.prefHasUserValue(`${startupPrefs}.fake.preference`),
      "stop cleared the startup preference for fake.preference.",
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["unenroll", "preference_study", "test", {
        didResetValue: "true",
        reason: "test-reason",
        branch: "fakebranch",
      }]],
      "stop should send the correct telemetry event"
    );

    PreferenceExperiments.stopAllObservers();
  },
);

// stop should also support user pref experiments
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    expired: false,
    preferenceName: "fake.preference",
    preferenceValue: "experimentvalue",
    preferenceType: "string",
    previousPreferenceValue: "oldvalue",
    preferenceBranchType: "user",
  })]),
  withMockPreferences,
  withStub(PreferenceExperiments, "stopObserver"),
  withStub(PreferenceExperiments, "hasObserver"),
  async function testStopUserPrefs(experiments, mockPreferences, stopObserver, hasObserver) {
    hasObserver.returns(true);

    mockPreferences.set("fake.preference", "experimentvalue", "user");
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");

    await PreferenceExperiments.stop("test");
    ok(stopObserver.calledWith("test"), "stop removed an observer");
    const experiment = await PreferenceExperiments.get("test");
    is(experiment.expired, true, "stop marked the experiment as expired");
    is(
      Preferences.get("fake.preference"),
      "oldvalue",
      "stop reverted the preference to its previous value",
    );
    stopObserver.restore();
    PreferenceExperiments.stopAllObservers();
  }
);

// stop should remove a preference that had no value prior to an experiment for user prefs
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    expired: false,
    preferenceName: "fake.preference",
    preferenceValue: "experimentvalue",
    preferenceType: "string",
    previousPreferenceValue: null,
    preferenceBranchType: "user",
  })]),
  withMockPreferences,
  async function(experiments, mockPreferences) {
    const stopObserver = sinon.stub(PreferenceExperiments, "stopObserver");
    mockPreferences.set("fake.preference", "experimentvalue", "user");

    await PreferenceExperiments.stop("test");
    ok(
      !Preferences.isSet("fake.preference"),
      "stop removed the preference that had no value prior to the experiment",
    );

    stopObserver.restore();
  }
);

// stop should not modify a preference if resetValue is false
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    expired: false,
    branch: "fakebranch",
    preferenceName: "fake.preference",
    preferenceValue: "experimentvalue",
    preferenceType: "string",
    previousPreferenceValue: "oldvalue",
    preferenceBranchType: "default",
  })]),
  withMockPreferences,
  withStub(PreferenceExperiments, "stopObserver"),
  withSendEventStub,
  async function testStopReset(experiments, mockPreferences, stopObserverStub, sendEventStub) {
    mockPreferences.set("fake.preference", "customvalue", "default");

    await PreferenceExperiments.stop("test", {reason: "test-reason", resetValue: false});
    is(
      DefaultPreferences.get("fake.preference"),
      "customvalue",
      "stop did not modify the preference",
    );
    Assert.deepEqual(
      sendEventStub.args,
      [["unenroll", "preference_study", "test", {
        didResetValue: "false",
        reason: "test-reason",
        branch: "fakebranch",
      }]],
      "stop should send the correct telemetry event"
    );
  }
);

// get should throw if no experiment exists with the given name
decorate_task(
  withMockExperiments(),
  async function() {
    await Assert.rejects(
      PreferenceExperiments.get("neverexisted"),
      /could not find/i,
      "get rejects if no experiment with the given name is found",
    );
  }
);

// get
decorate_task(
  withMockExperiments([experimentFactory({ name: "test" })]),
  async function(experiments) {
    const experiment = await PreferenceExperiments.get("test");
    is(experiment.name, "test", "get fetches the correct experiment");

    // Modifying the fetched experiment must not edit the data source.
    experiment.name = "othername";
    const refetched = await PreferenceExperiments.get("test");
    is(refetched.name, "test", "get returns a copy of the experiment");
  }
);

// get all
decorate_task(
  withMockExperiments([
    experimentFactory({ name: "experiment1", disabled: false }),
    experimentFactory({ name: "experiment2", disabled: true }),
  ]),
  async function testGetAll([experiment1, experiment2]) {
    const fetchedExperiments = await PreferenceExperiments.getAll();
    is(fetchedExperiments.length, 2, "getAll returns a list of all stored experiments");
    Assert.deepEqual(
      fetchedExperiments.find(e => e.name === "experiment1"),
      experiment1,
      "getAll returns a list with the correct experiments",
    );
    const fetchedExperiment2 = fetchedExperiments.find(e => e.name === "experiment2");
    Assert.deepEqual(
      fetchedExperiment2,
      experiment2,
      "getAll returns a list with the correct experiments, including disabled ones",
    );

    fetchedExperiment2.name = "othername";
    is(experiment2.name, "experiment2", "getAll returns copies of the experiments");
  }
);

// get all active
decorate_task(
  withMockExperiments([
    experimentFactory({
      name: "active",
      expired: false,
    }),
    experimentFactory({
      name: "inactive",
      expired: true,
    }),
  ]),
  withMockPreferences,
  async function testGetAllActive([activeExperiment, inactiveExperiment]) {
    let allActiveExperiments = await PreferenceExperiments.getAllActive();
    Assert.deepEqual(
      allActiveExperiments,
      [activeExperiment],
      "getAllActive only returns active experiments",
    );

    allActiveExperiments[0].name = "newfakename";
    allActiveExperiments = await PreferenceExperiments.getAllActive();
    Assert.notEqual(
      allActiveExperiments,
      "newfakename",
      "getAllActive returns copies of stored experiments",
    );
  }
);

// has
decorate_task(
  withMockExperiments([experimentFactory({ name: "test" })]),
  async function(experiments) {
    ok(await PreferenceExperiments.has("test"), "has returned true for a stored experiment");
    ok(!(await PreferenceExperiments.has("missing")), "has returned false for a missing experiment");
  }
);

// init should register telemetry experiments
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    branch: "branch",
    preferenceName: "fake.pref",
    preferenceValue: "experiment value",
    expired: false,
    preferenceBranchType: "default",
  })]),
  withMockPreferences,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(PreferenceExperiments, "startObserver"),
  async function testInit(experiments, mockPreferences, setActiveStub, startObserverStub) {
    mockPreferences.set("fake.pref", "experiment value");
    await PreferenceExperiments.init();
    ok(
      setActiveStub.calledWith("test", "branch", { type: "normandy-exp" }),
      "Experiment is registered by init",
    );
  },
);

// init should use the provided experiment type
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    branch: "branch",
    preferenceName: "fake.pref",
    preferenceValue: "experiment value",
    experimentType: "pref-test",
  })]),
  withMockPreferences,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(PreferenceExperiments, "startObserver"),
  async function testInit(experiments, mockPreferences, setActiveStub, startObserverStub) {
    mockPreferences.set("fake.pref", "experiment value");
    await PreferenceExperiments.init();
    ok(
      setActiveStub.calledWith("test", "branch", { type: "normandy-pref-test" }),
      "init should use the provided experiment type",
    );
  },
);

// starting and stopping experiments should register in telemetry
decorate_task(
  withMockExperiments(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventStub,
  async function testStartAndStopTelemetry(experiments, setActiveStub, setInactiveStub, sendEventStub) {
    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "value",
      preferenceType: "string",
      preferenceBranchType: "default",
    });

    Assert.deepEqual(
      setActiveStub.getCall(0).args,
      ["test", "branch", { type: "normandy-exp" }],
      "Experiment is registered by start()",
    );
    await PreferenceExperiments.stop("test", { reason: "test-reason" });
    Assert.deepEqual(setInactiveStub.args, [["test"]], "Experiment is unregistered by stop()");

    Assert.deepEqual(
      sendEventStub.args,
      [
        ["enroll", "preference_study", "test", {
          experimentType: "exp",
          branch: "branch",
        }],
        ["unenroll", "preference_study", "test", {
          reason: "test-reason",
          didResetValue: "true",
          branch: "branch",
        }],
      ],
      "PreferenceExperiments.start() and stop() should send the correct telemetry event"
    );
  },
);

// starting experiments should use the provided experiment type
decorate_task(
  withMockExperiments(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventStub,
  async function testInitTelemetryExperimentType(experiments, setActiveStub, setInactiveStub, sendEventStub) {
    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "value",
      preferenceType: "string",
      preferenceBranchType: "default",
      experimentType: "pref-test",
    });

    Assert.deepEqual(
      setActiveStub.getCall(0).args,
      ["test", "branch", { type: "normandy-pref-test" }],
      "start() should register the experiment with the provided type",
    );

    Assert.deepEqual(
      sendEventStub.getCall(0).args,
      ["enroll", "preference_study", "test", {
        experimentType: "pref-test",
        branch: "branch",
      }],
      "start should include the passed reason in the telemetry event"
    );

    // start sets the passed preference in a way that is hard to mock.
    // Reset the preference so it doesn't interfere with other tests.
    Services.prefs.getDefaultBranch("fake.preference").deleteBranch("");
  },
);

// Experiments shouldn't be recorded by init() in telemetry if they are expired
decorate_task(
  withMockExperiments([experimentFactory({ name: "expired", branch: "branch", expired: true })]),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  async function testInitTelemetryExpired(experiments, setActiveStub) {
    await PreferenceExperiments.init();
    ok(!setActiveStub.called, "Expired experiment is not registered by init");
  },
);

// Experiments should end if the preference has been changed when init() is called
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    preferenceName: "fake.preference",
    preferenceValue: "experiment value",
  })]),
  withMockPreferences,
  withStub(PreferenceExperiments, "stop"),
  async function testInitChanges(experiments, mockPreferences, stopStub) {
    mockPreferences.set("fake.preference", "experiment value", "default");
    mockPreferences.set("fake.preference", "changed value", "user");
    await PreferenceExperiments.init();

    is(Preferences.get("fake.preference"), "changed value", "Preference value was not changed");

    Assert.deepEqual(
      stopStub.getCall(0).args,
      ["test", {
        resetValue: false,
        reason: "user-preference-changed-sideload",
      }],
      "Experiment is stopped correctly because value changed"
    );
  },
);

// init should register an observer for experiments
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    preferenceName: "fake.preference",
    preferenceValue: "experiment value",
  })]),
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stop"),
  withStub(CleanupManager, "addCleanupHandler"),
  async function testInitRegistersObserver(experiments, mockPreferences, startObserver, stop) {
    stop.throws("Stop should not be called");
    mockPreferences.set("fake.preference", "experiment value", "default");
    is(Preferences.get("fake.preference"), "experiment value", "pref shouldn't have a user value");
    await PreferenceExperiments.init();

    ok(startObserver.calledOnce, "init should register an observer");
    Assert.deepEqual(
      startObserver.getCall(0).args,
      ["test", "fake.preference", "string", "experiment value"],
      "init should register an observer with the right args",
    );
  }
);

// saveStartupPrefs
decorate_task(
  withMockExperiments([
    experimentFactory({
      name: "char",
      preferenceName: `fake.char`,
      preferenceValue: "string",
    }),
    experimentFactory({
      name: "int",
      preferenceName: `fake.int`,
      preferenceValue: 2,
    }),
    experimentFactory({
      name: "bool",
      preferenceName: `fake.bool`,
      preferenceValue: true,
    }),
  ]),
  async function testSaveStartupPrefs(experiments) {
    Services.prefs.deleteBranch(startupPrefs);
    Services.prefs.setBoolPref(`${startupPrefs}.fake.old`, true);
    await PreferenceExperiments.saveStartupPrefs();

    ok(
      Services.prefs.getBoolPref(`${startupPrefs}.fake.bool`),
      "The startup value for fake.bool was saved.",
    );
    is(
      Services.prefs.getCharPref(`${startupPrefs}.fake.char`),
      "string",
      "The startup value for fake.char was saved.",
    );
    is(
      Services.prefs.getIntPref(`${startupPrefs}.fake.int`),
      2,
      "The startup value for fake.int was saved.",
    );
    ok(
      !Services.prefs.prefHasUserValue(`${startupPrefs}.fake.old`),
      "saveStartupPrefs deleted old startup pref values.",
    );
  },
);

// saveStartupPrefs errors for invalid pref type
decorate_task(
  withMockExperiments([experimentFactory({
    name: "test",
    preferenceName: "fake.invalidValue",
    preferenceValue: new Date(),
  })]),
  async function testSaveStartupPrefsError(experiments) {
    await Assert.rejects(
      PreferenceExperiments.saveStartupPrefs(),
      /invalid preference type/i,
      "saveStartupPrefs throws if an experiment has an invalid preference value type",
    );
  },
);

// saveStartupPrefs should not store values for user-branch recipes
decorate_task(
  withMockExperiments([
    experimentFactory({
      name: "defaultBranchRecipe",
      preferenceName: "fake.default",
      preferenceValue: "experiment value",
      preferenceBranchType: "default",
    }),
    experimentFactory({
      name: "userBranchRecipe",
      preferenceName: "fake.user",
      preferenceValue: "experiment value",
      preferenceBranchType: "user",
    }),
  ]),
  async function testSaveStartupPrefsUserBranch(experiments) {
    Assert.deepEqual(Services.prefs.getChildList(startupPrefs), [], "As a prerequisite no startup prefs are set");

    await PreferenceExperiments.saveStartupPrefs();

    Assert.deepEqual(
      Services.prefs.getChildList(startupPrefs),
      [`${startupPrefs}.fake.default`],
      "only the expected prefs are set",
    );
    is(
      Services.prefs.getCharPref(`${startupPrefs}.fake.default`, "fallback value"),
      "experiment value",
      "The startup value for fake.default was set",
    );
    is(
      Services.prefs.getPrefType(`${startupPrefs}.fake.user`),
      Services.prefs.PREF_INVALID,
      "The startup value for fake.user was not set",
    );

    Services.prefs.deleteBranch(startupPrefs);
  },
);

// test that default branch prefs restore to the right value if the default pref changes
decorate_task(
  withMockExperiments(),
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stopObserver"),
  async function testDefaultBranchStop(mockExperiments, mockPreferences, stopObserverStub) {
    const prefName = "fake.preference";
    mockPreferences.set(prefName, "old version's value", "default");

    // start an experiment
    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: prefName,
      preferenceValue: "experiment value",
      preferenceBranchType: "default",
      preferenceType: "string",
    });

    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Starting an experiment should change the pref",
    );

    // Now pretend that firefox has updated and restarted to a version
    // where the built-default value of fake.preference is something
    // else. Bootstrap has run and changed the pref to the
    // experimental value, and produced the call to
    // recordOriginalValues below.
    PreferenceExperiments.recordOriginalValues({ [prefName]: "new version's value" });
    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Recording original values shouldn't affect the preference."
    );

    // Now stop the experiment. It should revert to the new version's default, not the old.
    await PreferenceExperiments.stop("test");
    is(
      Services.prefs.getCharPref(prefName),
      "new version's value",
      "Preference should revert to new default",
    );
  },
);

// test that default branch prefs restore to the right value if the preference is removed
decorate_task(
  withMockExperiments(),
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stopObserver"),
  async function testDefaultBranchStop(mockExperiments, mockPreferences, stopObserverStub) {
    const prefName = "fake.preference";
    mockPreferences.set(prefName, "old version's value", "default");

    // start an experiment
    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: prefName,
      preferenceValue: "experiment value",
      preferenceBranchType: "default",
      preferenceType: "string",
    });

    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Starting an experiment should change the pref",
    );

    // Now pretend that firefox has updated and restarted to a version
    // where fake.preference has been removed in the default pref set.
    // Bootstrap has run and changed the pref to the experimental
    // value, and produced the call to recordOriginalValues below.
    PreferenceExperiments.recordOriginalValues({ [prefName]: null });
    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Recording original values shouldn't affect the preference."
    );

    // Now stop the experiment. It should remove the preference
    await PreferenceExperiments.stop("test");
    is(
      Services.prefs.getCharPref(prefName, "DEFAULT"),
      "DEFAULT",
      "Preference should be absent",
    );
  },
).skip(/* bug 1502410 and bug 1505941 */);

// stop should pass "unknown" to telemetry event for `reason` if none is specified
decorate_task(
  withMockExperiments([experimentFactory({ name: "test", preferenceName: "fake.preference" })]),
  withMockPreferences,
  withStub(PreferenceExperiments, "stopObserver"),
  withSendEventStub,
  async function testStopUnknownReason(experiments, mockPreferences, stopObserverStub, sendEventStub) {
    mockPreferences.set("fake.preference", "default value", "default");
    await PreferenceExperiments.stop("test");
    is(
      sendEventStub.getCall(0).args[3].reason,
      "unknown",
      "PreferenceExperiments.stop() should use unknown as the default reason",
    );
  }
);

// stop should pass along the value for resetValue to Telemetry Events as didResetValue
decorate_task(
  withMockExperiments([
    experimentFactory({ name: "test1", preferenceName: "fake.preference1" }),
    experimentFactory({ name: "test2", preferenceName: "fake.preference2" }),
  ]),
  withMockPreferences,
  withStub(PreferenceExperiments, "stopObserver"),
  withSendEventStub,
  async function testStopResetValue(experiments, mockPreferences, stopObserverStub, sendEventStub) {
    mockPreferences.set("fake.preference1", "default value", "default");
    await PreferenceExperiments.stop("test1", { resetValue: true });
    is(sendEventStub.callCount, 1);
    is(
      sendEventStub.getCall(0).args[3].didResetValue,
      "true",
      "PreferenceExperiments.stop() should pass true values of resetValue as didResetValue",
    );

    mockPreferences.set("fake.preference2", "default value", "default");
    await PreferenceExperiments.stop("test2", { resetValue: false });
    is(sendEventStub.callCount, 2);
    is(
      sendEventStub.getCall(1).args[3].didResetValue,
      "false",
      "PreferenceExperiments.stop() should pass false values of resetValue as didResetValue",
    );
  }
);

// Should send the correct event telemetry when a study ends because
// the user changed preferences during a browser run.
decorate_task(
  withMockPreferences,
  withSendEventStub,
  withMockExperiments([experimentFactory({
    name: "test",
    expired: false,
    branch: "fakebranch",
    preferenceName: "fake.preference",
    preferenceValue: "experimentvalue",
    preferenceType: "string",
    previousPreferenceValue: "oldvalue",
    preferenceBranchType: "default",
  })]),
  async function testPrefChangeEventTelemetry(mockPreferences, sendEventStub, mockExperiments) {
    is(Preferences.get("fake.preference"), null, "preference should start unset");
    mockPreferences.set("fake.preference", "oldvalue", "default");
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");

    // setting the preference on the user branch should trigger the observer to stop the experiment
    mockPreferences.set("fake.preference", "uservalue", "user");

    // let the event loop tick to run the observer
    await Promise.resolve();

    Assert.deepEqual(
      sendEventStub.args,
      [["unenroll", "preference_study", "test", {
        didResetValue: "false",
        reason: "user-preference-changed",
        branch: "fakebranch",
      }]],
      "stop should send a telemetry event indicating the user unenrolled manually",
    );
  },
);
