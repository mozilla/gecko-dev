/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "ssl.h"

#define GTEST_HAS_RTTI 0
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "MockJsepCodecPreferences.h"
#include "jsapi/DefaultCodecPreferences.h"
#include "jsep/JsepTrack.h"
#include "sdp/SipccSdp.h"
#include "sdp/SipccSdpParser.h"
#include "sdp/SdpHelper.h"

using testing::UnorderedElementsAre;

namespace mozilla {

class JsepTrackTestBase : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    NSS_NoDB_Init(nullptr);
    NSS_SetDomesticPolicy();
  }
};

struct CodecOverrides {
  CodecOverrides() = default;
  bool addFecCodecs = false;
  bool preferRed = false;
  bool addDtmfCodec = false;
  bool enableRemb = true;
  bool enableTransportCC = true;
  void ApplyToPrefs(MockJsepCodecPreferences& aPrefs) const {
    aPrefs.mUseRemb = enableRemb;
    aPrefs.mUseTransportCC = enableTransportCC;
  }
};

class JsepTrackTest : public JsepTrackTestBase {
 public:
  JsepTrackTest()
      : mSendOff(SdpMediaSection::kAudio, sdp::kSend),
        mRecvOff(SdpMediaSection::kAudio, sdp::kRecv),
        mSendAns(SdpMediaSection::kAudio, sdp::kSend),
        mRecvAns(SdpMediaSection::kAudio, sdp::kRecv) {}

  void TearDown() override {
    if (::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->result()
            ->Failed()) {
      if (mOffer) {
        std::cerr << "Offer SDP: \n";
        mOffer->Serialize(std::cerr);
      }

      if (mAnswer) {
        std::cerr << "Answer SDP: \n";
        mAnswer->Serialize(std::cerr);
      }
    }
  }

  std::vector<UniquePtr<JsepCodecDescription>> MakeCodecs(
      const CodecOverrides overrides) const {
    MockJsepCodecPreferences prefs;
    overrides.ApplyToPrefs(prefs);

    prefs.mUseRemb = overrides.enableRemb;
    prefs.mUseTransportCC = overrides.enableTransportCC;
    JsepCodecPreferences& prefsRef = prefs;
    std::cout << "CodecPrefrences: " << prefsRef << "\n";
    std::vector<UniquePtr<JsepCodecDescription>> results;
    results.emplace_back(JsepAudioCodecDescription::CreateDefaultOpus(prefs));
    results.emplace_back(JsepAudioCodecDescription::CreateDefaultG722());
    if (overrides.addDtmfCodec) {
      results.emplace_back(
          JsepAudioCodecDescription::CreateDefaultTelephoneEvent());
    }

    if (overrides.addFecCodecs && overrides.preferRed) {
      results.emplace_back(JsepVideoCodecDescription::CreateDefaultRed(prefs));
    }
    results.emplace_back(JsepVideoCodecDescription::CreateDefaultVP8(prefs));
    results.emplace_back(JsepVideoCodecDescription::CreateDefaultH264_1(prefs));
    results.emplace_back(JsepVideoCodecDescription::CreateDefaultAV1(prefs));

    if (overrides.addFecCodecs) {
      if (!overrides.preferRed) {
        results.emplace_back(
            JsepVideoCodecDescription::CreateDefaultRed(prefs));
      }
      results.emplace_back(
          JsepVideoCodecDescription::CreateDefaultUlpFec(prefs));
    }

    results.emplace_back(new JsepApplicationCodecDescription(
        "webrtc-datachannel", 256, 5999, 499));

    return results;
  }

  void Init(SdpMediaSection::MediaType type) {
    InitCodecs(CodecOverrides{});
    InitTracks(type);
    InitSdp(type);
  }

  struct SplitOverrides {
    CodecOverrides offer = {};
    CodecOverrides answer = {};
  };

  void InitCodecs(const CodecOverrides& overrides) {
    mOffCodecs = MakeCodecs(overrides);
    mAnsCodecs = MakeCodecs(overrides);
  }
  void InitCodecs(const SplitOverrides& overrides) {
    mOffCodecs = MakeCodecs(overrides.offer);
    mAnsCodecs = MakeCodecs(overrides.answer);
  }

  void InitTracks(SdpMediaSection::MediaType type) {
    mSendOff = JsepTrack(type, sdp::kSend);
    if (type != SdpMediaSection::MediaType::kApplication) {
      mSendOff.UpdateStreamIds(std::vector<std::string>(1, "stream_id"));
    }
    mRecvOff = JsepTrack(type, sdp::kRecv);
    mSendOff.PopulateCodecs(mOffCodecs);
    mRecvOff.PopulateCodecs(mOffCodecs);

    mSendAns = JsepTrack(type, sdp::kSend);
    if (type != SdpMediaSection::MediaType::kApplication) {
      mSendAns.UpdateStreamIds(std::vector<std::string>(1, "stream_id"));
    }
    mRecvAns = JsepTrack(type, sdp::kRecv);
    mSendAns.PopulateCodecs(mAnsCodecs);
    mRecvAns.PopulateCodecs(mAnsCodecs);
  }

  void InitSdp(SdpMediaSection::MediaType type) {
    std::vector<std::string> msids(1, "*");
    std::string error;
    SdpHelper helper(&error);

    mOffer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
    mOffer->AddMediaSection(type, SdpDirectionAttribute::kSendrecv, 0,
                            SdpHelper::GetProtocolForMediaType(type),
                            sdp::kIPv4, "0.0.0.0");
    // JsepTrack doesn't set msid-semantic
    helper.SetupMsidSemantic(msids, mOffer.get());

    mAnswer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
    mAnswer->AddMediaSection(type, SdpDirectionAttribute::kSendrecv, 0,
                             SdpHelper::GetProtocolForMediaType(type),
                             sdp::kIPv4, "0.0.0.0");
    // JsepTrack doesn't set msid-semantic
    helper.SetupMsidSemantic(msids, mAnswer.get());
  }

  SdpMediaSection& GetOffer() { return mOffer->GetMediaSection(0); }

  SdpMediaSection& GetAnswer() { return mAnswer->GetMediaSection(0); }

  void CreateOffer() {
    mSendOff.AddToOffer(mSsrcGenerator, &GetOffer());
    mRecvOff.AddToOffer(mSsrcGenerator, &GetOffer());
  }

  void CreateAnswer() {
    if (mRecvAns.GetMediaType() != SdpMediaSection::MediaType::kApplication) {
      mRecvAns.RecvTrackSetRemote(*mOffer, GetOffer());
      mSendAns.SendTrackSetRemote(mSsrcGenerator, GetOffer());
    }

    mSendAns.AddToAnswer(GetOffer(), mSsrcGenerator, &GetAnswer());
    mRecvAns.AddToAnswer(GetOffer(), mSsrcGenerator, &GetAnswer());
  }

  void Negotiate() {
    if (mRecvOff.GetMediaType() != SdpMediaSection::MediaType::kApplication) {
      mRecvOff.RecvTrackSetRemote(*mAnswer, GetAnswer());
      mSendOff.SendTrackSetRemote(mSsrcGenerator, GetAnswer());
    }

    if (GetAnswer().IsSending()) {
      mSendAns.Negotiate(GetAnswer(), GetOffer(), GetAnswer());
      mRecvOff.Negotiate(GetAnswer(), GetAnswer(), GetOffer());
    }

    if (GetAnswer().IsReceiving()) {
      mRecvAns.Negotiate(GetAnswer(), GetOffer(), GetAnswer());
      mSendOff.Negotiate(GetAnswer(), GetAnswer(), GetOffer());
    }
  }

  void OfferAnswer(bool offerCodecsMatchAnswer = true) {
    CreateOffer();
    CreateAnswer();
    Negotiate();
    SanityCheck(offerCodecsMatchAnswer);
  }

