/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "GfxInfoBase.h"

using namespace mozilla::widget;

TEST(GfxInfo, GfxVersionEx)
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

  GfxVersionEx noParts;
  EXPECT_TRUE(noParts.Parse(""_ns));
  EXPECT_EQ(noParts.Compare(GfxVersionEx(0, 0, 0, 0)), 0);

  GfxVersionEx onePart;
  EXPECT_TRUE(onePart.Parse("9"_ns));
  EXPECT_EQ(onePart.Compare(GfxVersionEx(9, 0, 0, 0)), 0);

  GfxVersionEx twoParts;
  EXPECT_TRUE(twoParts.Parse("1000.1"_ns));
  EXPECT_EQ(twoParts.Compare(GfxVersionEx(1000, 1, 0, 0)), 0);

  GfxVersionEx threeParts;
  EXPECT_TRUE(threeParts.Parse("44.1000.33"_ns));
  EXPECT_EQ(threeParts.Compare(GfxVersionEx(44, 1000, 33, 0)), 0);

  GfxVersionEx fourParts;
  EXPECT_TRUE(fourParts.Parse("10.5.4.1000"_ns));
  EXPECT_EQ(fourParts.Compare(GfxVersionEx(10, 5, 4, 1000)), 0);

  GfxVersionEx fiveParts;
  EXPECT_TRUE(fiveParts.Parse("10.5.4.1000.87"_ns));
  EXPECT_EQ(fiveParts.Compare(GfxVersionEx(10, 5, 4, 1000)), 0);

  GfxVersionEx badParts;
  EXPECT_FALSE(badParts.Parse("10.5.abc.25"_ns));
  EXPECT_EQ(badParts.Compare(GfxVersionEx(10, 5, 0, 0)), 0);
}
