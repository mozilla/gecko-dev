/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);
Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const { STATE_NOT_BLOCKED, STATE_SOFTBLOCKED, STATE_BLOCKED } =
  Ci.nsIBlocklistService;

function getTestExtensionDefinition(
  addonId,
  addonVersion,
  addonType = "extension",
  { hidden = false } = {}
) {
  let manifestData = {};
  let files = {};
  switch (addonType) {
    case "extension":
      manifestData = { hidden };
      break;
    case "theme":
      manifestData = { theme: {} };
      break;
    case "dictionary":
      manifestData = { dictionaries: { "en-US": "en-US.dic" } };
      files = { "en-US.dic": "", "en-US.aff": "" };
      break;
    case "locale":
      manifestData = {
        langpack_id: "und",
        languages: {},
      };
      break;
    default:
      throw new Error(
        `Unexpected addonType for ${addonId}:${addonVersion}: ${addonType}`
      );
  }
  return {
    useAddonManager: "permanent",
    manifest: {
      ...manifestData,
      version: addonVersion,
      browser_specific_settings: { gecko: { id: addonId } },
    },
    files,
  };
}

async function installTestExtension(addonId) {
  const extension = ExtensionTestUtils.loadExtension(
    getTestExtensionDefinition(addonId, "1.0", "extension")
  );
  await extension.startup();
  const addon = await AddonManager.getAddonByID(addonId);
  Assert.equal(
    addon.type,
    "extension",
    `${addonId} should have the expected addon type`
  );
  return extension;
}

async function installTestAddon(addonId, addonType, { hidden = false } = {}) {
  const xpi = AddonTestUtils.createTempWebExtensionFile(
    getTestExtensionDefinition(addonId, "1.0", addonType, { hidden })
  );
  await AddonTestUtils.promiseInstallFile(xpi);
  const addon = await AddonManager.getAddonByID(addonId);
  Assert.equal(
    addon.type,
    addonType,
    `${addonId} should have the expected addon type`
  );
  Assert.equal(
    addon.hidden,
    hidden,
    `${addonId} is expected to ${hidden ? "" : "NOT"} be hidden`
  );
  return addon;
}

function assertXPIDatabaseBlocklistAttentionAddonIds(expectedAddonIdsArray) {
  const { XPIDatabase } = AddonTestUtils.getXPIExports();
  Assert.deepEqual(
    expectedAddonIdsArray,
    Array.from(XPIDatabase.blocklistAttentionAddonIdsSet),
    "Got the expected addon ids in the XPIDatabase.blocklistAttentionAddonIdsSet"
  );
}

function getBlockKey({ id, version }) {
  if (!id || !version) {
    // Throw an error if the resulting block key would not be a valid one.
    throw new Error(
      "getBlockKey requires id and version to be defined and non-empty"
    );
  }
  return `${id}:${version}`;
}

function createStashRecord({
  blocked = [],
  softblocked = [],
  unblocked = [],
  stash_time = 0,
} = {}) {
  return {
    stash_time,
    stash: {
      blocked: blocked.map(getBlockKey),
      softblocked: softblocked.map(getBlockKey),
      unblocked: unblocked.map(getBlockKey),
    },
  };
}

async function assertBlocklistAttentionInfo(expected) {
  let baInfo = await AddonManager.getBlocklistAttentionInfo();
  Assert.deepEqual(
    baInfo.addons?.map(it => it.id),
    expected.addons,
    "blocklistAttentionInfo.addons"
  );
  Assert.equal(
    baInfo.shouldShow,
    expected.shouldShow,
    "blocklistAttentionInfo.shouldShow"
  );
  Assert.equal(
    baInfo.hasSoftBlocked,
    expected.hasSoftBlocked,
    "blocklistAttentionInfo.hasSoftBlocked"
  );
  Assert.equal(
    baInfo.hasHardBlocked,
    expected.hasHardBlocked,
    "blocklistAttentionInfo.hasHardBlocked"
  );
  Assert.equal(
    baInfo.extensionsCount,
    expected.addons.length,
    "blocklistAttentionInfo.extensionsCount"
  );
  return baInfo;
}

