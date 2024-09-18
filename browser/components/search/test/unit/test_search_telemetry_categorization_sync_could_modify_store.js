/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the region logic in DomainToCategoriesMap.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  Region: "resource://gre/modules/Region.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

// For the tests, domains aren't checked, but add at least one value so the
// store isn't considered empty.
const DATA = [
  {
    "foo.com": [0, 0],
  },
];
const VERSION = 1;
const USER_REGION = "US";
const OTHER_REGION = "CA";

add_setup(async () => {
  do_get_profile();
  Services.prefs.setBoolPref(
    "browser.search.serpEventTelemetryCategorization.enabled",
    true
  );
  await SearchSERPDomainToCategoriesMap.init();
  await Region.init();
  let originalRegion = Region.home;
  Region._setHomeRegion(USER_REGION);
  registerCleanupFunction(() => {
    Region._setHomeRegion(originalRegion);
    Services.prefs.clearUserPref(
      "browser.search.serpEventTelemetryCategorization.enabled"
    );
  });
});

const TESTS = [
  /**
   * These tests set an empty map and apply an update.
   */
  {
    emptyMap: true,
    title: "Update other regions but not the user region",
    data: {
      current: [{ includeRegions: [OTHER_REGION] }],
      updated: [
        {
          old: { includeRegions: [OTHER_REGION] },
          new: { includeRegions: [OTHER_REGION] },
        },
      ],
      deleted: [],
      created: [],
    },
    expected: false,
  },
  {
    emptyMap: true,
    title: "Create default record",
    data: {
      current: [{ isDefault: true }],
      updated: [],
      deleted: [],
      created: [{ isDefault: true }],
    },
    expected: true,
  },
  {
    emptyMap: true,
    title: "Create custom record",
    data: {
      current: [{ includeRegions: [USER_REGION] }],
      updated: [],
      deleted: [],
      created: [{ includeRegions: [USER_REGION] }],
    },
    expected: true,
  },
  {
    emptyMap: true,
    title: "Create multiple matching records",
    data: {
      current: [{ includeRegions: [USER_REGION] }, { isDefault: true }],
      updated: [],
      deleted: [],
      created: [{ includeRegions: [USER_REGION] }, { isDefault: true }],
    },
    expected: true,
  },
  /**
   * Set map with default records and apply an update.
   */
  {
    isDefault: true,
    title: "Update other regions but not the user region",
    data: {
      current: [{ includeRegions: [OTHER_REGION] }, { isDefault: true }],
      updated: [
        {
          old: { includeRegions: [OTHER_REGION] },
          new: { includeRegions: [OTHER_REGION] },
        },
      ],
      deleted: [],
      created: [],
    },
    expected: false,
  },
  {
    isDefault: true,
    title: "Updated default record",
    data: {
      current: [{ isDefault: true }],
      updated: [{ old: { isDefault: true }, new: { isDefault: true } }],
      deleted: [],
      created: [],
    },
    expected: true,
  },
  {
    isDefault: true,
    title: "Deleted a default records",
    data: {
      current: [{ isDefault: true }],
      updated: [],
      deleted: [{ isDefault: true }],
      created: [],
    },
    expected: true,
  },
  {
    isDefault: true,
    title: "Deleted one of the default records and left a current region",
    data: {
      current: [{ includeRegions: [USER_REGION] }, { isDefault: true }],
      updated: [],
      deleted: [{ isDefault: true }],
      created: [{ includeRegions: [USER_REGION] }],
    },
    expected: true,
  },
  /**
   * Set map with custom records and apply an update.
   */
  {
    isDefault: false,
    title: "Created a default record",
    data: {
      current: [{ includeRegions: [USER_REGION] }, { isDefault: true }],
      updated: [],
      deleted: [],
      created: [{ isDefault: true }],
    },
    expected: false,
  },
  {
    isDefault: false,
    title: "Created an additional custom record",
    data: {
      current: [
        { includeRegions: [USER_REGION] },
        { includeRegions: [USER_REGION] },
        { isDefault: true },
      ],
      updated: [],
      deleted: [],
      created: [{ includeRegions: [USER_REGION] }],
    },
    expected: true,
  },
  {
    isDefault: false,
    title: "Deleted one of the default records",
    data: {
      current: [{ includeRegions: [USER_REGION] }, { isDefault: true }],
      updated: [],
      deleted: [{ isDefault: true }],
      created: [],
    },
    expected: false,
  },
  {
    isDefault: false,
    title: "Deleted all custom records",
    data: {
      current: [{ isDefault: true }],
      updated: [],
      deleted: [{ includeRegions: [USER_REGION] }],
      created: [],
    },
    expected: true,
  },
];

add_task(async function test_sync_may_modify_store() {
  for (let test of TESTS) {
    if (test.emptyMap) {
      await SearchSERPDomainToCategoriesMap.overrideMapForTests({}, 0, false);
    } else {
      await SearchSERPDomainToCategoriesMap.overrideMapForTests(
        DATA,
        VERSION,
        test.isDefault
      );
    }
    info(
      `Domain to Categories Map: ${
        SearchSERPDomainToCategoriesMap.empty ? "Empty" : "Has Existing Data"
      }.`
    );
    info(`${test.title}.`);
    let result = await SearchSERPDomainToCategoriesMap.syncMayModifyStore(
      test.data,
      USER_REGION
    );
    Assert.equal(result, test.expected, "Should modify store.");
  }
});
