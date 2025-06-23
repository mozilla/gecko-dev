/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "TestContentAnalysisUtils.h"
#include <combaseapi.h>
#include <pathcch.h>
#include <shlwapi.h>
#include <rpc.h>
#include <windows.h>

using namespace mozilla;

MozAgentInfo LaunchAgentNormal(const wchar_t* aToBlock,
                               const wchar_t* aToWarn /* = L"warn" */) {
  nsString pipeName;
  GeneratePipeName(L"contentanalysissdk-gtest-", pipeName);
  return LaunchAgentNormal(aToBlock, aToWarn, pipeName);
}

MozAgentInfo LaunchAgentNormal(const wchar_t* aToBlock, const wchar_t* aToWarn,
                               const nsString& pipeName) {
  nsString cmdLineArguments;
  if (aToBlock && aToBlock[0] != 0) {
    cmdLineArguments.Append(L" --toblock=");
    cmdLineArguments.Append(aToBlock);
  }
  cmdLineArguments.Append(L" --towarn=");
  cmdLineArguments.Append(aToWarn);
  cmdLineArguments.Append(L" --user");
  cmdLineArguments.Append(L" --path=");
  cmdLineArguments.Append(pipeName);
  cmdLineArguments.Append(L" --delaysMs=100");
  MozAgentInfo agentInfo;
  LaunchAgentWithCommandLineArguments(cmdLineArguments, pipeName, agentInfo);
  return agentInfo;
}

void GeneratePipeName(const wchar_t* prefix, nsString& pipeName) {
  pipeName = u""_ns;
  pipeName.Append(prefix);
  UUID uuid;
  ASSERT_EQ(RPC_S_OK, UuidCreate(&uuid));
  // 39 == length of a UUID string including braces and NUL.
  wchar_t guidBuf[39] = {};
  ASSERT_EQ(39, StringFromGUID2(uuid, guidBuf, 39));
  // omit opening and closing braces (and trailing null)
  pipeName.Append(&guidBuf[1], 36);
}

void LaunchAgentWithCommandLineArguments(const nsString& cmdLineArguments,
                                         const nsString& pipeName,
                                         MozAgentInfo& agentInfo) {
  wchar_t progName[MAX_PATH] = {};
  // content_analysis_sdk_agent.exe is either next to firefox.exe (for local
  // builds), or in ../../tests/bin/ (for try/treeherder builds)
  DWORD nameSize = ::GetModuleFileNameW(nullptr, progName, MAX_PATH);
  ASSERT_NE(DWORD{0}, nameSize);
  ASSERT_EQ(S_OK, PathCchRemoveFileSpec(progName, nameSize));
  wchar_t normalizedPath[MAX_PATH] = {};
  nsString test1 = nsString(progName) + u"\\content_analysis_sdk_agent.exe"_ns;
  ASSERT_EQ(S_OK, PathCchCanonicalize(normalizedPath, MAX_PATH, test1.get()));
  nsString agentPath;
  if (::PathFileExistsW(normalizedPath)) {
    agentPath = nsString(normalizedPath);
  }
  if (agentPath.IsEmpty()) {
    nsString unNormalizedPath =
        nsString(progName) +
        u"\\..\\..\\tests\\bin\\content_analysis_sdk_agent.exe"_ns;
    ASSERT_EQ(S_OK, PathCchCanonicalize(normalizedPath, MAX_PATH,
                                        unNormalizedPath.get()));
    if (::PathFileExistsW(normalizedPath)) {
      agentPath = nsString(normalizedPath);
    }
  }
  ASSERT_FALSE(agentPath.IsEmpty());
  nsString localCmdLine = nsString(agentPath) + u" "_ns + cmdLineArguments;
  STARTUPINFOW startupInfo = {sizeof(startupInfo)};
  PROCESS_INFORMATION processInfo;
  BOOL ok =
      ::CreateProcessW(nullptr, localCmdLine.get(), nullptr, nullptr, FALSE, 0,
                       nullptr, nullptr, &startupInfo, &processInfo);
  // The documentation for CreateProcessW() says that any non-zero value is a
  // success
  if (!ok) {
    // Show the last error
    ASSERT_EQ(0UL, GetLastError())
        << "Failed to launch content_analysis_sdk_agent";
  }
  // Allow time for the agent to set up the pipe
  ::Sleep(2000);
  content_analysis::sdk::Client::Config config;
  config.name = NS_ConvertUTF16toUTF8(pipeName);
  config.user_specific = true;
  auto clientPtr = content_analysis::sdk::Client::Create(config);
  ASSERT_NE(nullptr, clientPtr.get());

  agentInfo.processInfo = processInfo;
  agentInfo.client = std::move(clientPtr);
}

