/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/OpenClientDirectoryInfo.h"

namespace mozilla::dom::quota::test {

TEST(DOM_Quota_OpenClientDirectoryInfo, BasicUsage)
{
  OpenClientDirectoryInfo openClientDirectoryInfo;

  EXPECT_EQ(openClientDirectoryInfo.ClientDirectoryLockHandleCount(), 0u);

  openClientDirectoryInfo.IncreaseClientDirectoryLockHandleCount();
  EXPECT_EQ(openClientDirectoryInfo.ClientDirectoryLockHandleCount(), 1u);

  openClientDirectoryInfo.IncreaseClientDirectoryLockHandleCount();
  EXPECT_EQ(openClientDirectoryInfo.ClientDirectoryLockHandleCount(), 2u);

  openClientDirectoryInfo.DecreaseClientDirectoryLockHandleCount();
  EXPECT_EQ(openClientDirectoryInfo.ClientDirectoryLockHandleCount(), 1u);

  openClientDirectoryInfo.DecreaseClientDirectoryLockHandleCount();
  EXPECT_EQ(openClientDirectoryInfo.ClientDirectoryLockHandleCount(), 0u);
}

}  //  namespace mozilla::dom::quota::test
