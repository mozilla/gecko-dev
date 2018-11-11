/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMtlCaps_DEFINED
#define GrMtlCaps_DEFINED

#include "GrCaps.h"
#include "GrMtlStencilAttachment.h"
#include "SkTDArray.h"

#import <Metal/Metal.h>

class GrShaderCaps;

/**
 * Stores some capabilities of a Mtl backend.
 */
class GrMtlCaps : public GrCaps {
public:
    typedef GrMtlStencilAttachment::Format StencilFormat;

    GrMtlCaps(const GrContextOptions& contextOptions, id<MTLDevice> device,
              MTLFeatureSet featureSet);

    bool isConfigTexturable(GrPixelConfig config) const override {
        return SkToBool(fConfigTable[config].fFlags & ConfigInfo::kTextureable_Flag);
    }

    int getRenderTargetSampleCount(int requestedCount, GrPixelConfig) const override;
    int maxRenderTargetSampleCount(GrPixelConfig) const override;

    bool surfaceSupportsWritePixels(const GrSurface*) const override { return true; }
    bool surfaceSupportsReadPixels(const GrSurface*) const override { return true; }

    bool isConfigCopyable(GrPixelConfig config) const override {
        return true;
    }

    /**
     * Returns both a supported and most prefered stencil format to use in draws.
     */
    const StencilFormat& preferredStencilFormat() const {
        return fPreferredStencilFormat;
    }

    bool canCopyAsBlit(GrPixelConfig dstConfig, int dstSampleCount, GrSurfaceOrigin dstOrigin,
                       GrPixelConfig srcConfig, int srcSampleCount, GrSurfaceOrigin srcOrigin,
                       const SkIRect& srcRect, const SkIPoint& dstPoint,
                       bool areDstSrcSameObj) const;

    bool canCopyAsDraw(GrPixelConfig dstConfig, bool dstIsRenderable,
                       GrPixelConfig srcConfig, bool srcIsTextureable) const;

    bool canCopyAsDrawThenBlit(GrPixelConfig dstConfig, GrPixelConfig srcConfig,
                               bool srcIsTextureable) const;

    bool canCopySurface(const GrSurfaceProxy* dst, const GrSurfaceProxy* src,
                        const SkIRect& srcRect, const SkIPoint& dstPoint) const override;

    bool initDescForDstCopy(const GrRenderTargetProxy* src, GrSurfaceDesc* desc, GrSurfaceOrigin*,
                            bool* rectsMustMatch, bool* disallowSubrect) const override {
        return false;
    }

    bool validateBackendTexture(const GrBackendTexture&, SkColorType,
                                GrPixelConfig*) const override {
        return false;
    }
    bool validateBackendRenderTarget(const GrBackendRenderTarget&, SkColorType,
                                     GrPixelConfig*) const override {
        return false;
    }

    bool getConfigFromBackendFormat(const GrBackendFormat&, SkColorType,
                                    GrPixelConfig*) const override {
        return false;
    }

private:
    void initFeatureSet(MTLFeatureSet featureSet);

    void initStencilFormat(const id<MTLDevice> device);

    void initGrCaps(const id<MTLDevice> device);
    void initShaderCaps();

#ifdef GR_TEST_UTILS
    GrBackendFormat onCreateFormatFromBackendTexture(const GrBackendTexture&) const override;
#endif

    void initConfigTable();

    struct ConfigInfo {
        ConfigInfo() : fFlags(0) {}

        enum {
            kTextureable_Flag = 0x1,
            kRenderable_Flag  = 0x2, // Color attachment and blendable
            kMSAA_Flag        = 0x4,
            kResolve_Flag     = 0x8,
        };
        // TODO: Put kMSAA_Flag back when MSAA is implemented
        static const uint16_t kAllFlags = kTextureable_Flag | kRenderable_Flag |
                                          /*kMSAA_Flag |*/ kResolve_Flag;

        uint16_t fFlags;
    };
    ConfigInfo fConfigTable[kGrPixelConfigCnt];

    enum class Platform {
        kMac,
        kIOS
    };
    bool isMac() { return Platform::kMac == fPlatform; }
    bool isIOS() { return Platform::kIOS == fPlatform; }

    Platform fPlatform;
    int fFamilyGroup;
    int fVersion;

    SkTDArray<int> fSampleCounts;

    StencilFormat fPreferredStencilFormat;

    typedef GrCaps INHERITED;
};

#endif
