/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteMediaManagerChild.h"

#include "ErrorList.h"
#include "MP4Decoder.h"
#include "PDMFactory.h"
#include "PEMFactory.h"
#include "PlatformDecoderModule.h"
#include "PlatformEncoderModule.h"
#include "RemoteAudioDecoder.h"
#include "RemoteMediaDataDecoder.h"
#include "RemoteMediaDataEncoderChild.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"
#include "mozilla/DataMutex.h"
#include "mozilla/RemoteDecodeUtils.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/dom/ContentChild.h"  // for launching RDD w/ ContentChild
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/ipc/UtilityAudioDecoderChild.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"

#ifdef MOZ_WMF_MEDIA_ENGINE
#  include "MFMediaEngineChild.h"
#endif

#ifdef MOZ_WMF_CDM
#  include "MFCDMChild.h"
#endif

namespace mozilla {

#define LOG(msg, ...) \
  MOZ_LOG(gRemoteDecodeLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

using namespace layers;
using namespace gfx;

using media::EncodeSupport;
using media::EncodeSupportSet;

// Used so that we only ever attempt to check if the RDD/GPU/Utility processes
// should be launched serially. Protects sLaunchPromise
StaticMutex sLaunchMutex;
static EnumeratedArray<RemoteMediaIn, StaticRefPtr<GenericNonExclusivePromise>,
                       size_t(RemoteMediaIn::SENTINEL)>
    sLaunchPromises MOZ_GUARDED_BY(sLaunchMutex);

// Only modified on the main-thread, read on any thread. While it could be read
// on the main thread directly, for clarity we force access via the DataMutex
// wrapper.
MOZ_RUNINIT static StaticDataMutex<StaticRefPtr<nsIThread>>
    sRemoteMediaManagerChildThread("sRemoteMediaManagerChildThread");

// Only accessed from sRemoteMediaManagerChildThread
static EnumeratedArray<RemoteMediaIn, StaticRefPtr<RemoteMediaManagerChild>,
                       size_t(RemoteMediaIn::SENTINEL)>
    sRemoteMediaManagerChildForProcesses;

static StaticAutoPtr<nsTArray<RefPtr<Runnable>>> sRecreateTasks;

// Used for protecting codec support information collected from different remote
// processes.
StaticMutex sProcessSupportedMutex;
MOZ_GLOBINIT static EnumeratedArray<RemoteMediaIn,
                                    Maybe<media::MediaCodecsSupported>,
                                    size_t(RemoteMediaIn::SENTINEL)>
    sProcessSupported MOZ_GUARDED_BY(sProcessSupportedMutex);

class ShutdownObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 protected:
  ~ShutdownObserver() = default;
};
NS_IMPL_ISUPPORTS(ShutdownObserver, nsIObserver);

NS_IMETHODIMP
ShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID));
  RemoteMediaManagerChild::Shutdown();
  return NS_OK;
}

StaticRefPtr<ShutdownObserver> sObserver;

/* static */
void RemoteMediaManagerChild::Init() {
  LOG("RemoteMediaManagerChild Init");

  auto remoteDecoderManagerThread = sRemoteMediaManagerChildThread.Lock();
  if (!*remoteDecoderManagerThread) {
    LOG("RemoteMediaManagerChild's thread is created");
    // We can't use a MediaThreadType::SUPERVISOR as the RemoteDecoderModule
    // runs on it and dispatch synchronous tasks to the manager thread, should
    // more than 4 concurrent videos being instantiated at the same time, we
    // could end up in a deadlock.
    RefPtr<nsIThread> childThread;
    nsresult rv = NS_NewNamedThread(
        "RemVidChild", getter_AddRefs(childThread),
        NS_NewRunnableFunction(
            "RemoteMediaManagerChild::InitPBackground", []() {
              ipc::PBackgroundChild* bgActor =
                  ipc::BackgroundChild::GetOrCreateForCurrentThread();
              NS_WARNING_ASSERTION(bgActor,
                                   "Failed to start Background channel");
              Unused << bgActor;
            }));

    NS_ENSURE_SUCCESS_VOID(rv);
    *remoteDecoderManagerThread = childThread;
    sRecreateTasks = new nsTArray<RefPtr<Runnable>>();
    sObserver = new ShutdownObserver();
    nsContentUtils::RegisterShutdownObserver(sObserver);
  }
}

/* static */
void RemoteMediaManagerChild::InitForGPUProcess(
    Endpoint<PRemoteMediaManagerChild>&& aVideoManager) {
  MOZ_ASSERT(NS_IsMainThread());

  Init();

  auto remoteDecoderManagerThread = sRemoteMediaManagerChildThread.Lock();
  MOZ_ALWAYS_SUCCEEDS(
      (*remoteDecoderManagerThread)
          ->Dispatch(NewRunnableFunction(
              "InitForContentRunnable", &OpenRemoteMediaManagerChildForProcess,
              std::move(aVideoManager), RemoteMediaIn::GpuProcess)));
}

