/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrAHardwareBufferImageGenerator.h"
#include "GrBackendSurface.h"
#include "GrBackendTextureImageGenerator.h"
#include "GrBitmapTextureMaker.h"
#include "GrCaps.h"
#include "GrClip.h"
#include "GrColorSpaceXform.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGpu.h"
#include "GrImageTextureMaker.h"
#include "GrProxyProvider.h"
#include "GrRenderTargetContext.h"
#include "GrResourceProvider.h"
#include "GrResourceProviderPriv.h"
#include "GrSemaphore.h"
#include "GrSurfacePriv.h"
#include "GrTexture.h"
#include "GrTextureAdjuster.h"
#include "GrTexturePriv.h"
#include "GrTextureProxy.h"
#include "GrTextureProxyPriv.h"
#include "SkAutoPixmapStorage.h"
#include "SkBitmapCache.h"
#include "SkCanvas.h"
#include "SkGr.h"
#include "SkImageInfoPriv.h"
#include "SkImage_Gpu.h"
#include "SkMipMap.h"
#include "SkPixelRef.h"
#include "SkTraceEvent.h"
#include "SkYUVAIndex.h"
#include "effects/GrYUVtoRGBEffect.h"
#include "gl/GrGLTexture.h"

SkImage_Gpu::SkImage_Gpu(sk_sp<GrContext> context, uint32_t uniqueID, SkAlphaType at,
                         sk_sp<GrTextureProxy> proxy, sk_sp<SkColorSpace> colorSpace,
                         SkBudgeted budgeted)
        : INHERITED(std::move(context), proxy->worstCaseWidth(), proxy->worstCaseHeight(), uniqueID,
                    at, budgeted, colorSpace)
        , fProxy(std::move(proxy)) {}

SkImage_Gpu::~SkImage_Gpu() {}

SkImageInfo SkImage_Gpu::onImageInfo() const {
    SkColorType colorType;
    if (!GrPixelConfigToColorType(fProxy->config(), &colorType)) {
        colorType = kUnknown_SkColorType;
    }

    return SkImageInfo::Make(fProxy->width(), fProxy->height(), colorType, fAlphaType, fColorSpace);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static sk_sp<SkImage> new_wrapped_texture_common(GrContext* ctx,
                                                 const GrBackendTexture& backendTex,
                                                 GrSurfaceOrigin origin,
                                                 SkAlphaType at, sk_sp<SkColorSpace> colorSpace,
                                                 GrWrapOwnership ownership,
                                                 SkImage::TextureReleaseProc releaseProc,
                                                 SkImage::ReleaseContext releaseCtx) {
    if (!backendTex.isValid() || backendTex.width() <= 0 || backendTex.height() <= 0) {
        return nullptr;
    }

    GrProxyProvider* proxyProvider = ctx->contextPriv().proxyProvider();
    sk_sp<GrTextureProxy> proxy = proxyProvider->wrapBackendTexture(backendTex, origin, ownership,
                                                                    releaseProc, releaseCtx);
    if (!proxy) {
        return nullptr;
    }
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(ctx), kNeedNewImageUniqueID, at, std::move(proxy),
                                   std::move(colorSpace), SkBudgeted::kNo);
}

sk_sp<SkImage> SkImage::MakeFromTexture(GrContext* ctx,
                                        const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                        SkColorType ct, SkAlphaType at, sk_sp<SkColorSpace> cs,
                                        TextureReleaseProc releaseP, ReleaseContext releaseC) {
    if (!ctx) {
        return nullptr;
    }
    GrBackendTexture texCopy = tex;
    if (!SkImage_GpuBase::ValidateBackendTexture(ctx, texCopy, &texCopy.fConfig, ct, at, cs)) {
        return nullptr;
    }
    return new_wrapped_texture_common(ctx, texCopy, origin, at, std::move(cs),
                                      kBorrow_GrWrapOwnership, releaseP, releaseC);
}

sk_sp<SkImage> SkImage::MakeFromAdoptedTexture(GrContext* ctx,
                                               const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                               SkColorType ct, SkAlphaType at,
                                               sk_sp<SkColorSpace> cs) {
    if (!ctx || !ctx->contextPriv().resourceProvider()) {
        // We have a DDL context and we don't support adopted textures for them.
        return nullptr;
    }
    GrBackendTexture texCopy = tex;
    if (!SkImage_GpuBase::ValidateBackendTexture(ctx, texCopy, &texCopy.fConfig, ct, at, cs)) {
        return nullptr;
    }
    return new_wrapped_texture_common(ctx, texCopy, origin, at, std::move(cs),
                                      kAdopt_GrWrapOwnership, nullptr, nullptr);
}

