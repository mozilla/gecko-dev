/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["XPIUtils"];
var exports = {};
var XPIUtils = exports;

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/sdk/Preferences.js");

const PREF_NATIVE_JETPACK = "extensions.addon-sdk.native.enabled";
const FILE_PACKAGE_JSON = "package.json";

function hasPackageJSON({ container }) {
  if (!Preferences.get(PREF_NATIVE_JETPACK, false)) {
    return false;
  }

  if (container instanceof Ci.nsIFile) {
    let file = container.clone();
    file.append(FILE_PACKAGE_JSON);
    return file.exists() && file.isFile();
  }

  if (container instanceof Ci.nsIZipReader) {
    return container.hasEntry(FILE_PACKAGE_JSON);
  }

  return false;
}
exports.hasPackageJSON = hasPackageJSON;
