/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const prefsDefaults = Services.prefs.getDefaultBranch("");
const defaultSoftBlockEnabledPrefValue =
  prefsDefaults.getPrefType("extensions.blocklist.softblock.enabled") ===
  prefsDefaults.PREF_INVALID
    ? undefined
    : prefsDefaults.getBoolPref("extensions.blocklist.softblock.enabled");

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);
Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", true);

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();

// This test needs to interact with the RemoteSettings client.
ExtensionBlocklistMLBF.ensureInitialized();

const softBlockedAddon = {
  id: "@softblocked",
  version: "1",
  signedDate: new Date(0), // a date in the past, before MLBF's generationTime.
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
};
const hardBlockedAddon = {
  id: "@blocked",
  version: "1",
  signedDate: new Date(0), // a date in the past, before MLBF's generationTime.
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
};
const nonBlockedAddon = {
  id: "@unblocked",
  version: "2",
  signedDate: new Date(0), // a date in the past, before MLBF's generationTime.
  signedState: AddonManager.SIGNEDSTATE_SIGNED,
};

add_setup(async () => {
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_ATTACHMENT_ID,
    { record: MLBF_RECORD, blob: await load_mlbf_record_as_blob() }
  );
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_SOFTBLOCKS_ATTACHMENT_ID,
    {
      record: MLBF_SOFTBLOCK_RECORD,
      blob: await load_mlbf_record_as_blob("mlbf-softblocked1.bin"),
    }
  );
});

add_task(async function fetch_and_apply_valid_softblocks_mlbf() {
  const { mlbfSoftBlocks: result } = await ExtensionBlocklistMLBF._fetchMLBF(
    null,
    MLBF_RECORD
  );
  Assert.equal(
    result.cascadeHash,
    MLBF_SOFTBLOCK_RECORD.attachment.hash,
    "hash OK"
  );
  Assert.equal(
    result.generationTime,
    MLBF_SOFTBLOCK_RECORD.generation_time,
    "time OK"
  );
  Assert.ok(result.cascadeFilter.has("@softblocked:1"), "item soft-blocked");
  Assert.ok(
    !result.cascadeFilter.has("@blocked:1"),
    "item hard-blocked should not be soft-blocked"
  );
  Assert.ok(!result.cascadeFilter.has("@unblocked:2"), "item non-blocked");

  const { mlbfSoftBlocks: result2 } = await ExtensionBlocklistMLBF._fetchMLBF(
    null,
    {
      attachment: { size: 1, hash: "invalid" },
      generation_time: Date.now(),
    }
  );
  Assert.equal(
    result2.cascadeHash,
    MLBF_SOFTBLOCK_RECORD.attachment.hash,
    "The cached MLBF should be used when the attachment is invalid"
  );

  // The attachment is kept in the database for use by the next test task.
});

add_task(async function public_api_uses_softblock_mlbf() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();

  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [MLBF_SOFTBLOCK_RECORD],
  });

  Assert.deepEqual(
    await Blocklist.getAddonBlocklistEntry(softBlockedAddon),
    {
      state: Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
      url: "https://addons.mozilla.org/en-US/firefox/blocked-addon/@softblocked/1/",
    },
    "soft-blocked addon should have soft-blocked entry"
  );

  Assert.deepEqual(
    await Blocklist.getAddonBlocklistEntry(nonBlockedAddon),
    null,
    "non-blocked addon should not be blocked"
  );

  Assert.deepEqual(
    await Blocklist.getAddonBlocklistEntry(hardBlockedAddon),
    {
      state: Ci.nsIBlocklistService.STATE_BLOCKED,
      url: "https://addons.mozilla.org/en-US/firefox/blocked-addon/@blocked/1/",
    },
    "hard-blocked addon should have an blocked entry"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(softBlockedAddon),
    Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
    "soft-blocked entry should have soft-blocked state"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(nonBlockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "non-blocked entry should have unblocked state"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(hardBlockedAddon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "hard-blocked entry should have blocked state"
  );

  // Note: Blocklist collection and attachment carries over to the next test.
});

