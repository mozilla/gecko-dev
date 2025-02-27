/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../../../toolkit/crashreporter/test/browser/head.js */
load("head.js");

const { RemoteSettingsCrashPull } = ChromeUtils.importESModule(
  "resource://gre/modules/RemoteSettingsCrashPull.sys.mjs"
);

const { CrashServiceUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/CrashService.sys.mjs"
);

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const kRemoteSettingsCollectionName = "crash-reports-ondemand";

let rscp = undefined;
let crD = undefined;

async function generatePending(content) {
  const pending = crD.clone();
  pending.append("pending");
  const pendingCrash = addPendingCrashreport(
    crD,
    Date.now(),
    content,
    JSON.stringify(content)
  );
  const pendingDmp = PathUtils.join(pending.path, `${pendingCrash.id}.dmp`);
  return {
    id: pendingCrash.id,
    sha: await CrashServiceUtils.computeMinidumpHash(pendingDmp),
  };
}

async function deletePending(id) {
  const pending = crD.clone();
  await pending.append("pending");

  let dmp = pending.clone();
  await dmp.append(`${id}.dmp`);
  await dmp.remove(false);

  let extra = pending.clone();
  await extra.append(`${id}.extra`);
  await extra.remove(false);

  let memory_json_gz = pending.clone();
  await memory_json_gz.append(`${id}.memory.json.gz`);
  await memory_json_gz.remove(false);
}

add_setup(async function setup_test() {
  do_get_profile();

  const appD = make_fake_appdir();
  crD = appD.clone();
  crD.append("Crash Reports");

  rscp = RemoteSettingsCrashPull;
  await rscp.collection();
});

add_task(function test_verify_valid() {
  Assert.ok(rscp, "RemoteSettingsCrashPull obtained.");
});

add_task(async function test_no_crash_pending_found() {
  let dateLimit = new Date();
  dateLimit.setDate(dateLimit.getDate() - 1);

  let crashes = await rscp.getPendingSha256(dateLimit);
  Assert.equal(crashes.length, 0, "No pending crash");
});

add_task(async function test_one_crash_pending_found() {
  let dateLimit = new Date();
  dateLimit.setDate(dateLimit.getDate() - 1);

  const generatedSha = await generatePending({ foo: "bar" });

  let crashes = await rscp.getPendingSha256(dateLimit);
  Assert.equal(crashes.length, 1, "One pending crash");
  Assert.ok(!!crashes[0].sha256.length, "Generated crash SHA256 exists");
  Assert.equal(
    crashes[0].sha256,
    generatedSha.sha,
    "Generated crash SHA256 match"
  );

  await deletePending(crashes[0].id);
  crashes = await rscp.getPendingSha256(dateLimit);
  Assert.equal(crashes.length, 0, "No more pending crash");
});

add_task(async function test_more_crash_pending_found() {
  let dateLimit = new Date();
  dateLimit.setDate(dateLimit.getDate() - 1);

  const NB_CRASHES = 10;

  let generatedShas = [];
  for (let i = 0; i < NB_CRASHES; ++i) {
    generatedShas[i] = await generatePending({ foo: "bar", val: i });
  }

  let crashes = await rscp.getPendingSha256(dateLimit);
  Assert.equal(crashes.length, generatedShas.length, "More pending crash");

  const generatedShaValues = generatedShas.map(e => e.sha);
  const crashesShaValues = crashes.map(e => e.sha256);

  for (let i = 0; i < NB_CRASHES; ++i) {
    Assert.ok(!!crashesShaValues.length, `Generated crash ${i} SHA256 exists`);
    Assert.ok(
      crashesShaValues.includes(generatedShaValues[i]),
      `Generated crash ${i} SHA256 match`
    );
  }

  for (let i = 0; i < NB_CRASHES; ++i) {
    await deletePending(crashes[i].id);
  }

  crashes = await rscp.getPendingSha256(dateLimit);
  Assert.equal(crashes.length, 0, "No more pending crash");
});

add_task(async function test_no_interesting_crash_at_all() {
  let matches = await rscp.checkForInterestingUnsubmittedCrash([]);
  Assert.equal(matches.length, 0, "No matching crash");
});

