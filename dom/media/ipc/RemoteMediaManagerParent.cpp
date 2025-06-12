/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteMediaManagerParent.h"

#if XP_WIN
#  include <objbase.h>
#endif

#include "ImageContainer.h"
#include "PDMFactory.h"
#include "RemoteAudioDecoder.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"  // for MediaThreadType
#include "mozilla/RDDParent.h"
#include "mozilla/RemoteDecodeUtils.h"
#include "mozilla/ipc/UtilityProcessChild.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/VideoBridgeChild.h"
#include "mozilla/layers/VideoBridgeParent.h"
#include "nsIObserverService.h"

#ifdef MOZ_WMF_MEDIA_ENGINE
#  include "MFMediaEngineParent.h"
#endif

#ifdef MOZ_WMF_CDM
#  include "MFCDMParent.h"
#endif

namespace mozilla {

#define LOG(msg, ...) \
  MOZ_LOG(gRemoteDecodeLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

using namespace ipc;
using namespace layers;
using namespace gfx;

StaticRefPtr<TaskQueue> sRemoteMediaManagerParentThread;

void RemoteMediaManagerParent::StoreImage(const SurfaceDescriptorGPUVideo& aSD,
                                          Image* aImage,
                                          TextureClient* aTexture) {
  MOZ_ASSERT(OnManagerThread());
  mImageMap[static_cast<SurfaceDescriptorRemoteDecoder>(aSD).handle()] = aImage;
  mTextureMap[static_cast<SurfaceDescriptorRemoteDecoder>(aSD).handle()] =
      aTexture;
}

class RemoteDecoderManagerThreadShutdownObserver : public nsIObserver {
  virtual ~RemoteDecoderManagerThreadShutdownObserver() = default;

 public:
  RemoteDecoderManagerThreadShutdownObserver() = default;

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    RemoteMediaManagerParent::ShutdownVideoBridge();
    RemoteMediaManagerParent::ShutdownThreads();
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(RemoteDecoderManagerThreadShutdownObserver, nsIObserver);

bool RemoteMediaManagerParent::StartupThreads() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sRemoteMediaManagerParentThread) {
    return true;
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return false;
  }

  sRemoteMediaManagerParentThread = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR), "RemVidParent");
  if (XRE_IsGPUProcess()) {
    MOZ_ALWAYS_SUCCEEDS(
        sRemoteMediaManagerParentThread->Dispatch(NS_NewRunnableFunction(
            "RemoteMediaManagerParent::StartupThreads",
            []() { layers::VideoBridgeChild::StartupForGPUProcess(); })));
  }

  auto* obs = new RemoteDecoderManagerThreadShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  return true;
}

void RemoteMediaManagerParent::ShutdownThreads() {
  sRemoteMediaManagerParentThread->BeginShutdown();
  sRemoteMediaManagerParentThread->AwaitShutdownAndIdle();
  sRemoteMediaManagerParentThread = nullptr;
}

/* static */
void RemoteMediaManagerParent::ShutdownVideoBridge() {
  if (sRemoteMediaManagerParentThread) {
    RefPtr<Runnable> task =
        NS_NewRunnableFunction("RemoteMediaManagerParent::ShutdownVideoBridge",
                               []() { VideoBridgeChild::Shutdown(); });
    SyncRunnable::DispatchToThread(sRemoteMediaManagerParentThread, task);
  }
}

bool RemoteMediaManagerParent::OnManagerThread() {
  return sRemoteMediaManagerParentThread->IsOnCurrentThread();
}

PDMFactory& RemoteMediaManagerParent::EnsurePDMFactory() {
  MOZ_ASSERT(OnManagerThread());
  if (!mPDMFactory) {
    mPDMFactory = MakeRefPtr<PDMFactory>();
  }
  return *mPDMFactory;
}

