/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrTextureProducer_DEFINED
#define GrTextureProducer_DEFINED

#include "GrResourceKey.h"
#include "GrSamplerState.h"
#include "SkImageInfo.h"
#include "SkNoncopyable.h"

class GrContext;
class GrFragmentProcessor;
class GrTexture;
class GrTextureProxy;
class SkColorSpace;
class SkMatrix;
struct SkRect;

/**
 * Different GPUs and API extensions have different requirements with respect to what texture
 * sampling parameters may be used with textures of various types. This class facilitates making
 * texture compatible with a given GrSamplerState. There are two immediate subclasses defined
 * below. One is a base class for sources that are inherently texture-backed (e.g. a texture-backed
 * SkImage). It supports subsetting the original texture. The other is for use cases where the
 * source can generate a texture that represents some content (e.g. cpu pixels, SkPicture, ...).
 */
class GrTextureProducer : public SkNoncopyable {
public:
    struct CopyParams {
        GrSamplerState::Filter fFilter;
        int fWidth;
        int fHeight;
    };

    enum FilterConstraint {
        kYes_FilterConstraint,
        kNo_FilterConstraint,
    };

    /**
     * Helper for creating a fragment processor to sample the texture with a given filtering mode.
     * It attempts to avoid making texture copies or using domains whenever possible.
     *
     * @param textureMatrix                    Matrix used to access the texture. It is applied to
     *                                         the local coords. The post-transformed coords should
     *                                         be in texel units (rather than normalized) with
     *                                         respect to this Producer's bounds (width()/height()).
     * @param constraintRect                   A rect that represents the area of the texture to be
     *                                         sampled. It must be contained in the Producer's
     *                                         bounds as defined by width()/height().
     * @param filterConstriant                 Indicates whether filtering is limited to
     *                                         constraintRect.
     * @param coordsLimitedToConstraintRect    Is it known that textureMatrix*localCoords is bound
     *                                         by the portion of the texture indicated by
     *                                         constraintRect (without consideration of filter
     *                                         width, just the raw coords).
     * @param filterOrNullForBicubic           If non-null indicates the filter mode. If null means
     *                                         use bicubic filtering.
     **/
    virtual std::unique_ptr<GrFragmentProcessor> createFragmentProcessor(
            const SkMatrix& textureMatrix,
            const SkRect& constraintRect,
            FilterConstraint filterConstraint,
            bool coordsLimitedToConstraintRect,
            const GrSamplerState::Filter* filterOrNullForBicubic,
            SkColorSpace* dstColorSpace) = 0;

    /**
     *  Returns a texture that is safe for use with the params.
     *
     * If the size of the returned texture does not match width()/height() then the contents of the
     * original may have been scaled to fit the texture or the original may have been copied into
     * a subrect of the copy. 'scaleAdjust' must be  applied to the normalized texture coordinates
     * in order to correct for the latter case.
     *
     * If the GrSamplerState is known to clamp and use kNearest or kBilerp filter mode then the
     * proxy will always be unscaled and nullptr can be passed for scaleAdjust. There is a weird
     * contract that if scaleAdjust is not null it must be initialized to {1, 1} before calling
     * this method. (TODO: Fix this and make this function always initialize scaleAdjust).
     *
     * Places the color space of the texture in (*proxyColorSpace).
     */
    sk_sp<GrTextureProxy> refTextureProxyForParams(const GrSamplerState&,
                                                   SkColorSpace* dstColorSpace,
                                                   sk_sp<SkColorSpace>* proxyColorSpace,
                                                   SkScalar scaleAdjust[2]);

    sk_sp<GrTextureProxy> refTextureProxyForParams(GrSamplerState::Filter filter,
                                                   SkColorSpace* dstColorSpace,
                                                   sk_sp<SkColorSpace>* proxyColorSpace,
                                                   SkScalar scaleAdjust[2]) {
        return this->refTextureProxyForParams(
                GrSamplerState(GrSamplerState::WrapMode::kClamp, filter), dstColorSpace,
                proxyColorSpace, scaleAdjust);
    }

