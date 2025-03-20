/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Capsule.h"
#include "CapsuleEncoder.h"
#include "CapsuleParser.h"

using namespace mozilla;
using namespace mozilla::net;

class CapsuleParserListener : public CapsuleParser::Listener {
 public:
  NS_INLINE_DECL_REFCOUNTING(CapsuleParserListener, override)

  CapsuleParserListener() = default;
  bool OnCapsule(Capsule&& aCapsule) override;
  void OnCapsuleParseFailure(nsresult aError) override;

  nsTArray<Capsule> GetParsedCapsules() { return std::move(mParsedCapsules); }

  Maybe<nsresult> GetErrorResult() { return mError; }

 private:
  virtual ~CapsuleParserListener() = default;

  nsTArray<Capsule> mParsedCapsules;
  Maybe<nsresult> mError = Nothing();
};

bool CapsuleParserListener::OnCapsule(Capsule&& aParsed) {
  mParsedCapsules.AppendElement(std::move(aParsed));
  return true;
}

void CapsuleParserListener::OnCapsuleParseFailure(nsresult aError) {
  mError = Some(aError);
}

TEST(TestCapsule, UnknownCapsule)
{
  nsTArray<uint8_t> data({0x1, 0x2});
  nsTArray<uint8_t> cloned(data.Clone());
  Capsule capsule = Capsule::Unknown(0x1234, std::move(cloned));
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  auto buffer = encoder.GetBuffer();

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
  bool res = parser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
  ASSERT_TRUE(res);

  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 1u);

  UnknownCapsule& unknown = parsed[0].GetUnknownCapsule();
  ASSERT_EQ(unknown.mData, data);

  ASSERT_TRUE(parser->IsBufferEmpty());
}

TEST(TestCapsule, CloseWebTransportSessionCapsule)
{
  nsCString reason("test");
  Capsule capsule = Capsule::CloseWebTransportSession(42, reason);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  auto buffer = encoder.GetBuffer();

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
  bool res = parser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
  ASSERT_TRUE(res);

  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 1u);

  CloseWebTransportSessionCapsule& parsedCapsule =
      parsed[0].GetCloseWebTransportSessionCapsule();
  ASSERT_EQ(parsedCapsule.mStatus, 42u);
  ASSERT_EQ(parsedCapsule.mReason, reason);

  ASSERT_TRUE(parser->IsBufferEmpty());
}

TEST(TestCapsule, CloseWebTransportSessionCapsuleWithReasonTooLong)
{
  nsCString reason;
  for (uint32_t i = 0; i < 1025; i++) {
    reason.AppendLiteral("1");
  }

  Capsule capsule = Capsule::CloseWebTransportSession(42, reason);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  auto buffer = encoder.GetBuffer();

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
  bool res = parser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
  ASSERT_FALSE(res);

  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isSome());
  ASSERT_EQ(*error, NS_ERROR_UNEXPECTED);

  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 0u);

  ASSERT_TRUE(parser->IsBufferEmpty());
}

TEST(TestCapsule, MultipleCapsules)
{
  nsCString reason("test");
  Capsule capsule1 = Capsule::CloseWebTransportSession(42, reason);

  nsTArray<uint8_t> data({0x1, 0x2, 0x3, 0x4});
  Capsule capsule2 = Capsule::WebTransportStreamData(0, true, std::move(data));

  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule1);
  encoder.EncodeCapsule(capsule2);

  auto buffer = encoder.GetBuffer();

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
  bool res = parser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
  ASSERT_TRUE(res);

  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 2u);

  CloseWebTransportSessionCapsule& parsedCapsule =
      parsed[0].GetCloseWebTransportSessionCapsule();
  ASSERT_EQ(parsedCapsule.mStatus, 42u);
  ASSERT_EQ(parsedCapsule.mReason, reason);

  WebTransportStreamDataCapsule& streamData =
      parsed[1].GetWebTransportStreamDataCapsule();
  ASSERT_EQ(streamData.mID, 0u);
  ASSERT_EQ(streamData.mData.Length(), 4u);

  ASSERT_TRUE(parser->IsBufferEmpty());
}

TEST(TestCapsule, WouldBlock)
{
  nsCString reason("test");
  Capsule capsule1 = Capsule::CloseWebTransportSession(42, reason);

  nsTArray<uint8_t> data;
  for (uint32_t i = 0; i < 4096; i++) {
    data.AppendElement(0x2);
  }
  Capsule capsule2 = Capsule::WebTransportStreamData(0, true, std::move(data));

  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule1);
  encoder.EncodeCapsule(capsule2);

  auto buffer = encoder.GetBuffer();

  const uint8_t* buf1 = buffer.Elements();
  uint32_t firstHalf = buffer.Length() / 2;

  const uint8_t* buf2 = buffer.Elements() + firstHalf;
  uint32_t secondHalf = buffer.Length() - firstHalf;

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
  bool res = parser->ProcessCapsuleData(buf1, firstHalf);
  ASSERT_TRUE(res);

  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 1u);

  CloseWebTransportSessionCapsule& parsedCapsule =
      parsed[0].GetCloseWebTransportSessionCapsule();
  ASSERT_EQ(parsedCapsule.mStatus, 42u);
  ASSERT_EQ(parsedCapsule.mReason, reason);

  ASSERT_FALSE(parser->IsBufferEmpty());

  res = parser->ProcessCapsuleData(buf2, secondHalf);
  parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 1u);

  WebTransportStreamDataCapsule& streamData =
      parsed[0].GetWebTransportStreamDataCapsule();
  ASSERT_EQ(streamData.mID, 0u);
  ASSERT_EQ(streamData.mData.Length(), 4096u);

  ASSERT_TRUE(parser->IsBufferEmpty());
}

