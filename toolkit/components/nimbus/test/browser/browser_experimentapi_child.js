/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup() {
  const cleanup = await setupTest();

  SpecialPowers.addTaskImport(
    "ExperimentAPI",
    "resource://nimbus/ExperimentAPI.sys.mjs"
  );
  SpecialPowers.addTaskImport(
    "NimbusFeatures",
    "resource://nimbus/ExperimentAPI.sys.mjs"
  );
  SpecialPowers.addTaskImport(
    "TestUtils",
    "resource://testing-common/TestUtils.sys.mjs"
  );

  registerCleanupFunction(cleanup);
});

add_task(async function testGetFromChildNewEnrollment() {
  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Open a tab so we have a content process.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: browserWindow.gBrowser,
    url: "https://example.com",
    forceNewProcess: true,
  });
  const browser = tab.linkedBrowser;

  // Assert that the tab is in fact a content process and that we don't have any
  // experiments available yet.
  await SpecialPowers.spawn(browser, [], async () => {
    Assert.equal(
      Services.appinfo.processType,
      Services.appinfo.PROCESS_TYPE_CONTENT,
      "This is running in a content process"
    );

    await ExperimentAPI.ready();

    Assert.equal(
      NimbusFeatures.testFeature.getEnrollmentMetadata(),
      null,
      "Experiment should not exist in child yet"
    );
  });

  let childUpdated = await childSharedDataChanged(browser);

  // Enroll in an experiment in the parent process.
  await ExperimentAPI.manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "testFeature",
      value: {
        enabled: true,
        testInt: 123,
      },
    })
  );

  // Immediately serialize sharedData and broadcast changes to the child processes.
  //
  // In normal operation, this will happen during idle dispatch [1], but we want
  // to test that our IPC mechanisms work correctly, so we force it to happen so
  // that we can act immediately in the child.
  //
  // [1]: https://searchfox.org/mozilla-central/rev/5bea6ede57be43d450ecc24af7a535288c9a9f7d/dom/ipc/SharedMap.cpp#416-422
  Services.ppmm.sharedData.flush();
  await childUpdated.promise;

  // Check that the new state is reflected in the content process.
  await SpecialPowers.spawn(browser, [], async () => {
    await TestUtils.waitForCondition(
      () => NimbusFeatures.testFeature.getEnrollmentMetadata(),
      "Wait for enrollment child to sync"
    );

    const meta = NimbusFeatures.testFeature.getEnrollmentMetadata();

    Assert.equal(meta.slug, "foo", "Experiment slug is correct");
    Assert.equal(meta.branch, "control", "Experiment branch slug is correct");

    Assert.deepEqual(
      NimbusFeatures.testFeature.getAllVariables(),
      { enabled: true, testInt: 123 },
      "Experiment values are correct"
    );

    Assert.equal(
      NimbusFeatures.testFeature.getVariable("enabled"),
      true,
      "Experiment values are correct"
    );

    Assert.equal(
      NimbusFeatures.testFeature.getVariable("testInt"),
      123,
      "Experiment values are correct"
    );
  });

  childUpdated = await childSharedDataChanged(browser);
  // Unenroll from the experiment in the parent process.
  await ExperimentAPI.manager.unenroll("foo");
  // Propagate the change to child processes.
  Services.ppmm.sharedData.flush();
  await childUpdated.promise;

  // Check that the new state is reflected in the content process.
  await SpecialPowers.spawn(browser, [], async () => {
    await TestUtils.waitForCondition(
      () => NimbusFeatures.testFeature.getEnrollmentMetadata() === null,
      "Wait for unenrollment to sync"
    );
  });

  ExperimentAPI.manager.store._deleteForTests("foo");

  BrowserTestUtils.removeTab(tab);

  Services.ppmm.sharedData.flush();
});

add_task(async function testGetFromChildExistingEnrollment() {
  const browserWindow =
    Services.wm.getMostRecentBrowserWindow("navigator:browser");

  // We only want to test the new process case, so make sure to shut down any
  // existing processes.
  Services.ppmm.releaseCachedProcesses();

  await ExperimentAPI.manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("qux", {
      branchSlug: "treatment",
      featureId: "testFeature",
      value: {
        enabled: false,
        testInt: 456,
      },
    })
  );

  // We don't have to wait for this to update in the client, but we *do* have to
  // flush to make it re-serialize the contents to make it available to new
  // processes.
  Services.ppmm.sharedData.flush();

  // Open a tab so that we have a content process.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: browserWindow.gBrowser,
    url: "https://example.com",
    forceNewProcess: true,
  });
  const browser = tab.linkedBrowser;

  // Check that the experiment is available in the child process.
  await SpecialPowers.spawn(browser, [], async () => {
    await ExperimentAPI.ready();

    const meta = NimbusFeatures.testFeature.getEnrollmentMetadata();

    Assert.equal(meta.slug, "qux", "Experiment slug is correct");
    Assert.equal(meta.branch, "treatment", "Experiment branch slug is correct");

    Assert.deepEqual(
      NimbusFeatures.testFeature.getAllVariables(),
      { enabled: false, testInt: 456 },
      "Experiment values are correct"
    );

    Assert.equal(
      NimbusFeatures.testFeature.getVariable("enabled"),
      false,
      "Experiment values are correct"
    );

    Assert.equal(
      NimbusFeatures.testFeature.getVariable("testInt"),
      456,
      "Experiment values are correct"
    );
  });

  await ExperimentAPI.manager.unenroll("qux");
  ExperimentAPI.manager.store._deleteForTests("qux");
  BrowserTestUtils.removeTab(tab);

  Services.ppmm.sharedData.flush();
});
