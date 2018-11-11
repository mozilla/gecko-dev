/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkBitmapCache.h"
#include "SkCanvas.h"
#include "SkData.h"
#include "SkImageEncoder.h"
#include "SkImageFilter.h"
#include "SkImageFilterCache.h"
#include "SkImageGenerator.h"
#include "SkImagePriv.h"
#include "SkImageShader.h"
#include "SkImage_Base.h"
#include "SkNextID.h"
#include "SkPicture.h"
#include "SkPixelRef.h"
#include "SkReadPixelsRec.h"
#include "SkSpecialImage.h"
#include "SkString.h"
#include "SkSurface.h"

#if SK_SUPPORT_GPU
#include "GrTexture.h"
#include "GrContext.h"
#include "SkImage_Gpu.h"
#endif
#include "GrBackendSurface.h"

SkImage::SkImage(int width, int height, uint32_t uniqueID)
    : fWidth(width)
    , fHeight(height)
    , fUniqueID(kNeedNewImageUniqueID == uniqueID ? SkNextID::ImageID() : uniqueID)
{
    SkASSERT(width > 0);
    SkASSERT(height > 0);
}

bool SkImage::peekPixels(SkPixmap* pm) const {
    SkPixmap tmp;
    if (!pm) {
        pm = &tmp;
    }
    return as_IB(this)->onPeekPixels(pm);
}

bool SkImage::readPixels(const SkImageInfo& dstInfo, void* dstPixels, size_t dstRowBytes,
                           int srcX, int srcY, CachingHint chint) const {
    return as_IB(this)->onReadPixels(dstInfo, dstPixels, dstRowBytes, srcX, srcY, chint);
}

