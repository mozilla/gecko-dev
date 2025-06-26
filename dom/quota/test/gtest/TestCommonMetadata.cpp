/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsString.h"
#include "QuotaManagerTestHelpers.h"

namespace mozilla::dom::quota::test {

TEST(DOM_Quota_CommonMetadata, PrincipalMetadata_Equals)
{
  // Base object to compare against.

  PrincipalMetadata principalMetadata1 = GetPrincipalMetadata(
      ""_ns, "example.org"_ns, "http://www.example.org"_ns);

  {
    // All fields are the same

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    EXPECT_TRUE(principalMetadata1.Equals(principalMetadata2));
  }

  {
    // Different suffix.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        "^userContextId=42"_ns, "example.org"_ns, "http://www.example.org"_ns);

    EXPECT_FALSE(principalMetadata1.Equals(principalMetadata2));
  }

  {
    // Different group.

    PrincipalMetadata principalMetadata2 =
        GetPrincipalMetadata(""_ns, "org"_ns, "http://www.example.org"_ns);

    EXPECT_FALSE(principalMetadata1.Equals(principalMetadata2));
  }

  {
    // Different origin.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.sub.example.org"_ns);

    EXPECT_FALSE(principalMetadata1.Equals(principalMetadata2));
  }

  {
    // Different isPrivate flag.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns,
        /* aIsPrivate */ true);

    EXPECT_FALSE(principalMetadata1.Equals(principalMetadata2));
  }
}

TEST(DOM_Quota_CommonMetadata, OriginMetadata_Equals)
{
  // Base object to compare against.

  PrincipalMetadata principalMetadata1 = GetPrincipalMetadata(
      ""_ns, "example.org"_ns, "http://www.example.org"_ns);

  OriginMetadata originMetadata1(principalMetadata1, PERSISTENCE_TYPE_DEFAULT);

  {
    // All fields are the same.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    EXPECT_TRUE(originMetadata1.Equals(originMetadata2));
  }

  {
    // Different PrincipalMetadata (isPrivate differs).

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns,
        /* aIsPrivate */ true);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    EXPECT_FALSE(originMetadata1.Equals(originMetadata2));
  }

  {
    // Different persistence type.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_TEMPORARY);

    EXPECT_FALSE(originMetadata1.Equals(originMetadata2));
  }
}

TEST(DOM_Quota_CommonMetadata, OriginStateMetadata_Equals)
{
  // Base object to compare against.

  OriginStateMetadata originStateMetadata1 = OriginStateMetadata(
      /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
      /* aAccessed */ false, /* aPersisted */ false);

  {
    // All fields are the same.

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    EXPECT_TRUE(originStateMetadata1.Equals(originStateMetadata2));
  }

  {
    // Different last access time.

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 1, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    EXPECT_FALSE(originStateMetadata1.Equals(originStateMetadata2));
  }

  {
    // Different last maintenanace date.

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 1,
        /* aAccessed */ false, /* aPersisted */ false);

    EXPECT_FALSE(originStateMetadata1.Equals(originStateMetadata2));
  }

  {
    // Different accessed flag.

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ true, /* aPersisted */ false);

    EXPECT_FALSE(originStateMetadata1.Equals(originStateMetadata2));
  }

  {
    // Different persisted flag.

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ true);

    EXPECT_FALSE(originStateMetadata1.Equals(originStateMetadata2));
  }
}

// Tests that OriginMetadata::GetCompositeKey returns the expected
// "<persistence>*<origin>" string.
TEST(DOM_Quota_CommonMetadata, OriginMetadata_GetCompositeKey)
{
  auto originMetadata =
      GetOriginMetadata(""_ns, "mozilla.org"_ns, "http://www.mozilla.org"_ns);

  auto compositeKey = originMetadata.GetCompositeKey();

  EXPECT_STREQ(compositeKey.get(), "2*http://www.mozilla.org");
}

