/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "nsStringFwd.h"
#include "nsTDependentString.h"
#include "nsTLiteralString.h"

namespace mozilla::dom::quota {

TEST(DOM_Quota_OriginScope, SanityChecks)
{
  OriginScope originScope;

  // Sanity checks.

  {
    constexpr auto origin = "http://www.mozilla.org"_ns;
    originScope.SetFromOrigin(origin);
    EXPECT_TRUE(originScope.IsOrigin());
    EXPECT_TRUE(originScope.GetOrigin().Equals(origin));
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(origin));
  }

  {
    constexpr auto prefix = "http://www.mozilla.org"_ns;
    originScope.SetFromPrefix(prefix);
    EXPECT_TRUE(originScope.IsPrefix());
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(prefix));
  }

  {
    originScope.SetFromNull();
    EXPECT_TRUE(originScope.IsNull());
  }
}

TEST(DOM_Quota_OriginScope, MatchesOrigin)
{
  // Test each origin scope type against particular origins.

  {
    const auto originScope(
        OriginScope::FromOrigin("http://www.mozilla.org"_ns));

    EXPECT_TRUE(originScope.Matches(
        OriginScope::FromOrigin("http://www.mozilla.org"_ns)));
    EXPECT_FALSE(originScope.Matches(
        OriginScope::FromOrigin("http://www.example.org"_ns)));
  }

  {
    const auto originScope(
        OriginScope::FromPrefix("http://www.mozilla.org"_ns));

    EXPECT_TRUE(originScope.Matches(
        OriginScope::FromOrigin("http://www.mozilla.org"_ns)));
    EXPECT_TRUE(originScope.Matches(
        OriginScope::FromOrigin("http://www.mozilla.org^userContextId=1"_ns)));
    EXPECT_FALSE(originScope.Matches(
        OriginScope::FromOrigin("http://www.example.org^userContextId=1"_ns)));
  }

  {
    const auto originScope(OriginScope::FromNull());

    EXPECT_TRUE(originScope.Matches(
        OriginScope::FromOrigin("http://www.mozilla.org"_ns)));
    EXPECT_TRUE(originScope.Matches(
        OriginScope::FromOrigin("http://www.mozilla.org^userContextId=1"_ns)));
    EXPECT_TRUE(originScope.Matches(
        OriginScope::FromOrigin("http://www.example.org^userContextId=1"_ns)));
  }
}

}  //  namespace mozilla::dom::quota
