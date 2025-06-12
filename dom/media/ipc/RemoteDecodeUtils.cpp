/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteDecodeUtils.h"
#include "mozilla/ipc/UtilityProcessChild.h"

namespace mozilla {

using SandboxingKind = ipc::SandboxingKind;

SandboxingKind GetCurrentSandboxingKind() {
  MOZ_ASSERT(XRE_IsUtilityProcess());
  return ipc::UtilityProcessChild::GetSingleton()->mSandbox;
}

SandboxingKind GetSandboxingKindFromLocation(RemoteMediaIn aLocation) {
  switch (aLocation) {
    case RemoteMediaIn::UtilityProcess_Generic:
      return SandboxingKind::GENERIC_UTILITY;
#ifdef MOZ_APPLEMEDIA
    case RemoteMediaIn::UtilityProcess_AppleMedia:
      return SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA;
      break;
#endif
#ifdef XP_WIN
    case RemoteMediaIn::UtilityProcess_WMF:
      return SandboxingKind::UTILITY_AUDIO_DECODING_WMF;
#endif
#ifdef MOZ_WMF_MEDIA_ENGINE
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM:
      return SandboxingKind::MF_MEDIA_ENGINE_CDM;
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported RemoteMediaIn");
      return SandboxingKind::COUNT;
  }
}

RemoteMediaIn GetRemoteMediaInFromKind(SandboxingKind aKind) {
  switch (aKind) {
    case SandboxingKind::GENERIC_UTILITY:
      return RemoteMediaIn::UtilityProcess_Generic;
#ifdef MOZ_APPLEMEDIA
    case SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA:
      return RemoteMediaIn::UtilityProcess_AppleMedia;
#endif
#ifdef XP_WIN
    case SandboxingKind::UTILITY_AUDIO_DECODING_WMF:
      return RemoteMediaIn::UtilityProcess_WMF;
#endif
#ifdef MOZ_WMF_MEDIA_ENGINE
    case SandboxingKind::MF_MEDIA_ENGINE_CDM:
      return RemoteMediaIn::UtilityProcess_MFMediaEngineCDM;
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported SandboxingKind");
      return RemoteMediaIn::Unspecified;
  }
}

RemoteMediaIn GetRemoteMediaInFromVideoBridgeSource(
    layers::VideoBridgeSource aSource) {
  switch (aSource) {
    case layers::VideoBridgeSource::RddProcess:
      return RemoteMediaIn::RddProcess;
    case layers::VideoBridgeSource::GpuProcess:
      return RemoteMediaIn::GpuProcess;
    case layers::VideoBridgeSource::MFMediaEngineCDMProcess:
      return RemoteMediaIn::UtilityProcess_MFMediaEngineCDM;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported VideoBridgeSource");
      return RemoteMediaIn::Unspecified;
  }
}

layers::VideoBridgeSource GetVideoBridgeSourceFromRemoteMediaIn(
    RemoteMediaIn aSource) {
  switch (aSource) {
    case RemoteMediaIn::RddProcess:
      return layers::VideoBridgeSource::RddProcess;
    case RemoteMediaIn::GpuProcess:
      return layers::VideoBridgeSource::GpuProcess;
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM:
      return layers::VideoBridgeSource::MFMediaEngineCDMProcess;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported RemoteMediaIn");
      return layers::VideoBridgeSource::_Count;
  }
}

const char* RemoteMediaInToStr(RemoteMediaIn aLocation) {
  switch (aLocation) {
    case RemoteMediaIn::RddProcess:
      return "RDD";
    case RemoteMediaIn::GpuProcess:
      return "GPU";
    case RemoteMediaIn::UtilityProcess_Generic:
      return "Utility Generic";
#ifdef MOZ_APPLEMEDIA
    case RemoteMediaIn::UtilityProcess_AppleMedia:
      return "Utility AppleMedia";
#endif
#ifdef XP_WIN
    case RemoteMediaIn::UtilityProcess_WMF:
      return "Utility WMF";
#endif
#ifdef MOZ_WMF_MEDIA_ENGINE
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM:
      return "Utility MF Media Engine CDM";
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported RemoteMediaIn");
      return "Unknown";
  }
}

}  // namespace mozilla