async function testBlocklistAttentionScenario({
  blocklistStashRecords,
  expectBeforeDismiss,
  expectAfterDismiss,
}) {
  if (blocklistStashRecords) {
    info("Loading test blocklist data");
    await AddonTestUtils.loadBlocklistRawData({
      extensionsMLBF: blocklistStashRecords,
    });
  }

  info("Verify expectations before dismissing the blocklist attention info");

  for (const [addonId, blocklistState] of Object.entries(
    expectBeforeDismiss.blocklistStates
  )) {
    // Sanity check the expected addon blocklist states.
    const addon = await AddonManager.getAddonByID(addonId);
    Assert.equal(
      addon?.blocklistState,
      blocklistState,
      `Got expected blocklistState for ${addonId}`
    );
  }
  for (const [addonId, blocklistAttentionDismissed] of Object.entries(
    expectBeforeDismiss.dismissedStates
  )) {
    const addon = await AddonManager.getAddonByID(addonId);
    Assert.equal(
      addon?.blocklistAttentionDismissed,
      blocklistAttentionDismissed,
      `Got expected blocklistAttentionDismissed for ${addonId}`
    );
  }
  Assert.equal(
    AddonManager.shouldShowBlocklistAttention(),
    expectBeforeDismiss.shouldShowBlocklistAttention,
    "AddonManager.shouldShowBlocklistAttention()"
  );

  const baInfo = await assertBlocklistAttentionInfo(
    expectBeforeDismiss.blocklistAttentionInfo
  );

  if (!expectAfterDismiss) {
    return;
  }

  let managerListener = {
    onBlocklistAttentionUpdated: sinon.spy(),
  };
  AddonManager.addManagerListener(managerListener);
  baInfo.dismiss();
  AddonManager.removeManagerListener(managerListener);

  info("Verify expected managerListener call count");
  Assert.equal(
    managerListener.onBlocklistAttentionUpdated.callCount,
    expectAfterDismiss.managerListenerCallCount ?? 0,
    "Got the expected number of call to onBlocklistAttentionUpdated manager listeners"
  );

  info("Verify expectations before dismissing the blocklist attention info");
  for (const [addonId, blocklistAttentionDismissed] of Object.entries(
    expectAfterDismiss.dismissedStates
  )) {
    const addon = await AddonManager.getAddonByID(addonId);
    Assert.equal(
      addon?.blocklistAttentionDismissed,
      blocklistAttentionDismissed,
      `Got expected blocklistAttentionDismissed for ${addonId}`
    );
  }
  Assert.equal(
    AddonManager.shouldShowBlocklistAttention(),
    expectAfterDismiss.shouldShowBlocklistAttention,
    "AddonManager.shouldShowBlocklistAttention()"
  );

  {
    const expected = expectAfterDismiss.blocklistAttentionInfo;
    Assert.equal(
      baInfo.shouldShow,
      expected.shouldShow,
      "blocklistAttentionInfo.shouldShow"
    );
  }
}

add_setup(async function setup() {
  await promiseStartupManager();
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
});