bool RemoteMediaManagerParent::CreateForContent(
    Endpoint<PRemoteMediaManagerParent>&& aEndpoint,
    dom::ContentParentId aChildId) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_RDD ||
             XRE_GetProcessType() == GeckoProcessType_Utility ||
             XRE_GetProcessType() == GeckoProcessType_GPU);
  MOZ_ASSERT(NS_IsMainThread());

  if (!StartupThreads()) {
    return false;
  }

  RefPtr<RemoteMediaManagerParent> parent =
      new RemoteMediaManagerParent(sRemoteMediaManagerParentThread, aChildId);

  RefPtr<Runnable> task =
      NewRunnableMethod<Endpoint<PRemoteMediaManagerParent>&&>(
          "dom::RemoteMediaManagerParent::Open", parent,
          &RemoteMediaManagerParent::Open, std::move(aEndpoint));
  MOZ_ALWAYS_SUCCEEDS(sRemoteMediaManagerParentThread->Dispatch(task.forget()));
  return true;
}

bool RemoteMediaManagerParent::CreateVideoBridgeToOtherProcess(
    Endpoint<PVideoBridgeChild>&& aEndpoint) {
  LOG("Create video bridge");
  // We never want to decode in the GPU process, but output
  // frames to the parent process.
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_RDD ||
             XRE_GetProcessType() == GeckoProcessType_Utility);
#ifdef MOZ_WMF_MEDIA_ENGINE
  MOZ_ASSERT_IF(
      XRE_GetProcessType() == GeckoProcessType_Utility,
      GetCurrentSandboxingKind() == SandboxingKind::MF_MEDIA_ENGINE_CDM);
#endif
  MOZ_ASSERT(NS_IsMainThread());

  if (!StartupThreads()) {
    return false;
  }

  RefPtr<Runnable> task =
      NewRunnableFunction("gfx::VideoBridgeChild::Open",
                          &VideoBridgeChild::Open, std::move(aEndpoint));
  MOZ_ALWAYS_SUCCEEDS(sRemoteMediaManagerParentThread->Dispatch(task.forget()));
  return true;
}

RemoteMediaManagerParent::RemoteMediaManagerParent(
    nsISerialEventTarget* aThread, dom::ContentParentId aContentId)
    : mThread(aThread), mContentId(aContentId) {
  MOZ_COUNT_CTOR(RemoteMediaManagerParent);
  auto& registrar =
      XRE_IsGPUProcess() ? GPUParent::GetSingleton()->AsyncShutdownService()
      : XRE_IsUtilityProcess()
          ? UtilityProcessChild::GetSingleton()->AsyncShutdownService()
          : RDDParent::GetSingleton()->AsyncShutdownService();
  registrar.Register(this);
}

RemoteMediaManagerParent::~RemoteMediaManagerParent() {
  MOZ_COUNT_DTOR(RemoteMediaManagerParent);
  auto& registrar =
      XRE_IsGPUProcess() ? GPUParent::GetSingleton()->AsyncShutdownService()
      : XRE_IsUtilityProcess()
          ? UtilityProcessChild::GetSingleton()->AsyncShutdownService()
          : RDDParent::GetSingleton()->AsyncShutdownService();
  registrar.Deregister(this);
}

void RemoteMediaManagerParent::ActorDestroy(
    mozilla::ipc::IProtocol::ActorDestroyReason) {
  mThread = nullptr;
}

PRemoteDecoderParent* RemoteMediaManagerParent::AllocPRemoteDecoderParent(
    const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
    const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
    const Maybe<uint64_t>& aMediaEngineId,
    const Maybe<TrackingId>& aTrackingId) {
  RefPtr<TaskQueue> decodeTaskQueue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "RemoteVideoDecoderParent::mDecodeTaskQueue");

  if (aRemoteDecoderInfo.type() ==
      RemoteDecoderInfoIPDL::TVideoDecoderInfoIPDL) {
    const VideoDecoderInfoIPDL& decoderInfo =
        aRemoteDecoderInfo.get_VideoDecoderInfoIPDL();
    return new RemoteVideoDecoderParent(
        this, decoderInfo.videoInfo(), decoderInfo.framerate(), aOptions,
        aIdentifier, sRemoteMediaManagerParentThread, decodeTaskQueue,
        aMediaEngineId, aTrackingId);
  }

  if (aRemoteDecoderInfo.type() == RemoteDecoderInfoIPDL::TAudioInfo) {
    return new RemoteAudioDecoderParent(
        this, aRemoteDecoderInfo.get_AudioInfo(), aOptions,
        sRemoteMediaManagerParentThread, decodeTaskQueue, aMediaEngineId);
  }

  MOZ_CRASH("unrecognized type of RemoteDecoderInfoIPDL union");
  return nullptr;
}

