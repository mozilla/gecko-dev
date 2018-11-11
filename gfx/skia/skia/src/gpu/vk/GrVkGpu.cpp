/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrVkGpu.h"

#include "GrBackendSemaphore.h"
#include "GrBackendSurface.h"
#include "GrContextOptions.h"
#include "GrGeometryProcessor.h"
#include "GrGpuResourceCacheAccess.h"
#include "GrMesh.h"
#include "GrPipeline.h"
#include "GrRenderTargetPriv.h"
#include "GrTexturePriv.h"
#include "GrVkAMDMemoryAllocator.h"
#include "GrVkCommandBuffer.h"
#include "GrVkGpuCommandBuffer.h"
#include "GrVkImage.h"
#include "GrVkIndexBuffer.h"
#include "GrVkInterface.h"
#include "GrVkMemory.h"
#include "GrVkPipeline.h"
#include "GrVkPipelineState.h"
#include "GrVkRenderPass.h"
#include "GrVkResourceProvider.h"
#include "GrVkSemaphore.h"
#include "GrVkTexture.h"
#include "GrVkTextureRenderTarget.h"
#include "GrVkTransferBuffer.h"
#include "GrVkVertexBuffer.h"
#include "SkConvertPixels.h"
#include "SkMipMap.h"
#include "SkSLCompiler.h"
#include "SkTo.h"

#include "vk/GrVkExtensions.h"
#include "vk/GrVkTypes.h"

#include <utility>

#if !defined(SK_BUILD_FOR_WIN)
#include <unistd.h>
#endif // !defined(SK_BUILD_FOR_WIN)

#if defined(SK_BUILD_FOR_WIN) && defined(SK_DEBUG)
#include "SkLeanWindows.h"
#endif

#define VK_CALL(X) GR_VK_CALL(this->vkInterface(), X)
#define VK_CALL_RET(RET, X) GR_VK_CALL_RET(this->vkInterface(), RET, X)
#define VK_CALL_ERRCHECK(X) GR_VK_CALL_ERRCHECK(this->vkInterface(), X)

sk_sp<GrGpu> GrVkGpu::Make(const GrVkBackendContext& backendContext,
                           const GrContextOptions& options, GrContext* context) {
    if (backendContext.fInstance == VK_NULL_HANDLE ||
        backendContext.fPhysicalDevice == VK_NULL_HANDLE ||
        backendContext.fDevice == VK_NULL_HANDLE ||
        backendContext.fQueue == VK_NULL_HANDLE) {
        return nullptr;
    }
    if (!backendContext.fGetProc) {
        return nullptr;
    }

    PFN_vkGetPhysicalDeviceProperties localGetPhysicalDeviceProperties =
            reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
                    backendContext.fGetProc("vkGetPhysicalDeviceProperties",
                                            backendContext.fInstance,
                                            VK_NULL_HANDLE));

    if (!localGetPhysicalDeviceProperties) {
        return nullptr;
    }
    VkPhysicalDeviceProperties physDeviceProperties;
    localGetPhysicalDeviceProperties(backendContext.fPhysicalDevice, &physDeviceProperties);
    uint32_t physDevVersion = physDeviceProperties.apiVersion;

    sk_sp<const GrVkInterface> interface;

    if (backendContext.fVkExtensions) {
        interface.reset(new GrVkInterface(backendContext.fGetProc,
                                          backendContext.fInstance,
                                          backendContext.fDevice,
                                          backendContext.fInstanceVersion,
                                          physDevVersion,
                                          backendContext.fVkExtensions));
        if (!interface->validate(backendContext.fInstanceVersion, physDevVersion,
                                 backendContext.fVkExtensions)) {
            return nullptr;
        }
    } else {
        // None of our current GrVkExtension flags actually affect the vulkan backend so we just
        // make an empty GrVkExtensions and pass that to the GrVkInterface.
        GrVkExtensions extensions;
        interface.reset(new GrVkInterface(backendContext.fGetProc,
                                          backendContext.fInstance,
                                          backendContext.fDevice,
                                          backendContext.fInstanceVersion,
                                          physDevVersion,
                                          &extensions));
        if (!interface->validate(backendContext.fInstanceVersion, physDevVersion, &extensions)) {
            return nullptr;
        }
    }

    return sk_sp<GrGpu>(new GrVkGpu(context, options, backendContext, interface));
}

////////////////////////////////////////////////////////////////////////////////

GrVkGpu::GrVkGpu(GrContext* context, const GrContextOptions& options,
                 const GrVkBackendContext& backendContext, sk_sp<const GrVkInterface> interface)
        : INHERITED(context)
        , fInterface(std::move(interface))
        , fMemoryAllocator(backendContext.fMemoryAllocator)
        , fInstance(backendContext.fInstance)
        , fPhysicalDevice(backendContext.fPhysicalDevice)
        , fDevice(backendContext.fDevice)
        , fQueue(backendContext.fQueue)
        , fQueueIndex(backendContext.fGraphicsQueueIndex)
        , fResourceProvider(this)
        , fDisconnected(false) {
    SkASSERT(!backendContext.fOwnsInstanceAndDevice);

    if (!fMemoryAllocator) {
        // We were not given a memory allocator at creation
        fMemoryAllocator.reset(new GrVkAMDMemoryAllocator(backendContext.fPhysicalDevice,
                                                          fDevice, fInterface));
    }

    fCompiler = new SkSL::Compiler();

    uint32_t instanceVersion = backendContext.fInstanceVersion ? backendContext.fInstanceVersion
                                                               : backendContext.fMinAPIVersion;

    if (backendContext.fDeviceFeatures2) {
        fVkCaps.reset(new GrVkCaps(options, this->vkInterface(), backendContext.fPhysicalDevice,
                                   *backendContext.fDeviceFeatures2, instanceVersion,
                                   *backendContext.fVkExtensions));
    } else if (backendContext.fDeviceFeatures) {
        VkPhysicalDeviceFeatures2 features2;
        features2.pNext = nullptr;
        features2.features = *backendContext.fDeviceFeatures;
        fVkCaps.reset(new GrVkCaps(options, this->vkInterface(), backendContext.fPhysicalDevice,
                                   features2, instanceVersion, *backendContext.fVkExtensions));
    } else {
        VkPhysicalDeviceFeatures2 features;
        memset(&features, 0, sizeof(VkPhysicalDeviceFeatures2));
        features.pNext = nullptr;
        if (backendContext.fFeatures & kGeometryShader_GrVkFeatureFlag) {
            features.features.geometryShader = true;
        }
        if (backendContext.fFeatures & kDualSrcBlend_GrVkFeatureFlag) {
            features.features.dualSrcBlend = true;
        }
        if (backendContext.fFeatures & kSampleRateShading_GrVkFeatureFlag) {
            features.features.sampleRateShading = true;
        }
        fVkCaps.reset(new GrVkCaps(options, this->vkInterface(), backendContext.fPhysicalDevice,
                                   features, instanceVersion, GrVkExtensions()));
    }
    fCaps.reset(SkRef(fVkCaps.get()));

    VK_CALL(GetPhysicalDeviceProperties(backendContext.fPhysicalDevice, &fPhysDevProps));
    VK_CALL(GetPhysicalDeviceMemoryProperties(backendContext.fPhysicalDevice, &fPhysDevMemProps));

    const VkCommandPoolCreateInfo cmdPoolInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType
        nullptr,                                         // pNext
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // CmdPoolCreateFlags
        backendContext.fGraphicsQueueIndex,              // queueFamilyIndex
    };
    GR_VK_CALL_ERRCHECK(this->vkInterface(), CreateCommandPool(fDevice, &cmdPoolInfo, nullptr,
                                                               &fCmdPool));

    // must call this after creating the CommandPool
    fResourceProvider.init();
    fCurrentCmdBuffer = fResourceProvider.findOrCreatePrimaryCommandBuffer();
    SkASSERT(fCurrentCmdBuffer);
    fCurrentCmdBuffer->begin(this);
}

void GrVkGpu::destroyResources() {
    if (fCurrentCmdBuffer) {
        fCurrentCmdBuffer->end(this);
        fCurrentCmdBuffer->unref(this);
    }

    // wait for all commands to finish
    fResourceProvider.checkCommandBuffers();
    VkResult res = VK_CALL(QueueWaitIdle(fQueue));

    // On windows, sometimes calls to QueueWaitIdle return before actually signalling the fences
    // on the command buffers even though they have completed. This causes an assert to fire when
    // destroying the command buffers. Currently this ony seems to happen on windows, so we add a
    // sleep to make sure the fence signals.
#ifdef SK_DEBUG
    if (this->vkCaps().mustSleepOnTearDown()) {
#if defined(SK_BUILD_FOR_WIN)
        Sleep(10); // In milliseconds
#else
        sleep(1);  // In seconds
#endif
    }
#endif

#ifdef SK_DEBUG
    SkASSERT(VK_SUCCESS == res || VK_ERROR_DEVICE_LOST == res);
#endif

    for (int i = 0; i < fSemaphoresToWaitOn.count(); ++i) {
        fSemaphoresToWaitOn[i]->unref(this);
    }
    fSemaphoresToWaitOn.reset();

    for (int i = 0; i < fSemaphoresToSignal.count(); ++i) {
        fSemaphoresToSignal[i]->unref(this);
    }
    fSemaphoresToSignal.reset();


    fCopyManager.destroyResources(this);

    // must call this just before we destroy the command pool and VkDevice
    fResourceProvider.destroyResources(VK_ERROR_DEVICE_LOST == res);

    if (fCmdPool != VK_NULL_HANDLE) {
        VK_CALL(DestroyCommandPool(fDevice, fCmdPool, nullptr));
    }

    fMemoryAllocator.reset();

    fQueue = VK_NULL_HANDLE;
    fDevice = VK_NULL_HANDLE;
    fInstance = VK_NULL_HANDLE;
}

GrVkGpu::~GrVkGpu() {
    if (!fDisconnected) {
        this->destroyResources();
    }
    delete fCompiler;
}


void GrVkGpu::disconnect(DisconnectType type) {
    INHERITED::disconnect(type);
    if (!fDisconnected) {
        if (DisconnectType::kCleanup == type) {
            this->destroyResources();
        } else {
            if (fCurrentCmdBuffer) {
                fCurrentCmdBuffer->unrefAndAbandon();
            }
            for (int i = 0; i < fSemaphoresToWaitOn.count(); ++i) {
                fSemaphoresToWaitOn[i]->unrefAndAbandon();
            }
            for (int i = 0; i < fSemaphoresToSignal.count(); ++i) {
                fSemaphoresToSignal[i]->unrefAndAbandon();
            }
            fCopyManager.abandonResources();

            // must call this just before we destroy the command pool and VkDevice
            fResourceProvider.abandonResources();

            fMemoryAllocator.reset();
        }
        fSemaphoresToWaitOn.reset();
        fSemaphoresToSignal.reset();
        fCurrentCmdBuffer = nullptr;
        fCmdPool = VK_NULL_HANDLE;
        fDisconnected = true;
    }
}

