/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrResourceProvider.h"

#include "GrBackendSemaphore.h"
#include "GrBuffer.h"
#include "GrCaps.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGpu.h"
#include "GrPath.h"
#include "GrPathRendering.h"
#include "GrProxyProvider.h"
#include "GrRenderTargetPriv.h"
#include "GrResourceCache.h"
#include "GrResourceKey.h"
#include "GrSemaphore.h"
#include "GrStencilAttachment.h"
#include "GrTexturePriv.h"
#include "../private/GrSingleOwner.h"
#include "SkGr.h"
#include "SkMathPriv.h"

GR_DECLARE_STATIC_UNIQUE_KEY(gQuadIndexBufferKey);

const uint32_t GrResourceProvider::kMinScratchTextureSize = 16;

#ifdef SK_DISABLE_EXPLICIT_GPU_RESOURCE_ALLOCATION
static const bool kDefaultExplicitlyAllocateGPUResources = false;
#else
static const bool kDefaultExplicitlyAllocateGPUResources = true;
#endif

#define ASSERT_SINGLE_OWNER \
    SkDEBUGCODE(GrSingleOwner::AutoEnforce debug_SingleOwner(fSingleOwner);)

GrResourceProvider::GrResourceProvider(GrGpu* gpu, GrResourceCache* cache, GrSingleOwner* owner,
                                       GrContextOptions::Enable explicitlyAllocateGPUResources)
        : fCache(cache)
        , fGpu(gpu)
#ifdef SK_DEBUG
        , fSingleOwner(owner)
#endif
{
    if (GrContextOptions::Enable::kNo == explicitlyAllocateGPUResources) {
        fExplicitlyAllocateGPUResources = false;
    } else if (GrContextOptions::Enable::kYes == explicitlyAllocateGPUResources) {
        fExplicitlyAllocateGPUResources = true;
    } else {
        fExplicitlyAllocateGPUResources = kDefaultExplicitlyAllocateGPUResources;
    }

    fCaps = sk_ref_sp(fGpu->caps());

    GR_DEFINE_STATIC_UNIQUE_KEY(gQuadIndexBufferKey);
    fQuadIndexBufferKey = gQuadIndexBufferKey;
}

sk_sp<GrTexture> GrResourceProvider::createTexture(const GrSurfaceDesc& desc, SkBudgeted budgeted,
                                                   const GrMipLevel texels[], int mipLevelCount) {
    ASSERT_SINGLE_OWNER

    SkASSERT(mipLevelCount > 0);

    if (this->isAbandoned()) {
        return nullptr;
    }

    GrMipMapped mipMapped = mipLevelCount > 1 ? GrMipMapped::kYes : GrMipMapped::kNo;
    if (!fCaps->validateSurfaceDesc(desc, mipMapped)) {
        return nullptr;
    }

    return fGpu->createTexture(desc, budgeted, texels, mipLevelCount);
}

sk_sp<GrTexture> GrResourceProvider::getExactScratch(const GrSurfaceDesc& desc,
                                                     SkBudgeted budgeted, Flags flags) {
    sk_sp<GrTexture> tex(this->refScratchTexture(desc, flags));
    if (tex && SkBudgeted::kNo == budgeted) {
        tex->resourcePriv().makeUnbudgeted();
    }

    return tex;
}

sk_sp<GrTexture> GrResourceProvider::createTexture(const GrSurfaceDesc& desc,
                                                   SkBudgeted budgeted,
                                                   SkBackingFit fit,
                                                   const GrMipLevel& mipLevel,
                                                   Flags flags) {
    ASSERT_SINGLE_OWNER

    if (this->isAbandoned()) {
        return nullptr;
    }

    if (!mipLevel.fPixels) {
        return nullptr;
    }

    if (!fCaps->validateSurfaceDesc(desc, GrMipMapped::kNo)) {
        return nullptr;
    }

    GrContext* context = fGpu->getContext();
    GrProxyProvider* proxyProvider = context->contextPriv().proxyProvider();

    SkColorType colorType;
    if (GrPixelConfigToColorType(desc.fConfig, &colorType)) {
        sk_sp<GrTexture> tex = (SkBackingFit::kApprox == fit)
                ? this->createApproxTexture(desc, flags)
                : this->createTexture(desc, budgeted, flags);
        if (!tex) {
            return nullptr;
        }

        sk_sp<GrTextureProxy> proxy = proxyProvider->createWrapped(std::move(tex),
                                                                   kTopLeft_GrSurfaceOrigin);
        if (!proxy) {
            return nullptr;
        }
        auto srcInfo = SkImageInfo::Make(desc.fWidth, desc.fHeight, colorType,
                                         kUnknown_SkAlphaType);
        sk_sp<GrSurfaceContext> sContext = context->contextPriv().makeWrappedSurfaceContext(
                std::move(proxy));
        if (!sContext) {
            return nullptr;
        }
        SkAssertResult(sContext->writePixels(srcInfo, mipLevel.fPixels, mipLevel.fRowBytes, 0, 0));
        return sk_ref_sp(sContext->asTextureProxy()->peekTexture());
    } else {
        return fGpu->createTexture(desc, budgeted, &mipLevel, 1);
    }
}