bool RemoteMediaManagerParent::DeallocPRemoteDecoderParent(
    PRemoteDecoderParent* actor) {
  RemoteDecoderParent* parent = static_cast<RemoteDecoderParent*>(actor);
  parent->Destroy();
  return true;
}

PMFMediaEngineParent* RemoteMediaManagerParent::AllocPMFMediaEngineParent() {
#ifdef MOZ_WMF_MEDIA_ENGINE
  return new MFMediaEngineParent(this, sRemoteMediaManagerParentThread);
#else
  return nullptr;
#endif
}

bool RemoteMediaManagerParent::DeallocPMFMediaEngineParent(
    PMFMediaEngineParent* actor) {
#ifdef MOZ_WMF_MEDIA_ENGINE
  MFMediaEngineParent* parent = static_cast<MFMediaEngineParent*>(actor);
  parent->Destroy();
#endif
  return true;
}

PMFCDMParent* RemoteMediaManagerParent::AllocPMFCDMParent(
    const nsAString& aKeySystem) {
#ifdef MOZ_WMF_CDM
  return new MFCDMParent(aKeySystem, this, sRemoteMediaManagerParentThread);
#else
  return nullptr;
#endif
}

bool RemoteMediaManagerParent::DeallocPMFCDMParent(PMFCDMParent* actor) {
#ifdef MOZ_WMF_CDM
  static_cast<MFCDMParent*>(actor)->Destroy();
#endif
  return true;
}

void RemoteMediaManagerParent::Open(
    Endpoint<PRemoteMediaManagerParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind RemoteMediaManagerParent to endpoint");
  }
}

mozilla::ipc::IPCResult RemoteMediaManagerParent::RecvReadback(
    const SurfaceDescriptorGPUVideo& aSD, SurfaceDescriptor* aResult) {
  const SurfaceDescriptorRemoteDecoder& sd = aSD;
  RefPtr<Image> image = mImageMap[sd.handle()];
  if (!image) {
    *aResult = null_t();
    return IPC_OK();
  }

  // Read directly into the shmem to avoid extra copies, if possible.
  SurfaceDescriptorBuffer sdb;
  nsresult rv = image->BuildSurfaceDescriptorBuffer(
      sdb, Image::BuildSdbFlags::RgbOnly, [&](uint32_t aBufferSize) {
        Shmem buffer;
        if (!AllocShmem(aBufferSize, &buffer)) {
          return MemoryOrShmem();
        }
        return MemoryOrShmem(std::move(buffer));
      });

  if (NS_SUCCEEDED(rv)) {
    *aResult = std::move(sdb);
    return IPC_OK();
  }

  if (sdb.data().type() == MemoryOrShmem::TShmem) {
    DeallocShmem(sdb.data().get_Shmem());
  }
  *aResult = null_t();
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemoteMediaManagerParent::RecvDeallocateSurfaceDescriptorGPUVideo(
    const SurfaceDescriptorGPUVideo& aSD) {
  MOZ_ASSERT(OnManagerThread());
  const SurfaceDescriptorRemoteDecoder& sd = aSD;
  mImageMap.erase(sd.handle());
  mTextureMap.erase(sd.handle());
  return IPC_OK();
}

void RemoteMediaManagerParent::DeallocateSurfaceDescriptor(
    const SurfaceDescriptorGPUVideo& aSD) {
  if (!OnManagerThread()) {
    MOZ_ALWAYS_SUCCEEDS(
        sRemoteMediaManagerParentThread->Dispatch(NS_NewRunnableFunction(
            "RemoteMediaManagerParent::DeallocateSurfaceDescriptor",
            [ref = RefPtr{this}, sd = aSD]() {
              ref->RecvDeallocateSurfaceDescriptorGPUVideo(sd);
            })));
  } else {
    RecvDeallocateSurfaceDescriptorGPUVideo(aSD);
  }
}

#undef LOG

}  // namespace mozilla