///////////////////////////////////////////////////////////////////////////////

GrGpuRTCommandBuffer* GrVkGpu::getCommandBuffer(
            GrRenderTarget* rt, GrSurfaceOrigin origin,
            const GrGpuRTCommandBuffer::LoadAndStoreInfo& colorInfo,
            const GrGpuRTCommandBuffer::StencilLoadAndStoreInfo& stencilInfo) {
    if (!fCachedRTCommandBuffer) {
        fCachedRTCommandBuffer.reset(new GrVkGpuRTCommandBuffer(this));
    }

    fCachedRTCommandBuffer->set(rt, origin, colorInfo, stencilInfo);
    return fCachedRTCommandBuffer.get();
}

GrGpuTextureCommandBuffer* GrVkGpu::getCommandBuffer(GrTexture* texture, GrSurfaceOrigin origin) {
    if (!fCachedTexCommandBuffer) {
        fCachedTexCommandBuffer.reset(new GrVkGpuTextureCommandBuffer(this));
    }

    fCachedTexCommandBuffer->set(texture, origin);
    return fCachedTexCommandBuffer.get();
}

void GrVkGpu::submitCommandBuffer(SyncQueue sync) {
    SkASSERT(fCurrentCmdBuffer);
    fCurrentCmdBuffer->end(this);

    fCurrentCmdBuffer->submitToQueue(this, fQueue, sync, fSemaphoresToSignal, fSemaphoresToWaitOn);

    for (int i = 0; i < fSemaphoresToWaitOn.count(); ++i) {
        fSemaphoresToWaitOn[i]->unref(this);
    }
    fSemaphoresToWaitOn.reset();
    for (int i = 0; i < fSemaphoresToSignal.count(); ++i) {
        fSemaphoresToSignal[i]->unref(this);
    }
    fSemaphoresToSignal.reset();

    fResourceProvider.checkCommandBuffers();

    // Release old command buffer and create a new one
    fCurrentCmdBuffer->unref(this);
    fCurrentCmdBuffer = fResourceProvider.findOrCreatePrimaryCommandBuffer();
    SkASSERT(fCurrentCmdBuffer);

    fCurrentCmdBuffer->begin(this);
}

///////////////////////////////////////////////////////////////////////////////
GrBuffer* GrVkGpu::onCreateBuffer(size_t size, GrBufferType type, GrAccessPattern accessPattern,
                                  const void* data) {
    GrBuffer* buff;
    switch (type) {
        case kVertex_GrBufferType:
            SkASSERT(kDynamic_GrAccessPattern == accessPattern ||
                     kStatic_GrAccessPattern == accessPattern);
            buff = GrVkVertexBuffer::Create(this, size, kDynamic_GrAccessPattern == accessPattern);
            break;
        case kIndex_GrBufferType:
            SkASSERT(kDynamic_GrAccessPattern == accessPattern ||
                     kStatic_GrAccessPattern == accessPattern);
            buff = GrVkIndexBuffer::Create(this, size, kDynamic_GrAccessPattern == accessPattern);
            break;
        case kXferCpuToGpu_GrBufferType:
            SkASSERT(kDynamic_GrAccessPattern == accessPattern ||
                     kStream_GrAccessPattern == accessPattern);
            buff = GrVkTransferBuffer::Create(this, size, GrVkBuffer::kCopyRead_Type);
            break;
        case kXferGpuToCpu_GrBufferType:
            SkASSERT(kDynamic_GrAccessPattern == accessPattern ||
                     kStream_GrAccessPattern == accessPattern);
            buff = GrVkTransferBuffer::Create(this, size, GrVkBuffer::kCopyWrite_Type);
            break;
        case kDrawIndirect_GrBufferType:
            SK_ABORT("DrawIndirect Buffers not supported  in vulkan backend.");
            return nullptr;
        default:
            SK_ABORT("Unknown buffer type.");
            return nullptr;
    }
    if (data && buff) {
        buff->updateData(data, size);
    }
    return buff;
}

bool GrVkGpu::onWritePixels(GrSurface* surface, int left, int top, int width, int height,
                            GrColorType srcColorType, const GrMipLevel texels[],
                            int mipLevelCount) {
    GrVkTexture* vkTex = static_cast<GrVkTexture*>(surface->asTexture());
    if (!vkTex) {
        return false;
    }

    // Make sure we have at least the base level
    if (!mipLevelCount || !texels[0].fPixels) {
        return false;
    }

    bool success = false;
    bool linearTiling = vkTex->isLinearTiled();
    if (linearTiling) {
        if (mipLevelCount > 1) {
            SkDebugf("Can't upload mipmap data to linear tiled texture");
            return false;
        }
        if (VK_IMAGE_LAYOUT_PREINITIALIZED != vkTex->currentLayout()) {
            // Need to change the layout to general in order to perform a host write
            vkTex->setImageLayout(this,
                                  VK_IMAGE_LAYOUT_GENERAL,
                                  VK_ACCESS_HOST_WRITE_BIT,
                                  VK_PIPELINE_STAGE_HOST_BIT,
                                  false);
            this->submitCommandBuffer(kForce_SyncQueue);
        }
        success = this->uploadTexDataLinear(vkTex, left, top, width, height, srcColorType,
                                            texels[0].fPixels, texels[0].fRowBytes);
    } else {
        SkASSERT(mipLevelCount <= vkTex->texturePriv().maxMipMapLevel() + 1);
        success = this->uploadTexDataOptimal(vkTex, left, top, width, height, srcColorType, texels,
                                             mipLevelCount);
    }

    return success;
}

bool GrVkGpu::onTransferPixels(GrTexture* texture, int left, int top, int width, int height,
                               GrColorType bufferColorType, GrBuffer* transferBuffer,
                               size_t bufferOffset, size_t rowBytes) {
    // Vulkan only supports 4-byte aligned offsets
    if (SkToBool(bufferOffset & 0x2)) {
        return false;
    }
    GrVkTexture* vkTex = static_cast<GrVkTexture*>(texture);
    if (!vkTex) {
        return false;
    }
    GrVkTransferBuffer* vkBuffer = static_cast<GrVkTransferBuffer*>(transferBuffer);
    if (!vkBuffer) {
        return false;
    }

    SkDEBUGCODE(
        SkIRect subRect = SkIRect::MakeXYWH(left, top, width, height);
        SkIRect bounds = SkIRect::MakeWH(texture->width(), texture->height());
        SkASSERT(bounds.contains(subRect));
    )
    int bpp = GrColorTypeBytesPerPixel(bufferColorType);
    if (rowBytes == 0) {
        rowBytes = bpp * width;
    }

    // Set up copy region
    VkBufferImageCopy region;
    memset(&region, 0, sizeof(VkBufferImageCopy));
    region.bufferOffset = bufferOffset;
    region.bufferRowLength = (uint32_t)(rowBytes/bpp);
    region.bufferImageHeight = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { left, top, 0 };
    region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    // Change layout of our target so it can be copied to
    vkTex->setImageLayout(this,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          false);

    // Copy the buffer to the image
    fCurrentCmdBuffer->copyBufferToImage(this,
                                         vkBuffer,
                                         vkTex,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         1,
                                         &region);

    vkTex->texturePriv().markMipMapsDirty();
    return true;
}

void GrVkGpu::resolveImage(GrSurface* dst, GrVkRenderTarget* src, const SkIRect& srcRect,
                           const SkIPoint& dstPoint) {
    SkASSERT(dst);
    SkASSERT(src && src->numColorSamples() > 1 && src->msaaImage());

    if (this->vkCaps().mustSubmitCommandsBeforeCopyOp()) {
        this->submitCommandBuffer(GrVkGpu::kSkip_SyncQueue);
    }

    VkImageResolve resolveInfo;
    resolveInfo.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    resolveInfo.srcOffset = {srcRect.fLeft, srcRect.fTop, 0};
    resolveInfo.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    resolveInfo.dstOffset = {dstPoint.fX, dstPoint.fY, 0};
    resolveInfo.extent = {(uint32_t)srcRect.width(), (uint32_t)srcRect.height(), 1};

    GrVkImage* dstImage;
    GrRenderTarget* dstRT = dst->asRenderTarget();
    if (dstRT) {
        GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(dstRT);
        dstImage = vkRT;
    } else {
        SkASSERT(dst->asTexture());
        dstImage = static_cast<GrVkTexture*>(dst->asTexture());
    }
    dstImage->setImageLayout(this,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             false);

    src->msaaImage()->setImageLayout(this,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     VK_ACCESS_TRANSFER_READ_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     false);

    fCurrentCmdBuffer->resolveImage(this, *src->msaaImage(), *dstImage, 1, &resolveInfo);
}

void GrVkGpu::internalResolveRenderTarget(GrRenderTarget* target, bool requiresSubmit) {
    if (target->needsResolve()) {
        SkASSERT(target->numColorSamples() > 1);
        GrVkRenderTarget* rt = static_cast<GrVkRenderTarget*>(target);
        SkASSERT(rt->msaaImage());

        const SkIRect& srcRect = rt->getResolveRect();

        this->resolveImage(target, rt, srcRect, SkIPoint::Make(srcRect.fLeft, srcRect.fTop));

        rt->flagAsResolved();

        if (requiresSubmit) {
            this->submitCommandBuffer(kSkip_SyncQueue);
        }
    }
}

