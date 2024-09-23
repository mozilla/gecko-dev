// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.pki.certificate_transparency.mode");
  Services.prefs.clearUserPref("security.test.built_in_root_hash");
  let cert = constructCertFromFile("test_ct/ct-valid.example.com.pem");
  setCertTrust(cert, ",,");
});

function run_test() {
  Services.prefs.setIntPref("security.pki.certificate_transparency.mode", 1);
  add_tls_server_setup("BadCertAndPinningServer", "test_ct");

  // Test that certificate transparency is not checked for certificates issued
  // by roots that are not built-in.
  add_ct_test(
    "ct-unknown-log.example.com",
    Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE
  );

  add_test(function set_test_root_as_built_in() {
    // Make the test root appear to be a built-in root, so that certificate
    // transparency is checked.
    let rootCert = constructCertFromFile("test_ct/test-ca.pem");
    Services.prefs.setCharPref(
      "security.test.built_in_root_hash",
      rootCert.sha256Fingerprint
    );
    run_next_test();
  });

  // These certificates have a validity period of 800 days, which is greater
  // than 180 days. Our policy requires 3 embedded SCTs for certificates with a
  // validity period greater than 180 days.
  add_ct_test(
    "ct-valid.example.com",
    Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_POLICY_COMPLIANT
  );
  // This certificate has only 2 embedded SCTs, and so is not policy-compliant.
  add_ct_test(
    "ct-insufficient-scts.example.com",
    Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS
  );

  // Test that SCTs with timestamps from the future are not valid.
  add_ct_test(
    "ct-future-timestamp.example.com",
    Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS
  );

  // Test that additional SCTs from the same log do not contribute to meeting
  // the requirements.
  add_ct_test(
    "ct-multiple-from-same-log.example.com",
    Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_POLICY_NOT_DIVERSE_SCTS
  );

  // Test that SCTs from an unknown log do not contribute to meeting the
  // requirements.
  add_ct_test(
    "ct-unknown-log.example.com",
    Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS
  );

  // Test that if an end-entity is marked as a trust anchor, CT verification
  // returns a "not enough SCTs" result.
  add_test(() => {
    let cert = constructCertFromFile("test_ct/ct-valid.example.com.pem");
    Services.prefs.setCharPref(
      "security.test.built_in_root_hash",
      cert.sha256Fingerprint
    );
    setCertTrust(cert, "CTu,,");
    clearSessionCache();
    run_next_test();
  });
  add_ct_test(
    "ct-valid.example.com",
    Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS
  );

  run_next_test();
}
