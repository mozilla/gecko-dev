/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTVerifyResult_h
#define CTVerifyResult_h

#include <vector>

#include "CTKnownLogs.h"
#include "CTLog.h"
#include "SignedCertificateTimestamp.h"

namespace mozilla {
namespace ct {

enum class SCTOrigin {
  Embedded,
  TLSExtension,
  OCSPResponse,
};

// Holds a verified Signed Certificate Timestamp along with the verification
// status (e.g. valid/invalid) and additional information related to the
// verification.
struct VerifiedSCT {
  VerifiedSCT(SignedCertificateTimestamp&& sct, SCTOrigin origin,
              CTLogOperatorId logOperatorId, CTLogState logState,
              uint64_t logTimestamp);

  // The original SCT.
  SignedCertificateTimestamp sct;
  SCTOrigin origin;
  CTLogOperatorId logOperatorId;
  CTLogState logState;
  uint64_t logTimestamp;
};

typedef std::vector<VerifiedSCT> VerifiedSCTList;

// Holds Signed Certificate Timestamps verification results.
class CTVerifyResult {
 public:
  CTVerifyResult() { Reset(); }

  // SCTs that were processed during the verification along with their
  // verification results.
  VerifiedSCTList verifiedScts;

  // The verifier makes the best effort to extract the available SCTs
  // from the binary sources provided to it.
  // If some SCT cannot be extracted due to encoding errors, the verifier
  // proceeds to the next available one. In other words, decoding errors are
  // effectively ignored.
  // Note that a serialized SCT may fail to decode for a "legitimate" reason,
  // e.g. if the SCT is from a future version of the Certificate Transparency
  // standard.
  // |decodingErrors| field counts the errors of the above kind.
  size_t decodingErrors;
  // The number of SCTs encountered from unknown logs.
  size_t sctsFromUnknownLogs;
  // The number of SCTs encountered with invalid signatures.
  size_t sctsWithInvalidSignatures;
  // The number of SCTs encountered with timestamps in the future.
  size_t sctsWithInvalidTimestamps;

  // The number of SCTs that were embedded in the certificate.
  size_t embeddedSCTs;
  // The number of SCTs included in the TLS handshake.
  size_t sctsFromTLSHandshake;
  // The number of SCTs delivered via OCSP.
  size_t sctsFromOCSP;

  void Reset();
};

}  // namespace ct
}  // namespace mozilla

#endif  // CTVerifyResult_h
