// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const { utils: Cu, interfaces: Ci } = Components;
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

function CertDialogService() {}
CertDialogService.prototype = {
  classID: Components.ID("{a70153f2-3590-4317-93e9-73b3e7ffca5d}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICertificateDialogs]),

  getPKCS12FilePassword: function() {
    return true; // Simulates entering an empty password
  }
};

let Prompter = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),
  alert: function() {} // Do nothing when asked to show an alert
};

function WindowWatcherService() {}
WindowWatcherService.prototype = {
  classID: Components.ID("{01ae923c-81bb-45db-b860-d423b0fc4fe1}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowWatcher]),

  getNewPrompter: function() {
    return Prompter;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  CertDialogService,
  WindowWatcherService
]);
