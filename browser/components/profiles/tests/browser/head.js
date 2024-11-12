/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Sqlite } = ChromeUtils.importESModule(
  "resource://gre/modules/Sqlite.sys.mjs"
);

/**
 * A mock toolkit profile.
 */
class MockProfile {
  #service = null;
  #storeID = null;

  constructor(service) {
    this.#service = service;
    this.name = "Testing";
    this.rootDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    this.localDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    this.#storeID = null;
    this.showProfileSelector = false;
  }

  get storeID() {
    return this.#storeID;
  }

  set storeID(val) {
    this.#storeID = val;

    if (val) {
      this.#service.groupProfile = this;
    } else {
      this.#service.groupProfile = null;
    }
  }
}

/**
 * A mock profile service to use with the selectable profile service.
 */
class MockProfileService {
  constructor() {
    this.currentProfile = new MockProfile(this);
    this.groupProfile = null;
  }

  async asyncFlush() {}

  async asyncFlushCurrentProfile() {}
}

const gProfileService = new MockProfileService();

let testRoot = Services.dirsvc.get("ProfD", Ci.nsIFile);
testRoot.append(`SP${Date.now()}`);
try {
  testRoot.remove(true);
} catch (e) {
  if (e.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
    console.error(e);
  }
}
try {
  testRoot.create(Ci.nsIFile.DIRECTORY_TYPE, 0o777);
} catch (e) {
  console.error(e);
}

// UAppData must be above any profile folder.
let uAppData = Services.dirsvc.get("ProfD", Ci.nsIFile).parent;
let defProfRt = testRoot.clone();
defProfRt.append("DefProfRt");
let defProflLRt = testRoot.clone();
defProflLRt.append("DefProfLRt");

SelectableProfileService.overrideDirectoryService({
  UAppData: uAppData,
  DefProfRt: defProfRt,
  DefProfLRt: defProflLRt,
  ProfileGroups: testRoot.path,
});

async function openDatabase() {
  let dbFile = testRoot.clone();
  dbFile.append(`${gProfileService.currentProfile.storeID}.sqlite`);
  return Sqlite.openConnection({
    path: dbFile.path,
    openNotExclusive: true,
  });
}

add_setup(async () => {
  await SelectableProfileService.resetProfileService(gProfileService);

  registerCleanupFunction(async () => {
    SelectableProfileService.overrideDirectoryService(null);
    await SelectableProfileService.resetProfileService(null);
  });
});

async function initGroupDatabase() {
  await SelectableProfileService.maybeSetupDataStore();
}
