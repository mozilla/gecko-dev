/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu } = require("chrome");
const prefs = require("sdk/preferences/service");
const Branch = prefs.Branch;
const BundleService = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);

const specialChars = "!@#$%^&*()_-=+[]{}~`\'\"<>,./?;:";

exports.testReset = function(assert) {
  prefs.reset("test_reset_pref");
  assert.equal(prefs.has("test_reset_pref"), false);
  assert.equal(prefs.isSet("test_reset_pref"), false);
  prefs.set("test_reset_pref", 5);
  assert.equal(prefs.has("test_reset_pref"), true);
  assert.equal(prefs.isSet("test_reset_pref"), true);
  assert.equal(prefs.keys("test_reset_pref").toString(), "test_reset_pref");
};

exports.testGetAndSet = function(assert) {
  let svc = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch(null);
  svc.setCharPref("test_set_get_pref", "a normal string");
  assert.equal(prefs.get("test_set_get_pref"), "a normal string",
                   "preferences-service should read from " +
                   "application-wide preferences service");

  // test getting a pref that does not exist,
  // and where we provide no default
  assert.equal(
      prefs.get("test_dne_get_pref", "default"),
      "default",
      "default was used for a pref that does not exist");
  assert.equal(
      prefs.get("test_dne_get_pref"),
      undefined,
      "undefined was returned for a pref that does not exist with no default");

  prefs.set("test_set_get_pref.integer", 1);
  assert.equal(prefs.get("test_set_get_pref.integer"), 1,
                   "set/get integer preference should work");

  assert.equal(
      prefs.keys("test_set_get_pref").sort().toString(),
      ["test_set_get_pref.integer","test_set_get_pref"].sort().toString(),
      "the key list is correct");

  prefs.set("test_set_get_number_pref", 42);
  assert.throws(
    () => prefs.set("test_set_get_number_pref", 3.14159),
    /cannot store non-integer number: 3.14159/,
    "setting a float preference should raise an error"
  );
  assert.equal(prefs.get("test_set_get_number_pref"),
               42,
               "bad-type write attempt should not overwrite");

  // 0x80000000 (bad), 0x7fffffff (ok), -0x80000000 (ok), -0x80000001 (bad)
  assert.throws(
    () => prefs.set("test_set_get_number_pref", 0x80000000),
    /32\-bit/,
    "setting an int pref above 2^31-1 shouldn't work"
  );

  assert.equal(prefs.get("test_set_get_number_pref"), 42,
                   "out-of-range write attempt should not overwrite 1");

  prefs.set("test_set_get_number_pref", 0x7fffffff);
  assert.equal(prefs.get("test_set_get_number_pref"),
               0x7fffffff,
               "in-range write attempt should work 1");

  prefs.set("test_set_get_number_pref", -0x80000000);
  assert.equal(prefs.get("test_set_get_number_pref"),
               -0x80000000,
               "in-range write attempt should work 2");
  assert.throws(
    () => prefs.set("test_set_get_number_pref", -0x80000001),
    /32\-bit/,
    "setting an int pref below -(2^31) shouldn't work"
  );
  assert.equal(prefs.get("test_set_get_number_pref"), -0x80000000,
                   "out-of-range write attempt should not overwrite 2");


  prefs.set("test_set_get_pref.string", "foo");
  assert.equal(prefs.get("test_set_get_pref.string"), "foo",
                   "set/get string preference should work");

  prefs.set("test_set_get_pref.boolean", true);
  assert.equal(prefs.get("test_set_get_pref.boolean"), true,
                   "set/get boolean preference should work");

  prefs.set("test_set_get_unicode_pref", String.fromCharCode(960));
  assert.equal(prefs.get("test_set_get_unicode_pref"),
                   String.fromCharCode(960),
                   "set/get unicode preference should work");

  [ null, [], undefined ].forEach((value) => {
    assert.throws(
      () => prefs.set("test_set_pref", value),
      new RegExp("can't set pref test_set_pref to value '" + value + "'; " +
       "it isn't a string, number, or boolean", "i"),
      "Setting a pref to " + uneval(value) + " should raise error"
    );
  });
};

exports.testPrefClass = function(assert) {
  var branch = Branch("test_foo");

  assert.equal(branch.test, undefined, "test_foo.test is undefined");
  branch.test = true;
  assert.equal(branch.test, true, "test_foo.test is true");
  delete branch.test;
  assert.equal(branch.test, undefined, "test_foo.test is undefined");
};

exports.testGetSetLocalized = function(assert) {
  let prefName = "general.useragent.locale";

  // Ensure that "general.useragent.locale" is a 'localized' pref
  let bundleURL = "chrome://global/locale/intl.properties";
  prefs.setLocalized(prefName, bundleURL);

  // Fetch the expected value directly from the property file
  let expectedValue = BundleService.createBundle(bundleURL).
    GetStringFromName(prefName).
    toLowerCase();

  assert.equal(prefs.getLocalized(prefName).toLowerCase(),
                   expectedValue,
                   "get localized preference");

  // Undo our modification
  prefs.reset(prefName);
}

// TEST: setting and getting preferences with special characters work
exports.testSpecialChars = function(assert) {
  let chars = specialChars.split('');
  const ROOT = "test.";

  chars.forEach((char) => {
    let rand = Math.random() + "";
    prefs.set(ROOT + char, rand);
    assert.equal(prefs.get(ROOT+char), rand, "setting pref with a name that is a special char, " + char + ", worked!");
  });
};

require("sdk/test").run(exports);
