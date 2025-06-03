/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DefaultCodecPreferences.h"
#include "gmp/GMPUtils.h"
#include "libwebrtcglue/VideoConduit.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

bool DefaultCodecPreferences::AV1EnabledStatic() {
  return WebrtcVideoConduit::HasAv1() &&
         StaticPrefs::media_webrtc_codec_video_av1_enabled();
}

bool DefaultCodecPreferences::AV1PreferredStatic() {
  return StaticPrefs::media_webrtc_codec_video_av1_experimental_preferred();
}

bool DefaultCodecPreferences::H264EnabledStatic() {
  return SoftwareH264EnabledStatic() || HardwareH264EnabledStatic();
}

bool DefaultCodecPreferences::SoftwareH264EnabledStatic() {
#ifdef MOZ_WIDGET_ANDROID
  // Although Play Store policy doesn't allow GMP plugin, Android has H.264 SW
  // codec.
  MOZ_ASSERT(!HaveGMPFor("encode-video"_ns, {"h264"_ns}),
             "GMP plugin not allowed on Android");
  return true;
#else
  return HaveGMPFor("encode-video"_ns, {"h264"_ns}) &&
         HaveGMPFor("decode-video"_ns, {"h264"_ns});
#endif
}

bool DefaultCodecPreferences::HardwareH264EnabledStatic() {
  return WebrtcVideoConduit::HasH264Hardware() &&
         Preferences::GetBool("media.webrtc.hw.h264.enabled", false);
}

bool DefaultCodecPreferences::
    SendingH264PacketizationModeZeroSupportedStatic() {
  // Packetization mode 0 is unsupported by MediaDataEncoder.
  return HaveGMPFor("encode-video"_ns, {"h264"_ns});
}

}  // namespace mozilla