sk_sp<GrTexture> GrResourceProvider::createTexture(const GrSurfaceDesc& desc, SkBudgeted budgeted,
                                                   Flags flags) {
    ASSERT_SINGLE_OWNER
    if (this->isAbandoned()) {
        return nullptr;
    }

    if (!fCaps->validateSurfaceDesc(desc, GrMipMapped::kNo)) {
        return nullptr;
    }

    sk_sp<GrTexture> tex = this->getExactScratch(desc, budgeted, flags);
    if (tex) {
        return tex;
    }

    return fGpu->createTexture(desc, budgeted);
}

sk_sp<GrTexture> GrResourceProvider::createApproxTexture(const GrSurfaceDesc& desc,
                                                         Flags flags) {
    ASSERT_SINGLE_OWNER
    SkASSERT(Flags::kNone == flags || Flags::kNoPendingIO == flags);

    if (this->isAbandoned()) {
        return nullptr;
    }

    if (!fCaps->validateSurfaceDesc(desc, GrMipMapped::kNo)) {
        return nullptr;
    }

    if (auto tex = this->refScratchTexture(desc, flags)) {
        return tex;
    }

    SkTCopyOnFirstWrite<GrSurfaceDesc> copyDesc(desc);

    // bin by pow2 with a reasonable min
    if (!SkToBool(desc.fFlags & kPerformInitialClear_GrSurfaceFlag) &&
        (fGpu->caps()->reuseScratchTextures() || (desc.fFlags & kRenderTarget_GrSurfaceFlag))) {
        GrSurfaceDesc* wdesc = copyDesc.writable();
        wdesc->fWidth  = SkTMax(kMinScratchTextureSize, GrNextPow2(desc.fWidth));
        wdesc->fHeight = SkTMax(kMinScratchTextureSize, GrNextPow2(desc.fHeight));
    }

    if (auto tex = this->refScratchTexture(*copyDesc, flags)) {
        return tex;
    }

    return fGpu->createTexture(*copyDesc, SkBudgeted::kYes);
}

sk_sp<GrTexture> GrResourceProvider::refScratchTexture(const GrSurfaceDesc& desc, Flags flags) {
    ASSERT_SINGLE_OWNER
    SkASSERT(!this->isAbandoned());
    SkASSERT(fCaps->validateSurfaceDesc(desc, GrMipMapped::kNo));

    // We could make initial clears work with scratch textures but it is a rare case so we just opt
    // to fall back to making a new texture.
    if (!SkToBool(desc.fFlags & kPerformInitialClear_GrSurfaceFlag) &&
        (fGpu->caps()->reuseScratchTextures() || (desc.fFlags & kRenderTarget_GrSurfaceFlag))) {

        GrScratchKey key;
        GrTexturePriv::ComputeScratchKey(desc, &key);
        auto scratchFlags = GrResourceCache::ScratchFlags::kNone;
        if (Flags::kNoPendingIO & flags) {
            scratchFlags |= GrResourceCache::ScratchFlags::kRequireNoPendingIO;
        } else  if (!(desc.fFlags & kRenderTarget_GrSurfaceFlag)) {
            // If it is not a render target then it will most likely be populated by
            // writePixels() which will trigger a flush if the texture has pending IO.
            scratchFlags |= GrResourceCache::ScratchFlags::kPreferNoPendingIO;
        }
        GrGpuResource* resource = fCache->findAndRefScratchResource(key,
                                                                    GrSurface::WorstCaseSize(desc),
                                                                    scratchFlags);
        if (resource) {
            GrSurface* surface = static_cast<GrSurface*>(resource);
            return sk_sp<GrTexture>(surface->asTexture());
        }
    }

    return nullptr;
}

