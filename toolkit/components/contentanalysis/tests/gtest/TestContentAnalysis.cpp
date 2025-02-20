/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/media/MediaUtils.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsJSUtils.h"
#include "ContentAnalysis.h"
#include "SpecialSystemDirectory.h"
#include "TestContentAnalysisUtils.h"
#include <processenv.h>
#include <synchapi.h>
#include <vector>

const char* kAllowUrlPref = "browser.contentanalysis.allow_url_regex_list";
const char* kDenyUrlPref = "browser.contentanalysis.deny_url_regex_list";
const char* kPipePathNamePref = "browser.contentanalysis.pipe_path_name";
const char* kIsDLPEnabledPref = "browser.contentanalysis.enabled";
const char* kDefaultResultPref = "browser.contentanalysis.default_result";
const char* kTimeoutPref = "browser.contentanalysis.agent_timeout";
const char* kTimeoutResultPref = "browser.contentanalysis.timeout_result";
const char* kClientSignaturePref = "browser.contentanalysis.client_signature";

using namespace mozilla;
using namespace mozilla::contentanalysis;

class ContentAnalysisTest : public testing::Test {
 protected:
  ContentAnalysisTest() {
    auto* logmodule = LogModule::Get("contentanalysis");
    logmodule->SetLevel(LogLevel::Verbose);
    MOZ_ALWAYS_SUCCEEDS(
        Preferences::SetString(kPipePathNamePref, mPipeName.get()));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetBool(kIsDLPEnabledPref, true));

    nsCOMPtr<nsIContentAnalysis> caSvc =
        do_GetService("@mozilla.org/contentanalysis;1");
    MOZ_ASSERT(caSvc);
    mContentAnalysis = static_cast<ContentAnalysis*>(caSvc.get());

    // Tests run earlier could have altered these values
    mContentAnalysis->mParsedUrlLists = false;
    mContentAnalysis->mAllowUrlList = {};
    mContentAnalysis->mDenyUrlList = {};

    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->TestOnlySetCACmdLineArg(true));

    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kAllowUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ""));

    bool isActive = false;
    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->GetIsActive(&isActive));
    EXPECT_TRUE(isActive);
  }

  // Note that the constructor (and SetUp() method) get called once per test,
  // not once for the whole fixture. Because Firefox does not currently
  // reconnect to an agent after the DLP pipe is closed (bug 1888293), we only
  // want to create the agent once and make sure the same process stays alive
  // through all of these tests.
  static void SetUpTestSuite() {
    GeneratePipeName(L"contentanalysissdk-gtest-", mPipeName);
    StartAgent();
  }

  static void TearDownTestSuite() { mAgentInfo.TerminateProcess(); }

  static void StartAgent() {
    mAgentInfo = LaunchAgentNormal(L"block", mPipeName);
  }

  void TearDown() override {
    mContentAnalysis->mParsedUrlLists = false;
    mContentAnalysis->mAllowUrlList = {};
    mContentAnalysis->mDenyUrlList = {};

    MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->TestOnlySetCACmdLineArg(false));

    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kAllowUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ""));
    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kPipePathNamePref));
    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kIsDLPEnabledPref));
  }

  already_AddRefed<nsIContentAnalysisRequest> CreateRequest(const char* aUrl) {
    nsCOMPtr<nsIURI> uri;
    MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), aUrl));
    // We will only use the URL and, implicitly, the analysisType
    // (behavior differs for download vs other types).
    return RefPtr(new ContentAnalysisRequest(
                      nsIContentAnalysisRequest::AnalysisType::eFileTransfer,
                      nsIContentAnalysisRequest::Reason::eFilePickerDialog,
                      EmptyString(), false, EmptyCString(), uri,
                      nsIContentAnalysisRequest::OperationType::eDroppedText,
                      nullptr))
        .forget();
  }

  enum class CancelMechanism {
    // Wait for the service to assign our request tokens, then cancel using
    // that (deprecated)
    eCancelByRequestToken,
    // Wait for the service to assign our requests a user action ID, then cancel
    // using that.
    eCancelByUserActionId,
  };

  nsresult SendRequestsCancelAndExpectResponse(
      RefPtr<ContentAnalysis> contentAnalysis,
      const nsTArray<RefPtr<nsIContentAnalysisRequest>>& requests,
      CancelMechanism aCancelMechanism, bool aExpectFailure);
  RefPtr<ContentAnalysisDiagnosticInfo> GetDiagnosticInfo(
      RefPtr<ContentAnalysis> contentAnalysis);
  RefPtr<ContentAnalysis> mContentAnalysis;
  static nsString mPipeName;
  static MozAgentInfo mAgentInfo;

  // Proxies for private members of ContentAnalysis.  TEST_F
  // creates new subclasses -- they do not inherit `friend`s.
  // (FRIEND_TEST is another more verbose solution.)
  using UrlFilterResult = ContentAnalysis::UrlFilterResult;
  UrlFilterResult FilterByUrlLists(nsIContentAnalysisRequest* aReq) {
    // For testing, just pull the URI from the request.
    nsCOMPtr<nsIURI> uri;
    MOZ_ALWAYS_SUCCEEDS(aReq->GetUrl(getter_AddRefs(uri)));
    MOZ_ASSERT(uri);
    return mContentAnalysis->FilterByUrlLists(aReq, uri);
  }
};
MOZ_RUNINIT nsString ContentAnalysisTest::mPipeName;
MOZ_RUNINIT MozAgentInfo ContentAnalysisTest::mAgentInfo;

