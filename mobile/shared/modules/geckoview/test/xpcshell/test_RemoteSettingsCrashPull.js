/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsCrashPull:
    "resource://gre/modules/RemoteSettingsCrashPull.sys.mjs",
  EventDispatcher: "resource://gre/modules/Messaging.sys.mjs",
  makeFakeAppDir: "resource://testing-common/AppData.sys.mjs",
});

const kRemoteSettingsCollectionName = "crash-reports-ondemand";

add_setup(async function () {
  await lazy.makeFakeAppDir();
});

add_task(
  async function test_remoteSettingsGeneratesCorrectEvent_whenAttached() {
    let listener;
    const crashPullPromise = new Promise(resolve => {
      listener = {
        onEvent(aEvent, aData) {
          resolve([aEvent, aData]);
        },
      };
    });
    lazy.EventDispatcher.instance.registerListener(listener, [
      "GeckoView:RemoteSettingsCrashPull",
    ]);

    const oldCheckForInterestingUnsubmittedCrash =
      lazy.RemoteSettingsCrashPull.checkForInterestingUnsubmittedCrash;
    lazy.RemoteSettingsCrashPull.checkForInterestingUnsubmittedCrash =
      async _ => {
        return ["989df240-a40c-405a-9a22-f2fc4a31db6c"];
      };

    lazy.EventDispatcher.instance.dispatch(
      "GeckoView:CrashPullController.Delegate:Attached",
      undefined,
      undefined,
      undefined
    );

    const payload = {
      current: [],
      created: [
        {
          hashes: [
            // value here is not important
            "2435191bfd64cf0c8cbf0397f1cb5654f778388a3be72cb01502196896f5a0e9",
          ],
        },
      ],
      updated: [],
      deleted: [],
    };

    await lazy.RemoteSettings(kRemoteSettingsCollectionName).emit("sync", {
      data: payload,
    });

    const [aEvent, aData] = await crashPullPromise;
    Assert.equal(
      "GeckoView:RemoteSettingsCrashPull",
      aEvent,
      "expected a remote settings crash pull"
    );
    Assert.equal(
      1,
      aData.crashIDs.length,
      `expected one crash ID, but got ${aData.crashIDs.length}`
    );
    Assert.ok(
      aData.crashIDs[0].includes(
        "pending/989df240-a40c-405a-9a22-f2fc4a31db6c.dmp"
      ),
      "build path for crash dump"
    );

    lazy.RemoteSettingsCrashPull.checkForInterestingUnsubmittedCrash =
      oldCheckForInterestingUnsubmittedCrash;
  }
);
