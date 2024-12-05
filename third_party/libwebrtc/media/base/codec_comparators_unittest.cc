/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "media/base/codec_comparators.h"

#include <string>

#include "api/audio_codecs/audio_format.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/vp9_profile.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using cricket::Codec;
using cricket::CreateAudioCodec;
using cricket::CreateVideoCodec;
using cricket::kH264CodecName;
using cricket::kH264FmtpPacketizationMode;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

TEST(CodecComparatorsTest, CodecMatchesItself) {
  Codec codec = cricket::CreateVideoCodec("custom");
  EXPECT_TRUE(MatchesWithCodecRules(codec, codec));
}

TEST(CodecComparatorsTest, MismatchedBasicParameters) {
  Codec codec = CreateAudioCodec(SdpAudioFormat("opus", 48000, 2));
  Codec nonmatch_codec = codec;
  nonmatch_codec.name = "g711";
  EXPECT_FALSE(MatchesWithCodecRules(nonmatch_codec, codec));
  nonmatch_codec = codec;
  nonmatch_codec.clockrate = 8000;
  EXPECT_FALSE(MatchesWithCodecRules(nonmatch_codec, codec));
  nonmatch_codec = codec;
  nonmatch_codec.channels = 1;
  EXPECT_FALSE(MatchesWithCodecRules(nonmatch_codec, codec));
}

TEST(CodecComparatorsTest, H264PacketizationModeMismatch) {
  Codec pt_mode_1 = CreateVideoCodec(kH264CodecName);
  Codec pt_mode_0 = pt_mode_1;
  pt_mode_0.SetParam(kH264FmtpPacketizationMode, "0");
  EXPECT_FALSE(MatchesWithCodecRules(pt_mode_1, pt_mode_0));
  EXPECT_FALSE(MatchesWithCodecRules(pt_mode_0, pt_mode_1));
  Codec no_pt_mode = pt_mode_1;
  no_pt_mode.RemoveParam(kH264FmtpPacketizationMode);
  EXPECT_TRUE(MatchesWithCodecRules(pt_mode_0, no_pt_mode));
  EXPECT_TRUE(MatchesWithCodecRules(no_pt_mode, pt_mode_0));
  EXPECT_FALSE(MatchesWithCodecRules(no_pt_mode, pt_mode_1));
}

TEST(CodecComparatorsTest, AudioParametersIgnored) {
  // Currently, all parameters on audio codecs are ignored for matching.
  Codec basic_opus = CreateAudioCodec(SdpAudioFormat("opus", 48000, 2));
  Codec opus_with_parameters = basic_opus;
  opus_with_parameters.SetParam("stereo", "0");
  EXPECT_TRUE(MatchesWithCodecRules(basic_opus, opus_with_parameters));
  EXPECT_TRUE(MatchesWithCodecRules(opus_with_parameters, basic_opus));
  opus_with_parameters.SetParam("nonsense", "stuff");
  EXPECT_TRUE(MatchesWithCodecRules(basic_opus, opus_with_parameters));
  EXPECT_TRUE(MatchesWithCodecRules(opus_with_parameters, basic_opus));
}

TEST(CodecComparatorsTest, StaticPayloadTypesIgnoreName) {
  // This is the IANA registered format for PT 8
  Codec codec_1 = CreateAudioCodec(8, "pcma", 8000, 1);
  Codec codec_2 = CreateAudioCodec(8, "nonsense", 8000, 1);
  EXPECT_TRUE(MatchesWithCodecRules(codec_1, codec_2));
}

struct TestParams {
  std::string name;
  SdpVideoFormat codec1;
  SdpVideoFormat codec2;
  bool expected_result;
};

using IsSameRtpCodecTest = TestWithParam<TestParams>;