TEST_F(ContentAnalysisTest, AllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kAllowUrlPref, ".*\\.org/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
}

TEST_F(ContentAnalysisTest, DefaultAllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kAllowUrlPref));
  RefPtr<nsIContentAnalysisRequest> car = CreateRequest("about:home");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("about:blank");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("about:srcdoc");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("https://example.com/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
}

TEST_F(ContentAnalysisTest, MultipleAllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(
      kAllowUrlPref, ".*\\.org/match.* .*\\.net/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("https://example.net/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eAllow);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
}

TEST_F(ContentAnalysisTest, DenyUrlList) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kDenyUrlPref, ".*\\.com/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
}

TEST_F(ContentAnalysisTest, MultipleDenyUrlList) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(
      kDenyUrlPref, ".*\\.com/match.* .*\\.biz/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eCheck);
  car = CreateRequest("https://example.com/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
  car = CreateRequest("https://example.biz/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
}

TEST_F(ContentAnalysisTest, DenyOverridesAllowUrlList) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kAllowUrlPref, ".*\\.org/match.*"));
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(kDenyUrlPref, ".*.org/match.*"));
  RefPtr<nsIContentAnalysisRequest> car =
      CreateRequest("https://example.org/matchme/");
  ASSERT_EQ(FilterByUrlLists(car), UrlFilterResult::eDeny);
}

nsCOMPtr<nsIURI> GetExampleDotComURI() {
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), "https://example.com"));
  return uri;
}
nsCOMPtr<nsIURI> GetExampleDotComWithPathURI() {
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(
      NS_NewURI(getter_AddRefs(uri), "https://example.com/path"));
  return uri;
}

struct BoolStruct {
  bool mValue = false;
};

RefPtr<ContentAnalysisDiagnosticInfo> ContentAnalysisTest::GetDiagnosticInfo(
    RefPtr<ContentAnalysis> contentAnalysis) {
  dom::AutoJSAPI jsapi;
  // We're using this context to deserialize, stringify, and print a message
  // manager message here. Since the messages are always sent from and to system
  // scopes, we need to do this in a system scope, or attempting to deserialize
  // certain privileged objects will fail.
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();
  bool gotResponse = false;
  RefPtr timedOut = MakeRefPtr<media::Refcountable<BoolStruct>>();
  dom::Promise* promise = nullptr;
  RefPtr<ContentAnalysisDiagnosticInfo> diagnosticInfo = nullptr;
  MOZ_ALWAYS_SUCCEEDS(mContentAnalysis->GetDiagnosticInfo(cx, &promise));
  auto result = promise->ThenWithCycleCollectedArgs(
      [&, timedOut](JSContext* aCx, JS::Handle<JS::Value> aValue,
                    ErrorResult& aRv) -> already_AddRefed<dom::Promise> {
        if (timedOut->mValue) {
          return nullptr;
        }
        EXPECT_TRUE(aValue.isObject());
        JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
        JS::Rooted<JS::Value> value(aCx);
        EXPECT_TRUE(JS_GetProperty(aCx, obj, "connectedToAgent", &value));
        bool connectedToAgent = JS::ToBoolean(value);
        EXPECT_TRUE(JS_GetProperty(aCx, obj, "agentPath", &value));
        nsAutoJSString agentPath;
        EXPECT_TRUE(agentPath.init(aCx, value));
        EXPECT_TRUE(
            JS_GetProperty(aCx, obj, "failedSignatureVerification", &value));
        bool failedSignatureVerification = JS::ToBoolean(value);
        EXPECT_TRUE(JS_GetProperty(aCx, obj, "requestCount", &value));
        int64_t requestCount;
        EXPECT_TRUE(JS::ToInt64(aCx, value, &requestCount));
        diagnosticInfo = MakeRefPtr<ContentAnalysisDiagnosticInfo>(
            connectedToAgent, agentPath, failedSignatureVerification,
            requestCount);

        gotResponse = true;
        return nullptr;
      });

  RefPtr<CancelableRunnable> timer =
      NS_NewCancelableRunnableFunction("GetDiagnosticInfo timeout", [&] {
        if (!gotResponse) {
          timedOut->mValue = true;
        }
      });
  constexpr uint32_t kDiagnosticTimeout = 10000;
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), kDiagnosticTimeout);
  mozilla::SpinEventLoopUntil(
      "Waiting for GetDiagnosticInfo result"_ns,
      [&, timedOut]() { return gotResponse || timedOut->mValue; });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_FALSE(timedOut->mValue);

  return diagnosticInfo;
}

class RawAcknowledgementObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  const std::vector<content_analysis::sdk::ContentAnalysisAcknowledgement>&
  GetAcknowledgements() {
    return mAcknowledgements;
  }

 private:
  ~RawAcknowledgementObserver() = default;
  std::vector<content_analysis::sdk::ContentAnalysisAcknowledgement>
      mAcknowledgements;
};

NS_IMPL_ISUPPORTS(RawAcknowledgementObserver, nsIObserver);

NS_IMETHODIMP RawAcknowledgementObserver::Observe(nsISupports* aSubject,
                                                  const char* aTopic,
                                                  const char16_t* aData) {
  std::wstring dataWideString(reinterpret_cast<const wchar_t*>(aData));
  std::vector<uint8_t> dataVector(dataWideString.size());
  for (size_t i = 0; i < dataWideString.size(); ++i) {
    // Since this data is really bytes and not a null-terminated string, the
    // calling code adds 0xFF00 to every member to ensure there are no 0 values.
    dataVector[i] = static_cast<uint8_t>(dataWideString[i] - 0xFF00);
  }
  content_analysis::sdk::ContentAnalysisAcknowledgement request;
  EXPECT_TRUE(request.ParseFromArray(dataVector.data(), dataVector.size()));
  mAcknowledgements.push_back(std::move(request));
  return NS_OK;
}

nsresult ContentAnalysisTest::SendRequestsCancelAndExpectResponse(
    RefPtr<ContentAnalysis> contentAnalysis,
    const nsTArray<RefPtr<nsIContentAnalysisRequest>>& requests,
    CancelMechanism aCancelMechanism, bool aExpectFailure) {
  bool gotResponse = false;
  // Make timedOut a RefPtr so if we get a response from content analysis
  // after this function has finished we can safely check that (and don't
  // start accessing stack values that don't exist anymore)
  RefPtr timedOut = MakeRefPtr<media::Refcountable<BoolStruct>>();
  auto callback = MakeRefPtr<ContentAnalysisCallback>(
      [&, timedOut, aExpectFailure](nsIContentAnalysisResult* result) {
        if (timedOut->mValue) {
          return;
        }
        bool shouldAllow;
        MOZ_ALWAYS_SUCCEEDS(result->GetShouldAllowContent(&shouldAllow));
        EXPECT_EQ(false, shouldAllow);
        EXPECT_EQ(false, aExpectFailure);
        gotResponse = true;
      },
      [&gotResponse, timedOut, aExpectFailure](nsresult error) {
        if (timedOut->mValue) {
          return;
        }
        const char* errorName = mozilla::GetStaticErrorName(error);
        errorName = errorName ? errorName : "";
        printf("Got error response code %s(%x)\n", errorName, error);
        // Errors should not have errorCode NS_OK
        EXPECT_NE(NS_OK, error);
        gotResponse = true;
        EXPECT_EQ(true, aExpectFailure);
      });

  nsresult rv = contentAnalysis->AnalyzeContentRequestsCallback(
      requests, true /* autoAcknowledge */, callback);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<CancelableRunnable> timer = NS_NewCancelableRunnableFunction(
      "SendRequestsCancelAndExpectResponse timeout", [&] {
        if (!gotResponse) {
          timedOut->mValue = true;
        }
      });
#if defined(MOZ_ASAN)
  // This can be pretty slow on ASAN builds (bug 1895256)
  constexpr uint32_t kCATimeout = 25000;
#else
  constexpr uint32_t kCATimeout = 10000;
#endif
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), kCATimeout);

  // The user action ID should be set by now, whether we set it or not.
  nsAutoCString userActionId;
  MOZ_ALWAYS_SUCCEEDS(requests[0]->GetUserActionId(userActionId));
  EXPECT_TRUE(!userActionId.IsEmpty());

  bool hasCanceledRequest = false;
  if (aCancelMechanism == CancelMechanism::eCancelByUserActionId) {
    MOZ_ALWAYS_SUCCEEDS(
        contentAnalysis->CancelRequestsByUserAction(userActionId));
    hasCanceledRequest = true;
  }

  mozilla::SpinEventLoopUntil(
      "Waiting for ContentAnalysis result"_ns, [&, timedOut]() {
        if (!hasCanceledRequest) {
          // Internally, GetFinalRequests expands the request list
          // asynchronously.  We need to wait for that.
          // (In the case of this test, nothing actually needs to be expanded.)
          if (aCancelMechanism == CancelMechanism::eCancelByRequestToken) {
            nsAutoCString requestToken;
            MOZ_ALWAYS_SUCCEEDS(requests[0]->GetRequestToken(requestToken));
            if (!requestToken.IsEmpty()) {
              MOZ_ALWAYS_SUCCEEDS(
                  contentAnalysis->CancelRequestsByRequestToken(requestToken));
              hasCanceledRequest = true;
            }
          }
        }
        return gotResponse || timedOut->mValue;
      });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_FALSE(timedOut->mValue);
  return NS_OK;
}

