/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var tokenName;

function onLoad()
{
  if ("arguments" in window) {
    var params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    tokenName = params.GetString(1);
  } else {
    tokenName = self.name;
  }
}

function resetPassword()
{
  var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
  var token = pk11db.findTokenByName(tokenName);
  token.reset();

  try {
    var loginManager = Components.classes["@mozilla.org/login-manager;1"].
                       getService(Components.interfaces.nsILoginManager);
    loginManager.removeAllLogins();
  } catch (e) {
  }

  var bundle = document.getElementById("pippki_bundle");
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
  promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
  if (promptService && bundle) {
    promptService.alert(window,
                        bundle.getString("resetPasswordConfirmationTitle"),
                        bundle.getString("resetPasswordConfirmationMessage"));
  }

  return true;
}

