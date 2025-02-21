/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/codec_vendor.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "media/base/codec.h"
#include "media/base/codec_list.h"
#include "media/base/media_constants.h"
#include "media/base/test_utils.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace cricket {
namespace {

Codec CreateRedAudioCodec(absl::string_view encoding_id) {
  Codec red = CreateAudioCodec(63, "red", 48000, 2);
  red.SetParam(kCodecParamNotInNameValueFormat,
               std::string(encoding_id) + '/' + std::string(encoding_id));
  return red;
}

const Codec kAudioCodecs1[] = {CreateAudioCodec(111, "opus", 48000, 2),
                               CreateRedAudioCodec("111"),
                               CreateAudioCodec(102, "iLBC", 8000, 1),
                               CreateAudioCodec(0, "PCMU", 8000, 1),
                               CreateAudioCodec(8, "PCMA", 8000, 1),
                               CreateAudioCodec(107, "CN", 48000, 1)};

const Codec kAudioCodecs2[] = {
    CreateAudioCodec(126, "foo", 16000, 1),
    CreateAudioCodec(0, "PCMU", 8000, 1),
    CreateAudioCodec(127, "iLBC", 8000, 1),
};

const Codec kAudioCodecsAnswer[] = {
    CreateAudioCodec(102, "iLBC", 8000, 1),
    CreateAudioCodec(0, "PCMU", 8000, 1),
};

TEST(CodecVendorTest, TestSetAudioCodecs) {
  CodecVendor codec_vendor(nullptr, false);
  std::vector<Codec> send_codecs = MAKE_VECTOR(kAudioCodecs1);
  std::vector<Codec> recv_codecs = MAKE_VECTOR(kAudioCodecs2);

  // The merged list of codecs should contain any send codecs that are also
  // nominally in the receive codecs list. Payload types should be picked from
  // the send codecs and a number-of-channels of 0 and 1 should be equivalent
  // (set to 1). This equals what happens when the send codecs are used in an
  // offer and the receive codecs are used in the following answer.
  const std::vector<Codec> sendrecv_codecs = MAKE_VECTOR(kAudioCodecsAnswer);
  const std::vector<Codec> no_codecs;

  RTC_CHECK_EQ(send_codecs[2].name, "iLBC")
      << "Please don't change shared test data!";
  RTC_CHECK_EQ(recv_codecs[2].name, "iLBC")
      << "Please don't change shared test data!";
  // Alter iLBC send codec to have zero channels, to test that that is handled
  // properly.
  send_codecs[2].channels = 0;

  // Alter iLBC receive codec to be lowercase, to test that case conversions
  // are handled properly.
  recv_codecs[2].name = "ilbc";

  // Test proper merge
  codec_vendor.set_audio_codecs(send_codecs, recv_codecs);
  EXPECT_EQ(send_codecs, codec_vendor.audio_send_codecs().codecs());
  EXPECT_EQ(recv_codecs, codec_vendor.audio_recv_codecs().codecs());
  EXPECT_EQ(sendrecv_codecs, codec_vendor.audio_sendrecv_codecs().codecs());

  // Test empty send codecs list
  codec_vendor.set_audio_codecs(no_codecs, recv_codecs);
  EXPECT_EQ(no_codecs, codec_vendor.audio_send_codecs().codecs());
  EXPECT_EQ(recv_codecs, codec_vendor.audio_recv_codecs().codecs());
  EXPECT_EQ(no_codecs, codec_vendor.audio_sendrecv_codecs().codecs());

  // Test empty recv codecs list
  codec_vendor.set_audio_codecs(send_codecs, no_codecs);
  EXPECT_EQ(send_codecs, codec_vendor.audio_send_codecs().codecs());
  EXPECT_EQ(no_codecs, codec_vendor.audio_recv_codecs().codecs());
  EXPECT_EQ(no_codecs, codec_vendor.audio_sendrecv_codecs().codecs());

  // Test all empty codec lists
  codec_vendor.set_audio_codecs(no_codecs, no_codecs);
  EXPECT_EQ(no_codecs, codec_vendor.audio_send_codecs().codecs());
  EXPECT_EQ(no_codecs, codec_vendor.audio_recv_codecs().codecs());
  EXPECT_EQ(no_codecs, codec_vendor.audio_sendrecv_codecs().codecs());
}

}  // namespace
}  // namespace cricket
