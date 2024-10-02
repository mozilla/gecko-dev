/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const { STATE_NOT_BLOCKED, STATE_SOFTBLOCKED, STATE_BLOCKED } =
  Ci.nsIBlocklistService;
const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();
const { RS_ATTACHMENT_ID, RS_ATTACHMENT_TYPE, RS_SOFTBLOCKS_ATTACHMENT_ID } =
  ExtensionBlocklistMLBF;

// A set of blockKeys that are expect to be checked by some of
// the test task in this test file, but that shouldn't be
// found in the bloom filter data even if they are being checked.
const MLBF_MOCK_BLOCKS = {
  [RS_ATTACHMENT_ID]: new Set(),
  [RS_SOFTBLOCKS_ATTACHMENT_ID]: new Set(),
};

const getBlockKey = addon => `${addon.id}:${addon.version}`;
function clearMLBFMockBlocks() {
  MLBF_MOCK_BLOCKS[RS_ATTACHMENT_ID].clear();
  MLBF_MOCK_BLOCKS[RS_SOFTBLOCKS_ATTACHMENT_ID].clear();
}
// An helper function called by tests that needs to mocks MLBF results
// (e.g. calling `mockMLBFBlocks({ "@onlyblockedbymlbf:1": STATE_BLOCKED });`
// will make the hard block MLBF CascadeFilter to return true for the
// given blockKey).
function mockMLBFBlocks(blockStatePerBlockKey) {
  clearMLBFMockBlocks();
  for (const [blockKey, blocklistState] of Object.entries(
    blockStatePerBlockKey
  )) {
    const blocklistStates = Array.isArray(blocklistState)
      ? blocklistState
      : [blocklistState];
    for (const blockState of blocklistStates) {
      switch (blockState) {
        case STATE_NOT_BLOCKED:
          // clearMLBFMockBlocks would have already delete any
          // existing entry.
          break;
        case STATE_SOFTBLOCKED:
          MLBF_MOCK_BLOCKS[RS_SOFTBLOCKS_ATTACHMENT_ID].add(blockKey);
          break;
        case STATE_BLOCKED:
          MLBF_MOCK_BLOCKS[RS_ATTACHMENT_ID].add(blockKey);
          break;
        default:
          throw new Error(
            `Unexpected blocklistState value ${blockState} for blockKey ${blockKey}`
          );
      }
    }
  }
}

let MLBF_CHECKED_BLOCKS = [];
let MLBF_LOAD_ATTEMPTS = [];
const mockGetMLBFData = async (record, attachmentId, _mlbfData) => {
  MLBF_LOAD_ATTEMPTS.push(record);
  return {
    generationTime: record?.generation_time ?? 0,
    cascadeFilter: {
      has(blockKey) {
        info(`MLBF data ${attachmentId} being checked for ${blockKey}`);
        MLBF_CHECKED_BLOCKS.push([attachmentId, blockKey]);
        return MLBF_MOCK_BLOCKS[attachmentId].has(blockKey);
      },
    },
  };
};
ExtensionBlocklistMLBF._getMLBFData = mockGetMLBFData;

function assertMLBFBlockChecks(expected, msg) {
  Assert.deepEqual(
    MLBF_CHECKED_BLOCKS,
    expected,
    msg ?? "Found the expected entries in MLBF_CHECKED_BLOCKS"
  );
  MLBF_CHECKED_BLOCKS = [];
}
function assertNoRemainingMLBFBlockChecks() {
  assertMLBFBlockChecks(
    [],
    "Expect no unchecked MLBF_CHECKED_BLOCKS entries after all tests have been executed"
  );
}
registerCleanupFunction(assertNoRemainingMLBFBlockChecks);