sk_sp<GrTexture> GrResourceProvider::wrapBackendTexture(const GrBackendTexture& tex,
                                                        GrWrapOwnership ownership) {
    ASSERT_SINGLE_OWNER
    if (this->isAbandoned()) {
        return nullptr;
    }
    return fGpu->wrapBackendTexture(tex, ownership);
}

sk_sp<GrTexture> GrResourceProvider::wrapRenderableBackendTexture(const GrBackendTexture& tex,
                                                                  int sampleCnt,
                                                                  GrWrapOwnership ownership) {
    ASSERT_SINGLE_OWNER
    if (this->isAbandoned()) {
        return nullptr;
    }
    return fGpu->wrapRenderableBackendTexture(tex, sampleCnt, ownership);
}

sk_sp<GrRenderTarget> GrResourceProvider::wrapBackendRenderTarget(
        const GrBackendRenderTarget& backendRT)
{
    ASSERT_SINGLE_OWNER
    return this->isAbandoned() ? nullptr : fGpu->wrapBackendRenderTarget(backendRT);
}

void GrResourceProvider::assignUniqueKeyToResource(const GrUniqueKey& key,
                                                   GrGpuResource* resource) {
    ASSERT_SINGLE_OWNER
    if (this->isAbandoned() || !resource) {
        return;
    }
    resource->resourcePriv().setUniqueKey(key);
}

sk_sp<GrGpuResource> GrResourceProvider::findResourceByUniqueKey(const GrUniqueKey& key) {
    ASSERT_SINGLE_OWNER
    return this->isAbandoned() ? nullptr
                               : sk_sp<GrGpuResource>(fCache->findAndRefUniqueResource(key));
}

sk_sp<const GrBuffer> GrResourceProvider::findOrMakeStaticBuffer(GrBufferType intendedType,
                                                                 size_t size,
                                                                 const void* data,
                                                                 const GrUniqueKey& key) {
    if (auto buffer = this->findByUniqueKey<GrBuffer>(key)) {
        return std::move(buffer);
    }
    if (auto buffer = this->createBuffer(size, intendedType, kStatic_GrAccessPattern, Flags::kNone,
                                         data)) {
        // We shouldn't bin and/or cachestatic buffers.
        SkASSERT(buffer->sizeInBytes() == size);
        SkASSERT(!buffer->resourcePriv().getScratchKey().isValid());
        SkASSERT(!buffer->resourcePriv().hasPendingIO_debugOnly());
        buffer->resourcePriv().setUniqueKey(key);
        return sk_sp<const GrBuffer>(buffer);
    }
    return nullptr;
}

sk_sp<const GrBuffer> GrResourceProvider::createPatternedIndexBuffer(const uint16_t* pattern,
                                                                     int patternSize,
                                                                     int reps,
                                                                     int vertCount,
                                                                     const GrUniqueKey& key) {
    size_t bufferSize = patternSize * reps * sizeof(uint16_t);

    // This is typically used in GrMeshDrawOps, so we assume kNoPendingIO.
    sk_sp<GrBuffer> buffer(this->createBuffer(bufferSize, kIndex_GrBufferType,
                                              kStatic_GrAccessPattern, Flags::kNoPendingIO));
    if (!buffer) {
        return nullptr;
    }
    uint16_t* data = (uint16_t*) buffer->map();
    SkAutoTArray<uint16_t> temp;
    if (!data) {
        temp.reset(reps * patternSize);
        data = temp.get();
    }
    for (int i = 0; i < reps; ++i) {
        int baseIdx = i * patternSize;
        uint16_t baseVert = (uint16_t)(i * vertCount);
        for (int j = 0; j < patternSize; ++j) {
            data[baseIdx+j] = baseVert + pattern[j];
        }
    }
    if (temp.get()) {
        if (!buffer->updateData(data, bufferSize)) {
            return nullptr;
        }
    } else {
        buffer->unmap();
    }
    this->assignUniqueKeyToResource(key, buffer.get());
    return std::move(buffer);
}

static constexpr int kMaxQuads = 1 << 12;  // max possible: (1 << 14) - 1;

sk_sp<const GrBuffer> GrResourceProvider::createQuadIndexBuffer() {
    GR_STATIC_ASSERT(4 * kMaxQuads <= 65535);
    static const uint16_t kPattern[] = { 0, 1, 2, 2, 1, 3 };
    return this->createPatternedIndexBuffer(kPattern, 6, kMaxQuads, 4, fQuadIndexBufferKey);
}

int GrResourceProvider::QuadCountOfQuadBuffer() { return kMaxQuads; }

