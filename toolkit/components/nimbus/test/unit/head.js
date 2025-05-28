"use strict";

/* import-globals-from ../../../../../toolkit/profile/test/xpcshell/head.js */
/* import-globals-from ../../../../../browser/components/profiles/tests/unit/head.js */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const {
  _ExperimentFeature: ExperimentFeature,
  ExperimentAPI,
  NimbusFeatures,
} = ChromeUtils.importESModule("resource://nimbus/ExperimentAPI.sys.mjs");
const { NimbusTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  RegionTestUtils: "resource://testing-common/RegionTestUtils.sys.mjs",
});

NimbusTestUtils.init(this);

add_setup(async function () {
  do_get_profile();

  await initSelectableProfileService();

  // TODO(bug 1967779): require the ProfilesDatastoreService to be initialized
  Services.prefs.setBoolPref("nimbus.profilesdatastoreservice.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref(
      "nimbus.profilesdatastoreservice.enabled",
      false
    );
  });
});

/**
 * Assert the manager has no active pref observers.
 */
function assertNoObservers(manager) {
  Assert.equal(
    manager._prefs.size,
    0,
    "There should be no active pref observers"
  );
  Assert.equal(
    manager._prefsBySlug.size,
    0,
    "There should be no active pref observers"
  );
  Assert.equal(
    manager._prefFlips._registeredPrefCount,
    0,
    "There should be no prefFlips pref observers"
  );
}

/**
 * Remove all pref observers on the given ExperimentManager.
 */
function removePrefObservers(manager) {
  for (const [name, entry] of manager._prefs.entries()) {
    Services.prefs.removeObserver(name, entry.observer);
  }

  manager._prefs.clear();
  manager._prefsBySlug.clear();
}