/* static */
void RemoteMediaManagerChild::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("RemoteMediaManagerChild Shutdown");

  if (sObserver) {
    nsContentUtils::UnregisterShutdownObserver(sObserver);
    sObserver = nullptr;
  }

  nsCOMPtr<nsIThread> childThread;
  {
    auto remoteDecoderManagerThread = sRemoteMediaManagerChildThread.Lock();
    childThread = remoteDecoderManagerThread->forget();
    LOG("RemoteMediaManagerChild's thread is released");
  }
  if (childThread) {
    MOZ_ALWAYS_SUCCEEDS(childThread->Dispatch(
        NS_NewRunnableFunction("dom::RemoteMediaManagerChild::Shutdown", []() {
          for (auto& p : sRemoteMediaManagerChildForProcesses) {
            if (p && p->CanSend()) {
              p->Close();
            }
            p = nullptr;
          }
          {
            StaticMutexAutoLock lock(sLaunchMutex);
            for (auto& p : sLaunchPromises) {
              p = nullptr;
            }
          }
          ipc::BackgroundChild::CloseForCurrentThread();
        })));
    childThread->Shutdown();
    sRecreateTasks = nullptr;
  }
}

/* static */ void RemoteMediaManagerChild::RunWhenGPUProcessRecreated(
    const RemoteMediaManagerChild* aDyingManager,
    already_AddRefed<Runnable> aTask) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());

  // If we've already been recreated, then run the task immediately.
  auto* manager = GetSingleton(RemoteMediaIn::GpuProcess);
  if (manager && manager != aDyingManager && manager->CanSend()) {
    RefPtr<Runnable> task = aTask;
    task->Run();
  } else {
    sRecreateTasks->AppendElement(aTask);
  }
}

/* static */
RemoteMediaManagerChild* RemoteMediaManagerChild::GetSingleton(
    RemoteMediaIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return nullptr;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());
  switch (aLocation) {
    case RemoteMediaIn::GpuProcess:
    case RemoteMediaIn::RddProcess:
    case RemoteMediaIn::UtilityProcess_Generic:
    case RemoteMediaIn::UtilityProcess_AppleMedia:
    case RemoteMediaIn::UtilityProcess_WMF:
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM:
      return sRemoteMediaManagerChildForProcesses[aLocation];
    default:
      MOZ_CRASH("Unexpected RemoteMediaIn variant");
      return nullptr;
  }
}

/* static */
nsISerialEventTarget* RemoteMediaManagerChild::GetManagerThread() {
  auto remoteDecoderManagerThread = sRemoteMediaManagerChildThread.Lock();
  return *remoteDecoderManagerThread;
}

/* static */
bool RemoteMediaManagerChild::Supports(RemoteMediaIn aLocation,
                                       const SupportDecoderParams& aParams,
                                       DecoderDoctorDiagnostics* aDiagnostics) {
  Maybe<media::MediaCodecsSupported> supported;
  switch (aLocation) {
    case RemoteMediaIn::GpuProcess:
    case RemoteMediaIn::RddProcess:
    case RemoteMediaIn::UtilityProcess_AppleMedia:
    case RemoteMediaIn::UtilityProcess_Generic:
    case RemoteMediaIn::UtilityProcess_WMF:
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM: {
      StaticMutexAutoLock lock(sProcessSupportedMutex);
      supported = sProcessSupported[aLocation];
      break;
    }
    default:
      return false;
  }
  if (!supported) {
    // We haven't received the correct information yet from either the GPU or
    // the RDD process nor the Utility process.
    if (aLocation == RemoteMediaIn::UtilityProcess_Generic ||
        aLocation == RemoteMediaIn::UtilityProcess_AppleMedia ||
        aLocation == RemoteMediaIn::UtilityProcess_WMF ||
        aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
      LaunchUtilityProcessIfNeeded(aLocation);
    }
    if (aLocation == RemoteMediaIn::RddProcess) {
      // Ensure the RDD process got started.
      // TODO: This can be removed once bug 1684991 is fixed.
      LaunchRDDProcessIfNeeded();
    }

    // Assume the format is supported to prevent false negative, if the remote
    // process supports that specific track type.
    const bool isVideo = aParams.mConfig.IsVideo();
    const bool isAudio = aParams.mConfig.IsAudio();
    const auto trackSupport = GetTrackSupport(aLocation);
    if (isVideo) {
      // Special condition for HEVC, which can only be supported in specific
      // process. As HEVC support is still a experimental feature, we don't want
      // to report support for it arbitrarily.
      if (MP4Decoder::IsHEVC(aParams.mConfig.mMimeType)) {
        if (!StaticPrefs::media_hevc_enabled()) {
          return false;
        }
#if defined(XP_WIN)
        return aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM ||
               aLocation == RemoteMediaIn::GpuProcess;
#else
        return trackSupport.contains(TrackSupport::DecodeVideo);
#endif
      }
      return trackSupport.contains(TrackSupport::DecodeVideo);
    }
    if (isAudio) {
      return trackSupport.contains(TrackSupport::DecodeAudio);
    }
    MOZ_ASSERT_UNREACHABLE("Not audio and video?!");
    return false;
  }

  // We can ignore the SupportDecoderParams argument for now as creation of the
  // decoder will actually fail later and fallback PDMs will be tested on later.
  return !PDMFactory::SupportsMimeType(aParams.MimeType(), *supported,
                                       aLocation)
              .isEmpty();
}

