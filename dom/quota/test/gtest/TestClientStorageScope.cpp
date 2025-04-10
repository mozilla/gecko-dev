/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/ClientStorageScope.h"

namespace mozilla::dom::quota {

TEST(DOM_Quota_ClientStorageScope, SanityChecks)
{
  // Sanity checks.

  {
    const auto clientStorageScope(
        ClientStorageScope::CreateFromClient(Client::IDB));
    ASSERT_TRUE(clientStorageScope.IsClient());
    ASSERT_EQ(clientStorageScope.GetClientType(), Client::IDB);
  }

  {
    const auto clientStorageScope(ClientStorageScope::CreateFromMetadata());
    ASSERT_TRUE(clientStorageScope.IsMetadata());
  }

  {
    const auto clientStorageScope(ClientStorageScope::CreateFromNull());
    ASSERT_TRUE(clientStorageScope.IsNull());
  }
}

TEST(DOM_Quota_ClientStorageScope, MatchesClient)
{
  // Test each client storage scope type against particular client types.

  {
    const auto clientStorageScope(
        ClientStorageScope::CreateFromClient(Client::IDB));

    ASSERT_TRUE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::IDB)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::DOMCACHE)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::SDB)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::FILESYSTEM)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::LS)));
  }

  {
    const auto clientStorageScope(ClientStorageScope::CreateFromMetadata());

    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::IDB)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::DOMCACHE)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::SDB)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::FILESYSTEM)));
    ASSERT_FALSE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::LS)));
  }

  {
    const auto clientStorageScope(ClientStorageScope::CreateFromNull());

    ASSERT_TRUE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::IDB)));
    ASSERT_TRUE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::DOMCACHE)));
    ASSERT_TRUE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::SDB)));
    ASSERT_TRUE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::FILESYSTEM)));
    ASSERT_TRUE(clientStorageScope.Matches(
        ClientStorageScope::CreateFromClient(Client::LS)));
  }
}

TEST(DOM_Quota_ClientStorageScope, MatchesMetadata)
{
  // Test each client storage scope type against particular client types.

  {
    const auto clientStorageScope(
        ClientStorageScope::CreateFromClient(Client::IDB));

    ASSERT_FALSE(
        clientStorageScope.Matches(ClientStorageScope::CreateFromMetadata()));
  }

  {
    const auto clientStorageScope(ClientStorageScope::CreateFromMetadata());

    ASSERT_TRUE(
        clientStorageScope.Matches(ClientStorageScope::CreateFromMetadata()));
  }

  {
    const auto clientStorageScope(ClientStorageScope::CreateFromNull());

    ASSERT_TRUE(
        clientStorageScope.Matches(ClientStorageScope::CreateFromMetadata()));
  }
}

}  //  namespace mozilla::dom::quota
