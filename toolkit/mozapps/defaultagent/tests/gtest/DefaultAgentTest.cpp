/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include <windows.h>
#include "common.h"
#include "nsCOMPtr.h"
#include "Registry.h"
#include "DefaultAgent.h"
#include "Telemetry.h"
#include "nsComponentManagerUtils.h"
#include "nsLiteralString.h"

using namespace mozilla::default_agent;

const wchar_t* REG_KEY = L"AppLastRunTime";
const char* DEFAULT_AGENT_CONTRACT_ID = "@mozilla.org/default-agent;1";

class DefaultAgentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_lastRunTimestamp = 0;
    m_lastRunRegistryEntryExisted = false;
    m_lastRunRegisterEntryWasModified = false;
    // Save whatever the registry setting was...
    MaybeQwordResult lastRunTimeResult =
        RegistryGetValueQword(IsPrefixed::Prefixed, REG_KEY);
    if (!lastRunTimeResult.isErr()) {
      mozilla::Maybe<ULONGLONG> lastRunTimeMaybe = lastRunTimeResult.unwrap();
      if (lastRunTimeMaybe.isSome()) {
        m_lastRunRegistryEntryExisted = true;
        m_lastRunTimestamp = lastRunTimeMaybe.value();
      }
    }
  }

  void TearDown() override {
    if (m_lastRunRegisterEntryWasModified) {
      if (m_lastRunRegistryEntryExisted) {
        // Restore registry value
        mozilla::WindowsErrorResult<mozilla::Ok> writeResult =
            RegistrySetValueQword(IsPrefixed::Prefixed, REG_KEY,
                                  m_lastRunTimestamp);
        ASSERT_FALSE(writeResult.isErr());

      } else {
        mozilla::WindowsErrorResult<mozilla::Ok> deleteResult =
            RegistryDeleteValue(IsPrefixed::Prefixed, REG_KEY);
        ASSERT_FALSE(deleteResult.isErr());
      }
    }
  }

  ULONGLONG m_lastRunTimestamp;
  bool m_lastRunRegistryEntryExisted;
  bool m_lastRunRegisterEntryWasModified;
};

TEST_F(DefaultAgentTest, SecondsSinceLastRun) {
  // Now let's overwrite the value
  ULONGLONG now = GetCurrentTimestamp();
  mozilla::WindowsErrorResult<mozilla::Ok> writeResult =
      RegistrySetValueQword(IsPrefixed::Prefixed, REG_KEY, now);
  ASSERT_FALSE(writeResult.isErr());
  m_lastRunRegisterEntryWasModified = true;
  int64_t secondsSinceLastRun;
  nsresult result = getSecondsSinceLastAppRun(&secondsSinceLastRun);
  ASSERT_EQ(result, NS_OK);
  ASSERT_GE(secondsSinceLastRun, 0);  // should be a positive number
  ASSERT_LE(secondsSinceLastRun, 5);  // should be close to zero
}

TEST_F(DefaultAgentTest, AgentSecondsSinceLastRun) {
  int64_t secondsSinceLastRun;
  ULONGLONG now = GetCurrentTimestamp();
  mozilla::WindowsErrorResult<mozilla::Ok> writeResult =
      RegistrySetValueQword(IsPrefixed::Prefixed, REG_KEY, now);
  ASSERT_FALSE(writeResult.isErr());
  m_lastRunRegisterEntryWasModified = true;

  nsCOMPtr<nsIDefaultAgent> defaultAgent =
      do_CreateInstance(DEFAULT_AGENT_CONTRACT_ID);

  nsresult result = defaultAgent->SecondsSinceLastAppRun(&secondsSinceLastRun);
  ASSERT_EQ(result, NS_OK);
  ASSERT_GE(secondsSinceLastRun, 0);  // should be a positive number
  ASSERT_LE(secondsSinceLastRun, 5);  // should be close to zero
}

TEST_F(DefaultAgentTest, SendDefaultAgentPing) {
  DefaultBrowserInfo browserInfo = {Browser::InternetExplorer, Browser::Opera};
  DefaultPdfInfo pdfInfo = {PDFHandler::AdobeAcrobat};
  NotificationActivities activities = {NotificationType::Initial,
                                       NotificationShown::NotShown,
                                       NotificationAction::NoAction};
  uint32_t daysSinceAppLaunch = 12;
  HRESULT result = SendDefaultAgentPing(browserInfo, pdfInfo, activities,
                                        daysSinceAppLaunch);
  ASSERT_EQ(S_OK, result);
}

TEST_F(DefaultAgentTest, SendPing) {
  nsCOMPtr<nsIDefaultAgent> defaultAgent =
      do_CreateInstance(DEFAULT_AGENT_CONTRACT_ID);

  auto currentBrowser = u"ie"_ns;
  auto previousBrowser = u"opera"_ns;
  auto pdfHandler = u"Adobe Acrobat"_ns;
  auto notificationShown = u"not-shown"_ns;
  auto notificationAction = u"no-action"_ns;
  uint32_t daysSinceLaunch = 12;

  nsresult result = defaultAgent->SendPing(currentBrowser, previousBrowser,
                                           pdfHandler, notificationShown,
                                           notificationAction, daysSinceLaunch);
  ASSERT_EQ(result, NS_OK);
}
