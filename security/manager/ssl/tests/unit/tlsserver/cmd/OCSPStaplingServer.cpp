/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server that delivers various stapled OCSP responses.
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the OCSP response.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <fstream>
#include <stdio.h>

#include "OCSPCommon.h"
#include "TLSServer.h"

using namespace mozilla;
using namespace mozilla::pkix::test;
using namespace mozilla::test;

const OCSPHost sOCSPHosts[] = {
    {"ocsp-stapling-good.example.com", ORTGood, nullptr, nullptr},
    {"ocsp-stapling-revoked.example.com", ORTRevoked, nullptr, nullptr},
    {"ocsp-stapling-revoked-old.example.com", ORTRevokedOld, nullptr, nullptr},
    {"ocsp-stapling-unknown.example.com", ORTUnknown, nullptr, nullptr},
    {"ocsp-stapling-unknown-old.example.com", ORTUnknownOld, nullptr, nullptr},
    {"ocsp-stapling-good-other.example.com", ORTGoodOtherCert,
     "ocspOtherEndEntity", nullptr},
    {"ocsp-stapling-good-other-ca.example.com", ORTGoodOtherCA, "other-test-ca",
     nullptr},
    {"ocsp-stapling-expired.example.com", ORTExpired, nullptr, nullptr},
    {"ocsp-stapling-expired-fresh-ca.example.com", ORTExpiredFreshCA, nullptr,
     nullptr},
    {"ocsp-stapling-none.example.com", ORTNone, nullptr, nullptr},
    {"ocsp-stapling-empty.example.com", ORTEmpty, nullptr, nullptr},
    {"ocsp-stapling-malformed.example.com", ORTMalformed, nullptr, nullptr},
    {"ocsp-stapling-srverr.example.com", ORTSrverr, nullptr, nullptr},
    {"ocsp-stapling-trylater.example.com", ORTTryLater, nullptr, nullptr},
    {"ocsp-stapling-needssig.example.com", ORTNeedsSig, nullptr, nullptr},
    {"ocsp-stapling-unauthorized.example.com", ORTUnauthorized, nullptr,
     nullptr},
    {"ocsp-stapling-with-intermediate.example.com", ORTGood, nullptr,
     "ocspEEWithIntermediate"},
    {"ocsp-stapling-bad-signature.example.com", ORTBadSignature, nullptr,
     nullptr},
    {"ocsp-stapling-skip-responseBytes.example.com", ORTSkipResponseBytes,
     nullptr, nullptr},
    {"ocsp-stapling-critical-extension.example.com", ORTCriticalExtension,
     nullptr, nullptr},
    {"ocsp-stapling-noncritical-extension.example.com", ORTNoncriticalExtension,
     nullptr, nullptr},
    {"ocsp-stapling-empty-extensions.example.com", ORTEmptyExtensions, nullptr,
     nullptr},
    {"ocsp-stapling-delegated-included.example.com", ORTDelegatedIncluded,
     "delegatedSigner", nullptr},
    {"ocsp-stapling-delegated-included-last.example.com",
     ORTDelegatedIncludedLast, "delegatedSigner", nullptr},
    {"ocsp-stapling-delegated-missing.example.com", ORTDelegatedMissing,
     "delegatedSigner", nullptr},
    {"ocsp-stapling-delegated-missing-multiple.example.com",
     ORTDelegatedMissingMultiple, "delegatedSigner", nullptr},
    {"ocsp-stapling-delegated-no-extKeyUsage.example.com", ORTDelegatedIncluded,
     "invalidDelegatedSignerNoExtKeyUsage", nullptr},
    {"ocsp-stapling-delegated-from-intermediate.example.com",
     ORTDelegatedIncluded, "invalidDelegatedSignerFromIntermediate", nullptr},
    {"ocsp-stapling-delegated-keyUsage-crlSigning.example.com",
     ORTDelegatedIncluded, "invalidDelegatedSignerKeyUsageCrlSigning", nullptr},
    {"ocsp-stapling-delegated-wrong-extKeyUsage.example.com",
     ORTDelegatedIncluded, "invalidDelegatedSignerWrongExtKeyUsage", nullptr},
    {"ocsp-stapling-ancient-valid.example.com", ORTAncientAlmostExpired,
     nullptr, nullptr},
    {"keysize-ocsp-delegated.example.com", ORTDelegatedIncluded,
     "rsa-1016-keysizeDelegatedSigner", nullptr},
    {"revoked-ca-cert-used-as-end-entity.example.com", ORTRevoked,
     "ca-used-as-end-entity", nullptr},
    {"ocsp-stapling-must-staple.example.com", ORTGood, nullptr,
     "must-staple-ee"},
    {"ocsp-stapling-must-staple-revoked.example.com", ORTRevoked, nullptr,
     "must-staple-ee"},
    {"ocsp-stapling-must-staple-missing.example.com", ORTNone, nullptr,
     "must-staple-ee"},
    {"ocsp-stapling-must-staple-empty.example.com", ORTEmpty, nullptr,
     "must-staple-ee"},
    {"ocsp-stapling-must-staple-ee-with-must-staple-int.example.com", ORTGood,
     nullptr, "must-staple-ee-with-must-staple-int"},
    {"ocsp-stapling-plain-ee-with-must-staple-int.example.com", ORTGood,
     nullptr, "must-staple-missing-ee"},
    {"ocsp-stapling-must-staple-expired.example.com", ORTExpired, nullptr,
     "must-staple-ee"},
    {"ocsp-stapling-must-staple-try-later.example.com", ORTTryLater, nullptr,
     "must-staple-ee"},
    {"ocsp-stapling-must-staple-invalid-signer.example.com", ORTGoodOtherCA,
     "other-test-ca", "must-staple-ee"},
    {"multi-tls-feature-good.example.com", ORTNone, nullptr,
     "multi-tls-feature-good-ee"},
    {"multi-tls-feature-bad.example.com", ORTNone, nullptr,
     "multi-tls-feature-bad-ee"},
    {nullptr, ORTNull, nullptr, nullptr}};