/* static */
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteMediaManagerChild::CreateAudioDecoder(const CreateDecoderParams& aParams,
                                            RemoteMediaIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }

  if (!GetTrackSupport(aLocation).contains(TrackSupport::DecodeAudio)) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_CANCELED,
                    nsPrintfCString("%s doesn't support audio decoding",
                                    RemoteMediaInToStr(aLocation))
                        .get()),
        __func__);
  }

  if (!aParams.mMediaEngineId &&
      aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                    nsPrintfCString("%s only support for media engine playback",
                                    RemoteMediaInToStr(aLocation))
                        .get()),
        __func__);
  }

  RefPtr<GenericNonExclusivePromise> launchPromise;
  if (StaticPrefs::media_utility_process_enabled() &&
      (aLocation == RemoteMediaIn::UtilityProcess_Generic ||
       aLocation == RemoteMediaIn::UtilityProcess_AppleMedia ||
       aLocation == RemoteMediaIn::UtilityProcess_WMF)) {
    launchPromise = LaunchUtilityProcessIfNeeded(aLocation);
  } else if (aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
    launchPromise = LaunchUtilityProcessIfNeeded(aLocation);
  } else {
    if (StaticPrefs::media_allow_audio_non_utility()) {
      launchPromise = LaunchRDDProcessIfNeeded();
    } else {
      return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
          MediaResult(
              NS_ERROR_DOM_MEDIA_DENIED_IN_NON_UTILITY,
              nsPrintfCString("%s is not allowed to perform audio decoding",
                              RemoteMediaInToStr(aLocation))
                  .get()),
          __func__);
    }
  }
  LOG("Create audio decoder in %s", RemoteMediaInToStr(aLocation));

  return launchPromise->Then(
      managerThread, __func__,
      [params = CreateDecoderParamsForAsync(aParams), aLocation](bool) {
        auto child = MakeRefPtr<RemoteAudioDecoderChild>(aLocation);
        MediaResult result = child->InitIPDL(
            params.AudioConfig(), params.mOptions, params.mMediaEngineId);
        if (NS_FAILED(result)) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              result, __func__);
        }
        return Construct(std::move(child), aLocation);
      },
      [aLocation](nsresult aResult) {
        return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
            MediaResult(aResult,
                        aLocation == RemoteMediaIn::GpuProcess
                            ? "Couldn't start GPU process"
                            : (aLocation == RemoteMediaIn::RddProcess
                                   ? "Couldn't start RDD process"
                                   : "Couldn't start Utility process")),
            __func__);
      });
}

/* static */
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteMediaManagerChild::CreateVideoDecoder(const CreateDecoderParams& aParams,
                                            RemoteMediaIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }

  if (!aParams.mKnowsCompositor && aLocation == RemoteMediaIn::GpuProcess) {
    // We don't have an image bridge; don't attempt to decode in the GPU
    // process.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR, __func__);
  }

  if (!GetTrackSupport(aLocation).contains(TrackSupport::DecodeVideo)) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_CANCELED,
                    nsPrintfCString("%s doesn't support video decoding",
                                    RemoteMediaInToStr(aLocation))
                        .get()),
        __func__);
  }

  if (!aParams.mMediaEngineId &&
      aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                    nsPrintfCString("%s only support for media engine playback",
                                    RemoteMediaInToStr(aLocation))
                        .get()),
        __func__);
  }

  MOZ_ASSERT(aLocation != RemoteMediaIn::Unspecified);

  RefPtr<GenericNonExclusivePromise> p;
  if (aLocation == RemoteMediaIn::GpuProcess) {
    p = GenericNonExclusivePromise::CreateAndResolve(true, __func__);
  } else if (aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
    p = LaunchUtilityProcessIfNeeded(aLocation);
  } else {
    p = LaunchRDDProcessIfNeeded();
  }
  LOG("Create video decoder in %s", RemoteMediaInToStr(aLocation));

  return p->Then(
      managerThread, __func__,
      [aLocation, params = CreateDecoderParamsForAsync(aParams)](bool) {
        auto child = MakeRefPtr<RemoteVideoDecoderChild>(aLocation);
        MediaResult result = child->InitIPDL(
            params.VideoConfig(), params.mRate.mValue, params.mOptions,
            params.mKnowsCompositor
                ? Some(params.mKnowsCompositor->GetTextureFactoryIdentifier())
                : Nothing(),
            params.mMediaEngineId, params.mTrackingId);
        if (NS_FAILED(result)) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              result, __func__);
        }
        return Construct(std::move(child), aLocation);
      },
      [](nsresult aResult) {
        return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
            MediaResult(aResult, "Couldn't start RDD process"), __func__);
      });
}

/* static */
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteMediaManagerChild::Construct(RefPtr<RemoteDecoderChild>&& aChild,
                                   RemoteMediaIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());

  RefPtr<PlatformDecoderModule::CreateDecoderPromise> p =
      aChild->SendConstruct()->Then(
          managerThread, __func__,
          [child = std::move(aChild)](MediaResult aResult) {
            if (NS_FAILED(aResult)) {
              // We will never get to use this remote decoder, tear it down.
              child->DestroyIPDL();
              return PlatformDecoderModule::CreateDecoderPromise::
                  CreateAndReject(aResult, __func__);
            }
            return PlatformDecoderModule::CreateDecoderPromise::
                CreateAndResolve(MakeRefPtr<RemoteMediaDataDecoder>(child),
                                 __func__);
          },
          [aLocation](const mozilla::ipc::ResponseRejectReason& aReason) {
            // The parent has died.
            nsresult err = NS_ERROR_DOM_MEDIA_REMOTE_CRASHED_UTILITY_ERR;
            if (aLocation == RemoteMediaIn::GpuProcess ||
                aLocation == RemoteMediaIn::RddProcess) {
              err = NS_ERROR_DOM_MEDIA_REMOTE_CRASHED_RDD_OR_GPU_ERR;
            } else if (aLocation ==
                       RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
              err = NS_ERROR_DOM_MEDIA_REMOTE_CRASHED_MF_CDM_ERR;
            }
            return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
                err, __func__);
          });
  return p;
}

