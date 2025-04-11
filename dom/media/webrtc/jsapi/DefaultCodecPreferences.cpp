/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "jsapi/DefaultCodecPreferences.h"
#include "jsapi/PeerConnectionCtx.h"
#include "libwebrtcglue/VideoConduit.h"

namespace mozilla {

bool DefaultCodecPreferences::AV1EnabledStatic() {
  return WebrtcVideoConduit::HasAv1() &&
         StaticPrefs::media_webrtc_codec_video_av1_enabled();
}

bool DefaultCodecPreferences::H264EnabledStatic() {
  return SoftwareH264EnabledStatic() || HardwareH264EnabledStatic();
}

bool DefaultCodecPreferences::SoftwareH264EnabledStatic() {
  // If PeerConnectionCtx is not initialized, we can't check if H264 is
  // enabled. In that case, we assume sw H264 is enabled.
  // This only happens in gtest tests.
#ifdef MOZ_WIDGET_ANDROID
  // Although Play Store policy doesn't allow GMP plugin, Android has H.264 SW
  // codec.
  MOZ_ASSERT(!PeerConnectionCtx::isActive() ||
                 !PeerConnectionCtx::GetInstance()->gmpHasH264(),
             "GMP plugin not allowed on Android");
  return true;
#else
  return PeerConnectionCtx::isActive()
             ? PeerConnectionCtx::GetInstance()->gmpHasH264()
             : true;
#endif
}

bool DefaultCodecPreferences::HardwareH264EnabledStatic() {
  return WebrtcVideoConduit::HasH264Hardware() &&
         Preferences::GetBool("media.webrtc.hw.h264.enabled", false);
}

}  // namespace mozilla
