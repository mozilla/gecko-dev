/**
 * Test for unassigned code points in IDNs (RFC 3454 section 7)
 */

"use strict";

var idnService;

function expected_pass(inputIDN) {
  var displayIDN = idnService.convertToDisplayIDN(inputIDN);
  Assert.equal(displayIDN, inputIDN);
}

function expected_fail(inputIDN) {
  var displayIDN = "";

  try {
    displayIDN = idnService.convertToDisplayIDN(inputIDN);
  } catch (e) {}

  Assert.notEqual(displayIDN, inputIDN);
}

function run_test() {
  idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
    Ci.nsIIDNService
  );

  // assigned code point
  expected_pass("foo\u0101bar.com");

  // assigned code point in punycode. Should *fail* because the URL will be
  // converted to Unicode for display
  expected_fail("xn--foobar-5za.com");

  // unassigned code point
  expected_fail("foo\u3040bar.com");

  // unassigned code point in punycode. Should *fail* because Punycode
  // is decoded and checked.
  expected_fail("xn--foobar-533e.com");

  // code point assigned since Unicode 3.0
  // XXX This test will unexpectedly pass when we update to IDNAbis
  expected_fail("foo\u0370bar.com");
}
