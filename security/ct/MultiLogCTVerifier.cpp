/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultiLogCTVerifier.h"

#include "CTObjectsExtractor.h"
#include "CTSerialization.h"
#include "mozilla/StaticPrefs_security.h"

namespace mozilla {
namespace ct {

using namespace mozilla::pkix;

MultiLogCTVerifier::MultiLogCTVerifier()
    : mSignatureCache(signature_cache_new(
                          StaticPrefs::security_pki_sct_signature_cache_size()),
                      signature_cache_free) {}

void MultiLogCTVerifier::AddLog(CTLogVerifier&& log) {
  mLogs.push_back(std::move(log));
}

pkix::Result MultiLogCTVerifier::Verify(Input cert,
                                        Input issuerSubjectPublicKeyInfo,
                                        Input sctListFromCert,
                                        Input sctListFromOCSPResponse,
                                        Input sctListFromTLSExtension,
                                        Time time, CTVerifyResult& result) {
  assert(cert.GetLength() > 0);
  result.Reset();

  pkix::Result rv;

  // Verify embedded SCTs
  if (issuerSubjectPublicKeyInfo.GetLength() > 0 &&
      sctListFromCert.GetLength() > 0) {
    LogEntry precertEntry;
    rv = GetPrecertLogEntry(cert, issuerSubjectPublicKeyInfo, precertEntry);
    if (rv != Success) {
      return rv;
    }
    rv = VerifySCTs(sctListFromCert, precertEntry, SCTOrigin::Embedded, time,
                    result);
    if (rv != Success) {
      return rv;
    }
  }

  LogEntry x509Entry;
  GetX509LogEntry(cert, x509Entry);

  // Verify SCTs from a stapled OCSP response
  if (sctListFromOCSPResponse.GetLength() > 0) {
    rv = VerifySCTs(sctListFromOCSPResponse, x509Entry, SCTOrigin::OCSPResponse,
                    time, result);
    if (rv != Success) {
      return rv;
    }
  }

  // Verify SCTs from a TLS extension
  if (sctListFromTLSExtension.GetLength() > 0) {
    rv = VerifySCTs(sctListFromTLSExtension, x509Entry, SCTOrigin::TLSExtension,
                    time, result);
    if (rv != Success) {
      return rv;
    }
  }
  return Success;
}

void DecodeSCTs(Input encodedSctList,
                std::vector<SignedCertificateTimestamp>& decodedSCTs,
                size_t& decodingErrors) {
  decodedSCTs.clear();

  Reader listReader;
  pkix::Result rv = DecodeSCTList(encodedSctList, listReader);
  if (rv != Success) {
    decodingErrors++;
    return;
  }

  while (!listReader.AtEnd()) {
    Input encodedSct;
    rv = ReadSCTListItem(listReader, encodedSct);
    if (rv != Success) {
      decodingErrors++;
      return;
    }

    Reader encodedSctReader(encodedSct);
    SignedCertificateTimestamp sct;
    rv = DecodeSignedCertificateTimestamp(encodedSctReader, sct);
    if (rv != Success) {
      decodingErrors++;
      continue;
    }
    decodedSCTs.push_back(std::move(sct));
  }
}

pkix::Result MultiLogCTVerifier::VerifySCTs(Input encodedSctList,
                                            const LogEntry& expectedEntry,
                                            SCTOrigin origin, Time time,
                                            CTVerifyResult& result) {
  std::vector<SignedCertificateTimestamp> decodedSCTs;
  DecodeSCTs(encodedSctList, decodedSCTs, result.decodingErrors);
  for (auto sct : decodedSCTs) {
    pkix::Result rv =
        VerifySingleSCT(std::move(sct), expectedEntry, origin, time, result);
    if (rv != Success) {
      return rv;
    }
  }
  return Success;
}

pkix::Result MultiLogCTVerifier::VerifySingleSCT(
    SignedCertificateTimestamp&& sct, const LogEntry& expectedEntry,
    SCTOrigin origin, Time time, CTVerifyResult& result) {
  switch (origin) {
    case SCTOrigin::Embedded:
      result.embeddedSCTs++;
      break;
    case SCTOrigin::TLSExtension:
      result.sctsFromTLSHandshake++;
      break;
    case SCTOrigin::OCSPResponse:
      result.sctsFromOCSP++;
      break;
  }

  CTLogVerifier* matchingLog = nullptr;
  for (auto& log : mLogs) {
    if (log.keyId() == sct.logId) {
      matchingLog = &log;
      break;
    }
  }

  if (!matchingLog) {
    // SCT does not match any known log.
    result.sctsFromUnknownLogs++;
    return Success;
  }

  if (!matchingLog->SignatureParametersMatch(sct.signature)) {
    // SCT signature parameters do not match the log's.
    result.sctsWithInvalidSignatures++;
    return Success;
  }

  pkix::Result rv =
      matchingLog->Verify(expectedEntry, sct, mSignatureCache.get());
  if (rv != Success) {
    if (rv == pkix::Result::ERROR_BAD_SIGNATURE) {
      result.sctsWithInvalidSignatures++;
      return Success;
    }
    return rv;
  }

  // Make sure the timestamp is legitimate (not in the future).
  // SCT's |timestamp| is measured in milliseconds since the epoch,
  // ignoring leap seconds. When converting it to a second-level precision
  // pkix::Time, we need to round it either up or down. In our case, rounding up
  // (towards the future) is more "secure", although practically
  // it does not matter.
  Time sctTime = TimeFromEpochInSeconds((sct.timestamp + 999u) / 1000u);
  if (sctTime > time) {
    result.sctsWithInvalidTimestamps++;
    return Success;
  }

  VerifiedSCT verifiedSct(std::move(sct), origin, matchingLog->operatorId(),
                          matchingLog->state(), matchingLog->timestamp());
  result.verifiedScts.push_back(std::move(verifiedSct));
  return Success;
}

}  // namespace ct
}  // namespace mozilla
