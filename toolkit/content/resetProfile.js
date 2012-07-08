/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

// based on onImportItemsPageShow from migration.js
function onResetProfileLoad() {
#expand const MOZ_BUILD_APP = "__MOZ_BUILD_APP__";
#expand const MOZ_APP_NAME = "__MOZ_APP_NAME__";

  // From migration.properties
  const MIGRATED_TYPES = [
    "4_" + MOZ_APP_NAME + "_history_and_bookmarks",
    "16_" + MOZ_APP_NAME, // Passwords
    "8_" + MOZ_APP_NAME,  // Form History
    "2_" + MOZ_APP_NAME,  // Cookies
  ];

  var migratedItems = document.getElementById("migratedItems");
  var bundle = Services.strings.createBundle("chrome://" + MOZ_BUILD_APP +
                                             "/locale/migration/migration.properties");

  // Loop over possible data to migrate to give the user a list of what will be preserved.
  for (var itemStringName of MIGRATED_TYPES) {
    try {
      var checkbox = document.createElement("label");
      checkbox.setAttribute("value", bundle.GetStringFromName(itemStringName));
      migratedItems.appendChild(checkbox);
    } catch (x) {
      // Catch exceptions when the string for a data type doesn't exist.
      Components.utils.reportError(x);
    }
  }
}

function onResetProfileAccepted() {
  var retVals = window.arguments[0];
  retVals.reset = true;
}
