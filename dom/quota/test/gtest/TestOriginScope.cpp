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

namespace {

PrincipalMetadata GetPrincipalMetadata(const nsCString& aGroup,
                                       const nsCString& aOriginNoSuffix) {
  return PrincipalMetadata{""_ns, aGroup, aOriginNoSuffix, aOriginNoSuffix,
                           /* aIsPrivate */ false};
}

PrincipalMetadata GetPrincipalMetadata(const nsCString& aSuffix,
                                       const nsCString& aGroupNoSuffix,
                                       const nsCString& aOriginNoSuffix) {
  nsCString group = aGroupNoSuffix + aSuffix;
  nsCString origin = aOriginNoSuffix + aSuffix;

  return PrincipalMetadata{aSuffix, group, origin, origin,
                           /* aIsPrivate */ false};
}

}  // namespace

TEST(DOM_Quota_OriginScope, SanityChecks)
{
  OriginScope originScope;

  // Sanity checks.

  {
    constexpr auto group = "www.mozilla.org"_ns;
    constexpr auto origin = "http://www.mozilla.org"_ns;
    originScope.SetFromOrigin(GetPrincipalMetadata(group, origin));
    EXPECT_TRUE(originScope.IsOrigin());
    EXPECT_TRUE(originScope.GetOrigin().Equals(origin));
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(origin));
  }

  {
    constexpr auto group = "mozilla.org"_ns;
    constexpr auto origin = "http://www.mozilla.org"_ns;
    originScope.SetFromPrefix(GetPrincipalMetadata(group, origin));
    EXPECT_TRUE(originScope.IsPrefix());
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(origin));
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
    const auto originScope(OriginScope::FromOrigin(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns)));

    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns))));
    EXPECT_FALSE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("example.org"_ns, "http://www.example.org"_ns))));
  }

  {
    const auto originScope(OriginScope::FromPrefix(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns)));

    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns))));
    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("^userContextId=1"_ns, "mozilla.org"_ns,
                             "http://www.mozilla.org"_ns))));
    EXPECT_FALSE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("^userContextId=1"_ns, "example.org"_ns,
                             "http://www.example.org"_ns))));
  }

  {
    const auto originScope(
        OriginScope::FromJSONPattern(u"{ \"userContextId\": 1 }"_ns));

    EXPECT_FALSE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns))));
    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("^userContextId=1"_ns, "mozilla.org"_ns,
                             "http://www.mozilla.org"_ns))));
    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("^userContextId=1"_ns, "example.org"_ns,
                             "http://www.example.org"_ns))));
  }

  {
    const auto originScope(OriginScope::FromNull());

    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns))));
    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("^userContextId=1"_ns, "mozilla.org"_ns,
                             "http://www.mozilla.org"_ns))));
    EXPECT_TRUE(originScope.Matches(OriginScope::FromOrigin(
        GetPrincipalMetadata("^userContextId=1"_ns, "example.org"_ns,
                             "http://www.example.org"_ns))));
  }
}

TEST(DOM_Quota_OriginScope, MatchesGroup)
{
  {
    const auto originScope(OriginScope::FromOrigin(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns)));

    ASSERT_TRUE(originScope.Matches(OriginScope::FromGroup("mozilla.org"_ns)));
    ASSERT_FALSE(originScope.Matches(
        OriginScope::FromGroup("mozilla.org^userContextId=1"_ns)));
    ASSERT_FALSE(originScope.Matches(OriginScope::FromGroup("mozilla.com"_ns)));
  }

  {
    const auto originScope(OriginScope::FromOrigin(GetPrincipalMetadata(
        "^userContextId=1"_ns, "mozilla.org"_ns, "http://www.mozilla.org"_ns)));

    ASSERT_FALSE(originScope.Matches(OriginScope::FromGroup("mozilla.org"_ns)));
    ASSERT_TRUE(originScope.Matches(
        OriginScope::FromGroup("mozilla.org^userContextId=1"_ns)));
    ASSERT_FALSE(originScope.Matches(OriginScope::FromGroup("mozilla.com"_ns)));
  }

  {
    const auto originScope(OriginScope::FromPrefix(
        GetPrincipalMetadata("mozilla.org"_ns, "http://www.mozilla.org"_ns)));

    ASSERT_TRUE(originScope.Matches(OriginScope::FromGroup("mozilla.org"_ns)));
    ASSERT_TRUE(originScope.Matches(
        OriginScope::FromGroup("mozilla.org^userContextId=1"_ns)));
    ASSERT_FALSE(originScope.Matches(OriginScope::FromGroup("mozilla.com"_ns)));
  }

  {
    const auto originScope(
        OriginScope::FromJSONPattern(u"{ \"userContextId\": 1 }"_ns));

    ASSERT_FALSE(originScope.Matches(OriginScope::FromGroup("mozilla.org"_ns)));
    ASSERT_TRUE(originScope.Matches(
        OriginScope::FromGroup("mozilla.org^userContextId=1"_ns)));
    ASSERT_FALSE(originScope.Matches(OriginScope::FromGroup("mozilla.com"_ns)));
  }

  {
    const auto originScope(OriginScope::FromGroup("mozilla.org"_ns));

    ASSERT_TRUE(originScope.Matches(OriginScope::FromGroup("mozilla.org"_ns)));
    ASSERT_FALSE(originScope.Matches(
        OriginScope::FromGroup("mozilla.org^userContextId=1"_ns)));
    ASSERT_FALSE(originScope.Matches(OriginScope::FromGroup("mozilla.com"_ns)));
  }

  {
    const auto originScope(OriginScope::FromNull());

    ASSERT_TRUE(originScope.Matches(OriginScope::FromGroup("mozilla.org"_ns)));
    ASSERT_TRUE(originScope.Matches(
        OriginScope::FromGroup("mozilla.org^userContextId=1"_ns)));
    ASSERT_TRUE(originScope.Matches(OriginScope::FromGroup("mozilla.com"_ns)));
  }
}

}  //  namespace mozilla::dom::quota
