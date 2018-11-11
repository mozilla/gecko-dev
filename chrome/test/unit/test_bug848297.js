/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var MANIFESTS = [
  do_get_file("data/test_bug848297.manifest"),
];

// Stub in the locale service so we can control what gets returned as the OS locale setting
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

registerManifests(MANIFESTS);

var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                .getService(Ci.nsIXULChromeRegistry)
                .QueryInterface(Ci.nsIToolkitChromeRegistry);
chromeReg.checkForNewChrome();

function enum_to_array(strings) {
  return Array.from(strings).sort();
}

function run_test() {

  // without override
  Services.locale.requestedLocales = ["de"];
  Assert.equal(chromeReg.getSelectedLocale("basepack"), "en-US");
  Assert.equal(chromeReg.getSelectedLocale("overpack"), "de");
  Assert.deepEqual(enum_to_array(chromeReg.getLocalesForPackage("basepack")),
                   ["en-US", "fr"]);

  // with override
  Services.prefs.setCharPref("chrome.override_package.basepack", "overpack");
  Assert.equal(chromeReg.getSelectedLocale("basepack"), "de");
  Assert.deepEqual(enum_to_array(chromeReg.getLocalesForPackage("basepack")),
                   ["de", "en-US"]);

}
