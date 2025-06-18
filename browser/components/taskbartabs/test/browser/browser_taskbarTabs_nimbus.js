/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  NimbusTestUtils: "resource://testing-common/NimbusTestUtils.sys.mjs",
  TaskbarTabsUtils: "resource:///modules/taskbartabs/TaskbarTabsUtils.sys.mjs",
});

const TASKBARTABS_PREF = "browser.taskbarTabs.enabled";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    clear: [TASKBARTABS_PREF],
  });
});

add_task(async function test_taskbarTabsNimbusPref() {
  await ExperimentAPI.ready();

  await checkEnrollmentWithValue(true);
  await checkEnrollmentWithValue(false);
});

async function checkEnrollmentWithValue(aEnabledState) {
  const doCleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "webApps",
    value: { enabled: aEnabledState },
  });

  is(TaskbarTabsUtils.isEnabled(), aEnabledState);
  await doCleanup();
}