TEST_P(IsSameRtpCodecTest, IsSameRtpCodec) {
  TestParams param = GetParam();
  Codec codec1 = cricket::CreateVideoCodec(param.codec1);
  Codec codec2 = cricket::CreateVideoCodec(param.codec2);

  EXPECT_EQ(IsSameRtpCodec(codec1, codec2.ToCodecParameters()),
            param.expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    CodecTest,
    IsSameRtpCodecTest,
    ValuesIn<TestParams>({
        {.name = "CodecWithDifferentName",
         .codec1 = {"VP9", {}},
         .codec2 = {"VP8", {}},
         .expected_result = false},
        {.name = "Vp8WithoutParameters",
         .codec1 = {"vp8", {}},
         .codec2 = {"VP8", {}},
         .expected_result = true},
        {.name = "Vp8WithSameParameters",
         .codec1 = {"VP8", {{"x", "1"}}},
         .codec2 = {"VP8", {{"x", "1"}}},
         .expected_result = true},
        {.name = "Vp8WithDifferentParameters",
         .codec1 = {"VP8", {}},
         .codec2 = {"VP8", {{"x", "1"}}},
         .expected_result = false},
        {.name = "Av1WithoutParameters",
         .codec1 = {"AV1", {}},
         .codec2 = {"AV1", {}},
         .expected_result = true},
        {.name = "Av1WithSameProfile",
         .codec1 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .codec2 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .expected_result = true},
        {.name = "Av1WithoutParametersTreatedAsProfile0",
         .codec1 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .codec2 = {"AV1", {}},
         .expected_result = true},
        {.name = "Av1WithoutProfileTreatedAsProfile0",
         .codec1 = {"AV1", {{cricket::kAv1FmtpProfile, "0"}, {"x", "1"}}},
         .codec2 = {"AV1", {{"x", "1"}}},
         .expected_result = true},
        {.name = "Av1WithDifferentProfile",
         .codec1 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .codec2 = {"AV1", SdpVideoFormat::AV1Profile1().parameters},
         .expected_result = false},
        {.name = "Av1WithDifferentParameters",
         .codec1 = {"AV1", {{cricket::kAv1FmtpProfile, "0"}, {"x", "1"}}},
         .codec2 = {"AV1", {{cricket::kAv1FmtpProfile, "0"}, {"x", "2"}}},
         .expected_result = false},
        {.name = "Vp9WithSameProfile",
         .codec1 = {"VP9", SdpVideoFormat::VP9Profile0().parameters},
         .codec2 = {"VP9", SdpVideoFormat::VP9Profile0().parameters},
         .expected_result = true},
        {.name = "Vp9WithoutProfileTreatedAsProfile0",
         .codec1 = {"VP9", {{kVP9FmtpProfileId, "0"}, {"x", "1"}}},
         .codec2 = {"VP9", {{"x", "1"}}},
         .expected_result = true},
        {.name = "Vp9WithDifferentProfile",
         .codec1 = {"VP9", SdpVideoFormat::VP9Profile0().parameters},
         .codec2 = {"VP9", SdpVideoFormat::VP9Profile1().parameters},
         .expected_result = false},
        {.name = "H264WithSamePacketizationMode",
         .codec1 = {"H264", {{kH264FmtpPacketizationMode, "0"}}},
         .codec2 = {"H264", {{kH264FmtpPacketizationMode, "0"}}},
         .expected_result = true},
        {.name = "H264WithoutPacketizationModeTreatedAsMode0",
         .codec1 = {"H264", {{kH264FmtpPacketizationMode, "0"}, {"x", "1"}}},
         .codec2 = {"H264", {{"x", "1"}}},
         .expected_result = true},
        {.name = "H264WithDifferentPacketizationMode",
         .codec1 = {"H264", {{kH264FmtpPacketizationMode, "0"}}},
         .codec2 = {"H264", {{kH264FmtpPacketizationMode, "1"}}},
         .expected_result = false},
#ifdef RTC_ENABLE_H265
        {.name = "H265WithSameProfile",
         .codec1 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .codec2 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .expected_result = true},
        {.name = "H265WithoutParametersTreatedAsDefault",
         .codec1 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .codec2 = {"H265", {}},
         .expected_result = true},
        {.name = "H265WithDifferentProfile",
         .codec1 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .codec2 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "1"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .expected_result = false},
#endif
    }),
    [](const testing::TestParamInfo<IsSameRtpCodecTest::ParamType>& info) {
      return info.param.name;
    });

}  // namespace webrtc
