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

#include "api/audio_codecs/audio_format.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "test/gtest.h"

namespace webrtc {

using cricket::Codec;
using cricket::CreateAudioCodec;
using cricket::CreateVideoCodec;
using cricket::kH264CodecName;
using cricket::kH264FmtpPacketizationMode;

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

}  // namespace webrtc