sk_sp<SkImage> SkImage_Gpu::ConvertYUVATexturesToRGB(
        GrContext* ctx, SkYUVColorSpace yuvColorSpace, const GrBackendTexture yuvaTextures[],
        const SkYUVAIndex yuvaIndices[4], SkISize size, GrSurfaceOrigin origin,
        SkBudgeted isBudgeted, GrRenderTargetContext* renderTargetContext) {
    SkASSERT(renderTargetContext);

    GrProxyProvider* proxyProvider = ctx->contextPriv().proxyProvider();

    // We need to make a copy of the input backend textures because we need to preserve the result
    // of validate_backend_texture.
    GrBackendTexture yuvaTexturesCopy[4];

    bool nv12 = (yuvaIndices[1].fIndex == yuvaIndices[2].fIndex);

    for (int i = 0; i < 4; ++i) {
        // Validate that the yuvaIndices refer to valid backend textures.
        const SkYUVAIndex& yuvaIndex = yuvaIndices[i];
        if (i == 3 && yuvaIndex.fIndex == -1) {
            // Meaning the A plane isn't passed in.
            continue;
        }
        if (yuvaIndex.fIndex == -1 || yuvaIndex.fIndex > 3) {
            // Y plane, U plane, and V plane must refer to image sources being passed in. There are
            // at most 4 images sources being passed in, could not have a index more than 3.
            return nullptr;
        }

        SkColorType ct = kUnknown_SkColorType;
        if (SkYUVAIndex::kA_Index == i) {
            // The A plane is always kAlpha8 (for now)
            ct = kAlpha_8_SkColorType;
        } else {
            // The UV planes can either be interleaved or planar. If interleaved the Y plane
            // will have RBGA color type.
            ct = nv12 ? kRGBA_8888_SkColorType : kAlpha_8_SkColorType;
        }

        if (!yuvaTexturesCopy[yuvaIndex.fIndex].isValid()) {
            yuvaTexturesCopy[yuvaIndex.fIndex] = yuvaTextures[yuvaIndex.fIndex];

            // TODO: Instead of using assumption about whether it is NV12 format to guess colorType,
            // actually use channel information here.
            if (!ValidateBackendTexture(ctx, yuvaTexturesCopy[yuvaIndex.fIndex],
                                        &yuvaTexturesCopy[yuvaIndex.fIndex].fConfig,
                                        ct, kPremul_SkAlphaType, nullptr)) {
                return nullptr;
            }
        }

        // TODO: Check that for each plane, the channel actually exist in the image source we are
        // reading from.
    }

    sk_sp<GrTextureProxy> tempTextureProxies[4];
    for (int i = 0; i < 4; ++i) {
        // Fill in tempTextureProxies to avoid duplicate texture proxies.
        int textureIndex = yuvaIndices[i].fIndex;

        // Safely ignore since this means we are missing the A plane.
        if (textureIndex == -1) {
            SkASSERT(SkYUVAIndex::kA_Index == i);
            continue;
        }

        if (!tempTextureProxies[textureIndex]) {
            SkASSERT(yuvaTexturesCopy[textureIndex].isValid());
            tempTextureProxies[textureIndex] =
                    proxyProvider->wrapBackendTexture(yuvaTexturesCopy[textureIndex], origin);
            if (!tempTextureProxies[textureIndex]) {
                return nullptr;
            }
        }
    }

    const int width = size.width();
    const int height = size.height();

    GrPaint paint;
    paint.setPorterDuffXPFactory(SkBlendMode::kSrc);
    // TODO: Modify the fragment processor to sample from different channel instead of taking nv12
    // bool.
    paint.addColorFragmentProcessor(GrYUVtoRGBEffect::Make(tempTextureProxies, yuvaIndices,
                                                           yuvColorSpace));

    const SkRect rect = SkRect::MakeIWH(width, height);

    renderTargetContext->drawRect(GrNoClip(), std::move(paint), GrAA::kNo, SkMatrix::I(), rect);

    if (!renderTargetContext->asSurfaceProxy()) {
        return nullptr;
    }

    // DDL TODO: in the promise image version we must not flush here
    ctx->contextPriv().flushSurfaceWrites(renderTargetContext->asSurfaceProxy());

    // MDB: this call is okay bc we know 'renderTargetContext' was exact
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(ctx), kNeedNewImageUniqueID, kOpaque_SkAlphaType,
                                   renderTargetContext->asTextureProxyRef(),
                                   renderTargetContext->colorSpaceInfo().refColorSpace(),
                                   isBudgeted);
}