add_task(async function blockstate_changes_on_softblock_pref_changes() {
  info("Verify addons blocklist state changes after disabling soft-blocks");

  Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", false);
  await ExtensionBlocklistMLBF._updatePromise;

  Assert.equal(
    await Blocklist.getAddonBlocklistState(softBlockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "soft-blocked entry should not have soft-blocked state after disabling soft-blocks"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(nonBlockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "non-blocked entry should still have unblocked state after disabling soft-blocks"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(hardBlockedAddon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "hard-blocked entry should still have blocked state after disabling soft-blocks"
  );

  info("Verify blocked addons states after re-enabling soft-blocks");

  // Shutdown the Blocklist to simulate the scenario where the
  // pref has been flipped between browser app sessions (while
  // Blocklist singleton wouldn't be already listening for changes
  // to the pref).
  Blocklist.shutdown();
  Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", true);
  Blocklist._init();
  // Force a blocklist check to confirm we have updated gBlocklistSoftBlockEnabled
  // from Blocklist._init() based on the pref value currently set as expected.
  ExtensionBlocklistMLBF._onUpdate();
  await ExtensionBlocklistMLBF._updatePromise;

  Assert.equal(
    await Blocklist.getAddonBlocklistState(softBlockedAddon),
    Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
    "soft-blocked entry should have soft-blocked state again after re-enabling soft-blocks"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(nonBlockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "non-blocked entry should still have unblocked state after re-enabling soft-blocks"
  );

  Assert.equal(
    await Blocklist.getAddonBlocklistState(hardBlockedAddon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "hard-blocked entry should still have blocked state after re-enabling soft-blocks"
  );

  info("Test again with soft-blocks derived from stashes");

  async function verifyAddonsState(expectedState) {
    Assert.equal(
      await Blocklist.getAddonBlocklistState(softBlockedAddon),
      expectedState,
      `Got expected block state on ${softBlockedAddon.id}:${softBlockedAddon.version}`
    );
    Assert.equal(
      await Blocklist.getAddonBlocklistState(nonBlockedAddon),
      expectedState,
      `Got expected block state on ${nonBlockedAddon.id}:${nonBlockedAddon.version}`
    );
    Assert.equal(
      await Blocklist.getAddonBlocklistState(hardBlockedAddon),
      Ci.nsIBlocklistService.STATE_BLOCKED,
      "hard-blocked entry should still have blocked state"
    );
  }

  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash: {
          blocked: [],
          softblocked: [
            `${softBlockedAddon.id}:${softBlockedAddon.version}`,
            `${nonBlockedAddon.id}:${nonBlockedAddon.version}`,
          ],
          unblocked: [],
        },
        stash_time: Date.now(),
      },
      MLBF_SOFTBLOCK_RECORD,
    ],
  });

  info("Test soft-block stashes with soft-blocks enabled");
  await verifyAddonsState(Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  info("Test soft-block stashes with soft-blocks disabled");
  Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", false);
  await verifyAddonsState(Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  info("Test soft-block stashes with soft-blocks enabled again");
  Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", true);
  await verifyAddonsState(Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  // Clear the stashes and sanity check the expected test addons block states.
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [MLBF_SOFTBLOCK_RECORD],
  });
  // Verify that the test addon soft-blocked only through stashes to become unblocked.
  Assert.equal(
    await Blocklist.getAddonBlocklistState(nonBlockedAddon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    `Got expected block state on ${nonBlockedAddon.id}:${nonBlockedAddon.version}`
  );
  // Verify that the test addon soft-blocked through soft-blocks bloomfilter to stay soft-blocked.
  Assert.equal(
    await Blocklist.getAddonBlocklistState(softBlockedAddon),
    Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
    `Got expected block state on ${softBlockedAddon.id}:${softBlockedAddon.version}`
  );
  // Verify hard-blocked addon is still hard-blocked.
  Assert.equal(
    await Blocklist.getAddonBlocklistState(hardBlockedAddon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "hard-blocked entry should still have blocked state after re-enabling soft-blocks"
  );

  // Note: Blocklist collection and attachment carries over to the next test.
});

add_task(
  // This test is skipped on builds that don't support enterprise policies
  // (e.g. android builds).
  { skip_if: () => !Services.policies },
  async function blockstate_changes_on_policies_softblock_pref() {
    info(
      "Verify blocked addons states after disabling soft-blocks from enterprise policies"
    );
    const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
      "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
    );
    await EnterprisePolicyTesting.setupPolicyEngineWithJson({
      policies: {
        Preferences: {
          "extensions.blocklist.softblock.enabled": {
            Value: false,
            Status: "user",
          },
        },
      },
    });
    EnterprisePolicyTesting.checkPolicyPref(
      "extensions.blocklist.softblock.enabled",
      false /* expectedValue */,
      false /* expectedLockedness */
    );
    await ExtensionBlocklistMLBF._updatePromise;

    Assert.equal(
      await Blocklist.getAddonBlocklistState(softBlockedAddon),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "Blocked entry should have unblocked state on policies disabled soft-blocks"
    );

    info(
      "Verify blocked addons states after being cleared from enterprise policies"
    );
    // Clear the pref explicitly then clear the enterprise policy.
    await EnterprisePolicyTesting.setupPolicyEngineWithJson({
      policies: {
        Preferences: {
          "extensions.blocklist.softblock.enabled": {
            Status: "clear",
          },
        },
      },
    });
    EnterprisePolicyTesting.checkPolicyPref(
      "extensions.blocklist.softblock.enabled",
      defaultSoftBlockEnabledPrefValue /* expectedValue */,
      false /* expectedLockedness */
    );

    // Re-enable soft-blocks if the current build has it disabled
    // by default and clearing the pref from the enterprise policy has
    // cleared the pref value explicitly set at the start of this test file.
    if (!Blocklist.isSoftBlockEnabled) {
      Services.prefs.setBoolPref(
        "extensions.blocklist.softblock.enabled",
        true
      );
    }
    await ExtensionBlocklistMLBF._updatePromise;

    Assert.equal(
      await Blocklist.getAddonBlocklistState(softBlockedAddon),
      Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
      "Blocked entry should have soft-blocked state again when policies are removed"
    );
    // Reset the enterprise policy config.
    await EnterprisePolicyTesting.setupPolicyEngineWithJson({});

    // Note: Blocklist collection and attachment carries over to the next test.
  }
);

add_task(async function blockstate_changes_on_hardblock_stashes() {
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash: {
          blocked: [`${softBlockedAddon.id}:${softBlockedAddon.version}`],
          softblocked: [],
          unblocked: [],
        },
        stash_time: Date.now(),
      },
      MLBF_SOFTBLOCK_RECORD,
    ],
  });

  Assert.equal(
    await Blocklist.getAddonBlocklistState(softBlockedAddon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Blocked entry should have hard-blocked state"
  );

  // Note: Blocklist collection and attachment carries over to the next test.
});
