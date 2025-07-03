/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTPolicyEnforcer.h"

#include <algorithm>
#include <stdint.h>
#include <stdio.h>

#include "CTLogVerifier.h"
#include "CTVerifyResult.h"
#include "SignedCertificateTimestamp.h"
#include "mozpkix/Time.h"
#include "gtest/gtest.h"
#include "hasht.h"
#include "prtime.h"

namespace mozilla {
namespace ct {

using namespace mozilla::pkix;

class CTPolicyEnforcerTest : public ::testing::Test {
 public:
  void GetLogId(Buffer& logId, size_t logNo) {
    logId.resize(SHA256_LENGTH);
    std::fill(logId.begin(), logId.end(), 0);
    // Just raw-copy |logId| into the output buffer.
    assert(sizeof(logNo) <= logId.size());
    memcpy(logId.data(), &logNo, sizeof(logNo));
  }

  void AddSct(VerifiedSCTList& verifiedScts, size_t logNo,
              CTLogOperatorId operatorId, SCTOrigin origin, uint64_t timestamp,
              CTLogState logState = CTLogState::Admissible) {
    SignedCertificateTimestamp sct;
    sct.version = SignedCertificateTimestamp::Version::V1;
    sct.timestamp = timestamp;
    Buffer logId;
    GetLogId(logId, logNo);
    sct.logId = std::move(logId);
    VerifiedSCT verifiedSct(std::move(sct), origin, operatorId, logState,
                            LOG_TIMESTAMP);
    verifiedScts.push_back(std::move(verifiedSct));
  }

  void AddMultipleScts(VerifiedSCTList& verifiedScts, size_t logsCount,
                       uint8_t operatorsCount, SCTOrigin origin,
                       uint64_t timestamp,
                       CTLogState logState = CTLogState::Admissible) {
    for (size_t logNo = 0; logNo < logsCount; logNo++) {
      CTLogOperatorId operatorId = logNo % operatorsCount;
      AddSct(verifiedScts, logNo, operatorId, origin, timestamp, logState);
    }
  }

  void CheckCompliance(const VerifiedSCTList& verifiedSct,
                       Duration certLifetime,
                       CTPolicyCompliance expectedCompliance) {
    CTPolicyCompliance compliance =
        CheckCTPolicyCompliance(verifiedSct, certLifetime);
    EXPECT_EQ(expectedCompliance, compliance);
  }

 protected:
  const size_t LOG_1 = 1;
  const size_t LOG_2 = 2;
  const size_t LOG_3 = 3;

  const CTLogOperatorId OPERATOR_1 = 1;
  const CTLogOperatorId OPERATOR_2 = 2;
  const CTLogOperatorId OPERATOR_3 = 3;

  const SCTOrigin ORIGIN_EMBEDDED = SCTOrigin::Embedded;
  const SCTOrigin ORIGIN_TLS = SCTOrigin::TLSExtension;
  const SCTOrigin ORIGIN_OCSP = SCTOrigin::OCSPResponse;

  // 1 year of cert lifetime requires 3 SCTs for the embedded case.
  const Duration DEFAULT_LIFETIME = Duration(365 * Time::ONE_DAY_IN_SECONDS);

  // Date.parse("2015-08-15T00:00:00Z")
  const uint64_t TIMESTAMP_1 = 1439596800000L;

  // Date.parse("2016-04-15T00:00:00Z")
  const uint64_t LOG_TIMESTAMP = 1460678400000L;

  // Date.parse("2016-04-01T00:00:00Z")
  const uint64_t BEFORE_RETIREMENT = 1459468800000L;

  // Date.parse("2016-04-16T00:00:00Z")
  const uint64_t AFTER_RETIREMENT = 1460764800000L;
};

TEST_F(CTPolicyEnforcerTest, ConformsToCTPolicyWithNonEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformNotEnoughDiverseNonEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);

