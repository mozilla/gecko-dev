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

TEST(ReceiverFlowControlTest, NoNeedMaxAllowedFrameAtStart)
{
  ReceiverFlowControlBase fc(0);
  EXPECT_FALSE(fc.CapsuleNeeded());
}

TEST(ReceiverFlowControlTest, MaxAllowedAfterItemsRetired)
{
  ReceiverFlowControlBase fc(100);
  fc.Retire(49);
  EXPECT_FALSE(fc.CapsuleNeeded());
  fc.Retire(51);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 151u);
}

TEST(ReceiverFlowControlTest, ForceSendMaxAllowed)
{
  ReceiverFlowControlBase fc(100);
  fc.Retire(10);
  EXPECT_FALSE(fc.CapsuleNeeded());
}

TEST(ReceiverFlowControlTest, MultipleRetriesAfterFramePendingIsSet)
{
  ReceiverFlowControlBase fc(100);
  fc.Retire(51);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 151u);
  fc.Retire(61);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 161u);
  fc.Retire(88);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 188u);
  fc.Retire(90);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 190u);
  fc.CapsuleSent(190);
  EXPECT_FALSE(fc.CapsuleNeeded());
  fc.Retire(141);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 241u);
  fc.CapsuleSent(241);
  EXPECT_FALSE(fc.CapsuleNeeded());
}

TEST(ReceiverFlowControlTest, ChangingMaxActive)
{
  ReceiverFlowControlBase fc(100);
  fc.SetMaxActive(50);
  EXPECT_FALSE(fc.CapsuleNeeded());
  fc.Retire(60);
  EXPECT_FALSE(fc.CapsuleNeeded());
  fc.Retire(76);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 126u);
  fc.SetMaxActive(60);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 136u);
  fc.Retire(136);
  EXPECT_TRUE(fc.CapsuleNeeded());
  EXPECT_EQ(fc.NextLimit(), 196u);
}

TEST(RemoteStreamLimitsTest, HandlesStreamLimitLogicWithRawIds)
{
  RemoteStreamLimits fc(/*bidi=*/2, /*unidi=*/1);

  StreamId bidi0(1);   // Stream 0 (BiDi, server-initiated)
  StreamId bidi1(5);   // Stream 1
  StreamId bidi2(9);   // Stream 2
  StreamId bidi3(13);  // Stream 3

  StreamId uni0(3);   // Stream 0 (UniDi, server-initiated)
  StreamId uni1(7);   // Stream 1
  StreamId uni2(11);  // Stream 2

  // Initial streams should be allowed
  EXPECT_TRUE(fc[WebTransportStreamType::BiDi].IsNewStream(bidi0).unwrap());
  EXPECT_TRUE(fc[WebTransportStreamType::BiDi].IsNewStream(bidi1).unwrap());
  EXPECT_TRUE(fc[WebTransportStreamType::UniDi].IsNewStream(uni0).unwrap());

  // Exceed limits
  EXPECT_EQ(fc[WebTransportStreamType::BiDi].IsNewStream(bidi2).unwrapErr(),
            NS_ERROR_NOT_AVAILABLE);
  EXPECT_EQ(fc[WebTransportStreamType::UniDi].IsNewStream(uni1).unwrapErr(),
            NS_ERROR_NOT_AVAILABLE);

  // Take stream IDs
  EXPECT_EQ(fc[WebTransportStreamType::BiDi].TakeStreamId(), bidi0);
  EXPECT_EQ(fc[WebTransportStreamType::BiDi].TakeStreamId(), bidi1);
  EXPECT_EQ(fc[WebTransportStreamType::UniDi].TakeStreamId(), uni0);

  // Retire and allow new BiDi stream
  fc[WebTransportStreamType::BiDi].FlowControl().AddRetired(1);
  fc[WebTransportStreamType::BiDi].FlowControl().SendFlowControlUpdate();

  // Send MaxStreams capsule
  auto encoder =
      fc[WebTransportStreamType::BiDi].FlowControl().CreateMaxStreamsCapsule();
  EXPECT_TRUE(encoder.isSome());

  auto extractLimitFromEncoder = [](CapsuleEncoder& encoder) -> uint64_t {
    auto buffer = encoder.GetBuffer();
    RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
    UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
    parser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
    nsTArray<Capsule> parsed = listener->GetParsedCapsules();
    WebTransportMaxStreamsCapsule maxStreams =
        parsed[0].GetWebTransportMaxStreamsCapsule();
    return maxStreams.mLimit;
  };
  EXPECT_EQ(extractLimitFromEncoder(*encoder), 3u);

  EXPECT_TRUE(fc[WebTransportStreamType::BiDi].IsNewStream(bidi2).unwrap());
  EXPECT_EQ(fc[WebTransportStreamType::BiDi].TakeStreamId(), bidi2);

  EXPECT_EQ(fc[WebTransportStreamType::BiDi].IsNewStream(bidi3).unwrapErr(),
            NS_ERROR_NOT_AVAILABLE);

  // Retire and allow new UniDi stream
  fc[WebTransportStreamType::UniDi].FlowControl().AddRetired(1);
  fc[WebTransportStreamType::UniDi].FlowControl().SendFlowControlUpdate();

  auto encoder1 =
      fc[WebTransportStreamType::UniDi].FlowControl().CreateMaxStreamsCapsule();
  EXPECT_TRUE(encoder1.isSome());
  EXPECT_EQ(extractLimitFromEncoder(*encoder1), 2u);

  EXPECT_TRUE(fc[WebTransportStreamType::UniDi].IsNewStream(uni1).unwrap());
  EXPECT_EQ(fc[WebTransportStreamType::UniDi].TakeStreamId(), uni1);

  EXPECT_EQ(fc[WebTransportStreamType::UniDi].IsNewStream(uni2).unwrapErr(),
            NS_ERROR_NOT_AVAILABLE);
}