add_task(
  async function test_no_interesting_crash_with_no_crash_pending_one_record() {
    let matches = await rscp.checkForInterestingUnsubmittedCrash([
      {
        hashes: [
          "eec354db7cec5508f75c2dbb07b780386b0022806f164a75c230f988c4779f65",
        ],
      },
    ]);
    Assert.equal(matches.length, 0, "No matching crash");
  }
);

add_task(
  async function test_no_interesting_crash_with_crash_pending_one_record_not_matching() {
    const generatedSha = await generatePending({ foo: "bar", val: "toto" });
    Assert.equal(
      generatedSha.sha,
      "97d33a98fe06d7b652a678c2e8cc2fc6f317e7ea6544b353490e97143db76f34",
      "Verify generated crash sha"
    );

    let matches = await rscp.checkForInterestingUnsubmittedCrash([
      {
        hashes: [
          "eec354db7cec5508f75c2dbb07b780386b0022806f164a75c230f988c4779f65",
        ],
      },
    ]);
    Assert.equal(matches.length, 0, "No matching crash");

    await deletePending(generatedSha.id);
  }
);

add_task(
  async function test_one_interesting_crash_with_crash_pending_one_record_matching() {
    const generatedSha = await generatePending({ foo: "bar", val: "toto" });
    Assert.equal(
      generatedSha.sha,
      "97d33a98fe06d7b652a678c2e8cc2fc6f317e7ea6544b353490e97143db76f34",
      "Verify generated crash sha"
    );

    let matches = await rscp.checkForInterestingUnsubmittedCrash([
      {
        hashes: [
          "97d33a98fe06d7b652a678c2e8cc2fc6f317e7ea6544b353490e97143db76f34",
        ],
      },
    ]);
    Assert.equal(matches.length, 1, "One matching crash");
    Assert.equal(matches[0], generatedSha.id, "IDs of crash matches");

    await deletePending(generatedSha.id);
  }
);

add_task(
  async function test_no_interesting_crash_at_all_from_remoteSettings_sync() {
    const payload = {
      current: [],
      created: [
        {
          hashes: ["1", "2", "3"],
        },
        {
          hashes: ["4", "5", "6"],
        },
      ],
      updated: [],
      deleted: [],
    };

    let callbackCalled = false;
    rscp._showCallback = function (_) {
      callbackCalled = true;
    };

    await RemoteSettings(kRemoteSettingsCollectionName).emit("sync", {
      data: payload,
    });

    try {
      await TestUtils.waitForCondition(
        () => callbackCalled,
        "Waiting for callback to have been called (should not happen"
      );
    } catch (ex) {
      if (!ex.includes("timed out after 50 tries")) {
        throw ex;
      }
    }

    Assert.ok(!callbackCalled, "Show callback should not have been called");
    rscp._showCallback = undefined;
  }
);

add_task(
  async function test_one_interesting_crash_from_remoteSettings_one_record() {
    const generatedSha = await generatePending({ foo: "bar", val: "coucou" });
    Assert.equal(
      generatedSha.sha,
      "2435191bfd64cf0c8cbf0397f1cb5654f778388a3be72cb01502196896f5a0e9",
      "Verify generated crash sha"
    );

    const payload = {
      current: [],
      created: [
        {
          hashes: [
            "2435191bfd64cf0c8cbf0397f1cb5654f778388a3be72cb01502196896f5a0e9",
          ],
        },
      ],
      updated: [],
      deleted: [],
    };

    let callbackCalled = false;
    rscp._showCallback = function (reportIDs) {
      Assert.equal(reportIDs.length, 1, "Should have one match");
      Assert.equal(reportIDs[0], generatedSha.id, "ID of report should match");
      callbackCalled = true;
    };

    await RemoteSettings(kRemoteSettingsCollectionName).emit("sync", {
      data: payload,
    });
    await TestUtils.waitForCondition(
      () => callbackCalled,
      "Waiting for callback to have been called"
    );
    rscp._showCallback = undefined;
    await deletePending(generatedSha.id);
  }
);

