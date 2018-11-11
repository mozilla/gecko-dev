/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/agc/circular_buffer.h"

#include <assert.h>
#include <stdlib.h>

namespace webrtc {

AgcCircularBuffer::AgcCircularBuffer(int buffer_size)
    : buffer_(new double[buffer_size]),
      is_full_(false),
      index_(0),
      buffer_size_(buffer_size),
      sum_(0) {}

AgcCircularBuffer::~AgcCircularBuffer() {}

void AgcCircularBuffer::Reset() {
  is_full_ = false;
  index_ = 0;
  sum_ = 0;
}

AgcCircularBuffer* AgcCircularBuffer::Create(int buffer_size) {
  if (buffer_size <= 0)
    return NULL;
  return new AgcCircularBuffer(buffer_size);
}

double AgcCircularBuffer::Oldest() const {
  if (!is_full_)
    return buffer_[0];
  else
    return buffer_[index_];
}

double AgcCircularBuffer::Mean() {
  double m;
  if (is_full_) {
    m = sum_ / buffer_size_;
  } else {
    if (index_ > 0)
      m = sum_ / index_;
    else
      m = 0;
  }
  return m;
}

void AgcCircularBuffer::Insert(double value) {
  if (is_full_) {
    sum_ -= buffer_[index_];
  }
  sum_ += value;
  buffer_[index_] = value;
  index_++;
  if (index_ >= buffer_size_) {
    is_full_ = true;
    index_ = 0;
  }
}
int AgcCircularBuffer::BufferLevel() {
  if (is_full_)
    return buffer_size_;
  return index_;
}

int AgcCircularBuffer::Get(int index, double* value) const {
  int err = ConvertToLinearIndex(&index);
  if (err < 0)
    return -1;
  *value = buffer_[index];
  return 0;
}

int AgcCircularBuffer::Set(int index, double value) {
  int err = ConvertToLinearIndex(&index);
  if (err < 0)
    return -1;

  sum_ -= buffer_[index];
  buffer_[index] = value;
  sum_ += value;
  return 0;
}

int AgcCircularBuffer::ConvertToLinearIndex(int* index) const {
  if (*index < 0 || *index >= buffer_size_)
    return -1;

  if (!is_full_ && *index >= index_)
    return -1;

  *index = index_ - 1 - *index;
  if (*index < 0)
    *index += buffer_size_;
  return 0;
}

int AgcCircularBuffer::RemoveTransient(int width_threshold,
                                       double val_threshold) {
  if (!is_full_ && index_ < width_threshold + 2)
    return 0;

  int index_1 = 0;
  int index_2 = width_threshold + 1;
  double v = 0;
  if (Get(index_1, &v) < 0)
    return -1;
  if (v < val_threshold) {
    Set(index_1, 0);
    int index;
    for (index = index_2; index > index_1; index--) {
      if (Get(index, &v) < 0)
        return -1;
      if (v < val_threshold)
        break;
    }
    for (; index > index_1; index--) {
      if (Set(index, 0.0) < 0)
        return -1;
    }
  }
  return 0;
}

}  // namespace webrtc
