/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/gain_control_impl.h"

#include <assert.h>

#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/agc/legacy/gain_control.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

typedef void Handle;

namespace {
int16_t MapSetting(GainControl::Mode mode) {
  switch (mode) {
    case GainControl::kAdaptiveAnalog:
      return kAgcModeAdaptiveAnalog;
    case GainControl::kAdaptiveDigital:
      return kAgcModeAdaptiveDigital;
    case GainControl::kFixedDigital:
      return kAgcModeFixedDigital;
  }
  assert(false);
  return -1;
}
}  // namespace

GainControlImpl::GainControlImpl(const AudioProcessing* apm,
                                 CriticalSectionWrapper* crit)
  : ProcessingComponent(),
    apm_(apm),
    crit_(crit),
    mode_(kAdaptiveAnalog),
    minimum_capture_level_(0),
    maximum_capture_level_(255),
    limiter_enabled_(true),
    target_level_dbfs_(3),
    compression_gain_db_(9),
    analog_capture_level_(0),
    was_analog_level_set_(false),
    stream_is_saturated_(false) {}

GainControlImpl::~GainControlImpl() {}

int GainControlImpl::ProcessRenderAudio(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  assert(audio->num_frames_per_band() <= 160);

  for (int i = 0; i < num_handles(); i++) {
    Handle* my_handle = static_cast<Handle*>(handle(i));
    int err = WebRtcAgc_AddFarend(
        my_handle,
        audio->mixed_low_pass_data(),
        static_cast<int16_t>(audio->num_frames_per_band()));

    if (err != apm_->kNoError) {
      return GetHandleError(my_handle);
    }
  }

  return apm_->kNoError;
}

int GainControlImpl::AnalyzeCaptureAudio(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == num_handles());

  int err = apm_->kNoError;

  if (mode_ == kAdaptiveAnalog) {
    capture_levels_.assign(num_handles(), analog_capture_level_);
    for (int i = 0; i < num_handles(); i++) {
      Handle* my_handle = static_cast<Handle*>(handle(i));
      err = WebRtcAgc_AddMic(
          my_handle,
          audio->split_bands(i),
          audio->num_bands(),
          static_cast<int16_t>(audio->num_frames_per_band()));

      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);
      }
    }
  } else if (mode_ == kAdaptiveDigital) {

    for (int i = 0; i < num_handles(); i++) {
      Handle* my_handle = static_cast<Handle*>(handle(i));
      int32_t capture_level_out = 0;

      err = WebRtcAgc_VirtualMic(
          my_handle,
          audio->split_bands(i),
          audio->num_bands(),
          static_cast<int16_t>(audio->num_frames_per_band()),
          analog_capture_level_,
          &capture_level_out);

      capture_levels_[i] = capture_level_out;

      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);
      }

    }
  }

  return apm_->kNoError;
}

int GainControlImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  if (mode_ == kAdaptiveAnalog && !was_analog_level_set_) {
    return apm_->kStreamParameterNotSetError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == num_handles());

  stream_is_saturated_ = false;
  for (int i = 0; i < num_handles(); i++) {
    Handle* my_handle = static_cast<Handle*>(handle(i));
    int32_t capture_level_out = 0;
    uint8_t saturation_warning = 0;

    int err = WebRtcAgc_Process(
        my_handle,
        audio->split_bands_const(i),
        audio->num_bands(),
        static_cast<int16_t>(audio->num_frames_per_band()),
        audio->split_bands(i),
        capture_levels_[i],
        &capture_level_out,
        apm_->echo_cancellation()->stream_has_echo(),
        &saturation_warning);

    if (err != apm_->kNoError) {
      return GetHandleError(my_handle);
    }

    capture_levels_[i] = capture_level_out;
    if (saturation_warning == 1) {
      stream_is_saturated_ = true;
    }
  }

  if (mode_ == kAdaptiveAnalog) {
    // Take the analog level to be the average across the handles.
    analog_capture_level_ = 0;
    for (int i = 0; i < num_handles(); i++) {
      analog_capture_level_ += capture_levels_[i];
    }

    analog_capture_level_ /= num_handles();
  }

  was_analog_level_set_ = false;
  return apm_->kNoError;
}

// TODO(ajm): ensure this is called under kAdaptiveAnalog.
int GainControlImpl::set_stream_analog_level(int level) {
  was_analog_level_set_ = true;
  if (level < minimum_capture_level_ || level > maximum_capture_level_) {
    return apm_->kBadParameterError;
  }
  analog_capture_level_ = level;

  return apm_->kNoError;
}

