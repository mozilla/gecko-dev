/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import('resource://gre/modules/Sample.jsm');
Components.utils.import('resource://gre/modules/Dropbox.jsm');
Components.utils.import('resource://gre/modules/udManager.jsm');
Components.utils.import('resource://gre/modules/MetaCache.jsm');
Components.utils.import('resource://gre/modules/DataCache.jsm');

XPCOMUtils.defineLazyModuleGetter(this, "CloudStorageUnidiskManager",
                                  "resource://gre/modules/udManager.jsm");

function log(msg) {
  dump('CloudStorage: ' + msg + '\n');
}

function nsCloudStorageInterface() { 
  log("nsCloudStorageInterface constructor");
  log("call udManager.init()");
  udManager.init({
      accessToken:
        'kR1EcZML0N4AAAAAAAAAR7FkWBgDSVFk17g3--pNIBv0sR_gs85HlSkHAo8q_6-N',
      webStorageModule: Sample,
      metaCacheModule: MetaCache,
      dataCacheModule: DataCache
  })
}

nsCloudStorageInterface.prototype = {
  classDescription: "Cloud storage javascript XPCOM Component",
  classID:          Components.ID("{2A478E9A-FF9D-11E4-BA25-AC271E5D46B0}"),
  contractID:       "@mozilla.org/cloudstorageinterface;1",
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsICloudStorageInterface]),
  getFileMeta: function(cloudname, path) {
    log("cloudname: " + cloudname + " " + "path: " + path);
    log("call udManager.getFileMeta");
    udManager.getFileMeta(path, function(error, response) {
      log(JSON.stringify(response.data));
      var cls, instance;
      cls = Components.classes["@mozilla.org/cloudstoragegeckointerface;1"];
      instance = cls.createInstance(Components.interfaces.nsICloudStorageGeckoInterface);
      if (response.data) {
        instance.setFileMeta(cloudname, path, response.data.list[0].isdir, response.data.list[0].size, response.data.list[0].mtime, response.data.list[0].ctime);
      }
      instance.finishRequest(cloudname);
    });
  },
  
  getFileList: function(cloudname, path) {
    log("cloudname: " + cloudname + " " + "path: " + path);
    log("call udManager.getFileList");
    udManager.getFileList(path, function (error, response) {
      log(JSON.stringify(response.data));
      var cls, instance;
      cls = Components.classes["@mozilla.org/cloudstoragegeckointerface;1"];
      instance = cls.createInstance(Components.interfaces.nsICloudStorageGeckoInterface);
      for (var pathIdx = 0; pathIdx < response.data.list.length; pathIdx++ ) {
         var fileData = response.data.list[pathIdx];
         instance.setFileList(cloudname, path, fileData.path, fileData.isdir, fileData.size, fileData.mtime, fileData.ctime);
      }
      instance.finishRequest(cloudname);
    });
  },

  getData: function(cloudname, path, size, offset) {
    log("cloudname: " + cloudname + " " + "path: " + path);
    log("call udManager.downloadFileInRange");
    var buffer = new Uint8Array(size);
    udManager.downloadFileInRangeByCache(path, buffer, offset, size, function () {
      var cls, instance;
      cls = Components.classes["@mozilla.org/cloudstoragegeckointerface;1"];
      for (var idx = 0; idx < 10; ++idx) {
        log("buffer["+idx+"]: 0x"+buffer[idx]);
      }
      instance = cls.createInstance(Components.interfaces.nsICloudStorageGeckoInterface);
      instance.setData(cloudname, buffer, buffer.byteLength);
      instance.finishRequest(cloudname);
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsCloudStorageInterface]);
