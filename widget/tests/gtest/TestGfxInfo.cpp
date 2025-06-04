/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "GfxInfoBase.h"

using namespace mozilla::widget;

TEST(GfxInfo, GfxVersionEx_Compare)
{
  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                            DRIVER_LESS_THAN));
  EXPECT_TRUE(GfxVersionEx(9, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN));
  EXPECT_TRUE(GfxVersionEx(10, 2, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN));
  EXPECT_TRUE(GfxVersionEx(10, 3, 3, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 999)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN));

  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 1001)
                   .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                            DRIVER_LESS_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(9, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 2, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 3, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 999)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_LESS_THAN_OR_EQUAL));

  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                            DRIVER_GREATER_THAN));
  EXPECT_TRUE(GfxVersionEx(11, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN));
  EXPECT_TRUE(GfxVersionEx(10, 4, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN));
  EXPECT_TRUE(GfxVersionEx(10, 3, 5, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1001)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN));

  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 999)
                   .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                            DRIVER_GREATER_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(11, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 4, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 5, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1001)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_GREATER_THAN_OR_EQUAL));

  EXPECT_TRUE(
      GfxVersionEx(10, 3, 4, 1000)
          .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(), DRIVER_EQUAL));
  EXPECT_FALSE(
      GfxVersionEx(11, 3, 4, 1000)
          .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(), DRIVER_EQUAL));
  EXPECT_FALSE(
      GfxVersionEx(10, 4, 4, 1000)
          .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(), DRIVER_EQUAL));
  EXPECT_FALSE(
      GfxVersionEx(10, 3, 5, 1000)
          .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(), DRIVER_EQUAL));
  EXPECT_FALSE(
      GfxVersionEx(10, 3, 4, 1001)
          .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(), DRIVER_EQUAL));

  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                            DRIVER_NOT_EQUAL));
  EXPECT_TRUE(GfxVersionEx(11, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_NOT_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 4, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_NOT_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 5, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_NOT_EQUAL));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1001)
                  .Compare(GfxVersionEx(10, 3, 4, 1000), GfxVersionEx(),
                           DRIVER_NOT_EQUAL));

  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_FALSE(GfxVersionEx(10, 5, 6, 1100)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_FALSE(GfxVersionEx(9, 5, 6, 1100)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_FALSE(GfxVersionEx(11, 5, 6, 1100)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 3, 6, 1100)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1100)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1001)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 5, 6, 1099)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 4, 6, 1100)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_EXCLUSIVE));

  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 999)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_FALSE(GfxVersionEx(9, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_FALSE(GfxVersionEx(11, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1101)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 4, 5, 1050)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 3, 6, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 5, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxVersionEx(10, 5, 6, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE));

  EXPECT_FALSE(GfxVersionEx(10, 3, 4, 999)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_FALSE(GfxVersionEx(9, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_FALSE(GfxVersionEx(11, 3, 4, 1000)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_FALSE(GfxVersionEx(10, 5, 6, 1100)
                   .Compare(GfxVersionEx(10, 3, 4, 1000),
                            GfxVersionEx(10, 5, 6, 1100),
                            DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1101)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxVersionEx(10, 3, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxVersionEx(10, 4, 5, 1050)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxVersionEx(10, 3, 6, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxVersionEx(10, 5, 4, 1000)
                  .Compare(GfxVersionEx(10, 3, 4, 1000),
                           GfxVersionEx(10, 5, 6, 1100),
                           DRIVER_BETWEEN_INCLUSIVE_START));
}

TEST(GfxInfo, MatchingRefreshRateStatus)
{
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(RefreshRateStatus::Single,
                                                     RefreshRateStatus::Any));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::MultipleSame, RefreshRateStatus::Any));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(RefreshRateStatus::Mixed,
                                                     RefreshRateStatus::Any));

  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::Single, RefreshRateStatus::AnySame));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::MultipleSame, RefreshRateStatus::AnySame));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::Mixed, RefreshRateStatus::AnySame));

  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::Single, RefreshRateStatus::Single));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::MultipleSame, RefreshRateStatus::Single));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::Mixed, RefreshRateStatus::Single));

  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::Single, RefreshRateStatus::MultipleSame));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::MultipleSame, RefreshRateStatus::MultipleSame));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::Mixed, RefreshRateStatus::MultipleSame));

  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::Single, RefreshRateStatus::Mixed));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRateStatus(
      RefreshRateStatus::MultipleSame, RefreshRateStatus::Mixed));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRateStatus(RefreshRateStatus::Mixed,
                                                     RefreshRateStatus::Mixed));
}

TEST(GfxInfo, MatchingRefreshRates)
{
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(60, 60, 0, DRIVER_LESS_THAN));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(59, 60, 0, DRIVER_LESS_THAN));

  EXPECT_FALSE(
      GfxInfoBase::MatchingRefreshRates(61, 60, 0, DRIVER_LESS_THAN_OR_EQUAL));
  EXPECT_TRUE(
      GfxInfoBase::MatchingRefreshRates(60, 60, 0, DRIVER_LESS_THAN_OR_EQUAL));
  EXPECT_TRUE(
      GfxInfoBase::MatchingRefreshRates(59, 60, 0, DRIVER_LESS_THAN_OR_EQUAL));

  EXPECT_FALSE(
      GfxInfoBase::MatchingRefreshRates(60, 60, 0, DRIVER_GREATER_THAN));
  EXPECT_TRUE(
      GfxInfoBase::MatchingRefreshRates(61, 60, 0, DRIVER_GREATER_THAN));

  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(59, 60, 0,
                                                 DRIVER_GREATER_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(60, 60, 0,
                                                DRIVER_GREATER_THAN_OR_EQUAL));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(61, 60, 0,
                                                DRIVER_GREATER_THAN_OR_EQUAL));

  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(59, 60, 0, DRIVER_EQUAL));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(60, 60, 0, DRIVER_EQUAL));

  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(60, 60, 0, DRIVER_NOT_EQUAL));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(59, 60, 0, DRIVER_NOT_EQUAL));

  EXPECT_FALSE(
      GfxInfoBase::MatchingRefreshRates(60, 60, 120, DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(120, 60, 120,
                                                 DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_TRUE(
      GfxInfoBase::MatchingRefreshRates(61, 60, 120, DRIVER_BETWEEN_EXCLUSIVE));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(119, 60, 120,
                                                DRIVER_BETWEEN_EXCLUSIVE));

  EXPECT_FALSE(
      GfxInfoBase::MatchingRefreshRates(59, 60, 120, DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(121, 60, 120,
                                                 DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(
      GfxInfoBase::MatchingRefreshRates(60, 60, 120, DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(
      GfxInfoBase::MatchingRefreshRates(61, 60, 120, DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(119, 60, 120,
                                                DRIVER_BETWEEN_INCLUSIVE));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(120, 60, 120,
                                                DRIVER_BETWEEN_INCLUSIVE));

  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(
      59, 60, 120, DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(
      120, 60, 120, DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_FALSE(GfxInfoBase::MatchingRefreshRates(
      121, 60, 120, DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(
      60, 60, 120, DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(
      61, 60, 120, DRIVER_BETWEEN_INCLUSIVE_START));
  EXPECT_TRUE(GfxInfoBase::MatchingRefreshRates(
      119, 60, 120, DRIVER_BETWEEN_INCLUSIVE_START));
}
