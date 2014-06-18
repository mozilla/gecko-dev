/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_audio_device.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/media_file/source/media_file_utility.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {
namespace test {

FakeAudioDevice::FakeAudioDevice(Clock* clock, const std::string& filename)
    : audio_callback_(NULL),
      capturing_(false),
      captured_audio_(),
      playout_buffer_(),
      last_playout_ms_(-1),
      clock_(clock),
      tick_(EventWrapper::Create()),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      file_utility_(new ModuleFileUtility(0)),
      input_stream_(FileWrapper::Create()) {
  memset(captured_audio_, 0, sizeof(captured_audio_));
  memset(playout_buffer_, 0, sizeof(playout_buffer_));
  // Open audio input file as read-only and looping.
  EXPECT_EQ(0, input_stream_->OpenFile(filename.c_str(), true, true))
      << filename;
}

FakeAudioDevice::~FakeAudioDevice() {
  Stop();

  if (thread_.get() != NULL)
    thread_->Stop();
}

int32_t FakeAudioDevice::Init() {
  CriticalSectionScoped cs(lock_.get());
  if (file_utility_->InitPCMReading(*input_stream_.get()) != 0)
    return -1;

  if (!tick_->StartTimer(true, 10))
    return -1;
  thread_.reset(ThreadWrapper::CreateThread(
      FakeAudioDevice::Run, this, webrtc::kHighPriority, "FakeAudioDevice"));
  if (thread_.get() == NULL)
    return -1;
  unsigned int thread_id;
  if (!thread_->Start(thread_id)) {
    thread_.reset();
    return -1;
  }
  return 0;
}

int32_t FakeAudioDevice::RegisterAudioCallback(AudioTransport* callback) {
  CriticalSectionScoped cs(lock_.get());
  audio_callback_ = callback;
  return 0;
}

bool FakeAudioDevice::Playing() const {
  CriticalSectionScoped cs(lock_.get());
  return capturing_;
}

int32_t FakeAudioDevice::PlayoutDelay(uint16_t* delay_ms) const {
  *delay_ms = 0;
  return 0;
}

bool FakeAudioDevice::Recording() const {
  CriticalSectionScoped cs(lock_.get());
  return capturing_;
}

bool FakeAudioDevice::Run(void* obj) {
  static_cast<FakeAudioDevice*>(obj)->CaptureAudio();
  return true;
}

void FakeAudioDevice::CaptureAudio() {
  {
    CriticalSectionScoped cs(lock_.get());
    if (capturing_) {
      int bytes_read = file_utility_->ReadPCMData(
          *input_stream_.get(), captured_audio_, kBufferSizeBytes);
      if (bytes_read <= 0)
        return;
      int num_samples = bytes_read / 2;  // 2 bytes per sample.
      uint32_t new_mic_level;
      EXPECT_EQ(0,
                audio_callback_->RecordedDataIsAvailable(captured_audio_,
                                                         num_samples,
                                                         2,
                                                         1,
                                                         kFrequencyHz,
                                                         0,
                                                         0,
                                                         0,
                                                         false,
                                                         new_mic_level));
      uint32_t samples_needed = kFrequencyHz / 100;
      int64_t now_ms = clock_->TimeInMilliseconds();
      uint32_t time_since_last_playout_ms = now_ms - last_playout_ms_;
      if (last_playout_ms_ > 0 && time_since_last_playout_ms > 0)
        samples_needed = std::min(kFrequencyHz / time_since_last_playout_ms,
                                  kBufferSizeBytes / 2);
      uint32_t samples_out = 0;
      EXPECT_EQ(0,
                audio_callback_->NeedMorePlayData(samples_needed,
                                                  2,
                                                  1,
                                                  kFrequencyHz,
                                                  playout_buffer_,
                                                  samples_out));
    }
  }
  tick_->Wait(WEBRTC_EVENT_INFINITE);
}

void FakeAudioDevice::Start() {
  CriticalSectionScoped cs(lock_.get());
  capturing_ = true;
}

void FakeAudioDevice::Stop() {
  CriticalSectionScoped cs(lock_.get());
  capturing_ = false;
}
}  // namespace test
}  // namespace webrtc