void SendRequestAndExpectResponse(
    RefPtr<ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    Maybe<bool> expectedShouldAllow,
    Maybe<nsIContentAnalysisResponse::Action> expectedAction,
    Maybe<bool> expectedIsCached) {
  std::atomic<bool> gotResponse = false;
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
          bool shouldAllow = false;
          MOZ_ALWAYS_SUCCEEDS(response->GetShouldAllowContent(&shouldAllow));
          EXPECT_EQ(*expectedShouldAllow, shouldAllow);
        }
        if (expectedAction.isSome()) {
          nsIContentAnalysisResponse::Action action;
          MOZ_ALWAYS_SUCCEEDS(response->GetAction(&action));
          EXPECT_EQ(*expectedAction, action);
        }
        if (expectedIsCached.isSome()) {
          bool isCached;
          MOZ_ALWAYS_SUCCEEDS(response->GetIsCachedResponse(&isCached));
          EXPECT_EQ(*expectedIsCached, isCached);
        }
        nsCString requestToken, originalRequestToken;
        MOZ_ALWAYS_SUCCEEDS(response->GetRequestToken(requestToken));
        MOZ_ALWAYS_SUCCEEDS(request->GetRequestToken(originalRequestToken));
        EXPECT_EQ(originalRequestToken, requestToken);
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
      requests, false, callback));
  RefPtr<CancelableRunnable> timer = NS_NewCancelableRunnableFunction(
      "SendRequestAndExpectResponse timeout", [&] {
        if (!gotResponse.load()) {
          timedOut->mValue = true;
        }
      });
#if defined(MOZ_ASAN)
  // This can be pretty slow on ASAN builds (bug 1895256)
  constexpr uint32_t kCATimeout = 25000;
#else
  constexpr uint32_t kCATimeout = 10000;
#endif
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), kCATimeout);
  mozilla::SpinEventLoopUntil(
      "Waiting for ContentAnalysis result"_ns,
      [&, timedOut]() { return gotResponse.load() || timedOut->mValue; });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_FALSE(timedOut->mValue);
}
void SendRequestAndExpectNoAgentResponse(
    RefPtr<ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    bool expectedShouldAllow = false,
    nsIContentAnalysisResponse::CancelError expectedCancelError =
        nsIContentAnalysisResponse::CancelError::eNoAgent) {
  std::atomic<bool> gotResponse = false;
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
        EXPECT_EQ(expectedCancelError, response->GetCancelError());
        EXPECT_EQ(expectedShouldAllow, response->GetShouldAllowContent());
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
      requests, false, callback));
  RefPtr<CancelableRunnable> timer =
      NS_NewCancelableRunnableFunction("Content Analysis timeout", [&] {
        if (!gotResponse.load()) {
          timedOut->mValue = true;
        }
      });
#if defined(MOZ_ASAN)
  // This can be pretty slow on ASAN builds (bug 1895256)
  constexpr uint32_t kCATimeout = 25000;
#else
  constexpr uint32_t kCATimeout = 10000;
#endif
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), kCATimeout);
  mozilla::SpinEventLoopUntil(
      "Waiting for ContentAnalysis result"_ns,
      [&, timedOut]() { return gotResponse.load() || timedOut->mValue; });
  timer->Cancel();
  EXPECT_TRUE(gotResponse);
  EXPECT_FALSE(timedOut->mValue);
}

