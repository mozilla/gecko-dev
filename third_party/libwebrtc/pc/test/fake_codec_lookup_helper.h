/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKE_CODEC_LOOKUP_HELPER_H_
#define PC_TEST_FAKE_CODEC_LOOKUP_HELPER_H_

#include <memory>
#include <string>

#include "call/payload_type.h"
#include "pc/codec_vendor.h"
#include "pc/connection_context.h"
#include "rtc_base/checks.h"

namespace webrtc {

class FakeCodecLookupHelper : public cricket::CodecLookupHelper {
 public:
  explicit FakeCodecLookupHelper(ConnectionContext* context)
      : context_(context),
        codec_vendor_(std::make_unique<cricket::CodecVendor>(
            context->media_engine(),
            context->use_rtx(),
            context->env().field_trials())) {}
  webrtc::PayloadTypeSuggester* PayloadTypeSuggester() override {
    // Not used in this test.
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  cricket::CodecVendor* CodecVendor(const std::string& mid) override {
    return codec_vendor_.get();
  }
  // Recreate the codec vendor.
  // Used by tests that manipulate the factory's codecs and expect the
  // result to show up in the codec vendor's output.
  void Reset() {
    codec_vendor_ = std::make_unique<cricket::CodecVendor>(
        context_->media_engine(), context_->use_rtx(),
        context_->env().field_trials());
  }

 private:
  ConnectionContext* context_;
  std::unique_ptr<cricket::CodecVendor> codec_vendor_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKE_CODEC_LOOKUP_HELPER_H_
