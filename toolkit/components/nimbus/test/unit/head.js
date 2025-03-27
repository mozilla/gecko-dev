"use strict";
// Globals

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
ChromeUtils.defineESModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ExperimentTestUtils: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  RegionTestUtils: "resource://testing-common/RegionTestUtils.sys.mjs",
});

RegionTestUtils.setNetworkRegion("US");

/**
 * Assert the store has no active experiments or rollouts.
 */
async function assertEmptyStore(store, { cleanup = false } = {}) {
  Assert.deepEqual(
    store
      .getAll()
      .filter(e => e.active)
      .map(e => e.slug),
    [],
    "Store should have no active enrollments"
  );

  store
    .getAll()
    .filter(e => !e.active)
    .forEach(e => store._deleteForTests(e.slug));

  Assert.deepEqual(
    store
      .getAll()
      .filter(e => !e.active)
      .map(e => e.slug),
    [],
    "Store should have no inactive enrollments"
  );

  if (cleanup) {
    // We need to call finalize first to ensure that any pending saves from
    // JSONFile.saveSoon overwrite files on disk.
    store._store.saveSoon();
    await store._store.finalize();
    await IOUtils.remove(store._store.path);
  }
}
