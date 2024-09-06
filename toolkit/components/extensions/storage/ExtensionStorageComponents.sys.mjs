/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  WebExtStorageStore: "resource://gre/modules/RustWebextstorage.sys.mjs",
});

function StorageSyncService() {}

StorageSyncService.prototype = {
  _storageAreaPromise: null,
  async getStorageAreaInstance() {
    if (!this._storageAreaPromise) {
      let path = PathUtils.join(PathUtils.profileDir, "storage-sync-v2.sqlite");
      this._storageAreaPromise = lazy.WebExtStorageStore.init(path);
    }

    return await this._storageAreaPromise;
  },
};

export var storageSyncService = new StorageSyncService();
