/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { NimbusTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_task(async function test_nimbus_experiment_enabled() {
  let smartshortcutsfeed = AboutNewTab.activityStream.store.feeds.get(
    "feeds.smartshortcutsfeed"
  );

  // Initialize the feed, because that doesn't happen by default.
  await smartshortcutsfeed.onAction({ type: "INIT" });

  ok(!smartshortcutsfeed?.loaded, "Should initially not be loaded.");

  // Setup the experiment.
  await ExperimentAPI.ready();
  let doExperimentCleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "newtabSmartShortcuts",
    value: { enabled: true },
  });

  ok(smartshortcutsfeed?.loaded, "Should now be loaded.");

  await doExperimentCleanup();
});
