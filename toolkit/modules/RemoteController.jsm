// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteController"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function RemoteController(browser)
{
  this._browser = browser;

  // A map of commands that have had their enabled/disabled state assigned. The
  // value of each key will be true if enabled, and false if disabled.
  this._supportedCommands = { };
}

RemoteController.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIController,
                                         Ci.nsICommandController]),

  isCommandEnabled: function(aCommand) {
    return this._supportedCommands[aCommand] || false;
  },

  supportsCommand: function(aCommand) {
    return aCommand in this._supportedCommands;
  },

  doCommand: function(aCommand) {
    this._browser.messageManager.sendAsyncMessage("ControllerCommands:Do", aCommand);
  },

  getCommandStateWithParams: function(aCommand, aCommandParams) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  doCommandWithParams: function(aCommand, aCommandParams) {
    let cmd = {
      cmd: aCommand,
      params: null
    };
    if (aCommand == "cmd_lookUpDictionary") {
      // Although getBoundingClientRect of the element is logical pixel, but
      // x and y parameter of cmd_lookUpDictionary are device pixel.
      // So we need calculate child process's coordinate using correct unit.
      let rect = this._browser.getBoundingClientRect();
      let scale = this._browser.ownerDocument.defaultView.devicePixelRatio;
      cmd.params = {
        x:  {
          type: "long",
          value: aCommandParams.getLongValue("x") - rect.left * scale
        },
        y: {
          type: "long",
          value: aCommandParams.getLongValue("y") - rect.top * scale
        }
      };
    } else {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }
    this._browser.messageManager.sendAsyncMessage(
      "ControllerCommands:DoWithParams", cmd);
  },

  getSupportedCommands: function(aCount, aCommands) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  onEvent: function () {},

  // This is intended to be called from the remote-browser binding to update
  // the enabled and disabled commands.
  enableDisableCommands: function(aAction,
                                  aEnabledLength, aEnabledCommands,
                                  aDisabledLength, aDisabledCommands) {
    // Clear the list first
    this._supportedCommands = { };

    for (let c = 0; c < aEnabledLength; c++) {
      this._supportedCommands[aEnabledCommands[c]] = true;
    }

    for (let c = 0; c < aDisabledLength; c++) {
      this._supportedCommands[aDisabledCommands[c]] = false;
    }

    this._browser.ownerDocument.defaultView.updateCommands(aAction);
  }
};