sk_sp<SkImage> SkImage::MakeFromYUVATexturesCopy(GrContext* ctx,
                                                 SkYUVColorSpace yuvColorSpace,
                                                 const GrBackendTexture yuvaTextures[],
                                                 const SkYUVAIndex yuvaIndices[4],
                                                 SkISize imageSize,
                                                 GrSurfaceOrigin imageOrigin,
                                                 sk_sp<SkColorSpace> imageColorSpace) {
    const int width = imageSize.width();
    const int height = imageSize.height();

    // Needs to create a render target in order to draw to it for the yuv->rgb conversion.
    sk_sp<GrRenderTargetContext> renderTargetContext(
            ctx->contextPriv().makeDeferredRenderTargetContext(
                    SkBackingFit::kExact, width, height, kRGBA_8888_GrPixelConfig,
                    std::move(imageColorSpace), 1, GrMipMapped::kNo, imageOrigin));
    if (!renderTargetContext) {
        return nullptr;
    }

    return SkImage_Gpu::ConvertYUVATexturesToRGB(ctx, yuvColorSpace, yuvaTextures, yuvaIndices,
                                                 imageSize, imageOrigin, SkBudgeted::kYes,
                                                 renderTargetContext.get());
}

sk_sp<SkImage> SkImage::MakeFromYUVATexturesCopyWithExternalBackend(
        GrContext* ctx,
        SkYUVColorSpace yuvColorSpace,
        const GrBackendTexture yuvaTextures[],
        const SkYUVAIndex yuvaIndices[4],
        SkISize imageSize,
        GrSurfaceOrigin imageOrigin,
        const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> imageColorSpace) {
    GrBackendTexture backendTextureCopy = backendTexture;

    if (!SkImage_Gpu::ValidateBackendTexture(ctx, backendTextureCopy, &backendTextureCopy.fConfig,
                                             kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr)) {
        return nullptr;
    }

    // Needs to create a render target with external texture
    // in order to draw to it for the yuv->rgb conversion.
    sk_sp<GrRenderTargetContext> renderTargetContext(
            ctx->contextPriv().makeBackendTextureRenderTargetContext(backendTextureCopy,
                                                                     imageOrigin, 1,
                                                                     std::move(imageColorSpace)));

    if (!renderTargetContext) {
        return nullptr;
    }

    return SkImage_Gpu::ConvertYUVATexturesToRGB(ctx, yuvColorSpace, yuvaTextures, yuvaIndices,
                                                 imageSize, imageOrigin, SkBudgeted::kNo,
                                                 renderTargetContext.get());
}

sk_sp<SkImage> SkImage::MakeFromYUVTexturesCopy(GrContext* ctx, SkYUVColorSpace yuvColorSpace,
                                                const GrBackendTexture yuvTextures[3],
                                                GrSurfaceOrigin imageOrigin,
                                                sk_sp<SkColorSpace> imageColorSpace) {
    // TODO: SkImageSourceChannel input is being ingored right now. Setup correctly in the future.
    SkYUVAIndex yuvaIndices[4] = {
            SkYUVAIndex{0, SkColorChannel::kR},
            SkYUVAIndex{1, SkColorChannel::kR},
            SkYUVAIndex{2, SkColorChannel::kR},
            SkYUVAIndex{-1, SkColorChannel::kA}};
    SkISize size{yuvTextures[0].width(), yuvTextures[0].height()};
    return SkImage_Gpu::MakeFromYUVATexturesCopy(ctx, yuvColorSpace, yuvTextures, yuvaIndices,
                                                 size, imageOrigin, std::move(imageColorSpace));
}

sk_sp<SkImage> SkImage::MakeFromYUVTexturesCopyWithExternalBackend(
        GrContext* ctx, SkYUVColorSpace yuvColorSpace, const GrBackendTexture yuvTextures[3],
        GrSurfaceOrigin imageOrigin, const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> imageColorSpace) {
    SkYUVAIndex yuvaIndices[4] = {
            SkYUVAIndex{0, SkColorChannel::kR},
            SkYUVAIndex{1, SkColorChannel::kR},
            SkYUVAIndex{2, SkColorChannel::kR},
            SkYUVAIndex{-1, SkColorChannel::kA}};
    SkISize size{yuvTextures[0].width(), yuvTextures[0].height()};
    return SkImage_Gpu::MakeFromYUVATexturesCopyWithExternalBackend(
            ctx, yuvColorSpace, yuvTextures, yuvaIndices, size, imageOrigin, backendTexture,
            std::move(imageColorSpace));
}