TEST(TestCapsule, WouldBlock1)
{
  nsTArray<uint8_t> data;
  for (uint32_t i = 0; i < 4096; i++) {
    data.AppendElement(0x2);
  }
  Capsule capsule1 = Capsule::WebTransportStreamData(0, true, std::move(data));

  nsCString reason("test");
  Capsule capsule2 = Capsule::CloseWebTransportSession(42, reason);

  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule1);
  encoder.EncodeCapsule(capsule2);

  auto buffer = encoder.GetBuffer();

  const uint8_t* buf1 = buffer.Elements();
  uint32_t firstHalf = buffer.Length() / 2;

  const uint8_t* buf2 = buffer.Elements() + firstHalf;
  uint32_t secondHalf = buffer.Length() - firstHalf;

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
  bool res = parser->ProcessCapsuleData(buf1, firstHalf);
  ASSERT_TRUE(res);

  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 0u);

  ASSERT_FALSE(parser->IsBufferEmpty());

  res = parser->ProcessCapsuleData(buf2, secondHalf);
  parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 2u);
  error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  WebTransportStreamDataCapsule& streamData =
      parsed[0].GetWebTransportStreamDataCapsule();
  ASSERT_EQ(streamData.mID, 0u);
  ASSERT_EQ(streamData.mData.Length(), 4096u);

  CloseWebTransportSessionCapsule& parsedCapsule =
      parsed[1].GetCloseWebTransportSessionCapsule();
  ASSERT_EQ(parsedCapsule.mStatus, 42u);
  ASSERT_EQ(parsedCapsule.mReason, reason);

  ASSERT_TRUE(parser->IsBufferEmpty());
}

TEST(TestCapsule, WouldBlock2)
{
  nsTArray<uint8_t> data;
  for (uint32_t i = 0; i < 4096; i++) {
    data.AppendElement(0x2);
  }
  Capsule capsule1 = Capsule::WebTransportStreamData(0, true, std::move(data));

  nsCString reason("test");
  Capsule capsule2 = Capsule::CloseWebTransportSession(42, reason);

  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule1);
  encoder.EncodeCapsule(capsule2);

  auto buffer = encoder.GetBuffer();
  uint32_t totalLength = buffer.Length();

  // Split the buffer into three parts.
  uint32_t part1Length = totalLength / 3;
  uint32_t part2Length = totalLength / 3;
  uint32_t part3Length = totalLength - part1Length - part2Length;

  const uint8_t* buf1 = buffer.Elements();
  const uint8_t* buf2 = buffer.Elements() + part1Length;
  const uint8_t* buf3 = buffer.Elements() + part1Length + part2Length;

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);

  // Process first part.
  bool res = parser->ProcessCapsuleData(buf1, part1Length);
  ASSERT_TRUE(res);
  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());
  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  // At this stage, we might not have a complete capsule yet.
  ASSERT_EQ(parsed.Length(), 0u);
  // The parser's internal buffer should not be empty.
  ASSERT_FALSE(parser->IsBufferEmpty());

  // Process second part.
  res = parser->ProcessCapsuleData(buf2, part2Length);
  ASSERT_TRUE(res);
  error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());
  // Still, we might be waiting for more data to form a complete capsule.
  parsed = listener->GetParsedCapsules();
  // It's possible that no complete capsule is parsed yet, depending on how the
  // data splits. We won't assert parsed.Length() here since it may vary.

  // Process third part.
  res = parser->ProcessCapsuleData(buf3, part3Length);
  ASSERT_TRUE(res);
  parsed = listener->GetParsedCapsules();
  // At the end, we should have parsed both capsules.
  ASSERT_EQ(parsed.Length(), 2u);
  error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  WebTransportStreamDataCapsule& streamData =
      parsed[0].GetWebTransportStreamDataCapsule();
  ASSERT_EQ(streamData.mID, 0u);
  ASSERT_EQ(streamData.mData.Length(), 4096u);

  CloseWebTransportSessionCapsule& parsedCapsule =
      parsed[1].GetCloseWebTransportSessionCapsule();
  ASSERT_EQ(parsedCapsule.mStatus, 42u);
  ASSERT_EQ(parsedCapsule.mReason, reason);

  ASSERT_TRUE(parser->IsBufferEmpty());
}

TEST(TestCapsule, WebTransportMaxDataCapsule)
{
  Capsule capsule = Capsule::WebTransportMaxData(16384);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);

  auto buffer = encoder.GetBuffer();

  Capsule::LogBuffer(buffer.Elements(), buffer.Length());

  RefPtr<CapsuleParserListener> listener = new CapsuleParserListener();
  UniquePtr<CapsuleParser> parser = MakeUnique<CapsuleParser>(listener);
  bool res = parser->ProcessCapsuleData(buffer.Elements(), buffer.Length());
  ASSERT_TRUE(res);

  Maybe<nsresult> error = listener->GetErrorResult();
  ASSERT_TRUE(error.isNothing());

  nsTArray<Capsule> parsed = listener->GetParsedCapsules();
  ASSERT_EQ(parsed.Length(), 1u);

  WebTransportMaxDataCapsule& parsedCapsule =
      parsed[0].GetWebTransportMaxDataCapsule();
  ASSERT_EQ(parsedCapsule.mMaxDataSize, 16384u);

  ASSERT_TRUE(parser->IsBufferEmpty());
}
