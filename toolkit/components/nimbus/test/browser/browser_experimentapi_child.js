/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup() {
  const cleanup = await setupTest();
  registerCleanupFunction(cleanup);
});

add_task(async function testGetExperimentFromChildNewEnrollment() {
  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Open a tab so we have a content process.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: browserWindow.gBrowser,
    url: "https://example.com",
  });
  const browser = tab.linkedBrowser;

  // Assert that the tab is in fact a content process and that we don't have any
  // experiments available yet.
  await SpecialPowers.spawn(browser, [], async () => {
    const { ExperimentAPI } = ChromeUtils.importESModule(
      "resource://nimbus/ExperimentAPI.sys.mjs"
    );

    Assert.equal(
      Services.appinfo.processType,
      Services.appinfo.PROCESS_TYPE_CONTENT,
      "This is running in a content process"
    );

    await ExperimentAPI.ready();

    Assert.equal(
      ExperimentAPI.getExperiment({ slug: "foo" }),
      null,
      "Experiment should not exist in child yet"
    );
    Assert.equal(
      ExperimentAPI.getExperiment({ featureId: "test-feature" }),
      null,
      "Experiment should not exist in child yet"
    );
  });

  // Enroll in an experiment in the parent process.
  await ExperimentAPI._manager.enroll(
    ExperimentFakes.recipe("foo", {
      bucketConfig: {
        ...ExperimentFakes.recipe.bucketConfig,
        count: 1000,
      },
      branches: [
        {
          slug: "control",
          ratio: 1,
          features: [
            {
              featureId: "testFeature",
              value: {
                foo: "bar",
              },
            },
          ],
        },
      ],
    })
  );

  // Check that the new state is reflected in the content process.
  await SpecialPowers.spawn(browser, [], async () => {
    const { ExperimentAPI } = ChromeUtils.importESModule(
      "resource://nimbus/ExperimentAPI.sys.mjs"
    );
    const { TestUtils } = ChromeUtils.importESModule(
      "resource://testing-common/TestUtils.sys.mjs"
    );

    await TestUtils.waitForCondition(
      () => ExperimentAPI.getExperiment({ slug: "foo" }),
      "Wait for enrollment child to sync"
    );

    const bySlug = ExperimentAPI.getExperiment({ slug: "foo" });
    const byFeature = ExperimentAPI.getExperiment({ featureId: "testFeature" });

    for (const [rv, field] of [
      [bySlug, "slug"],
      [byFeature, "featureId"],
    ]) {
      info(`when calling ExperimentAPI.getExperiment with ${field}:`);

      Assert.equal(rv.slug, "foo", "Experiment slug is correct");
      Assert.ok(rv.active, "Experiment is active");
      Assert.equal(
        rv.branch.slug,
        "control",
        "Experiment branch slug is correct"
      );
      Assert.deepEqual(
        rv.branch.features,
        [
          {
            featureId: "testFeature",
            value: {
              foo: "bar",
            },
          },
        ],
        "Experiment branch value is correct"
      );
    }
  });

  // Unenroll from the experiment in the parent process.
  ExperimentAPI._manager.unenroll("foo");

  // Check that the new state is reflected in the content process.
  await SpecialPowers.spawn(browser, [], async () => {
    const { ExperimentAPI } = ChromeUtils.importESModule(
      "resource://nimbus/ExperimentAPI.sys.mjs"
    );
    const { TestUtils } = ChromeUtils.importESModule(
      "resource://testing-common/TestUtils.sys.mjs"
    );

    await TestUtils.waitForCondition(
      () => !ExperimentAPI.getExperiment({ slug: "foo" }).active,
      "Wait for unenrollment to sync"
    );

    const bySlug = ExperimentAPI.getExperiment({ slug: "foo" });
    const byFeature = ExperimentAPI.getExperiment({ featureId: "testFeature" });

    info(
      "After unenrollment, when calling ExperimentAPI.getExperiment with slug:"
    );

    Assert.notEqual(bySlug, null, "Experiment is not null");
    Assert.ok(!bySlug.active, "Experiment is not active");
    Assert.equal(bySlug.branch.slug, "control", "Experiment branch is correct");

    info(
      "After unenrollment, when calling ExperimentAPI.getExperiment with featureId:"
    );

    Assert.equal(byFeature, null, "Experiment is null");
  });

  ExperimentAPI._manager.store._deleteForTests("foo");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testGetExperimentFromChildExistingEnrollment() {
  const browserWindow =
    Services.wm.getMostRecentBrowserWindow("navigator:browser");

  await ExperimentAPI._manager.enroll(
    ExperimentFakes.recipe("qux", {
      bucketConfig: {
        ...ExperimentFakes.recipe.bucketConfig,
        count: 1000,
      },
      branches: [
        {
          slug: "treatment",
          ratio: 1,
          features: [
            {
              featureId: "testFeature",
              value: {
                foo: "bar",
              },
            },
          ],
        },
      ],
    })
  );

  // Open a tab so that we have a content process.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: browserWindow.gBrowser,
    url: "https://example.com",
  });
  const browser = tab.linkedBrowser;

  // Check that the experiment is available in the child process.
  await SpecialPowers.spawn(browser, [], async () => {
    const { ExperimentAPI } = ChromeUtils.importESModule(
      "resource://nimbus/ExperimentAPI.sys.mjs"
    );
    const { Assert } = ChromeUtils.importESModule(
      "resource://testing-common/Assert.sys.mjs"
    );

    await ExperimentAPI.ready();

    const bySlug = ExperimentAPI.getExperiment({ slug: "qux" });
    const byFeature = ExperimentAPI.getExperiment({ featureId: "testFeature" });

    for (const [rv, field] of [
      [bySlug, "slug"],
      [byFeature, "featureId"],
    ]) {
      info(`when calling ExperimentAPI.getExperiment with ${field}:`);

      Assert.equal(rv.slug, "qux", "Experiment slug is correct");
      Assert.ok(rv.active, "Experiment is active");
      Assert.equal(
        rv.branch.slug,
        "treatment",
        "Experiment branch slug is correct"
      );
      Assert.deepEqual(
        rv.branch.features,
        [
          {
            featureId: "testFeature",
            value: {
              foo: "bar",
            },
          },
        ],
        "Experiment branch value is correct"
      );
    }
  });

  ExperimentAPI._manager.unenroll("qux");
  ExperimentAPI._manager.store._deleteForTests("qux");
  BrowserTestUtils.removeTab(tab);
});