void SendRequestAndExpectResponse(
    RefPtr<contentanalysis::ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    Maybe<bool> expectedShouldAllow,
    Maybe<nsIContentAnalysisResponse::Action> expectedAction,
    Maybe<bool> expectedIsCached) {
  SendRequestAndExpectResponseInternal(contentAnalysis, request,
                                       expectedShouldAllow, expectedAction,
                                       expectedIsCached, false);
}

void SendRequestAndExpectResponseInternal(
    RefPtr<contentanalysis::ContentAnalysis> contentAnalysis,
    const nsCOMPtr<nsIContentAnalysisRequest>& request,
    Maybe<bool> expectedShouldAllow,
    Maybe<nsIContentAnalysisResponse::Action> expectedAction,
    Maybe<bool> expectedIsCached, bool aIsEarlyResponse) {
  if (aIsEarlyResponse) {
    EXPECT_FALSE(expectedAction.isSome())
        << "Early responses do not have an action";
    EXPECT_FALSE(expectedIsCached.isSome())
        << "Early responses do not have an isCached value";
  }

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
  auto callback = MakeRefPtr<contentanalysis::ContentAnalysisCallback>(
      [&, timedOut](nsIContentAnalysisResult* result) {
        EXPECT_TRUE(NS_IsMainThread());
        if (timedOut->mValue) {
          return;
        }
        if (expectedShouldAllow.isSome()) {
          EXPECT_EQ(*expectedShouldAllow, result->GetShouldAllowContent());
        }
        if (aIsEarlyResponse) {
          // We will not get an acknowledgement for early responses,
          // so just set gotAcknowledgement to true so we don't wait for it.
          gotAcknowledgement = true;
        } else {
          nsCOMPtr<nsIContentAnalysisResponse> response =
              do_QueryInterface(result);
          EXPECT_TRUE(response);
          if (expectedAction.isSome()) {
            EXPECT_EQ(*expectedAction, response->GetAction());
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
          nsCString userActionId, originalUserActionId;
          MOZ_ALWAYS_SUCCEEDS(response->GetUserActionId(userActionId));
          MOZ_ALWAYS_SUCCEEDS(request->GetUserActionId(originalUserActionId));
          EXPECT_EQ(originalUserActionId, userActionId);
        }
        gotResponse = true;
      },
      [&gotResponse, &gotAcknowledgement, timedOut](nsresult error) {
        EXPECT_TRUE(NS_IsMainThread());
        if (timedOut->mValue) {
          return;
        }
        const char* errorName = mozilla::GetStaticErrorName(error);
        errorName = errorName ? errorName : "";
        printf("Got error response code %s(%x)\n", errorName, error);
        // Errors should not have errorCode NS_OK
        EXPECT_NE(NS_OK, error);
        gotResponse = true;
        // An acknowledgement won't be sent, so don't wait for one
        gotAcknowledgement = true;
        FAIL() << "Got error response";
      });

  auto rawAcknowledgementObserver = MakeRefPtr<RawAcknowledgementObserver>();
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  if (!aIsEarlyResponse) {
    MOZ_ALWAYS_SUCCEEDS(obsServ->AddObserver(
        rawAcknowledgementObserver, "dlp-acknowledgement-sent-raw", false));
  }

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
  if (!aIsEarlyResponse) {
    obsServ->RemoveObserver(rawAcknowledgementObserver,
                            "dlp-acknowledgement-sent-raw");
  }
}

NS_IMPL_ISUPPORTS(RawAcknowledgementObserver, nsIObserver);

NS_IMETHODIMP RawAcknowledgementObserver::Observe(nsISupports* aSubject,
                                                  const char* aTopic,
                                                  const char16_t* aData) {
  content_analysis::sdk::ContentAnalysisAcknowledgement acknowledgement;
  ParseFromWideModifiedString(&acknowledgement, aData);
  mAcknowledgements.push_back(std::move(acknowledgement));
  return NS_OK;
}
