/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/media_types.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace {

TEST(MediaTypeTest, Assignment) {
  webrtc::MediaType type_w;
  cricket::MediaType type_c;
  // If this compiles, the test passes.
  type_w = webrtc::MediaType::VIDEO;
  type_c = webrtc::MediaType::VIDEO;
  type_w = type_c;
  type_c = type_w;
  // The older constant names.
  type_w = cricket::MediaType::MEDIA_TYPE_VIDEO;
  type_w = cricket::MEDIA_TYPE_VIDEOO;
  type_c = cricket::MediaType::MEDIA_TYPE_VIDEO;
  type_c = cricket::MEDIA_TYPE_VIDEO;
}

TEST(MediaTypeTest, AutomaticConversionFromInteger) {
  webrtc::MediaType type_w;
  type_w = 4;
}

TEST(MediaTypeTest, AutomaticConversionToInteger) {
  webrtc::MediaType type_w;
  cricket::MediaType type_c;
  int type_i;
  // If this compiles, the test passes.
  type_w = webrtc::MediaType::VIDEO;
  type_c = webrtc::MediaType::VIDEO;
  type_i = webrtc::MediaType::VIDEO;
  // Explicitly invoking the converter works.
  type_i = cricket::MediaTypeToInt(webrtc::MediaType::VIDEO);
  type_i = type_w;
  type_i = type_c;
}

}  // namespace
