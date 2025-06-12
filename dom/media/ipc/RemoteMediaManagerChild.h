/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteMediaManagerChild_h
#define include_dom_media_ipc_RemoteMediaManagerChild_h

#include <functional>

#include "GPUVideoImage.h"
#include "PDMFactory.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/PRemoteMediaManagerChild.h"
#include "mozilla/layers/VideoBridgeUtils.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"

namespace mozilla {

class PMFCDMChild;
class PMFMediaEngineChild;
class RemoteDecoderChild;

enum class RemoteMediaIn {
  Unspecified,
  RddProcess,
  GpuProcess,
  UtilityProcess_Generic,
  UtilityProcess_AppleMedia,
  UtilityProcess_WMF,
  UtilityProcess_MFMediaEngineCDM,
  SENTINEL,
};

enum class TrackSupport {
  None,
  Audio,
  Video,
};
using TrackSupportSet = EnumSet<TrackSupport, uint8_t>;

class RemoteMediaManagerChild final
    : public PRemoteMediaManagerChild,
      public mozilla::ipc::IShmemAllocator,
      public mozilla::layers::IGPUVideoSurfaceManager {
  friend class PRemoteMediaManagerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteMediaManagerChild, override)

  // Can only be called from the manager thread
  static RemoteMediaManagerChild* GetSingleton(RemoteMediaIn aLocation);

  static void Init();
  static void SetSupported(RemoteMediaIn aLocation,
                           const media::MediaCodecsSupported& aSupported);

  // Can be called from any thread.
  static bool Supports(RemoteMediaIn aLocation,
                       const SupportDecoderParams& aParams,
                       DecoderDoctorDiagnostics* aDiagnostics);
  static RefPtr<PlatformDecoderModule::CreateDecoderPromise> CreateAudioDecoder(
      const CreateDecoderParams& aParams, RemoteMediaIn aLocation);
  static RefPtr<PlatformDecoderModule::CreateDecoderPromise> CreateVideoDecoder(
      const CreateDecoderParams& aParams, RemoteMediaIn aLocation);

  // Can be called from any thread.
  static nsISerialEventTarget* GetManagerThread();

  // Return the track support information based on the location of the remote
  // process. Thread-safe.
  static TrackSupportSet GetTrackSupport(RemoteMediaIn aLocation);

  // Can be called from any thread, dispatches the request to the IPDL thread
  // internally and will be ignored if the IPDL actor has been destroyed.
  already_AddRefed<gfx::SourceSurface> Readback(
      const SurfaceDescriptorGPUVideo& aSD) override;
  void DeallocateSurfaceDescriptor(
      const SurfaceDescriptorGPUVideo& aSD) override;

  bool AllocShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override {
    return PRemoteMediaManagerChild::AllocShmem(aSize, aShmem);
  }
  bool AllocUnsafeShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override {
    return PRemoteMediaManagerChild::AllocUnsafeShmem(aSize, aShmem);
  }

  // Can be called from any thread, dispatches the request to the IPDL thread
  // internally and will be ignored if the IPDL actor has been destroyed.
  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  // Main thread only
  static void InitForGPUProcess(
      Endpoint<PRemoteMediaManagerChild>&& aVideoManager);
  static void Shutdown();

  // Helper method to handle IPDL promise rejections. This will allow the
  // caller in the layers above to recover gracefully by recreating the encoder
  // or decoder.
  static void HandleRejectionError(
      const RemoteMediaManagerChild* aDyingManager, RemoteMediaIn aLocation,
      const mozilla::ipc::ResponseRejectReason& aReason,
      std::function<void(const MediaResult&)>&& aCallback);

  // Run aTask (on the manager thread) when we next attempt to create a new
  // manager (even if creation fails). Intended to be called from ActorDestroy
  // when we get notified that the old manager is being destroyed. Can only be
  // called from the manager thread.
  static void RunWhenGPUProcessRecreated(
      const RemoteMediaManagerChild* aDyingManager,
      already_AddRefed<Runnable> aTask);

  RemoteMediaIn Location() const { return mLocation; }

  // A thread-safe method to launch the utility process if it hasn't launched
  // yet.
  static RefPtr<GenericNonExclusivePromise> LaunchUtilityProcessIfNeeded(
      RemoteMediaIn aLocation);

 protected:
  void HandleFatalError(const char* aMsg) override;

  PRemoteDecoderChild* AllocPRemoteDecoderChild(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions,
      const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
      const Maybe<uint64_t>& aMediaEngineId,
      const Maybe<TrackingId>& aTrackingId);
  bool DeallocPRemoteDecoderChild(PRemoteDecoderChild* actor);

  PMFMediaEngineChild* AllocPMFMediaEngineChild();
  bool DeallocPMFMediaEngineChild(PMFMediaEngineChild* actor);

  PMFCDMChild* AllocPMFCDMChild(const nsAString& aKeySystem);
  bool DeallocPMFCDMChild(PMFCDMChild* actor);

 private:
  explicit RemoteMediaManagerChild(RemoteMediaIn aLocation);
  ~RemoteMediaManagerChild() = default;
  static RefPtr<PlatformDecoderModule::CreateDecoderPromise> Construct(
      RefPtr<RemoteDecoderChild>&& aChild, RemoteMediaIn aLocation);

  static void OpenRemoteMediaManagerChildForProcess(
      Endpoint<PRemoteMediaManagerChild>&& aEndpoint, RemoteMediaIn aLocation);

  // A thread-safe method to launch the RDD process if it hasn't launched yet.
  static RefPtr<GenericNonExclusivePromise> LaunchRDDProcessIfNeeded();

  // The location for decoding, Rdd or Gpu process.
  const RemoteMediaIn mLocation;
};

}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::RemoteMediaIn>
    : public ContiguousEnumSerializer<mozilla::RemoteMediaIn,
                                      mozilla::RemoteMediaIn::Unspecified,
                                      mozilla::RemoteMediaIn::SENTINEL> {};
}  // namespace IPC

#endif  // include_dom_media_ipc_RemoteMediaManagerChild_h
