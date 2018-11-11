/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "GrGpu.h"

#include "GrBackendSemaphore.h"
#include "GrBackendSurface.h"
#include "GrBuffer.h"
#include "GrCaps.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGpuResourcePriv.h"
#include "GrMesh.h"
#include "GrPathRendering.h"
#include "GrPipeline.h"
#include "GrRenderTargetPriv.h"
#include "GrResourceCache.h"
#include "GrResourceProvider.h"
#include "GrSemaphore.h"
#include "GrStencilAttachment.h"
#include "GrStencilSettings.h"
#include "GrSurfacePriv.h"
#include "GrTexturePriv.h"
#include "GrTextureProxyPriv.h"
#include "GrTracing.h"
#include "SkJSONWriter.h"
#include "SkMathPriv.h"

////////////////////////////////////////////////////////////////////////////////

GrGpu::GrGpu(GrContext* context)
    : fResetTimestamp(kExpiredTimestamp+1)
    , fResetBits(kAll_GrBackendState)
    , fContext(context) {
}

GrGpu::~GrGpu() {}

void GrGpu::disconnect(DisconnectType) {}

////////////////////////////////////////////////////////////////////////////////

bool GrGpu::IsACopyNeededForRepeatWrapMode(const GrCaps* caps, GrTextureProxy* texProxy,
                                           int width, int height,
                                           GrSamplerState::Filter filter,
                                           GrTextureProducer::CopyParams* copyParams,
                                           SkScalar scaleAdjust[2]) {
    if (!caps->npotTextureTileSupport() &&
        (!SkIsPow2(width) || !SkIsPow2(height))) {
        SkASSERT(scaleAdjust);
        copyParams->fWidth = GrNextPow2(width);
        copyParams->fHeight = GrNextPow2(height);
        SkASSERT(scaleAdjust);
        scaleAdjust[0] = ((SkScalar)copyParams->fWidth) / width;
        scaleAdjust[1] = ((SkScalar)copyParams->fHeight) / height;
        switch (filter) {
        case GrSamplerState::Filter::kNearest:
            copyParams->fFilter = GrSamplerState::Filter::kNearest;
            break;
        case GrSamplerState::Filter::kBilerp:
        case GrSamplerState::Filter::kMipMap:
            // We are only ever scaling up so no reason to ever indicate kMipMap.
            copyParams->fFilter = GrSamplerState::Filter::kBilerp;
            break;
        }
        return true;
    }

    if (texProxy) {
        // If the texture format itself doesn't support repeat wrap mode or mipmapping (and
        // those capabilities are required) force a copy.
        if (texProxy->hasRestrictedSampling()) {
            copyParams->fFilter = GrSamplerState::Filter::kNearest;
            copyParams->fWidth = texProxy->width();
            copyParams->fHeight = texProxy->height();
            return true;
        }
    }

    return false;
}

bool GrGpu::IsACopyNeededForMips(const GrCaps* caps, const GrTextureProxy* texProxy,
                                 GrSamplerState::Filter filter,
                                 GrTextureProducer::CopyParams* copyParams) {
    SkASSERT(texProxy);
    bool willNeedMips = GrSamplerState::Filter::kMipMap == filter && caps->mipMapSupport();
    // If the texture format itself doesn't support mipmapping (and those capabilities are required)
    // force a copy.
    if (willNeedMips && texProxy->mipMapped() == GrMipMapped::kNo) {
        copyParams->fFilter = GrSamplerState::Filter::kNearest;
        copyParams->fWidth = texProxy->width();
        copyParams->fHeight = texProxy->height();
        return true;
    }

    return false;
}