int GainControlImpl::stream_analog_level() {
  // TODO(ajm): enable this assertion?
  //assert(mode_ == kAdaptiveAnalog);

  return analog_capture_level_;
}

int GainControlImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  return EnableComponent(enable);
}

bool GainControlImpl::is_enabled() const {
  return is_component_enabled();
}

int GainControlImpl::set_mode(Mode mode) {
  CriticalSectionScoped crit_scoped(crit_);
  if (MapSetting(mode) == -1) {
    return apm_->kBadParameterError;
  }

  mode_ = mode;
  return Initialize();
}

GainControl::Mode GainControlImpl::mode() const {
  return mode_;
}

int GainControlImpl::set_analog_level_limits(int minimum,
                                             int maximum) {
  CriticalSectionScoped crit_scoped(crit_);
  if (minimum < 0) {
    return apm_->kBadParameterError;
  }

  if (maximum > 65535) {
    return apm_->kBadParameterError;
  }

  if (maximum < minimum) {
    return apm_->kBadParameterError;
  }

  minimum_capture_level_ = minimum;
  maximum_capture_level_ = maximum;

  return Initialize();
}

int GainControlImpl::analog_level_minimum() const {
  return minimum_capture_level_;
}

int GainControlImpl::analog_level_maximum() const {
  return maximum_capture_level_;
}

bool GainControlImpl::stream_is_saturated() const {
  return stream_is_saturated_;
}

int GainControlImpl::set_target_level_dbfs(int level) {
  CriticalSectionScoped crit_scoped(crit_);
  if (level > 31 || level < 0) {
    return apm_->kBadParameterError;
  }

  target_level_dbfs_ = level;
  return Configure();
}

int GainControlImpl::target_level_dbfs() const {
  return target_level_dbfs_;
}

int GainControlImpl::set_compression_gain_db(int gain) {
  CriticalSectionScoped crit_scoped(crit_);
  if (gain < 0 || gain > 90) {
    return apm_->kBadParameterError;
  }

  compression_gain_db_ = gain;
  return Configure();
}

int GainControlImpl::compression_gain_db() const {
  return compression_gain_db_;
}

int GainControlImpl::enable_limiter(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  limiter_enabled_ = enable;
  return Configure();
}

bool GainControlImpl::is_limiter_enabled() const {
  return limiter_enabled_;
}

int GainControlImpl::Initialize() {
  int err = ProcessingComponent::Initialize();
  if (err != apm_->kNoError || !is_component_enabled()) {
    return err;
  }

  capture_levels_.assign(num_handles(), analog_capture_level_);
  return apm_->kNoError;
}

void* GainControlImpl::CreateHandle() const {
  Handle* handle = NULL;
  if (WebRtcAgc_Create(&handle) != apm_->kNoError) {
    handle = NULL;
  } else {
    assert(handle != NULL);
  }

  return handle;
}

void GainControlImpl::DestroyHandle(void* handle) const {
  WebRtcAgc_Free(static_cast<Handle*>(handle));
}

int GainControlImpl::InitializeHandle(void* handle) const {
  return WebRtcAgc_Init(static_cast<Handle*>(handle),
                          minimum_capture_level_,
                          maximum_capture_level_,
                          MapSetting(mode_),
                          apm_->proc_sample_rate_hz());
}

int GainControlImpl::ConfigureHandle(void* handle) const {
  WebRtcAgcConfig config;
  // TODO(ajm): Flip the sign here (since AGC expects a positive value) if we
  //            change the interface.
  //assert(target_level_dbfs_ <= 0);
  //config.targetLevelDbfs = static_cast<int16_t>(-target_level_dbfs_);
  config.targetLevelDbfs = static_cast<int16_t>(target_level_dbfs_);
  config.compressionGaindB =
      static_cast<int16_t>(compression_gain_db_);
  config.limiterEnable = limiter_enabled_;

  return WebRtcAgc_set_config(static_cast<Handle*>(handle), config);
}

int GainControlImpl::num_handles_required() const {
  return apm_->num_output_channels();
}

int GainControlImpl::GetHandleError(void* handle) const {
  // The AGC has no get_error() function.
  // (Despite listing errors in its interface...)
  assert(handle != NULL);
  return apm_->kUnspecifiedError;
}
}  // namespace webrtc