enum class SCTsVia {
  None,
  OCSP,
  TLS,
};

struct CTHost {
  const char* mHostName;
  std::vector<const char*> mSCTFilenames;
  SCTsVia mSCTsVia;
};

const CTHost sCTHosts[] = {
    {"ct-via-ocsp.example.com",
     {"test_ct/ct-via-ocsp-1.sct", "test_ct/ct-via-ocsp-2.sct"},
     SCTsVia::OCSP},
    {"ct-via-tls.example.com",
     {"test_ct/ct-via-tls-1.sct", "test_ct/ct-via-tls-2.sct"},
     SCTsVia::TLS},
    {"ct-tampered.example.com",
     {"test_ct/ct-tampered-1.sct", "test_ct/ct-tampered-2.sct"},
     SCTsVia::TLS},
    {nullptr, {}, SCTsVia::None}};

ByteString ReadSCTList(const std::vector<const char*>& sctFilenames) {
  std::vector<std::string> scts;
  for (const auto& sctFilename : sctFilenames) {
    std::ifstream in(sctFilename, std::ios::binary);
    if (in.bad() || !in.is_open()) {
      if (gDebugLevel >= DEBUG_ERRORS) {
        fprintf(stderr, "couldn't open '%s'\n", sctFilename);
        return ByteString();
      }
    }
    std::ostringstream contentsStream;
    contentsStream << in.rdbuf();
    std::string contents = contentsStream.str();
    scts.push_back(std::move(contents));
  }

  ByteString contents;
  for (const auto& sct : scts) {
    // Each SCT has a 2-byte length prefix.
    contents.push_back(sct.length() / 256);
    contents.push_back(sct.length() % 256);
    contents.append(reinterpret_cast<const uint8_t*>(sct.data()), sct.length());
  }
  // The entire SCT list also has a 2-byte length prefix.
  ByteString sctList;
  sctList.push_back(contents.length() / 256);
  sctList.push_back(contents.length() % 256);
  sctList.append(reinterpret_cast<const uint8_t*>(contents.data()),
                 contents.length());
  return sctList;
}

int32_t DoSNISocketConfig(PRFileDesc* aFd, const SECItem* aSrvNameArr,
                          uint32_t aSrvNameArrSize, void* aArg) {
  const char* hostName = nullptr;
  OCSPResponseType ocspResponseType = ORTNone;
  const char* additionalCertName = nullptr;
  const char* serverCertName = nullptr;
  ByteString sctList;
  SCTsVia sctsVia = SCTsVia::None;

  const OCSPHost* host =
      GetHostForSNI(aSrvNameArr, aSrvNameArrSize, sOCSPHosts);
  if (host) {
    hostName = host->mHostName;
    ocspResponseType = host->mORT;
    additionalCertName = host->mAdditionalCertName;
    serverCertName = host->mServerCertName;
  } else {
    const CTHost* ctHost =
        GetHostForSNI(aSrvNameArr, aSrvNameArrSize, sCTHosts);
    if (!ctHost) {
      return SSL_SNI_SEND_ALERT;
    }
    hostName = ctHost->mHostName;
    ocspResponseType = ORTGood;
    serverCertName = ctHost->mHostName;
    sctList = ReadSCTList(ctHost->mSCTFilenames);
    if (sctList.empty()) {
      return SSL_SNI_SEND_ALERT;
    }
    sctsVia = ctHost->mSCTsVia;
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "found pre-defined host '%s'\n", hostName);
  }

  const char* certNickname =
      serverCertName ? serverCertName : DEFAULT_CERT_NICKNAME;

  UniqueCERTCertificate cert;
  SSLKEAType certKEA;
  if (SECSuccess != ConfigSecureServerWithNamedCert(aFd, certNickname, &cert,
                                                    &certKEA, nullptr)) {
    return SSL_SNI_SEND_ALERT;
  }

  // If the OCSP response type is "none", don't staple a response.
  if (ocspResponseType == ORTNone) {
    return 0;
  }

  UniquePLArenaPool arena(PORT_NewArena(1024));
  if (!arena) {
    PrintPRError("PORT_NewArena failed");
    return SSL_SNI_SEND_ALERT;
  }

  // response is contained by the arena - freeing the arena will free it
  SECItemArray* response =
      GetOCSPResponseForType(ocspResponseType, cert, arena, additionalCertName,
                             0, sctsVia == SCTsVia::OCSP ? &sctList : nullptr);
  if (!response) {
    return SSL_SNI_SEND_ALERT;
  }

  // SSL_SetStapledOCSPResponses makes a deep copy of response
  SECStatus st = SSL_SetStapledOCSPResponses(aFd, response, certKEA);
  if (st != SECSuccess) {
    PrintPRError("SSL_SetStapledOCSPResponses failed");
    return SSL_SNI_SEND_ALERT;
  }

  if (sctsVia == SCTsVia::TLS) {
    SECItem scts = {siBuffer, const_cast<unsigned char*>(sctList.data()),
                    (unsigned int)sctList.size()};
    st = SSL_SetSignedCertTimestamps(aFd, &scts, certKEA);
    if (st != SECSuccess) {
      PrintPRError("SSL_SetSignedCertTimestamps failed");
      return SSL_SNI_SEND_ALERT;
    }
  }

  return 0;
}

int main(int argc, char* argv[]) {
  return StartServer(argc, argv, DoSNISocketConfig, nullptr);
}
