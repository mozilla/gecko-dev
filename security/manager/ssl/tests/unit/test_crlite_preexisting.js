// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that starting a profile with a preexisting CRLite filter and stash
// works correctly.

"use strict";

add_task(async function () {
  Services.prefs.setIntPref(
    "security.pki.crlite_mode",
    CRLiteModeEnforcePrefValue
  );

  let securityStateDirectory = do_get_profile();
  securityStateDirectory.append("security_state");

  // For simplicity, re-use the filters from test_crlite_filters.js.
  do_get_file("test_crlite_filters/20201017-0-filter").copyTo(
    securityStateDirectory,
    "crlite.filter"
  );

  do_get_file("test_crlite_filters/20201017-1-filter.delta").copyTo(
    securityStateDirectory,
    "20201017-1-filter.delta"
  );

  do_get_file("test_crlite_filters/20201201-3-filter.delta").copyTo(
    securityStateDirectory,
    "20201201-3-filter.delta"
  );

  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
    Ci.nsICertStorage
  );

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );

  // This needs to be available for path building.
  let issuerCert = constructCertFromFile("test_crlite_filters/issuer.pem");
  ok(issuerCert, "issuer certificate should decode successfully");

  // Mark CRLite filter as fresh
  await new Promise(resolve => {
    certStorage.testNoteCRLiteUpdateTime((rv, _) => {
      Assert.equal(rv, Cr.NS_OK, "marked filter as fresh");
      resolve();
    });
  });

  let validCert = constructCertFromFile("test_crlite_filters/valid.pem");
  await checkCertErrorGenericAtTime(
    certdb,
    validCert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "vpn.worldofspeed.org",
    0
  );

  let revokedCert = constructCertFromFile("test_crlite_filters/revoked.pem");
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    Ci.nsIX509CertDB.verifyUsageTLSServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "us-datarecovery.com",
    0
  );

  let revokedInStashCert = constructCertFromFile(
    "test_crlite_filters/revoked-in-stash.pem"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStashCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    Ci.nsIX509CertDB.verifyUsageTLSServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "stokedmoto.com",
    0
  );

  let revokedInStash2Cert = constructCertFromFile(
    "test_crlite_filters/revoked-in-stash-2.pem"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStash2Cert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    Ci.nsIX509CertDB.verifyUsageTLSServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "icsreps.com",
    0
  );
});