/* static */
EncodeSupportSet RemoteMediaManagerChild::Supports(RemoteMediaIn aLocation,
                                                   CodecType aCodec) {
  Maybe<media::MediaCodecsSupported> supported;
  switch (aLocation) {
    case RemoteMediaIn::GpuProcess:
    case RemoteMediaIn::RddProcess:
    case RemoteMediaIn::UtilityProcess_AppleMedia:
    case RemoteMediaIn::UtilityProcess_Generic:
    case RemoteMediaIn::UtilityProcess_WMF:
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM: {
      StaticMutexAutoLock lock(sProcessSupportedMutex);
      supported = sProcessSupported[aLocation];
      break;
    }
    default:
      return EncodeSupportSet{};
  }
  if (!supported) {
    // We haven't received the correct information yet from either the GPU or
    // the RDD process nor the Utility process.
    if (aLocation == RemoteMediaIn::UtilityProcess_Generic ||
        aLocation == RemoteMediaIn::UtilityProcess_AppleMedia ||
        aLocation == RemoteMediaIn::UtilityProcess_WMF ||
        aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
      LaunchUtilityProcessIfNeeded(aLocation);
    }
    if (aLocation == RemoteMediaIn::RddProcess) {
      // Ensure the RDD process got started.
      // TODO: This can be removed once bug 1684991 is fixed.
      LaunchRDDProcessIfNeeded();
    }

    // Assume the format is supported to prevent false negative, if the remote
    // process supports that specific track type.
    const bool isVideo =
        aCodec > CodecType::_BeginVideo_ && aCodec < CodecType::_EndVideo_;
    const bool isAudio =
        aCodec > CodecType::_BeginAudio_ && aCodec < CodecType::_EndAudio_;
    const auto trackSupport = GetTrackSupport(aLocation);
    if (isVideo) {
      // Special condition for HEVC, which can only be supported in specific
      // process. As HEVC support is still a experimental feature, we don't want
      // to report support for it arbitrarily.
      bool supported = trackSupport.contains(TrackSupport::EncodeVideo);
      if (aCodec == CodecType::H265) {
        if (!StaticPrefs::media_hevc_enabled()) {
          return EncodeSupportSet{};
        }
#if defined(XP_WIN)
        supported = aLocation == RemoteMediaIn::GpuProcess;
#endif
      }
      return supported ? EncodeSupportSet{EncodeSupport::SoftwareEncode}
                       : EncodeSupportSet{};
    }
    if (isAudio) {
      return trackSupport.contains(TrackSupport::EncodeAudio)
                 ? EncodeSupportSet{EncodeSupport::SoftwareEncode}
                 : EncodeSupportSet{};
    }
    MOZ_ASSERT_UNREACHABLE("Not audio and video?!");
    return EncodeSupportSet{};
  }

  // We can ignore the rest of EncoderConfig for now as creation of the encoder
  // will actually fail later and fallback PEMs will be tested on later.
  return PEMFactory::SupportsCodec(aCodec, *supported, aLocation);
}

/* static */ RefPtr<PlatformEncoderModule::CreateEncoderPromise>
RemoteMediaManagerChild::InitializeEncoder(
    RefPtr<RemoteMediaDataEncoderChild>&& aEncoder,
    const EncoderConfig& aConfig) {
  RemoteMediaIn location = aEncoder->GetLocation();

  TrackSupport required;
  if (aConfig.IsAudio()) {
    required = TrackSupport::EncodeAudio;
  } else if (aConfig.IsVideo()) {
    required = TrackSupport::EncodeVideo;
  } else {
    return PlatformEncoderModule::CreateEncoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_CANCELED,
                    nsPrintfCString("%s doesn't support encoding",
                                    RemoteMediaInToStr(location))
                        .get()),
        __func__);
  }

  if (!GetTrackSupport(location).contains(required)) {
    return PlatformEncoderModule::CreateEncoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_CANCELED,
                    nsPrintfCString("%s doesn't support encoding",
                                    RemoteMediaInToStr(location))
                        .get()),
        __func__);
  }

  MOZ_ASSERT(location != RemoteMediaIn::Unspecified);

  RefPtr<GenericNonExclusivePromise> p;
  if (location == RemoteMediaIn::UtilityProcess_Generic ||
      location == RemoteMediaIn::UtilityProcess_AppleMedia ||
      location == RemoteMediaIn::UtilityProcess_WMF) {
    p = LaunchUtilityProcessIfNeeded(location);
  } else if (location == RemoteMediaIn::GpuProcess) {
    p = GenericNonExclusivePromise::CreateAndResolve(true, __func__);
  } else if (location == RemoteMediaIn::RddProcess) {
    p = LaunchRDDProcessIfNeeded();
  } else {
    p = GenericNonExclusivePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DENIED_IN_NON_UTILITY, __func__);
  }
  LOG("Creating %s encoder type %d in %s",
      aConfig.IsAudio() ? "audio" : "video", static_cast<int>(aConfig.mCodec),
      RemoteMediaInToStr(location));

  auto* managerThread = aEncoder->GetManagerThread();
  return p->Then(
      managerThread, __func__,
      [encoder = std::move(aEncoder), aConfig](bool) {
        auto* manager = GetSingleton(encoder->GetLocation());
        if (!manager) {
          LOG("Create encoder in %s failed, shutdown",
              RemoteMediaInToStr(encoder->GetLocation()));
          // We got shutdown.
          return PlatformEncoderModule::CreateEncoderPromise::CreateAndReject(
              MediaResult(NS_ERROR_DOM_MEDIA_CANCELED,
                          "Remote manager not available"),
              __func__);
        }
        if (!manager->SendPRemoteEncoderConstructor(encoder, aConfig)) {
          LOG("Create encoder in %s failed, send failed",
              RemoteMediaInToStr(encoder->GetLocation()));
          return PlatformEncoderModule::CreateEncoderPromise::CreateAndReject(
              MediaResult(NS_ERROR_NOT_AVAILABLE,
                          "Failed to construct encoder actor"),
              __func__);
        }
        return encoder->Construct();
      },
      [location](nsresult aResult) {
        LOG("Create encoder in %s failed, cannot start process",
            RemoteMediaInToStr(location));
        return PlatformEncoderModule::CreateEncoderPromise::CreateAndReject(
            MediaResult(aResult, "Couldn't start encode process"), __func__);
      });
}

