/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/payload_type_picker.h"

#include "media/base/codec.h"
#include "test/gtest.h"

namespace webrtc {

TEST(PayloadTypePicker, PayloadTypeAssignmentWorks) {
  // Note: This behavior is due to be deprecated and removed.
  PayloadType pt_a(1);
  PayloadType pt_b = 1;  // Implicit conversion
  EXPECT_EQ(pt_a, pt_b);
  int pt_as_int = pt_a;  // Implicit conversion
  EXPECT_EQ(1, pt_as_int);
}

TEST(PayloadTypePicker, InstantiateTypes) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
}

TEST(PayloadTypePicker, StoreAndRecall) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  const PayloadType not_a_payload_type(44);
  cricket::Codec a_codec = cricket::CreateVideoCodec(0, "vp8");
  auto error = recorder.AddMapping(a_payload_type, a_codec);
  ASSERT_TRUE(error.ok());
  auto result = recorder.LookupCodec(a_payload_type);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), a_codec);
  auto result_pt = recorder.LookupPayloadType(a_codec);
  ASSERT_TRUE(result_pt.ok());
  EXPECT_EQ(result_pt.value(), a_payload_type);
  EXPECT_FALSE(recorder.LookupCodec(not_a_payload_type).ok());
}

TEST(PayloadTypePicker, RollbackAndCommit) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  const PayloadType b_payload_type(124);
  const PayloadType not_a_payload_type(44);

  cricket::Codec a_codec = cricket::CreateVideoCodec(0, "vp8");

  cricket::Codec b_codec = cricket::CreateVideoCodec(0, "vp9");
  auto error = recorder.AddMapping(a_payload_type, a_codec);
  ASSERT_TRUE(error.ok());
  recorder.Checkpoint();
  ASSERT_TRUE(recorder.AddMapping(b_payload_type, b_codec).ok());
  {
    auto result = recorder.LookupCodec(a_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), a_codec);
  }
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), b_codec);
  }
  recorder.Rollback();
  {
    auto result = recorder.LookupCodec(a_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), a_codec);
  }
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_FALSE(result.ok());
  }
  ASSERT_TRUE(recorder.AddMapping(b_payload_type, b_codec).ok());
  // Rollback after a new checkpoint has no effect.
  recorder.Checkpoint();
  recorder.Rollback();
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), b_codec);
  }
}

}  // namespace webrtc