  // TODO: Look into writing a macro that wraps an ASSERT_ and returns false
  // if it fails (probably requires writing a bool-returning function that
  // takes a void-returning lambda with a bool outparam, which will in turn
  // invokes the ASSERT_)
  static void CheckEncodingCount(size_t expected, const JsepTrack& send,
                                 const JsepTrack& recv) {
    if (expected) {
      ASSERT_TRUE(send.GetNegotiatedDetails());
      ASSERT_TRUE(recv.GetNegotiatedDetails());
    }

    if (!send.GetStreamIds().empty() && send.GetNegotiatedDetails()) {
      ASSERT_EQ(expected, send.GetNegotiatedDetails()->GetEncodingCount());
    }

    if (!recv.GetStreamIds().empty() && recv.GetNegotiatedDetails()) {
      ASSERT_EQ(expected, recv.GetNegotiatedDetails()->GetEncodingCount());
    }
  }

  void CheckOffEncodingCount(size_t expected) const {
    CheckEncodingCount(expected, mSendOff, mRecvAns);
  }

  void CheckAnsEncodingCount(size_t expected) const {
    CheckEncodingCount(expected, mSendAns, mRecvOff);
  }

  UniquePtr<JsepCodecDescription> GetCodec(const JsepTrack& track,
                                           SdpMediaSection::MediaType type,
                                           size_t expectedSize,
                                           size_t codecIndex) const {
    if (!track.GetNegotiatedDetails() ||
        track.GetNegotiatedDetails()->GetEncodingCount() != 1U ||
        track.GetMediaType() != type) {
      return nullptr;
    }
    const auto& codecs =
        track.GetNegotiatedDetails()->GetEncoding(0).GetCodecs();
    // it should not be possible for codecs to have a different type
    // than the track, but we'll check the codec here just in case.
    if (codecs.size() != expectedSize || codecIndex >= expectedSize ||
        codecs[codecIndex]->Type() != type) {
      return nullptr;
    }
    return UniquePtr<JsepCodecDescription>(codecs[codecIndex]->Clone());
  }

  UniquePtr<JsepVideoCodecDescription> GetVideoCodec(
      const JsepTrack& track, size_t expectedSize = 1,
      size_t codecIndex = 0) const {
    auto codec =
        GetCodec(track, SdpMediaSection::kVideo, expectedSize, codecIndex);
    return UniquePtr<JsepVideoCodecDescription>(
        static_cast<JsepVideoCodecDescription*>(codec.release()));
  }

  UniquePtr<JsepAudioCodecDescription> GetAudioCodec(
      const JsepTrack& track, size_t expectedSize = 1,
      size_t codecIndex = 0) const {
    auto codec =
        GetCodec(track, SdpMediaSection::kAudio, expectedSize, codecIndex);
    return UniquePtr<JsepAudioCodecDescription>(
        static_cast<JsepAudioCodecDescription*>(codec.release()));
  }

  void CheckOtherFbExists(const JsepVideoCodecDescription& videoCodec,
                          SdpRtcpFbAttributeList::Type type) const {
    for (const auto& fb : videoCodec.mOtherFbTypes) {
      if (fb.type == type) {
        return;  // found the RtcpFb type, so stop looking
      }
    }
    FAIL();  // RtcpFb type not found
  }

  void SanityCheckRtcpFbs(const JsepVideoCodecDescription& a,
                          const JsepVideoCodecDescription& b) const {
    ASSERT_EQ(a.mNackFbTypes.size(), b.mNackFbTypes.size());
    ASSERT_EQ(a.mAckFbTypes.size(), b.mAckFbTypes.size());
    ASSERT_EQ(a.mCcmFbTypes.size(), b.mCcmFbTypes.size());
    ASSERT_EQ(a.mOtherFbTypes.size(), b.mOtherFbTypes.size());
  }

  void SanityCheckCodecs(const JsepCodecDescription& a,
                         const JsepCodecDescription& b) const {
#define MSG                                                               \
  "For codecs " << a.mName << " (" << a.mDirection << ") and " << b.mName \
                << " (" << b.mDirection << ")"
    ASSERT_EQ(a.Type(), b.Type()) << MSG;
    if (a.Type() != SdpMediaSection::kApplication) {
      ASSERT_EQ(a.mDefaultPt, b.mDefaultPt) << MSG;
    }
    ASSERT_EQ(a.mName, b.mName);
    if (!mExpectDifferingFmtp) {
      ASSERT_EQ(a.mSdpFmtpLine, b.mSdpFmtpLine) << MSG;
    }
    ASSERT_EQ(a.mClock, b.mClock) << MSG;
    ASSERT_EQ(a.mChannels, b.mChannels) << MSG;
    ASSERT_NE(a.mDirection, b.mDirection) << MSG;
    // These constraints are for fmtp and rid, which _are_ signaled
    ASSERT_EQ(a.mConstraints, b.mConstraints) << MSG;
#undef MSG

    if (a.Type() == SdpMediaSection::kVideo) {
      SanityCheckRtcpFbs(static_cast<const JsepVideoCodecDescription&>(a),
                         static_cast<const JsepVideoCodecDescription&>(b));
    }
  }

  void SanityCheckEncodings(const JsepTrackEncoding& a,
                            const JsepTrackEncoding& b) const {
    ASSERT_EQ(a.GetCodecs().size(), b.GetCodecs().size());
    for (size_t i = 0; i < a.GetCodecs().size(); ++i) {
      SanityCheckCodecs(*a.GetCodecs()[i], *b.GetCodecs()[i]);
    }

    ASSERT_EQ(a.mRid, b.mRid);
    // mConstraints will probably differ, since they are not signaled to the
    // other side.
  }

  void SanityCheckNegotiatedDetails(const JsepTrack& aTrack,
                                    const JsepTrack& bTrack,
                                    bool codecsMustMatch) const {
    const auto aDetails = *aTrack.GetNegotiatedDetails();
    const auto bDetails = *bTrack.GetNegotiatedDetails();
    ASSERT_EQ(aDetails.GetEncodingCount(), bDetails.GetEncodingCount());
    if (codecsMustMatch) {
      for (size_t i = 0; i < aDetails.GetEncodingCount(); ++i) {
        SanityCheckEncodings(aDetails.GetEncoding(i), bDetails.GetEncoding(i));
      }
    }

    ASSERT_EQ(aTrack.GetUniqueReceivePayloadTypes().size(),
              bTrack.GetUniqueReceivePayloadTypes().size());
    for (size_t i = 0; i < aTrack.GetUniqueReceivePayloadTypes().size(); ++i) {
      ASSERT_EQ(aTrack.GetUniqueReceivePayloadTypes()[i],
                bTrack.GetUniqueReceivePayloadTypes()[i]);
    }
  }

  void SanityCheckTracks(const JsepTrack& a, const JsepTrack& b,
                         bool codecsMustMatch) const {
    if (!a.GetNegotiatedDetails()) {
      ASSERT_FALSE(!!b.GetNegotiatedDetails());
      return;
    }

    ASSERT_TRUE(!!a.GetNegotiatedDetails());
    ASSERT_TRUE(!!b.GetNegotiatedDetails());
    ASSERT_EQ(a.GetMediaType(), b.GetMediaType());
    ASSERT_EQ(a.GetStreamIds(), b.GetStreamIds());
    ASSERT_EQ(a.GetCNAME(), b.GetCNAME());
    ASSERT_NE(a.GetDirection(), b.GetDirection());
    ASSERT_EQ(a.GetSsrcs().size(), b.GetSsrcs().size());
    for (size_t i = 0; i < a.GetSsrcs().size(); ++i) {
      ASSERT_EQ(a.GetSsrcs()[i], b.GetSsrcs()[i]);
    }

    SanityCheckNegotiatedDetails(a, b, codecsMustMatch);
  }

  void SanityCheck(bool offerCodecsMatchAnswer = true) const {
    SanityCheckTracks(mSendOff, mRecvAns, true);
    SanityCheckTracks(mRecvOff, mSendAns, offerCodecsMatchAnswer);
  }