/* static */
RefPtr<GenericNonExclusivePromise>
RemoteMediaManagerChild::LaunchRDDProcessIfNeeded() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess(),
                        "Only supported from a content process.");

  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  StaticMutexAutoLock lock(sLaunchMutex);
  auto& rddLaunchPromise = sLaunchPromises[RemoteMediaIn::RddProcess];
  if (rddLaunchPromise) {
    return rddLaunchPromise;
  }

  // We have a couple possible states here.  We are in a content process
  // and:
  // 1) the RDD process has never been launched.  RDD should be launched
  //    and the IPC connections setup.
  // 2) the RDD process has been launched, but this particular content
  //    process has not setup (or has lost) its IPC connection.
  // In the code below, we assume we need to launch the RDD process and
  // setup the IPC connections.  However, if the manager thread for
  // RemoteMediaManagerChild is available we do a quick check to see
  // if we can send (meaning the IPC channel is open).  If we can send,
  // then no work is necessary.  If we can't send, then we call
  // LaunchRDDProcess which will launch RDD if necessary, and setup the
  // IPC connections between *this* content process and the RDD process.

  RefPtr<GenericNonExclusivePromise> p = InvokeAsync(
      managerThread, __func__, []() -> RefPtr<GenericNonExclusivePromise> {
        auto* rps = GetSingleton(RemoteMediaIn::RddProcess);
        if (rps && rps->CanSend()) {
          return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
        }
        nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
        ipc::PBackgroundChild* bgActor =
            ipc::BackgroundChild::GetForCurrentThread();
        if (!managerThread || NS_WARN_IF(!bgActor)) {
          return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                             __func__);
        }

        return bgActor->SendEnsureRDDProcessAndCreateBridge()->Then(
            managerThread, __func__,
            [](ipc::PBackgroundChild::EnsureRDDProcessAndCreateBridgePromise::
                   ResolveOrRejectValue&& aResult) {
              nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
              if (!managerThread || aResult.IsReject()) {
                // The parent process died or we got shutdown
                return GenericNonExclusivePromise::CreateAndReject(
                    NS_ERROR_FAILURE, __func__);
              }
              nsresult rv = std::get<0>(aResult.ResolveValue());
              if (NS_FAILED(rv)) {
                return GenericNonExclusivePromise::CreateAndReject(rv,
                                                                   __func__);
              }
              OpenRemoteMediaManagerChildForProcess(
                  std::get<1>(std::move(aResult.ResolveValue())),
                  RemoteMediaIn::RddProcess);
              return GenericNonExclusivePromise::CreateAndResolve(true,
                                                                  __func__);
            });
      });

  // This should not be dispatched to a threadpool thread, so use managerThread
  p = p->Then(
      managerThread, __func__,
      [](const GenericNonExclusivePromise::ResolveOrRejectValue& aResult) {
        StaticMutexAutoLock lock(sLaunchMutex);
        sLaunchPromises[RemoteMediaIn::RddProcess] = nullptr;
        return GenericNonExclusivePromise::CreateAndResolveOrReject(aResult,
                                                                    __func__);
      });

  rddLaunchPromise = p;
  return rddLaunchPromise;
}

