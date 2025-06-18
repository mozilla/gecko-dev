/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Assertions.h"
#include "mozilla/glean/ContentanalysisMetrics.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsServiceManagerUtils.h"
#include "ContentAnalysis.h"
#include "TestContentAnalysisUtils.h"

using namespace mozilla;
using namespace mozilla::contentanalysis;

class ContentAnalysisTelemetryTest : public testing::Test {
 protected:
  ContentAnalysisTelemetryTest() {
    auto* logmodule = LogModule::Get("contentanalysis");
    logmodule->SetLevel(LogLevel::Verbose);

    nsCOMPtr<nsIContentAnalysis> caSvc =
        do_GetService("@mozilla.org/contentanalysis;1");
    MOZ_ASSERT(caSvc);
    mContentAnalysis = static_cast<ContentAnalysis*>(caSvc.get());
    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->TestOnlySetCACmdLineArg(true));
    GeneratePipeName(L"contentanalysissdk-gtest-", sPipeName);
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetBool(kIsDLPEnabledPref, true));
    MOZ_ALWAYS_SUCCEEDS(
        Preferences::SetString(kPipePathNamePref, sPipeName.get()));
  }

  void TearDown() override {
    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->TestOnlySetCACmdLineArg(false));

    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kPipePathNamePref));
    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kIsDLPEnabledPref));
  }
  void AttemptToConnectAndMeasureTelemetry(const nsCString& expectedError);

  // This is used to help tests clean up after terminating and restarting
  // the agent.
  RefPtr<ContentAnalysis> mContentAnalysis;
  static nsString sPipeName;
};

MOZ_RUNINIT nsString ContentAnalysisTelemetryTest::sPipeName;

void ContentAnalysisTelemetryTest::AttemptToConnectAndMeasureTelemetry(
    const nsCString& expectedError) {
  ASSERT_TRUE(expectedError.IsEmpty() ||
              expectedError.EqualsLiteral("NS_ERROR_CONNECTION_REFUSED") ||
              expectedError.EqualsLiteral("NS_ERROR_INVALID_SIGNATURE"))
  << "Unexpected expectedError in test!";
  auto originalConnectionAttempts =
      glean::content_analysis::connection_attempt.TestGetValue()
          .unwrap()
          .valueOr(0);
  auto originalConnectionRefusedFailures =
      glean::content_analysis::connection_failure
          .Get(nsCString("NS_ERROR_CONNECTION_REFUSED"))
          .TestGetValue()
          .unwrap()
          .valueOr(0);
  auto originalInvalidSignatureFailures =
      glean::content_analysis::connection_failure
          .Get(nsCString("NS_ERROR_INVALID_SIGNATURE"))
          .TestGetValue()
          .unwrap()
          .valueOr(0);

  mContentAnalysis->ForceRecreateClientForTest();

  RefPtr timedOut = MakeRefPtr<media::Refcountable<BoolStruct>>();
  RefPtr<CancelableRunnable> timer = QueueTimeoutToMainThread(timedOut);
  mozilla::SpinEventLoopUntil("Waiting for attempt"_ns, [&, timedOut]() {
    if (timedOut->mValue) {
      return true;
    }
    return !mContentAnalysis->GetCreatingClientForTest();
  });

  timer->Cancel();
  EXPECT_FALSE(timedOut->mValue);

  EXPECT_EQ(glean::content_analysis::connection_attempt.TestGetValue()
                .unwrap()
                .value(),
            originalConnectionAttempts + 1);
  EXPECT_EQ(
      glean::content_analysis::connection_failure
          .Get(nsCString("NS_ERROR_CONNECTION_REFUSED"))
          .TestGetValue()
          .unwrap()
          .valueOr(0),
      originalConnectionRefusedFailures +
          (expectedError.EqualsASCII("NS_ERROR_CONNECTION_REFUSED") ? 1 : 0));
  EXPECT_EQ(
      glean::content_analysis::connection_failure
          .Get(nsCString("NS_ERROR_INVALID_SIGNATURE"))
          .TestGetValue()
          .unwrap()
          .valueOr(0),
      originalInvalidSignatureFailures +
          (expectedError.EqualsASCII("NS_ERROR_INVALID_SIGNATURE") ? 1 : 0));
}

TEST_F(ContentAnalysisTelemetryTest, TestConnectionSuccess) {
  auto agentInfo = LaunchAgentNormal(L"block", L"warn", sPipeName);
  ASSERT_TRUE(agentInfo.client);
  auto terminateAgent = MakeScopeExit([&] { agentInfo.TerminateProcess(); });

  AttemptToConnectAndMeasureTelemetry(""_ns);
}

TEST_F(ContentAnalysisTelemetryTest, TestConnectionFailureBecauseNoAgent) {
  AttemptToConnectAndMeasureTelemetry("NS_ERROR_CONNECTION_REFUSED"_ns);
}

TEST_F(ContentAnalysisTelemetryTest,
       TestConnectionFailureBecauseSignatureVerification) {
  auto agentInfo = LaunchAgentNormal(L"block", L"warn", sPipeName);
  ASSERT_TRUE(agentInfo.client);
  auto terminateAgent = MakeScopeExit([&] { agentInfo.TerminateProcess(); });
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kClientSignaturePref, "anInvalidSignature"));
  auto restorePref =
      MakeScopeExit([&] { Preferences::ClearUser(kClientSignaturePref); });
  AttemptToConnectAndMeasureTelemetry("NS_ERROR_INVALID_SIGNATURE"_ns);
}