sk_sp<GrTexture> GrGpu::createTexture(const GrSurfaceDesc& origDesc, SkBudgeted budgeted,
                                      const GrMipLevel texels[], int mipLevelCount) {
    GR_CREATE_TRACE_MARKER_CONTEXT("GrGpu", "createTexture", fContext);
    GrSurfaceDesc desc = origDesc;

    GrMipMapped mipMapped = mipLevelCount > 1 ? GrMipMapped::kYes : GrMipMapped::kNo;
    if (!this->caps()->validateSurfaceDesc(desc, mipMapped)) {
        return nullptr;
    }

    bool isRT = desc.fFlags & kRenderTarget_GrSurfaceFlag;
    if (isRT) {
        desc.fSampleCnt = this->caps()->getRenderTargetSampleCount(desc.fSampleCnt, desc.fConfig);
    }
    // Attempt to catch un- or wrongly initialized sample counts.
    SkASSERT(desc.fSampleCnt > 0 && desc.fSampleCnt <= 64);

    if (mipLevelCount && (desc.fFlags & kPerformInitialClear_GrSurfaceFlag)) {
        return nullptr;
    }

    this->handleDirtyContext();
    sk_sp<GrTexture> tex = this->onCreateTexture(desc, budgeted, texels, mipLevelCount);
    if (tex) {
        if (!this->caps()->reuseScratchTextures() && !isRT) {
            tex->resourcePriv().removeScratchKey();
        }
        fStats.incTextureCreates();
        if (mipLevelCount) {
            if (texels[0].fPixels) {
                fStats.incTextureUploads();
            }
        }
    }
    return tex;
}

sk_sp<GrTexture> GrGpu::createTexture(const GrSurfaceDesc& desc, SkBudgeted budgeted) {
    return this->createTexture(desc, budgeted, nullptr, 0);
}

sk_sp<GrTexture> GrGpu::wrapBackendTexture(const GrBackendTexture& backendTex,
                                           GrWrapOwnership ownership) {
    this->handleDirtyContext();
    if (!this->caps()->isConfigTexturable(backendTex.config())) {
        return nullptr;
    }
    if (backendTex.width() > this->caps()->maxTextureSize() ||
        backendTex.height() > this->caps()->maxTextureSize()) {
        return nullptr;
    }
    return this->onWrapBackendTexture(backendTex, ownership);
}

sk_sp<GrTexture> GrGpu::wrapRenderableBackendTexture(const GrBackendTexture& backendTex,
                                                     int sampleCnt, GrWrapOwnership ownership) {
    this->handleDirtyContext();
    if (sampleCnt < 1) {
        return nullptr;
    }
    if (!this->caps()->isConfigTexturable(backendTex.config()) ||
        !this->caps()->getRenderTargetSampleCount(sampleCnt, backendTex.config())) {
        return nullptr;
    }

    if (backendTex.width() > this->caps()->maxRenderTargetSize() ||
        backendTex.height() > this->caps()->maxRenderTargetSize()) {
        return nullptr;
    }
    sk_sp<GrTexture> tex = this->onWrapRenderableBackendTexture(backendTex, sampleCnt, ownership);
    SkASSERT(!tex || tex->asRenderTarget());
    return tex;
}

sk_sp<GrRenderTarget> GrGpu::wrapBackendRenderTarget(const GrBackendRenderTarget& backendRT) {
    if (0 == this->caps()->getRenderTargetSampleCount(backendRT.sampleCnt(), backendRT.config())) {
        return nullptr;
    }
    this->handleDirtyContext();
    return this->onWrapBackendRenderTarget(backendRT);
}

sk_sp<GrRenderTarget> GrGpu::wrapBackendTextureAsRenderTarget(const GrBackendTexture& tex,
                                                              int sampleCnt) {
    if (0 == this->caps()->getRenderTargetSampleCount(sampleCnt, tex.config())) {
        return nullptr;
    }
    int maxSize = this->caps()->maxTextureSize();
    if (tex.width() > maxSize || tex.height() > maxSize) {
        return nullptr;
    }
    this->handleDirtyContext();
    return this->onWrapBackendTextureAsRenderTarget(tex, sampleCnt);
}

