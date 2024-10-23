/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Test that default uri is bypassable by an unknown protocol that is
 *  present in the bypass list (and the pref is enabled)
 */
"use strict";

const {
  checkInputAndSerializationMatch,
  checkSerializationMissingSecondColon,
} = ChromeUtils.importESModule(
  "resource://testing-common/simple_unknown_uri_helpers.sys.mjs"
);

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  // In-Parent-only process pref setup
  if (!inChildProcess()) {
    // child-test sets these in test_simple_unknown_uris_wrap.js
    Services.prefs.setBoolPref("network.url.useDefaultURI", true);
    Services.prefs.setBoolPref(
      "network.url.simple_uri_unknown_schemes_enabled",
      true
    );
    Services.prefs.setCharPref(
      "network.url.simple_uri_unknown_schemes",
      "simpleprotocol,otherproto"
    );
  }

  // sanity check: non-nested special url is fine
  checkInputAndSerializationMatch("https://example.com/");

  // nsStandardURL removes second colon when nesting protocols
  checkSerializationMissingSecondColon("https://https://example.com/");

  // no-bypass for unknown protocol uses defaultURI
  checkSerializationMissingSecondColon(
    "nonsimpleprotocol://https://example.com"
  );

  // an unknown protocol in the bypass list will use simpleURI
  checkInputAndSerializationMatch("simpleprotocol://https://example.com");

  // setCharPref not accessible from child process
  if (!inChildProcess()) {
    // check that pref update removes simpleprotocol from bypass list
    Services.prefs.setCharPref(
      "network.url.simple_uri_unknown_schemes",
      "otherproto"
    );
    checkSerializationMissingSecondColon(
      "simpleprotocol://https://example.com"
    );

    // check that spaces are parsed out
    Services.prefs.setCharPref(
      "network.url.simple_uri_unknown_schemes",
      " simpleprotocol , otherproto "
    );
    checkInputAndSerializationMatch("simpleprotocol://https://example.com");
    checkInputAndSerializationMatch("otherproto://https://example.com");
  }
}
