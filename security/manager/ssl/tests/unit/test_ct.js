// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB

function expectCT(value) {
  return securityInfo => {
    Assert.equal(
      securityInfo.certificateTransparencyStatus,
      value,
      "actual and expected CT status should match"
    );
  };
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.pki.certificate_transparency.mode");
  let cert = constructCertFromFile("test_ct/ct-valid.example.com.pem");
  setCertTrust(cert, ",,");
});

function run_test() {
  Services.prefs.setIntPref("security.pki.certificate_transparency.mode", 1);
  add_tls_server_setup("BadCertAndPinningServer", "test_ct");
  // These certificates have a validity period of 800 days, which is greater
  // than 180 days. Our policy requires 3 embedded SCTs for certificates with a
  // validity period greater than 180 days.
  add_connection_test(
    "ct-valid.example.com",
    PRErrorCodeSuccess,
    null,
    expectCT(
      Ci.nsITransportSecurityInfo.CERTIFICATE_TRANSPARENCY_POLICY_COMPLIANT
    )
  );
  // This certificate has only 2 embedded SCTs, and so is not policy-compliant.
  add_connection_test(
    "ct-insufficient-scts.example.com",
    PRErrorCodeSuccess,
    null,
    expectCT(
      Ci.nsITransportSecurityInfo
        .CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS
    )
  );

  // Test that if an end-entity is marked as a trust anchor, CT verification
  // returns a "not enough SCTs" result.
  add_test(() => {
    let cert = constructCertFromFile("test_ct/ct-valid.example.com.pem");
    setCertTrust(cert, "CTu,,");
    clearSessionCache();
    run_next_test();
  });
  add_connection_test(
    "ct-valid.example.com",
    PRErrorCodeSuccess,
    null,
    expectCT(
      Ci.nsITransportSecurityInfo
        .CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS
    )
  );

  run_next_test();
}