GrBuffer* GrGpu::createBuffer(size_t size, GrBufferType intendedType,
                              GrAccessPattern accessPattern, const void* data) {
    this->handleDirtyContext();
    GrBuffer* buffer = this->onCreateBuffer(size, intendedType, accessPattern, data);
    if (!this->caps()->reuseScratchBuffers()) {
        buffer->resourcePriv().removeScratchKey();
    }
    return buffer;
}

bool GrGpu::copySurface(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                        GrSurface* src, GrSurfaceOrigin srcOrigin,
                        const SkIRect& srcRect, const SkIPoint& dstPoint,
                        bool canDiscardOutsideDstRect) {
    GR_CREATE_TRACE_MARKER_CONTEXT("GrGpu", "copySurface", fContext);
    SkASSERT(dst && src);
    this->handleDirtyContext();
    return this->onCopySurface(dst, dstOrigin, src, srcOrigin, srcRect, dstPoint,
                               canDiscardOutsideDstRect);
}

bool GrGpu::readPixels(GrSurface* surface, int left, int top, int width, int height,
                       GrColorType dstColorType, void* buffer, size_t rowBytes) {
    SkASSERT(surface);

    int bpp = GrColorTypeBytesPerPixel(dstColorType);
    if (!GrSurfacePriv::AdjustReadPixelParams(surface->width(), surface->height(), bpp,
                                              &left, &top, &width, &height,
                                              &buffer,
                                              &rowBytes)) {
        return false;
    }

    this->handleDirtyContext();

    return this->onReadPixels(surface, left, top, width, height, dstColorType, buffer, rowBytes);
}

bool GrGpu::writePixels(GrSurface* surface, int left, int top, int width, int height,
                        GrColorType srcColorType, const GrMipLevel texels[], int mipLevelCount) {
    SkASSERT(surface);
    if (1 == mipLevelCount) {
        // We require that if we are not mipped, then the write region is contained in the surface
        SkIRect subRect = SkIRect::MakeXYWH(left, top, width, height);
        SkIRect bounds = SkIRect::MakeWH(surface->width(), surface->height());
        if (!bounds.contains(subRect)) {
            return false;
        }
    } else if (0 != left || 0 != top || width != surface->width() || height != surface->height()) {
        // We require that if the texels are mipped, than the write region is the entire surface
        return false;
    }

    for (int currentMipLevel = 0; currentMipLevel < mipLevelCount; currentMipLevel++) {
        if (!texels[currentMipLevel].fPixels ) {
            return false;
        }
    }

    this->handleDirtyContext();
    if (this->onWritePixels(surface, left, top, width, height, srcColorType, texels,
                            mipLevelCount)) {
        SkIRect rect = SkIRect::MakeXYWH(left, top, width, height);
        this->didWriteToSurface(surface, kTopLeft_GrSurfaceOrigin, &rect, mipLevelCount);
        fStats.incTextureUploads();
        return true;
    }
    return false;
}

bool GrGpu::transferPixels(GrTexture* texture, int left, int top, int width, int height,
                           GrColorType bufferColorType, GrBuffer* transferBuffer, size_t offset,
                           size_t rowBytes) {
    SkASSERT(transferBuffer);

    // We require that the write region is contained in the texture
    SkIRect subRect = SkIRect::MakeXYWH(left, top, width, height);
    SkIRect bounds = SkIRect::MakeWH(texture->width(), texture->height());
    if (!bounds.contains(subRect)) {
        return false;
    }

    this->handleDirtyContext();
    if (this->onTransferPixels(texture, left, top, width, height, bufferColorType, transferBuffer,
                               offset, rowBytes)) {
        SkIRect rect = SkIRect::MakeXYWH(left, top, width, height);
        this->didWriteToSurface(texture, kTopLeft_GrSurfaceOrigin, &rect);
        fStats.incTransfersToTexture();

        return true;
    }
    return false;
}

