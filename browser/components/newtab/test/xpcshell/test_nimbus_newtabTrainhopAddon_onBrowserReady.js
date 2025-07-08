/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../../extensions/newtab/test/xpcshell/head.js */

/* import-globals-from head_nimbus_trainhop.js */

add_task(
  { pref_set: [[TRAINHOP_SCHEDULED_UPDATE_STATE_PREF, 100]] },
  async function test_scheduled_updateAddonState_onBrowserReady() {
    const { sinon } = ChromeUtils.importESModule(
      "resource://testing-common/Sinon.sys.mjs"
    );
    const sandbox = sinon.createSandbox();
    const asyncAssertNoPendingInstalls = async () => {
      Assert.deepEqual(
        await AddonManager.getAllInstalls(),
        [],
        "Expect no pending install to be found"
      );
    };

    const promiseInstallPostponed =
      AddonTestUtils.promiseInstallEvent("onInstallPostponed");
    const updateAddonVersion = `${BUILTIN_ADDON_VERSION}.123`;
    const { nimbusFeatureCleanup } = await setupNimbusTrainhopAddon({
      updateAddonVersion,
    });
    AboutNewTab.onBrowserReady();
    await promiseInstallPostponed;
    const { pendingInstall } = await asyncAssertNimbusTrainhopAddonStaged({
      updateAddonVersion,
    });
    await cancelPendingInstall(pendingInstall);
    // Sanity check.
    await asyncAssertNoPendingInstalls();

    await nimbusFeatureCleanup();

    // Verify that calls to updateTrainhopAddonState triggered while the client isn't
    // enrolled in the newtabTrainhopAddon Nimbus feature don't log any warning for
    // incomplete Nimbus feature variables, and that we do not call _installTrainhopAddon.
    const loggerWarnSpy = sandbox.spy(
      AboutNewTabResourceMapping.logger,
      "warn"
    );
    await AboutNewTabResourceMapping.updateTrainhopAddonState();
    await asyncAssertNoPendingInstalls();
    Assert.deepEqual(
      loggerWarnSpy.getCalls().map(spyCall => spyCall.args),
      [],
      "Expect no warning to be logged by updateTrainhopAddonState when not enrolled"
    );
    sandbox.restore();

    // NOTE: prevents AboutNewTab.uninit to hit unexpected failures while the test harness
    // is shutting down (side-effect of calling AboutNewTab onBrowserReady and the simulated
    // application restarts).
    AboutNewTab.activityStream = null;
  }
);
