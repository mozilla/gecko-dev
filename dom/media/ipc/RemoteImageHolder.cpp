/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteImageHolder.h"

#include "GPUVideoImage.h"
#include "mozilla/PRemoteDecoderChild.h"
#include "mozilla/RemoteDecodeUtils.h"
#include "mozilla/RemoteMediaManagerChild.h"
#include "mozilla/RemoteMediaManagerParent.h"
#include "mozilla/gfx/SourceSurfaceRawData.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/VideoBridgeUtils.h"

namespace mozilla {

using namespace gfx;
using namespace layers;

RemoteImageHolder::RemoteImageHolder() = default;
RemoteImageHolder::RemoteImageHolder(layers::SurfaceDescriptor&& aSD)
    : mSD(Some(std::move(aSD))) {}
RemoteImageHolder::RemoteImageHolder(
    layers::IGPUVideoSurfaceManager* aManager,
    layers::VideoBridgeSource aSource, const gfx::IntSize& aSize,
    const gfx::ColorDepth& aColorDepth, const layers::SurfaceDescriptor& aSD,
    gfx::YUVColorSpace aYUVColorSpace, gfx::ColorSpace2 aColorPrimaries,
    gfx::TransferFunction aTransferFunction, gfx::ColorRange aColorRange)
    : mSource(aSource),
      mSize(aSize),
      mColorDepth(aColorDepth),
      mSD(Some(aSD)),
      mManager(aManager),
      mYUVColorSpace(aYUVColorSpace),
      mColorPrimaries(aColorPrimaries),
      mTransferFunction(aTransferFunction),
      mColorRange(aColorRange) {}

RemoteImageHolder::RemoteImageHolder(RemoteImageHolder&& aOther)
    : mSource(aOther.mSource),
      mSize(aOther.mSize),
      mColorDepth(aOther.mColorDepth),
      mSD(std::move(aOther.mSD)),
      mManager(aOther.mManager),
      mYUVColorSpace(aOther.mYUVColorSpace),
      mColorPrimaries(aOther.mColorPrimaries),
      mTransferFunction(aOther.mTransferFunction),
      mColorRange(aOther.mColorRange) {
  aOther.mSD = Nothing();
}

already_AddRefed<Image> RemoteImageHolder::DeserializeImage(
    layers::BufferRecycleBin* aBufferRecycleBin) {
  MOZ_ASSERT(mSD && mSD->type() == SurfaceDescriptor::TSurfaceDescriptorBuffer);
  if (!aBufferRecycleBin) {
    return nullptr;
  }

  const SurfaceDescriptorBuffer& sdBuffer = mSD->get_SurfaceDescriptorBuffer();
  const MemoryOrShmem& memOrShmem = sdBuffer.data();
  if (memOrShmem.type() != MemoryOrShmem::TShmem) {
    MOZ_ASSERT_UNREACHABLE("Unexpected MemoryOrShmem type");
    return nullptr;
  }

  // Note that the shmem will be recycled by the parent automatically.
  uint8_t* buffer = memOrShmem.get_Shmem().get<uint8_t>();
  if (!buffer) {
    return nullptr;
  }

  size_t bufferSize = memOrShmem.get_Shmem().Size<uint8_t>();

  if (sdBuffer.desc().type() == BufferDescriptor::TYCbCrDescriptor) {
    const YCbCrDescriptor& descriptor = sdBuffer.desc().get_YCbCrDescriptor();

    size_t descriptorSize = ImageDataSerializer::ComputeYCbCrBufferSize(
        descriptor.ySize(), descriptor.yStride(), descriptor.cbCrSize(),
        descriptor.cbCrStride(), descriptor.yOffset(), descriptor.cbOffset(),
        descriptor.crOffset());
    if (NS_WARN_IF(descriptorSize > bufferSize)) {
      MOZ_ASSERT_UNREACHABLE("Buffer too small to fit descriptor!");
      return nullptr;
    }

    PlanarYCbCrData pData;
    pData.mYStride = descriptor.yStride();
    pData.mCbCrStride = descriptor.cbCrStride();
    // default mYSkip, mCbSkip, mCrSkip because not held in YCbCrDescriptor
    pData.mYSkip = pData.mCbSkip = pData.mCrSkip = 0;
    pData.mPictureRect = descriptor.display();
    pData.mStereoMode = descriptor.stereoMode();
    pData.mColorDepth = descriptor.colorDepth();
    pData.mYUVColorSpace = descriptor.yUVColorSpace();
    pData.mColorRange = descriptor.colorRange();
    pData.mChromaSubsampling = descriptor.chromaSubsampling();
    pData.mYChannel = ImageDataSerializer::GetYChannel(buffer, descriptor);
    pData.mCbChannel = ImageDataSerializer::GetCbChannel(buffer, descriptor);
    pData.mCrChannel = ImageDataSerializer::GetCrChannel(buffer, descriptor);

    // images coming from AOMDecoder are RecyclingPlanarYCbCrImages.
    RefPtr<RecyclingPlanarYCbCrImage> image =
        new RecyclingPlanarYCbCrImage(aBufferRecycleBin);
    if (NS_WARN_IF(NS_FAILED(image->CopyData(pData)))) {
      return nullptr;
    }

    return image.forget();
  }

  if (sdBuffer.desc().type() == BufferDescriptor::TRGBDescriptor) {
    const RGBDescriptor& descriptor = sdBuffer.desc().get_RGBDescriptor();

    size_t descriptorSize = ImageDataSerializer::ComputeRGBBufferSize(
        descriptor.size(), descriptor.format());
    if (NS_WARN_IF(descriptorSize > bufferSize)) {
      MOZ_ASSERT_UNREACHABLE("Buffer too small to fit descriptor!");
      return nullptr;
    }

    auto stride = ImageDataSerializer::ComputeRGBStride(
        descriptor.format(), descriptor.size().width);
    auto surface = MakeRefPtr<SourceSurfaceAlignedRawData>();
    if (NS_WARN_IF(!surface->Init(descriptor.size(), descriptor.format(),
                                  /* aClearMem */ false, /* aClearValue */ 0,
                                  stride))) {
      return nullptr;
    }

    DataSourceSurface::ScopedMap map(surface, DataSourceSurface::WRITE);
    if (NS_WARN_IF(!map.IsMapped())) {
      return nullptr;
    }

    if (NS_WARN_IF(!SwizzleData(buffer, stride, descriptor.format(),
                                map.GetData(), map.GetStride(),
                                descriptor.format(), descriptor.size()))) {
      return nullptr;
    }

    return MakeAndAddRef<SourceSurfaceImage>(descriptor.size(), surface);
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected buffer descriptor type!");
  return nullptr;
}

already_AddRefed<layers::Image> RemoteImageHolder::TransferToImage(
    layers::BufferRecycleBin* aBufferRecycleBin) {
  if (IsEmpty()) {
    return nullptr;
  }
  RefPtr<Image> image;
  if (mSD->type() == SurfaceDescriptor::TSurfaceDescriptorBuffer) {
    image = DeserializeImage(aBufferRecycleBin);
  } else if (mManager) {
    image = mManager->TransferToImage(*mSD, mSize, mColorDepth, mYUVColorSpace,
                                      mColorPrimaries, mTransferFunction,
                                      mColorRange);
  }
  mSD = Nothing();
  mManager = nullptr;

  return image.forget();
}

RemoteImageHolder::~RemoteImageHolder() {
  // GPU Images are held by the RemoteMediaManagerParent, we didn't get to use
  // this image holder (the decoder could have been flushed). We don't need to
  // worry about Shmem based image as the Shmem will be automatically re-used
  // once the decoder is used again.
  if (!IsEmpty() && mManager &&
      mSD->type() != SurfaceDescriptor::TSurfaceDescriptorBuffer) {
    SurfaceDescriptorRemoteDecoder remoteSD =
        static_cast<const SurfaceDescriptorGPUVideo&>(*mSD);
    mManager->DeallocateSurfaceDescriptor(remoteSD);
  }
}

/* static */ void ipc::IPDLParamTraits<RemoteImageHolder>::Write(
    IPC::MessageWriter* aWriter, ipc::IProtocol* aActor,
    RemoteImageHolder&& aParam) {
  WriteIPDLParam(aWriter, aActor, aParam.mSource);
  WriteIPDLParam(aWriter, aActor, aParam.mSize);
  WriteIPDLParam(aWriter, aActor, aParam.mColorDepth);
  WriteIPDLParam(aWriter, aActor, aParam.mSD);
  WriteIPDLParam(aWriter, aActor, aParam.mYUVColorSpace);
  WriteIPDLParam(aWriter, aActor, aParam.mColorPrimaries);
  WriteIPDLParam(aWriter, aActor, aParam.mTransferFunction);
  WriteIPDLParam(aWriter, aActor, aParam.mColorRange);
  // Empty this holder.
  aParam.mSD = Nothing();
  aParam.mManager = nullptr;
}

/* static */ bool ipc::IPDLParamTraits<RemoteImageHolder>::Read(
    IPC::MessageReader* aReader, ipc::IProtocol* aActor,
    RemoteImageHolder* aResult) {
  if (!ReadIPDLParam(aReader, aActor, &aResult->mSource) ||
      !ReadIPDLParam(aReader, aActor, &aResult->mSize) ||
      !ReadIPDLParam(aReader, aActor, &aResult->mColorDepth) ||
      !ReadIPDLParam(aReader, aActor, &aResult->mSD) ||
      !ReadIPDLParam(aReader, aActor, &aResult->mYUVColorSpace) ||
      !ReadIPDLParam(aReader, aActor, &aResult->mColorPrimaries) ||
      !ReadIPDLParam(aReader, aActor, &aResult->mTransferFunction) ||
      !ReadIPDLParam(aReader, aActor, &aResult->mColorRange)) {
    return false;
  }

  if (aResult->IsEmpty()) {
    return true;
  }

  if (auto* manager = aActor->Manager()) {
    if (manager->GetProtocolId() == ProtocolId::PRemoteMediaManagerMsgStart) {
      aResult->mManager =
          XRE_IsContentProcess()
              ? static_cast<IGPUVideoSurfaceManager*>(
                    static_cast<RemoteMediaManagerChild*>(manager))
              : static_cast<IGPUVideoSurfaceManager*>(
                    static_cast<RemoteMediaManagerParent*>(manager));
      return true;
    }
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected or missing protocol manager!");
  return false;
}

}  // namespace mozilla