TEST(DOM_Quota_CommonMetadata, FullOriginMetadata_Equals)
{
  // Base object to compare against.
  PrincipalMetadata principalMetadata1 = GetPrincipalMetadata(
      ""_ns, "example.org"_ns, "http://www.example.org"_ns);

  OriginMetadata originMetadata1(principalMetadata1, PERSISTENCE_TYPE_DEFAULT);

  OriginStateMetadata originStateMetadata1 = OriginStateMetadata(
      /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
      /* aAccessed */ false, /* aPersisted */ false);

  FullOriginMetadata fullOriginMetadata1(originMetadata1, originStateMetadata1,
                                         ClientUsageArray(), /* aUsage */ 0,
                                         kCurrentQuotaVersion);

  {
    // All fields are the same.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_TRUE(fullOriginMetadata1.Equals(fullOriginMetadata2));
  }

  {
    // Different OriginMetadata (PrincipalMetadata differs).

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns,
        /* aIsPrivate */ true);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_FALSE(fullOriginMetadata1.Equals(fullOriginMetadata2));
  }

  {
    // Different OriginMetadata (persistence type differs).

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_TEMPORARY);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_FALSE(fullOriginMetadata1.Equals(fullOriginMetadata2));
  }

  {
    // Different OriginStateMetadata (last access time differs).

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 1, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_FALSE(fullOriginMetadata1.Equals(fullOriginMetadata2));
  }

  {
    // Different client usage array.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime*/ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2,
        ClientUsageArray{{Some(1), Nothing(), Nothing(), Nothing(), Nothing()}},
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_FALSE(fullOriginMetadata1.Equals(fullOriginMetadata2));
  }

  {
    // Different origin usage.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime*/ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 1, kCurrentQuotaVersion);

    EXPECT_FALSE(fullOriginMetadata1.Equals(fullOriginMetadata2));
  }

  {
    // Different quota version.

    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime*/ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion + 1);

    EXPECT_FALSE(fullOriginMetadata1.Equals(fullOriginMetadata2));
  }
}

TEST(DOM_Quota_CommonMetadata, FullOriginMetadata_EqualsIgnoringOriginState)
{
  // Base object to compare against.
  PrincipalMetadata principalMetadata1 = GetPrincipalMetadata(
      ""_ns, "example.org"_ns, "http://www.example.org"_ns);

  OriginMetadata originMetadata1(principalMetadata1, PERSISTENCE_TYPE_DEFAULT);

  OriginStateMetadata originStateMetadata1 = OriginStateMetadata(
      /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
      /* aAccessed */ false, /* aPersisted */ false);

  FullOriginMetadata fullOriginMetadata1(originMetadata1, originStateMetadata1,
                                         ClientUsageArray(), /* aUsage */ 0,
                                         kCurrentQuotaVersion);

  {
    // All fields are the same.
    PrincipalMetadata principalMetadata2 = GetPrincipalMetadata(
        ""_ns, "example.org"_ns, "http://www.example.org"_ns);

    OriginMetadata originMetadata2(principalMetadata2,
                                   PERSISTENCE_TYPE_DEFAULT);

    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata2, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_TRUE(
        fullOriginMetadata1.EqualsIgnoringOriginState(fullOriginMetadata2));
  }

  {
    // Different last access time (ignored).
    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 1, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata1, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_TRUE(
        fullOriginMetadata1.EqualsIgnoringOriginState(fullOriginMetadata2));
  }

  {
    // Different last maintenance date (ignored).
    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 1,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata1, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_TRUE(
        fullOriginMetadata1.EqualsIgnoringOriginState(fullOriginMetadata2));
  }

  {
    // Different accessed flag (ignored).
    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ true, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata1, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_TRUE(
        fullOriginMetadata1.EqualsIgnoringOriginState(fullOriginMetadata2));
  }

  {
    // Different persisted flag (ignored).
    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ true);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata1, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 0, kCurrentQuotaVersion);

    EXPECT_TRUE(
        fullOriginMetadata1.EqualsIgnoringOriginState(fullOriginMetadata2));
  }

  {
    // Different origin usage (not ignored).
    OriginStateMetadata originStateMetadata2 = OriginStateMetadata(
        /* aLastAccessTime */ 0, /* aLastMaintenanceDate */ 0,
        /* aAccessed */ false, /* aPersisted */ false);

    FullOriginMetadata fullOriginMetadata2(
        originMetadata1, originStateMetadata2, ClientUsageArray(),
        /* aUsage */ 1, kCurrentQuotaVersion);

    EXPECT_FALSE(
        fullOriginMetadata1.EqualsIgnoringOriginState(fullOriginMetadata2));
  }
}

TEST(DOM_Quota_CommonMetadata, FullOriginMetadata_Clone)
{
  auto fullOriginMetadata1 = GetFullOriginMetadata(""_ns, "mozilla.org"_ns,
                                                   "http://www.mozilla.org"_ns);

  auto fullOriginMetadata2 = fullOriginMetadata1.Clone();

  EXPECT_TRUE(fullOriginMetadata1.Equals(fullOriginMetadata2));
}

}  //  namespace mozilla::dom::quota::test
