/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Interfaces for drawing graphics to an in process buffer when
// recording/replaying.

#include "ProcessRecordReplay.h"
#include "mozilla/Base64.h"
#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/LayersMessages.h"
#include "imgIEncoder.h"

using namespace mozilla::layers;

namespace mozilla::recordreplay {

static bool (*gShouldPaint)();
static void (*gOnPaint)(const char* aMimeType, const char* aData);

void InitializeGraphics() {
  LoadSymbol("RecordReplayShouldPaint", gShouldPaint);
  LoadSymbol("RecordReplayOnPaint", gOnPaint);
}

static LayerManagerComposite* gLayerManager;
static CompositorBridgeParent* gCompositorBridge;
static LayerTransactionParent* gLayerTransactionParent;

static void EnsureInitialized() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (gLayerTransactionParent) {
    return;
  }

  Compositor* compositor = new BasicCompositor(nullptr, nullptr);
  gLayerManager = new LayerManagerComposite(compositor);

  gCompositorBridge = new CompositorBridgeParent(nullptr,
                                                 CSSToLayoutDeviceScale(1),
                                                 TimeDuration(),
                                                 CompositorOptions(),
                                                 false,
                                                 gfx::IntSize());
  gCompositorBridge->SetLayerManager(gLayerManager);

  gLayerTransactionParent = new LayerTransactionParent(gLayerManager,
                                                       gCompositorBridge, nullptr,
                                                       LayersId(), TimeDuration());
}

static void ReportPaint();

void SendUpdate(const TransactionInfo& aInfo) {
  EnsureInitialized();

  // We never need to update the compositor state in the recording process,
  // because we send updates to the UI process which will composite in the
  // regular way.
  if (IsRecording()) {
    return;
  }

  // Make sure the compositor does not interact with the recording.
  recordreplay::AutoDisallowThreadEvents disallow;

  // Even if we won't be painting, we need to continue updating the layer state
  // in case we end up wanting to paint later.
  ipc::IPCResult rv = gLayerTransactionParent->RecvUpdate(aInfo);
  MOZ_RELEASE_ASSERT(rv == ipc::IPCResult::Ok());

  // Only composite if the driver wants to know about the painted data.
  if (gShouldPaint()) {
    gCompositorBridge->CompositeToTarget(VsyncId(), nullptr, nullptr);
    ReportPaint();
  }
}

void SendNewCompositable(const layers::CompositableHandle& aHandle,
                         const layers::TextureInfo& aInfo) {
  EnsureInitialized();

  ipc::IPCResult rv = gLayerTransactionParent->RecvNewCompositable(aHandle, aInfo);
  MOZ_RELEASE_ASSERT(rv == ipc::IPCResult::Ok());
}

// Format to use for graphics data.
static const gfx::SurfaceFormat SurfaceFormat = gfx::SurfaceFormat::R8G8B8X8;

// Buffer for the draw target used for main thread compositing.
static void* gDrawTargetBuffer;
static size_t gDrawTargetBufferSize;

// Dimensions of the last paint which the compositor performed.
static size_t gPaintWidth, gPaintHeight;

already_AddRefed<gfx::DrawTarget> DrawTargetForRemoteDrawing(const gfx::IntRect& aSize) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (aSize.IsEmpty()) {
    return nullptr;
  }

  gPaintWidth = aSize.width;
  gPaintHeight = aSize.height;

  gfx::IntSize size(aSize.width, aSize.height);
  size_t bufferSize = ImageDataSerializer::ComputeRGBBufferSize(size, SurfaceFormat);

  if (bufferSize != gDrawTargetBufferSize) {
    free(gDrawTargetBuffer);
    gDrawTargetBuffer = malloc(bufferSize);
    gDrawTargetBufferSize = bufferSize;
  }

  size_t stride = ImageDataSerializer::ComputeRGBStride(SurfaceFormat, aSize.width);
  RefPtr<gfx::DrawTarget> drawTarget = gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, (uint8_t*)gDrawTargetBuffer, size, stride,
      SurfaceFormat,
      /* aUninitialized = */ true);
  MOZ_RELEASE_ASSERT(drawTarget);

  return drawTarget.forget();
}

struct TextureInfo {
  uint8_t* mBuffer;
  BufferDescriptor mDesc;
  TextureFlags mFlags;
};

static std::unordered_map<PTextureChild*, TextureInfo> gTextureInfo;

void RegisterTextureChild(PTextureChild* aChild, TextureData* aData,
                          const SurfaceDescriptor& aDesc,
                          TextureFlags aFlags) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (aDesc.type() != SurfaceDescriptor::TSurfaceDescriptorBuffer) {
    return;
  }

  const SurfaceDescriptorBuffer& buf = aDesc.get_SurfaceDescriptorBuffer();
  MOZ_RELEASE_ASSERT(buf.data().type() == MemoryOrShmem::TShmem);
  uint8_t* buffer = static_cast<BufferTextureData*>(aData)->GetBuffer();

  TextureInfo info = {
    buffer,
    buf.desc(),
    aFlags
  };

  gTextureInfo[aChild] = info;
}

TextureHost* CreateTextureHost(PTextureChild* aChild) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  auto iter = gTextureInfo.find(aChild);
  MOZ_RELEASE_ASSERT(iter != gTextureInfo.end());
  const TextureInfo& info = iter->second;
  MemoryTextureHost* rv = new MemoryTextureHost(info.mBuffer, info.mDesc, info.mFlags);

  // Leak the result so it doesn't get deleted later. We aren't respecting
  // ownership rules by giving this MemoryTextureHost an internal pointer to
  // a shmem.
  new RefPtr(rv);

  return rv;
}

static void EncodeGraphics(const char* aMimeType,
                           const char* aEncodeOptions) {
  AutoPassThroughThreadEvents pt;

  // Get an image encoder for the media type.
  nsPrintfCString encoderCID("@mozilla.org/image/encoder;2?type=%s",
                             nsCString(aMimeType).get());
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());

  size_t stride = layers::ImageDataSerializer::ComputeRGBStride(SurfaceFormat,
                                                                gPaintWidth);

  nsString options = NS_ConvertUTF8toUTF16(aEncodeOptions);
  nsresult rv = encoder->InitFromData(
      (const uint8_t*)gDrawTargetBuffer, stride * gPaintHeight, gPaintWidth,
      gPaintHeight, stride, imgIEncoder::INPUT_FORMAT_RGBA, options);
  if (NS_FAILED(rv)) {
    PrintLog("Error: encoder->InitFromData() failed");
    return;
  }

  uint64_t count;
  rv = encoder->Available(&count);
  if (NS_FAILED(rv)) {
    PrintLog("Error: encoder->Available() failed");
    return;
  }

  nsCString data;
  rv = Base64EncodeInputStream(encoder, data, count);
  if (NS_FAILED(rv)) {
    PrintLog("Error: Base64EncodeInputStream() failed");
    return;
  }

  gOnPaint(aMimeType, data.get());
}

static void ReportPaint() {
  // For now we only supply JPEG graphics, which is all that the devtools
  // client uses. This API needs to be redesigned to decouple these things
  // and allow the driver to fetch whatever graphics it wants...
  //EncodeGraphics("image/png", "");
  EncodeGraphics("image/jpeg", "quality=50");
}

} // namespace mozilla::recordreplay
