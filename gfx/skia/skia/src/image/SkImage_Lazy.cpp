/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImage_Lazy.h"

#include "SkBitmap.h"
#include "SkBitmapCache.h"
#include "SkData.h"
#include "SkImageGenerator.h"
#include "SkImagePriv.h"
#include "SkNextID.h"
#include "SkPixelRef.h"

#if SK_SUPPORT_GPU
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGpuResourcePriv.h"
#include "GrImageTextureMaker.h"
#include "GrResourceKey.h"
#include "GrProxyProvider.h"
#include "GrSamplerState.h"
#include "GrYUVProvider.h"
#include "SkGr.h"
#endif

// Ref-counted tuple(SkImageGenerator, SkMutex) which allows sharing one generator among N images
class SharedGenerator final : public SkNVRefCnt<SharedGenerator> {
public:
    static sk_sp<SharedGenerator> Make(std::unique_ptr<SkImageGenerator> gen) {
        return gen ? sk_sp<SharedGenerator>(new SharedGenerator(std::move(gen))) : nullptr;
    }

    // This is thread safe.  It is a const field set in the constructor.
    const SkImageInfo& getInfo() { return fGenerator->getInfo(); }

private:
    explicit SharedGenerator(std::unique_ptr<SkImageGenerator> gen)
            : fGenerator(std::move(gen)) {
        SkASSERT(fGenerator);
    }

    friend class ScopedGenerator;
    friend class SkImage_Lazy;

    std::unique_ptr<SkImageGenerator> fGenerator;
    SkMutex                           fMutex;
};

///////////////////////////////////////////////////////////////////////////////

SkImage_Lazy::Validator::Validator(sk_sp<SharedGenerator> gen, const SkIRect* subset,
                                   sk_sp<SkColorSpace> colorSpace)
        : fSharedGenerator(std::move(gen)) {
    if (!fSharedGenerator) {
        return;
    }

    // The following generator accessors are safe without acquiring the mutex (const getters).
    // TODO: refactor to use a ScopedGenerator instead, for clarity.
    const SkImageInfo& info = fSharedGenerator->fGenerator->getInfo();
    if (info.isEmpty()) {
        fSharedGenerator.reset();
        return;
    }

    fUniqueID = fSharedGenerator->fGenerator->uniqueID();
    const SkIRect bounds = SkIRect::MakeWH(info.width(), info.height());
    if (subset) {
        if (!bounds.contains(*subset)) {
            fSharedGenerator.reset();
            return;
        }
        if (*subset != bounds) {
            // we need a different uniqueID since we really are a subset of the raw generator
            fUniqueID = SkNextID::ImageID();
        }
    } else {
        subset = &bounds;
    }

    fInfo   = info.makeWH(subset->width(), subset->height());
    fOrigin = SkIPoint::Make(subset->x(), subset->y());
    if (colorSpace) {
        fInfo = fInfo.makeColorSpace(colorSpace);
        fUniqueID = SkNextID::ImageID();
    }
}

///////////////////////////////////////////////////////////////////////////////

// Helper for exclusive access to a shared generator.
class SkImage_Lazy::ScopedGenerator {
public:
    ScopedGenerator(const sk_sp<SharedGenerator>& gen)
      : fSharedGenerator(gen)
      , fAutoAquire(gen->fMutex) {}

    SkImageGenerator* operator->() const {
        fSharedGenerator->fMutex.assertHeld();
        return fSharedGenerator->fGenerator.get();
    }

    operator SkImageGenerator*() const {
        fSharedGenerator->fMutex.assertHeld();
        return fSharedGenerator->fGenerator.get();
    }

private:
    const sk_sp<SharedGenerator>& fSharedGenerator;
    SkAutoExclusive               fAutoAquire;
};

///////////////////////////////////////////////////////////////////////////////

SkImage_Lazy::SkImage_Lazy(Validator* validator)
        : INHERITED(validator->fInfo.width(), validator->fInfo.height(), validator->fUniqueID)
        , fSharedGenerator(std::move(validator->fSharedGenerator))
        , fInfo(validator->fInfo)
        , fOrigin(validator->fOrigin) {
    SkASSERT(fSharedGenerator);
    fUniqueID = validator->fUniqueID;
}