/* static */
RefPtr<GenericNonExclusivePromise>
RemoteMediaManagerChild::LaunchUtilityProcessIfNeeded(RemoteMediaIn aLocation) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess(),
                        "Only supported from a content process.");

  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  MOZ_ASSERT(aLocation == RemoteMediaIn::UtilityProcess_Generic ||
             aLocation == RemoteMediaIn::UtilityProcess_AppleMedia ||
             aLocation == RemoteMediaIn::UtilityProcess_WMF ||
             aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM);
  StaticMutexAutoLock lock(sLaunchMutex);
  auto& utilityLaunchPromise = sLaunchPromises[aLocation];

  if (utilityLaunchPromise) {
    return utilityLaunchPromise;
  }

  // We have a couple possible states here.  We are in a content process
  // and:
  // 1) the Utility process has never been launched.  Utility should be launched
  //    and the IPC connections setup.
  // 2) the Utility process has been launched, but this particular content
  //    process has not setup (or has lost) its IPC connection.
  // In the code below, we assume we need to launch the Utility process and
  // setup the IPC connections.  However, if the manager thread for
  // RemoteMediaManagerChild is available we do a quick check to see
  // if we can send (meaning the IPC channel is open).  If we can send,
  // then no work is necessary.  If we can't send, then we call
  // LaunchUtilityProcess which will launch Utility if necessary, and setup the
  // IPC connections between *this* content process and the Utility process.

  RefPtr<GenericNonExclusivePromise> p = InvokeAsync(
      managerThread, __func__,
      [aLocation]() -> RefPtr<GenericNonExclusivePromise> {
        auto* rps = GetSingleton(aLocation);
        if (rps && rps->CanSend()) {
          return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
        }
        nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
        ipc::PBackgroundChild* bgActor =
            ipc::BackgroundChild::GetForCurrentThread();
        if (!managerThread || NS_WARN_IF(!bgActor)) {
          return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                             __func__);
        }

        return bgActor->SendEnsureUtilityProcessAndCreateBridge(aLocation)
            ->Then(managerThread, __func__,
                   [aLocation](ipc::PBackgroundChild::
                                   EnsureUtilityProcessAndCreateBridgePromise::
                                       ResolveOrRejectValue&& aResult)
                       -> RefPtr<GenericNonExclusivePromise> {
                     nsCOMPtr<nsISerialEventTarget> managerThread =
                         GetManagerThread();
                     if (!managerThread || aResult.IsReject()) {
                       // The parent process died or we got shutdown
                       return GenericNonExclusivePromise::CreateAndReject(
                           NS_ERROR_FAILURE, __func__);
                     }
                     nsresult rv = std::get<0>(aResult.ResolveValue());
                     if (NS_FAILED(rv)) {
                       return GenericNonExclusivePromise::CreateAndReject(
                           rv, __func__);
                     }
                     OpenRemoteMediaManagerChildForProcess(
                         std::get<1>(std::move(aResult.ResolveValue())),
                         aLocation);
                     return GenericNonExclusivePromise::CreateAndResolve(
                         true, __func__);
                   });
      });

  // Let's make sure this promise is also run on the managerThread to avoid
  // situations where it would be run on a threadpool thread.
  // During bug 1794988 this was happening when enabling Utility for audio on
  // Android when running the sequence of tests
  //   dom/media/test/test_access_control.html
  //   dom/media/test/test_arraybuffer.html
  //
  // We would have a launched utility process but the promises would not have
  // been cleared, so any subsequent tentative to perform audio decoding would
  // think the process is not yet ran and it would try to wait on the pending
  // promises.
  p = p->Then(
      managerThread, __func__,
      [aLocation](
          const GenericNonExclusivePromise::ResolveOrRejectValue& aResult) {
        StaticMutexAutoLock lock(sLaunchMutex);
        sLaunchPromises[aLocation] = nullptr;
        return GenericNonExclusivePromise::CreateAndResolveOrReject(aResult,
                                                                    __func__);
      });
  utilityLaunchPromise = p;
  return utilityLaunchPromise;
}

/* static */
TrackSupportSet RemoteMediaManagerChild::GetTrackSupport(
    RemoteMediaIn aLocation) {
  TrackSupportSet s{TrackSupport::None};
  switch (aLocation) {
    case RemoteMediaIn::GpuProcess:
      s = TrackSupport::DecodeVideo;
      if (StaticPrefs::media_use_remote_encoder_video()) {
        s += TrackSupport::EncodeVideo;
      }
      break;
    case RemoteMediaIn::RddProcess:
      s = TrackSupport::DecodeVideo;
      if (StaticPrefs::media_use_remote_encoder_video()) {
        s += TrackSupport::EncodeVideo;
      }
      // Only use RDD for audio coding if we don't have the utility process.
      if (!StaticPrefs::media_utility_process_enabled()) {
        s += TrackSupport::DecodeAudio;
        if (StaticPrefs::media_use_remote_encoder_audio()) {
          s += TrackSupport::EncodeAudio;
        }
      }
      break;
    case RemoteMediaIn::UtilityProcess_Generic:
    case RemoteMediaIn::UtilityProcess_AppleMedia:
    case RemoteMediaIn::UtilityProcess_WMF:
      if (StaticPrefs::media_utility_process_enabled()) {
        s = TrackSupport::DecodeAudio;
        if (StaticPrefs::media_use_remote_encoder_audio()) {
          s += TrackSupport::EncodeAudio;
        }
      }
      break;
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM:
#ifdef MOZ_WMF_MEDIA_ENGINE
      // When we enable the media engine, it would need both tracks to
      // synchronize the a/v playback.
      if (StaticPrefs::media_wmf_media_engine_enabled()) {
        s = TrackSupportSet{TrackSupport::DecodeAudio,
                            TrackSupport::DecodeVideo};
      }
#endif
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Undefined location!");
      break;
  }
  return s;
}

PRemoteDecoderChild* RemoteMediaManagerChild::AllocPRemoteDecoderChild(
    const RemoteDecoderInfoIPDL& /* not used */,
    const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
    const Maybe<uint64_t>& aMediaEngineId,
    const Maybe<TrackingId>& aTrackingId) {
  // RemoteDecoderModule is responsible for creating RemoteDecoderChild
  // classes.
  MOZ_ASSERT(false,
             "RemoteMediaManagerChild cannot create "
             "RemoteDecoderChild classes");
  return nullptr;
}

