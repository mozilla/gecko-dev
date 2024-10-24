/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import {
  parse,
  pemToDER,
} from "chrome://global/content/certviewer/certDecoder.mjs";

// The following parameters are parsed from the error URL:
//   e - the error code
//   s - custom CSS class to allow alternate styling/favicons
//   d - error description
//   captive - "true" to indicate we're behind a captive portal.
//             Any other value is ignored.

// Note that this file uses document.documentURI to get
// the URL (with the format from above). This is because
// document.location.href gets the current URI off the docshell,
// which is the URL displayed in the location bar, i.e.
// the URI that the user attempted to load.

export let searchParams = new URLSearchParams(
  document.documentURI.split("?")[1]
);

export let gErrorCode = searchParams.get("e");
export let gIsCertError = gErrorCode == "nssBadCert";
export let gHasSts = gIsCertError && getCSSClass() === "badStsCert";
const HOST_NAME = getHostName();

export function getCSSClass() {
  return searchParams.get("s");
}

export function getHostName() {
  try {
    return new URL(RPMGetInnerMostURI(document.location.href)).hostname;
  } catch (error) {
    console.error("Could not parse URL", error);
  }
  return "";
}

export async function getFailedCertificatesAsPEMString() {
  let locationUrl = document.location.href;
  let failedCertInfo = document.getFailedCertSecurityInfo();
  let errorMessage = failedCertInfo.errorMessage;
  let hasHSTS = failedCertInfo.hasHSTS.toString();
  let hasHPKP = failedCertInfo.hasHPKP.toString();
  let [hstsLabel, hpkpLabel, failedChainLabel] =
    await document.l10n.formatValues([
      { id: "cert-error-details-hsts-label", args: { hasHSTS } },
      { id: "cert-error-details-key-pinning-label", args: { hasHPKP } },
      { id: "cert-error-details-cert-chain-label" },
    ]);

  let certStrings = failedCertInfo.certChainStrings;
  let failedChainCertificates = "";
  for (let der64 of certStrings) {
    let wrapped = der64.replace(/(\S{64}(?!$))/g, "$1\r\n");
    failedChainCertificates +=
      "-----BEGIN CERTIFICATE-----\r\n" +
      wrapped +
      "\r\n-----END CERTIFICATE-----\r\n";
  }

  let details =
    locationUrl +
    "\r\n\r\n" +
    errorMessage +
    "\r\n\r\n" +
    hstsLabel +
    "\r\n" +
    hpkpLabel +
    "\r\n\r\n" +
    failedChainLabel +
    "\r\n\r\n" +
    failedChainCertificates;
  return details;
}

export async function getSubjectAltNames(failedCertInfo) {
  const serverCertBase64 = failedCertInfo.certChainStrings[0];
  const parsed = await parse(pemToDER(serverCertBase64));
  const subjectAltNamesExtension = parsed.ext.san;
  const subjectAltNames = [];
  if (subjectAltNamesExtension) {
    for (let [key, value] of subjectAltNamesExtension.altNames) {
      if (key === "DNS Name" && value.length) {
        subjectAltNames.push(value);
      }
    }
  }
  return subjectAltNames;
}

export async function recordSecurityUITelemetry(category, name, errorInfo) {
  // Truncate the error code to avoid going over the allowed
  // string size limit for telemetry events.
  let errorCode = errorInfo.errorCodeString.substring(0, 40);
  let extraKeys = {
    value: errorCode,
    is_frame: window.parent != window,
  };
  if (category == "securityUiCerterror") {
    extraKeys.has_sts = gHasSts;
  }
  if (name.startsWith("load")) {
    extraKeys.channel_status = errorInfo.channelStatus;
  }
  if (category == "securityUiCerterror" && name.startsWith("load")) {
    extraKeys.issued_by_cca = false;
    extraKeys.hyphen_compat = false;
    // This issue only applies to certificate domain name mismatch errors where
    // the first label in the domain name starts or ends with a hyphen.
    let label = HOST_NAME.substring(0, HOST_NAME.indexOf("."));
    if (
      errorCode == "SSL_ERROR_BAD_CERT_DOMAIN" &&
      (label.startsWith("-") || label.endsWith("-"))
    ) {
      try {
        let subjectAltNames = await getSubjectAltNames(errorInfo);
        for (let subjectAltName of subjectAltNames) {
          // If the certificate has a wildcard entry that matches the domain
          // name (e.g. '*.example.com' matches 'foo-.example.com'), then
          // this error is probably due to Firefox disallowing hyphens in
          // domain names when matching wildcard entries.
          if (
            subjectAltName.startsWith("*.") &&
            subjectAltName.substring(1) == HOST_NAME.substring(label.length)
          ) {
            extraKeys.hyphen_compat = true;
            break;
          }
        }
      } catch (e) {
        console.error("error parsing certificate:", e);
      }
    }
    let issuer = errorInfo.certChainStrings.at(-1);
    if (issuer && errorCode == "SEC_ERROR_UNKNOWN_ISSUER") {
      try {
        let parsed = await parse(pemToDER(issuer));
        extraKeys.issued_by_cca =
          parsed.issuer.dn == "c=IN, o=India PKI, cn=CCA India 2022 SPL" ||
          parsed.issuer.dn == "c=IN, o=India PKI, cn=CCA India 2015 SPL";
      } catch (e) {
        console.error("error parsing issuer certificate:", e);
      }
    }
  }
  RPMRecordGleanEvent(category, name, extraKeys);
}
