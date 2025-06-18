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
  }

  // Note that the constructor (and SetUp() method) get called once per test,
  // not once for the whole fixture. To make running these tests faster,
  // start the agent once here. A test or two may restart it, but this will
  // be faster than starting it for every test that wants it.
  static void SetUpTestSuite() {
    if (sAgentInfo.processInfo.hProcess != nullptr) {
      // Agent is already running, no need to start it again.
      return;
    }
    GeneratePipeName(L"contentanalysissdk-gtest-", sPipeName);
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetBool(kIsDLPEnabledPref, true));
    MOZ_ALWAYS_SUCCEEDS(
        Preferences::SetString(kPipePathNamePref, sPipeName.get()));
    EnsureAgentStarted();
  }

  static void EnsureAgentStarted() {
    if (sAgentInfo.processInfo.hProcess != nullptr) {
      // Agent is already running, no need to start it again.
      return;
    }
    sAgentInfo = LaunchAgentNormal(L"block", L"warn", sPipeName);
  }
  static void EnsureAgentTerminated() {
    sAgentInfo.TerminateProcess();
    sAgentInfo = MozAgentInfo();
  }

  static void TearDownTestSuite() {
    EnsureAgentTerminated();

    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kPipePathNamePref));
    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kIsDLPEnabledPref));
  }

  void TearDown() override {
    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->TestOnlySetCACmdLineArg(false));
  }
  void AttemptToConnectAndMeasureTelemetry(const nsCString& expectedError);

  void ResetUrlLists() {
    mContentAnalysis->mParsedUrlLists = false;
    mContentAnalysis->mAllowUrlList = {};
    mContentAnalysis->mDenyUrlList = {};
  }

  // This is used to help tests clean up after terminating and restarting
  // the agent.
  RefPtr<ContentAnalysis> mContentAnalysis;
  static nsString sPipeName;
  static MozAgentInfo sAgentInfo;
};

MOZ_RUNINIT nsString ContentAnalysisTelemetryTest::sPipeName;
MOZ_RUNINIT MozAgentInfo ContentAnalysisTelemetryTest::sAgentInfo;

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
                .valueOr(0),
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
  EnsureAgentStarted();

  AttemptToConnectAndMeasureTelemetry(""_ns);
}

TEST_F(ContentAnalysisTelemetryTest, TestConnectionFailureBecauseNoAgent) {
  EnsureAgentTerminated();
  AttemptToConnectAndMeasureTelemetry("NS_ERROR_CONNECTION_REFUSED"_ns);
}

TEST_F(ContentAnalysisTelemetryTest,
       TestConnectionFailureBecauseSignatureVerification) {
  EnsureAgentStarted();
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kClientSignaturePref, "anInvalidSignature"));
  auto restorePref =
      MakeScopeExit([&] { Preferences::ClearUser(kClientSignaturePref); });
  AttemptToConnectAndMeasureTelemetry("NS_ERROR_INVALID_SIGNATURE"_ns);
}

// Sends a request that is expected to return an early result
// because the allow or deny lists will handle it. This means
// this method does not wait for an agent acknowlegement, since
// one will not be sent.
void SendRequestAndWaitForEarlyResult(
    RefPtr<ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    Maybe<bool> expectedShouldAllow) {
  bool gotResponse = false;
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
        EXPECT_TRUE(result);
        if (expectedShouldAllow.isSome()) {
          EXPECT_EQ(*expectedShouldAllow, result->GetShouldAllowContent());
        }
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

  AutoTArray<RefPtr<nsIContentAnalysisRequest>, 1> requests{request.get()};
  MOZ_ALWAYS_SUCCEEDS(contentAnalysis->AnalyzeContentRequestsCallback(
      requests, true, callback));
  RefPtr<CancelableRunnable> timer = QueueTimeoutToMainThread(timedOut);

  mozilla::SpinEventLoopUntil("Waiting for ContentAnalysis result"_ns,
                              [&, timedOut]() {
                                if (timedOut->mValue) {
                                  return true;
                                }
                                return gotResponse;
                              });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_FALSE(timedOut->mValue);
}

void SendRequestAndWaitForResponse(
    RefPtr<ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    Maybe<bool> expectedShouldAllow,
    Maybe<nsIContentAnalysisResponse::Action> expectedAction) {
  bool gotResponse = false;
  bool gotAcknowledgement = false;
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
      [&gotResponse, &gotAcknowledgement, timedOut](nsresult error) {
        if (timedOut->mValue) {
          return;
        }
        const char* errorName = mozilla::GetStaticErrorName(error);
        errorName = errorName ? errorName : "";
        printf("Got error response code %s(%x)\n", errorName, error);
        // Errors should not have errorCode NS_OK
        EXPECT_NE(NS_OK, error);
        gotResponse = true;
        gotAcknowledgement = true;
        FAIL() << "Got error response";
      });

  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawAcknowledgementObserver = MakeRefPtr<RawAcknowledgementObserver>();
  MOZ_ALWAYS_SUCCEEDS(obsServ->AddObserver(
      rawAcknowledgementObserver, "dlp-acknowledgement-sent-raw", false));

  AutoTArray<RefPtr<nsIContentAnalysisRequest>, 1> requests{request.get()};
  MOZ_ALWAYS_SUCCEEDS(contentAnalysis->AnalyzeContentRequestsCallback(
      requests, true, callback));
  RefPtr<CancelableRunnable> timer = QueueTimeoutToMainThread(timedOut);

  mozilla::SpinEventLoopUntil(
      "Waiting for ContentAnalysis result"_ns, [&, timedOut]() {
        if (timedOut->mValue) {
          return true;
        }

        auto acknowledgements =
            rawAcknowledgementObserver->GetAcknowledgements();
        nsCString requestToken;
        MOZ_ALWAYS_SUCCEEDS(request->GetRequestToken(requestToken));
        for (const auto& acknowledgement : acknowledgements) {
          if (nsCString(acknowledgement.request_token()) == requestToken) {
            // Wait for the acknowledgement to happen to avoid background
            // activity that might interfere with other tests.
            gotAcknowledgement = true;
            break;
          }
        }

        return gotResponse && gotAcknowledgement;
      });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_TRUE(gotAcknowledgement);
  EXPECT_FALSE(timedOut->mValue);
  obsServ->RemoveObserver(rawAcknowledgementObserver,
                          "dlp-acknowledgement-sent-raw");
}