bool GrVkGpu::uploadTexDataLinear(GrVkTexture* tex, int left, int top, int width, int height,
                                  GrColorType dataColorType, const void* data, size_t rowBytes) {
    SkASSERT(data);
    SkASSERT(tex->isLinearTiled());

    SkDEBUGCODE(
        SkIRect subRect = SkIRect::MakeXYWH(left, top, width, height);
        SkIRect bounds = SkIRect::MakeWH(tex->width(), tex->height());
        SkASSERT(bounds.contains(subRect));
    )
    int bpp = GrColorTypeBytesPerPixel(dataColorType);
    size_t trimRowBytes = width * bpp;
    if (!rowBytes) {
        rowBytes = trimRowBytes;
    }

    SkASSERT(VK_IMAGE_LAYOUT_PREINITIALIZED == tex->currentLayout() ||
             VK_IMAGE_LAYOUT_GENERAL == tex->currentLayout());
    const VkImageSubresource subres = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,  // mipLevel
        0,  // arraySlice
    };
    VkSubresourceLayout layout;

    const GrVkInterface* interface = this->vkInterface();

    GR_VK_CALL(interface, GetImageSubresourceLayout(fDevice,
                                                    tex->image(),
                                                    &subres,
                                                    &layout));

    const GrVkAlloc& alloc = tex->alloc();
    VkDeviceSize offset = top * layout.rowPitch + left * bpp;
    VkDeviceSize size = height*layout.rowPitch;
    SkASSERT(size + offset <= alloc.fSize);
    void* mapPtr = GrVkMemory::MapAlloc(this, alloc);
    if (!mapPtr) {
        return false;
    }
    mapPtr = reinterpret_cast<char*>(mapPtr) + offset;

    SkRectMemcpy(mapPtr, static_cast<size_t>(layout.rowPitch), data, rowBytes, trimRowBytes,
                 height);

    GrVkMemory::FlushMappedAlloc(this, alloc, offset, size);
    GrVkMemory::UnmapAlloc(this, alloc);

    return true;
}

