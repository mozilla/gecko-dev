// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Get a profile directory and ensure PSM initializes NSS,
// to ensure the error string tables are installed.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

function run_test() {
  let nssErrorsService = Cc["@mozilla.org/nss_errors_service;1"].getService(
    Ci.nsINSSErrorsService
  );

  let xpcom = nssErrorsService.getXPCOMFromNSSError(SEC_ERROR_UNTRUSTED_CERT);
  let name = nssErrorsService.getErrorName(xpcom);

  equal(
    name,
    "SEC_ERROR_UNTRUSTED_CERT",
    "GetErrorName should work for SEC errors"
  );

  xpcom = nssErrorsService.getXPCOMFromNSSError(SSL_ERROR_BAD_CERT_DOMAIN);
  name = nssErrorsService.getErrorName(xpcom);

  equal(
    name,
    "SSL_ERROR_BAD_CERT_DOMAIN",
    "GetErrorName should work for SSL errors"
  );

  xpcom = nssErrorsService.getXPCOMFromNSSError(
    MOZILLA_PKIX_ERROR_INSUFFICIENT_CERTIFICATE_TRANSPARENCY
  );
  name = nssErrorsService.getErrorName(xpcom);

  equal(
    name,
    "MOZILLA_PKIX_ERROR_INSUFFICIENT_CERTIFICATE_TRANSPARENCY",
    "GetErrorName should work for PKIX errors"
  );
}
