/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_PARENT_H_
#define WEBGPU_PARENT_H_

#include <unordered_map>

#include "mozilla/WeakPtr.h"
#include "mozilla/ipc/SharedMemoryHandle.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/PWebGPUParent.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "WebGPUTypes.h"
#include "base/timer.h"

namespace mozilla {

namespace layers {
class RemoteTextureOwnerClient;
}  // namespace layers

namespace webgpu {

class ErrorBuffer;
class ExternalTexture;
class PresentationData;

// Destroy/Drop messages:
// - Messages with "Destroy" in their name request deallocation of resources
// owned by the
//   object and put the object in a destroyed state without deleting the object.
//   It is still safe to reffer to these objects.
// - Messages with "Drop" in their name can be thought of as C++ destructors.
// They completely
//   delete the object, so future attempts at accessing to these objects will
//   crash. The child process should *never* send a Drop message if it still
//   holds references to the object. An object that has been destroyed still
//   needs to be dropped when the last reference to it dies on the child
//   process.

class WebGPUParent final : public PWebGPUParent, public SupportsWeakPtr {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGPUParent, override)

 public:
  explicit WebGPUParent();

  ipc::IPCResult RecvInstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions, RawId aAdapterId,
      InstanceRequestAdapterResolver&& resolver);
  ipc::IPCResult RecvAdapterRequestDevice(
      RawId aAdapterId, const ipc::ByteBuf& aByteBuf, RawId aDeviceId,
      RawId aQueueId, AdapterRequestDeviceResolver&& resolver);
  ipc::IPCResult RecvAdapterDrop(RawId aAdapterId);
  ipc::IPCResult RecvDeviceDestroy(RawId aDeviceId);
  ipc::IPCResult RecvDeviceDrop(RawId aDeviceId);
  ipc::IPCResult RecvDeviceCreateBuffer(
      RawId aDeviceId, RawId aBufferId, dom::GPUBufferDescriptor&& aDesc,
      ipc::MutableSharedMemoryHandle&& aShmem);
  ipc::IPCResult RecvBufferMap(RawId aDeviceId, RawId aBufferId, uint32_t aMode,
                               uint64_t aOffset, uint64_t size,
                               BufferMapResolver&& aResolver);
  ipc::IPCResult RecvBufferUnmap(RawId aDeviceId, RawId aBufferId, bool aFlush);
  ipc::IPCResult RecvBufferDestroy(RawId aBufferId);
  ipc::IPCResult RecvBufferDrop(RawId aBufferId);
  ipc::IPCResult RecvTextureDestroy(RawId aTextureId, RawId aDeviceId);
  ipc::IPCResult RecvTextureDrop(RawId aTextureId);
  ipc::IPCResult RecvTextureViewDrop(RawId aTextureViewId);
  ipc::IPCResult RecvSamplerDrop(RawId aSamplerId);
  ipc::IPCResult RecvQuerySetDrop(RawId aQuerySetId);
  ipc::IPCResult RecvCommandEncoderFinish(
      RawId aEncoderId, RawId aDeviceId,
      const dom::GPUCommandBufferDescriptor& aDesc);
  ipc::IPCResult RecvCommandEncoderDrop(RawId aEncoderId);
  ipc::IPCResult RecvCommandBufferDrop(RawId aCommandBufferId);
  ipc::IPCResult RecvRenderBundleDrop(RawId aBundleId);
  ipc::IPCResult RecvQueueSubmit(RawId aQueueId, RawId aDeviceId,
                                 const nsTArray<RawId>& aCommandBuffers,
                                 const nsTArray<RawId>& aTextureIds);
  ipc::IPCResult RecvQueueOnSubmittedWorkDone(
      RawId aQueueId, std::function<void(mozilla::void_t)>&& aResolver);
  ipc::IPCResult RecvQueueWriteAction(RawId aQueueId, RawId aDeviceId,
                                      const ipc::ByteBuf& aByteBuf,
                                      ipc::MutableSharedMemoryHandle&& aShmem);
  ipc::IPCResult RecvBindGroupLayoutDrop(RawId aBindGroupLayoutId);
  ipc::IPCResult RecvPipelineLayoutDrop(RawId aPipelineLayoutId);
  ipc::IPCResult RecvBindGroupDrop(RawId aBindGroupId);
  ipc::IPCResult RecvShaderModuleDrop(RawId aModuleId);
  ipc::IPCResult RecvComputePipelineDrop(RawId aPipelineId);
  ipc::IPCResult RecvRenderPipelineDrop(RawId aPipelineId);
  ipc::IPCResult RecvImplicitLayoutDrop(RawId aImplicitPlId,
                                        const nsTArray<RawId>& aImplicitBglIds);
  ipc::IPCResult RecvDeviceCreateSwapChain(
      RawId aDeviceId, RawId aQueueId, const layers::RGBDescriptor& aDesc,
      const nsTArray<RawId>& aBufferIds,
      const layers::RemoteTextureOwnerId& aOwnerId,
      bool aUseExternalTextureInSwapChain);
  ipc::IPCResult RecvDeviceCreateShaderModule(
      RawId aDeviceId, RawId aModuleId, const nsString& aLabel,
      const nsCString& aCode, DeviceCreateShaderModuleResolver&& aOutMessage);