 protected:
  JsepTrack mSendOff;
  JsepTrack mRecvOff;
  JsepTrack mSendAns;
  JsepTrack mRecvAns;
  std::vector<UniquePtr<JsepCodecDescription>> mOffCodecs;
  std::vector<UniquePtr<JsepCodecDescription>> mAnsCodecs;
  UniquePtr<Sdp> mOffer;
  UniquePtr<Sdp> mAnswer;
  SsrcGenerator mSsrcGenerator;
  bool mExpectDifferingFmtp = false;
};

TEST_F(JsepTrackTestBase, CreateDestroy) {}

TEST_F(JsepTrackTest, CreateDestroy) { Init(SdpMediaSection::kAudio); }

TEST_F(JsepTrackTest, AudioNegotiation) {
  Init(SdpMediaSection::kAudio);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoNegotiation) {
  Init(SdpMediaSection::kVideo);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

class CheckForCodecType {
 public:
  explicit CheckForCodecType(SdpMediaSection::MediaType type, bool* result)
      : mResult(result), mType(type) {}

  void operator()(const UniquePtr<JsepCodecDescription>& codec) {
    if (codec->Type() == mType) {
      *mResult = true;
    }
  }

 private:
  bool* mResult;
  SdpMediaSection::MediaType mType;
};

TEST_F(JsepTrackTest, CheckForMismatchedAudioCodecAndVideoTrack) {
  std::vector<UniquePtr<JsepCodecDescription>> offerCodecs;

  // make codecs including telephone-event (an audio codec)
  offerCodecs = MakeCodecs({.addDtmfCodec = true});
  JsepTrack videoTrack(SdpMediaSection::kVideo, sdp::kSend);
  videoTrack.UpdateStreamIds(std::vector<std::string>(1, "stream_id"));
  // populate codecs and then make sure we don't have any audio codecs
  // in the video track
  videoTrack.PopulateCodecs(offerCodecs);

  bool found = false;
  videoTrack.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);

  found = false;
  videoTrack.ForEachCodec(CheckForCodecType(SdpMediaSection::kVideo, &found));
  ASSERT_TRUE(found);  // for sanity, make sure we did find video codecs
}

TEST_F(JsepTrackTest, CheckVideoTrackWithHackedDtmfSdp) {
  Init(SdpMediaSection::kVideo);
  CreateOffer();
  // make sure we don't find sdp containing telephone-event in video track
  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  // force audio codec telephone-event into video m= section of offer
  GetOffer().AddCodec("101", "telephone-event", 8000, 1);
  // make sure we _do_ find sdp containing telephone-event in video track
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  CreateAnswer();
  // make sure we don't find sdp containing telephone-event in video track
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  // force audio codec telephone-event into video m= section of answer
  GetAnswer().AddCodec("101", "telephone-event", 8000, 1);
  // make sure we _do_ find sdp containing telephone-event in video track
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  // make sure we still don't find any audio codecs in the video track after
  // hacking the sdp
  bool found = false;
  mSendOff.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mRecvOff.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mSendAns.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
  mRecvAns.ForEachCodec(CheckForCodecType(SdpMediaSection::kAudio, &found));
  ASSERT_FALSE(found);
}

