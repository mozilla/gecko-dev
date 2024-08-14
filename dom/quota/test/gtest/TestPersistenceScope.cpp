/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/PersistenceScope.h"

namespace mozilla::dom::quota {

TEST(DOM_Quota_PersistenceScope, SanityChecks)
{
  // Sanity checks.

  {
    const auto persistenceScope(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT));
    ASSERT_TRUE(persistenceScope.IsValue());
    ASSERT_EQ(persistenceScope.GetValue(), PERSISTENCE_TYPE_PERSISTENT);
  }

  {
    const auto persistenceScope(PersistenceScope::CreateFromNull());
    ASSERT_TRUE(persistenceScope.IsNull());
  }
}

TEST(DOM_Quota_PersistenceScope, MatchesValue)
{
  // Test each persistence scope type against particular persistence types.

  {
    const auto persistenceScope(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT));

    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT)));
    ASSERT_FALSE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_TEMPORARY)));
    ASSERT_FALSE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_DEFAULT)));
    ASSERT_FALSE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PRIVATE)));
  }

  {
    const auto persistenceScope(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_TEMPORARY, PERSISTENCE_TYPE_DEFAULT));

    ASSERT_FALSE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT)));
    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_TEMPORARY)));
    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_DEFAULT)));
    ASSERT_FALSE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PRIVATE)));
  }

  {
    const auto persistenceScope(PersistenceScope::CreateFromNull());

    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT)));
    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_TEMPORARY)));
    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_DEFAULT)));
    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PRIVATE)));
  }
}

TEST(DOM_Quota_PersistenceScope, MatchesSet)
{
  // Test each persistence scope type against particular persistence types.

  {
    const auto persistenceScope(
        PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT));

    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromSet(PERSISTENCE_TYPE_PERSISTENT)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY,
        PERSISTENCE_TYPE_DEFAULT)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY,
        PERSISTENCE_TYPE_DEFAULT, PERSISTENCE_TYPE_PRIVATE)));
  }

  {
    const auto persistenceScope(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_TEMPORARY, PERSISTENCE_TYPE_DEFAULT));

    ASSERT_FALSE(persistenceScope.Matches(
        PersistenceScope::CreateFromSet(PERSISTENCE_TYPE_PERSISTENT)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY,
        PERSISTENCE_TYPE_DEFAULT)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY,
        PERSISTENCE_TYPE_DEFAULT, PERSISTENCE_TYPE_PRIVATE)));
  }

  {
    const auto persistenceScope(PersistenceScope::CreateFromNull());

    ASSERT_TRUE(persistenceScope.Matches(
        PersistenceScope::CreateFromSet(PERSISTENCE_TYPE_PERSISTENT)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY,
        PERSISTENCE_TYPE_DEFAULT)));
    ASSERT_TRUE(persistenceScope.Matches(PersistenceScope::CreateFromSet(
        PERSISTENCE_TYPE_PERSISTENT, PERSISTENCE_TYPE_TEMPORARY,
        PERSISTENCE_TYPE_DEFAULT, PERSISTENCE_TYPE_PRIVATE)));
  }
}

}  //  namespace mozilla::dom::quota
