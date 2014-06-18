/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq4/decision_logic.h"

#include <algorithm>

#include "webrtc/modules/audio_coding/neteq4/buffer_level_filter.h"
#include "webrtc/modules/audio_coding/neteq4/decision_logic_fax.h"
#include "webrtc/modules/audio_coding/neteq4/decision_logic_normal.h"
#include "webrtc/modules/audio_coding/neteq4/delay_manager.h"
#include "webrtc/modules/audio_coding/neteq4/expand.h"
#include "webrtc/modules/audio_coding/neteq4/packet_buffer.h"
#include "webrtc/modules/audio_coding/neteq4/sync_buffer.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

DecisionLogic* DecisionLogic::Create(int fs_hz,
                                     int output_size_samples,
                                     NetEqPlayoutMode playout_mode,
                                     DecoderDatabase* decoder_database,
                                     const PacketBuffer& packet_buffer,
                                     DelayManager* delay_manager,
                                     BufferLevelFilter* buffer_level_filter) {
  switch (playout_mode) {
    case kPlayoutOn:
    case kPlayoutStreaming:
      return new DecisionLogicNormal(fs_hz,
                                     output_size_samples,
                                     playout_mode,
                                     decoder_database,
                                     packet_buffer,
                                     delay_manager,
                                     buffer_level_filter);
    case kPlayoutFax:
    case kPlayoutOff:
      return new DecisionLogicFax(fs_hz,
                                  output_size_samples,
                                  playout_mode,
                                  decoder_database,
                                  packet_buffer,
                                  delay_manager,
                                  buffer_level_filter);
  }
  // This line cannot be reached, but must be here to avoid compiler errors.
  assert(false);
  return NULL;
}

DecisionLogic::DecisionLogic(int fs_hz,
                             int output_size_samples,
                             NetEqPlayoutMode playout_mode,
                             DecoderDatabase* decoder_database,
                             const PacketBuffer& packet_buffer,
                             DelayManager* delay_manager,
                             BufferLevelFilter* buffer_level_filter)
    : decoder_database_(decoder_database),
      packet_buffer_(packet_buffer),
      delay_manager_(delay_manager),
      buffer_level_filter_(buffer_level_filter),
      cng_state_(kCngOff),
      generated_noise_samples_(0),
      packet_length_samples_(0),
      sample_memory_(0),
      prev_time_scale_(false),
      timescale_hold_off_(kMinTimescaleInterval),
      num_consecutive_expands_(0),
      playout_mode_(playout_mode) {
  delay_manager_->set_streaming_mode(playout_mode_ == kPlayoutStreaming);
  SetSampleRate(fs_hz, output_size_samples);
}

void DecisionLogic::Reset() {
  cng_state_ = kCngOff;
  generated_noise_samples_ = 0;
  packet_length_samples_ = 0;
  sample_memory_ = 0;
  prev_time_scale_ = false;
  timescale_hold_off_ = 0;
  num_consecutive_expands_ = 0;
}

void DecisionLogic::SoftReset() {
  packet_length_samples_ = 0;
  sample_memory_ = 0;
  prev_time_scale_ = false;
  timescale_hold_off_ = kMinTimescaleInterval;
}

void DecisionLogic::SetSampleRate(int fs_hz, int output_size_samples) {
  // TODO(hlundin): Change to an enumerator and skip assert.
  assert(fs_hz == 8000 || fs_hz == 16000 || fs_hz ==  32000 || fs_hz == 48000);
  fs_mult_ = fs_hz / 8000;
  output_size_samples_ = output_size_samples;
}

Operations DecisionLogic::GetDecision(const SyncBuffer& sync_buffer,
                                      const Expand& expand,
                                      int decoder_frame_length,
                                      const RTPHeader* packet_header,
                                      Modes prev_mode,
                                      bool play_dtmf, bool* reset_decoder) {
  if (prev_mode == kModeRfc3389Cng ||
      prev_mode == kModeCodecInternalCng ||
      prev_mode == kModeExpand) {
    // If last mode was CNG (or Expand, since this could be covering up for
    // a lost CNG packet), increase the |generated_noise_samples_| counter.
    generated_noise_samples_ += output_size_samples_;
    // Remember that CNG is on. This is needed if comfort noise is interrupted
    // by DTMF.
    if (prev_mode == kModeRfc3389Cng) {
      cng_state_ = kCngRfc3389On;
    } else if (prev_mode == kModeCodecInternalCng) {
      cng_state_ = kCngInternalOn;
    }
  }

  const int samples_left = static_cast<int>(
      sync_buffer.FutureLength() - expand.overlap_length());
  const int cur_size_samples =
      samples_left + packet_buffer_.NumSamplesInBuffer(decoder_database_,
                                                       decoder_frame_length);
  LOG(LS_VERBOSE) << "Buffers: " << packet_buffer_.NumPacketsInBuffer() <<
      " packets * " << decoder_frame_length << " samples/packet + " <<
      samples_left << " samples in sync buffer = " << cur_size_samples;

  prev_time_scale_ = prev_time_scale_ &&
      (prev_mode == kModeAccelerateSuccess ||
          prev_mode == kModeAccelerateLowEnergy ||
          prev_mode == kModePreemptiveExpandSuccess ||
          prev_mode == kModePreemptiveExpandLowEnergy);

  FilterBufferLevel(cur_size_samples, prev_mode);

  return GetDecisionSpecialized(sync_buffer, expand, decoder_frame_length,
                                packet_header, prev_mode, play_dtmf,
                                reset_decoder);
}

void DecisionLogic::ExpandDecision(bool is_expand_decision) {
  if (is_expand_decision) {
    num_consecutive_expands_++;
  } else {
    num_consecutive_expands_ = 0;
  }
}

void DecisionLogic::FilterBufferLevel(int buffer_size_samples,
                                      Modes prev_mode) {
  const int elapsed_time_ms = output_size_samples_ / (8 * fs_mult_);
  delay_manager_->UpdateCounters(elapsed_time_ms);

  // Do not update buffer history if currently playing CNG since it will bias
  // the filtered buffer level.
  if ((prev_mode != kModeRfc3389Cng) && (prev_mode != kModeCodecInternalCng)) {
    buffer_level_filter_->SetTargetBufferLevel(
        delay_manager_->base_target_level());

    int buffer_size_packets = 0;
    if (packet_length_samples_ > 0) {
      // Calculate size in packets.
      buffer_size_packets = buffer_size_samples / packet_length_samples_;
    }
    int sample_memory_local = 0;
    if (prev_time_scale_) {
      sample_memory_local = sample_memory_;
      timescale_hold_off_ = kMinTimescaleInterval;
    }
    buffer_level_filter_->Update(buffer_size_packets, sample_memory_local,
                                 packet_length_samples_);
    prev_time_scale_ = false;
  }

  timescale_hold_off_ = std::max(timescale_hold_off_ - 1, 0);
}

}  // namespace webrtc