add_task(
  async function test_one_interesting_crash_from_remoteSettings_more_records() {
    const generatedSha = await generatePending({ foo: "bar", val: "coucou2" });
    Assert.equal(
      generatedSha.sha,
      "93211f02bf1bd5316e85ff3738c3edcf8569e11e4d1a5bd23e9596cd0f7c3520",
      "Verify generated crash sha"
    );

    const payload = {
      current: [],
      created: [
        {
          hashes: ["1", "2"],
        },
        {
          hashes: [
            "93211f02bf1bd5316e85ff3738c3edcf8569e11e4d1a5bd23e9596cd0f7c3520",
            "3",
          ],
        },
      ],
      updated: [],
      deleted: [],
    };

    let callbackCalled = false;
    rscp._showCallback = function (reportIDs) {
      Assert.equal(reportIDs.length, 1, "Should have one match");
      Assert.equal(reportIDs[0], generatedSha.id, "ID of report should match");
      callbackCalled = true;
    };

    await RemoteSettings(kRemoteSettingsCollectionName).emit("sync", {
      data: payload,
    });
    await TestUtils.waitForCondition(
      () => callbackCalled,
      "Waiting for callback to have been called"
    );
    rscp._showCallback = undefined;
    await deletePending(generatedSha.id);
  }
);

add_task(
  async function test_one_interesting_crash_from_remoteSettings_more_records_dupes() {
    const generatedSha = await generatePending({ foo: "bar", val: "coucou3" });
    Assert.equal(
      generatedSha.sha,
      "7f004093f5821cbf1ab055d07d53759052acca5a03a978ba3d96ed121e2b8266",
      "Verify generated crash sha"
    );

    const payload = {
      current: [],
      created: [
        {
          hashes: ["1", "2"],
        },
        {
          hashes: [
            "7f004093f5821cbf1ab055d07d53759052acca5a03a978ba3d96ed121e2b8266",
            "3",
          ],
        },
        {
          hashes: [
            "4",
            "5",
            "7f004093f5821cbf1ab055d07d53759052acca5a03a978ba3d96ed121e2b8266",
            "7",
          ],
        },
      ],
      updated: [],
      deleted: [],
    };

    let callbackCalled = false;
    rscp._showCallback = function (reportIDs) {
      Assert.equal(reportIDs.length, 1, "Should have one match");
      Assert.equal(reportIDs[0], generatedSha.id, "ID of report should match");
      callbackCalled = true;
    };

    await RemoteSettings(kRemoteSettingsCollectionName).emit("sync", {
      data: payload,
    });
    await TestUtils.waitForCondition(
      () => callbackCalled,
      "Waiting for callback to have been called"
    );
    rscp._showCallback = undefined;
    await deletePending(generatedSha.id);
  }
);

add_task(
  async function test_many_interesting_crash_from_remoteSettings_many_records() {
    const NB_CRASHES = 10;

    let generatedShas = [];
    for (let i = 0; i < NB_CRASHES; ++i) {
      generatedShas[i] = await generatePending({
        foo: "bar",
        val: "coucou",
        idx: i,
      });
    }

    const payload = {
      current: [],
      created: [
        {
          hashes: ["1", generatedShas[9].sha, "2", generatedShas[0].sha],
        },
        {
          hashes: [generatedShas[1].sha, "3"],
        },
        {
          hashes: ["4", generatedShas[6].sha, "5", generatedShas[3].sha, "7"],
        },
      ],
      updated: [],
      deleted: [],
    };

    let callbackCalled = false;
    rscp._showCallback = function (reportIDs) {
      Assert.equal(reportIDs.length, 5, "Should have one match");

      Assert.notEqual(
        reportIDs[0],
        reportIDs[1],
        "ID of report #0 should be different from #1"
      );
      Assert.notEqual(
        reportIDs[1],
        reportIDs[2],
        "ID of report #1 should be different from #2"
      );
      Assert.notEqual(
        reportIDs[2],
        reportIDs[3],
        "ID of report #2 should be different from #3"
      );
      Assert.notEqual(
        reportIDs[3],
        reportIDs[4],
        "ID of report #3 should be different from #4"
      );
      for (let i of [0, 1, 3, 6, 9]) {
        Assert.ok(
          reportIDs.includes(generatedShas[i].id),
          `ID of sha ${i} found in reports`
        );
      }

      callbackCalled = true;
    };

    await RemoteSettings(kRemoteSettingsCollectionName).emit("sync", {
      data: payload,
    });
    await TestUtils.waitForCondition(
      () => callbackCalled,
      "Waiting for callback to have been called"
    );
    rscp._showCallback = undefined;
    for (let i = 0; i < NB_CRASHES; ++i) {
      await deletePending(generatedShas[i].id);
    }
  }
);

add_task(function teardown_test() {
  cleanup_fake_appdir();
});