bool RemoteMediaManagerChild::DeallocPRemoteDecoderChild(
    PRemoteDecoderChild* actor) {
  RemoteDecoderChild* child = static_cast<RemoteDecoderChild*>(actor);
  child->IPDLActorDestroyed();
  return true;
}

PMFMediaEngineChild* RemoteMediaManagerChild::AllocPMFMediaEngineChild() {
  MOZ_ASSERT_UNREACHABLE(
      "RemoteMediaManagerChild cannot create MFMediaEngineChild classes");
  return nullptr;
}

bool RemoteMediaManagerChild::DeallocPMFMediaEngineChild(
    PMFMediaEngineChild* actor) {
#ifdef MOZ_WMF_MEDIA_ENGINE
  MFMediaEngineChild* child = static_cast<MFMediaEngineChild*>(actor);
  child->IPDLActorDestroyed();
#endif
  return true;
}

PMFCDMChild* RemoteMediaManagerChild::AllocPMFCDMChild(const nsAString&) {
  MOZ_ASSERT_UNREACHABLE(
      "RemoteMediaManagerChild cannot create PMFContentDecryptionModuleChild "
      "classes");
  return nullptr;
}

bool RemoteMediaManagerChild::DeallocPMFCDMChild(PMFCDMChild* actor) {
#ifdef MOZ_WMF_CDM
  static_cast<MFCDMChild*>(actor)->IPDLActorDestroyed();
#endif
  return true;
}

RemoteMediaManagerChild::RemoteMediaManagerChild(RemoteMediaIn aLocation)
    : mLocation(aLocation) {
  MOZ_ASSERT(mLocation == RemoteMediaIn::GpuProcess ||
             mLocation == RemoteMediaIn::RddProcess ||
             mLocation == RemoteMediaIn::UtilityProcess_Generic ||
             mLocation == RemoteMediaIn::UtilityProcess_AppleMedia ||
             mLocation == RemoteMediaIn::UtilityProcess_WMF ||
             mLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM);
}

/* static */
void RemoteMediaManagerChild::OpenRemoteMediaManagerChildForProcess(
    Endpoint<PRemoteMediaManagerChild>&& aEndpoint, RemoteMediaIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());

  // For GPU process, make sure we always dispatch everything in sRecreateTasks,
  // even if we fail since this is as close to being recreated as we will ever
  // be.
  auto runRecreateTasksIfNeeded = MakeScopeExit([aLocation]() {
    if (aLocation == RemoteMediaIn::GpuProcess) {
      for (Runnable* task : *sRecreateTasks) {
        task->Run();
      }
      sRecreateTasks->Clear();
    }
  });

  // Only create RemoteMediaManagerChild, bind new endpoint and init
  // ipdl if:
  // 1) haven't init'd sRemoteMediaManagerChildForProcesses[aLocation]
  // or
  // 2) if ActorDestroy was called meaning the other end of the ipc channel was
  //    torn down
  // But for GPU process, we always recreate a new manager child.
  MOZ_ASSERT(aLocation != RemoteMediaIn::SENTINEL);
  auto& remoteDecoderManagerChild =
      sRemoteMediaManagerChildForProcesses[aLocation];
  if (aLocation != RemoteMediaIn::GpuProcess && remoteDecoderManagerChild &&
      remoteDecoderManagerChild->CanSend()) {
    return;
  }
  remoteDecoderManagerChild = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<RemoteMediaManagerChild> manager =
        new RemoteMediaManagerChild(aLocation);
    if (aEndpoint.Bind(manager)) {
      remoteDecoderManagerChild = manager;
    }
  }
}

bool RemoteMediaManagerChild::DeallocShmem(mozilla::ipc::Shmem& aShmem) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    return false;
  }
  if (!managerThread->IsOnCurrentThread()) {
    MOZ_ALWAYS_SUCCEEDS(managerThread->Dispatch(NS_NewRunnableFunction(
        "RemoteMediaManagerChild::DeallocShmem",
        [self = RefPtr{this}, shmem = aShmem]() mutable {
          if (self->CanSend()) {
            self->PRemoteMediaManagerChild::DeallocShmem(shmem);
          }
        })));
    return true;
  }
  return PRemoteMediaManagerChild::DeallocShmem(aShmem);
}

struct SurfaceDescriptorUserData {
  SurfaceDescriptorUserData(RemoteMediaManagerChild* aAllocator,
                            SurfaceDescriptor& aSD)
      : mAllocator(aAllocator), mSD(aSD) {}
  ~SurfaceDescriptorUserData() { DestroySurfaceDescriptor(mAllocator, &mSD); }

  RefPtr<RemoteMediaManagerChild> mAllocator;
  SurfaceDescriptor mSD;
};

void DeleteSurfaceDescriptorUserData(void* aClosure) {
  SurfaceDescriptorUserData* sd =
      reinterpret_cast<SurfaceDescriptorUserData*>(aClosure);
  delete sd;
}