TEST_F(ContentAnalysisTelemetryTest, TestSimpleRequest) {
  EnsureAgentStarted();

  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allowSimple");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  nsCString analysisTypeString = "BULK_DATA_ENTRY"_ns;
  auto originalAnalysisTypeCount =
      glean::content_analysis::request_sent_by_analysis_type
          .Get(analysisTypeString)
          .TestGetValue()
          .unwrap()
          .valueOr(0);
  nsCString reasonString = "CLIPBOARD_PASTE"_ns;
  auto originalReasonCount =
      glean::content_analysis::request_sent_by_reason.Get(reasonString)
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

TEST_F(ContentAnalysisTelemetryTest, TestAllowAndDenyLists) {
  EnsureAgentStarted();
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kAllowUrlPref, ".*example\\.com.*"));

  ResetUrlLists();
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allowSimple");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  auto originalAllowCount =
      glean::content_analysis::request_allowed_by_allow_url.TestGetValue()
          .unwrap()
          .valueOr(0);

  SendRequestAndWaitForEarlyResult(mContentAnalysis, request, Some(true));

  EXPECT_EQ(glean::content_analysis::request_allowed_by_allow_url.TestGetValue()
                .unwrap()
                .valueOr(0),
            originalAllowCount + 1);

  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kAllowUrlPref));
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kDenyUrlPref, ".*example\\.com.*"));
  ResetUrlLists();

  auto originalDenyCount =
      glean::content_analysis::request_blocked_by_deny_url.TestGetValue()
          .unwrap()
          .valueOr(0);

  SendRequestAndWaitForEarlyResult(mContentAnalysis, request, Some(false));

  EXPECT_EQ(glean::content_analysis::request_blocked_by_deny_url.TestGetValue()
                .unwrap()
                .valueOr(0),
            originalDenyCount + 1);

  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kDenyUrlPref));
  ResetUrlLists();
}

TEST_F(ContentAnalysisTelemetryTest, TestSimpleAllowResponse) {
  EnsureAgentStarted();

  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allowSimple");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  nsCString allowString = "1000"_ns;  // eAllow
  auto originalAnalysisTypeCount =
      glean::content_analysis::response_action.Get(allowString)
          .TestGetValue()
          .unwrap()
          .valueOr(0);

  SendRequestAndWaitForResponse(mContentAnalysis, request, Some(true),
                                Some(nsIContentAnalysisResponse::eAllow));

  EXPECT_EQ(glean::content_analysis::response_action.Get(allowString)
                .TestGetValue()
                .unwrap()
                .valueOr(0),
            originalAnalysisTypeCount + 1);
}

TEST_F(ContentAnalysisTelemetryTest, TestSimpleBlockResponse) {
  EnsureAgentStarted();

  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString block(L"block");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(block),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  nsCString blockString = "3"_ns;  // eBlock
  auto originalAnalysisTypeCount =
      glean::content_analysis::response_action.Get(blockString)
          .TestGetValue()
          .unwrap()
          .valueOr(0);

  SendRequestAndWaitForResponse(mContentAnalysis, request, Some(false),
                                Some(nsIContentAnalysisResponse::eBlock));

  EXPECT_EQ(glean::content_analysis::response_action.Get(blockString)
                .TestGetValue()
                .unwrap()
                .valueOr(0),
            originalAnalysisTypeCount + 1);
}

TEST_F(ContentAnalysisTelemetryTest, TestConnectionRetry) {
  // This is a little tricky to test because the usual way of
  // establishing a connection is to call ForceRecreateClientForTest(),
  // which counts as a retry. So make sure we have a good connection,
  // then restart the agent, then do another request, which should
  // trigger a retry.
  EnsureAgentStarted();
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allowSimple");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  SendRequestAndWaitForResponse(mContentAnalysis, request, Some(true),
                                Some(nsIContentAnalysisResponse::eAllow));

  EnsureAgentTerminated();
  EnsureAgentStarted();

  auto originalConnectionAttempts =
      glean::content_analysis::connection_attempt.TestGetValue()
          .unwrap()
          .valueOr(0);
  auto originalRetryAttempts =
      glean::content_analysis::connection_attempt_retry.TestGetValue()
          .unwrap()
          .valueOr(0);

  SendRequestAndWaitForResponse(mContentAnalysis, request, Some(true),
                                Some(nsIContentAnalysisResponse::eAllow));

  EXPECT_EQ(glean::content_analysis::connection_attempt.TestGetValue()
                .unwrap()
                .valueOr(0),
            originalConnectionAttempts + 1);
  EXPECT_EQ(glean::content_analysis::connection_attempt_retry.TestGetValue()
                .unwrap()
                .valueOr(0),
            originalRetryAttempts + 1);
}