sk_sp<GrPath> GrResourceProvider::createPath(const SkPath& path, const GrStyle& style) {
    if (this->isAbandoned()) {
        return nullptr;
    }

    SkASSERT(this->gpu()->pathRendering());
    return this->gpu()->pathRendering()->createPath(path, style);
}

GrBuffer* GrResourceProvider::createBuffer(size_t size, GrBufferType intendedType,
                                           GrAccessPattern accessPattern, Flags flags,
                                           const void* data) {
    if (this->isAbandoned()) {
        return nullptr;
    }
    if (kDynamic_GrAccessPattern != accessPattern) {
        return this->gpu()->createBuffer(size, intendedType, accessPattern, data);
    }
    if (!(flags & Flags::kRequireGpuMemory) &&
        this->gpu()->caps()->preferClientSideDynamicBuffers() &&
        GrBufferTypeIsVertexOrIndex(intendedType) &&
        kDynamic_GrAccessPattern == accessPattern) {
        return GrBuffer::CreateCPUBacked(this->gpu(), size, intendedType, data);
    }

    // bin by pow2 with a reasonable min
    static const size_t MIN_SIZE = 1 << 12;
    size_t allocSize = SkTMax(MIN_SIZE, GrNextSizePow2(size));

    GrScratchKey key;
    GrBuffer::ComputeScratchKeyForDynamicVBO(allocSize, intendedType, &key);
    auto scratchFlags = GrResourceCache::ScratchFlags::kNone;
    if (flags & Flags::kNoPendingIO) {
        scratchFlags = GrResourceCache::ScratchFlags::kRequireNoPendingIO;
    } else {
        scratchFlags = GrResourceCache::ScratchFlags::kPreferNoPendingIO;
    }
    GrBuffer* buffer = static_cast<GrBuffer*>(
        this->cache()->findAndRefScratchResource(key, allocSize, scratchFlags));
    if (!buffer) {
        buffer = this->gpu()->createBuffer(allocSize, intendedType, kDynamic_GrAccessPattern);
        if (!buffer) {
            return nullptr;
        }
    }
    if (data) {
        buffer->updateData(data, size);
    }
    SkASSERT(!buffer->isCPUBacked()); // We should only cache real VBOs.
    return buffer;
}

bool GrResourceProvider::attachStencilAttachment(GrRenderTarget* rt) {
    SkASSERT(rt);
    if (rt->renderTargetPriv().getStencilAttachment()) {
        return true;
    }

    if (!rt->wasDestroyed() && rt->canAttemptStencilAttachment()) {
        GrUniqueKey sbKey;

        int width = rt->width();
        int height = rt->height();
#if 0
        if (this->caps()->oversizedStencilSupport()) {
            width  = SkNextPow2(width);
            height = SkNextPow2(height);
        }
#endif
        GrStencilAttachment::ComputeSharedStencilAttachmentKey(width, height,
                                                               rt->numStencilSamples(), &sbKey);
        auto stencil = this->findByUniqueKey<GrStencilAttachment>(sbKey);
        if (!stencil) {
            // Need to try and create a new stencil
            stencil.reset(this->gpu()->createStencilAttachmentForRenderTarget(rt, width, height));
            if (!stencil) {
                return false;
            }
            this->assignUniqueKeyToResource(sbKey, stencil.get());
        }
        rt->renderTargetPriv().attachStencilAttachment(std::move(stencil));
    }
    return SkToBool(rt->renderTargetPriv().getStencilAttachment());
}

sk_sp<GrRenderTarget> GrResourceProvider::wrapBackendTextureAsRenderTarget(
        const GrBackendTexture& tex, int sampleCnt)
{
    if (this->isAbandoned()) {
        return nullptr;
    }
    return fGpu->wrapBackendTextureAsRenderTarget(tex, sampleCnt);
}

sk_sp<GrSemaphore> SK_WARN_UNUSED_RESULT GrResourceProvider::makeSemaphore(bool isOwned) {
    return fGpu->makeSemaphore(isOwned);
}

sk_sp<GrSemaphore> GrResourceProvider::wrapBackendSemaphore(const GrBackendSemaphore& semaphore,
                                                            SemaphoreWrapType wrapType,
                                                            GrWrapOwnership ownership) {
    ASSERT_SINGLE_OWNER
    return this->isAbandoned() ? nullptr : fGpu->wrapBackendSemaphore(semaphore,
                                                                      wrapType,
                                                                      ownership);
}