bool GrVkGpu::uploadTexDataOptimal(GrVkTexture* tex, int left, int top, int width, int height,
                                   GrColorType dataColorType, const GrMipLevel texels[],
                                   int mipLevelCount) {
    SkASSERT(!tex->isLinearTiled());
    // The assumption is either that we have no mipmaps, or that our rect is the entire texture
    SkASSERT(1 == mipLevelCount ||
             (0 == left && 0 == top && width == tex->width() && height == tex->height()));

    // We assume that if the texture has mip levels, we either upload to all the levels or just the
    // first.
    SkASSERT(1 == mipLevelCount || mipLevelCount == (tex->texturePriv().maxMipMapLevel() + 1));

    if (width == 0 || height == 0) {
        return false;
    }

    if (GrPixelConfigToColorType(tex->config()) != dataColorType) {
        return false;
    }

    // For RGB_888x src data we are uploading it first to an RGBA texture and then copying it to the
    // dst RGB texture. Thus we do not upload mip levels for that.
    if (dataColorType == GrColorType::kRGB_888x) {
        SkASSERT(tex->imageFormat() == VK_FORMAT_R8G8B8_UNORM &&
                 tex->config() == kRGB_888_GrPixelConfig);
        // First check that we'll be able to do the copy to the to the R8G8B8 image in the end via a
        // blit or draw.
        if (!this->vkCaps().configCanBeDstofBlit(kRGB_888_GrPixelConfig, tex->isLinearTiled()) &&
            !this->vkCaps().maxRenderTargetSampleCount(kRGB_888_GrPixelConfig)) {
            return false;
        }
        mipLevelCount = 1;
    }

    SkASSERT(this->caps()->isConfigTexturable(tex->config()));
    int bpp = GrColorTypeBytesPerPixel(dataColorType);

    // texels is const.
    // But we may need to adjust the fPixels ptr based on the copyRect, or fRowBytes.
    // Because of this we need to make a non-const shallow copy of texels.
    SkAutoTMalloc<GrMipLevel> texelsShallowCopy;

    texelsShallowCopy.reset(mipLevelCount);
    memcpy(texelsShallowCopy.get(), texels, mipLevelCount*sizeof(GrMipLevel));

    SkTArray<size_t> individualMipOffsets(mipLevelCount);
    individualMipOffsets.push_back(0);
    size_t combinedBufferSize = width * bpp * height;
    int currentWidth = width;
    int currentHeight = height;
    if (!texelsShallowCopy[0].fPixels) {
        combinedBufferSize = 0;
    }

    // The alignment must be at least 4 bytes and a multiple of the bytes per pixel of the image
    // config. This works with the assumption that the bytes in pixel config is always a power of 2.
    SkASSERT((bpp & (bpp - 1)) == 0);
    const size_t alignmentMask = 0x3 | (bpp - 1);
    for (int currentMipLevel = 1; currentMipLevel < mipLevelCount; currentMipLevel++) {
        currentWidth = SkTMax(1, currentWidth/2);
        currentHeight = SkTMax(1, currentHeight/2);

        if (texelsShallowCopy[currentMipLevel].fPixels) {
            const size_t trimmedSize = currentWidth * bpp * currentHeight;
            const size_t alignmentDiff = combinedBufferSize & alignmentMask;
            if (alignmentDiff != 0) {
                combinedBufferSize += alignmentMask - alignmentDiff + 1;
            }
            individualMipOffsets.push_back(combinedBufferSize);
            combinedBufferSize += trimmedSize;
        } else {
            individualMipOffsets.push_back(0);
        }
    }
    if (0 == combinedBufferSize) {
        // We don't actually have any data to upload so just return success
        return true;
    }

    // allocate buffer to hold our mip data
    GrVkTransferBuffer* transferBuffer =
                   GrVkTransferBuffer::Create(this, combinedBufferSize, GrVkBuffer::kCopyRead_Type);
    if (!transferBuffer) {
        return false;
    }

    int uploadLeft = left;
    int uploadTop = top;
    GrVkTexture* uploadTexture = tex;
    // For uploading RGB_888x data to an R8G8B8_UNORM texture we must first upload the data to an
    // R8G8B8A8_UNORM image and then copy it.
    sk_sp<GrVkTexture> copyTexture;
    if (dataColorType == GrColorType::kRGB_888x) {
        GrSurfaceDesc surfDesc;
        surfDesc.fFlags = kRenderTarget_GrSurfaceFlag;
        surfDesc.fWidth = width;
        surfDesc.fHeight = height;
        surfDesc.fConfig = kRGBA_8888_GrPixelConfig;
        surfDesc.fSampleCnt = 1;

        VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        GrVkImage::ImageDesc imageDesc;
        imageDesc.fImageType = VK_IMAGE_TYPE_2D;
        imageDesc.fFormat = VK_FORMAT_R8G8B8A8_UNORM;
        imageDesc.fWidth = width;
        imageDesc.fHeight = height;
        imageDesc.fLevels = 1;
        imageDesc.fSamples = 1;
        imageDesc.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
        imageDesc.fUsageFlags = usageFlags;
        imageDesc.fMemProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        copyTexture = GrVkTexture::MakeNewTexture(this, SkBudgeted::kYes, surfDesc, imageDesc,
                                                  GrMipMapsStatus::kNotAllocated);
        if (!copyTexture) {
            return false;
        }
        uploadTexture = copyTexture.get();
        uploadLeft = 0;
        uploadTop = 0;
    }

    char* buffer = (char*) transferBuffer->map();
    SkTArray<VkBufferImageCopy> regions(mipLevelCount);

    currentWidth = width;
    currentHeight = height;
    int layerHeight = uploadTexture->height();
    for (int currentMipLevel = 0; currentMipLevel < mipLevelCount; currentMipLevel++) {
        if (texelsShallowCopy[currentMipLevel].fPixels) {
            SkASSERT(1 == mipLevelCount || currentHeight == layerHeight);
            const size_t trimRowBytes = currentWidth * bpp;
            const size_t rowBytes = texelsShallowCopy[currentMipLevel].fRowBytes
                                    ? texelsShallowCopy[currentMipLevel].fRowBytes
                                    : trimRowBytes;

            // copy data into the buffer, skipping the trailing bytes
            char* dst = buffer + individualMipOffsets[currentMipLevel];
            const char* src = (const char*)texelsShallowCopy[currentMipLevel].fPixels;
            SkRectMemcpy(dst, trimRowBytes, src, rowBytes, trimRowBytes, currentHeight);

            VkBufferImageCopy& region = regions.push_back();
            memset(&region, 0, sizeof(VkBufferImageCopy));
            region.bufferOffset = transferBuffer->offset() + individualMipOffsets[currentMipLevel];
            region.bufferRowLength = currentWidth;
            region.bufferImageHeight = currentHeight;
            region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, SkToU32(currentMipLevel), 0, 1 };
            region.imageOffset = {uploadLeft, uploadTop, 0};
            region.imageExtent = { (uint32_t)currentWidth, (uint32_t)currentHeight, 1 };
        }
        currentWidth = SkTMax(1, currentWidth/2);
        currentHeight = SkTMax(1, currentHeight/2);
        layerHeight = currentHeight;
    }

    // no need to flush non-coherent memory, unmap will do that for us
    transferBuffer->unmap();

    // Change layout of our target so it can be copied to
    uploadTexture->setImageLayout(this,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_ACCESS_TRANSFER_WRITE_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  false);

    // Copy the buffer to the image
    fCurrentCmdBuffer->copyBufferToImage(this,
                                         transferBuffer,
                                         uploadTexture,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         regions.count(),
                                         regions.begin());
    transferBuffer->unref();

    // If we copied the data into a temporary image first, copy that image into our main texture
    // now.
    if (copyTexture.get()) {
        SkASSERT(dataColorType == GrColorType::kRGB_888x);
        static const GrSurfaceOrigin kOrigin = kTopLeft_GrSurfaceOrigin;
        SkAssertResult(this->copySurface(tex, kOrigin, copyTexture.get(), kOrigin,
                                         SkIRect::MakeWH(width, height), SkIPoint::Make(left, top),
                                         false));
    }
    if (1 == mipLevelCount) {
        tex->texturePriv().markMipMapsDirty();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
sk_sp<GrTexture> GrVkGpu::onCreateTexture(const GrSurfaceDesc& desc, SkBudgeted budgeted,
                                          const GrMipLevel texels[], int mipLevelCount) {
    bool renderTarget = SkToBool(desc.fFlags & kRenderTarget_GrSurfaceFlag);

    VkFormat pixelFormat;
    SkAssertResult(GrPixelConfigToVkFormat(desc.fConfig, &pixelFormat));

    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
    if (renderTarget) {
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    // For now we will set the VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT and
    // VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT on every texture since we do not know whether or not we
    // will be using this texture in some copy or not. Also this assumes, as is the current case,
    // that all render targets in vulkan are also textures. If we change this practice of setting
    // both bits, we must make sure to set the destination bit if we are uploading srcData to the
    // texture.
    usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // This ImageDesc refers to the texture that will be read by the client. Thus even if msaa is
    // requested, this ImageDesc describes the resolved texture. Therefore we always have samples set
    // to 1.
    int mipLevels = !mipLevelCount ? 1 : mipLevelCount;
    GrVkImage::ImageDesc imageDesc;
    imageDesc.fImageType = VK_IMAGE_TYPE_2D;
    imageDesc.fFormat = pixelFormat;
    imageDesc.fWidth = desc.fWidth;
    imageDesc.fHeight = desc.fHeight;
    imageDesc.fLevels = mipLevels;
    imageDesc.fSamples = 1;
    imageDesc.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    imageDesc.fUsageFlags = usageFlags;
    imageDesc.fMemProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    GrMipMapsStatus mipMapsStatus = GrMipMapsStatus::kNotAllocated;
    if (mipLevels > 1) {
        mipMapsStatus = GrMipMapsStatus::kValid;
        for (int i = 0; i < mipLevels; ++i) {
            if (!texels[i].fPixels) {
                mipMapsStatus = GrMipMapsStatus::kDirty;
                break;
            }
        }
    }

    sk_sp<GrVkTexture> tex;
    if (renderTarget) {
        tex = GrVkTextureRenderTarget::MakeNewTextureRenderTarget(this, budgeted, desc,
                                                                  imageDesc,
                                                                  mipMapsStatus);
    } else {
        tex = GrVkTexture::MakeNewTexture(this, budgeted, desc, imageDesc, mipMapsStatus);
    }

    if (!tex) {
        return nullptr;
    }

    auto colorType = GrPixelConfigToColorType(desc.fConfig);
    if (mipLevelCount) {
        if (!this->uploadTexDataOptimal(tex.get(), 0, 0, desc.fWidth, desc.fHeight, colorType,
                                        texels, mipLevelCount)) {
            tex->unref();
            return nullptr;
        }
    }

    if (desc.fFlags & kPerformInitialClear_GrSurfaceFlag) {
        VkClearColorValue zeroClearColor;
        memset(&zeroClearColor, 0, sizeof(zeroClearColor));
        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseArrayLayer = 0;
        range.baseMipLevel = 0;
        range.layerCount = 1;
        range.levelCount = 1;
        tex->setImageLayout(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, false);
        this->currentCommandBuffer()->clearColorImage(this, tex.get(), &zeroClearColor, 1, &range);
    }
    return std::move(tex);
}

////////////////////////////////////////////////////////////////////////////////

void GrVkGpu::copyBuffer(GrVkBuffer* srcBuffer, GrVkBuffer* dstBuffer, VkDeviceSize srcOffset,
                         VkDeviceSize dstOffset, VkDeviceSize size) {
    VkBufferCopy copyRegion;
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    fCurrentCmdBuffer->copyBuffer(this, srcBuffer, dstBuffer, 1, &copyRegion);
}

bool GrVkGpu::updateBuffer(GrVkBuffer* buffer, const void* src,
                           VkDeviceSize offset, VkDeviceSize size) {
    // Update the buffer
    fCurrentCmdBuffer->updateBuffer(this, buffer, offset, size, src);

    return true;
}

////////////////////////////////////////////////////////////////////////////////

static bool check_backend_texture(const GrBackendTexture& backendTex,
                                  GrPixelConfig config) {
    GrVkImageInfo info;
    if (!backendTex.getVkImageInfo(&info)) {
        return false;
    }

    if (VK_NULL_HANDLE == info.fImage || VK_NULL_HANDLE == info.fAlloc.fMemory) {
        return false;
    }

    SkASSERT(GrVkFormatPixelConfigPairIsValid(info.fFormat, config));
    return true;
}

sk_sp<GrTexture> GrVkGpu::onWrapBackendTexture(const GrBackendTexture& backendTex,
                                               GrWrapOwnership ownership) {
    if (!check_backend_texture(backendTex, backendTex.config())) {
        return nullptr;
    }

    GrSurfaceDesc surfDesc;
    surfDesc.fFlags = kNone_GrSurfaceFlags;
    surfDesc.fWidth = backendTex.width();
    surfDesc.fHeight = backendTex.height();
    surfDesc.fConfig = backendTex.config();
    surfDesc.fSampleCnt = 1;

    GrVkImageInfo imageInfo;
    if (!backendTex.getVkImageInfo(&imageInfo)) {
        return nullptr;
    }
    sk_sp<GrVkImageLayout> layout = backendTex.getGrVkImageLayout();
    SkASSERT(layout);
    return GrVkTexture::MakeWrappedTexture(this, surfDesc, ownership, imageInfo, std::move(layout));
}

sk_sp<GrTexture> GrVkGpu::onWrapRenderableBackendTexture(const GrBackendTexture& backendTex,
                                                         int sampleCnt,
                                                         GrWrapOwnership ownership) {
    if (!check_backend_texture(backendTex, backendTex.config())) {
        return nullptr;
    }

    GrSurfaceDesc surfDesc;
    surfDesc.fFlags = kRenderTarget_GrSurfaceFlag;
    surfDesc.fWidth = backendTex.width();
    surfDesc.fHeight = backendTex.height();
    surfDesc.fConfig = backendTex.config();
    surfDesc.fSampleCnt = this->caps()->getRenderTargetSampleCount(sampleCnt, backendTex.config());

    GrVkImageInfo imageInfo;
    if (!backendTex.getVkImageInfo(&imageInfo)) {
        return nullptr;
    }
    sk_sp<GrVkImageLayout> layout = backendTex.getGrVkImageLayout();
    SkASSERT(layout);

    return GrVkTextureRenderTarget::MakeWrappedTextureRenderTarget(this, surfDesc, ownership,
                                                                   imageInfo, std::move(layout));
}

sk_sp<GrRenderTarget> GrVkGpu::onWrapBackendRenderTarget(const GrBackendRenderTarget& backendRT){
    // Currently the Vulkan backend does not support wrapping of msaa render targets directly. In
    // general this is not an issue since swapchain images in vulkan are never multisampled. Thus if
    // you want a multisampled RT it is best to wrap the swapchain images and then let Skia handle
    // creating and owning the MSAA images.
    if (backendRT.sampleCnt() > 1) {
        return nullptr;
    }

    GrVkImageInfo info;
    if (!backendRT.getVkImageInfo(&info)) {
        return nullptr;
    }

    if (VK_NULL_HANDLE == info.fImage) {
        return nullptr;
    }

    GrSurfaceDesc desc;
    desc.fFlags = kRenderTarget_GrSurfaceFlag;
    desc.fWidth = backendRT.width();
    desc.fHeight = backendRT.height();
    desc.fConfig = backendRT.config();
    desc.fSampleCnt = 1;

    sk_sp<GrVkImageLayout> layout = backendRT.getGrVkImageLayout();

    sk_sp<GrVkRenderTarget> tgt = GrVkRenderTarget::MakeWrappedRenderTarget(this, desc, info,
                                                                            std::move(layout));

    // We don't allow the client to supply a premade stencil buffer. We always create one if needed.
    SkASSERT(!backendRT.stencilBits());
    if (tgt) {
        SkASSERT(tgt->canAttemptStencilAttachment());
    }

    return std::move(tgt);
}

sk_sp<GrRenderTarget> GrVkGpu::onWrapBackendTextureAsRenderTarget(const GrBackendTexture& tex,
                                                                  int sampleCnt) {

    GrVkImageInfo imageInfo;
    if (!tex.getVkImageInfo(&imageInfo)) {
        return nullptr;
    }
    if (VK_NULL_HANDLE == imageInfo.fImage) {
        return nullptr;
    }

    GrSurfaceDesc desc;
    desc.fFlags = kRenderTarget_GrSurfaceFlag;
    desc.fWidth = tex.width();
    desc.fHeight = tex.height();
    desc.fConfig = tex.config();
    desc.fSampleCnt = this->caps()->getRenderTargetSampleCount(sampleCnt, tex.config());
    if (!desc.fSampleCnt) {
        return nullptr;
    }

    sk_sp<GrVkImageLayout> layout = tex.getGrVkImageLayout();
    SkASSERT(layout);

    return GrVkRenderTarget::MakeWrappedRenderTarget(this, desc, imageInfo, std::move(layout));
}

bool GrVkGpu::onRegenerateMipMapLevels(GrTexture* tex) {
    auto* vkTex = static_cast<GrVkTexture*>(tex);
    // don't do anything for linearly tiled textures (can't have mipmaps)
    if (vkTex->isLinearTiled()) {
        SkDebugf("Trying to create mipmap for linear tiled texture");
        return false;
    }

    // determine if we can blit to and from this format
    const GrVkCaps& caps = this->vkCaps();
    if (!caps.configCanBeDstofBlit(tex->config(), false) ||
        !caps.configCanBeSrcofBlit(tex->config(), false) ||
        !caps.mipMapSupport()) {
        return false;
    }

    if (this->vkCaps().mustSubmitCommandsBeforeCopyOp()) {
        this->submitCommandBuffer(kSkip_SyncQueue);
    }

    int width = tex->width();
    int height = tex->height();
    VkImageBlit blitRegion;
    memset(&blitRegion, 0, sizeof(VkImageBlit));

    // SkMipMap doesn't include the base level in the level count so we have to add 1
    uint32_t levelCount = SkMipMap::ComputeLevelCount(tex->width(), tex->height()) + 1;
    SkASSERT(levelCount == vkTex->mipLevels());

    // change layout of the layers so we can write to them.
    vkTex->setImageLayout(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, false);

    // setup memory barrier
    SkASSERT(GrVkFormatIsSupported(vkTex->imageFormat()));
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageMemoryBarrier imageMemoryBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // sType
            nullptr,                                 // pNext
            VK_ACCESS_TRANSFER_WRITE_BIT,            // srcAccessMask
            VK_ACCESS_TRANSFER_READ_BIT,             // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,    // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,    // newLayout
            VK_QUEUE_FAMILY_IGNORED,                 // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                 // dstQueueFamilyIndex
            vkTex->image(),                          // image
            {aspectFlags, 0, 1, 0, 1}                // subresourceRange
    };

    // Blit the miplevels
    uint32_t mipLevel = 1;
    while (mipLevel < levelCount) {
        int prevWidth = width;
        int prevHeight = height;
        width = SkTMax(1, width / 2);
        height = SkTMax(1, height / 2);

        imageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel - 1;
        this->addImageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    false, &imageMemoryBarrier);

        blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1, 0, 1 };
        blitRegion.srcOffsets[0] = { 0, 0, 0 };
        blitRegion.srcOffsets[1] = { prevWidth, prevHeight, 1 };
        blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1 };
        blitRegion.dstOffsets[0] = { 0, 0, 0 };
        blitRegion.dstOffsets[1] = { width, height, 1 };
        fCurrentCmdBuffer->blitImage(this,
                                     vkTex->resource(),
                                     vkTex->image(),
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     vkTex->resource(),
                                     vkTex->image(),
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1,
                                     &blitRegion,
                                     VK_FILTER_LINEAR);
        ++mipLevel;
    }
    // This barrier logically is not needed, but it changes the final level to the same layout as
    // all the others, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL. This makes tracking of the layouts and
    // future layout changes easier.
    imageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel - 1;
    this->addImageMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                false, &imageMemoryBarrier);
    vkTex->updateImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

