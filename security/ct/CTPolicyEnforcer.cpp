/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTPolicyEnforcer.h"

#include "mozilla/Assertions.h"
#include "mozpkix/Time.h"
#include <set>
#include <stdint.h>

namespace mozilla {
namespace ct {

using namespace mozilla::pkix;

// Returns the number of embedded SCTs required to be present in a certificate.
// For certificates with a lifetime of less than or equal to 180 days, only 2
// embedded SCTs are required. Otherwise 3 are required.
MOZ_RUNINIT const Duration ONE_HUNDRED_AND_EIGHTY_DAYS =
    Duration(180 * Time::ONE_DAY_IN_SECONDS);
size_t GetRequiredEmbeddedSctsCount(Duration certLifetime) {
  // pkix::Duration doesn't define operator<=, hence phrasing this comparison
  // in an awkward way
  return ONE_HUNDRED_AND_EIGHTY_DAYS < certLifetime ? 3 : 2;
}

// Calculates the effective issuance time of connection's certificate using
// the SCTs present on the connection (we can't rely on notBefore validity
// field of the certificate since it can be backdated).
// Used to determine whether to accept SCTs issued by past qualified logs.
// The effective issuance time is defined as the earliest of all SCTs,
// rather than the latest of embedded SCTs, in order to give CAs the benefit
// of the doubt in the event a log is revoked in the midst of processing
// a precertificate and issuing the certificate.
// It is acceptable to ignore the origin of the SCTs because SCTs
// delivered via OCSP/TLS extension will cover the full certificate,
// which necessarily will exist only after the precertificate
// has been logged and the actual certificate issued.
uint64_t GetEffectiveCertIssuanceTime(const VerifiedSCTList& verifiedScts) {
  uint64_t result = UINT64_MAX;
  for (const VerifiedSCT& verifiedSct : verifiedScts) {
    if (verifiedSct.logState == CTLogState::Admissible) {
      result = std::min(result, verifiedSct.sct.timestamp);
    }
  }
  return result;
}

// Checks if the log that issued the given SCT is "once or currently qualified"
// (i.e. was qualified at the time of the certificate issuance). In addition,
// makes sure the SCT is before the retirement timestamp.
bool LogWasQualifiedForSct(const VerifiedSCT& verifiedSct,
                           uint64_t certIssuanceTime) {
  switch (verifiedSct.logState) {
    case CTLogState::Admissible:
      return true;
    case CTLogState::Retired: {
      uint64_t logRetirementTime = verifiedSct.logTimestamp;
      return certIssuanceTime < logRetirementTime &&
             verifiedSct.sct.timestamp < logRetirementTime;
    }
  }
  MOZ_ASSERT_UNREACHABLE("verifiedSct.logState must be Admissible or Retired");
  return false;
}

// Qualification for embedded SCTs:
// There must be at least one embedded SCT from a log that was Admissible (i.e.
// Qualified, Usable, or ReadOnly) at the time of the check.
// There must be at least N embedded SCTs from distinct logs that were
// Admissible or Retired at the time of the check, where N depends on the
// lifetime of the certificate. If the certificate lifetime is less than or
// equal to 180 days, N is 2. Otherwise, N is 3.
// Among these SCTs, at least two must be issued from distinct log operators.
CTPolicyCompliance EmbeddedSCTsCompliant(const VerifiedSCTList& verifiedScts,
                                         uint64_t certIssuanceTime,
                                         Duration certLifetime) {
  size_t admissibleCount = 0;
  size_t admissibleOrRetiredCount = 0;
  std::set<CTLogOperatorId> logOperators;
  std::set<Buffer> logIds;
  for (const auto& verifiedSct : verifiedScts) {
    if (verifiedSct.origin != SCTOrigin::Embedded) {
      continue;
    }
    if (verifiedSct.logState != CTLogState::Admissible &&
        !LogWasQualifiedForSct(verifiedSct, certIssuanceTime)) {
      continue;
    }
    // Note that a single SCT can count for both the "from a log that was
    // admissible" case and the "from a log that was admissible or retired"
    // case.
    if (verifiedSct.logState == CTLogState::Admissible) {
      admissibleCount++;
    }
    if (LogWasQualifiedForSct(verifiedSct, certIssuanceTime)) {
      admissibleOrRetiredCount++;
      logIds.insert(verifiedSct.sct.logId);
    }
    logOperators.insert(verifiedSct.logOperatorId);
  }

  size_t requiredEmbeddedScts = GetRequiredEmbeddedSctsCount(certLifetime);
  if (admissibleCount < 1 || admissibleOrRetiredCount < requiredEmbeddedScts) {
    return CTPolicyCompliance::NotEnoughScts;
  }
  if (logIds.size() < requiredEmbeddedScts || logOperators.size() < 2) {
    return CTPolicyCompliance::NotDiverseScts;
  }
  return CTPolicyCompliance::Compliant;
}

// Qualification for non-embedded SCTs (i.e. SCTs delivered via TLS handshake
// or OCSP response):
// There must be at least two SCTs from logs that were Admissible (i.e.
// Qualified, Usable, or ReadOnly) at the time of the check. Among these SCTs,
// at least two must be issued from distinct log operators.
CTPolicyCompliance NonEmbeddedSCTsCompliant(
    const VerifiedSCTList& verifiedScts) {
  size_t admissibleCount = 0;
  std::set<CTLogOperatorId> logOperators;
  std::set<Buffer> logIds;
  for (const auto& verifiedSct : verifiedScts) {
    if (verifiedSct.origin == SCTOrigin::Embedded) {
      continue;
    }
    if (verifiedSct.logState != CTLogState::Admissible) {
      continue;
    }
    admissibleCount++;
    logIds.insert(verifiedSct.sct.logId);
    logOperators.insert(verifiedSct.logOperatorId);
  }

  if (admissibleCount < 2) {
    return CTPolicyCompliance::NotEnoughScts;
  }
  if (logIds.size() < 2 || logOperators.size() < 2) {
    return CTPolicyCompliance::NotDiverseScts;
  }
  return CTPolicyCompliance::Compliant;
}

CTPolicyCompliance CheckCTPolicyCompliance(const VerifiedSCTList& verifiedScts,
                                           Duration certLifetime) {
  if (NonEmbeddedSCTsCompliant(verifiedScts) == CTPolicyCompliance::Compliant) {
    return CTPolicyCompliance::Compliant;
  }

  uint64_t certIssuanceTime = GetEffectiveCertIssuanceTime(verifiedScts);
  return EmbeddedSCTsCompliant(verifiedScts, certIssuanceTime, certLifetime);
}

}  // namespace ct
}  // namespace mozilla
