/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginParser.h"

#include "gtest/gtest.h"
#include "mozilla/OriginAttributes.h"
#include "nsFmtString.h"

namespace mozilla::dom::quota {

TEST(DOM_Quota_OriginParser_IsUUIDOrigin, Valid)
{
  EXPECT_TRUE(IsUUIDOrigin("uuid://1ef9867c-e754-4303-a18b-684f0321f6e2"_ns));
}

TEST(DOM_Quota_OriginParser_IsUUIDOrigin, Invalid)
{
  EXPECT_FALSE(IsUUIDOrigin("Invalid UUID Origin"_ns));

  EXPECT_FALSE(IsUUIDOrigin("1ef9867c-e754-4303-a18b-684f0321f6e2"_ns));

  EXPECT_FALSE(IsUUIDOrigin("uuid://1ef9867c-e754-4303-a18b"_ns));

  EXPECT_FALSE(IsUUIDOrigin("uuid+++1ef9867c-e754-4303-a18b-684f0321f6e2"_ns));
}

TEST(DOM_Quota_OriginParser_IsUserContextSuffix, True)
{
  const uint32_t userContextId = 5;

  EXPECT_TRUE(IsUserContextSuffix(
      nsFmtCString(FMT_STRING("^userContextId={}"), userContextId),
      userContextId));
  EXPECT_TRUE(IsUserContextSuffix(
      nsFmtCString(FMT_STRING("^inBrowser=1&userContextId={}"), userContextId),
      userContextId));
}

TEST(DOM_Quota_OriginParser_IsUserContextSuffix, False)
{
  const uint32_t userContextId = 5;

  EXPECT_FALSE(IsUserContextSuffix(""_ns, userContextId));
  EXPECT_FALSE(IsUserContextSuffix("^inBrowser=1"_ns, userContextId));
  EXPECT_FALSE(IsUserContextSuffix("^userContextId=1"_ns, userContextId));
  EXPECT_FALSE(
      IsUserContextSuffix("^inBrowser=1&userContextId=1"_ns, userContextId));
}

TEST(DOM_Quota_OriginParser_IsUserContextPattern, True)
{
  const uint32_t userContextId = 5;

  {
    OriginAttributesPattern pattern;
    pattern.Init(
        nsFmtString(FMT_STRING(u"{{ \"userContextId\": {} }}"), userContextId));
    EXPECT_TRUE(IsUserContextPattern(pattern, userContextId));
  }

  {
    OriginAttributesPattern pattern;
    pattern.Init(nsFmtString(
        FMT_STRING(u"{{ \"userContextId\": 5, \"privateBrowsingId\": 1 }}"),
        userContextId));
    EXPECT_TRUE(IsUserContextPattern(pattern, userContextId));
  }
}

TEST(DOM_Quota_OriginParser_IsUserContextPattern, False)
{
  const uint32_t userContextId = 5;

  {
    OriginAttributesPattern pattern;
    pattern.Init(u"{ \"inBrowser\": 1 }"_ns);
    EXPECT_FALSE(IsUserContextPattern(pattern, userContextId));
  }

  {
    OriginAttributesPattern pattern;
    pattern.Init(u"{ \"userContextId\": 1 }"_ns);
    EXPECT_FALSE(IsUserContextPattern(pattern, userContextId));
  }
}

}  //  namespace mozilla::dom::quota