GrStencilAttachment* GrVkGpu::createStencilAttachmentForRenderTarget(const GrRenderTarget* rt,
                                                                     int width,
                                                                     int height) {
    SkASSERT(width >= rt->width());
    SkASSERT(height >= rt->height());

    int samples = rt->numStencilSamples();

    const GrVkCaps::StencilFormat& sFmt = this->vkCaps().preferredStencilFormat();

    GrVkStencilAttachment* stencil(GrVkStencilAttachment::Create(this,
                                                                 width,
                                                                 height,
                                                                 samples,
                                                                 sFmt));
    fStats.incStencilAttachmentCreates();
    return stencil;
}

////////////////////////////////////////////////////////////////////////////////

bool copy_testing_data(GrVkGpu* gpu, const void* srcData, const GrVkAlloc& alloc,
                       size_t bufferOffset, size_t srcRowBytes, size_t dstRowBytes,
                       size_t trimRowBytes, int h) {
    VkDeviceSize size = dstRowBytes * h;
    VkDeviceSize offset = bufferOffset;
    SkASSERT(size + offset <= alloc.fSize);
    void* mapPtr = GrVkMemory::MapAlloc(gpu, alloc);
    if (!mapPtr) {
        return false;
    }
    mapPtr = reinterpret_cast<char*>(mapPtr) + offset;

    if (srcData) {
        // If there is no padding on dst we can do a single memcopy.
        // This assumes the srcData comes in with no padding.
        SkRectMemcpy(mapPtr, dstRowBytes, srcData, srcRowBytes, trimRowBytes, h);
    } else {
        // If there is no srcdata we always copy 0's into the textures so that it is initialized
        // with some data.
        memset(mapPtr, 0, dstRowBytes * h);
    }
    GrVkMemory::FlushMappedAlloc(gpu, alloc, offset, size);
    GrVkMemory::UnmapAlloc(gpu, alloc);
    return true;
}