sk_sp<SkImage> SkImage::MakeFromNV12TexturesCopy(GrContext* ctx, SkYUVColorSpace yuvColorSpace,
                                                 const GrBackendTexture nv12Textures[2],
                                                 GrSurfaceOrigin imageOrigin,
                                                 sk_sp<SkColorSpace> imageColorSpace) {
    // TODO: SkImageSourceChannel input is being ingored right now. Setup correctly in the future.
    SkYUVAIndex yuvaIndices[4] = {
            SkYUVAIndex{0, SkColorChannel::kR},
            SkYUVAIndex{1, SkColorChannel::kR},
            SkYUVAIndex{1, SkColorChannel::kG},
            SkYUVAIndex{-1, SkColorChannel::kA}};
    SkISize size{nv12Textures[0].width(), nv12Textures[0].height()};
    return SkImage_Gpu::MakeFromYUVATexturesCopy(ctx, yuvColorSpace, nv12Textures, yuvaIndices,
                                                 size, imageOrigin, std::move(imageColorSpace));
}

sk_sp<SkImage> SkImage::MakeFromNV12TexturesCopyWithExternalBackend(
        GrContext* ctx,
        SkYUVColorSpace yuvColorSpace,
        const GrBackendTexture nv12Textures[2],
        GrSurfaceOrigin imageOrigin,
        const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> imageColorSpace) {
    SkYUVAIndex yuvaIndices[4] = {
            SkYUVAIndex{0, SkColorChannel::kR},
            SkYUVAIndex{1, SkColorChannel::kR},
            SkYUVAIndex{1, SkColorChannel::kG},
            SkYUVAIndex{-1, SkColorChannel::kA}};
    SkISize size{nv12Textures[0].width(), nv12Textures[0].height()};
    return SkImage_Gpu::MakeFromYUVATexturesCopyWithExternalBackend(
            ctx, yuvColorSpace, nv12Textures, yuvaIndices, size, imageOrigin, backendTexture,
            std::move(imageColorSpace));
}

static sk_sp<SkImage> create_image_from_producer(GrContext* context, GrTextureProducer* producer,
                                                 SkAlphaType at, uint32_t id,
                                                 SkColorSpace* dstColorSpace,
                                                 GrMipMapped mipMapped) {
    sk_sp<SkColorSpace> texColorSpace;
    sk_sp<GrTextureProxy> proxy(producer->refTextureProxy(mipMapped, dstColorSpace,
                                                          &texColorSpace));
    if (!proxy) {
        return nullptr;
    }
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(context), id, at, std::move(proxy),
                                   std::move(texColorSpace), SkBudgeted::kNo);
}

