/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cm = Components.manager;

const CONTRACT_ID = "@mozilla.org/sound;1";

Cu.crashIfNotInAutomation();

var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
var oldClassID = "";
var newClassID = Services.uuid.generateUUID();
var newFactory = function (window) {
  return {
    createInstance(aIID) {
      return new MockSoundInstance(window).QueryInterface(aIID);
    },
    QueryInterface: ChromeUtils.generateQI(["nsIFactory"]),
  };
};

export var MockSound = {
  _played: [],

  get played() {
    return this._played;
  },

  init() {
    this.reset();
    this.factory = newFactory();
    if (!registrar.isCIDRegistered(newClassID)) {
      try {
        oldClassID = registrar.contractIDToCID(CONTRACT_ID);
      } catch (ex) {
        oldClassID = "";
        dump(
          "TEST-INFO | can't get sound registered component, " +
            "assuming there is none"
        );
      }
      registrar.registerFactory(newClassID, "", CONTRACT_ID, this.factory);
    }
  },

  reset() {
    this._played = [];
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

function MockSoundInstance() {}
MockSoundInstance.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsISound"]),

  play(aURL) {
    MockSound._played.push(`(uri)${aURL.spec}`);
  },

  beep() {
    MockSound._played.push("beep");
  },

  init() {},

  playEventSound(aEventId) {
    MockSound._played.push(`(event)${aEventId}`);
  },
};