#if GR_TEST_UTILS
bool GrVkGpu::createTestingOnlyVkImage(GrPixelConfig config, int w, int h, bool texturable,
                                       bool renderable, GrMipMapped mipMapped, const void* srcData,
                                       size_t srcRowBytes, GrVkImageInfo* info) {
    SkASSERT(texturable || renderable);
    if (!texturable) {
        SkASSERT(GrMipMapped::kNo == mipMapped);
        SkASSERT(!srcData);
    }
    VkFormat pixelFormat;
    if (!GrPixelConfigToVkFormat(config, &pixelFormat)) {
        return false;
    }

    if (texturable && !fVkCaps->isConfigTexturable(config)) {
        return false;
    }

    if (renderable && !fVkCaps->isConfigRenderable(config)) {
        return false;
    }

    // Currently we don't support uploading pixel data when mipped.
    if (srcData && GrMipMapped::kYes == mipMapped) {
        return false;
    }

    VkImageUsageFlags usageFlags = 0;
    usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (texturable) {
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (renderable) {
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    VkImage image = VK_NULL_HANDLE;
    GrVkAlloc alloc;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Create Image
    VkSampleCountFlagBits vkSamples;
    if (!GrSampleCountToVkSampleCount(1, &vkSamples)) {
        return false;
    }

    // Figure out the number of mip levels.
    uint32_t mipLevels = 1;
    if (GrMipMapped::kYes == mipMapped) {
        mipLevels = SkMipMap::ComputeLevelCount(w, h) + 1;
    }

    const VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,  // sType
            nullptr,                              // pNext
            0,                                    // VkImageCreateFlags
            VK_IMAGE_TYPE_2D,                     // VkImageType
            pixelFormat,                          // VkFormat
            {(uint32_t)w, (uint32_t)h, 1},        // VkExtent3D
            mipLevels,                            // mipLevels
            1,                                    // arrayLayers
            vkSamples,                            // samples
            VK_IMAGE_TILING_OPTIMAL,              // VkImageTiling
            usageFlags,                           // VkImageUsageFlags
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode
            0,                                    // queueFamilyCount
            0,                                    // pQueueFamilyIndices
            initialLayout                         // initialLayout
    };

    GR_VK_CALL_ERRCHECK(this->vkInterface(),
                        CreateImage(this->device(), &imageCreateInfo, nullptr, &image));

    if (!GrVkMemory::AllocAndBindImageMemory(this, image, false, &alloc)) {
        VK_CALL(DestroyImage(this->device(), image, nullptr));
        return false;
    }

    // We need to declare these early so that we can delete them at the end outside of the if block.
    GrVkAlloc bufferAlloc;
    VkBuffer buffer = VK_NULL_HANDLE;

    VkResult err;
    const VkCommandBufferAllocateInfo cmdInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,   // sType
        nullptr,                                          // pNext
        fCmdPool,                                         // commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                  // level
        1                                                 // bufferCount
    };

    VkCommandBuffer cmdBuffer;
    err = VK_CALL(AllocateCommandBuffers(fDevice, &cmdInfo, &cmdBuffer));
    if (err) {
        GrVkMemory::FreeImageMemory(this, false, alloc);
        VK_CALL(DestroyImage(fDevice, image, nullptr));
        return false;
    }

    VkCommandBufferBeginInfo cmdBufferBeginInfo;
    memset(&cmdBufferBeginInfo, 0, sizeof(VkCommandBufferBeginInfo));
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.pNext = nullptr;
    cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cmdBufferBeginInfo.pInheritanceInfo = nullptr;

    err = VK_CALL(BeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
    SkASSERT(!err);

    size_t bpp = GrBytesPerPixel(config);
    SkASSERT(w && h);

    const size_t trimRowBytes = w * bpp;
    if (!srcRowBytes) {
        srcRowBytes = trimRowBytes;
    }

    SkTArray<size_t> individualMipOffsets(mipLevels);
    individualMipOffsets.push_back(0);
    size_t combinedBufferSize = w * bpp * h;
    int currentWidth = w;
    int currentHeight = h;
    // The alignment must be at least 4 bytes and a multiple of the bytes per pixel of the image
    // config. This works with the assumption that the bytes in pixel config is always a power
    // of 2.
    SkASSERT((bpp & (bpp - 1)) == 0);
    const size_t alignmentMask = 0x3 | (bpp - 1);
    for (uint32_t currentMipLevel = 1; currentMipLevel < mipLevels; currentMipLevel++) {
        currentWidth = SkTMax(1, currentWidth / 2);
        currentHeight = SkTMax(1, currentHeight / 2);

        const size_t trimmedSize = currentWidth * bpp * currentHeight;
        const size_t alignmentDiff = combinedBufferSize & alignmentMask;
        if (alignmentDiff != 0) {
            combinedBufferSize += alignmentMask - alignmentDiff + 1;
        }
        individualMipOffsets.push_back(combinedBufferSize);
        combinedBufferSize += trimmedSize;
    }

    VkBufferCreateInfo bufInfo;
    memset(&bufInfo, 0, sizeof(VkBufferCreateInfo));
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.flags = 0;
    bufInfo.size = combinedBufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufInfo.queueFamilyIndexCount = 0;
    bufInfo.pQueueFamilyIndices = nullptr;
    err = VK_CALL(CreateBuffer(fDevice, &bufInfo, nullptr, &buffer));

    if (err) {
        GrVkMemory::FreeImageMemory(this, false, alloc);
        VK_CALL(DestroyImage(fDevice, image, nullptr));
        VK_CALL(EndCommandBuffer(cmdBuffer));
        VK_CALL(FreeCommandBuffers(fDevice, fCmdPool, 1, &cmdBuffer));
        return false;
    }

    if (!GrVkMemory::AllocAndBindBufferMemory(this, buffer, GrVkBuffer::kCopyRead_Type, true,
                                              &bufferAlloc)) {
        GrVkMemory::FreeImageMemory(this, false, alloc);
        VK_CALL(DestroyImage(fDevice, image, nullptr));
        VK_CALL(DestroyBuffer(fDevice, buffer, nullptr));
        VK_CALL(EndCommandBuffer(cmdBuffer));
        VK_CALL(FreeCommandBuffers(fDevice, fCmdPool, 1, &cmdBuffer));
        return false;
    }

    currentWidth = w;
    currentHeight = h;
    for (uint32_t currentMipLevel = 0; currentMipLevel < mipLevels; currentMipLevel++) {
        SkASSERT(0 == currentMipLevel || !srcData);
        size_t currentRowBytes = bpp * currentWidth;
        size_t bufferOffset = individualMipOffsets[currentMipLevel];
        if (!copy_testing_data(this, srcData, bufferAlloc, bufferOffset, srcRowBytes,
                               currentRowBytes, trimRowBytes, currentHeight)) {
            GrVkMemory::FreeImageMemory(this, false, alloc);
            VK_CALL(DestroyImage(fDevice, image, nullptr));
            GrVkMemory::FreeBufferMemory(this, GrVkBuffer::kCopyRead_Type, bufferAlloc);
            VK_CALL(DestroyBuffer(fDevice, buffer, nullptr));
            VK_CALL(EndCommandBuffer(cmdBuffer));
            VK_CALL(FreeCommandBuffers(fDevice, fCmdPool, 1, &cmdBuffer));
            return false;
        }
        currentWidth = SkTMax(1, currentWidth / 2);
        currentHeight = SkTMax(1, currentHeight / 2);
    }

    // Set image layout and add barrier
    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(VkImageMemoryBarrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = GrVkImage::LayoutToSrcAccessMask(initialLayout);
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = initialLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};

    VK_CALL(CmdPipelineBarrier(cmdBuffer, GrVkImage::LayoutToPipelineSrcStageFlags(initialLayout),
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                               &barrier));
    initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    SkTArray<VkBufferImageCopy> regions(mipLevels);

    currentWidth = w;
    currentHeight = h;
    for (uint32_t currentMipLevel = 0; currentMipLevel < mipLevels; currentMipLevel++) {
        // Submit copy command
        VkBufferImageCopy& region = regions.push_back();
        memset(&region, 0, sizeof(VkBufferImageCopy));
        region.bufferOffset = individualMipOffsets[currentMipLevel];
        region.bufferRowLength = currentWidth;
        region.bufferImageHeight = currentHeight;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {(uint32_t)currentWidth, (uint32_t)currentHeight, 1};
        currentWidth = SkTMax(1, currentWidth / 2);
        currentHeight = SkTMax(1, currentHeight / 2);
    }

    VK_CALL(CmdCopyBufferToImage(cmdBuffer, buffer, image, initialLayout, regions.count(),
                                 regions.begin()));

    if (texturable) {
        // Change Image layout to shader read since if we use this texture as a borrowed textures
        // within Ganesh we require that its layout be set to that
        memset(&barrier, 0, sizeof(VkImageMemoryBarrier));
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = GrVkImage::LayoutToSrcAccessMask(initialLayout);
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = initialLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
        VK_CALL(CmdPipelineBarrier(cmdBuffer,
                                   GrVkImage::LayoutToPipelineSrcStageFlags(initialLayout),
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                   0,
                                   0, nullptr,
                                   0, nullptr,
                                   1, &barrier));
        initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // End CommandBuffer
    err = VK_CALL(EndCommandBuffer(cmdBuffer));
    SkASSERT(!err);

    // Create Fence for queue
    VkFence fence;
    VkFenceCreateInfo fenceInfo;
    memset(&fenceInfo, 0, sizeof(VkFenceCreateInfo));
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    err = VK_CALL(CreateFence(fDevice, &fenceInfo, nullptr, &fence));
    SkASSERT(!err);

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(VkSubmitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = 0;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;
    err = VK_CALL(QueueSubmit(this->queue(), 1, &submitInfo, fence));
    SkASSERT(!err);

    err = VK_CALL(WaitForFences(fDevice, 1, &fence, true, UINT64_MAX));
    if (VK_TIMEOUT == err) {
        GrVkMemory::FreeImageMemory(this, false, alloc);
        VK_CALL(DestroyImage(fDevice, image, nullptr));
        GrVkMemory::FreeBufferMemory(this, GrVkBuffer::kCopyRead_Type, bufferAlloc);
        VK_CALL(DestroyBuffer(fDevice, buffer, nullptr));
        VK_CALL(FreeCommandBuffers(fDevice, fCmdPool, 1, &cmdBuffer));
        VK_CALL(DestroyFence(fDevice, fence, nullptr));
        SkDebugf("Fence failed to signal: %d\n", err);
        SK_ABORT("failing");
    }
    SkASSERT(!err);

    // Clean up transfer resources
    if (buffer != VK_NULL_HANDLE) { // workaround for an older NVidia driver crash
        GrVkMemory::FreeBufferMemory(this, GrVkBuffer::kCopyRead_Type, bufferAlloc);
        VK_CALL(DestroyBuffer(fDevice, buffer, nullptr));
    }
    VK_CALL(FreeCommandBuffers(fDevice, fCmdPool, 1, &cmdBuffer));
    VK_CALL(DestroyFence(fDevice, fence, nullptr));

    info->fImage = image;
    info->fAlloc = alloc;
    info->fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    info->fImageLayout = initialLayout;
    info->fFormat = pixelFormat;
    info->fLevelCount = mipLevels;

    return true;
}

GrBackendTexture GrVkGpu::createTestingOnlyBackendTexture(const void* srcData, int w, int h,
                                                          GrColorType colorType,
                                                          bool isRenderTarget,
                                                          GrMipMapped mipMapped, size_t rowBytes) {
    this->handleDirtyContext();

    if (w > this->caps()->maxTextureSize() || h > this->caps()->maxTextureSize()) {
        return GrBackendTexture();
    }

    GrPixelConfig config = GrColorTypeToPixelConfig(colorType, GrSRGBEncoded::kNo);
    if (!this->caps()->isConfigTexturable(config)) {
        return GrBackendTexture();
    }

    GrVkImageInfo info;
    if (!this->createTestingOnlyVkImage(config, w, h, true, isRenderTarget, mipMapped, srcData,
                                        rowBytes, &info)) {
        return {};
    }
    GrBackendTexture beTex = GrBackendTexture(w, h, info);
    // Lots of tests don't go through Skia's public interface which will set the config so for
    // testing we make sure we set a config here.
    beTex.setPixelConfig(config);
    return beTex;
}

bool GrVkGpu::isTestingOnlyBackendTexture(const GrBackendTexture& tex) const {
    SkASSERT(kVulkan_GrBackend == tex.fBackend);

    GrVkImageInfo backend;
    if (!tex.getVkImageInfo(&backend)) {
        return false;
    }

    if (backend.fImage && backend.fAlloc.fMemory) {
        VkMemoryRequirements req;
        memset(&req, 0, sizeof(req));
        GR_VK_CALL(this->vkInterface(), GetImageMemoryRequirements(fDevice,
                                                                   backend.fImage,
                                                                   &req));
        // TODO: find a better check
        // This will probably fail with a different driver
        return (req.size > 0) && (req.size <= 8192 * 8192);
    }

    return false;
}

void GrVkGpu::deleteTestingOnlyBackendTexture(const GrBackendTexture& tex) {
    SkASSERT(kVulkan_GrBackend == tex.fBackend);

    GrVkImageInfo info;
    if (tex.getVkImageInfo(&info)) {
        GrVkImage::DestroyImageInfo(this, const_cast<GrVkImageInfo*>(&info));
    }
}

GrBackendRenderTarget GrVkGpu::createTestingOnlyBackendRenderTarget(int w, int h, GrColorType ct) {
    if (w > this->caps()->maxRenderTargetSize() || h > this->caps()->maxRenderTargetSize()) {
        return GrBackendRenderTarget();
    }

    this->handleDirtyContext();
    GrVkImageInfo info;
    auto config = GrColorTypeToPixelConfig(ct, GrSRGBEncoded::kNo);
    if (kUnknown_GrPixelConfig == config) {
        return {};
    }
    if (!this->createTestingOnlyVkImage(config, w, h, false, true, GrMipMapped::kNo, nullptr, 0,
                                        &info)) {
        return {};
    }
    GrBackendRenderTarget beRT = GrBackendRenderTarget(w, h, 1, 0, info);
    // Lots of tests don't go through Skia's public interface which will set the config so for
    // testing we make sure we set a config here.
    beRT.setPixelConfig(config);
    return beRT;
}

void GrVkGpu::deleteTestingOnlyBackendRenderTarget(const GrBackendRenderTarget& rt) {
    SkASSERT(kVulkan_GrBackend == rt.fBackend);

    GrVkImageInfo info;
    if (rt.getVkImageInfo(&info)) {
        // something in the command buffer may still be using this, so force submit
        this->submitCommandBuffer(kForce_SyncQueue);
        GrVkImage::DestroyImageInfo(this, const_cast<GrVkImageInfo*>(&info));
    }
}

void GrVkGpu::testingOnly_flushGpuAndSync() {
    this->submitCommandBuffer(kForce_SyncQueue);
}
#endif

////////////////////////////////////////////////////////////////////////////////

void GrVkGpu::addMemoryBarrier(VkPipelineStageFlags srcStageMask,
                               VkPipelineStageFlags dstStageMask,
                               bool byRegion,
                               VkMemoryBarrier* barrier) const {
    SkASSERT(fCurrentCmdBuffer);
    fCurrentCmdBuffer->pipelineBarrier(this,
                                       srcStageMask,
                                       dstStageMask,
                                       byRegion,
                                       GrVkCommandBuffer::kMemory_BarrierType,
                                       barrier);
}

void GrVkGpu::addBufferMemoryBarrier(VkPipelineStageFlags srcStageMask,
                                     VkPipelineStageFlags dstStageMask,
                                     bool byRegion,
                                     VkBufferMemoryBarrier* barrier) const {
    SkASSERT(fCurrentCmdBuffer);
    fCurrentCmdBuffer->pipelineBarrier(this,
                                       srcStageMask,
                                       dstStageMask,
                                       byRegion,
                                       GrVkCommandBuffer::kBufferMemory_BarrierType,
                                       barrier);
}

void GrVkGpu::addImageMemoryBarrier(VkPipelineStageFlags srcStageMask,
                                    VkPipelineStageFlags dstStageMask,
                                    bool byRegion,
                                    VkImageMemoryBarrier* barrier) const {
    SkASSERT(fCurrentCmdBuffer);
    fCurrentCmdBuffer->pipelineBarrier(this,
                                       srcStageMask,
                                       dstStageMask,
                                       byRegion,
                                       GrVkCommandBuffer::kImageMemory_BarrierType,
                                       barrier);
}

void GrVkGpu::onFinishFlush(bool insertedSemaphore) {
    // Submit the current command buffer to the Queue. Whether we inserted semaphores or not does
    // not effect what we do here.
    this->submitCommandBuffer(kSkip_SyncQueue);
}

static int get_surface_sample_cnt(GrSurface* surf) {
    if (const GrRenderTarget* rt = surf->asRenderTarget()) {
        return rt->numColorSamples();
    }
    return 0;
}

void GrVkGpu::copySurfaceAsCopyImage(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                                     GrSurface* src, GrSurfaceOrigin srcOrigin,
                                     GrVkImage* dstImage,
                                     GrVkImage* srcImage,
                                     const SkIRect& srcRect,
                                     const SkIPoint& dstPoint) {
#ifdef SK_DEBUG
    int dstSampleCnt = get_surface_sample_cnt(dst);
    int srcSampleCnt = get_surface_sample_cnt(src);
    SkASSERT(this->vkCaps().canCopyImage(dst->config(), dstSampleCnt, dstOrigin,
                                         src->config(), srcSampleCnt, srcOrigin));

#endif

    // These flags are for flushing/invalidating caches and for the dst image it doesn't matter if
    // the cache is flushed since it is only being written to.
    dstImage->setImageLayout(this,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             false);

    srcImage->setImageLayout(this,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_ACCESS_TRANSFER_READ_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             false);

    // Flip rect if necessary
    SkIRect srcVkRect = srcRect;
    int32_t dstY = dstPoint.fY;

    if (kBottomLeft_GrSurfaceOrigin == srcOrigin) {
        SkASSERT(kBottomLeft_GrSurfaceOrigin == dstOrigin);
        srcVkRect.fTop = src->height() - srcRect.fBottom;
        srcVkRect.fBottom =  src->height() - srcRect.fTop;
        dstY = dst->height() - dstPoint.fY - srcVkRect.height();
    }

    VkImageCopy copyRegion;
    memset(&copyRegion, 0, sizeof(VkImageCopy));
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.srcOffset = { srcVkRect.fLeft, srcVkRect.fTop, 0 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstOffset = { dstPoint.fX, dstY, 0 };
    copyRegion.extent = { (uint32_t)srcVkRect.width(), (uint32_t)srcVkRect.height(), 1 };

    fCurrentCmdBuffer->copyImage(this,
                                 srcImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 dstImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 1,
                                 &copyRegion);

    SkIRect dstRect = SkIRect::MakeXYWH(dstPoint.fX, dstPoint.fY,
                                        srcRect.width(), srcRect.height());
    this->didWriteToSurface(dst, dstOrigin, &dstRect);
}

void GrVkGpu::copySurfaceAsBlit(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                                GrSurface* src, GrSurfaceOrigin srcOrigin,
                                GrVkImage* dstImage,
                                GrVkImage* srcImage,
                                const SkIRect& srcRect,
                                const SkIPoint& dstPoint) {
#ifdef SK_DEBUG
    int dstSampleCnt = get_surface_sample_cnt(dst);
    int srcSampleCnt = get_surface_sample_cnt(src);
    SkASSERT(this->vkCaps().canCopyAsBlit(dst->config(), dstSampleCnt, dstImage->isLinearTiled(),
                                          src->config(), srcSampleCnt, srcImage->isLinearTiled()));

#endif
    dstImage->setImageLayout(this,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             false);

    srcImage->setImageLayout(this,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_ACCESS_TRANSFER_READ_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             false);

    // Flip rect if necessary
    SkIRect srcVkRect;
    srcVkRect.fLeft = srcRect.fLeft;
    srcVkRect.fRight = srcRect.fRight;
    SkIRect dstRect;
    dstRect.fLeft = dstPoint.fX;
    dstRect.fRight = dstPoint.fX + srcRect.width();

    if (kBottomLeft_GrSurfaceOrigin == srcOrigin) {
        srcVkRect.fTop = src->height() - srcRect.fBottom;
        srcVkRect.fBottom = src->height() - srcRect.fTop;
    } else {
        srcVkRect.fTop = srcRect.fTop;
        srcVkRect.fBottom = srcRect.fBottom;
    }

    if (kBottomLeft_GrSurfaceOrigin == dstOrigin) {
        dstRect.fTop = dst->height() - dstPoint.fY - srcVkRect.height();
    } else {
        dstRect.fTop = dstPoint.fY;
    }
    dstRect.fBottom = dstRect.fTop + srcVkRect.height();

    // If we have different origins, we need to flip the top and bottom of the dst rect so that we
    // get the correct origintation of the copied data.
    if (srcOrigin != dstOrigin) {
        using std::swap;
        swap(dstRect.fTop, dstRect.fBottom);
    }

    VkImageBlit blitRegion;
    memset(&blitRegion, 0, sizeof(VkImageBlit));
    blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.srcOffsets[0] = { srcVkRect.fLeft, srcVkRect.fTop, 0 };
    blitRegion.srcOffsets[1] = { srcVkRect.fRight, srcVkRect.fBottom, 1 };
    blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.dstOffsets[0] = { dstRect.fLeft, dstRect.fTop, 0 };
    blitRegion.dstOffsets[1] = { dstRect.fRight, dstRect.fBottom, 1 };

    fCurrentCmdBuffer->blitImage(this,
                                 *srcImage,
                                 *dstImage,
                                 1,
                                 &blitRegion,
                                 VK_FILTER_NEAREST); // We never scale so any filter works here

    dstRect = SkIRect::MakeXYWH(dstPoint.fX, dstPoint.fY, srcRect.width(), srcRect.height());
    this->didWriteToSurface(dst, dstOrigin, &dstRect);
}

void GrVkGpu::copySurfaceAsResolve(GrSurface* dst, GrSurfaceOrigin dstOrigin, GrSurface* src,
                                   GrSurfaceOrigin srcOrigin, const SkIRect& origSrcRect,
                                   const SkIPoint& origDstPoint) {
    GrVkRenderTarget* srcRT = static_cast<GrVkRenderTarget*>(src->asRenderTarget());
    SkIRect srcRect = origSrcRect;
    SkIPoint dstPoint = origDstPoint;
    if (kBottomLeft_GrSurfaceOrigin == srcOrigin) {
        SkASSERT(kBottomLeft_GrSurfaceOrigin == dstOrigin);
        srcRect = {origSrcRect.fLeft, src->height() - origSrcRect.fBottom,
                   origSrcRect.fRight, src->height() - origSrcRect.fTop};
        dstPoint.fY = dst->height() - dstPoint.fY - srcRect.height();
    }
    this->resolveImage(dst, srcRT, srcRect, dstPoint);
    SkIRect dstRect = SkIRect::MakeXYWH(origDstPoint.fX, origDstPoint.fY,
                                        srcRect.width(), srcRect.height());
    this->didWriteToSurface(dst, dstOrigin, &dstRect);
}

bool GrVkGpu::onCopySurface(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                            GrSurface* src, GrSurfaceOrigin srcOrigin,
                            const SkIRect& srcRect, const SkIPoint& dstPoint,
                            bool canDiscardOutsideDstRect) {
    GrPixelConfig dstConfig = dst->config();
    GrPixelConfig srcConfig = src->config();

    int dstSampleCnt = get_surface_sample_cnt(dst);
    int srcSampleCnt = get_surface_sample_cnt(src);

    if (this->vkCaps().canCopyAsResolve(dstConfig, dstSampleCnt, dstOrigin,
                                        srcConfig, srcSampleCnt, srcOrigin)) {
        this->copySurfaceAsResolve(dst, dstOrigin, src, srcOrigin, srcRect, dstPoint);
        return true;
    }

    if (this->vkCaps().mustSubmitCommandsBeforeCopyOp()) {
        this->submitCommandBuffer(GrVkGpu::kSkip_SyncQueue);
    }

    if (this->vkCaps().canCopyAsDraw(dstConfig, SkToBool(dst->asRenderTarget()),
                                     srcConfig, SkToBool(src->asTexture()))) {
        SkAssertResult(fCopyManager.copySurfaceAsDraw(this, dst, dstOrigin, src, srcOrigin, srcRect,
                                                      dstPoint, canDiscardOutsideDstRect));
        auto dstRect = srcRect.makeOffset(dstPoint.fX, dstPoint.fY);
        this->didWriteToSurface(dst, dstOrigin, &dstRect);
        return true;
    }

    GrVkImage* dstImage;
    GrVkImage* srcImage;
    GrRenderTarget* dstRT = dst->asRenderTarget();
    if (dstRT) {
        GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(dstRT);
        dstImage = vkRT->numColorSamples() > 1 ? vkRT->msaaImage() : vkRT;
    } else {
        SkASSERT(dst->asTexture());
        dstImage = static_cast<GrVkTexture*>(dst->asTexture());
    }
    GrRenderTarget* srcRT = src->asRenderTarget();
    if (srcRT) {
        GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(srcRT);
        srcImage = vkRT->numColorSamples() > 1 ? vkRT->msaaImage() : vkRT;
    } else {
        SkASSERT(src->asTexture());
        srcImage = static_cast<GrVkTexture*>(src->asTexture());
    }

    if (this->vkCaps().canCopyImage(dstConfig, dstSampleCnt, dstOrigin,
                                    srcConfig, srcSampleCnt, srcOrigin)) {
        this->copySurfaceAsCopyImage(dst, dstOrigin, src, srcOrigin, dstImage, srcImage,
                                     srcRect, dstPoint);
        return true;
    }

    if (this->vkCaps().canCopyAsBlit(dstConfig, dstSampleCnt, dstImage->isLinearTiled(),
                                     srcConfig, srcSampleCnt, srcImage->isLinearTiled())) {
        this->copySurfaceAsBlit(dst, dstOrigin, src, srcOrigin, dstImage, srcImage,
                                srcRect, dstPoint);
        return true;
    }

    return false;
}

bool GrVkGpu::onReadPixels(GrSurface* surface, int left, int top, int width, int height,
                           GrColorType dstColorType, void* buffer, size_t rowBytes) {
    if (GrPixelConfigToColorType(surface->config()) != dstColorType) {
        return false;
    }

    GrVkImage* image = nullptr;
    GrVkRenderTarget* rt = static_cast<GrVkRenderTarget*>(surface->asRenderTarget());
    if (rt) {
        // resolve the render target if necessary
        switch (rt->getResolveType()) {
            case GrVkRenderTarget::kCantResolve_ResolveType:
                return false;
            case GrVkRenderTarget::kAutoResolves_ResolveType:
                break;
            case GrVkRenderTarget::kCanResolve_ResolveType:
                this->internalResolveRenderTarget(rt, false);
                break;
            default:
                SK_ABORT("Unknown resolve type");
        }
        image = rt;
    } else {
        image = static_cast<GrVkTexture*>(surface->asTexture());
    }

    if (!image) {
        return false;
    }

    // Skia's RGB_888x color type, which we map to the vulkan R8G8B8_UNORM, expects the data to be
    // 32 bits, but the Vulkan format is only 24. So we first copy the surface into an R8G8B8A8
    // image and then do the read pixels from that.
    sk_sp<GrVkTextureRenderTarget> copySurface;
    if (dstColorType == GrColorType::kRGB_888x) {
        SkASSERT(image->imageFormat() == VK_FORMAT_R8G8B8_UNORM &&
                 surface->config() == kRGB_888_GrPixelConfig);

        // Make a new surface that is RGBA to copy the RGB surface into.
        GrSurfaceDesc surfDesc;
        surfDesc.fFlags = kRenderTarget_GrSurfaceFlag;
        surfDesc.fWidth = width;
        surfDesc.fHeight = height;
        surfDesc.fConfig = kRGBA_8888_GrPixelConfig;
        surfDesc.fSampleCnt = 1;

        VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        GrVkImage::ImageDesc imageDesc;
        imageDesc.fImageType = VK_IMAGE_TYPE_2D;
        imageDesc.fFormat = VK_FORMAT_R8G8B8A8_UNORM;
        imageDesc.fWidth = width;
        imageDesc.fHeight = height;
        imageDesc.fLevels = 1;
        imageDesc.fSamples = 1;
        imageDesc.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
        imageDesc.fUsageFlags = usageFlags;
        imageDesc.fMemProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        copySurface = GrVkTextureRenderTarget::MakeNewTextureRenderTarget(
                this, SkBudgeted::kYes, surfDesc, imageDesc, GrMipMapsStatus::kNotAllocated);
        if (!copySurface) {
            return false;
        }

        int srcSampleCount = 0;
        if (rt) {
            srcSampleCount = rt->numColorSamples();
        }
        static const GrSurfaceOrigin kOrigin = kTopLeft_GrSurfaceOrigin;
        if (!this->vkCaps().canCopyAsBlit(copySurface->config(), 1, kOrigin,
                                          surface->config(), srcSampleCount, kOrigin) &&
            !this->vkCaps().canCopyAsDraw(copySurface->config(), false,
                                          surface->config(), SkToBool(surface->asTexture()))) {
            return false;
        }
        SkIRect srcRect = SkIRect::MakeXYWH(left, top, width, height);
        if (!this->copySurface(copySurface.get(), kOrigin, surface, kOrigin,
                               srcRect, SkIPoint::Make(0,0))) {
            return false;
        }
        top = 0;
        left = 0;
        dstColorType = GrColorType::kRGBA_8888;
        image = copySurface.get();
    }

    // Change layout of our target so it can be used as copy
    image->setImageLayout(this,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          false);

    int bpp = GrColorTypeBytesPerPixel(dstColorType);
    size_t tightRowBytes = bpp * width;

    VkBufferImageCopy region;
    memset(&region, 0, sizeof(VkBufferImageCopy));

    bool copyFromOrigin = this->vkCaps().mustDoCopiesFromOrigin();
    if (copyFromOrigin) {
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { (uint32_t)(left + width), (uint32_t)(top + height), 1 };
    } else {
        VkOffset3D offset = { left, top, 0 };
        region.imageOffset = offset;
        region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };
    }

    size_t transBufferRowBytes = bpp * region.imageExtent.width;
    size_t imageRows = region.imageExtent.height;
    GrVkTransferBuffer* transferBuffer =
            static_cast<GrVkTransferBuffer*>(this->createBuffer(transBufferRowBytes * imageRows,
                                                                kXferGpuToCpu_GrBufferType,
                                                                kStream_GrAccessPattern));

    // Copy the image to a buffer so we can map it to cpu memory
    region.bufferOffset = transferBuffer->offset();
    region.bufferRowLength = 0; // Forces RowLength to be width. We handle the rowBytes below.
    region.bufferImageHeight = 0; // Forces height to be tightly packed. Only useful for 3d images.
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

    fCurrentCmdBuffer->copyImageToBuffer(this,
                                         image,
                                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         transferBuffer,
                                         1,
                                         &region);

    // make sure the copy to buffer has finished
    transferBuffer->addMemoryBarrier(this,
                                     VK_ACCESS_TRANSFER_WRITE_BIT,
                                     VK_ACCESS_HOST_READ_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_HOST_BIT,
                                     false);

    // We need to submit the current command buffer to the Queue and make sure it finishes before
    // we can copy the data out of the buffer.
    this->submitCommandBuffer(kForce_SyncQueue);
    void* mappedMemory = transferBuffer->map();
    const GrVkAlloc& transAlloc = transferBuffer->alloc();
    GrVkMemory::InvalidateMappedAlloc(this, transAlloc, 0, transAlloc.fSize);

    if (copyFromOrigin) {
        uint32_t skipRows = region.imageExtent.height - height;
        mappedMemory = (char*)mappedMemory + transBufferRowBytes * skipRows + bpp * left;
    }

    SkRectMemcpy(buffer, rowBytes, mappedMemory, transBufferRowBytes, tightRowBytes, height);

    transferBuffer->unmap();
    transferBuffer->unref();
    return true;
}

// The RenderArea bounds we pass into BeginRenderPass must have a start x value that is a multiple
// of the granularity. The width must also be a multiple of the granularity or eaqual to the width
// the the entire attachment. Similar requirements for the y and height components.
void adjust_bounds_to_granularity(SkIRect* dstBounds, const SkIRect& srcBounds,
                                  const VkExtent2D& granularity, int maxWidth, int maxHeight) {
    // Adjust Width
    if ((0 != granularity.width && 1 != granularity.width)) {
        // Start with the right side of rect so we know if we end up going pass the maxWidth.
        int rightAdj = srcBounds.fRight % granularity.width;
        if (rightAdj != 0) {
            rightAdj = granularity.width - rightAdj;
        }
        dstBounds->fRight = srcBounds.fRight + rightAdj;
        if (dstBounds->fRight > maxWidth) {
            dstBounds->fRight = maxWidth;
            dstBounds->fLeft = 0;
        } else {
            dstBounds->fLeft = srcBounds.fLeft - srcBounds.fLeft % granularity.width;
        }
    } else {
        dstBounds->fLeft = srcBounds.fLeft;
        dstBounds->fRight = srcBounds.fRight;
    }

    // Adjust height
    if ((0 != granularity.height && 1 != granularity.height)) {
        // Start with the bottom side of rect so we know if we end up going pass the maxHeight.
        int bottomAdj = srcBounds.fBottom % granularity.height;
        if (bottomAdj != 0) {
            bottomAdj = granularity.height - bottomAdj;
        }
        dstBounds->fBottom = srcBounds.fBottom + bottomAdj;
        if (dstBounds->fBottom > maxHeight) {
            dstBounds->fBottom = maxHeight;
            dstBounds->fTop = 0;
        } else {
            dstBounds->fTop = srcBounds.fTop - srcBounds.fTop % granularity.height;
        }
    } else {
        dstBounds->fTop = srcBounds.fTop;
        dstBounds->fBottom = srcBounds.fBottom;
    }
}

void GrVkGpu::submitSecondaryCommandBuffer(const SkTArray<GrVkSecondaryCommandBuffer*>& buffers,
                                           const GrVkRenderPass* renderPass,
                                           const VkClearValue* colorClear,
                                           GrVkRenderTarget* target, GrSurfaceOrigin origin,
                                           const SkIRect& bounds) {
    const SkIRect* pBounds = &bounds;
    SkIRect flippedBounds;
    if (kBottomLeft_GrSurfaceOrigin == origin) {
        flippedBounds = bounds;
        flippedBounds.fTop = target->height() - bounds.fBottom;
        flippedBounds.fBottom = target->height() - bounds.fTop;
        pBounds = &flippedBounds;
    }

    // The bounds we use for the render pass should be of the granularity supported
    // by the device.
    const VkExtent2D& granularity = renderPass->granularity();
    SkIRect adjustedBounds;
    if ((0 != granularity.width && 1 != granularity.width) ||
        (0 != granularity.height && 1 != granularity.height)) {
        adjust_bounds_to_granularity(&adjustedBounds, *pBounds, granularity,
                                     target->width(), target->height());
        pBounds = &adjustedBounds;
    }

#ifdef SK_DEBUG
    uint32_t index;
    bool result = renderPass->colorAttachmentIndex(&index);
    SkASSERT(result && 0 == index);
    result = renderPass->stencilAttachmentIndex(&index);
    if (result) {
        SkASSERT(1 == index);
    }
#endif
    VkClearValue clears[2];
    clears[0].color = colorClear->color;
    clears[1].depthStencil.depth = 0.0f;
    clears[1].depthStencil.stencil = 0;

    fCurrentCmdBuffer->beginRenderPass(this, renderPass, clears, *target, *pBounds, true);
    for (int i = 0; i < buffers.count(); ++i) {
        fCurrentCmdBuffer->executeCommands(this, buffers[i]);
    }
    fCurrentCmdBuffer->endRenderPass(this);

    this->didWriteToSurface(target, origin, &bounds);
}

void GrVkGpu::submit(GrGpuCommandBuffer* buffer) {
    if (buffer->asRTCommandBuffer()) {
        SkASSERT(fCachedRTCommandBuffer.get() == buffer);

        fCachedRTCommandBuffer->submit();
        fCachedRTCommandBuffer->reset();
    } else {
        SkASSERT(fCachedTexCommandBuffer.get() == buffer);

        fCachedTexCommandBuffer->submit();
        fCachedTexCommandBuffer->reset();
    }
}

GrFence SK_WARN_UNUSED_RESULT GrVkGpu::insertFence() {
    VkFenceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkFenceCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    VkFence fence = VK_NULL_HANDLE;

    VK_CALL_ERRCHECK(CreateFence(this->device(), &createInfo, nullptr, &fence));
    VK_CALL(QueueSubmit(this->queue(), 0, nullptr, fence));

    GR_STATIC_ASSERT(sizeof(GrFence) >= sizeof(VkFence));
    return (GrFence)fence;
}

bool GrVkGpu::waitFence(GrFence fence, uint64_t timeout) {
    SkASSERT(VK_NULL_HANDLE != (VkFence)fence);

    VkResult result = VK_CALL(WaitForFences(this->device(), 1, (VkFence*)&fence, VK_TRUE, timeout));
    return (VK_SUCCESS == result);
}

void GrVkGpu::deleteFence(GrFence fence) const {
    VK_CALL(DestroyFence(this->device(), (VkFence)fence, nullptr));
}

sk_sp<GrSemaphore> SK_WARN_UNUSED_RESULT GrVkGpu::makeSemaphore(bool isOwned) {
    return GrVkSemaphore::Make(this, isOwned);
}

sk_sp<GrSemaphore> GrVkGpu::wrapBackendSemaphore(const GrBackendSemaphore& semaphore,
                                                 GrResourceProvider::SemaphoreWrapType wrapType,
                                                 GrWrapOwnership ownership) {
    return GrVkSemaphore::MakeWrapped(this, semaphore.vkSemaphore(), wrapType, ownership);
}

void GrVkGpu::insertSemaphore(sk_sp<GrSemaphore> semaphore, bool flush) {
    GrVkSemaphore* vkSem = static_cast<GrVkSemaphore*>(semaphore.get());

    GrVkSemaphore::Resource* resource = vkSem->getResource();
    if (resource->shouldSignal()) {
        resource->ref();
        fSemaphoresToSignal.push_back(resource);
    }

    if (flush) {
        this->submitCommandBuffer(kSkip_SyncQueue);
    }
}

void GrVkGpu::waitSemaphore(sk_sp<GrSemaphore> semaphore) {
    GrVkSemaphore* vkSem = static_cast<GrVkSemaphore*>(semaphore.get());

    GrVkSemaphore::Resource* resource = vkSem->getResource();
    if (resource->shouldWait()) {
        resource->ref();
        fSemaphoresToWaitOn.push_back(resource);
    }
}

sk_sp<GrSemaphore> GrVkGpu::prepareTextureForCrossContextUsage(GrTexture* texture) {
    SkASSERT(texture);
    GrVkTexture* vkTexture = static_cast<GrVkTexture*>(texture);
    vkTexture->setImageLayout(this,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_READ_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              false);
    this->submitCommandBuffer(kSkip_SyncQueue);

    // The image layout change serves as a barrier, so no semaphore is needed
    return nullptr;
}

