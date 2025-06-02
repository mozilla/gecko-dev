/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_JSAPI_DEFAULTCODECPREFERENCES_H_
#define DOM_MEDIA_WEBRTC_JSAPI_DEFAULTCODECPREFERENCES_H_

#include "jsep/JsepCodecDescription.h"
#include "mozilla/Preferences.h"

namespace mozilla {

enum class OverrideRtxPreference {
  NoOverride,
  OverrideWithEnabled,
  OverrideWithDisabled,
};

class DefaultCodecPreferences final : public JsepCodecPreferences {
 public:
  explicit DefaultCodecPreferences(
      const OverrideRtxPreference aOverrideRtxPreference)
      : mOverrideRtxEnabled(aOverrideRtxPreference) {}

  bool AV1Enabled() const override { return mAV1Enabled; }
  bool H264Enabled() const override { return mH264Enabled; }

  bool SoftwareH264Enabled() const override { return mSoftwareH264Enabled; }
  bool HardwareH264Enabled() const { return mHardwareH264Enabled; }

  bool H264PacketizationModeZeroSupported() const override {
    return mH264PacketizationModeZeroSupported;
  }

  int32_t H264Level() const override { return mH264Level; }

  int32_t H264MaxBr() const override { return mH264MaxBr; }

  int32_t H264MaxMbps() const override { return mH264MaxMbps; }

  bool VP9Enabled() const override { return mVP9Enabled; }

  bool VP9Preferred() const override { return mVP9Preferred; }

  int32_t VP8MaxFs() const override { return mVP8MaxFs; }

  int32_t VP8MaxFr() const override { return mVP8MaxFr; }

  bool UseTmmbr() const override { return mUseTmmbr; }

  bool UseRemb() const override { return mUseRemb; }

  bool UseRtx() const override {
    if (mOverrideRtxEnabled == OverrideRtxPreference::NoOverride) {
      return mUseRtx;
    }
    return mOverrideRtxEnabled == OverrideRtxPreference::OverrideWithEnabled;
  }

  bool UseTransportCC() const override { return mUseTransportCC; }

  bool UseAudioFec() const override { return mUseAudioFec; }

  bool RedUlpfecEnabled() const override { return mRedUlpfecEnabled; }

  static bool AV1EnabledStatic();

  static bool H264EnabledStatic();

  static bool SoftwareH264EnabledStatic();

  static bool HardwareH264EnabledStatic();

  static bool H264PacketizationModeZeroSupportedStatic();

  // minimum suggested for WebRTC spec
  static constexpr int32_t kDefaultH264Level = 31;
  static int32_t H264LevelStatic() {
    auto value = Preferences::GetInt("media.navigator.video.h264.level",
                                     kDefaultH264Level);
    if (value < 0) {
      return kDefaultH264Level;
    }
    return value & 0xFF;
  }

  static constexpr int32_t kDefaultH264MaxBr = 0;  // Unlimited
  static int32_t H264MaxBrStatic() {
    const auto maxBr = Preferences::GetInt("media.navigator.video.h264.max_br",
                                           kDefaultH264MaxBr);
    if (maxBr < 0) {
      return kDefaultH264MaxBr;
    }
    return maxBr;
  }

  static constexpr int32_t kDefaultH264MaxMbps = 0;  // Unlimited
  static int32_t H264MaxMbpsStatic() {
    const auto maxMbps = Preferences::GetInt(
        "media.navigator.video.h264.max_mbps", kDefaultH264MaxMbps);
    if (maxMbps < 0) {
      return kDefaultH264MaxMbps;
    }
    return maxMbps;
  }

  static constexpr bool kDefaultVP9Enabled = true;
  static bool VP9EnabledStatic() {
    return Preferences::GetBool("media.peerconnection.video.vp9_enabled",
                                kDefaultVP9Enabled);
  }

  static constexpr bool kDefaultVP9Preferred = false;
  static bool VP9PreferredStatic() {
    return Preferences::GetBool("media.peerconnection.video.vp9_preferred",
                                kDefaultVP9Preferred);
  }

  static constexpr int32_t kDefaultVP8MaxFs = 12288;  // Enough for 2048x1536
  static int32_t VP8MaxFsStatic() {
    auto value =
        Preferences::GetInt("media.navigator.video.max_fs", kDefaultVP8MaxFs);
    if (value <= 0) {
      return kDefaultVP8MaxFs;
    }
    return value;
  }

  static constexpr int32_t kDefaultVP8MaxFr = 60;
  static int32_t VP8MaxFrStatic() {
    auto value =
        Preferences::GetInt("media.navigator.video.max_fr", kDefaultVP8MaxFr);
    if (value <= kDefaultVP8MaxFr) {
      return 60;
    }
    return value;
  }

  static constexpr bool kDefaultUseTmmbr = false;
  static bool UseTmmbrStatic() {
    return Preferences::GetBool("media.navigator.video.use_tmmbr",
                                kDefaultUseTmmbr);
  }

  static constexpr bool kDefaultUseRemb = true;
  static bool UseRembStatic() {
    return Preferences::GetBool("media.navigator.video.use_remb",
                                kDefaultUseRemb);
  }

  static constexpr bool kDefaultUseRtx = true;
  static bool UseRtxStatic() {
    return Preferences::GetBool("media.peerconnection.video.use_rtx",
                                kDefaultUseRtx);
  }

  static constexpr bool kDefaultUseTransportCC = true;
  static bool UseTransportCCStatic() {
    return Preferences::GetBool("media.navigator.video.use_transport_cc",
                                kDefaultUseTransportCC);
  }

  static constexpr bool kDefaultUseAudioFec = true;
  static bool UseAudioFecStatic() {
    return Preferences::GetBool("media.navigator.audio.use_fec",
                                kDefaultUseAudioFec);
  }

  static constexpr bool kDefaultRedUlpfecEnabled = true;
  static bool RedUlpfecEnabledStatic() {
    return Preferences::GetBool("media.navigator.video.red_ulpfec_enabled",
                                kDefaultRedUlpfecEnabled);
  }

  // This is to accommodate the behavior of
  // RTCRtpTransceiver::SetCodecPreferences
  const OverrideRtxPreference mOverrideRtxEnabled =
      OverrideRtxPreference::NoOverride;

  const bool mAV1Enabled = AV1EnabledStatic();
  const bool mH264Enabled = H264EnabledStatic();
  const bool mSoftwareH264Enabled = SoftwareH264EnabledStatic();
  const bool mHardwareH264Enabled = HardwareH264EnabledStatic();
  const bool mH264PacketizationModeZeroSupported =
      H264PacketizationModeZeroSupportedStatic();
  const int32_t mH264Level = H264LevelStatic();
  const int32_t mH264MaxBr = H264MaxBrStatic();
  const int32_t mH264MaxMbps = H264MaxMbpsStatic();
  const bool mVP9Enabled = VP9EnabledStatic();
  const bool mVP9Preferred = VP9PreferredStatic();
  const int32_t mVP8MaxFs = VP8MaxFsStatic();
  const int32_t mVP8MaxFr = VP8MaxFrStatic();
  const bool mUseTmmbr = UseTmmbrStatic();
  const bool mUseRemb = UseRembStatic();
  const bool mUseRtx = UseRtxStatic();
  const bool mUseTransportCC = UseTransportCCStatic();
  const bool mUseAudioFec = UseAudioFecStatic();
  const bool mRedUlpfecEnabled = RedUlpfecEnabledStatic();
};
}  // namespace mozilla
#endif  // DOM_MEDIA_WEBRTC_JSAPI_DEFAULTCODECPREFERENCES_H_