void YieldMainThread(uint32_t timeInMs) {
  std::atomic<bool> timeExpired = false;
  // The timer gets cleared on the main thread, so we need to yield the main
  // thread for this to work
  RefPtr<CancelableRunnable> timer = NS_NewCancelableRunnableFunction(
      "Content Analysis yielding", [&] { timeExpired = true; });
  // Wait for longer than the cache timeout
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), timeInMs);
  mozilla::SpinEventLoopUntil("Waiting for Content Analysis yielding"_ns,
                              [&]() { return timeExpired.load(); });
  timer->Cancel();
}

TEST_F(ContentAnalysisTest, SendAllowedTextToAgent_GetAllowedResponse) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request, Some(true),
                               Some(nsIContentAnalysisResponse::eAllow),
                               Some(false));
}

TEST_F(ContentAnalysisTest, SendBlockedTextToAgent_GetBlockResponse) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString block(L"block");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(block),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request, Some(false),
                               Some(nsIContentAnalysisResponse::eBlock),
                               Some(false));
}

TEST_F(ContentAnalysisTest,
       RestartAgent_SendAllowedTextToAgent_GetAllowedResponse) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  mAgentInfo.TerminateProcess();
  StartAgent();
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request, Some(true),
                               Some(nsIContentAnalysisResponse::eAllow),
                               Some(false));
}

TEST_F(ContentAnalysisTest, TerminateAgent_SendAllowedTextToAgent_GetError) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  mAgentInfo.TerminateProcess();
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectNoAgentResponse(mContentAnalysis, request);
  StartAgent();
  SendRequestAndExpectResponse(mContentAnalysis, request, Some(true),
                               Some(nsIContentAnalysisResponse::eAllow),
                               Some(false));
}

TEST_F(ContentAnalysisTest,
       TerminateAgent_SendAllowedTextToAgentWithDefaultAllow_GetAllowResponse) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetInt(kDefaultResultPref, 2));
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  mAgentInfo.TerminateProcess();
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectNoAgentResponse(mContentAnalysis, request, true);
  StartAgent();

  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kDefaultResultPref));
}

class RawRequestObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  RawRequestObserver() {}

  const std::vector<content_analysis::sdk::ContentAnalysisRequest>&
  GetRequests() {
    return mRequests;
  }

 private:
  ~RawRequestObserver() = default;
  std::vector<content_analysis::sdk::ContentAnalysisRequest> mRequests;
};

NS_IMPL_ISUPPORTS(RawRequestObserver, nsIObserver);

NS_IMETHODIMP RawRequestObserver::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const char16_t* aData) {
  std::wstring dataWideString(reinterpret_cast<const wchar_t*>(aData));
  std::vector<uint8_t> dataVector(dataWideString.size());
  for (size_t i = 0; i < dataWideString.size(); ++i) {
    // Since this data is really bytes and not a null-terminated string, the
    // calling code adds 0xFF00 to every member to ensure there are no 0 values.
    dataVector[i] = static_cast<uint8_t>(dataWideString[i] - 0xFF00);
  }
  content_analysis::sdk::ContentAnalysisRequest request;
  EXPECT_TRUE(request.ParseFromArray(dataVector.data(), dataVector.size()));
  mRequests.push_back(std::move(request));
  return NS_OK;
}

TEST_F(ContentAnalysisTest, CheckRawRequestWithText) {
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetInt(kTimeoutPref, 65));
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));
  time_t now = time(nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request, Nothing(), Nothing(),
                               Some(false));
  auto requests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(1), requests.size());
  time_t t = requests[0].expires_at();
  time_t secs_remaining = t - now;
  // There should be around 65 seconds remaining
  EXPECT_LE(abs(secs_remaining - 65), 2);
  const auto& request_url = requests[0].request_data().url();
  EXPECT_EQ(uri->GetSpecOrDefault(),
            nsCString(request_url.data(), request_url.size()));
  const auto& request_text = requests[0].text_content();
  EXPECT_EQ(nsCString("allow"),
            nsCString(request_text.data(), request_text.size()));

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kTimeoutPref));
}

TEST_F(ContentAnalysisTest, CheckRawRequestWithFile) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsCOMPtr<nsIFile> file;
  MOZ_ALWAYS_SUCCEEDS(GetSpecialSystemDirectory(OS_CurrentWorkingDirectory,
                                                getter_AddRefs(file)));
  nsString allowRelativePath(L"allowedFile.txt");
  MOZ_ALWAYS_SUCCEEDS(file->AppendRelativePath(allowRelativePath));
  nsString allowPath;
  MOZ_ALWAYS_SUCCEEDS(file->GetPath(allowPath));

  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, allowPath, true,
      EmptyCString(), uri, nsIContentAnalysisRequest::OperationType::eClipboard,
      nullptr);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));

  SendRequestAndExpectResponse(mContentAnalysis, request, Nothing(), Nothing(),
                               Some(false));
  auto requests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(1), requests.size());
  const auto& request_url = requests[0].request_data().url();
  EXPECT_EQ(uri->GetSpecOrDefault(),
            nsCString(request_url.data(), request_url.size()));
  const auto& request_file_path = requests[0].file_path();
  EXPECT_EQ(NS_ConvertUTF16toUTF8(allowPath),
            nsCString(request_file_path.data(), request_file_path.size()));

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
}