async function checkBlockState(addonId, version, expectBlocked) {
  let addon = {
    id: addonId,
    version,
    // Note: signedDate is missing, so the MLBF does not apply
    // and we will effectively only test stashing.
  };
  let state = await Blocklist.getAddonBlocklistState(addon);
  if (expectBlocked) {
    Assert.equal(state, Ci.nsIBlocklistService.STATE_BLOCKED);
  } else {
    Assert.equal(state, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  }
}

add_setup(async function setup() {
  await promiseStartupManager();

  // Prevent intermittent failures to be hit due to the Blocklist trying to call
  // AddonManager.getAddonsByTypes (side-effect of the "extensions.blocklist.softblock.enabled"
  // pref being reset right after test tasks using the pref_set add_task option are
  // exiting) after the AddonManager instance started here in the setup task has been
  // already shutdown .
  registerCleanupFunction(async () => {
    await ExtensionBlocklistMLBF._updatePromise;
  });

  // Clear any existing data from the collection (needed so that running a single test
  // task in isolation doesn't hit failures because of ExtensionBlocklistMLBF attempting
  // to load the MLBF data based on the RemoteSettings data dump).
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [] });
  MLBF_LOAD_ATTEMPTS = [];
});

// Tests that add-ons can be blocked / unblocked via the stash.
add_task(async function basic_stash() {
  mockMLBFBlocks({ "@onlyblockedbymlbf:1": STATE_BLOCKED });
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash_time: 0,
        stash: {
          blocked: ["@blocked:1"],
          unblocked: ["@notblocked:2"],
        },
      },
    ],
  });
  await checkBlockState("@blocked", "1", true);
  await checkBlockState("@notblocked", "2", false);
  // Not in stash (but unsigned, so shouldn't reach MLBF):
  await checkBlockState("@blocked", "2", false);
  assertNoRemainingMLBFBlockChecks();

  Assert.equal(
    await Blocklist.getAddonBlocklistState({
      id: "@onlyblockedbymlbf",
      version: "1",
      signedDate: new Date(0), // = the MLBF's generationTime.
      signedState: AddonManager.SIGNEDSTATE_SIGNED,
    }),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "falls through to MLBF if entry is not found in stash"
  );
  assertMLBFBlockChecks(
    [[RS_ATTACHMENT_ID, "@onlyblockedbymlbf:1"]],
    "Expect the hard-blocks MLBF data to have been checked"
  );

  Assert.deepEqual(
    MLBF_LOAD_ATTEMPTS,
    Blocklist.isSoftBlockEnabled ? [null, null] : [null],
    "MLBF attachment not found"
  );
});

// To complement the privileged_xpi_not_blocked in test_blocklist_mlbf.js,
// verify that privileged add-ons can still be blocked through stashes.
add_task(async function privileged_addon_blocked_by_stash() {
  mockMLBFBlocks({ "@sysaddonblocked-mlbf:1": STATE_BLOCKED });
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash_time: 0,
        stash: {
          blocked: ["@sysaddonblocked:1"],
          unblocked: [],
          softblocked: [],
        },
      },
    ],
  });
  const system_addon = {
    id: "@sysaddonblocked",
    version: "1",
    signedDate: new Date(0), // = the MLBF's generationTime.
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
  };

  Assert.equal(
    await Blocklist.getAddonBlocklistState(system_addon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Privileged add-ons can still be blocked by a stash"
  );

  system_addon.signedState = AddonManager.SIGNEDSTATE_SYSTEM;
  Assert.equal(
    await Blocklist.getAddonBlocklistState(system_addon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Privileged system add-ons can still be blocked by a stash"
  );
  // Assert that the MLBF data has not been checked.
  assertNoRemainingMLBFBlockChecks();

  // For comparison, when an add-on is only blocked by a MLBF, the block
  // decision is ignored.
  system_addon.id = "@sysaddonblocked-mlbf";
  Assert.equal(
    await Blocklist.getAddonBlocklistState(system_addon),
    Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    "Privileged add-ons cannot be blocked via a MLBF"
  );
  assertMLBFBlockChecks(
    [
      [RS_ATTACHMENT_ID, "@sysaddonblocked-mlbf:1"],
      [RS_SOFTBLOCKS_ATTACHMENT_ID, "@sysaddonblocked-mlbf:1"],
    ],
    "Expect the hard-blocks and soft-blocks MLBF data to have been checked"
  );
  // (note that we haven't checked that SIGNEDSTATE_PRIVILEGED is not blocked
  // via the MLBF, but that is already covered by test_blocklist_mlbf.js ).
});

