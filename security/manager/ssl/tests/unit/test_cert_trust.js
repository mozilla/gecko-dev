// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

function load_cert(cert_name, trust_string) {
  let cert_filename = cert_name + ".pem";
  return addCertFromFile(
    certdb,
    "test_cert_trust/" + cert_filename,
    trust_string
  );
}

function setup_basic_trusts(ca_cert, int_cert) {
  certdb.setCertTrust(
    ca_cert,
    Ci.nsIX509Cert.CA_CERT,
    Ci.nsIX509CertDB.TRUSTED_SSL | Ci.nsIX509CertDB.TRUSTED_EMAIL
  );

  certdb.setCertTrust(int_cert, Ci.nsIX509Cert.CA_CERT, 0);
}

async function test_ca_distrust(ee_cert, cert_to_modify_trust, isRootCA) {
  // On reset most usages are successful
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_CA_CERT_INVALID,
    Ci.nsIX509CertDB.verifyUsageTLSServerCA
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );

  // Test of active distrust. No usage should pass.
  setCertTrust(cert_to_modify_trust, "p,p,p");
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_CA_CERT_INVALID,
    Ci.nsIX509CertDB.verifyUsageTLSServerCA
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );

  // Trust set to T  -  trusted CA to issue client certs, where client cert is
  // usageSSLClient.
  setCertTrust(cert_to_modify_trust, "T,T,T");
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );

  // XXX(Bug 982340)
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );

  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_CA_CERT_INVALID,
    Ci.nsIX509CertDB.verifyUsageTLSServerCA
  );

  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );

  // Now tests on the SSL trust bit
  setCertTrust(cert_to_modify_trust, "p,C,C");
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );

  // XXX(Bug 982340)
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_CA_CERT_INVALID,
    Ci.nsIX509CertDB.verifyUsageTLSServerCA
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );

  // Inherited trust SSL
  setCertTrust(cert_to_modify_trust, ",C,C");
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );
  // XXX(Bug 982340)
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_CA_CERT_INVALID,
    Ci.nsIX509CertDB.verifyUsageTLSServerCA
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );

  // Now tests on the EMAIL trust bit
  setCertTrust(cert_to_modify_trust, "C,p,C");
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_CA_CERT_INVALID,
    Ci.nsIX509CertDB.verifyUsageTLSServerCA
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );

  // inherited EMAIL Trust
  setCertTrust(cert_to_modify_trust, "C,,C");
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    SEC_ERROR_CA_CERT_INVALID,
    Ci.nsIX509CertDB.verifyUsageTLSServerCA
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    isRootCA ? SEC_ERROR_UNKNOWN_ISSUER : PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );
}

add_task(async function () {
  let certList = ["ca", "int", "ee"];
  let loadedCerts = [];
  for (let certName of certList) {
    loadedCerts.push(load_cert(certName, ",,"));
  }

  let ca_cert = loadedCerts[0];
  notEqual(ca_cert, null, "CA cert should have successfully loaded");
  let int_cert = loadedCerts[1];
  notEqual(int_cert, null, "Intermediate cert should have successfully loaded");
  let ee_cert = loadedCerts[2];
  notEqual(ee_cert, null, "EE cert should have successfully loaded");

  let init_num_trustObj = certdb.countTrustObjects();
  setup_basic_trusts(ca_cert, int_cert);
  await test_ca_distrust(ee_cert, ca_cert, true);

  // testing countTrustObjects(), loaded 2 certs from above code
  let num_trustObj = certdb.countTrustObjects();
  equal(
    num_trustObj,
    init_num_trustObj + 2,
    "Number of trust objects should be 2"
  );

  setup_basic_trusts(ca_cert, int_cert);
  await test_ca_distrust(ee_cert, int_cert, false);

  // Reset trust to default ("inherit trust")
  setCertTrust(ca_cert, ",,");
  setCertTrust(int_cert, ",,");

  // End-entities can be trust anchors for interoperability with users who
  // prefer not to build a hierarchy and instead directly trust a particular
  // server certificate.
  setCertTrust(ee_cert, "CTu,CTu,CTu");
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSServer
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageTLSClient
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailSigner
  );
  await checkCertErrorGeneric(
    certdb,
    ee_cert,
    PRErrorCodeSuccess,
    Ci.nsIX509CertDB.verifyUsageEmailRecipient
  );
});