add_task(async function test_blocklist_attention_basic() {
  const ext1 = await installTestExtension("@ext1");
  const ext2 = await installTestExtension("@ext2");
  const ext3 = await installTestExtension("@ext3");

  const addon1 = await AddonManager.getAddonByID(ext1.id);
  const addon2 = await AddonManager.getAddonByID(ext2.id);
  const addon3 = await AddonManager.getAddonByID(ext3.id);

  // Sanity checks.
  for (const addon of [addon1, addon2, addon3]) {
    Assert.ok(addon, "Expect addon to be found");
    Assert.equal(
      addon.blocklistState,
      STATE_NOT_BLOCKED,
      "Expect addon to not be blocked"
    );
    Assert.ok(addon.isActive, "Expect addon to be enabled");
    Assert.ok(
      !addon.blocklistAttentionDismissed,
      "Expect addon.blocklistAttentionDismissed to be false"
    );
  }
  Assert.ok(
    !AddonManager.shouldShowBlocklistAttention(),
    "Expect shouldShowBlocklistAttention to be initially returning false"
  );

  info("Test blocklist attention info on a single hard-blocked addon");

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [createStashRecord({ blocked: [addon1] })],
    expectBeforeDismiss: {
      blocklistStates: {
        [addon1.id]: STATE_BLOCKED,
        [addon2.id]: STATE_NOT_BLOCKED,
        [addon3.id]: STATE_NOT_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: false,
        hasHardBlocked: true,
        addons: [addon1.id],
      },
      dismissedStates: {
        [addon1.id]: false,
        [addon2.id]: false,
        [addon3.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
    expectAfterDismiss: {
      managerListenerCallCount: 1,
      blocklistAttentionInfo: {
        shouldShow: false,
      },
      dismissedStates: {
        [addon1.id]: true,
        [addon2.id]: false,
        [addon3.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  info("Test blocklist attention info on new single soft-blocked addon");

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [
      createStashRecord({ blocked: [addon1] }),
      createStashRecord({ softblocked: [addon2] }),
    ],
    expectBeforeDismiss: {
      blocklistStates: {
        [addon1.id]: STATE_BLOCKED,
        [addon2.id]: STATE_SOFTBLOCKED,
        [addon3.id]: STATE_NOT_BLOCKED,
      },
      // blocklistAttentionInfo should reflect that the attention
      // on addon1 hard-block was already dismissed before.
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: false,
        addons: [addon2.id],
      },
      dismissedStates: {
        [addon1.id]: true, // Dismissed as part of the previous scenario.
        [addon2.id]: false,
        [addon3.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
    expectAfterDismiss: {
      managerListenerCallCount: 1,
      blocklistAttentionInfo: {
        shouldShow: false,
      },
      dismissedStates: {
        [addon1.id]: true,
        [addon2.id]: true,
        [addon3.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  info("Test blocklist attention info after all blocks are removed");

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [createStashRecord()],
    expectBeforeDismiss: {
      blocklistStates: {
        [addon1.id]: STATE_NOT_BLOCKED,
        [addon2.id]: STATE_NOT_BLOCKED,
        [addon3.id]: STATE_NOT_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: false,
        hasSoftBlocked: false,
        hasHardBlocked: false,
        addons: [],
      },
      // The flag should have been cleared when the blocklistState for the test addons changed
      // to STATE_NOT_BLOCKED as a side-effect of the blocklist stash record used by this call.
      dismissedStates: {
        [addon1.id]: false,
        [addon2.id]: false,
        [addon3.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
    expectAfterDismiss: {
      managerListenerCallCount: 0,
      blocklistAttentionInfo: {
        shouldShow: false,
      },
      // None of the 3 test addons should have the blocklistAttentionDismissed flag set
      // to true after blocklistAttentionInfo.dismiss method was called.
      dismissedStates: {
        [addon1.id]: false,
        [addon2.id]: false,
        [addon3.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  info(
    "Test blocklist attention info after new hard and soft blocks have been received"
  );

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [
      createStashRecord({ blocked: [addon2], softblocked: [addon3] }),
    ],
    expectBeforeDismiss: {
      blocklistStates: {
        [addon1.id]: STATE_NOT_BLOCKED,
        [addon2.id]: STATE_BLOCKED,
        [addon3.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: true,
        addons: [addon2.id, addon3.id],
      },
      dismissedStates: {
        [addon1.id]: false,
        [addon2.id]: false,
        [addon3.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
    expectAfterDismiss: {
      managerListenerCallCount: 2,
      blocklistAttentionInfo: {
        shouldShow: false,
      },
      dismissedStates: {
        [addon1.id]: false,
        [addon2.id]: true,
        [addon3.id]: true,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
  await ext3.unload();
  await ext2.unload();
  await ext1.unload();
});

add_task(async function test_attentions_reinitalized_on_restart() {
  const ext1 = await installTestExtension("@ext1");
  const ext2 = await installTestExtension("@ext2");
  const ext3 = await installTestExtension("@ext3");

  // Test blocklist attention without dismissing it.
  await testBlocklistAttentionScenario({
    blocklistStashRecords: [
      createStashRecord({ blocked: [ext1], softblocked: [ext2] }),
    ],
    expectBeforeDismiss: {
      blocklistStates: {
        [ext1.id]: STATE_BLOCKED,
        [ext2.id]: STATE_SOFTBLOCKED,
        [ext3.id]: STATE_NOT_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: true,
        addons: [ext1.id, ext2.id],
      },
      dismissedStates: {
        [ext1.id]: false,
        [ext2.id]: false,
        [ext3.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  assertXPIDatabaseBlocklistAttentionAddonIds([ext1.id, ext2.id]);

  info("Simulate AOM shutdown");
  await promiseShutdownManager();
  AddonTestUtils.getXPIExports().XPIDatabase.clearBlocklistAttentionAddonIdsSet();
  assertXPIDatabaseBlocklistAttentionAddonIds([]);

  const promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
    "onBlocklistAttentionUpdated",
    () => {
      const { XPIDatabase } = AddonTestUtils.getXPIExports();
      // The BlocklistAttentionUpdated may have been fired more than once,
      // but we want to be sure that it is fired at least once when the
      // XPIDatabase blocklistAttention add-on ids has been populated
      // with all the expected addon ids.
      return XPIDatabase.blocklistAttentionAddonIdsSet.size == 2;
    }
  );
  info("Simulate new AOM startup");
  await promiseStartupManager();
  info("Wait for onBlocklistAttentionUpdated Manager listener to be called");
  await promiseBlocklistAttentionUpdated;
  assertXPIDatabaseBlocklistAttentionAddonIds([ext1.id, ext2.id]);

  // Test blocklist attention again after AOM restart
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [ext1.id]: STATE_BLOCKED,
        [ext2.id]: STATE_SOFTBLOCKED,
        [ext3.id]: STATE_NOT_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: true,
        addons: [ext1.id, ext2.id],
      },
      dismissedStates: {
        [ext1.id]: false,
        [ext2.id]: false,
        [ext3.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
  await ext3.unload();
  await ext2.unload();
  await ext1.unload();
});

add_task(async function test_dismissed_flag_cleared_on_addon_updates() {
  const ext1 = await installTestExtension("@ext1");
  const ext2 = await installTestExtension("@ext2");

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [createStashRecord({ blocked: [ext1, ext2] })],
    expectBeforeDismiss: {
      blocklistStates: {
        [ext1.id]: STATE_BLOCKED,
        [ext2.id]: STATE_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: false,
        hasHardBlocked: true,
        addons: [ext1.id, ext2.id],
      },
      dismissedStates: {
        [ext1.id]: false,
        [ext2.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
    expectAfterDismiss: {
      managerListenerCallCount: 2,
      blocklistAttentionInfo: {
        shouldShow: false,
      },
      dismissedStates: {
        [ext1.id]: true,
        [ext2.id]: true,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  await ext1.upgrade(getTestExtensionDefinition(ext1.id, "2.0"));
  Assert.equal(
    ext1.version,
    "2.0",
    "Expect test extension version changed to 2.0"
  );

  const addon1v2 = await AddonManager.getAddonByID(ext1.id);
  Assert.equal(
    addon1v2.version,
    "2.0",
    "Expect new AddonWrapper instance version to be 2.0"
  );
  // The blocklistAttentionDismissed field is not propagated from the old to the new AddonInternal
  // instances when the addon is being updated, that ensures the flag is implicitly cleared
  // when the addon is updated.
  Assert.equal(
    addon1v2.blocklistAttentionDismissed,
    false,
    "Expect new AddonWrapper instance blocklistAttentionDismissed to be false"
  );

  // The XPIDatabase blocklistAttentionAddonIdsSet is expected to be empty as a side-effect
  // of the user dismissing the blocklist attention.
  assertXPIDatabaseBlocklistAttentionAddonIds([]);

  await promiseRestartManager();

  // After restarting the AddonManager we expect the XPIDatabase blocklistAttentionAddonIdsSet
  // to still be empty because the only blocked addons have already been dismissed by the user.
  assertXPIDatabaseBlocklistAttentionAddonIds([]);

  const addon1v2AfterRestart = await AddonManager.getAddonByID(ext1.id);
  Assert.equal(
    addon1v2AfterRestart.blocklistAttentionDismissed,
    false,
    "Expect addon1 blocklistAttentionDismissed to be false"
  );

  // addon2 was not updated and so we expect the blocklistAttentionDismissed flag to have been
  // loaded from the addonDB and still be set to true.
  const addon2 = await AddonManager.getAddonByID(ext2.id);
  Assert.equal(
    addon2.blocklistAttentionDismissed,
    true,
    "Expect addon2 blocklistAttentionDismissed to be true"
  );

  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
  await ext2.unload();
  await ext1.unload();
});

add_task(async function test_blocklist_attention_on_reenabled_softblock() {
  const ext1 = await installTestExtension("@ext1");

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [createStashRecord({ softblocked: [ext1] })],
    expectBeforeDismiss: {
      blocklistStates: {
        [ext1.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: false,
        addons: [ext1.id],
      },
      dismissedStates: {
        [ext1.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  const addon1 = await AddonManager.getAddonByID(ext1.id);
  Assert.equal(
    addon1.softDisabled,
    true,
    "Expect test addon to be softDisabled"
  );
  const promiseBlocklistAttentionUpdated = AddonTestUtils.promiseManagerEvent(
    "onBlocklistAttentionUpdated"
  );
  await addon1.enable();
  Assert.equal(
    addon1.softDisabled,
    false,
    "Expect test addon to NOT be softDisabled after being explicitly enabled"
  );

  info(
    "Wait for onBlocklistAttentionUpdated Manager listeners to have been called"
  );
  await promiseBlocklistAttentionUpdated;

  info(
    "Verify blocklistAttentionInfo again after softblocked addon has been re-enabled"
  );
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [ext1.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: false,
        hasSoftBlocked: false,
        hasHardBlocked: false,
        addons: [],
      },
      dismissedStates: {
        [ext1.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
  await ext1.unload();
});

add_task(async function test_attention_after_uninstalls() {
  const ext1 = await installTestExtension("@ext1");
  const ext2 = await installTestExtension("@ext2");

  let managerListener = {
    onBlocklistAttentionUpdated: sinon.spy(),
  };
  AddonManager.addManagerListener(managerListener);

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [
      createStashRecord({ blocked: [ext1], softblocked: [ext2] }),
    ],
    expectBeforeDismiss: {
      blocklistStates: {
        [ext1.id]: STATE_BLOCKED,
        [ext2.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: true,
        addons: [ext1.id, ext2.id],
      },
      dismissedStates: {
        [ext1.id]: false,
        [ext2.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  Assert.equal(
    managerListener.onBlocklistAttentionUpdated.callCount,
    2,
    "Expect onBlocklistAttentionUpdated to have been called once for each of the blocked extensions"
  );

  const addon1 = await AddonManager.getAddonByID(ext1.id);
  managerListener.onBlocklistAttentionUpdated.resetHistory();
  await addon1.uninstall();

  Assert.equal(
    managerListener.onBlocklistAttentionUpdated.callCount,
    1,
    "Expect onBlocklistAttentionUpdated to have been called once after ext1 uninstalled"
  );

  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [ext2.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: false,
        addons: [ext2.id],
      },
      dismissedStates: {
        [ext2.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  const addon2 = await AddonManager.getAddonByID(ext2.id);
  managerListener.onBlocklistAttentionUpdated.resetHistory();
  await addon2.uninstall();

  Assert.equal(
    managerListener.onBlocklistAttentionUpdated.callCount,
    1,
    "Expect onBlocklistAttentionUpdated to have been called again after ext2 uninstalled"
  );

  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {},
      blocklistAttentionInfo: {
        shouldShow: false,
        hasSoftBlocked: false,
        hasHardBlocked: false,
        addons: [],
      },
      dismissedStates: {},
      shouldShowBlocklistAttention: false,
    },
  });

  AddonManager.removeManagerListener(managerListener);
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
});

add_task(async function test_hidden_addons() {
  const extensionAddon = await installTestAddon("@extension", "extension");
  const hiddenAddon1 = await installTestAddon(
    "@hidden-extension1",
    "extension",
    {
      hidden: true,
    }
  );
  const hiddenAddon2 = await installTestAddon(
    "@hidden-extension2",
    "extension",
    {
      hidden: true,
    }
  );

  await testBlocklistAttentionScenario({
    blocklistStashRecords: [
      createStashRecord({
        blocked: [extensionAddon, hiddenAddon1],
        softblocked: [hiddenAddon2],
      }),
    ],
    expectBeforeDismiss: {
      blocklistStates: {
        [extensionAddon.id]: STATE_BLOCKED,
        [hiddenAddon1.id]: STATE_BLOCKED,
        [hiddenAddon2.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: false,
        hasHardBlocked: true,
        addons: [extensionAddon.id],
      },
      dismissedStates: {
        [extensionAddon.id]: false,
        [hiddenAddon1.id]: false,
        [hiddenAddon2.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
    expectAfterDismiss: {
      managerListenerCallCount: 1,
      blocklistAttentionInfo: {
        shouldShow: false,
      },
      dismissedStates: {
        [extensionAddon.id]: true,
        [hiddenAddon1.id]: false,
        [hiddenAddon2.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
  await hiddenAddon2.uninstall();
  await hiddenAddon1.uninstall();
  await extensionAddon.uninstall();
});

// Android builds do not support the other XPIProvider addon types.
add_task(
  { skip_if: () => IS_ANDROID_BUILD },
  async function test_other_addon_types() {
    const extensionAddon = await installTestAddon("@extension", "extension");
    const themeAddon = await installTestAddon("@theme", "theme");
    const localeAddon = await installTestAddon("@langpack", "locale");
    const dictionaryAddon = await installTestAddon("@dictionary", "dictionary");

    await testBlocklistAttentionScenario({
      blocklistStashRecords: [
        createStashRecord({
          blocked: [extensionAddon, localeAddon, dictionaryAddon],
          softblocked: [themeAddon],
        }),
      ],
      expectBeforeDismiss: {
        blocklistStates: {
          [extensionAddon.id]: STATE_BLOCKED,
          [themeAddon.id]: STATE_SOFTBLOCKED,
          [localeAddon.id]: STATE_BLOCKED,
          [dictionaryAddon.id]: STATE_BLOCKED,
        },
        blocklistAttentionInfo: {
          shouldShow: true,
          hasSoftBlocked: false,
          hasHardBlocked: true,
          addons: [extensionAddon.id],
        },
        dismissedStates: {
          [extensionAddon.id]: false,
          [themeAddon.id]: false,
          [localeAddon.id]: false,
          [dictionaryAddon.id]: false,
        },
        shouldShowBlocklistAttention: true,
      },
      expectAfterDismiss: {
        managerListenerCallCount: 1,
        blocklistAttentionInfo: {
          shouldShow: false,
        },
        dismissedStates: {
          [extensionAddon.id]: true,
          [themeAddon.id]: false,
          [localeAddon.id]: false,
          [dictionaryAddon.id]: false,
        },
        shouldShowBlocklistAttention: false,
      },
    });

    await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
    await dictionaryAddon.uninstall();
    await localeAddon.uninstall();
    await themeAddon.uninstall();
    await extensionAddon.uninstall();
  }
);

add_task(async function test_blocklist_attention_on_blocklist_updates() {
  let managerListener = {
    onBlocklistAttentionUpdated: sinon.spy(),
  };
  function assertManagerListenerCallCount(expectedCallCount, msg) {
    Assert.equal(
      managerListener.onBlocklistAttentionUpdated.callCount,
      expectedCallCount,
      msg
    );
    managerListener.onBlocklistAttentionUpdated.resetHistory();
  }
  AddonManager.addManagerListener(managerListener);

  const extensionAddon = await installTestAddon("@extension", "extension");
  assertManagerListenerCallCount(
    0,
    "Expect no onBlocklistAttentionUpdated calls on a non-blocked install"
  );

  info(
    "Simulate blocklistState change from STATE_NOT_BLOCKED to STATE_BLOCKED"
  );
  let stashRecords = [
    createStashRecord({ blocked: [extensionAddon], stash_time: 0 }),
  ];
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: stashRecords });
  assertManagerListenerCallCount(
    1,
    "Expect a onBlocklistAttentionUpdated calls on a new hard-block"
  );
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [extensionAddon.id]: STATE_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: false,
        hasHardBlocked: true,
        addons: [extensionAddon.id],
      },
      dismissedStates: {
        [extensionAddon.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  info(
    "Simulate blocklistState change from STATE_BLOCKED to STATE_SOFTBLOCKED"
  );
  stashRecords.push(
    createStashRecord({ softblocked: [extensionAddon], stash_time: 1 })
  );
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: stashRecords });
  // The disabled state of the addon didn't change and so we don't call the manager listener
  // because it doesn't change the results we would get by AddonManager.shouldShowBlocklistAttention()
  // (e.g. the attention dot wouldn't need to be refreshed).
  assertManagerListenerCallCount(
    0,
    "Expect no onBlocklistAttentionUpdated calls on hard-block to soft-block"
  );
  // On the contrary we still expect AddonManager.getBlocklistAttentionInfo to reflect the blocklistState
  // change.
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [extensionAddon.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: false,
        addons: [extensionAddon.id],
      },
      dismissedStates: {
        [extensionAddon.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  info(
    "Simulate blocklistState change from STATE_SOFTBLOCKED to STATE_BLOCKED"
  );
  stashRecords.push(
    createStashRecord({ blocked: [extensionAddon], stash_time: 2 })
  );
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: stashRecords });
  // Same as hard to soft block transistion, we expect soft to hard block to not be
  // triggering the onBlocklistAttentionUpdated if the addon was disabled also while
  // soft blocked.
  assertManagerListenerCallCount(
    0,
    "Expect no onBlocklistAttentionUpdated calls on soft-block to hard-block"
  );
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [extensionAddon.id]: STATE_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: false,
        hasHardBlocked: true,
        addons: [extensionAddon.id],
      },
      dismissedStates: {
        [extensionAddon.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  info(
    "Simulate blocklistState change from STATE_BLOCKED to STATE_NOT_BLOCKED"
  );
  stashRecords.push(
    createStashRecord({ unblocked: [extensionAddon], stash_time: 3 })
  );
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: stashRecords });
  assertManagerListenerCallCount(
    1,
    "Expect a onBlocklistAttentionUpdated calls on hard-block to not-blocked"
  );
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [extensionAddon.id]: STATE_NOT_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: false,
        hasSoftBlocked: false,
        hasHardBlocked: false,
        addons: [],
      },
      dismissedStates: {
        [extensionAddon.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  info(
    "Simulate blocklistState change from STATE_NOT_BLOCKED to STATE_SOFTBLOCKED"
  );
  stashRecords.push(
    createStashRecord({ softblocked: [extensionAddon], stash_time: 4 })
  );
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: stashRecords });
  assertManagerListenerCallCount(
    1,
    "Expect a onBlocklistAttentionUpdated calls on a new soft-block"
  );
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [extensionAddon.id]: STATE_SOFTBLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: true,
        hasSoftBlocked: true,
        hasHardBlocked: false,
        addons: [extensionAddon.id],
      },
      dismissedStates: {
        [extensionAddon.id]: false,
      },
      shouldShowBlocklistAttention: true,
    },
  });

  info(
    "Simulate blocklistState change from STATE_SOFTBLOCKED to STATE_NOT_BLOCKED"
  );
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
  assertManagerListenerCallCount(
    1,
    "Expect a onBlocklistAttentionUpdated calls on soft-block to not-blocked"
  );
  await testBlocklistAttentionScenario({
    expectBeforeDismiss: {
      blocklistStates: {
        [extensionAddon.id]: STATE_NOT_BLOCKED,
      },
      blocklistAttentionInfo: {
        shouldShow: false,
        hasSoftBlocked: false,
        hasHardBlocked: false,
        addons: [],
      },
      dismissedStates: {
        [extensionAddon.id]: false,
      },
      shouldShowBlocklistAttention: false,
    },
  });

  AddonManager.removeManagerListener(managerListener);
  await extensionAddon.uninstall();
});

add_task(async function test_blocklist_attention_on_blocked_install() {
  const SIGNED_ADDON_XPI_FILE = do_get_file("amosigned.xpi");
  const SIGNED_ADDON_ID = "amosigned-xpi@tests.mozilla.org";
  const SIGNED_ADDON_VERSION = "2.2";
  const SIGNED_ADDON_KEY = `${SIGNED_ADDON_ID}:${SIGNED_ADDON_VERSION}`;

  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash: {
          blocked: [],
          softblocked: [SIGNED_ADDON_KEY],
          unblocked: [],
        },
        stash_time: Date.now(),
      },
    ],
  });

  let managerListener = {
    onBlocklistAttentionUpdated: sinon.spy(),
  };

  AddonManager.addManagerListener(managerListener);

  const install = await promiseInstallFile(SIGNED_ADDON_XPI_FILE);
  Assert.equal(
    install.error,
    AddonManager.ERROR_SOFT_BLOCKED,
    "Install should have an error"
  );
  Assert.equal(
    managerListener.onBlocklistAttentionUpdated.callCount,
    0,
    "Expect no onBlocklistAttentionUpdated calls on blocked installs"
  );
  assertXPIDatabaseBlocklistAttentionAddonIds([]);
  await assertBlocklistAttentionInfo({
    addons: [],
    shouldShow: false,
    hasSoftBlocked: false,
    hasHardBlocked: false,
  });
  Assert.ok(
    !AddonManager.shouldShowBlocklistAttention(),
    "Expect shouldShowBlocklistAttention to be false"
  );

  AddonManager.removeManagerListener(managerListener);
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
});