TEST_F(JsepTrackTest, AudioNegotiationOffererDtmf) {
  InitCodecs(
      {.offer = {.addDtmfCodec = true}, .answer = {.addDtmfCodec = false}});

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer(false);

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationAnswererDtmf) {
  InitCodecs(
      {.offer = {.addDtmfCodec = false}, .answer = {.addDtmfCodec = true}});

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_EQ(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 2, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 2, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationOffererAnswererDtmf) {
  InitCodecs(
      {.offer = {.addDtmfCodec = true}, .answer = {.addDtmfCodec = true}});

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:101 0-15"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererNoFmtpAnswererFmtp) {
  InitCodecs(
      {.offer = {.addDtmfCodec = true}, .answer = {.addDtmfCodec = true}});

  mExpectDifferingFmtp = true;

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);

  CreateOffer();
  GetOffer().RemoveFmtp("101");

  CreateAnswer();

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_EQ(mOffer->ToString().find("a=fmtp:101"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=fmtp:101 0-15"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("0-15", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("0-15", track->mSdpFmtpLine.valueOr("nothing"));
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererFmtpAnswererNoFmtp) {
  InitCodecs(
      {.offer = {.addDtmfCodec = true}, .answer = {.addDtmfCodec = true}});

  mExpectDifferingFmtp = true;

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);

  CreateOffer();

  CreateAnswer();
  GetAnswer().RemoveFmtp("101");

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_NE(mOffer->ToString().find("a=fmtp:101 0-15"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("0-15", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("0-15", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
}

TEST_F(JsepTrackTest, AudioNegotiationDtmfOffererNoFmtpAnswererNoFmtp) {
  InitCodecs(
      {.offer = {.addDtmfCodec = true}, .answer = {.addDtmfCodec = true}});

  mExpectDifferingFmtp = true;

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);

  CreateOffer();
  GetOffer().RemoveFmtp("101");

  CreateAnswer();
  GetAnswer().RemoveFmtp("101");

  Negotiate();
  SanityCheck();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:101 telephone-event"),
            std::string::npos);

  ASSERT_EQ(mOffer->ToString().find("a=fmtp:101"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=fmtp:101"), std::string::npos);

  UniquePtr<JsepAudioCodecDescription> track;
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("109", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("9", track->mDefaultPt);
  ASSERT_TRUE((track = GetAudioCodec(mSendOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mRecvOff, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mSendAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
  ASSERT_TRUE((track = GetAudioCodec(mRecvAns, 3, 2)));
  ASSERT_EQ("101", track->mDefaultPt);
  ASSERT_EQ("nothing", track->mSdpFmtpLine.valueOr("nothing"));
}

TEST_F(JsepTrackTest, VideoNegotationOffererFEC) {
  InitCodecs(
      {.offer = {.addFecCodecs = true}, .answer = {.addFecCodecs = false}});

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer(false);

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 5, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 3, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 5, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 3, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationAnswererFEC) {
  InitCodecs(
      {.offer = {.addFecCodecs = false}, .answer = {.addFecCodecs = true}});

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_EQ(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 3, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 3, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 3, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 3, 1)));
  ASSERT_EQ("126", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFEC) {
  InitCodecs(
      {.offer = {.addFecCodecs = true}, .answer = {.addFecCodecs = true}});

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECPreferred) {
  InitCodecs({.offer = {.addFecCodecs = true, .preferRed = true},
              .answer = {.addFecCodecs = true}});

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  UniquePtr<JsepVideoCodecDescription> track;
  // We should have 4 codecs, the first of which is VP8, because having a
  // pseudo codec come first is silly.
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
}

// Make sure we only put the right things in the fmtp:122 120/.... line
TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECMismatch) {
  InitCodecs({.offer = {.addFecCodecs = true, .preferRed = true},
              .answer = {.addFecCodecs = true}});
  // remove h264 & AV1 from answer codecs
  ASSERT_EQ("H264", mAnsCodecs[3]->mName);
  ASSERT_EQ("AV1", mAnsCodecs[4]->mName);
  mAnsCodecs.erase(mAnsCodecs.begin() + 4);
  mAnsCodecs.erase(mAnsCodecs.begin() + 3);

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer(false);

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);

  // We should have 3 codecs, the first of which is VP8, because having a
  // pseudo codec come first is silly.
  UniquePtr<JsepVideoCodecDescription> track;
  ASSERT_TRUE((track = GetVideoCodec(mSendOff, 3)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvOff, 5)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mSendAns, 3)));
  ASSERT_EQ("120", track->mDefaultPt);
  ASSERT_TRUE((track = GetVideoCodec(mRecvAns, 3)));
  ASSERT_EQ("120", track->mDefaultPt);
}

TEST_F(JsepTrackTest, VideoNegotationOffererAnswererFECZeroVP9Codec) {
  MockJsepCodecPreferences prefs;
  mOffCodecs = MakeCodecs({.addFecCodecs = true});
  auto vp9 = JsepVideoCodecDescription::CreateDefaultVP9(prefs);
  vp9->mDefaultPt = "0";
  mOffCodecs.push_back(std::move(vp9));

  ASSERT_EQ(9U, mOffCodecs.size());
  JsepVideoCodecDescription& red =
      static_cast<JsepVideoCodecDescription&>(*mOffCodecs[5]);
  ASSERT_EQ("red", red.mName);

  mAnsCodecs = MakeCodecs({.addFecCodecs = true});

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(mOffer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mOffer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:122 red"), std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtpmap:123 ulpfec"), std::string::npos);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferRemb) {
  // enable remb on the offer codecs
  InitCodecs({.offer = {.enableRemb = true, .enableTransportCC = false},
              .answer = {.enableRemb = false, .enableTransportCC = false}});
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure REMB is on offer and not on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);

  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationAnswerRemb) {
  InitCodecs({.offer = {.enableRemb = false, .enableTransportCC = false},
              .answer = {.enableRemb = true, .enableTransportCC = false}});
  // enable remb on the answer codecs
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableRemb();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure REMB is not on offer and not on answer
  ASSERT_EQ(mOffer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferAnswerRemb) {
  InitCodecs({.offer = {.enableRemb = true, .enableTransportCC = false},
              .answer = {.enableRemb = true, .enableTransportCC = false}});
  // enable remb on the offer and answer codecs
  ((JsepVideoCodecDescription&)*mOffCodecs[2]).EnableRemb();
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableRemb();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure REMB is on offer and on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtcp-fb:120 goog-remb"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kRemb);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferTransportCC) {
  InitCodecs({.offer = {.enableRemb = false, .enableTransportCC = true},
              .answer = {.enableRemb = false, .enableTransportCC = false}});
  // enable TransportCC on the offer codecs
  ((JsepVideoCodecDescription&)*mOffCodecs[2]).EnableTransportCC();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure TransportCC is on offer and not on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationAnswerTransportCC) {
  InitCodecs({.offer = {.enableRemb = false, .enableTransportCC = false},
              .answer = {.enableRemb = false, .enableTransportCC = true}});
  // enable TransportCC on the answer codecs
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableTransportCC();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure TransportCC is not on offer and not on answer
  ASSERT_EQ(mOffer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  ASSERT_EQ(mAnswer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 0U);
}

TEST_F(JsepTrackTest, VideoNegotiationOfferAnswerTransportCC) {
  InitCodecs({.enableRemb = false, .enableTransportCC = true});
  // enable TransportCC on the offer and answer codecs
  ((JsepVideoCodecDescription&)*mOffCodecs[2]).EnableTransportCC();
  ((JsepVideoCodecDescription&)*mAnsCodecs[2]).EnableTransportCC();
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // make sure TransportCC is on offer and on answer
  ASSERT_NE(mOffer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  ASSERT_NE(mAnswer->ToString().find("a=rtcp-fb:120 transport-cc"),
            std::string::npos);
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
  ASSERT_TRUE((codec = GetVideoCodec(mSendAns, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ(codec->mOtherFbTypes.size(), 1U);
  CheckOtherFbExists(*codec, SdpRtcpFbAttributeList::kTransportCC);
}

TEST_F(JsepTrackTest, AudioOffSendonlyAnsRecvonly) {
  Init(SdpMediaSection::kAudio);
  GetOffer().SetDirection(SdpDirectionAttribute::kSendonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, VideoOffSendonlyAnsRecvonly) {
  Init(SdpMediaSection::kVideo);
  GetOffer().SetDirection(SdpDirectionAttribute::kSendonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, AudioOffSendrecvAnsRecvonly) {
  Init(SdpMediaSection::kAudio);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, VideoOffSendrecvAnsRecvonly) {
  Init(SdpMediaSection::kVideo);
  GetAnswer().SetDirection(SdpDirectionAttribute::kRecvonly);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(0);
}

TEST_F(JsepTrackTest, AudioOffRecvonlyAnsSendonly) {
  Init(SdpMediaSection::kAudio);
  GetOffer().SetDirection(SdpDirectionAttribute::kRecvonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoOffRecvonlyAnsSendonly) {
  Init(SdpMediaSection::kVideo);
  GetOffer().SetDirection(SdpDirectionAttribute::kRecvonly);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, AudioOffSendrecvAnsSendonly) {
  Init(SdpMediaSection::kAudio);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, VideoOffSendrecvAnsSendonly) {
  Init(SdpMediaSection::kVideo);
  GetAnswer().SetDirection(SdpDirectionAttribute::kSendonly);
  OfferAnswer();
  CheckOffEncodingCount(0);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, DataChannelDraft05) {
  InitCodecs(CodecOverrides{});
  InitTracks(SdpMediaSection::kApplication);

  mOffer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
  mOffer->AddMediaSection(SdpMediaSection::kApplication,
                          SdpDirectionAttribute::kSendrecv, 0,
                          SdpMediaSection::kDtlsSctp, sdp::kIPv4, "0.0.0.0");
  mAnswer.reset(new SipccSdp(SdpOrigin("", 0, 0, sdp::kIPv4, "")));
  mAnswer->AddMediaSection(SdpMediaSection::kApplication,
                           SdpDirectionAttribute::kSendrecv, 0,
                           SdpMediaSection::kDtlsSctp, sdp::kIPv4, "0.0.0.0");

  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=sctpmap:5999 webrtc-datachannel 256"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=sctpmap:5999 webrtc-datachannel 256"));
  // Note: this is testing for a workaround, see bug 1335262 for details
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=max-message-size:499"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=max-message-size:499"));
  ASSERT_EQ(std::string::npos, mOffer->ToString().find("a=sctp-port"));
  ASSERT_EQ(std::string::npos, mAnswer->ToString().find("a=sctp-port"));
}

TEST_F(JsepTrackTest, DataChannelDraft21) {
  Init(SdpMediaSection::kApplication);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=sctp-port:5999"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=sctp-port:5999"));
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=max-message-size:499"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=max-message-size:499"));
  ASSERT_EQ(std::string::npos, mOffer->ToString().find("a=sctpmap"));
  ASSERT_EQ(std::string::npos, mAnswer->ToString().find("a=sctpmap"));
}

TEST_F(JsepTrackTest, DataChannelDraft21AnswerWithDifferentPort) {
  InitCodecs(CodecOverrides{});

  mOffCodecs.pop_back();
  mOffCodecs.emplace_back(new JsepApplicationCodecDescription(
      "webrtc-datachannel", 256, 4555, 10544));

  InitTracks(SdpMediaSection::kApplication);
  InitSdp(SdpMediaSection::kApplication);

  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);

  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=sctp-port:4555"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=sctp-port:5999"));
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=max-message-size:10544"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=max-message-size:499"));
  ASSERT_EQ(std::string::npos, mOffer->ToString().find("a=sctpmap"));
  ASSERT_EQ(std::string::npos, mAnswer->ToString().find("a=sctpmap"));
}

TEST_F(JsepTrackTest, SimulcastRejected) {
  Init(SdpMediaSection::kVideo);
  std::vector<std::string> rids;
  rids.push_back("foo");
  rids.push_back("bar");
  mSendOff.SetRids(rids);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, SimulcastPrevented) {
  Init(SdpMediaSection::kVideo);
  std::vector<std::string> rids;
  rids.push_back("foo");
  rids.push_back("bar");
  mSendAns.SetRids(rids);
  OfferAnswer();
  CheckOffEncodingCount(1);
  CheckAnsEncodingCount(1);
}

