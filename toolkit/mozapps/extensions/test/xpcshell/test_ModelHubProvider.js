/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");
AddonTestUtils.init(this);

const MODELHUBPROVIDER_PREF = "browser.ml.modelHubProvider";

function ensureBrowserDelayedStartupFinished() {
  // ModelHubProvider does not register itself until the application startup
  // has been completed, and so we simulate that by firing this notification.
  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
}

add_setup(async () => {
  await promiseStartupManager();
});

add_task(
  {
    pref_set: [[MODELHUBPROVIDER_PREF, false]],
  },
  async function test_modelhub_provider_disabled() {
    ensureBrowserDelayedStartupFinished();
    ok(
      !AddonManager.hasProvider("ModelHubProvider"),
      "Expect no ModelHubProvider to be registered"
    );
  }
);

add_task(
  {
    pref_set: [[MODELHUBPROVIDER_PREF, true]],
  },
  async function test_modelhub_provider_enabled() {
    ensureBrowserDelayedStartupFinished();
    ok(
      AddonManager.hasProvider("ModelHubProvider"),
      "Expect ModelHubProvider to be registered"
    );
  }
);