bool SkImage::scalePixels(const SkPixmap& dst, SkFilterQuality quality, CachingHint chint) const {
    if (this->width() == dst.width() && this->height() == dst.height()) {
        return this->readPixels(dst, 0, 0, chint);
    }

    // Idea: If/when SkImageGenerator supports a native-scaling API (where the generator itself
    //       can scale more efficiently) we should take advantage of it here.
    //
    SkBitmap bm;
    if (as_IB(this)->getROPixels(&bm, dst.info().colorSpace(), chint)) {
        SkPixmap pmap;
        // Note: By calling the pixmap scaler, we never cache the final result, so the chint
        //       is (currently) only being applied to the getROPixels. If we get a request to
        //       also attempt to cache the final (scaled) result, we would add that logic here.
        //
        return bm.peekPixels(&pmap) && pmap.scalePixels(dst, quality);
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

SkColorType SkImage::colorType() const {
    return as_IB(this)->onImageInfo().colorType();
}

SkAlphaType SkImage::alphaType() const {
    return as_IB(this)->onImageInfo().alphaType();
}

SkColorSpace* SkImage::colorSpace() const {
    return as_IB(this)->onImageInfo().colorSpace();
}

sk_sp<SkColorSpace> SkImage::refColorSpace() const {
    return as_IB(this)->onImageInfo().refColorSpace();
}

sk_sp<SkShader> SkImage::makeShader(SkShader::TileMode tileX, SkShader::TileMode tileY,
                                    const SkMatrix* localMatrix) const {
    return SkImageShader::Make(sk_ref_sp(const_cast<SkImage*>(this)), tileX, tileY, localMatrix);
}

sk_sp<SkData> SkImage::encodeToData(SkEncodedImageFormat type, int quality) const {
    SkBitmap bm;
    SkColorSpace* legacyColorSpace = nullptr;
    if (as_IB(this)->getROPixels(&bm, legacyColorSpace)) {
        return SkEncodeBitmap(bm, type, quality);
    }
    return nullptr;
}

sk_sp<SkData> SkImage::encodeToData() const {
    if (auto encoded = this->refEncodedData()) {
        return encoded;
    }

    SkBitmap bm;
    SkPixmap pmap;
    SkColorSpace* legacyColorSpace = nullptr;
    if (as_IB(this)->getROPixels(&bm, legacyColorSpace) && bm.peekPixels(&pmap)) {
        return SkEncodePixmap(pmap, SkEncodedImageFormat::kPNG, 100);
    }
    return nullptr;
}

sk_sp<SkData> SkImage::refEncodedData() const {
    return sk_sp<SkData>(as_IB(this)->onRefEncoded());
}

sk_sp<SkImage> SkImage::MakeFromEncoded(sk_sp<SkData> encoded, const SkIRect* subset) {
    if (nullptr == encoded || 0 == encoded->size()) {
        return nullptr;
    }
    return SkImage::MakeFromGenerator(SkImageGenerator::MakeFromEncoded(std::move(encoded)),
                                      subset);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage::makeSubset(const SkIRect& subset) const {
    if (subset.isEmpty()) {
        return nullptr;
    }

    const SkIRect bounds = SkIRect::MakeWH(this->width(), this->height());
    if (!bounds.contains(subset)) {
        return nullptr;
    }

    // optimization : return self if the subset == our bounds
    if (bounds == subset) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }
    return as_IB(this)->onMakeSubset(subset);
}

#if SK_SUPPORT_GPU

GrTexture* SkImage::getTexture() const {
    return as_IB(this)->onGetTexture();
}

bool SkImage::isTextureBacked() const { return SkToBool(as_IB(this)->peekProxy()); }

GrBackendTexture SkImage::getBackendTexture(bool flushPendingGrContextIO,
                                            GrSurfaceOrigin* origin) const {
    return as_IB(this)->onGetBackendTexture(flushPendingGrContextIO, origin);
}

bool SkImage::isValid(GrContext* context) const {
    if (context && context->abandoned()) {
        return false;
    }
    return as_IB(this)->onIsValid(context);
}

#else

GrTexture* SkImage::getTexture() const { return nullptr; }

bool SkImage::isTextureBacked() const { return false; }

GrBackendTexture SkImage::getBackendTexture(bool flushPendingGrContextIO,
                                            GrSurfaceOrigin* origin) const {
    return GrBackendTexture(); // invalid
}

bool SkImage::isValid(GrContext* context) const {
    if (context) {
        return false;
    }
    return as_IB(this)->onIsValid(context);
}

#endif

///////////////////////////////////////////////////////////////////////////////

SkImage_Base::SkImage_Base(int width, int height, uint32_t uniqueID)
    : INHERITED(width, height, uniqueID)
    , fAddedToRasterCache(false)
{}

SkImage_Base::~SkImage_Base() {
    if (fAddedToRasterCache.load()) {
        SkNotifyBitmapGenIDIsStale(this->uniqueID());
    }
}

GrBackendTexture SkImage_Base::onGetBackendTexture(bool flushPendingGrContextIO,
                                                   GrSurfaceOrigin* origin) const {
    return GrBackendTexture(); // invalid
}

bool SkImage::readPixels(const SkPixmap& pmap, int srcX, int srcY, CachingHint chint) const {
    return this->readPixels(pmap.info(), pmap.writable_addr(), pmap.rowBytes(), srcX, srcY, chint);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage::MakeFromBitmap(const SkBitmap& bm) {
    SkPixelRef* pr = bm.pixelRef();
    if (nullptr == pr) {
        return nullptr;
    }

    return SkMakeImageFromRasterBitmap(bm, kIfMutable_SkCopyPixelsMode);
}

bool SkImage::asLegacyBitmap(SkBitmap* bitmap, LegacyBitmapMode ) const {
    return as_IB(this)->onAsLegacyBitmap(bitmap);
}

sk_sp<SkCachedData> SkImage_Base::getPlanes(SkYUVSizeInfo*, SkYUVColorSpace*,const void*[3]) {
    return nullptr;
}

bool SkImage_Base::onAsLegacyBitmap(SkBitmap* bitmap) const {
    // As the base-class, all we can do is make a copy (regardless of mode).
    // Subclasses that want to be more optimal should override.
    SkImageInfo info = this->onImageInfo().makeColorType(kN32_SkColorType).makeColorSpace(nullptr);
    if (!bitmap->tryAllocPixels(info)) {
        return false;
    }
    if (!this->readPixels(bitmap->info(), bitmap->getPixels(), bitmap->rowBytes(), 0, 0)) {
        bitmap->reset();
        return false;
    }

    bitmap->setImmutable();
    return true;
}

sk_sp<SkImage> SkImage::MakeFromPicture(sk_sp<SkPicture> picture, const SkISize& dimensions,
                                        const SkMatrix* matrix, const SkPaint* paint,
                                        BitDepth bitDepth, sk_sp<SkColorSpace> colorSpace) {
    return MakeFromGenerator(SkImageGenerator::MakeFromPicture(dimensions, std::move(picture),
                                                               matrix, paint, bitDepth,
                                                               std::move(colorSpace)));
}

sk_sp<SkImage> SkImage::makeWithFilter(const SkImageFilter* filter, const SkIRect& subset,
                                       const SkIRect& clipBounds, SkIRect* outSubset,
                                       SkIPoint* offset) const {
    if (!filter || !outSubset || !offset || !this->bounds().contains(subset)) {
        return nullptr;
    }
    SkColorType colorType = as_IB(this)->onImageInfo().colorType();
    SkColorSpace* colorSpace = as_IB(this)->onImageInfo().colorSpace();
    sk_sp<SkSpecialImage> srcSpecialImage = SkSpecialImage::MakeFromImage(
        subset, sk_ref_sp(const_cast<SkImage*>(this)), colorSpace);
    if (!srcSpecialImage) {
        return nullptr;
    }

    sk_sp<SkImageFilterCache> cache(
        SkImageFilterCache::Create(SkImageFilterCache::kDefaultTransientSize));
    SkImageFilter::OutputProperties outputProperties(colorType, colorSpace);
    SkImageFilter::Context context(SkMatrix::I(), clipBounds, cache.get(), outputProperties);

    sk_sp<SkSpecialImage> result = filter->filterImage(srcSpecialImage.get(), context, offset);
    if (!result) {
        return nullptr;
    }

    *outSubset = SkIRect::MakeWH(result->width(), result->height());
    if (!outSubset->intersect(clipBounds.makeOffset(-offset->x(), -offset->y()))) {
        return nullptr;
    }
    offset->fX += outSubset->x();
    offset->fY += outSubset->y();

    // Note that here we're returning the special image's entire backing store, loose padding
    // and all!
    return result->asImage();
}

bool SkImage::isLazyGenerated() const {
    return as_IB(this)->onIsLazyGenerated();
}

bool SkImage::isAlphaOnly() const {
    return as_IB(this)->onImageInfo().colorType() == kAlpha_8_SkColorType;
}

sk_sp<SkImage> SkImage::makeColorSpace(sk_sp<SkColorSpace> target) const {
    if (!target) {
        return nullptr;
    }

    // No need to create a new image if:
    // (1) The color spaces are equal.
    // (2) The color type is kAlpha8.
    if (SkColorSpace::Equals(this->colorSpace(), target.get()) ||
            kAlpha_8_SkColorType == as_IB(this)->onImageInfo().colorType()) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }

    // TODO: We might consider making this a deferred conversion?
    return as_IB(this)->onMakeColorSpace(std::move(target));
}

sk_sp<SkImage> SkImage::makeNonTextureImage() const {
    if (!this->isTextureBacked()) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }
    return this->makeRasterImage();
}

sk_sp<SkImage> SkImage::makeRasterImage() const {
    SkPixmap pm;
    if (this->peekPixels(&pm)) {
        return sk_ref_sp(const_cast<SkImage*>(this));
    }

    const SkImageInfo info = as_IB(this)->onImageInfo();
    const size_t rowBytes = info.minRowBytes();
    size_t size = info.computeByteSize(rowBytes);
    if (SkImageInfo::ByteSizeOverflowed(size)) {
        return nullptr;
    }

    sk_sp<SkData> data = SkData::MakeUninitialized(size);
    pm = { info.makeColorSpace(nullptr), data->writable_data(), info.minRowBytes() };
    if (!this->readPixels(pm, 0, 0)) {
        return nullptr;
    }

    return SkImage::MakeRasterData(info, std::move(data), rowBytes);
}

//////////////////////////////////////////////////////////////////////////////////////

#if !SK_SUPPORT_GPU

sk_sp<SkImage> SkImage::MakeFromTexture(GrContext* ctx,
                                        const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                        SkColorType ct, SkAlphaType at, sk_sp<SkColorSpace> cs,
                                        TextureReleaseProc releaseP, ReleaseContext releaseC) {
    return nullptr;
}

bool SkImage::MakeBackendTextureFromSkImage(GrContext*,
                                            sk_sp<SkImage>,
                                            GrBackendTexture*,
                                            BackendTextureReleaseProc*) {
    return false;
}

sk_sp<SkImage> SkImage::MakeFromAdoptedTexture(GrContext* ctx,
                                               const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                               SkColorType ct, SkAlphaType at,
                                               sk_sp<SkColorSpace> cs) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromYUVATexturesCopy(GrContext* context,
                                                 SkYUVColorSpace yuvColorSpace,
                                                 const GrBackendTexture yuvaTextures[],
                                                 const SkYUVAIndex yuvaIndices[4],
                                                 SkISize imageSize,
                                                 GrSurfaceOrigin imageOrigin,
                                                 sk_sp<SkColorSpace> imageColorSpace) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromYUVATexturesCopyWithExternalBackend(
        GrContext* context,
        SkYUVColorSpace yuvColorSpace,
        const GrBackendTexture yuvaTextures[],
        const SkYUVAIndex yuvaIndices[4],
        SkISize imageSize,
        GrSurfaceOrigin imageOrigin,
        const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> imageColorSpace) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromYUVTexturesCopy(GrContext* ctx, SkYUVColorSpace space,
                                                const GrBackendTexture[3],
                                                GrSurfaceOrigin origin,
                                                sk_sp<SkColorSpace> imageColorSpace) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromYUVTexturesCopyWithExternalBackend(
        GrContext* context, SkYUVColorSpace yuvColorSpace, const GrBackendTexture yuvTextures[3],
        GrSurfaceOrigin surfaceOrigin, const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> colorSpace) {
    return nullptr;
}

sk_sp<SkImage> SkImage::MakeFromNV12TexturesCopy(GrContext* ctx, SkYUVColorSpace space,
                                                const GrBackendTexture[2],
                                                GrSurfaceOrigin origin,
                                                sk_sp<SkColorSpace> imageColorSpace) {
    return nullptr;
}

sk_sp<SkImage> SkImage::makeTextureImage(GrContext*, SkColorSpace* dstColorSpace,
                                         GrMipMapped mipMapped) const {
    return nullptr;
}

sk_sp<SkImage> MakeFromNV12TexturesCopyWithExternalBackend(GrContext* context,
                                                           SkYUVColorSpace yuvColorSpace,
                                                           const GrBackendTexture nv12Textures[2],
                                                           GrSurfaceOrigin surfaceOrigin,
                                                           const GrBackendTexture& backendTexture,
                                                           sk_sp<SkColorSpace> colorSpace) {
    return nullptr;
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

bool SkImage_pinAsTexture(const SkImage* image, GrContext* ctx) {
    SkASSERT(image);
    SkASSERT(ctx);
    return as_IB(image)->onPinAsTexture(ctx);
}

void SkImage_unpinAsTexture(const SkImage* image, GrContext* ctx) {
    SkASSERT(image);
    SkASSERT(ctx);
    as_IB(image)->onUnpinAsTexture(ctx);
}

SkIRect SkImage_getSubset(const SkImage* image) {
    SkASSERT(image);
    return as_IB(image)->onGetSubset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImageMakeRasterCopyAndAssignColorSpace(const SkImage* src,
                                                        SkColorSpace* colorSpace) {
    // Read the pixels out of the source image, with no conversion
    SkImageInfo info = as_IB(src)->onImageInfo();
    if (kUnknown_SkColorType == info.colorType()) {
        SkDEBUGFAIL("Unexpected color type");
        return nullptr;
    }

    size_t rowBytes = info.minRowBytes();
    size_t size = info.computeByteSize(rowBytes);
    if (SkImageInfo::ByteSizeOverflowed(size)) {
        return nullptr;
    }
    auto data = SkData::MakeUninitialized(size);
    if (!data) {
        return nullptr;
    }

    SkPixmap pm(info, data->writable_data(), rowBytes);
    if (!src->readPixels(pm, 0, 0, SkImage::kDisallow_CachingHint)) {
        return nullptr;
    }

    // Wrap them in a new image with a different color space
    return SkImage::MakeRasterData(info.makeColorSpace(sk_ref_sp(colorSpace)), data, rowBytes);
}