TEST_F(JsepTrackTest, SimulcastOfferer) {
  Init(SdpMediaSection::kVideo);
  std::vector<std::string> rids;
  rids.push_back("foo");
  rids.push_back("bar");
  mSendOff.SetRids(rids);
  CreateOffer();
  CreateAnswer();
  // Add simulcast/rid to answer
  mRecvAns.AddToMsection(rids, sdp::kRecv, mSsrcGenerator, false, &GetAnswer());
  Negotiate();
  ASSERT_TRUE(mSendOff.GetNegotiatedDetails());
  ASSERT_EQ(2U, mSendOff.GetNegotiatedDetails()->GetEncodingCount());
  ASSERT_EQ("foo", mSendOff.GetNegotiatedDetails()->GetEncoding(0).mRid);
  ASSERT_EQ("bar", mSendOff.GetNegotiatedDetails()->GetEncoding(1).mRid);
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=simulcast:send foo;bar"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=simulcast:recv foo;bar"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:foo send"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:bar send"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:foo recv"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:bar recv"));
}

TEST_F(JsepTrackTest, SimulcastOffererWithRtx) {
  Init(SdpMediaSection::kVideo);
  std::vector<std::string> rids;
  rids.push_back("foo");
  rids.push_back("bar");
  rids.push_back("pop");
  mSendOff.SetRids(rids);
  mSendOff.AddToMsection(rids, sdp::kSend, mSsrcGenerator, true, &GetOffer());
  mRecvOff.AddToMsection(rids, sdp::kSend, mSsrcGenerator, true, &GetOffer());
  CreateAnswer();
  // Add simulcast/rid to answer
  mRecvAns.AddToMsection(rids, sdp::kRecv, mSsrcGenerator, false, &GetAnswer());
  Negotiate();

  ASSERT_EQ(3U, mSendOff.GetSsrcs().size());
  const auto posSsrc0 =
      mOffer->ToString().find(std::to_string(mSendOff.GetSsrcs()[0]));
  const auto posSsrc1 =
      mOffer->ToString().find(std::to_string(mSendOff.GetSsrcs()[1]));
  const auto posSsrc2 =
      mOffer->ToString().find(std::to_string(mSendOff.GetSsrcs()[2]));
  ASSERT_NE(std::string::npos, posSsrc0);
  ASSERT_NE(std::string::npos, posSsrc1);
  ASSERT_NE(std::string::npos, posSsrc2);
  ASSERT_GT(posSsrc1, posSsrc0);
  ASSERT_GT(posSsrc2, posSsrc0);
  ASSERT_GT(posSsrc2, posSsrc1);

  ASSERT_EQ(3U, mSendOff.GetRtxSsrcs().size());
  const auto posRtxSsrc0 =
      mOffer->ToString().find(std::to_string(mSendOff.GetRtxSsrcs()[0]));
  const auto posRtxSsrc1 =
      mOffer->ToString().find(std::to_string(mSendOff.GetRtxSsrcs()[1]));
  const auto posRtxSsrc2 =
      mOffer->ToString().find(std::to_string(mSendOff.GetRtxSsrcs()[2]));
  ASSERT_NE(std::string::npos, posRtxSsrc0);
  ASSERT_NE(std::string::npos, posRtxSsrc1);
  ASSERT_NE(std::string::npos, posRtxSsrc2);
  ASSERT_GT(posRtxSsrc1, posRtxSsrc0);
  ASSERT_GT(posRtxSsrc2, posRtxSsrc0);
  ASSERT_GT(posRtxSsrc2, posRtxSsrc1);
}

TEST_F(JsepTrackTest, SimulcastAnswerer) {
  Init(SdpMediaSection::kVideo);
  std::vector<std::string> rids;
  rids.push_back("foo");
  rids.push_back("bar");
  mSendAns.SetRids(rids);
  CreateOffer();
  // Add simulcast/rid to offer
  mRecvOff.AddToMsection(rids, sdp::kRecv, mSsrcGenerator, false, &GetOffer());
  CreateAnswer();
  Negotiate();
  ASSERT_TRUE(mSendAns.GetNegotiatedDetails());
  ASSERT_EQ(2U, mSendAns.GetNegotiatedDetails()->GetEncodingCount());
  ASSERT_EQ("foo", mSendAns.GetNegotiatedDetails()->GetEncoding(0).mRid);
  ASSERT_EQ("bar", mSendAns.GetNegotiatedDetails()->GetEncoding(1).mRid);
  ASSERT_NE(std::string::npos,
            mOffer->ToString().find("a=simulcast:recv foo;bar"));
  ASSERT_NE(std::string::npos,
            mAnswer->ToString().find("a=simulcast:send foo;bar"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:foo recv"));
  ASSERT_NE(std::string::npos, mOffer->ToString().find("a=rid:bar recv"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:foo send"));
  ASSERT_NE(std::string::npos, mAnswer->ToString().find("a=rid:bar send"));
}

#define VERIFY_OPUS_MAX_PLAYBACK_RATE(track, expectedRate)          \
  {                                                                 \
    JsepTrack& copy(track);                                         \
    ASSERT_TRUE(copy.GetNegotiatedDetails());                       \
    ASSERT_TRUE(copy.GetNegotiatedDetails()->GetEncodingCount());   \
    for (const auto& codec :                                        \
         copy.GetNegotiatedDetails()->GetEncoding(0).GetCodecs()) { \
      if (codec->mName == "opus") {                                 \
        JsepAudioCodecDescription& audioCodec =                     \
            static_cast<JsepAudioCodecDescription&>(*codec);        \
        ASSERT_EQ((expectedRate), audioCodec.mMaxPlaybackRate);     \
      }                                                             \
    };                                                              \
  }

#define VERIFY_OPUS_FORCE_MONO(track, expected)                       \
  {                                                                   \
    JsepTrack& copy(track);                                           \
    ASSERT_TRUE(copy.GetNegotiatedDetails());                         \
    ASSERT_TRUE(copy.GetNegotiatedDetails()->GetEncodingCount());     \
    for (const auto& codec :                                          \
         copy.GetNegotiatedDetails()->GetEncoding(0).GetCodecs()) {   \
      if (codec->mName == "opus") {                                   \
        JsepAudioCodecDescription& audioCodec =                       \
            static_cast<JsepAudioCodecDescription&>(*codec);          \
        /* gtest has some compiler warnings when using ASSERT_EQ with \
         * booleans. */                                               \
        ASSERT_EQ((int)(expected), (int)audioCodec.mForceMono);       \
      }                                                               \
    };                                                                \
  }

TEST_F(JsepTrackTest, DefaultOpusParameters) {
  Init(SdpMediaSection::kAudio);
  OfferAnswer();

  VERIFY_OPUS_MAX_PLAYBACK_RATE(
      mSendOff, SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(
      mSendAns, SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvOff, 0U);
  VERIFY_OPUS_FORCE_MONO(mRecvOff, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvAns, 0U);
  VERIFY_OPUS_FORCE_MONO(mRecvAns, false);
}

TEST_F(JsepTrackTest, NonDefaultOpusParameters) {
  InitCodecs(CodecOverrides{});
  for (auto& codec : mAnsCodecs) {
    if (codec->mName == "opus") {
      JsepAudioCodecDescription* audioCodec =
          static_cast<JsepAudioCodecDescription*>(codec.get());
      audioCodec->mMaxPlaybackRate = 16000;
      audioCodec->mForceMono = true;
    }
  }
  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  VERIFY_OPUS_MAX_PLAYBACK_RATE(mSendOff, 16000U);
  VERIFY_OPUS_FORCE_MONO(mSendOff, true);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(
      mSendAns, SdpFmtpAttributeList::OpusParameters::kDefaultMaxPlaybackRate);
  VERIFY_OPUS_FORCE_MONO(mSendAns, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvOff, 0U);
  VERIFY_OPUS_FORCE_MONO(mRecvOff, false);
  VERIFY_OPUS_MAX_PLAYBACK_RATE(mRecvAns, 16000U);
  VERIFY_OPUS_FORCE_MONO(mRecvAns, true);
}

TEST_F(JsepTrackTest, RtcpFbWithPayloadTypeAsymmetry) {
  std::vector<std::string> expectedAckFbTypes;
  std::vector<std::string> expectedNackFbTypes{"", "pli"};
  std::vector<std::string> expectedCcmFbTypes{"fir"};
  std::vector<SdpRtcpFbAttributeList::Feedback> expectedOtherFbTypes{
      {"", SdpRtcpFbAttributeList::kRemb, "", ""},
      {"", SdpRtcpFbAttributeList::kTransportCC, "", ""}};

  InitCodecs(CodecOverrides{});

  // On offerer, configure to support remb and transport-cc on video codecs
  for (auto& codec : mOffCodecs) {
    if (codec->Type() == SdpMediaSection::kVideo) {
      auto& videoCodec = static_cast<JsepVideoCodecDescription&>(*codec);
      videoCodec.EnableRemb();
      videoCodec.EnableTransportCC();
    }
  }

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);

  CreateOffer();
  // We do not bother trying to bamboozle the answerer into doing asymmetric
  // payload types, we just use a raw SDP.
  const std::string answer =
      "v=0\r\n"
      "o=- 0 0 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=msid-semantic:WMS *\r\n"
      "m=video 0 UDP/TLS/RTP/SAVPF 136\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sendrecv\r\n"
      "a=fmtp:136 "
      "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode="
      "1\r\n"
      "a=msid:stream_id\r\n"
      "a=rtcp-fb:136 nack\r\n"
      "a=rtcp-fb:136 nack pli\r\n"
      "a=rtcp-fb:136 ccm fir\r\n"
      "a=rtcp-fb:136 goog-remb\r\n"
      "a=rtcp-fb:136 transport-cc\r\n"
      "a=rtpmap:136 H264/90000\r\n"
      "a=ssrc:2025549043 cname:\r\n";

  UniquePtr<SdpParser> parser(new SipccSdpParser);
  mAnswer = std::move(parser->Parse(answer)->Sdp());
  ASSERT_TRUE(mAnswer);

  mRecvOff.RecvTrackSetRemote(*mAnswer, GetAnswer());
  mRecvOff.Negotiate(GetAnswer(), GetAnswer(), GetOffer());
  mSendOff.Negotiate(GetAnswer(), GetAnswer(), GetOffer());

  ASSERT_TRUE(mSendOff.GetNegotiatedDetails());
  ASSERT_TRUE(mRecvOff.GetNegotiatedDetails());

  UniquePtr<JsepVideoCodecDescription> codec;
  ASSERT_TRUE((codec = GetVideoCodec(mSendOff)));
  ASSERT_EQ("136", codec->mDefaultPt)
      << "Offerer should have seen answer asymmetry!";
  ASSERT_TRUE((codec = GetVideoCodec(mRecvOff, 3, 0)));
  ASSERT_EQ("126", codec->mDefaultPt);
  ASSERT_EQ(expectedAckFbTypes, codec->mAckFbTypes);
  ASSERT_EQ(expectedNackFbTypes, codec->mNackFbTypes);
  ASSERT_EQ(expectedCcmFbTypes, codec->mCcmFbTypes);
  ASSERT_EQ(expectedOtherFbTypes, codec->mOtherFbTypes);
}

TEST_F(JsepTrackTest, AudioSdpFmtpLine) {
  mOffCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});
  mAnsCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});
  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);
  OfferAnswer();

  // SanityCheck checks that the sdpFmtpLine for a local codec matches that of
  // the corresponding remote codec.
  UniquePtr<JsepAudioCodecDescription> codec;
  EXPECT_TRUE((codec = GetAudioCodec(mSendOff, 3, 0)));
  EXPECT_EQ("opus", codec->mName);
  EXPECT_EQ("maxplaybackrate=48000;stereo=1;useinbandfec=1",
            codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetAudioCodec(mSendAns, 3, 0)));
  EXPECT_EQ("opus", codec->mName);
  EXPECT_EQ("maxplaybackrate=48000;stereo=1;useinbandfec=1",
            codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetAudioCodec(mSendOff, 3, 1)));
  EXPECT_EQ("G722", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetAudioCodec(mSendAns, 3, 1)));
  EXPECT_EQ("G722", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetAudioCodec(mSendOff, 3, 2)));
  EXPECT_EQ("telephone-event", codec->mName);
  EXPECT_EQ("0-15", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetAudioCodec(mSendAns, 3, 2)));
  EXPECT_EQ("telephone-event", codec->mName);
  EXPECT_EQ("0-15", codec->mSdpFmtpLine.valueOr("nothing"));
}