// To complement langpack_not_blocked_on_Nightly in test_blocklist_mlbf.js,
// verify that langpacks can still be blocked through stashes.
add_task(async function langpack_blocked_by_stash() {
  mockMLBFBlocks({ "@langpackblocked-mlbf:1": STATE_BLOCKED });
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash_time: 0,
        stash: {
          blocked: ["@langpackblocked:1"],
          unblocked: [],
          softblocked: [],
        },
      },
    ],
  });
  const langpack_addon = {
    id: "@langpackblocked",
    type: "locale",
    version: "1",
    signedDate: new Date(0), // = the MLBF's generationTime.
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
  };
  Assert.equal(
    await Blocklist.getAddonBlocklistState(langpack_addon),
    Ci.nsIBlocklistService.STATE_BLOCKED,
    "Langpack add-ons can still be blocked by a stash"
  );
  assertNoRemainingMLBFBlockChecks();

  // For comparison, when an add-on is only blocked by a MLBF, the block
  // decision is ignored on Nightly (but blocked on non-Nightly).
  langpack_addon.id = "@langpackblocked-mlbf";
  if (AppConstants.NIGHTLY_BUILD) {
    Assert.equal(
      await Blocklist.getAddonBlocklistState(langpack_addon),
      Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
      "Langpack add-ons cannot be blocked via a MLBF on Nightly"
    );
    assertMLBFBlockChecks(
      [
        [RS_ATTACHMENT_ID, "@langpackblocked-mlbf:1"],
        [RS_SOFTBLOCKS_ATTACHMENT_ID, "@langpackblocked-mlbf:1"],
      ],
      "Expect the hard-blocks and soft-blocks MLBF data to have been checked"
    );
  } else {
    Assert.equal(
      await Blocklist.getAddonBlocklistState(langpack_addon),
      Ci.nsIBlocklistService.STATE_BLOCKED,
      "Langpack add-ons can be blocked via a MLBF on non-Nightly"
    );
    assertMLBFBlockChecks(
      [[RS_ATTACHMENT_ID, "@langpackblocked-mlbf:1"]],
      "Expect the hard-blocks MLBF data to have been checked"
    );
  }
});

// Tests that invalid stash entries are ignored.
add_task(async function invalid_stashes() {
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {},
      { stash: null },
      { stash: 1 },
      { stash: {} },
      { stash: { blocked: ["@broken:1", "@okid:1"] } },
      { stash: { unblocked: ["@broken:2"] } },
      // The only correct entry:
      { stash: { blocked: ["@okid:2"], unblocked: ["@okid:1"] } },
      { stash: { blocked: ["@broken:1", "@okid:1"] } },
      { stash: { unblocked: ["@broken:2", "@okid:2"] } },
    ],
  });
  // The valid stash entry should be applied:
  await checkBlockState("@okid", "1", false);
  await checkBlockState("@okid", "2", true);
  // Entries from invalid stashes should be ignored:
  await checkBlockState("@broken", "1", false);
  await checkBlockState("@broken", "2", false);

  assertNoRemainingMLBFBlockChecks();
});

// Blocklist stashes should be processed in the reverse chronological order.
add_task(async function stash_time_order() {
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      // "@a:1" and "@a:2" are blocked at time 1, but unblocked later.
      { stash_time: 2, stash: { blocked: [], unblocked: ["@a:1"] } },
      { stash_time: 1, stash: { blocked: ["@a:1", "@a:2"], unblocked: [] } },
      { stash_time: 3, stash: { blocked: [], unblocked: ["@a:2"] } },

      // "@b:1" and "@b:2" are unblocked at time 4, but blocked later.
      { stash_time: 5, stash: { blocked: ["@b:1"], unblocked: [] } },
      { stash_time: 4, stash: { blocked: [], unblocked: ["@b:1", "@b:2"] } },
      { stash_time: 6, stash: { blocked: ["@b:2"], unblocked: [] } },
    ],
  });
  await checkBlockState("@a", "1", false);
  await checkBlockState("@a", "2", false);

  await checkBlockState("@b", "1", true);
  await checkBlockState("@b", "2", true);

  assertNoRemainingMLBFBlockChecks();
});