TEST_F(ContentAnalysisTest, CheckTwoRequestsHaveDifferentUserActionId) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow1(L"allowMe");
  nsCOMPtr<nsIContentAnalysisRequest> request1 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow1),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  // Use different text so the request doesn't match the cache
  nsString allow2(L"allowMeAgain");
  nsCOMPtr<nsIContentAnalysisRequest> request2 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow2),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));

  SendRequestAndExpectResponse(mContentAnalysis, request1, Nothing(), Nothing(),
                               Some(false));
  SendRequestAndExpectResponse(mContentAnalysis, request2, Nothing(), Nothing(),
                               Some(false));
  auto requests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(2), requests.size());
  EXPECT_NE(requests[0].user_action_id(), requests[1].user_action_id());

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
}

TEST_F(ContentAnalysisTest,
       CheckRequestTokensCanCancelAndHaveSameUserActionId) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow1(L"allowMe");
  RefPtr<nsIContentAnalysisRequest> request1 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow1),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  // Use different text so the request doesn't match the cache
  nsString allow2(L"allowMeAgain");
  RefPtr<nsIContentAnalysisRequest> request2 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow2),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  nsTArray<RefPtr<nsIContentAnalysisRequest>> requests{request1, request2};
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));

  nsresult rv = SendRequestsCancelAndExpectResponse(
      mContentAnalysis, requests, CancelMechanism::eCancelByRequestToken,
      false /* aExpectFailure */);
  EXPECT_EQ(rv, NS_OK);

  auto rawRequests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(2), rawRequests.size());
  EXPECT_EQ(rawRequests[0].user_action_id(), rawRequests[1].user_action_id());

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
}

TEST_F(ContentAnalysisTest,
       CheckAssignedUserActionIdCanCancelAndHaveSameUserActionId) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow1(L"allowMe");
  RefPtr<nsIContentAnalysisRequest> request1 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow1),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  // Use different text so the request doesn't match the cache
  nsString allow2(L"allowMeAgain");
  RefPtr<nsIContentAnalysisRequest> request2 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow2),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  nsTArray<RefPtr<nsIContentAnalysisRequest>> requests{request1, request2};
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));

  nsresult rv = SendRequestsCancelAndExpectResponse(
      mContentAnalysis, requests, CancelMechanism::eCancelByUserActionId,
      false /* aExpectFailure */);
  EXPECT_EQ(rv, NS_OK);

  auto rawRequests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(2), rawRequests.size());
  EXPECT_EQ(rawRequests[0].user_action_id(), rawRequests[1].user_action_id());

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
}

static nsCString GenerateUUID() {
  nsID id = nsID::GenerateUUID();
  return nsCString(id.ToString().get());
}

TEST_F(ContentAnalysisTest,
       CheckGivenUserActionIdCanCancelAndHaveSameUserActionId) {
  nsCString userActionId = GenerateUUID();
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();

  nsString allow1(L"allowMe");
  RefPtr<nsIContentAnalysisRequest> request1 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow1),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr, nullptr,
      nsCString(userActionId));

  // Use different text so the request doesn't match the cache
  nsString allow2(L"allowMeAgain");
  RefPtr<nsIContentAnalysisRequest> request2 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow2),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr, nullptr,
      nsCString(userActionId));
  nsTArray<RefPtr<nsIContentAnalysisRequest>> requests{request1, request2};
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawRequestObserver = MakeRefPtr<RawRequestObserver>();
  MOZ_ALWAYS_SUCCEEDS(
      obsServ->AddObserver(rawRequestObserver, "dlp-request-sent-raw", false));
  nsresult rv = SendRequestsCancelAndExpectResponse(
      mContentAnalysis, requests, CancelMechanism::eCancelByUserActionId,
      false /* aExpectFailure */);
  EXPECT_EQ(rv, NS_OK);

  auto rawRequests = rawRequestObserver->GetRequests();
  EXPECT_EQ(static_cast<size_t>(2), rawRequests.size());
  EXPECT_EQ(rawRequests[0].user_action_id(), rawRequests[1].user_action_id());
  EXPECT_EQ(rawRequests[0].user_action_id(), userActionId.get());

  MOZ_ALWAYS_SUCCEEDS(
      obsServ->RemoveObserver(rawRequestObserver, "dlp-request-sent-raw"));
}