TEST_F(JsepTrackTest, NonDefaultAudioSdpFmtpLine) {
  mOffCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});
  mAnsCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});

  for (auto& codec : mOffCodecs) {
    if (codec->mName == "opus") {
      auto* audio = static_cast<JsepAudioCodecDescription*>(codec.get());
      audio->mForceMono = true;
      audio->mMaxPlaybackRate = 32000;
    }
  }

  for (auto& codec : mAnsCodecs) {
    if (codec->mName == "opus") {
      auto* audio = static_cast<JsepAudioCodecDescription*>(codec.get());
      audio->mFECEnabled = true;
      audio->mCbrEnabled = true;
      audio->mDTXEnabled = true;
      audio->mFrameSizeMs = 10;
      audio->mMinFrameSizeMs = 5;
      audio->mMaxFrameSizeMs = 20;
    }
  }

  InitTracks(SdpMediaSection::kAudio);
  InitSdp(SdpMediaSection::kAudio);

  {
    // telephone-event doesn't store any params in JsepAudioCodecDescription.
    // Set them directly in the offer sdp instead.
    auto params = MakeUnique<SdpFmtpAttributeList::TelephoneEventParameters>();
    params->dtmfTones = "2-9";
    GetOffer().SetFmtp({"101", *params});
  }

  {
    // telephone-event doesn't store any params in JsepAudioCodecDescription.
    // Set them directly in the answer sdp instead.
    auto params = MakeUnique<SdpFmtpAttributeList::TelephoneEventParameters>();
    params->dtmfTones = "0-3,10";
    GetAnswer().SetFmtp({"101", *params});
  }

  OfferAnswer();

  // SanityCheck checks that the sdpFmtpLine for a local codec matches that of
  // the corresponding remote codec.
  UniquePtr<JsepAudioCodecDescription> codec;
  EXPECT_TRUE((codec = GetAudioCodec(mSendOff, 3, 0)));
  EXPECT_EQ("opus", codec->mName);
  EXPECT_EQ(
      "maxplaybackrate=48000;stereo=1;useinbandfec=1;usedtx=1;ptime=10;"
      "minptime=5;maxptime=20;cbr=1",
      codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetAudioCodec(mSendAns, 3, 0)));
  EXPECT_EQ("opus", codec->mName);
  EXPECT_EQ("maxplaybackrate=32000;stereo=0;useinbandfec=1",
            codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetAudioCodec(mSendOff, 3, 1)));
  EXPECT_EQ("G722", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetAudioCodec(mSendAns, 3, 1)));
  EXPECT_EQ("G722", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetAudioCodec(mSendOff, 3, 2)));
  EXPECT_EQ("telephone-event", codec->mName);
  EXPECT_EQ("0-3,10", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetAudioCodec(mSendAns, 3, 2)));
  EXPECT_EQ("telephone-event", codec->mName);
  EXPECT_EQ("2-9", codec->mSdpFmtpLine.valueOr("nothing"));
}

TEST_F(JsepTrackTest, VideoSdpFmtpLine) {
  mOffCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});
  mAnsCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});
  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // SanityCheck checks that the sdpFmtpLine for a local codec matches that of
  // the corresponding remote codec.
  UniquePtr<JsepVideoCodecDescription> codec;
  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 0)));
  EXPECT_EQ("VP8", codec->mName);
  EXPECT_EQ("max-fs=12288;max-fr=60", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 0)));
  EXPECT_EQ("VP8", codec->mName);
  EXPECT_EQ("max-fs=12288;max-fr=60", codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 1)));
  EXPECT_EQ("H264", codec->mName);
  EXPECT_EQ(
      "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1",
      codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 1)));
  EXPECT_EQ("H264", codec->mName);
  EXPECT_EQ(
      "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1",
      codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 3)));
  EXPECT_EQ("red", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 3)));
  EXPECT_EQ("red", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 4)));
  EXPECT_EQ("ulpfec", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 4)));
  EXPECT_EQ("ulpfec", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
}