bool GrGpu::regenerateMipMapLevels(GrTexture* texture) {
    SkASSERT(texture);
    SkASSERT(this->caps()->mipMapSupport());
    SkASSERT(texture->texturePriv().mipMapped() == GrMipMapped::kYes);
    SkASSERT(texture->texturePriv().mipMapsAreDirty());
    SkASSERT(!texture->asRenderTarget() || !texture->asRenderTarget()->needsResolve());
    if (this->onRegenerateMipMapLevels(texture)) {
        texture->texturePriv().markMipMapsClean();
        return true;
    }
    return false;
}

void GrGpu::resolveRenderTarget(GrRenderTarget* target) {
    SkASSERT(target);
    this->handleDirtyContext();
    this->onResolveRenderTarget(target);
}

void GrGpu::didWriteToSurface(GrSurface* surface, GrSurfaceOrigin origin, const SkIRect* bounds,
                              uint32_t mipLevels) const {
    SkASSERT(surface);
    // Mark any MIP chain and resolve buffer as dirty if and only if there is a non-empty bounds.
    if (nullptr == bounds || !bounds->isEmpty()) {
        if (GrRenderTarget* target = surface->asRenderTarget()) {
            SkIRect flippedBounds;
            if (kBottomLeft_GrSurfaceOrigin == origin && bounds) {
                flippedBounds = {bounds->fLeft, surface->height() - bounds->fBottom,
                                 bounds->fRight, surface->height() - bounds->fTop};
                bounds = &flippedBounds;
            }
            target->flagAsNeedingResolve(bounds);
        }
        GrTexture* texture = surface->asTexture();
        if (texture && 1 == mipLevels) {
            texture->texturePriv().markMipMapsDirty();
        }
    }
}

GrSemaphoresSubmitted GrGpu::finishFlush(int numSemaphores,
                                         GrBackendSemaphore backendSemaphores[]) {
    GrResourceProvider* resourceProvider = fContext->contextPriv().resourceProvider();

    if (this->caps()->fenceSyncSupport()) {
        for (int i = 0; i < numSemaphores; ++i) {
            sk_sp<GrSemaphore> semaphore;
            if (backendSemaphores[i].isInitialized()) {
                semaphore = resourceProvider->wrapBackendSemaphore(
                        backendSemaphores[i], GrResourceProvider::SemaphoreWrapType::kWillSignal,
                        kBorrow_GrWrapOwnership);
            } else {
                semaphore = resourceProvider->makeSemaphore(false);
            }
            this->insertSemaphore(semaphore, false);

            if (!backendSemaphores[i].isInitialized()) {
                backendSemaphores[i] = semaphore->backendSemaphore();
            }
        }
    }
    this->onFinishFlush((numSemaphores > 0 && this->caps()->fenceSyncSupport()));
    return this->caps()->fenceSyncSupport() ? GrSemaphoresSubmitted::kYes
                                            : GrSemaphoresSubmitted::kNo;
}

#ifdef SK_ENABLE_DUMP_GPU
void GrGpu::dumpJSON(SkJSONWriter* writer) const {
    writer->beginObject();

    // TODO: Is there anything useful in the base class to dump here?

    this->onDumpJSON(writer);

    writer->endObject();
}
#else
void GrGpu::dumpJSON(SkJSONWriter* writer) const { }
#endif

#if GR_TEST_UTILS
GrBackendTexture GrGpu::createTestingOnlyBackendTexture(const void* pixels, int w, int h,
                                                        SkColorType colorType, bool isRenderTarget,
                                                        GrMipMapped isMipped, size_t rowBytes) {
    GrColorType grCT = SkColorTypeToGrColorType(colorType);

    return this->createTestingOnlyBackendTexture(pixels, w, h, grCT, isRenderTarget, isMipped,
                                                 rowBytes);
}
#endif
