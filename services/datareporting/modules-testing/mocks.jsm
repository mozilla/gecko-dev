/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MockPolicyListener"];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");


this.MockPolicyListener = function MockPolicyListener() {
  this._log = Log.repository.getLogger("Services.DataReporting.Testing.MockPolicyListener");
  this._log.level = Log.Level["Debug"];

  this.requestDataUploadCount = 0;
  this.lastDataRequest = null;

  this.requestRemoteDeleteCount = 0;
  this.lastRemoteDeleteRequest = null;

  this.notifyUserCount = 0;
  this.lastNotifyRequest = null;
}

MockPolicyListener.prototype = {
  onRequestDataUpload: function (request) {
    this._log.info("onRequestDataUpload invoked.");
    this.requestDataUploadCount++;
    this.lastDataRequest = request;
  },

  onRequestRemoteDelete: function (request) {
    this._log.info("onRequestRemoteDelete invoked.");
    this.requestRemoteDeleteCount++;
    this.lastRemoteDeleteRequest = request;
  },

  onNotifyDataPolicy: function (request, rejectMessage=null) {
    this._log.info("onNotifyDataPolicy invoked.");
    this.notifyUserCount++;
    this.lastNotifyRequest = request;
    if (rejectMessage) {
      request.onUserNotifyFailed(rejectMessage);
    } else {
      request.onUserNotifyComplete();
    }
  },
};

