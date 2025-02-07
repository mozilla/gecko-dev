/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "../../io/nsSegmentedBuffer.h"
#include "nsIEventTarget.h"

using namespace mozilla;

TEST(SegmentedBuffer, AppendAndPop)
{
  auto buf = MakeUnique<nsSegmentedBuffer>();
  buf->Init(4);
  char* seg;
  mozilla::UniqueFreePtr<char> poppedSeg;
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  poppedSeg = buf->PopFirstSegment();
  EXPECT_TRUE(poppedSeg) << "PopFirstSegment failed";
  poppedSeg = buf->PopFirstSegment();
  EXPECT_TRUE(poppedSeg) << "PopFirstSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  seg = buf->AppendNewSegment();
  EXPECT_TRUE(seg) << "AppendNewSegment failed";
  poppedSeg = buf->PopFirstSegment();
  EXPECT_TRUE(poppedSeg) << "PopFirstSegment failed";
  poppedSeg = buf->PopFirstSegment();
  EXPECT_TRUE(poppedSeg) << "PopFirstSegment failed";
  poppedSeg = buf->PopFirstSegment();
  EXPECT_TRUE(poppedSeg) << "PopFirstSegment failed";
  poppedSeg = buf->PopFirstSegment();
  EXPECT_TRUE(poppedSeg) << "PopFirstSegment failed";
}