// Attachments with unsupported attachment_type should be ignored.
add_task(async function mlbf_bloomfilter_full_ignored() {
  MLBF_LOAD_ATTEMPTS.length = 0;

  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [{ attachment_type: "bloomfilter-full", attachment: {} }],
  });

  // Only bloomfilter-base records should be used.
  // Since there are no such records, we shouldn't find anything.
  Assert.deepEqual(
    MLBF_LOAD_ATTEMPTS,
    Blocklist.isSoftBlockEnabled ? [null, null] : [null],
    "no matching MLBFs found"
  );
});

// Tests that the most recent MLBF is downloaded.
add_task(async function mlbf_generation_time_recent() {
  MLBF_LOAD_ATTEMPTS.length = 0;
  const records = [
    { attachment_type: "bloomfilter-base", attachment: {}, generation_time: 2 },
    { attachment_type: "bloomfilter-base", attachment: {}, generation_time: 3 },
    { attachment_type: "bloomfilter-base", attachment: {}, generation_time: 1 },
  ];
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: records });
  Assert.equal(
    MLBF_LOAD_ATTEMPTS[0].generation_time,
    3,
    "expected to load most recent MLBF"
  );
});

async function test_stashes_vs_mlbf_data_timestamps({ softBlockEnabled }) {
  // Sanity check.
  Assert.equal(
    Blocklist.isSoftBlockEnabled,
    softBlockEnabled,
    `Expect soft-blocks feature to be ${
      softBlockEnabled ? "enabled" : "disabled"
    }`
  );

  const runBlocklistTest = async ({
    testCase,
    testSetup: { addon, records, mlbfBlocks },
    expected: { mlbfBlockChecks, blocklistState },
  }) => {
    info(`===== Running test case: ${testCase}`);
    // Sanity checks all records should have a stash_time or generation_time
    // property set.
    Assert.deepEqual(
      records.filter(
        record => record.stash_time == null && record.generation_time == null
      ),
      [],
      "All testSetup records should have a timestamp"
    );
    MLBF_LOAD_ATTEMPTS = [];
    await AddonTestUtils.loadBlocklistRawData({
      extensionsMLBF: [...records],
    });
    // Sanity checks (assert the expected mock MLBF data has been loaded,
    // and the testAddon signedDate and signedState are set as expected).
    Assert.deepEqual(
      MLBF_LOAD_ATTEMPTS,
      softBlockEnabled
        ? records.filter(r => r.attachment)
        : records.filter(r => r.attachment_type === RS_ATTACHMENT_TYPE),
      "Got the expected mlbf load attempts"
    );
    Assert.notEqual(
      addon.signedDate,
      undefined,
      "testAddon signedDate should not be undefined"
    );
    Assert.equal(
      addon.signedState,
      AddonManager.SIGNEDSTATE_SIGNED,
      "testAddon signedState should be SIGNEDSTATE_SIGNED"
    );
    mockMLBFBlocks(mlbfBlocks);
    // Assert the resulting blocklist state.
    Assert.equal(
      await Blocklist.getAddonBlocklistState(addon),
      blocklistState,
      "Got the expected blocklist state"
    );
    // Assert that the mlbf data has been checked or ignored
    // as expected for the specific scenario being tested.
    assertMLBFBlockChecks(mlbfBlockChecks);
  };

  const baseAddon = {
    version: "1",
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
    // signedDate to be set in the calls to runBlocklistTest
  };
  const notInMLBFAddon = { id: "@not-in-mlbf-addon", ...baseAddon };
  const hardBlockedAddon = { id: "@mlbf-hardblock", ...baseAddon };
  const softBlockedAddon = { id: "@mlbf-softblock", ...baseAddon };

  const MLBF_RECORD = {
    attachment_type: ExtensionBlocklistMLBF.RS_ATTACHMENT_TYPE,
    attachment: {},
    // generation_time to be set in the calls to runBlocklistTest
  };
  const MLBF_SOFTBLOCK_RECORD = {
    attachment_type: ExtensionBlocklistMLBF.RS_SOFTBLOCKS_ATTACHMENT_TYPE,
    attachment: {},
    // generation_time to be set in the calls to runBlocklistTest
  };

  await runBlocklistTest({
    testCase: "hard-block stash and mlbf older than soft-blocks mlbf",
    testSetup: {
      addon: { ...softBlockedAddon, signedDate: new Date(0) },
      records: [
        {
          stash: {
            blocked: [getBlockKey(softBlockedAddon)],
            softblocked: [],
            unblocked: [],
          },
          stash_time: 10,
        },
        { ...MLBF_RECORD, generation_time: 10 },
        { ...MLBF_SOFTBLOCK_RECORD, generation_time: 20 },
      ],
      mlbfBlocks: {
        [getBlockKey(softBlockedAddon)]: STATE_SOFTBLOCKED,
      },
    },
    expected: {
      blocklistState: softBlockEnabled ? STATE_SOFTBLOCKED : STATE_BLOCKED,
      mlbfBlockChecks: softBlockEnabled
        ? [[RS_SOFTBLOCKS_ATTACHMENT_ID, getBlockKey(softBlockedAddon)]]
        : [],
    },
  });

  await runBlocklistTest({
    testCase:
      "unblocked stash and hard-blocks mlbf older than soft-blocks mlbf",
    testSetup: {
      addon: { ...softBlockedAddon, signedDate: new Date(0) },
      records: [
        {
          stash: {
            blocked: [],
            softblocked: [],
            unblocked: [getBlockKey(softBlockedAddon)],
          },
          stash_time: 20,
        },
        { ...MLBF_RECORD, generation_time: 10 },
        { ...MLBF_SOFTBLOCK_RECORD, generation_time: 30 },
      ],
      mlbfBlocks: {
        [getBlockKey(softBlockedAddon)]: STATE_SOFTBLOCKED,
      },
    },
    expected: {
      blocklistState: softBlockEnabled ? STATE_SOFTBLOCKED : STATE_NOT_BLOCKED,
      mlbfBlockChecks: softBlockEnabled
        ? [[RS_SOFTBLOCKS_ATTACHMENT_ID, getBlockKey(softBlockedAddon)]]
        : [],
    },
  });

  await runBlocklistTest({
    testCase: "soft-blocked stash more recent than soft-blocks mlbf",
    testSetup: {
      addon: { ...notInMLBFAddon, signedDate: new Date(0) },
      records: [
        {
          stash: {
            blocked: [],
            softblocked: [getBlockKey(notInMLBFAddon)],
            unblocked: [],
          },
          stash_time: 20,
        },
        { ...MLBF_RECORD, generation_time: 10 },
        { ...MLBF_SOFTBLOCK_RECORD, generation_time: 10 },
      ],
      mlbfBlocks: {},
    },
    expected: {
      blocklistState: softBlockEnabled ? STATE_SOFTBLOCKED : STATE_NOT_BLOCKED,
      // We expect the hard-block mlbf data to be checked if the soft-block stashes
      // are ignored due to soft-blocking feature being disabled through prefs.
      mlbfBlockChecks: softBlockEnabled
        ? []
        : [[RS_ATTACHMENT_ID, getBlockKey(notInMLBFAddon)]],
    },
  });

  await runBlocklistTest({
    testCase:
      "unblocked stash more recent than soft-blocks mlbf and as recent as hard-blocks mlbf",
    testSetup: {
      addon: { ...softBlockedAddon, signedDate: new Date(0) },
      records: [
        {
          stash: {
            blocked: [],
            softblocked: [],
            unblocked: [getBlockKey(softBlockedAddon)],
          },
          stash_time: 20,
        },
        { ...MLBF_RECORD, generation_time: 20 },
        { ...MLBF_SOFTBLOCK_RECORD, generation_time: 10 },
      ],
      mlbfBlocks: {
        [getBlockKey(softBlockedAddon)]: STATE_SOFTBLOCKED,
      },
    },
    expected: {
      blocklistState: STATE_NOT_BLOCKED,
      mlbfBlockChecks: [],
    },
  });

  await runBlocklistTest({
    testCase:
      "unblocked stash more recent than hard-blocks and older than soft-blocks mlbf",
    testSetup: {
      addon: { ...hardBlockedAddon, signedDate: new Date(0) },
      records: [
        {
          stash: {
            blocked: [],
            softblocked: [],
            unblocked: [getBlockKey(hardBlockedAddon)],
          },
          // Set a stash_time newer than the soft-blocks mlbf generation_time.
          stash_time: 20,
        },
        { ...MLBF_RECORD, generation_time: 10 },
        { ...MLBF_SOFTBLOCK_RECORD, generation_time: 30 },
      ],
      mlbfBlocks: {
        [getBlockKey(hardBlockedAddon)]: STATE_BLOCKED,
      },
    },
    expected: {
      blocklistState: STATE_NOT_BLOCKED,
      // We expect the soft-blocks mlbf data to be checked when enabled
      // because the stash timestamp is older than the softblock mlbf.
      mlbfBlockChecks: softBlockEnabled
        ? [[RS_SOFTBLOCKS_ATTACHMENT_ID, getBlockKey(hardBlockedAddon)]]
        : [],
    },
  });

  await runBlocklistTest({
    testCase: "stash and hard-blocks mlbf older than soft-blocks mlbf",
    testSetup: {
      addon: { ...hardBlockedAddon, signedDate: new Date(0) },
      records: [
        {
          stash: {
            blocked: [],
            softblocked: [],
            unblocked: [getBlockKey(hardBlockedAddon)],
          },
          // Set a stash_time older than both the mlbf generation_time.
          stash_time: 10,
        },
        // hard-blocks mlbf data older than soft-blocks mlbf data.
        { ...MLBF_RECORD, generation_time: 20 },
        { ...MLBF_SOFTBLOCK_RECORD, generation_time: 30 },
      ],
      mlbfBlocks: {
        // A block for the test extension is going to be found
        // in both soft-blocks and hard-blocks MLBF data.
        [getBlockKey(hardBlockedAddon)]: [STATE_BLOCKED, STATE_SOFTBLOCKED],
      },
    },
    expected: {
      // The soft-blocks mlbf data is more recent and so:
      // - if soft-blocks are enabled, then the addon is expected to be softblocked
      // - if soft-blocks are disabled, then the addon is expected to be unblocked
      //   (because the stash record is more recent than the hard-blocks mlbf data).
      blocklistState: softBlockEnabled ? STATE_SOFTBLOCKED : STATE_BLOCKED,
      mlbfBlockChecks: softBlockEnabled
        ? [
            [RS_ATTACHMENT_ID, getBlockKey(hardBlockedAddon)],
            [RS_SOFTBLOCKS_ATTACHMENT_ID, getBlockKey(hardBlockedAddon)],
          ]
        : [[RS_ATTACHMENT_ID, getBlockKey(hardBlockedAddon)]],
    },
  });
}

add_task(
  {
    pref_set: [["extensions.blocklist.softblock.enabled", true]],
  },
  function stashes_vs_mlbf_data_timestamps_on_softblock_enabled() {
    return test_stashes_vs_mlbf_data_timestamps({ softBlockEnabled: true });
  }
);

add_task(
  {
    pref_set: [["extensions.blocklist.softblock.enabled", false]],
  },
  function stashes_vs_mlbf_data_timestamps_on_softblock_enabled() {
    return test_stashes_vs_mlbf_data_timestamps({ softBlockEnabled: false });
  }
);