already_AddRefed<SourceSurface> RemoteMediaManagerChild::Readback(
    const SurfaceDescriptorGPUVideo& aSD) {
  // We can't use NS_DispatchAndSpinEventLoopUntilComplete here since that will
  // spin the event loop while it waits. This function can be called from JS and
  // we don't want that to happen.
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    return nullptr;
  }

  SurfaceDescriptor sd;
  RefPtr<Runnable> task =
      NS_NewRunnableFunction("RemoteMediaManagerChild::Readback", [&]() {
        if (CanSend()) {
          SendReadback(aSD, &sd);
        }
      });
  SyncRunnable::DispatchToThread(managerThread, task);

  if (!IsSurfaceDescriptorValid(sd)) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> source = GetSurfaceForDescriptor(sd);
  if (!source) {
    DestroySurfaceDescriptor(this, &sd);
    NS_WARNING("Failed to map SurfaceDescriptor in Readback");
    return nullptr;
  }

  static UserDataKey sSurfaceDescriptor;
  source->AddUserData(&sSurfaceDescriptor,
                      new SurfaceDescriptorUserData(this, sd),
                      DeleteSurfaceDescriptorUserData);

  return source.forget();
}

already_AddRefed<Image> RemoteMediaManagerChild::TransferToImage(
    const SurfaceDescriptorGPUVideo& aSD, const IntSize& aSize,
    const ColorDepth& aColorDepth, YUVColorSpace aYUVColorSpace,
    ColorSpace2 aColorPrimaries, TransferFunction aTransferFunction,
    ColorRange aColorRange) {
  // The Image here creates a TextureData object that takes ownership
  // of the SurfaceDescriptor, and is responsible for making sure that
  // it gets deallocated.
  SurfaceDescriptorGPUVideo sd(aSD);
  sd.get_SurfaceDescriptorRemoteDecoder().source() =
      Some(GetVideoBridgeSourceFromRemoteMediaIn(mLocation));
  return MakeAndAddRef<GPUVideoImage>(this, sd, aSize, aColorDepth,
                                      aYUVColorSpace, aColorPrimaries,
                                      aTransferFunction, aColorRange);
}

void RemoteMediaManagerChild::DeallocateSurfaceDescriptor(
    const SurfaceDescriptorGPUVideo& aSD) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    return;
  }
  MOZ_ALWAYS_SUCCEEDS(managerThread->Dispatch(NS_NewRunnableFunction(
      "RemoteMediaManagerChild::DeallocateSurfaceDescriptor",
      [ref = RefPtr{this}, sd = aSD]() {
        if (ref->CanSend()) {
          ref->SendDeallocateSurfaceDescriptorGPUVideo(sd);
        }
      })));
}

/* static */ void RemoteMediaManagerChild::HandleRejectionError(
    const RemoteMediaManagerChild* aDyingManager, RemoteMediaIn aLocation,
    const ipc::ResponseRejectReason& aReason,
    std::function<void(const MediaResult&)>&& aCallback) {
  // If the channel goes down and CanSend() returns false, the IPDL promise will
  // be rejected with SendError rather than ActorDestroyed. Both means the same
  // thing and we can consider that the parent has crashed. The child can no
  // longer be used.

  if (aLocation == RemoteMediaIn::GpuProcess) {
    // The GPU process will get automatically restarted by the parent process.
    // Once it has been restarted the ContentChild will receive the message and
    // will call GetManager()->InitForGPUProcess.
    // We defer reporting an error until we've recreated the RemoteDecoder
    // manager so that it'll be safe for MediaFormatReader to recreate decoders
    RunWhenGPUProcessRecreated(
        aDyingManager,
        NS_NewRunnableFunction(
            "RemoteMediaManagerChild::HandleRejectionError",
            [callback = std::move(aCallback)]() {
              MediaResult error(
                  NS_ERROR_DOM_MEDIA_REMOTE_CRASHED_RDD_OR_GPU_ERR, __func__);
              callback(error);
            }));
    return;
  }

  nsresult err = NS_ERROR_DOM_MEDIA_REMOTE_CRASHED_UTILITY_ERR;
  if (aLocation == RemoteMediaIn::RddProcess) {
    err = NS_ERROR_DOM_MEDIA_REMOTE_CRASHED_RDD_OR_GPU_ERR;
  } else if (aLocation == RemoteMediaIn::UtilityProcess_MFMediaEngineCDM) {
    err = NS_ERROR_DOM_MEDIA_REMOTE_CRASHED_MF_CDM_ERR;
  }
  // The RDD/utility process is restarted on demand and asynchronously, we can
  // immediately inform the caller that a new en/decoder is needed. The process
  // will then be restarted during the new en/decoder creation by
  aCallback(MediaResult(err, __func__));
}

void RemoteMediaManagerChild::HandleFatalError(const char* aMsg) {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherChildID());
}

void RemoteMediaManagerChild::SetSupported(
    RemoteMediaIn aLocation, const media::MediaCodecsSupported& aSupported) {
  switch (aLocation) {
    case RemoteMediaIn::GpuProcess:
    case RemoteMediaIn::RddProcess:
    case RemoteMediaIn::UtilityProcess_AppleMedia:
    case RemoteMediaIn::UtilityProcess_Generic:
    case RemoteMediaIn::UtilityProcess_WMF:
    case RemoteMediaIn::UtilityProcess_MFMediaEngineCDM: {
      StaticMutexAutoLock lock(sProcessSupportedMutex);
      sProcessSupported[aLocation] = Some(aSupported);
      break;
    }
    default:
      MOZ_CRASH("Not to be used for any other process");
  }
}

#undef LOG

}  // namespace mozilla