TEST_F(ContentAnalysisTest, CheckGivenUserActionIdsMustMatch) {
  nsCString userActionId1 = GenerateUUID();
  nsCString userActionId2 = GenerateUUID();
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();

  nsString allow1(L"allowMe");
  RefPtr<nsIContentAnalysisRequest> request1 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow1),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr, nullptr,
      nsCString(userActionId1));

  // Use different text so the request doesn't match the cache
  nsString allow2(L"allowMeAgain");
  RefPtr<nsIContentAnalysisRequest> request2 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow2),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr, nullptr,
      nsCString(userActionId2));
  nsTArray<RefPtr<nsIContentAnalysisRequest>> requests{request1, request2};

  nsresult rv = SendRequestsCancelAndExpectResponse(
      mContentAnalysis, requests, CancelMechanism::eCancelByUserActionId,
      true /* aExpectFailure */);
  EXPECT_EQ(rv, NS_ERROR_INVALID_ARG);
}

TEST_F(ContentAnalysisTest, CheckBrowserReportsTimeout) {
  // Submit a request to the agent and then timeout before we get a response.
  // When we do get a response later, check that we acknowledge as TOO_LATE.
  // A negative timeout tells Firefox to timeout after 25ms.  The agent
  // always takes 100ms for requests in tests.  TODO: can we further reduce
  // these?
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetInt(kTimeoutPref, -1));
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow1(L"allowMe");
  nsCOMPtr<nsIContentAnalysisRequest> request1 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow1),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  auto rawAcknowledgementObserver = MakeRefPtr<RawAcknowledgementObserver>();
  MOZ_ALWAYS_SUCCEEDS(obsServ->AddObserver(
      rawAcknowledgementObserver, "dlp-acknowledgement-sent-raw", false));

  SendRequestAndExpectResponse(
      mContentAnalysis, request1, Some(false) /* expectedShouldAllow */,
      Some(nsIContentAnalysisResponse::Action::eCanceled),
      Some(false) /* expectIsCached */);

  // The request returns before the ack is sent.  Give it some time to catch up.
  bool hitTimeout = false;
  RefPtr<CancelableRunnable> timer = NS_NewCancelableRunnableFunction(
      "SendRequestsCancelAndExpectResponse timeout",
      [&] { hitTimeout = true; });

#if defined(MOZ_ASAN)
  // This can be pretty slow on ASAN builds (bug 1895256)
  constexpr uint32_t kCATimeoutMs = 25000;
#else
  constexpr uint32_t kCATimeoutMs = 10000;
#endif
  NS_DelayedDispatchToCurrentThread(do_AddRef(timer), kCATimeoutMs);

  mozilla::SpinEventLoopUntil("Waiting for ContentAnalysis result"_ns, [&]() {
    auto acknowledgements = rawAcknowledgementObserver->GetAcknowledgements();
    if (acknowledgements.empty()) {
      return hitTimeout;
    }
    EXPECT_EQ(static_cast<size_t>(1), acknowledgements.size());
    EXPECT_EQ(
        ::content_analysis::sdk::ContentAnalysisAcknowledgement_FinalAction::
            ContentAnalysisAcknowledgement_FinalAction_BLOCK,
        acknowledgements[0].final_action());
    EXPECT_EQ(::content_analysis::sdk::ContentAnalysisAcknowledgement_Status::
                  ContentAnalysisAcknowledgement_Status_TOO_LATE,
              acknowledgements[0].status());
    return true;
  });

  timer->Cancel();
  EXPECT_FALSE(hitTimeout);

  MOZ_ALWAYS_SUCCEEDS(obsServ->RemoveObserver(rawAcknowledgementObserver,
                                              "dlp-acknowledgement-sent-raw"));
  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kTimeoutPref));
}

TEST_F(ContentAnalysisTest, CheckBrowserReportsTimeoutWithDefaultTimeoutAllow) {
  // Submit a request to the agent and then timeout before we get a response.
  // When we do get a response later, check that we respect the timeout_result
  // pref.
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetInt(kTimeoutPref, -1));
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetInt(kTimeoutResultPref, 2));
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow1(L"allowMe");
  nsCOMPtr<nsIContentAnalysisRequest> request1 = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow1),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);

  SendRequestAndExpectResponse(mContentAnalysis, request1,
                               Some(true) /* expectedShouldAllow */,
                               Some(nsIContentAnalysisResponse::Action::eAllow),
                               Some(false) /* expectIsCached */);

  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kTimeoutPref));
  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kTimeoutResultPref));
}

