/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_CODEC_LIST_H_
#define MEDIA_BASE_CODEC_LIST_H_

#include <cstddef>
#include <vector>

#include "media/base/codec.h"

namespace cricket {

class CodecList {
 public:
  using iterator = std::vector<Codec>::iterator;
  using const_iterator = std::vector<Codec>::const_iterator;
  using value_type = Codec;

  CodecList() {}
  explicit CodecList(const std::vector<Codec>& codecs) {
    codecs_ = codecs;
    CheckConsistency();
  }
  // Vector-compatible API to access the codecs.
  iterator begin() { return codecs_.begin(); }
  iterator end() { return codecs_.end(); }
  const_iterator begin() const { return codecs_.begin(); }
  const_iterator end() const { return codecs_.end(); }
  const Codec& operator[](size_t i) const { return codecs_[i]; }
  Codec& operator[](size_t i) { return codecs_[i]; }
  void push_back(const Codec& codec) {
    codecs_.push_back(codec);
    CheckConsistency();
  }
  bool empty() const { return codecs_.empty(); }
  void clear() { codecs_.clear(); }
  size_t size() const { return codecs_.size(); }
  // Access to the whole codec list
  const std::vector<Codec>& codecs() const { return codecs_; }
  std::vector<Codec>& writable_codecs() { return codecs_; }
  // Verify consistency of the codec list.
  // Examples: checking that all RTX codecs have APT pointing
  // to a codec in the list.
  // The function will CHECK or DCHECK on inconsistencies.
  void CheckConsistency();

 private:
  std::vector<Codec> codecs_;
};

}  // namespace cricket

#endif  // MEDIA_BASE_CODEC_LIST_H_