  ipc::IPCResult RecvSwapChainPresent(
      RawId aTextureId, RawId aCommandEncoderId,
      const layers::RemoteTextureId& aRemoteTextureId,
      const layers::RemoteTextureOwnerId& aOwnerId);
  ipc::IPCResult RecvSwapChainDrop(const layers::RemoteTextureOwnerId& aOwnerId,
                                   layers::RemoteTextureTxnType aTxnType,
                                   layers::RemoteTextureTxnId aTxnId);

  ipc::IPCResult RecvDeviceAction(RawId aDeviceId,
                                  const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvDeviceActionWithAck(
      RawId aDeviceId, const ipc::ByteBuf& aByteBuf,
      DeviceActionWithAckResolver&& aResolver);
  ipc::IPCResult RecvTextureAction(RawId aTextureId, RawId aDevice,
                                   const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvCommandEncoderAction(RawId aEncoderId, RawId aDeviceId,
                                          const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvRenderPass(RawId aEncoderId, RawId aDeviceId,
                                const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvComputePass(RawId aEncoderId, RawId aDeviceId,
                                 const ipc::ByteBuf& aByteBuf);

  ipc::IPCResult RecvDevicePushErrorScope(RawId aDeviceId, dom::GPUErrorFilter);
  ipc::IPCResult RecvDevicePopErrorScope(
      RawId aDeviceId, DevicePopErrorScopeResolver&& aResolver);

  ipc::IPCResult GetFrontBufferSnapshot(
      IProtocol* aProtocol, const layers::RemoteTextureOwnerId& aOwnerId,
      const RawId& aCommandEncoderId, Maybe<Shmem>& aShmem, gfx::IntSize& aSize,
      uint32_t& aByteStride);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  struct BufferMapData {
    ipc::SharedMemoryMapping mShmem;
    // True if buffer's usage has MAP_READ or MAP_WRITE set.
    bool mHasMapFlags;
    uint64_t mMappedOffset;
    uint64_t mMappedSize;
    RawId mDeviceId;
  };

  BufferMapData* GetBufferMapData(RawId aBufferId);

  bool UseExternalTextureForSwapChain(ffi::WGPUSwapChainId aSwapChainId);

  void DisableExternalTextureForSwapChain(ffi::WGPUSwapChainId aSwapChainId);

  bool EnsureExternalTextureForSwapChain(ffi::WGPUSwapChainId aSwapChainId,
                                         ffi::WGPUDeviceId aDeviceId,
                                         ffi::WGPUTextureId aTextureId,
                                         uint32_t aWidth, uint32_t aHeight,
                                         struct ffi::WGPUTextureFormat aFormat,
                                         ffi::WGPUTextureUsages aUsage);

  void EnsureExternalTextureForReadBackPresent(
      ffi::WGPUSwapChainId aSwapChainId, ffi::WGPUDeviceId aDeviceId,
      ffi::WGPUTextureId aTextureId, uint32_t aWidth, uint32_t aHeight,
      struct ffi::WGPUTextureFormat aFormat, ffi::WGPUTextureUsages aUsage);

  std::shared_ptr<ExternalTexture> CreateExternalTexture(
      const layers::RemoteTextureOwnerId& aOwnerId, ffi::WGPUDeviceId aDeviceId,
      ffi::WGPUTextureId aTextureId, uint32_t aWidth, uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      ffi::WGPUTextureUsages aUsage);

  std::shared_ptr<ExternalTexture> GetExternalTexture(ffi::WGPUTextureId aId);

  void PostExternalTexture(
      const std::shared_ptr<ExternalTexture>&& aExternalTexture,
      const layers::RemoteTextureId aRemoteTextureId,
      const layers::RemoteTextureOwnerId aOwnerId);

  bool ForwardError(const RawId aDeviceId, ErrorBuffer& aError) {
    return ForwardError(Some(aDeviceId), aError);
  }

  ffi::WGPUGlobal* GetContext() const { return mContext.get(); }

  bool IsDeviceActive(const RawId aDeviceId) {
    return mActiveDeviceIds.Contains(aDeviceId);
  }

  RefPtr<gfx::FileHandleWrapper> GetDeviceFenceHandle(const RawId aDeviceId);

 private:
  static void MapCallback(uint8_t* aUserData,
                          ffi::WGPUBufferMapAsyncStatus aStatus);
  static void DeviceLostCallback(uint8_t* aUserData, uint8_t aReason,
                                 const char* aMessage);
  void DeallocBufferShmem(RawId aBufferId);

  void RemoveExternalTexture(RawId aTextureId);

  virtual ~WebGPUParent();
  void MaintainDevices();
  void LoseDevice(const RawId aDeviceId, Maybe<uint8_t> aReason,
                  const nsACString& aMessage);

  bool ForwardError(Maybe<RawId> aDeviceId, ErrorBuffer& aError);

  void ReportError(Maybe<RawId> aDeviceId, GPUErrorFilter,
                   const nsCString& message);

  static Maybe<ffi::WGPUFfiLUID> GetCompositorDeviceLuid();

  UniquePtr<ffi::WGPUGlobal> mContext;
  base::RepeatingTimer<WebGPUParent> mTimer;

  /// A map from wgpu buffer ids to data about their shared memory segments.
  /// Includes entries about mappedAtCreation, MAP_READ and MAP_WRITE buffers,
  /// regardless of their state.
  std::unordered_map<uint64_t, BufferMapData> mSharedMemoryMap;
  /// Associated presentation data for each swapchain.
  std::unordered_map<layers::RemoteTextureOwnerId, RefPtr<PresentationData>,
                     layers::RemoteTextureOwnerId::HashFn>
      mPresentationDataMap;

  RefPtr<layers::RemoteTextureOwnerClient> mRemoteTextureOwner;

  /// Associated stack of error scopes for each device.
  std::unordered_map<uint64_t, std::vector<ErrorScope>>
      mErrorScopeStackByDevice;

  std::unordered_map<ffi::WGPUTextureId, std::shared_ptr<ExternalTexture>>
      mExternalTextures;

  // Store a set of DeviceIds that have been SendDeviceLost. We use this to
  // limit each Device to one DeviceLost message.
  nsTHashSet<RawId> mLostDeviceIds;

  // Store active DeviceIds
  nsTHashSet<RawId> mActiveDeviceIds;

  // Shared handle of wgpu device's fence.
  std::unordered_map<RawId, RefPtr<gfx::FileHandleWrapper>> mDeviceFenceHandles;
};

#if defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID)
class VkImageHandle {
 public:
  explicit VkImageHandle(WebGPUParent* aParent,
                         const ffi::WGPUDeviceId aDeviceId,
                         ffi::WGPUVkImageHandle* aVkImageHandle)
      : mParent(aParent),
        mDeviceId(aDeviceId),
        mVkImageHandle(aVkImageHandle) {}

  const ffi::WGPUVkImageHandle* Get() { return mVkImageHandle; }

  ~VkImageHandle();

 protected:
  const WeakPtr<WebGPUParent> mParent;
  const RawId mDeviceId;
  ffi::WGPUVkImageHandle* mVkImageHandle;
};

class VkSemaphoreHandle {
 public:
  explicit VkSemaphoreHandle(WebGPUParent* aParent,
                             const ffi::WGPUDeviceId aDeviceId,
                             ffi::WGPUVkSemaphoreHandle* aVkSemaphoreHandle)
      : mParent(aParent),
        mDeviceId(aDeviceId),
        mVkSemaphoreHandle(aVkSemaphoreHandle) {}

  const ffi::WGPUVkSemaphoreHandle* Get() { return mVkSemaphoreHandle; }

  ~VkSemaphoreHandle();

 protected:
  const WeakPtr<WebGPUParent> mParent;
  const RawId mDeviceId;
  ffi::WGPUVkSemaphoreHandle* mVkSemaphoreHandle;
};
#endif

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_PARENT_H_