TEST_F(JsepTrackTest, NonDefaultVideoSdpFmtpLine) {
  mOffCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});
  mAnsCodecs = MakeCodecs(
      {.addFecCodecs = true, .preferRed = true, .addDtmfCodec = true});

  for (auto& codec : mOffCodecs) {
    if (codec->mName == "VP8" || codec->mName == "H264") {
      auto* video = static_cast<JsepVideoCodecDescription*>(codec.get());
      video->mConstraints.maxFs = 1200;
      if (codec->mName == "VP8") {
        video->mConstraints.maxFps = Some(15);
      } else {
        video->mConstraints.maxDpb = 6400;
        video->mConstraints.maxBr = 1000;
        JsepVideoCodecDescription::SetSaneH264Level(0x1F0,
                                                    &video->mProfileLevelId);
      }
    }
  }

  for (auto& codec : mAnsCodecs) {
    if (codec->mName == "VP8" || codec->mName == "H264") {
      auto* video = static_cast<JsepVideoCodecDescription*>(codec.get());
      video->mConstraints.maxFs = 32400;
      if (codec->mName == "VP8") {
        video->mConstraints.maxFps = Some(60);
      } else {
        video->mConstraints.maxMbps = 1944000;
        video->mConstraints.maxCpb = 800000;
        video->mConstraints.maxDpb = 128000;
        JsepVideoCodecDescription::SetSaneH264Level(0xAB,
                                                    &video->mProfileLevelId);
        video->mPacketizationMode = 1;
      }
    }
  }

  InitTracks(SdpMediaSection::kVideo);
  InitSdp(SdpMediaSection::kVideo);
  OfferAnswer();

  // SanityCheck checks that the sdpFmtpLine for a local codec matches that of
  // the corresponding remote codec.
  UniquePtr<JsepVideoCodecDescription> codec;
  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 0)));
  EXPECT_EQ("VP8", codec->mName);
  EXPECT_EQ("max-fs=32400;max-fr=60", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 0)));
  EXPECT_EQ("VP8", codec->mName);
  EXPECT_EQ("max-fs=1200;max-fr=15", codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 1)));
  EXPECT_EQ("H264", codec->mName);
  EXPECT_EQ(
      "profile-level-id=42f00b;level-asymmetry-allowed=1;packetization-mode=1;"
      "max-mbps=1944000;max-fs=32400;max-cpb=800000;max-dpb=128000",
      codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 1)));
  EXPECT_EQ("H264", codec->mName);
  EXPECT_EQ(
      "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1;"
      "max-fs=1200;max-dpb=6400;max-br=1000",
      codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 3)));
  EXPECT_EQ("red", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 3)));
  EXPECT_EQ("red", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));

  EXPECT_TRUE((codec = GetVideoCodec(mSendOff, 5, 4)));
  EXPECT_EQ("ulpfec", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
  EXPECT_TRUE((codec = GetVideoCodec(mSendAns, 5, 4)));
  EXPECT_EQ("ulpfec", codec->mName);
  EXPECT_EQ("nothing", codec->mSdpFmtpLine.valueOr("nothing"));
}

TEST(JsepTrackRecvPayloadTypesTest, SingleTrackPTsAreUnique)
{
  constexpr auto audio = SdpMediaSection::MediaType::kAudio;

  std::vector<UniquePtr<JsepCodecDescription>> codecs;
  codecs.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("1", "codec1", 48000, 1, true));

  SipccSdp offer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& offer1Msection1 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  SipccSdp answer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& answer1Msection1 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  for (const auto& codec : codecs) {
    codec->mDirection = sdp::kSend;
    offer1Msection1.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection1);
  }

  JsepTrack t1{audio, sdp::Direction::kRecv};
  t1.PopulateCodecs(codecs, false);
  t1.RecvTrackSetLocal(offer1Msection1);
  t1.RecvTrackSetRemote(answer1, answer1Msection1);
  ASSERT_EQ(t1.Negotiate(answer1Msection1, answer1Msection1, offer1Msection1),
            NS_OK);

  std::vector tracks{&t1};
  JsepTrack::SetUniqueReceivePayloadTypes(tracks);
  EXPECT_THAT(t1.GetUniqueReceivePayloadTypes(), UnorderedElementsAre(1));
  EXPECT_THAT(t1.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre());
}

TEST(JsepTrackRecvPayloadTypesTest, DoubleTrackPTsAreUnique)
{
  constexpr auto audio = SdpMediaSection::MediaType::kAudio;

  std::vector<UniquePtr<JsepCodecDescription>> codecs1;
  codecs1.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("1", "codec1", 48000, 1, true));

  std::vector<UniquePtr<JsepCodecDescription>> codecs2;
  codecs2.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("2", "codec1", 48000, 1, true));

  SipccSdp offer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& offer1Msection1 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& offer1Msection2 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  SipccSdp answer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& answer1Msection1 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& answer1Msection2 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  for (const auto& codec : codecs1) {
    codec->mDirection = sdp::kSend;
    offer1Msection1.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection1);
  }

  for (const auto& codec : codecs2) {
    codec->mDirection = sdp::kSend;
    offer1Msection2.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection2);
  }

  JsepTrack t1{audio, sdp::Direction::kRecv};
  t1.PopulateCodecs(codecs1, false);
  t1.RecvTrackSetLocal(offer1Msection1);
  t1.RecvTrackSetRemote(answer1, answer1Msection1);
  ASSERT_EQ(t1.Negotiate(answer1Msection1, answer1Msection1, offer1Msection1),
            NS_OK);

  JsepTrack t2{audio, sdp::Direction::kRecv};
  t2.PopulateCodecs(codecs2, false);
  t2.RecvTrackSetLocal(offer1Msection2);
  t2.RecvTrackSetRemote(answer1, answer1Msection2);
  ASSERT_EQ(t2.Negotiate(answer1Msection2, answer1Msection2, offer1Msection2),
            NS_OK);

  std::vector tracks{&t1, &t2};
  JsepTrack::SetUniqueReceivePayloadTypes(tracks);
  EXPECT_THAT(t1.GetUniqueReceivePayloadTypes(), UnorderedElementsAre(1));
  EXPECT_THAT(t1.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre());
  EXPECT_THAT(t2.GetUniqueReceivePayloadTypes(), UnorderedElementsAre(2));
  EXPECT_THAT(t2.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre());
}

TEST(JsepTrackRecvPayloadTypesTest, DoubleTrackPTsAreDuplicates)
{
  constexpr auto audio = SdpMediaSection::MediaType::kAudio;

  std::vector<UniquePtr<JsepCodecDescription>> codecs1;
  codecs1.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("1", "codec1", 48000, 1, true));

  std::vector<UniquePtr<JsepCodecDescription>> codecs2;
  codecs2.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("1", "codec1", 48000, 1, true));

  SipccSdp offer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& offer1Msection1 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& offer1Msection2 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  SipccSdp answer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& answer1Msection1 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& answer1Msection2 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  for (const auto& codec : codecs1) {
    codec->mDirection = sdp::kSend;
    offer1Msection1.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection1);
  }
  for (const auto& codec : codecs2) {
    codec->mDirection = sdp::kSend;
    offer1Msection2.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection2);
  }

  JsepTrack t1{audio, sdp::Direction::kRecv};
  t1.PopulateCodecs(codecs1, false);
  t1.RecvTrackSetLocal(offer1Msection1);
  t1.RecvTrackSetRemote(answer1, answer1Msection1);
  ASSERT_EQ(t1.Negotiate(answer1Msection1, answer1Msection1, offer1Msection1),
            NS_OK);

  JsepTrack t2{audio, sdp::Direction::kRecv};
  t2.PopulateCodecs(codecs2, false);
  t2.RecvTrackSetLocal(offer1Msection2);
  t2.RecvTrackSetRemote(answer1, answer1Msection2);
  ASSERT_EQ(t2.Negotiate(answer1Msection2, answer1Msection2, offer1Msection2),
            NS_OK);

  std::vector tracks{&t1, &t2};
  JsepTrack::SetUniqueReceivePayloadTypes(tracks);
  EXPECT_THAT(t1.GetUniqueReceivePayloadTypes(), UnorderedElementsAre());
  EXPECT_THAT(t1.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre(1));
  EXPECT_THAT(t2.GetUniqueReceivePayloadTypes(), UnorderedElementsAre());
  EXPECT_THAT(t2.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre(1));
}

