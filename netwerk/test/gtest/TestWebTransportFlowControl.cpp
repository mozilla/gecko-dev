/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "gtest/gtest.h"
#include "WebTransportFlowControl.h"
#include "Capsule.h"
#include "CapsuleEncoder.h"
#include "CapsuleParser.h"

using namespace mozilla;
using namespace mozilla::net;

TEST(SenderFlowControlTest, BlockedAtZero)
{
  SenderFlowControlBase fc(0);
  fc.Blocked();
  EXPECT_EQ(*fc.BlockedNeeded(), 0u);
}

TEST(SenderFlowControlTest, Blocked)
{
  SenderFlowControlBase fc(10);
  fc.Blocked();
  EXPECT_EQ(*fc.BlockedNeeded(), 10u);
}

TEST(SenderFlowControlTest, UpdateConsume)
{
  SenderFlowControlBase fc(10);
  fc.Consume(10);
  EXPECT_EQ(fc.Available(), 0u);
  fc.Update(5);
  EXPECT_EQ(fc.Available(), 0u);
  fc.Update(15);
  EXPECT_EQ(fc.Available(), 5u);
  fc.Consume(3);
  EXPECT_EQ(fc.Available(), 2u);
}

TEST(SenderFlowControlTest, UpdateClearsBlocked)
{
  SenderFlowControlBase fc(10);
  fc.Blocked();
  EXPECT_EQ(*fc.BlockedNeeded(), 10u);
  fc.Update(5);
  EXPECT_EQ(*fc.BlockedNeeded(), 10u);
  fc.Update(11);
  EXPECT_EQ(fc.BlockedNeeded(), Nothing());
}

TEST(LocalStreamLimitsTest, StreamIdAllocation)
{
  LocalStreamLimits fc;
  fc[WebTransportStreamType::BiDi].Update(2);
  fc[WebTransportStreamType::UniDi].Update(1);

  // Add streams
  EXPECT_EQ(*fc.TakeStreamId(WebTransportStreamType::BiDi), StreamId(0u));
  EXPECT_EQ(*fc.TakeStreamId(WebTransportStreamType::BiDi), StreamId(4u));
  EXPECT_TRUE(fc.TakeStreamId(WebTransportStreamType::BiDi).isNothing());
  EXPECT_EQ(*fc.TakeStreamId(WebTransportStreamType::UniDi), StreamId(2u));
  EXPECT_TRUE(fc.TakeStreamId(WebTransportStreamType::UniDi).isNothing());

  auto encoder = fc[WebTransportStreamType::BiDi].CreateStreamsBlockedCapsule();
  EXPECT_TRUE(encoder.isSome());

  auto extractLimitFromEncoder = [](CapsuleEncoder& encoder) -> uint64_t {
    auto buffer = encoder.GetBuffer();
    RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
    UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
    parser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
    nsTArray<Capsule> parsed = listener->GetParsedCapsules();
    WebTransportStreamsBlockedCapsule blocked =
        parsed[0].GetWebTransportStreamsBlockedCapsule();
    return blocked.mLimit;
  };

  EXPECT_EQ(extractLimitFromEncoder(*encoder), 2u);

  // Increase limit
  fc[WebTransportStreamType::BiDi].Update(3);
  fc[WebTransportStreamType::UniDi].Update(2);
  EXPECT_EQ(*fc.TakeStreamId(WebTransportStreamType::BiDi), StreamId(8u));
  EXPECT_TRUE(fc.TakeStreamId(WebTransportStreamType::BiDi).isNothing());
  EXPECT_EQ(*fc.TakeStreamId(WebTransportStreamType::UniDi), StreamId(6u));
  EXPECT_TRUE(fc.TakeStreamId(WebTransportStreamType::UniDi).isNothing());

  auto encoder1 =
      fc[WebTransportStreamType::UniDi].CreateStreamsBlockedCapsule();
  EXPECT_TRUE(encoder1.isSome());
  EXPECT_EQ(extractLimitFromEncoder(*encoder1), 2u);

  auto encoder2 =
      fc[WebTransportStreamType::BiDi].CreateStreamsBlockedCapsule();
  EXPECT_TRUE(encoder2.isSome());
  EXPECT_EQ(extractLimitFromEncoder(*encoder2), 3u);
}
