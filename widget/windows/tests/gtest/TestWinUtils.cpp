/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "WinUtils.h"

using namespace mozilla;
using namespace mozilla::widget;

static LayoutDeviceIntRegion GetTestRegion() {
  LayoutDeviceIntRegion region;
  region.OrWith(LayoutDeviceIntRect(0, 0, 10, 10));
  region.OrWith(LayoutDeviceIntRect(15, 15, 50, 50));
  return region;
}

TEST(WinUtils, Regions)
{
  auto region = GetTestRegion();
  nsAutoRegion rgn(WinUtils::RegionToHRGN(region));
  ASSERT_NE(rgn, nullptr) << "Conversion should succeed";
  ASSERT_EQ(region, WinUtils::ConvertHRGNToRegion(rgn))
      << "Region should round-trip";
}
