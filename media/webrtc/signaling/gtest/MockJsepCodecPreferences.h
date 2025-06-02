
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_WEBRTC_SIGNALING_GTEST_MOCKJSEPCODECPREFERENCES_H_
#define MEDIA_WEBRTC_SIGNALING_GTEST_MOCKJSEPCODECPREFERENCES_H_
#include "jsapi/DefaultCodecPreferences.h"

namespace mozilla {

/*
This provides a stable set of codec preferences for unit tests. In order to
change a preference, you can set the member variable to the desired value.
*/
class MockJsepCodecPreferences : public JsepCodecPreferences {
  using Prefs = DefaultCodecPreferences;

 public:
  MockJsepCodecPreferences()
      : mVp9Enabled(Prefs::kDefaultVP9Enabled),
        mVp9Preferred(Prefs::kDefaultVP9Preferred),
        mVp8MaxFs(Prefs::kDefaultVP8MaxFs),
        mVp8MaxFr(Prefs::kDefaultVP8MaxFr),
        mUseTmmbr(Prefs::kDefaultUseTmmbr),
        mUseRemb(Prefs::kDefaultUseRemb) {}

  bool AV1Enabled() const override { return mAv1Enabled; }
  bool H264Enabled() const override { return mH264Enabled; }
  bool SoftwareH264Enabled() const override { return mSoftwareH264Enabled; }
  bool SendingH264PacketizationModeZeroSupported() const override {
    return mH264PacketizationModeZeroSupported;
  }
  int32_t H264Level() const override { return mH264Level; }
  int32_t H264MaxBr() const override { return mH264MaxBr; }
  int32_t H264MaxMbps() const override { return mH264MaxMbps; }
  bool VP9Enabled() const override { return mVp9Enabled; }
  bool VP9Preferred() const override { return mVp9Preferred; }
  int32_t VP8MaxFs() const override { return mVp8MaxFs; }
  int32_t VP8MaxFr() const override { return mVp8MaxFr; }
  bool UseTmmbr() const override { return mUseTmmbr; }
  bool UseRemb() const override { return mUseRemb; }
  bool UseRtx() const override { return mUseRtx; }
  bool UseTransportCC() const override { return mUseTransportCC; }
  bool UseAudioFec() const override { return mUseAudioFec; }
  bool RedUlpfecEnabled() const override { return mRedUlpfecEnabled; }

  bool mAv1Enabled = true;
  bool mH264Enabled = true;
  bool mSoftwareH264Enabled = true;
  bool mH264PacketizationModeZeroSupported = true;
  int32_t mH264Level = Prefs::kDefaultH264Level;
  int32_t mH264MaxBr = Prefs::kDefaultH264MaxBr;
  int32_t mH264MaxMbps = Prefs::kDefaultH264MaxMbps;
  bool mVp9Enabled = Prefs::kDefaultVP9Enabled;
  bool mVp9Preferred = Prefs::kDefaultVP9Preferred;
  int32_t mVp8MaxFs = Prefs::kDefaultVP8MaxFs;
  int32_t mVp8MaxFr = Prefs::kDefaultVP8MaxFr;
  bool mUseTmmbr = Prefs::kDefaultUseTmmbr;
  bool mUseRemb = Prefs::kDefaultUseRemb;
  bool mUseRtx = Prefs::kDefaultUseRtx;
  bool mUseTransportCC = Prefs::kDefaultUseTransportCC;
  bool mUseAudioFec = Prefs::kDefaultUseAudioFec;
  bool mRedUlpfecEnabled = Prefs::kDefaultRedUlpfecEnabled;
};
}  // namespace mozilla

#endif  // MEDIA_WEBRTC_SIGNALING_GTEST_MOCKJSEPCODECPREFERENCES_H_
