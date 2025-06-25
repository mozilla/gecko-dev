/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/Date.h"

namespace mozilla::dom::quota::test {

TEST(DOM_Quota_Date, FromDays)
{
  const Date date = Date::FromDays(0);
  ASSERT_EQ(date.ToDays(), 0);
}

TEST(DOM_Quota_Date, FromTimestamp)
{
  const Date date = Date::FromTimestamp(PR_Now());
  ASSERT_GT(date.ToDays(), 0);
}

TEST(DOM_Quota_Date, Today)
{
  const Date date = Date::Today();
  ASSERT_GT(date.ToDays(), 0);
}

TEST(DOM_Quota_Date, ComparisonOperators)
{
  const Date dateA = Date::FromDays(100);
  const Date dateB = Date::FromDays(200);
  const Date dateC = Date::FromDays(200);
  const Date dateD = Date::FromDays(300);

  // ==
  ASSERT_EQ(dateB, dateC);

  // !=
  ASSERT_NE(dateA, dateB);

  // <
  ASSERT_LT(dateA, dateB);
  ASSERT_LT(dateB, dateD);

  // <=
  ASSERT_LE(dateA, dateB);
  ASSERT_LE(dateB, dateC);

  // >
  ASSERT_GT(dateD, dateB);
  ASSERT_GT(dateB, dateA);

  // >=
  ASSERT_GE(dateD, dateB);
  ASSERT_GE(dateB, dateC);
}

}  // namespace mozilla::dom::quota::test
