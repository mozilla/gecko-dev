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
#include "nsIURI.h"
#include "nsIURIMutator.h"
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

void SendRequestAndWaitForResponse(
    RefPtr<ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    Maybe<bool> expectedShouldAllow,
    Maybe<nsIContentAnalysisResponse::Action> expectedAction) {
  std::atomic<bool> gotResponse = false;
  nsCString requestToken;
  MOZ_ALWAYS_SUCCEEDS(request->GetRequestToken(requestToken));
  if (requestToken.IsEmpty()) {
    MOZ_ALWAYS_SUCCEEDS(request->SetRequestToken(GenerateUUID()));
  }

  // Make timedOut a RefPtr so if we get a response from content analysis
  // after this function has finished we can safely check that (and don't
  // start accessing stack values that don't exist anymore)
  RefPtr timedOut = MakeRefPtr<media::Refcountable<BoolStruct>>();
  auto callback = MakeRefPtr<ContentAnalysisCallback>(
      [&, timedOut](nsIContentAnalysisResult* result) {
        if (timedOut->mValue) {
          return;
        }
        nsCOMPtr<nsIContentAnalysisResponse> response =
            do_QueryInterface(result);
        EXPECT_TRUE(response);
        if (expectedShouldAllow.isSome()) {
          EXPECT_EQ(*expectedShouldAllow, response->GetShouldAllowContent());
        }
        if (expectedAction.isSome()) {
          EXPECT_EQ(*expectedAction, response->GetAction());
        }
        nsCString requestToken, originalRequestToken;
        MOZ_ALWAYS_SUCCEEDS(response->GetRequestToken(requestToken));
        MOZ_ALWAYS_SUCCEEDS(request->GetRequestToken(originalRequestToken));
        EXPECT_EQ(originalRequestToken, requestToken);
        nsCString userActionId, originalUserActionId;
        MOZ_ALWAYS_SUCCEEDS(response->GetUserActionId(userActionId));
        MOZ_ALWAYS_SUCCEEDS(request->GetUserActionId(originalUserActionId));
        EXPECT_EQ(originalUserActionId, userActionId);
        gotResponse = true;
      },
      [&gotResponse, timedOut](nsresult error) {
        if (timedOut->mValue) {
          return;
        }
        const char* errorName = mozilla::GetStaticErrorName(error);
        errorName = errorName ? errorName : "";
        printf("Got error response code %s(%x)\n", errorName, error);
        // Errors should not have errorCode NS_OK
        EXPECT_NE(NS_OK, error);
        gotResponse = true;
        FAIL() << "Got error response";
      });

  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();

  AutoTArray<RefPtr<nsIContentAnalysisRequest>, 1> requests{request.get()};
  MOZ_ALWAYS_SUCCEEDS(contentAnalysis->AnalyzeContentRequestsCallback(
      requests, true, callback));
  RefPtr<CancelableRunnable> timer = QueueTimeoutToMainThread(timedOut);

  mozilla::SpinEventLoopUntil("Waiting for ContentAnalysis result"_ns,
                              [&, timedOut]() {
                                if (timedOut->mValue) {
                                  return true;
                                }

                                return gotResponse.load();
                              });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_FALSE(timedOut->mValue);
}

TEST_F(ContentAnalysisTelemetryTest, TestSimpleRequest) {
  auto agentInfo = LaunchAgentNormal(L"block", L"warn", mPipeName);
  ASSERT_TRUE(agentInfo.client);
  auto terminateAgent = MakeScopeExit([&] { agentInfo.TerminateProcess(); });

  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allowSimple");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  nsCString analysisTypeString = "3"_ns;  // eBulkDataEntry
  auto originalAnalysisTypeCount =
      glean::content_analysis::connection_failure.Get(analysisTypeString)
          .TestGetValue()
          .unwrap()
          .valueOr(0);
  nsCString reasonString = "1"_ns;  // eClipboardPaste
  auto originalReasonCount =
      glean::content_analysis::connection_failure.Get(reasonString)
          .TestGetValue()
          .unwrap()
          .valueOr(0);

  SendRequestAndWaitForResponse(mContentAnalysis, request, Some(true),
                                Some(nsIContentAnalysisResponse::eAllow));

  EXPECT_EQ(glean::content_analysis::request_sent_by_analysis_type
                .Get(analysisTypeString)
                .TestGetValue()
                .unwrap()
                .valueOr(0),
            originalAnalysisTypeCount + 1);
  EXPECT_EQ(glean::content_analysis::request_sent_by_reason.Get(reasonString)
                .TestGetValue()
                .unwrap()
                .valueOr(0),
            originalReasonCount + 1);
}