SkImage_Lazy::~SkImage_Lazy() {
#if SK_SUPPORT_GPU
    for (int i = 0; i < fUniqueKeyInvalidatedMessages.count(); ++i) {
        SkMessageBus<GrUniqueKeyInvalidatedMessage>::Post(*fUniqueKeyInvalidatedMessages[i]);
    }
    fUniqueKeyInvalidatedMessages.deleteAll();
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////

static bool check_output_bitmap(const SkBitmap& bitmap, const SkImageInfo& info) {
    SkASSERT(bitmap.isImmutable());
    SkASSERT(bitmap.getPixels());
    SkASSERT(bitmap.colorType() == info.colorType());
    SkASSERT(SkColorSpace::Equals(bitmap.colorSpace(), info.colorSpace()));
    return true;
}

bool SkImage_Lazy::directGeneratePixels(const SkImageInfo& info, void* pixels, size_t rb,
                                        int srcX, int srcY) const {
    ScopedGenerator generator(fSharedGenerator);
    const SkImageInfo& genInfo = generator->getInfo();
    // Currently generators do not natively handle subsets, so check that first.
    if (srcX || srcY || genInfo.width() != info.width() || genInfo.height() != info.height()) {
        return false;
    }

    return generator->getPixels(info, pixels, rb);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool SkImage_Lazy::lockAsBitmapOnlyIfAlreadyCached(SkBitmap* bitmap,
                                                   const SkImageInfo& dstInfo) const {
    auto desc = SkBitmapCacheDesc::Make(fUniqueID, dstInfo.colorType(), dstInfo.colorSpace(),
                                        SkIRect::MakeSize(fInfo.dimensions()));
    return SkBitmapCache::Find(desc, bitmap) &&
           check_output_bitmap(*bitmap, dstInfo);
}

static bool generate_pixels(SkImageGenerator* gen, const SkPixmap& pmap, int originX, int originY) {
    const int genW = gen->getInfo().width();
    const int genH = gen->getInfo().height();
    const SkIRect srcR = SkIRect::MakeWH(genW, genH);
    const SkIRect dstR = SkIRect::MakeXYWH(originX, originY, pmap.width(), pmap.height());
    if (!srcR.contains(dstR)) {
        return false;
    }

    // If they are requesting a subset, we have to have a temp allocation for full image, and
    // then copy the subset into their allocation
    SkBitmap full;
    SkPixmap fullPM;
    const SkPixmap* dstPM = &pmap;
    if (srcR != dstR) {
        if (!full.tryAllocPixels(pmap.info().makeWH(genW, genH))) {
            return false;
        }
        if (!full.peekPixels(&fullPM)) {
            return false;
        }
        dstPM = &fullPM;
    }

    if (!gen->getPixels(dstPM->info(), dstPM->writable_addr(), dstPM->rowBytes())) {
        return false;
    }

    if (srcR != dstR) {
        if (!full.readPixels(pmap, originX, originY)) {
            return false;
        }
    }
    return true;
}

bool SkImage_Lazy::lockAsBitmap(SkBitmap* bitmap, SkImage::CachingHint chint,
                                const SkImageInfo& info) const {
    if (this->lockAsBitmapOnlyIfAlreadyCached(bitmap, info)) {
        return true;
    }

    SkBitmap tmpBitmap;
    SkBitmapCache::RecPtr cacheRec;
    SkPixmap pmap;
    if (SkImage::kAllow_CachingHint == chint) {
        auto desc = SkBitmapCacheDesc::Make(fUniqueID, info.colorType(), info.colorSpace(),
                                            SkIRect::MakeSize(info.dimensions()));
        cacheRec = SkBitmapCache::Alloc(desc, info, &pmap);
        if (!cacheRec) {
            return false;
        }
    } else {
        if (!tmpBitmap.tryAllocPixels(info)) {
            return false;
        }
        if (!tmpBitmap.peekPixels(&pmap)) {
            return false;
        }
    }

    ScopedGenerator generator(fSharedGenerator);
    if (!generate_pixels(generator, pmap, fOrigin.x(), fOrigin.y())) {
        return false;
    }

    if (cacheRec) {
        SkBitmapCache::Add(std::move(cacheRec), bitmap);
        this->notifyAddedToRasterCache();
    } else {
        *bitmap = tmpBitmap;
        bitmap->setImmutable();
    }

    check_output_bitmap(*bitmap, info);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool SkImage_Lazy::onReadPixels(const SkImageInfo& dstInfo, void* dstPixels, size_t dstRB,
                                int srcX, int srcY, CachingHint chint) const {
    SkColorSpace* dstColorSpace = dstInfo.colorSpace();
    SkBitmap bm;
    if (kDisallow_CachingHint == chint) {
        if (this->lockAsBitmapOnlyIfAlreadyCached(&bm, dstInfo)) {
            return bm.readPixels(dstInfo, dstPixels, dstRB, srcX, srcY);
        } else {
            // Try passing the caller's buffer directly down to the generator. If this fails we
            // may still succeed in the general case, as the generator may prefer some other
            // config, which we could then convert via SkBitmap::readPixels.
            if (this->directGeneratePixels(dstInfo, dstPixels, dstRB, srcX, srcY)) {
                return true;
            }
            // else fall through
        }
    }

    if (this->getROPixels(&bm, dstColorSpace, chint)) {
        return bm.readPixels(dstInfo, dstPixels, dstRB, srcX, srcY);
    }
    return false;
}

sk_sp<SkData> SkImage_Lazy::onRefEncoded() const {
    ScopedGenerator generator(fSharedGenerator);
    return generator->refEncodedData();
}

bool SkImage_Lazy::getROPixels(SkBitmap* bitmap, SkColorSpace* dstColorSpace,
                               CachingHint chint) const {
    return this->lockAsBitmap(bitmap, chint, fInfo);
}

bool SkImage_Lazy::onIsValid(GrContext* context) const {
    ScopedGenerator generator(fSharedGenerator);
    return generator->isValid(context);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU
sk_sp<GrTextureProxy> SkImage_Lazy::asTextureProxyRef(GrContext* context,
                                                      const GrSamplerState& params,
                                                      SkColorSpace* dstColorSpace,
                                                      sk_sp<SkColorSpace>* texColorSpace,
                                                      SkScalar scaleAdjust[2]) const {
    if (!context) {
        return nullptr;
    }

    GrImageTextureMaker textureMaker(context, this, kAllow_CachingHint);
    return textureMaker.refTextureProxyForParams(params, dstColorSpace, texColorSpace, scaleAdjust);
}
#endif

sk_sp<SkImage> SkImage_Lazy::onMakeSubset(const SkIRect& subset) const {
    SkASSERT(fInfo.bounds().contains(subset));
    SkASSERT(fInfo.bounds() != subset);

    const SkIRect generatorSubset = subset.makeOffset(fOrigin.x(), fOrigin.y());
    Validator validator(fSharedGenerator, &generatorSubset, fInfo.refColorSpace());
    return validator ? sk_sp<SkImage>(new SkImage_Lazy(&validator)) : nullptr;
}

sk_sp<SkImage> SkImage_Lazy::onMakeColorSpace(sk_sp<SkColorSpace> target) const {
    SkAutoExclusive autoAquire(fOnMakeColorSpaceMutex);
    if (target && fOnMakeColorSpaceTarget &&
        SkColorSpace::Equals(target.get(), fOnMakeColorSpaceTarget.get())) {
        return fOnMakeColorSpaceResult;
    }
    const SkIRect generatorSubset =
            SkIRect::MakeXYWH(fOrigin.x(), fOrigin.y(), fInfo.width(), fInfo.height());
    Validator validator(fSharedGenerator, &generatorSubset, target);
    sk_sp<SkImage> result = validator ? sk_sp<SkImage>(new SkImage_Lazy(&validator)) : nullptr;
    if (result) {
        fOnMakeColorSpaceTarget = target;
        fOnMakeColorSpaceResult = result;
    }
    return result;
}

sk_sp<SkImage> SkImage::MakeFromGenerator(std::unique_ptr<SkImageGenerator> generator,
                                          const SkIRect* subset) {
    SkImage_Lazy::Validator validator(SharedGenerator::Make(std::move(generator)), subset, nullptr);

    return validator ? sk_make_sp<SkImage_Lazy>(&validator) : nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU

void SkImage_Lazy::makeCacheKeyFromOrigKey(const GrUniqueKey& origKey,
                                           GrUniqueKey* cacheKey) const {
    // TODO: Take dstColorSpace, include hash in key
    SkASSERT(!cacheKey->isValid());
    if (origKey.isValid()) {
        static const GrUniqueKey::Domain kDomain = GrUniqueKey::GenerateDomain();
        GrUniqueKey::Builder builder(cacheKey, origKey, kDomain, 0, "Image");
    }
}

class Generator_GrYUVProvider : public GrYUVProvider {
public:
    Generator_GrYUVProvider(SkImageGenerator* gen) : fGen(gen) {}

private:
    uint32_t onGetID() const override { return fGen->uniqueID(); }
    bool onQueryYUV8(SkYUVSizeInfo* sizeInfo, SkYUVColorSpace* colorSpace) const override {
        return fGen->queryYUV8(sizeInfo, colorSpace);
    }
    bool onGetYUV8Planes(const SkYUVSizeInfo& sizeInfo, void* planes[3]) override {
        return fGen->getYUV8Planes(sizeInfo, planes);
    }

    SkImageGenerator* fGen;

    typedef GrYUVProvider INHERITED;
};

static void set_key_on_proxy(GrProxyProvider* proxyProvider,
                             GrTextureProxy* proxy, GrTextureProxy* originalProxy,
                             const GrUniqueKey& key) {
    if (key.isValid()) {
        if (originalProxy && originalProxy->getUniqueKey().isValid()) {
            SkASSERT(originalProxy->getUniqueKey() == key);
            SkASSERT(GrMipMapped::kYes == proxy->mipMapped() &&
                     GrMipMapped::kNo == originalProxy->mipMapped());
            // If we had an originalProxy with a valid key, that means there already is a proxy in
            // the cache which matches the key, but it does not have mip levels and we require them.
            // Thus we must remove the unique key from that proxy.
            proxyProvider->removeUniqueKeyFromProxy(key, originalProxy);
        }
        proxyProvider->assignUniqueKeyToProxy(key, proxy);
    }
}

sk_sp<SkCachedData> SkImage_Lazy::getPlanes(SkYUVSizeInfo* yuvSizeInfo,
                                            SkYUVColorSpace* yuvColorSpace,
                                            const void* planes[3]) {
    ScopedGenerator generator(fSharedGenerator);
    Generator_GrYUVProvider provider(generator);

    sk_sp<SkCachedData> data = provider.getPlanes(yuvSizeInfo, yuvColorSpace, planes);
    if (!data) {
        return nullptr;
    }

    return data;
}


/*
 *  We have 4 ways to try to return a texture (in sorted order)
 *
 *  1. Check the cache for a pre-existing one
 *  2. Ask the generator to natively create one
 *  3. Ask the generator to return YUV planes, which the GPU can convert
 *  4. Ask the generator to return RGB(A) data, which the GPU can convert
 */
sk_sp<GrTextureProxy> SkImage_Lazy::lockTextureProxy(
        GrContext* ctx,
        const GrUniqueKey& origKey,
        SkImage::CachingHint chint,
        bool willBeMipped,
        SkColorSpace* dstColorSpace,
        GrTextureMaker::AllowedTexGenType genType) const {
    // Values representing the various texture lock paths we can take. Used for logging the path
    // taken to a histogram.
    enum LockTexturePath {
        kFailure_LockTexturePath,
        kPreExisting_LockTexturePath,
        kNative_LockTexturePath,
        kCompressed_LockTexturePath, // Deprecated
        kYUV_LockTexturePath,
        kRGBA_LockTexturePath,
    };

    enum { kLockTexturePathCount = kRGBA_LockTexturePath + 1 };

    // Build our texture key.
    // Even though some proxies created here may have a specific origin and use that origin, we do
    // not include that in the key. Since SkImages are meant to be immutable, a given SkImage will
    // always have an associated proxy that is always one origin or the other. It never can change
    // origins. Thus we don't need to include that info in the key iteself.
    // TODO: This needs to include the dstColorSpace.
    GrUniqueKey key;
    this->makeCacheKeyFromOrigKey(origKey, &key);

    GrProxyProvider* proxyProvider = ctx->contextPriv().proxyProvider();
    sk_sp<GrTextureProxy> proxy;

    // 1. Check the cache for a pre-existing one
    if (key.isValid()) {
        proxy = proxyProvider->findOrCreateProxyByUniqueKey(key, kTopLeft_GrSurfaceOrigin);
        if (proxy) {
            SK_HISTOGRAM_ENUMERATION("LockTexturePath", kPreExisting_LockTexturePath,
                                     kLockTexturePathCount);
            if (!willBeMipped || GrMipMapped::kYes == proxy->mipMapped()) {
                return proxy;
            }
        }
    }

    // 2. Ask the generator to natively create one
    if (!proxy) {
        ScopedGenerator generator(fSharedGenerator);
        if (GrTextureMaker::AllowedTexGenType::kCheap == genType &&
                SkImageGenerator::TexGenType::kCheap != generator->onCanGenerateTexture()) {
            return nullptr;
        }
        if ((proxy = generator->generateTexture(ctx, fInfo, fOrigin, willBeMipped))) {
            SK_HISTOGRAM_ENUMERATION("LockTexturePath", kNative_LockTexturePath,
                                     kLockTexturePathCount);
            set_key_on_proxy(proxyProvider, proxy.get(), nullptr, key);
            if (!willBeMipped || GrMipMapped::kYes == proxy->mipMapped()) {
                *fUniqueKeyInvalidatedMessages.append() =
                        new GrUniqueKeyInvalidatedMessage(key, ctx->uniqueID());
                return proxy;
            }
        }
    }

    // 3. Ask the generator to return YUV planes, which the GPU can convert. If we will be mipping
    //    the texture we fall through here and have the CPU generate the mip maps for us.
    if (!proxy && !willBeMipped && !ctx->contextPriv().disableGpuYUVConversion()) {
        const GrSurfaceDesc desc = GrImageInfoToSurfaceDesc(fInfo);
        ScopedGenerator generator(fSharedGenerator);
        Generator_GrYUVProvider provider(generator);

        // The pixels in the texture will be in the generator's color space. If onMakeColorSpace
        // has been called then this will not match this image's color space. To correct this, apply
        // a color space conversion from the generator's color space to this image's color space.
        SkColorSpace* generatorColorSpace = fSharedGenerator->fGenerator->getInfo().colorSpace();
        SkColorSpace* thisColorSpace = fInfo.colorSpace();

        // TODO: Update to create the mipped surface in the YUV generator and draw the base
        // layer directly into the mipped surface.
        proxy = provider.refAsTextureProxy(ctx, desc, generatorColorSpace, thisColorSpace);
        if (proxy) {
            SK_HISTOGRAM_ENUMERATION("LockTexturePath", kYUV_LockTexturePath,
                                     kLockTexturePathCount);
            set_key_on_proxy(proxyProvider, proxy.get(), nullptr, key);
            *fUniqueKeyInvalidatedMessages.append() =
                    new GrUniqueKeyInvalidatedMessage(key, ctx->uniqueID());
            return proxy;
        }
    }

    // 4. Ask the generator to return RGB(A) data, which the GPU can convert
    SkBitmap bitmap;
    if (!proxy && this->lockAsBitmap(&bitmap, chint, fInfo)) {
        if (willBeMipped) {
            proxy = proxyProvider->createMipMapProxyFromBitmap(bitmap);
        }
        if (!proxy) {
            proxy = GrUploadBitmapToTextureProxy(proxyProvider, bitmap);
        }
        if (proxy && (!willBeMipped || GrMipMapped::kYes == proxy->mipMapped())) {
            SK_HISTOGRAM_ENUMERATION("LockTexturePath", kRGBA_LockTexturePath,
                                     kLockTexturePathCount);
            set_key_on_proxy(proxyProvider, proxy.get(), nullptr, key);
            *fUniqueKeyInvalidatedMessages.append() =
                    new GrUniqueKeyInvalidatedMessage(key, ctx->uniqueID());
            return proxy;
        }
    }

    if (proxy) {
        // We need a mipped proxy, but we either found a proxy earlier that wasn't mipped, generated
        // a native non mipped proxy, or generated a non-mipped yuv proxy. Thus we generate a new
        // mipped surface and copy the original proxy into the base layer. We will then let the gpu
        // generate the rest of the mips.
        SkASSERT(willBeMipped);
        SkASSERT(GrMipMapped::kNo == proxy->mipMapped());
        *fUniqueKeyInvalidatedMessages.append() =
                new GrUniqueKeyInvalidatedMessage(key, ctx->uniqueID());
        if (auto mippedProxy = GrCopyBaseMipMapToTextureProxy(ctx, proxy.get())) {
            set_key_on_proxy(proxyProvider, mippedProxy.get(), proxy.get(), key);
            return mippedProxy;
        }
        // We failed to make a mipped proxy with the base copied into it. This could have
        // been from failure to make the proxy or failure to do the copy. Thus we will fall
        // back to just using the non mipped proxy; See skbug.com/7094.
        return proxy;
    }

    SK_HISTOGRAM_ENUMERATION("LockTexturePath", kFailure_LockTexturePath,
                             kLockTexturePathCount);
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#endif
