/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTPolicyEnforcer_h
#define CTPolicyEnforcer_h

#include "CTLog.h"
#include "CTVerifyResult.h"
#include "mozpkix/Result.h"
#include "mozpkix/Time.h"

namespace mozilla {
namespace ct {

// A helper enum to describe the result of running CheckCTPolicyCompliance on a
// collection of verified SCTs.
enum class CTPolicyCompliance {
  // The connection complied with the certificate policy
  // by including SCTs that satisfy the policy.
  Compliant,
  // The connection did not have enough valid SCTs to comply.
  NotEnoughScts,
  // The connection had enough valid SCTs, but the diversity requirement
  // was not met (the number of CT log operators independent of the CA
  // and of each other is too low).
  NotDiverseScts,
};

// Checks the collected verified SCTs for compliance with the CT policy.
// The policy is based on Chrome's policy as described here:
// https://googlechrome.github.io/CertificateTransparency/ct_policy.html
// This policy (as well as Chrome's), is very similar to Apple's:
// https://support.apple.com/en-us/103214
// Essentially, the policy can be satisfied in two ways, depending on the
// source of the collected SCTs.
// For embedded SCTs, at least one must be from a log that was Admissible
// (Qualified, Usable, or ReadOnly) at the time of the check. There must be
// SCTs from N distinct logs that were Admissible or Retired at the time of the
// check, where N depends on the lifetime of the certificate. If the
// certificate lifetime is less than or equal to 180 days, N is 2. Otherwise, N
// is 3. Among these SCTs, at least two must be issued from distinct log
// operators.
// For SCTs delivered via the TLS handshake or an OCSP response, at least two
// must be from a log that was Admissible at the time of the check. Among these
// SCTs, at least two must be issued from distinct log operators.
//
// |verifiedSct| - SCTs present on the connection along with their verification
// status.
// |certLifetime| - certificate lifetime, based on the notBefore/notAfter
// fields.
CTPolicyCompliance CheckCTPolicyCompliance(const VerifiedSCTList& verifiedScts,
                                           pkix::Duration certLifetime);

}  // namespace ct
}  // namespace mozilla

#endif  // CTPolicyEnforcer_h