TEST(JsepTrackRecvPayloadTypesTest, DoubleTrackPTsOverlap)
{
  constexpr auto audio = SdpMediaSection::MediaType::kAudio;

  std::vector<UniquePtr<JsepCodecDescription>> codecs1;
  codecs1.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("1", "codec1", 48000, 1, true));
  codecs1.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("2", "codec2", 48000, 1, true));

  std::vector<UniquePtr<JsepCodecDescription>> codecs2;
  codecs2.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("1", "codec1", 48000, 1, true));
  codecs2.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("3", "codec2", 48000, 1, true));

  SipccSdp offer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& offer1Msection1 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& offer1Msection2 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  SipccSdp answer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& answer1Msection1 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& answer1Msection2 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  for (const auto& codec : codecs1) {
    codec->mDirection = sdp::kSend;
    offer1Msection1.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection1);
  }

  for (const auto& codec : codecs2) {
    codec->mDirection = sdp::kSend;
    offer1Msection2.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection2);
  }

  JsepTrack t1{audio, sdp::Direction::kRecv};
  t1.PopulateCodecs(codecs1, false);
  t1.RecvTrackSetLocal(offer1Msection1);
  t1.RecvTrackSetRemote(answer1, answer1Msection1);
  ASSERT_EQ(t1.Negotiate(answer1Msection1, answer1Msection1, offer1Msection1),
            NS_OK);

  JsepTrack t2{audio, sdp::Direction::kRecv};
  t2.PopulateCodecs(codecs2, false);
  t2.RecvTrackSetLocal(offer1Msection2);
  t2.RecvTrackSetRemote(answer1, answer1Msection2);
  ASSERT_EQ(t2.Negotiate(answer1Msection2, answer1Msection2, offer1Msection2),
            NS_OK);

  std::vector tracks{&t1, &t2};
  JsepTrack::SetUniqueReceivePayloadTypes(tracks);
  EXPECT_THAT(t1.GetUniqueReceivePayloadTypes(), UnorderedElementsAre(2));
  EXPECT_THAT(t1.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre(1));
  EXPECT_THAT(t2.GetUniqueReceivePayloadTypes(), UnorderedElementsAre(3));
  EXPECT_THAT(t2.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre(1));
}

TEST(JsepTrackRecvPayloadTypesTest, DoubleTrackPTsDuplicateAfterRenegotiation)
{
  constexpr auto audio = SdpMediaSection::MediaType::kAudio;

  std::vector<UniquePtr<JsepCodecDescription>> codecs1;
  codecs1.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("1", "codec1", 48000, 1, true));
  codecs1.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("2", "codec2", 48000, 1, true));

  std::vector<UniquePtr<JsepCodecDescription>> codecs2;
  codecs2.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("3", "codec1", 48000, 1, true));
  codecs2.emplace_back(
      MakeUnique<JsepAudioCodecDescription>("4", "codec2", 48000, 1, true));

  // First negotiation.
  SipccSdp offer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& offer1Msection1 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& offer1Msection2 = offer1.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  SipccSdp answer1(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& answer1Msection1 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& answer1Msection2 = answer1.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  for (const auto& codec : codecs1) {
    codec->mDirection = sdp::kSend;
    offer1Msection1.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection1);
  }

  for (const auto& codec : codecs2) {
    codec->mDirection = sdp::kSend;
    offer1Msection2.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer1Msection2);
  }

  // t1 and t2 use distinct payload types in the first negotiation.
  JsepTrack t1{audio, sdp::Direction::kRecv};
  t1.PopulateCodecs(codecs1, false);
  t1.RecvTrackSetLocal(offer1Msection1);
  t1.RecvTrackSetRemote(answer1, answer1Msection1);
  ASSERT_EQ(t1.Negotiate(answer1Msection1, answer1Msection1, offer1Msection1),
            NS_OK);

  JsepTrack t2{audio, sdp::Direction::kRecv};
  t2.PopulateCodecs(codecs2, false);
  t2.RecvTrackSetLocal(offer1Msection2);
  t2.RecvTrackSetRemote(answer1, answer1Msection2);
  ASSERT_EQ(t2.Negotiate(answer1Msection2, answer1Msection2, offer1Msection2),
            NS_OK);

  std::vector tracks{&t1, &t2};
  JsepTrack::SetUniqueReceivePayloadTypes(tracks);
  EXPECT_THAT(t1.GetUniqueReceivePayloadTypes(), UnorderedElementsAre(1, 2));
  EXPECT_THAT(t1.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre());
  EXPECT_THAT(t2.GetUniqueReceivePayloadTypes(), UnorderedElementsAre(3, 4));
  EXPECT_THAT(t2.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre());

  // Second negotiation.
  SipccSdp offer2(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& offer2Msection1 = offer2.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& offer2Msection2 = offer2.AddMediaSection(
      audio, SdpDirectionAttribute::kRecvonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  SipccSdp answer2(SdpOrigin("", 0, 0, sdp::kIPv4, ""));
  SdpMediaSection& answer2Msection1 = answer2.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");
  SdpMediaSection& answer2Msection2 = answer2.AddMediaSection(
      audio, SdpDirectionAttribute::kSendonly, 0,
      SdpHelper::GetProtocolForMediaType(audio), sdp::kIPv4, "0.0.0.0");

  for (const auto& codec : codecs1) {
    codec->mDirection = sdp::kSend;
    offer2Msection1.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer2Msection1);
  }

  for (const auto& codec : codecs2) {
    codec->mDirection = sdp::kSend;
    offer2Msection2.AddCodec(codec->mDefaultPt, codec->mName, codec->mClock,
                             codec->mChannels);
    auto clone = WrapUnique(codec->Clone());
    clone->mDirection = sdp::kRecv;
    clone->AddToMediaSection(answer2Msection2);
  }

  t1.PopulateCodecs(codecs1, false);
  t1.RecvTrackSetLocal(offer2Msection1);
  t1.RecvTrackSetRemote(answer2, answer2Msection1);
  ASSERT_EQ(t1.Negotiate(answer2Msection1, answer2Msection1, offer2Msection1),
            NS_OK);

  // Change t2 to use the same payload types as t1. Both tracks should now mark
  // all their payload types as duplicates.
  t2.PopulateCodecs(codecs1, false);
  t2.RecvTrackSetLocal(offer2Msection2);
  t2.RecvTrackSetRemote(answer2, answer2Msection2);
  ASSERT_EQ(t2.Negotiate(answer2Msection2, answer2Msection2, offer2Msection2),
            NS_OK);

  std::vector newTracks{&t1, &t2};
  JsepTrack::SetUniqueReceivePayloadTypes(newTracks);
  EXPECT_THAT(t1.GetUniqueReceivePayloadTypes(), UnorderedElementsAre());
  EXPECT_THAT(t1.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre(1, 2));
  EXPECT_THAT(t2.GetUniqueReceivePayloadTypes(), UnorderedElementsAre());
  EXPECT_THAT(t2.GetDuplicateReceivePayloadTypes(), UnorderedElementsAre(1, 2));
}
}  // namespace mozilla
