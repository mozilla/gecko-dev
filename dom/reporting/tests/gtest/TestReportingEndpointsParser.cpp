/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/dom/ReportingHeader.h"
#include "nsNetUtil.h"
#include "nsIURI.h"

using namespace mozilla;
using namespace mozilla::dom;

TEST(ReportingEndpointsParser, Basic)
{
  nsCOMPtr<nsIURI> uri1;
  nsCOMPtr<nsIURI> uri2;

  nsresult rv =
      NS_NewURI(getter_AddRefs(uri1), "https://example.com/csp-reports");
  ASSERT_EQ(NS_OK, rv);
  rv = NS_NewURI(getter_AddRefs(uri2), "https://example.com/hpkp-reports");
  ASSERT_EQ(NS_OK, rv);

  bool urlEqual = false;

  // Empty header
  UniquePtr<ReportingHeader::Client> client =
      ReportingHeader::ParseReportingEndpointsHeader(""_ns, uri1);
  ASSERT_TRUE(!client);

  // Empty header
  client = ReportingHeader::ParseReportingEndpointsHeader("     "_ns, uri1);
  ASSERT_TRUE(!client);

  // Single client
  client = ReportingHeader::ParseReportingEndpointsHeader(
      "csp-endpoint=\"https://example.com/csp-reports\""_ns, uri1);
  ASSERT_TRUE(client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("csp-endpoint"));
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri1, &urlEqual)) &&
      urlEqual);

  // 2 clients, different group names
  client = ReportingHeader::ParseReportingEndpointsHeader(
      "csp-endpoint=\"https://example.com/csp-reports\",\thpkp-endpoint=\"https://example.com/hpkp-reports\""_ns,
      uri1);
  ASSERT_TRUE(client);
  ASSERT_EQ((uint32_t)2, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("csp-endpoint"));
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri1, &urlEqual)) &&
      urlEqual);
  ASSERT_TRUE(
      client->mGroups.ElementAt(1).mName.EqualsLiteral("hpkp-endpoint"));
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(1).mEndpoints.ElementAt(0).mUrl->Equals(
              uri2, &urlEqual)) &&
      urlEqual);

  // Single client, passed in as an inner list with parameters to ignore
  client = ReportingHeader::ParseReportingEndpointsHeader(
      "csp-endpoint=(\"https://example.com/csp-reports\" 5);valid"_ns, uri1);
  ASSERT_TRUE(client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("csp-endpoint"));
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri1, &urlEqual)) &&
      urlEqual);

  // Single client, key's value is an empty string
  client = ReportingHeader::ParseReportingEndpointsHeader(
      "csp-endpoint=\"   \""_ns, uri1);
  ASSERT_TRUE(client);

  // Single client, key's value is a non-URL string
  client = ReportingHeader::ParseReportingEndpointsHeader(
      "csp-endpoint=\"Not URL syntax\""_ns, uri1);
  ASSERT_TRUE(client);

  // Single client, key's value cannot be translated to a String SFVItem
  client =
      ReportingHeader::ParseReportingEndpointsHeader("csp-endpoint=1"_ns, uri1);
  ASSERT_TRUE(!client);
}