    /**
     * Returns a texture that is safe for use with the dstColorSpace. If willNeedMips is true then
     * the returned texture is guaranteed to have allocated mip map levels. This can be a
     * performance win if future draws with the texture require mip maps.
     *
     * Places the color space of the texture in (*proxyColorSpace).
     */
    // TODO: Once we remove support for npot textures, we should add a flag for must support repeat
    // wrap mode. To support that flag now would require us to support scaleAdjust array like in
    // refTextureProxyForParams, however the current public API that uses this call does not expose
    // that array.
    sk_sp<GrTextureProxy> refTextureProxy(GrMipMapped willNeedMips,
                                          SkColorSpace* dstColorSpace,
                                          sk_sp<SkColorSpace>* proxyColorSpace);

    virtual ~GrTextureProducer() {}

    int width() const { return fWidth; }
    int height() const { return fHeight; }
    bool isAlphaOnly() const { return fIsAlphaOnly; }
    virtual SkAlphaType alphaType() const = 0;

protected:
    friend class GrTextureProducer_TestAccess;

    GrTextureProducer(GrContext* context, int width, int height, bool isAlphaOnly)
        : fContext(context)
        , fWidth(width)
        , fHeight(height)
        , fIsAlphaOnly(isAlphaOnly) {}

    /** Helper for creating a key for a copy from an original key. */
    static void MakeCopyKeyFromOrigKey(const GrUniqueKey& origKey,
                                       const CopyParams& copyParams,
                                       GrUniqueKey* copyKey) {
        SkASSERT(!copyKey->isValid());
        if (origKey.isValid()) {
            static const GrUniqueKey::Domain kDomain = GrUniqueKey::GenerateDomain();
            GrUniqueKey::Builder builder(copyKey, origKey, kDomain, 3);
            builder[0] = static_cast<uint32_t>(copyParams.fFilter);
            builder[1] = copyParams.fWidth;
            builder[2] = copyParams.fHeight;
        }
    }

    /**
    *  If we need to make a copy in order to be compatible with GrTextureParams producer is asked to
    *  return a key that identifies its original content + the CopyParms parameter. If the producer
    *  does not want to cache the stretched version (e.g. the producer is volatile), this should
    *  simply return without initializing the copyKey. If the texture generated by this producer
    *  depends on the destination color space, then that information should also be incorporated
    *  in the key.
    */
    virtual void makeCopyKey(const CopyParams&, GrUniqueKey* copyKey) = 0;

    /**
    *  If a stretched version of the texture is generated, it may be cached (assuming that
    *  makeCopyKey() returns true). In that case, the maker is notified in case it
    *  wants to note that for when the maker is destroyed.
    */
    virtual void didCacheCopy(const GrUniqueKey& copyKey, uint32_t contextUniqueID) = 0;

    enum DomainMode {
        kNoDomain_DomainMode,
        kDomain_DomainMode,
        kTightCopy_DomainMode
    };

    static sk_sp<GrTextureProxy> CopyOnGpu(GrContext*, sk_sp<GrTextureProxy> inputProxy,
                                           const CopyParams& copyParams,
                                           bool dstWillRequireMipMaps);

    static DomainMode DetermineDomainMode(const SkRect& constraintRect,
                                          FilterConstraint filterConstraint,
                                          bool coordsLimitedToConstraintRect,
                                          GrTextureProxy*,
                                          const GrSamplerState::Filter* filterModeOrNullForBicubic,
                                          SkRect* domainRect);

    static std::unique_ptr<GrFragmentProcessor> CreateFragmentProcessorForDomainAndFilter(
            sk_sp<GrTextureProxy> proxy,
            const SkMatrix& textureMatrix,
            DomainMode,
            const SkRect& domain,
            const GrSamplerState::Filter* filterOrNullForBicubic);

    GrContext* fContext;

private:
    virtual sk_sp<GrTextureProxy> onRefTextureProxyForParams(const GrSamplerState&,
                                                             SkColorSpace* dstColorSpace,
                                                             sk_sp<SkColorSpace>* proxyColorSpace,
                                                             bool willBeMipped,
                                                             SkScalar scaleAdjust[2]) = 0;

    const int   fWidth;
    const int   fHeight;
    const bool  fIsAlphaOnly;

    typedef SkNoncopyable INHERITED;
};

#endif