  // The implementation attempts to fulfill the non-embedded compliance case
  // first. Because the non-embedded SCTs do not have enough log diversity, the
  // implementation then attempts to fulfill the embedded compliance case.
  // Because there are no embedded SCTs, it returns a "not enough SCTs" error.
  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTPolicyWithEmbeddedSCTs) {
  VerifiedSCTList scts;

  // 3 embedded SCTs required for DEFAULT_LIFETIME.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_2, ORIGIN_EMBEDDED, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformNotEnoughDiverseEmbeddedSCTs) {
  VerifiedSCTList scts;

  // 3 embedded SCTs required for DEFAULT_LIFETIME.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotDiverseScts);
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTPolicyWithPooledNonEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_OCSP, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformToCTPolicyWithPooledEmbeddedSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_OCSP, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughSCTs) {
  VerifiedSCTList scts;

  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, TIMESTAMP_1);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformToCTPolicyNotEnoughFreshSCTs) {
  VerifiedSCTList scts;

  // The results should be the same before and after disqualification,
  // regardless of the delivery method.

  // SCT from before disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, BEFORE_RETIREMENT,
         CTLogState::Retired);
  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);
  // SCT from after disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_TLS, AFTER_RETIREMENT,
         CTLogState::Retired);
  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);

  // Embedded SCT from before disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, BEFORE_RETIREMENT,
         CTLogState::Retired);
  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);

  // Embedded SCT from after disqualification.
  scts.clear();
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_TLS, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, AFTER_RETIREMENT,
         CTLogState::Retired);
  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest, ConformsWithRetiredLogBeforeDisqualificationDate) {
  VerifiedSCTList scts;

  // 3 embedded SCTs required for DEFAULT_LIFETIME.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_2, ORIGIN_EMBEDDED, BEFORE_RETIREMENT,
         CTLogState::Retired);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::Compliant);
}

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformWithRetiredLogAfterDisqualificationDate) {
  VerifiedSCTList scts;

  // 3 embedded SCTs required for DEFAULT_LIFETIME.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  AddSct(scts, LOG_3, OPERATOR_2, ORIGIN_EMBEDDED, AFTER_RETIREMENT,
         CTLogState::Retired);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformWithIssuanceDateAfterDisqualificationDate) {
  VerifiedSCTList scts;

  // 3 embedded SCTs required for DEFAULT_LIFETIME.
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, AFTER_RETIREMENT,
         CTLogState::Retired);
  AddSct(scts, LOG_2, OPERATOR_1, ORIGIN_EMBEDDED, AFTER_RETIREMENT);
  AddSct(scts, LOG_3, OPERATOR_2, ORIGIN_EMBEDDED, AFTER_RETIREMENT);

  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotEnoughScts);
}

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformToCTPolicyNotEnoughUniqueEmbeddedRetiredLogs) {
  VerifiedSCTList scts;

  // Operator #1
  AddSct(scts, LOG_1, OPERATOR_1, ORIGIN_EMBEDDED, TIMESTAMP_1);
  // Operator #2, same retired logs
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, BEFORE_RETIREMENT,
         CTLogState::Retired);
  AddSct(scts, LOG_2, OPERATOR_2, ORIGIN_EMBEDDED, BEFORE_RETIREMENT,
         CTLogState::Retired);

  // 3 embedded SCTs required. However, only 2 are from distinct logs.
  CheckCompliance(scts, DEFAULT_LIFETIME, CTPolicyCompliance::NotDiverseScts);
}

TEST_F(CTPolicyEnforcerTest,
       ConformsToPolicyExactNumberOfSCTsForValidityPeriod) {
  // Test multiple validity periods.
  const struct TestData {
    Duration certLifetime;
    size_t sctsRequired;
  } kTestData[] = {{Duration(90 * Time::ONE_DAY_IN_SECONDS), 2},
                   {Duration(180 * Time::ONE_DAY_IN_SECONDS), 2},
                   {Duration(181 * Time::ONE_DAY_IN_SECONDS), 3},
                   {Duration(365 * Time::ONE_DAY_IN_SECONDS), 3}};

  for (size_t i = 0; i < MOZILLA_CT_ARRAY_LENGTH(kTestData); ++i) {
    SCOPED_TRACE(i);

    Duration certLifetime = kTestData[i].certLifetime;
    size_t sctsRequired = kTestData[i].sctsRequired;

    // Less SCTs than required is not enough.
    for (size_t sctsAvailable = 0; sctsAvailable < sctsRequired;
         ++sctsAvailable) {
      VerifiedSCTList scts;
      AddMultipleScts(scts, sctsAvailable, 1, ORIGIN_EMBEDDED, TIMESTAMP_1);

      CTPolicyCompliance compliance =
          CheckCTPolicyCompliance(scts, certLifetime);
      EXPECT_EQ(CTPolicyCompliance::NotEnoughScts, compliance)
          << "i=" << i << " sctsRequired=" << sctsRequired
          << " sctsAvailable=" << sctsAvailable;
    }

    // Add exactly the required number of SCTs (from 2 operators).
    VerifiedSCTList scts;
    AddMultipleScts(scts, sctsRequired, 2, ORIGIN_EMBEDDED, TIMESTAMP_1);

    CTPolicyCompliance compliance = CheckCTPolicyCompliance(scts, certLifetime);
    EXPECT_EQ(CTPolicyCompliance::Compliant, compliance) << "i=" << i;
  }
}

}  // namespace ct
}  // namespace mozilla
