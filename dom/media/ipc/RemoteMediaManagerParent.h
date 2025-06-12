/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteMediaManagerParent_h
#define include_dom_media_ipc_RemoteMediaManagerParent_h

#include "GPUVideoImage.h"
#include "mozilla/PRemoteMediaManagerParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layers/VideoBridgeChild.h"

namespace mozilla {

class PDMFactory;
class PMFCDMParent;
class PMFMediaEngineParent;

class RemoteMediaManagerParent final : public PRemoteMediaManagerParent,
                                       public layers::IGPUVideoSurfaceManager {
  friend class PRemoteMediaManagerParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteMediaManagerParent, override)

  static bool CreateForContent(Endpoint<PRemoteMediaManagerParent>&& aEndpoint,
                               dom::ContentParentId aContentId);

  static bool CreateVideoBridgeToOtherProcess(
      Endpoint<layers::PVideoBridgeChild>&& aEndpoint);

  // Must be called on manager thread.
  // Store the image so that it can be used out of process. Will be released
  // when DeallocateSurfaceDescriptor is called.
  void StoreImage(const SurfaceDescriptorGPUVideo& aSD, layers::Image* aImage,
                  layers::TextureClient* aTexture);

  // IGPUVideoSurfaceManager methods
  already_AddRefed<gfx::SourceSurface> Readback(
      const SurfaceDescriptorGPUVideo& aSD) override {
    MOZ_ASSERT_UNREACHABLE("Not usable from the parent");
    return nullptr;
  }
  already_AddRefed<layers::Image> TransferToImage(
      const SurfaceDescriptorGPUVideo& aSD, const gfx::IntSize& aSize,
      const gfx::ColorDepth& aColorDepth, gfx::YUVColorSpace aYUVColorSpace,
      gfx::ColorSpace2 aColorPrimaries, gfx::TransferFunction aTransferFunction,
      gfx::ColorRange aColorRange) override;
  void DeallocateSurfaceDescriptor(
      const SurfaceDescriptorGPUVideo& aSD) override;

  static bool StartupThreads();
  static void ShutdownThreads();

  static void ShutdownVideoBridge();

  bool OnManagerThread();

  // Can be called from manager thread only
  PDMFactory& EnsurePDMFactory();

  const dom::ContentParentId& GetContentId() const { return mContentId; }

 protected:
  PRemoteDecoderParent* AllocPRemoteDecoderParent(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions,
      const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
      const Maybe<uint64_t>& aMediaEngineId,
      const Maybe<TrackingId>& aTrackingId);
  bool DeallocPRemoteDecoderParent(PRemoteDecoderParent* actor);

  PMFMediaEngineParent* AllocPMFMediaEngineParent();
  bool DeallocPMFMediaEngineParent(PMFMediaEngineParent* actor);

  PMFCDMParent* AllocPMFCDMParent(const nsAString& aKeySystem);
  bool DeallocPMFCDMParent(PMFCDMParent* actor);

  mozilla::ipc::IPCResult RecvReadback(const SurfaceDescriptorGPUVideo& aSD,
                                       SurfaceDescriptor* aResult);
  mozilla::ipc::IPCResult RecvDeallocateSurfaceDescriptorGPUVideo(
      const SurfaceDescriptorGPUVideo& aSD);

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override;

 private:
  RemoteMediaManagerParent(nsISerialEventTarget* aThread,
                           dom::ContentParentId aContentId);
  ~RemoteMediaManagerParent();

  void Open(Endpoint<PRemoteMediaManagerParent>&& aEndpoint);

  std::map<uint64_t, RefPtr<layers::Image>> mImageMap;
  std::map<uint64_t, RefPtr<layers::TextureClient>> mTextureMap;

  nsCOMPtr<nsISerialEventTarget> mThread;
  RefPtr<PDMFactory> mPDMFactory;
  dom::ContentParentId mContentId;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteMediaManagerParent_h