sk_sp<SkImage> SkImage::makeTextureImage(GrContext* context, SkColorSpace* dstColorSpace,
                                         GrMipMapped mipMapped) const {
    if (!context) {
        return nullptr;
    }
    if (GrContext* incumbent = as_IB(this)->context()) {
        if (incumbent != context) {
            return nullptr;
        }
        sk_sp<GrTextureProxy> proxy = as_IB(this)->asTextureProxyRef();
        SkASSERT(proxy);
        if (GrMipMapped::kNo == mipMapped || proxy->mipMapped() == mipMapped) {
            return sk_ref_sp(const_cast<SkImage*>(this));
        }
        GrTextureAdjuster adjuster(context, std::move(proxy), this->alphaType(),
                                   this->uniqueID(), this->colorSpace());
        return create_image_from_producer(context, &adjuster, this->alphaType(),
                                          this->uniqueID(), dstColorSpace, mipMapped);
    }

    if (this->isLazyGenerated()) {
        GrImageTextureMaker maker(context, this, kDisallow_CachingHint);
        return create_image_from_producer(context, &maker, this->alphaType(),
                                          this->uniqueID(), dstColorSpace, mipMapped);
    }

    if (const SkBitmap* bmp = as_IB(this)->onPeekBitmap()) {
        GrBitmapTextureMaker maker(context, *bmp);
        return create_image_from_producer(context, &maker, this->alphaType(),
                                          this->uniqueID(), dstColorSpace, mipMapped);
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static GrTextureType TextureTypeFromBackendFormat(const GrBackendFormat& backendFormat) {
    if (const GrGLenum* target = backendFormat.getGLTarget()) {
        return GrGLTexture::TextureTypeFromTarget(*target);
    }
    return GrTextureType::k2D;
}

sk_sp<SkImage> SkImage_Gpu::MakePromiseTexture(GrContext* context,
                                               const GrBackendFormat& backendFormat,
                                               int width,
                                               int height,
                                               GrMipMapped mipMapped,
                                               GrSurfaceOrigin origin,
                                               SkColorType colorType,
                                               SkAlphaType alphaType,
                                               sk_sp<SkColorSpace> colorSpace,
                                               TextureFulfillProc textureFulfillProc,
                                               TextureReleaseProc textureReleaseProc,
                                               PromiseDoneProc promiseDoneProc,
                                               TextureContext textureContext) {
    // The contract here is that if 'promiseDoneProc' is passed in it should always be called,
    // even if creation of the SkImage fails.
    if (!promiseDoneProc) {
        return nullptr;
    }

    SkPromiseImageHelper promiseHelper(textureFulfillProc, textureReleaseProc, promiseDoneProc,
                                       textureContext);

    if (!context) {
        return nullptr;
    }

    if (width <= 0 || height <= 0) {
        return nullptr;
    }

    if (!textureFulfillProc || !textureReleaseProc) {
        return nullptr;
    }

    SkImageInfo info = SkImageInfo::Make(width, height, colorType, alphaType, colorSpace);
    if (!SkImageInfoIsValid(info)) {
        return nullptr;
    }
    GrPixelConfig config = kUnknown_GrPixelConfig;
    if (!context->contextPriv().caps()->getConfigFromBackendFormat(backendFormat, colorType,
                                                                   &config)) {
        return nullptr;
    }

    GrTextureType textureType = TextureTypeFromBackendFormat(backendFormat);

    if (mipMapped == GrMipMapped::kYes && GrTextureTypeHasRestrictedSampling(textureType)) {
        // It is invalid to have a GL_TEXTURE_EXTERNAL or GL_TEXTURE_RECTANGLE and have mips as
        // well.
        return nullptr;
    }

    GrProxyProvider* proxyProvider = context->contextPriv().proxyProvider();

    GrSurfaceDesc desc;
    desc.fWidth = width;
    desc.fHeight = height;
    desc.fConfig = config;

    sk_sp<GrTextureProxy> proxy = proxyProvider->createLazyProxy(
            [promiseHelper, config](GrResourceProvider* resourceProvider) mutable {
                if (!resourceProvider) {
                    promiseHelper.reset();
                    return sk_sp<GrTexture>();
                }

                return promiseHelper.getTexture(resourceProvider, config);
            },
            desc, origin, mipMapped, textureType, GrInternalSurfaceFlags::kNone,
            SkBackingFit::kExact, SkBudgeted::kNo,
            GrSurfaceProxy::LazyInstantiationType::kUninstantiate);

    if (!proxy) {
        return nullptr;
    }

    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(context), kNeedNewImageUniqueID, alphaType,
                                   std::move(proxy), std::move(colorSpace), SkBudgeted::kNo);
}

sk_sp<SkImage> SkImage_Gpu::MakePromiseYUVATexture(GrContext* context,
                                                   SkYUVColorSpace yuvColorSpace,
                                                   const GrBackendFormat yuvaFormats[],
                                                   const SkYUVAIndex yuvaIndices[4],
                                                   int imageWidth,
                                                   int imageHeight,
                                                   GrSurfaceOrigin imageOrigin,
                                                   sk_sp<SkColorSpace> imageColorSpace,
                                                   TextureFulfillProc textureFulfillProc,
                                                   TextureReleaseProc textureReleaseProc,
                                                   PromiseDoneProc promiseDoneProc,
                                                   TextureContext textureContexts[]) {
    // The contract here is that if 'promiseDoneProc' is passed in it should always be called,
    // even if creation of the SkImage fails.
    if (!promiseDoneProc) {
        return nullptr;
    }

    // Temporarily work around an MSVC compiler bug. Copying the arrays directly into the lambda
    // doesn't work on some older tool chains
    struct {
        GrPixelConfig fConfigs[4] = { kUnknown_GrPixelConfig, kUnknown_GrPixelConfig,
                                      kUnknown_GrPixelConfig, kUnknown_GrPixelConfig };
        SkPromiseImageHelper fPromiseHelpers[4];
        SkYUVAIndex fLocalIndices[4];
    } params;

    // Determine which of the slots in 'yuvaFormats' and 'textureContexts' are actually used
    bool slotUsed[4] = { false, false, false, false };
    for (int i = 0; i < 4; ++i) {
        if (yuvaIndices[i].fIndex < 0) {
            SkASSERT(SkYUVAIndex::kA_Index == i); // We had better have YUV channels
            continue;
        }

        SkASSERT(yuvaIndices[i].fIndex < 4);
        slotUsed[yuvaIndices[i].fIndex] = true;
    }

    for (int i = 0; i < 4; ++i) {
        params.fLocalIndices[i] = yuvaIndices[i];

        if (slotUsed[i]) {
            params.fPromiseHelpers[i].set(textureFulfillProc, textureReleaseProc,
                                          promiseDoneProc, textureContexts[i]);
        }
    }

    // DDL TODO: we need to create a SkImage_GpuYUVA here! This implementation just
    // returns the Y-plane.
    if (!context) {
        return nullptr;
    }

    if (imageWidth <= 0 || imageHeight <= 0) {
        return nullptr;
    }

    if (!textureFulfillProc || !textureReleaseProc) {
        return nullptr;
    }

    SkImageInfo info = SkImageInfo::Make(imageWidth, imageHeight, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType, imageColorSpace);
    if (!SkImageInfoIsValid(info)) {
        return nullptr;
    }

    for (int i = 0; i < 4; ++i) {
        if (slotUsed[i]) {
            // DDL TODO: This (the kAlpha_8) only works for non-NV12 YUV textures
            if (!context->contextPriv().caps()->getConfigFromBackendFormat(yuvaFormats[i],
                                                                           kAlpha_8_SkColorType,
                                                                           &params.fConfigs[i])) {
                return nullptr;
            }
        }
    }

    GrSurfaceDesc desc;
    desc.fFlags = kNone_GrSurfaceFlags;
    desc.fWidth = imageWidth;
    desc.fHeight = imageHeight;
    // Hack since we're just returning the Y-plane
    desc.fConfig = params.fConfigs[params.fLocalIndices[SkYUVAIndex::kY_Index].fIndex];
    desc.fSampleCnt = 1;

    GrProxyProvider* proxyProvider = context->contextPriv().proxyProvider();

    sk_sp<GrTextureProxy> proxy = proxyProvider->createLazyProxy(
            [params](GrResourceProvider* resourceProvider) mutable {
                if (!resourceProvider) {
                    for (int i = 0; i < 4; ++i) {
                        if (params.fPromiseHelpers[i].isValid()) {
                            params.fPromiseHelpers[i].reset();
                        }
                    }
                    return sk_sp<GrTexture>();
                }

                // We need to collect the YUVA planes as backend textures (vs. GrTextures) to
                // feed into the SkImage_GpuYUVA factory.
                GrBackendTexture yuvaTextures[4];
                for (int i = 0; i < 4; ++i) {
                    if (params.fPromiseHelpers[i].isValid()) {
                        sk_sp<GrTexture> tmp = params.fPromiseHelpers[i].getTexture(
                            resourceProvider, params.fConfigs[i]);
                        if (!tmp) {
                            return sk_sp<GrTexture>();
                        }
                        yuvaTextures[i] = tmp->getBackendTexture();
                    }
                }

#if 1
                // For the time being, simply return the Y-plane. The reason for this is that
                // this lazy proxy is instantiated at flush time, after the sort, therefore
                // we cannot be introducing a new opList (in order to render the YUV texture).
                int yIndex = params.fLocalIndices[SkYUVAIndex::kY_Index].fIndex;
                return params.fPromiseHelpers[yIndex].getTexture(resourceProvider,
                                                                 params.fConfigs[yIndex]);
#else
                GrGpu* gpu = resourceProvider->priv().gpu();
                GrContext* context = gpu->getContext();

                sk_sp<SkImage> tmp = SkImage_Gpu::MakeFromYUVATexturesCopyImpl(context,
                                                                               yuvColorSpace,
                                                                               yuvaTextures,
                                                                               localIndices,
                                                                               imageSize,
                                                                               imageOrigin,
                                                                               imageColorSpace);
                if (!tmp) {
                    return sk_sp<GrTexture>();
                }
                return sk_ref_sp<GrTexture>(tmp->getTexture());
#endif
            },
            desc, imageOrigin, GrMipMapped::kNo, GrTextureType::k2D,
            GrInternalSurfaceFlags::kNone, SkBackingFit::kExact, SkBudgeted::kNo,
            GrSurfaceProxy::LazyInstantiationType::kUninstantiate);

    if (!proxy) {
        return nullptr;
    }

    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(context), kNeedNewImageUniqueID, kPremul_SkAlphaType,
                                   std::move(proxy), std::move(imageColorSpace), SkBudgeted::kNo);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage::MakeCrossContextFromEncoded(GrContext* context, sk_sp<SkData> encoded,
                                                    bool buildMips, SkColorSpace* dstColorSpace,
                                                    bool limitToMaxTextureSize) {
    sk_sp<SkImage> codecImage = SkImage::MakeFromEncoded(std::move(encoded));
    if (!codecImage) {
        return nullptr;
    }

    // Some backends or drivers don't support (safely) moving resources between contexts
    if (!context || !context->contextPriv().caps()->crossContextTextureSupport()) {
        return codecImage;
    }

    auto maxTextureSize = context->contextPriv().caps()->maxTextureSize();
    if (limitToMaxTextureSize &&
        (codecImage->width() > maxTextureSize || codecImage->height() > maxTextureSize)) {
        SkAutoPixmapStorage pmap;
        SkImageInfo info = as_IB(codecImage)->onImageInfo();
        if (!dstColorSpace) {
            info = info.makeColorSpace(nullptr);
        }
        if (!pmap.tryAlloc(info) || !codecImage->readPixels(pmap, 0, 0, kDisallow_CachingHint)) {
            return nullptr;
        }
        return MakeCrossContextFromPixmap(context, pmap, buildMips, dstColorSpace, true);
    }

    // Turn the codec image into a GrTextureProxy
    GrImageTextureMaker maker(context, codecImage.get(), kDisallow_CachingHint);
    sk_sp<SkColorSpace> texColorSpace;
    GrSamplerState samplerState(
            GrSamplerState::WrapMode::kClamp,
            buildMips ? GrSamplerState::Filter::kMipMap : GrSamplerState::Filter::kBilerp);
    sk_sp<GrTextureProxy> proxy(
            maker.refTextureProxyForParams(samplerState, dstColorSpace, &texColorSpace, nullptr));
    if (!proxy) {
        return codecImage;
    }

    if (!proxy->instantiate(context->contextPriv().resourceProvider())) {
        return codecImage;
    }
    sk_sp<GrTexture> texture = sk_ref_sp(proxy->peekTexture());

    // Flush any writes or uploads
    context->contextPriv().prepareSurfaceForExternalIO(proxy.get());

    GrGpu* gpu = context->contextPriv().getGpu();
    sk_sp<GrSemaphore> sema = gpu->prepareTextureForCrossContextUsage(texture.get());

    auto gen = GrBackendTextureImageGenerator::Make(std::move(texture), proxy->origin(),
                                                    std::move(sema),
                                                    as_IB(codecImage)->onImageInfo().colorType(),
                                                    codecImage->alphaType(),
                                                    std::move(texColorSpace));
    return SkImage::MakeFromGenerator(std::move(gen));
}

sk_sp<SkImage> SkImage::MakeCrossContextFromPixmap(GrContext* context,
                                                   const SkPixmap& originalPixmap, bool buildMips,
                                                   SkColorSpace* dstColorSpace,
                                                   bool limitToMaxTextureSize) {
    // Some backends or drivers don't support (safely) moving resources between contexts
    if (!context || !context->contextPriv().caps()->crossContextTextureSupport()) {
        return SkImage::MakeRasterCopy(originalPixmap);
    }

    // If we don't have access to the resource provider and gpu (i.e. in a DDL context) we will not
    // be able to make everything needed for a GPU CrossContext image. Thus return a raster copy
    // instead.
    if (!context->contextPriv().resourceProvider()) {
        return SkImage::MakeRasterCopy(originalPixmap);
    }

    const SkPixmap* pixmap = &originalPixmap;
    SkAutoPixmapStorage resized;
    int maxTextureSize = context->contextPriv().caps()->maxTextureSize();
    int maxDim = SkTMax(originalPixmap.width(), originalPixmap.height());
    if (limitToMaxTextureSize && maxDim > maxTextureSize) {
        float scale = static_cast<float>(maxTextureSize) / maxDim;
        int newWidth = SkTMin(static_cast<int>(originalPixmap.width() * scale), maxTextureSize);
        int newHeight = SkTMin(static_cast<int>(originalPixmap.height() * scale), maxTextureSize);
        SkImageInfo info = originalPixmap.info().makeWH(newWidth, newHeight);
        if (!resized.tryAlloc(info) || !originalPixmap.scalePixels(resized, kLow_SkFilterQuality)) {
            return nullptr;
        }
        pixmap = &resized;
    }
    GrProxyProvider* proxyProvider = context->contextPriv().proxyProvider();
    // Turn the pixmap into a GrTextureProxy
    sk_sp<GrTextureProxy> proxy;
    if (buildMips) {
        SkBitmap bmp;
        bmp.installPixels(*pixmap);
        proxy = proxyProvider->createMipMapProxyFromBitmap(bmp);
    } else {
        if (SkImageInfoIsValid(pixmap->info())) {
            ATRACE_ANDROID_FRAMEWORK("Upload Texture [%ux%u]", pixmap->width(), pixmap->height());
            // We don't need a release proc on the data in pixmap since we know we are in a
            // GrContext that has a resource provider. Thus the createTextureProxy call will
            // immediately upload the data.
            sk_sp<SkImage> image = SkImage::MakeFromRaster(*pixmap, nullptr, nullptr);
            proxy = proxyProvider->createTextureProxy(std::move(image), kNone_GrSurfaceFlags, 1,
                                                      SkBudgeted::kYes, SkBackingFit::kExact);
        }
    }

    if (!proxy) {
        return SkImage::MakeRasterCopy(*pixmap);
    }

    sk_sp<GrTexture> texture = sk_ref_sp(proxy->peekTexture());

    // Flush any writes or uploads
    context->contextPriv().prepareSurfaceForExternalIO(proxy.get());
    GrGpu* gpu = context->contextPriv().getGpu();

    sk_sp<GrSemaphore> sema = gpu->prepareTextureForCrossContextUsage(texture.get());

    auto gen = GrBackendTextureImageGenerator::Make(std::move(texture), proxy->origin(),
                                                    std::move(sema), pixmap->colorType(),
                                                    pixmap->alphaType(),
                                                    pixmap->info().refColorSpace());
    return SkImage::MakeFromGenerator(std::move(gen));
}

#if defined(SK_BUILD_FOR_ANDROID) && __ANDROID_API__ >= 26
sk_sp<SkImage> SkImage::MakeFromAHardwareBuffer(AHardwareBuffer* graphicBuffer, SkAlphaType at,
                                                sk_sp<SkColorSpace> cs,
                                                GrSurfaceOrigin surfaceOrigin) {
    auto gen = GrAHardwareBufferImageGenerator::Make(graphicBuffer, at, cs, surfaceOrigin);
    return SkImage::MakeFromGenerator(std::move(gen));
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

bool SkImage::MakeBackendTextureFromSkImage(GrContext* ctx,
                                            sk_sp<SkImage> image,
                                            GrBackendTexture* backendTexture,
                                            BackendTextureReleaseProc* releaseProc) {
    if (!image || !ctx || !backendTexture || !releaseProc) {
        return false;
    }

    // Ensure we have a texture backed image.
    if (!image->isTextureBacked()) {
        image = image->makeTextureImage(ctx, nullptr);
        if (!image) {
            return false;
        }
    }
    GrTexture* texture = image->getTexture();
    if (!texture) {
        // In context-loss cases, we may not have a texture.
        return false;
    }

    // If the image's context doesn't match the provided context, fail.
    if (texture->getContext() != ctx) {
        return false;
    }

    // Flush any pending IO on the texture.
    ctx->contextPriv().prepareSurfaceForExternalIO(as_IB(image)->peekProxy());
    SkASSERT(!texture->surfacePriv().hasPendingIO());

    // We must make a copy of the image if the image is not unique, if the GrTexture owned by the
    // image is not unique, or if the texture wraps an external object.
    if (!image->unique() || !texture->surfacePriv().hasUniqueRef() ||
        texture->resourcePriv().refsWrappedObjects()) {
        // onMakeSubset will always copy the image.
        image = as_IB(image)->onMakeSubset(image->bounds());
        if (!image) {
            return false;
        }

        texture = image->getTexture();
        if (!texture) {
            return false;
        }

        // Flush to ensure that the copy is completed before we return the texture.
        ctx->contextPriv().prepareSurfaceForExternalIO(as_IB(image)->peekProxy());
        SkASSERT(!texture->surfacePriv().hasPendingIO());
    }

    SkASSERT(!texture->resourcePriv().refsWrappedObjects());
    SkASSERT(texture->surfacePriv().hasUniqueRef());
    SkASSERT(image->unique());

    // Take a reference to the GrTexture and release the image.
    sk_sp<GrTexture> textureRef(SkSafeRef(texture));
    image = nullptr;

    // Steal the backend texture from the GrTexture, releasing the GrTexture in the process.
    return GrTexture::StealBackendTexture(std::move(textureRef), backendTexture, releaseProc);
}
