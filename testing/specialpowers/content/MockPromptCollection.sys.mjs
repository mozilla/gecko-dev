/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  WrapPrivileged: "resource://testing-common/WrapPrivileged.sys.mjs",
});

const Cm = Components.manager;

const CONTRACT_ID = "@mozilla.org/embedcomp/prompt-collection;1";

Cu.crashIfNotInAutomation();

var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
var oldClassID = "";
var newClassID = Services.uuid.generateUUID();
var newFactory = function (window) {
  return {
    createInstance(aIID) {
      return new MockPromptCollectionInstance(window).QueryInterface(aIID);
    },
    QueryInterface: ChromeUtils.generateQI(["nsIFactory"]),
  };
};

export var MockPromptCollection = {
  init(browsingContext) {
    this.window = browsingContext.window;

    this.reset();
    this.factory = newFactory(this.window);
    if (!registrar.isCIDRegistered(newClassID)) {
      oldClassID = registrar.contractIDToCID(CONTRACT_ID);
      registrar.registerFactory(newClassID, "", CONTRACT_ID, this.factory);
    }
  },

  reset() {
    this.returnBeforeUnloadCheck = false;
    this.returnConfirmRepost = false;
    this.returnConfirmFolderUpload = false;
    this.showConfirmFolderUploadCallback = null;
  },

  cleanup() {
    var previousFactory = this.factory;
    this.reset();
    this.factory = null;

    registrar.unregisterFactory(newClassID, previousFactory);
    if (oldClassID != "") {
      registrar.registerFactory(oldClassID, "", CONTRACT_ID, null);
    }
  },
};

function MockPromptCollectionInstance(window) {
  this.window = window;
  this.showConfirmFolderUploadCallback = null;
  this.showConfirmFolderUploadCallbackWrapped = null;
  this.directoryName = "";
}

MockPromptCollectionInstance.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIPromptCollection"]),

  asyncBeforeUnloadCheck() {
    return Promise.resolve(MockPromptCollection.returnBeforeUnloadCheck);
  },

  confirmRepost() {
    return MockPromptCollection.returnConfirmRepost;
  },

  confirmFolderUpload(aBrowsingContext, aDirectoryName) {
    this.directoryName = aDirectoryName;

    if (
      typeof MockPromptCollection.showConfirmFolderUploadCallback == "function"
    ) {
      if (
        MockPromptCollection.showConfirmFolderUploadCallback !=
        this.showConfirmFolderUploadCallback
      ) {
        this.showConfirmFolderUploadCallback =
          MockPromptCollection.showConfirmFolderUploadCallback;
        if (Cu.isXrayWrapper(this.window)) {
          this.showConfirmFolderUploadCallbackWrapped =
            lazy.WrapPrivileged.wrapCallback(
              MockPromptCollection.showConfirmFolderUploadCallback,
              this.window
            );
        } else {
          this.showConfirmFolderUploadCallbackWrapped =
            this.showConfirmFolderUploadCallback;
        }
      }

      try {
        let returnValue = this.showConfirmFolderUploadCallbackWrapped(this);
        if (typeof returnValue == "boolean") {
          return returnValue;
        }
      } catch (e) {
        return false;
      }
    }

    return MockPromptCollection.returnConfirmFolderUpload;
  },
};