TEST_F(ContentAnalysisTest, GetDiagnosticInfo_Initial) {
  RefPtr<ContentAnalysisDiagnosticInfo> info =
      GetDiagnosticInfo(mContentAnalysis);
  EXPECT_TRUE(info->GetConnectedToAgent());
  EXPECT_FALSE(info->GetFailedSignatureVerification());
  nsString agentPath;
  MOZ_ALWAYS_SUCCEEDS(info->GetAgentPath(agentPath));
  int32_t index = agentPath.Find(u"content_analysis_sdk_agent.exe");
  EXPECT_EQ(agentPath.Length() - (sizeof("content_analysis_sdk_agent.exe") - 1),
            static_cast<size_t>(index));
  EXPECT_GE(info->GetRequestCount(), 0);
}

TEST_F(ContentAnalysisTest,
       GetDiagnosticInfo_AfterAgentTerminateAndOneRequest) {
  mAgentInfo.TerminateProcess();

  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  SendRequestAndExpectNoAgentResponse(mContentAnalysis, request);

  RefPtr<ContentAnalysisDiagnosticInfo> info =
      GetDiagnosticInfo(mContentAnalysis);
  EXPECT_FALSE(info->GetConnectedToAgent());
  EXPECT_FALSE(info->GetFailedSignatureVerification());
  nsString agentPath;
  MOZ_ALWAYS_SUCCEEDS(info->GetAgentPath(agentPath));
  EXPECT_TRUE(agentPath.IsEmpty());
  EXPECT_GE(info->GetRequestCount(), 0);

  StartAgent();
}

TEST_F(ContentAnalysisTest, GetDiagnosticInfo_AfterAgentTerminateAndReconnect) {
  mAgentInfo.TerminateProcess();
  StartAgent();

  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  SendRequestAndExpectResponse(mContentAnalysis, request, Some(true),
                               Some(nsIContentAnalysisResponse::eAllow),
                               Nothing());

  RefPtr<ContentAnalysisDiagnosticInfo> info =
      GetDiagnosticInfo(mContentAnalysis);
  EXPECT_TRUE(info->GetConnectedToAgent());
  EXPECT_FALSE(info->GetFailedSignatureVerification());
  nsString agentPath;
  MOZ_ALWAYS_SUCCEEDS(info->GetAgentPath(agentPath));
  int32_t index = agentPath.Find(u"content_analysis_sdk_agent.exe");
  EXPECT_EQ(agentPath.Length() - (sizeof("content_analysis_sdk_agent.exe") - 1),
            static_cast<size_t>(index));
  EXPECT_GE(info->GetRequestCount(), 0);
}

TEST_F(ContentAnalysisTest, GetDiagnosticInfo_RequestCountIncreases) {
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  RefPtr<ContentAnalysisDiagnosticInfo> info =
      GetDiagnosticInfo(mContentAnalysis);
  int64_t firstRequestCount = info->GetRequestCount();
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  SendRequestAndExpectResponse(mContentAnalysis, request, Some(true),
                               Some(nsIContentAnalysisResponse::eAllow),
                               Nothing());

  info = GetDiagnosticInfo(mContentAnalysis);
  EXPECT_EQ(firstRequestCount + 1, info->GetRequestCount());
}

TEST_F(ContentAnalysisTest, GetDiagnosticInfo_FailedSignatureVerification) {
  MOZ_ALWAYS_SUCCEEDS(
      Preferences::SetCString(kClientSignaturePref, "anInvalidSignature"));
  mAgentInfo.TerminateProcess();
  StartAgent();
  nsCOMPtr<nsIURI> uri = GetExampleDotComURI();
  nsString allow(L"allow");
  nsCOMPtr<nsIContentAnalysisRequest> request = new ContentAnalysisRequest(
      nsIContentAnalysisRequest::AnalysisType::eBulkDataEntry,
      nsIContentAnalysisRequest::Reason::eClipboardPaste, std::move(allow),
      false, EmptyCString(), uri,
      nsIContentAnalysisRequest::OperationType::eClipboard, nullptr);
  SendRequestAndExpectNoAgentResponse(
      mContentAnalysis, request, false,
      nsIContentAnalysisResponse::CancelError::eInvalidAgentSignature);

  RefPtr<ContentAnalysisDiagnosticInfo> info =
      GetDiagnosticInfo(mContentAnalysis);
  EXPECT_FALSE(info->GetConnectedToAgent());
  EXPECT_TRUE(info->GetFailedSignatureVerification());

  MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(kClientSignaturePref));
  // Reset the agent so it's working for future tests
  mAgentInfo.TerminateProcess();
  StartAgent();
}
