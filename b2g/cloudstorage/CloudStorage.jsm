/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Sample.jsm');
Cu.import('resource://gre/modules/Dropbox.jsm');
Cu.import('resource://gre/modules/udManager.jsm');
Cu.import('resource://gre/modules/MetaCache.jsm');
Cu.import('resource://gre/modules/DataCache.jsm');
const { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});

let CloudStorage = {
  init: function cloudstorage_init(){
    console.log('cloudstorage_init');
    this.status = false;

    udManager.init({
      webStorageModule: Dropbox,
      metaCacheModule: MetaCache,
      dataCacheModule: DataCache
    });

  },
  enable: function cloudstorage_enable() {
    console.log('cloudstorage_enable');
    if (!this.status) {
      this.status = true;

      udManager.getFileMeta('/', function (error, response) {
        console.log(JSON.stringify(response.data));
      });

      udManager.getFileList('/', function (error, response) {
        console.log(JSON.stringify(response.data));
      });

      console.log('CloudStorage is enabled.');
    } else {
      console.log('CloudStorage had already been enabled.');
    }
  },
  disable: function cloudstorage_disable() {
    console.log('cloudstorage_disable');
    if (this.status) {
      this.status = false;
      console.log('CloudStorage is disabled.');
    } else {
      console.log('CloudStorage had already been disabled.');
    }
  },
  toggle: function cloudstorage_toggle() {
    console.log('cloudstorage_toggle');
    this.status ? this.disable() : this.enable();
  }
};
this.EXPORTED_SYMBOLS = ['CloudStorage'];
this.CloudStorage = CloudStorage;
