/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_testcontentanalysis_h
#define mozilla_testcontentanalysis_h

#include <processthreadsapi.h>
#include <errhandlingapi.h>
#include <vector>

#include "ContentAnalysis.h"
#include "content_analysis/sdk/analysis_client.h"
#include "gtest/gtest.h"

#include "mozilla/media/MediaUtils.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsCOMArray.h"
#include "nsIContentAnalysis.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsThreadUtils.h"

constexpr const char* kAllowUrlPref =
    "browser.contentanalysis.allow_url_regex_list";
constexpr const char* kDenyUrlPref =
    "browser.contentanalysis.deny_url_regex_list";
constexpr const char* kPipePathNamePref =
    "browser.contentanalysis.pipe_path_name";
constexpr const char* kIsDLPEnabledPref = "browser.contentanalysis.enabled";
constexpr const char* kDefaultResultPref =
    "browser.contentanalysis.default_result";
constexpr const char* kTimeoutPref = "browser.contentanalysis.agent_timeout";
constexpr const char* kTimeoutResultPref =
    "browser.contentanalysis.timeout_result";
constexpr const char* kClientSignaturePref =
    "browser.contentanalysis.client_signature";
constexpr const char* kMaxConnections =
    "browser.contentanalysis.max_connections";

struct BoolStruct {
  bool mValue = false;
};

struct MozAgentInfo {
  PROCESS_INFORMATION processInfo;
  std::unique_ptr<content_analysis::sdk::Client> client;
  void TerminateProcess() {
    DWORD exitCode = 0;
    BOOL result = ::GetExitCodeProcess(processInfo.hProcess, &exitCode);
    EXPECT_NE(static_cast<BOOL>(0), result);
    EXPECT_EQ(STILL_ACTIVE, exitCode);

    BOOL terminateResult = ::TerminateProcess(processInfo.hProcess, 0);
    ASSERT_NE(FALSE, terminateResult)
        << "Failed to terminate content_analysis_sdk_agent process";
  }
};

void GeneratePipeName(const wchar_t* prefix, nsString& pipeName);
void LaunchAgentWithCommandLineArguments(const nsString& cmdLineArguments,
                                         const nsString& pipeName,
                                         MozAgentInfo& agentInfo);
MozAgentInfo LaunchAgentNormal(const wchar_t* aToBlock,
                               const wchar_t* aToWarn = L"warn");
MozAgentInfo LaunchAgentNormal(const wchar_t* aToBlock, const wchar_t* aToWarn,
                               const nsString& pipeName);

inline nsCString GenerateUUID() {
  nsID id = nsID::GenerateUUID();
  return nsCString(id.ToString().get());
}

inline RefPtr<mozilla::CancelableRunnable> QueueTimeoutToMainThread(
    RefPtr<mozilla::media::Refcountable<BoolStruct>> aTimedOut) {
  RefPtr<mozilla::CancelableRunnable> timer = NS_NewCancelableRunnableFunction(
      "timeout", [&] { aTimedOut->mValue = true; });
#if defined(MOZ_ASAN)
  // This can be pretty slow on ASAN builds (bug 1895256)
  constexpr uint32_t kCATimeout = 25000;
#else
  constexpr uint32_t kCATimeout = 10000;
#endif
  EXPECT_EQ(NS_OK,
            NS_DelayedDispatchToCurrentThread(do_AddRef(timer), kCATimeout));
  return timer;
}

inline nsCOMPtr<nsIURI> GetExampleDotComURI() {
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), "https://example.com"));
  return uri;
}

#endif
