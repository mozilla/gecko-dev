/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrConfigConversionEffect.h"
#include "GrContext.h"
#include "GrTBackendEffectFactory.h"
#include "GrSimpleTextureEffect.h"
#include "gl/GrGLEffect.h"
#include "gl/GrGLShaderBuilder.h"
#include "SkMatrix.h"

class GrGLConfigConversionEffect : public GrGLEffect {
public:
    GrGLConfigConversionEffect(const GrBackendEffectFactory& factory,
                               const GrDrawEffect& drawEffect)
    : INHERITED (factory) {
        const GrConfigConversionEffect& effect = drawEffect.castEffect<GrConfigConversionEffect>();
        fSwapRedAndBlue = effect.swapsRedAndBlue();
        fPMConversion = effect.pmConversion();
    }

    virtual void emitCode(GrGLShaderBuilder* builder,
                          const GrDrawEffect&,
                          const GrEffectKey& key,
                          const char* outputColor,
                          const char* inputColor,
                          const TransformedCoordsArray& coords,
                          const TextureSamplerArray& samplers) SK_OVERRIDE {
        builder->fsCodeAppendf("\t\t%s = ", outputColor);
        builder->fsAppendTextureLookup(samplers[0], coords[0].c_str(), coords[0].type());
        builder->fsCodeAppend(";\n");
        if (GrConfigConversionEffect::kNone_PMConversion == fPMConversion) {
            SkASSERT(fSwapRedAndBlue);
            builder->fsCodeAppendf("\t%s = %s.bgra;\n", outputColor, outputColor);
        } else {
            const char* swiz = fSwapRedAndBlue ? "bgr" : "rgb";
            switch (fPMConversion) {
                case GrConfigConversionEffect::kMulByAlpha_RoundUp_PMConversion:
                    builder->fsCodeAppendf(
                        "\t\t%s = vec4(ceil(%s.%s * %s.a * 255.0) / 255.0, %s.a);\n",
                        outputColor, outputColor, swiz, outputColor, outputColor);
                    break;
                case GrConfigConversionEffect::kMulByAlpha_RoundDown_PMConversion:
                    // Add a compensation(0.001) here to avoid the side effect of the floor operation.
                    // In Intel GPUs, the integer value converted from floor(%s.r * 255.0) / 255.0
                    // is less than the integer value converted from  %s.r by 1 when the %s.r is
                    // converted from the integer value 2^n, such as 1, 2, 4, 8, etc.
                    builder->fsCodeAppendf(
                        "\t\t%s = vec4(floor(%s.%s * %s.a * 255.0 + 0.001) / 255.0, %s.a);\n",
                        outputColor, outputColor, swiz, outputColor, outputColor);
                    break;
                case GrConfigConversionEffect::kDivByAlpha_RoundUp_PMConversion:
                    builder->fsCodeAppendf("\t\t%s = %s.a <= 0.0 ? vec4(0,0,0,0) : vec4(ceil(%s.%s / %s.a * 255.0) / 255.0, %s.a);\n",
                        outputColor, outputColor, outputColor, swiz, outputColor, outputColor);
                    break;
                case GrConfigConversionEffect::kDivByAlpha_RoundDown_PMConversion:
                    builder->fsCodeAppendf("\t\t%s = %s.a <= 0.0 ? vec4(0,0,0,0) : vec4(floor(%s.%s / %s.a * 255.0) / 255.0, %s.a);\n",
                        outputColor, outputColor, outputColor, swiz, outputColor, outputColor);
                    break;
                default:
                    SkFAIL("Unknown conversion op.");
                    break;
            }
        }
        SkString modulate;
        GrGLSLMulVarBy4f(&modulate, 2, outputColor, inputColor);
        builder->fsCodeAppend(modulate.c_str());
    }

    static inline void GenKey(const GrDrawEffect& drawEffect, const GrGLCaps&,
                              GrEffectKeyBuilder* b) {
        const GrConfigConversionEffect& conv = drawEffect.castEffect<GrConfigConversionEffect>();
        uint32_t key = (conv.swapsRedAndBlue() ? 0 : 1) | (conv.pmConversion() << 1);
        b->add32(key);
    }

private:
    bool                                    fSwapRedAndBlue;
    GrConfigConversionEffect::PMConversion  fPMConversion;

    typedef GrGLEffect INHERITED;

};

///////////////////////////////////////////////////////////////////////////////

GrConfigConversionEffect::GrConfigConversionEffect(GrTexture* texture,
                                                   bool swapRedAndBlue,
                                                   PMConversion pmConversion,
                                                   const SkMatrix& matrix)
    : GrSingleTextureEffect(texture, matrix)
    , fSwapRedAndBlue(swapRedAndBlue)
    , fPMConversion(pmConversion) {
    SkASSERT(kRGBA_8888_GrPixelConfig == texture->config() ||
             kBGRA_8888_GrPixelConfig == texture->config());
    // Why did we pollute our texture cache instead of using a GrSingleTextureEffect?
    SkASSERT(swapRedAndBlue || kNone_PMConversion != pmConversion);
}

const GrBackendEffectFactory& GrConfigConversionEffect::getFactory() const {
    return GrTBackendEffectFactory<GrConfigConversionEffect>::getInstance();
}

bool GrConfigConversionEffect::onIsEqual(const GrEffect& s) const {
    const GrConfigConversionEffect& other = CastEffect<GrConfigConversionEffect>(s);
    return this->texture(0) == s.texture(0) &&
           other.fSwapRedAndBlue == fSwapRedAndBlue &&
           other.fPMConversion == fPMConversion;
}

void GrConfigConversionEffect::getConstantColorComponents(GrColor* color,
                                                          uint32_t* validFlags) const {
    this->updateConstantColorComponentsForModulation(color, validFlags);
}

///////////////////////////////////////////////////////////////////////////////

GR_DEFINE_EFFECT_TEST(GrConfigConversionEffect);

GrEffect* GrConfigConversionEffect::TestCreate(SkRandom* random,
                                               GrContext*,
                                               const GrDrawTargetCaps&,
                                               GrTexture* textures[]) {
    PMConversion pmConv = static_cast<PMConversion>(random->nextULessThan(kPMConversionCnt));
    bool swapRB;
    if (kNone_PMConversion == pmConv) {
        swapRB = true;
    } else {
        swapRB = random->nextBool();
    }
    return SkNEW_ARGS(GrConfigConversionEffect,
                                      (textures[GrEffectUnitTest::kSkiaPMTextureIdx],
                                       swapRB,
                                       pmConv,
                                       GrEffectUnitTest::TestMatrix(random)));
}

///////////////////////////////////////////////////////////////////////////////
void GrConfigConversionEffect::TestForPreservingPMConversions(GrContext* context,
                                                              PMConversion* pmToUPMRule,
                                                              PMConversion* upmToPMRule) {
    *pmToUPMRule = kNone_PMConversion;
    *upmToPMRule = kNone_PMConversion;
    SkAutoTMalloc<uint32_t> data(256 * 256 * 3);
    uint32_t* srcData = data.get();
    uint32_t* firstRead = data.get() + 256 * 256;
    uint32_t* secondRead = data.get() + 2 * 256 * 256;

    // Fill with every possible premultiplied A, color channel value. There will be 256-y duplicate
    // values in row y. We set r,g, and b to the same value since they are handled identically.
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            uint8_t* color = reinterpret_cast<uint8_t*>(&srcData[256*y + x]);
            color[3] = y;
            color[2] = SkTMin(x, y);
            color[1] = SkTMin(x, y);
            color[0] = SkTMin(x, y);
        }
    }

    GrTextureDesc desc;
    desc.fFlags = kRenderTarget_GrTextureFlagBit |
                  kNoStencil_GrTextureFlagBit;
    desc.fWidth = 256;
    desc.fHeight = 256;
    desc.fConfig = kRGBA_8888_GrPixelConfig;

    SkAutoTUnref<GrTexture> readTex(context->createUncachedTexture(desc, NULL, 0));
    if (!readTex.get()) {
        return;
    }
    SkAutoTUnref<GrTexture> tempTex(context->createUncachedTexture(desc, NULL, 0));
    if (!tempTex.get()) {
        return;
    }
    desc.fFlags = kNone_GrTextureFlags;
    SkAutoTUnref<GrTexture> dataTex(context->createUncachedTexture(desc, data, 0));
    if (!dataTex.get()) {
        return;
    }

    static const PMConversion kConversionRules[][2] = {
        {kDivByAlpha_RoundDown_PMConversion, kMulByAlpha_RoundUp_PMConversion},
        {kDivByAlpha_RoundUp_PMConversion, kMulByAlpha_RoundDown_PMConversion},
    };

    GrContext::AutoWideOpenIdentityDraw awoid(context, NULL);

    bool failed = true;

    for (size_t i = 0; i < SK_ARRAY_COUNT(kConversionRules) && failed; ++i) {
        *pmToUPMRule = kConversionRules[i][0];
        *upmToPMRule = kConversionRules[i][1];

        static const SkRect kDstRect = SkRect::MakeWH(SkIntToScalar(256), SkIntToScalar(256));
        static const SkRect kSrcRect = SkRect::MakeWH(SK_Scalar1, SK_Scalar1);
        // We do a PM->UPM draw from dataTex to readTex and read the data. Then we do a UPM->PM draw
        // from readTex to tempTex followed by a PM->UPM draw to readTex and finally read the data.
        // We then verify that two reads produced the same values.

        SkAutoTUnref<GrEffect> pmToUPM1(SkNEW_ARGS(GrConfigConversionEffect, (dataTex,
                                                                              false,
                                                                              *pmToUPMRule,
                                                                              SkMatrix::I())));
        SkAutoTUnref<GrEffect> upmToPM(SkNEW_ARGS(GrConfigConversionEffect, (readTex,
                                                                             false,
                                                                             *upmToPMRule,
                                                                             SkMatrix::I())));
        SkAutoTUnref<GrEffect> pmToUPM2(SkNEW_ARGS(GrConfigConversionEffect, (tempTex,
                                                                              false,
                                                                              *pmToUPMRule,
                                                                              SkMatrix::I())));

        context->setRenderTarget(readTex->asRenderTarget());
        GrPaint paint1;
        paint1.addColorEffect(pmToUPM1);
        context->drawRectToRect(paint1, kDstRect, kSrcRect);

        readTex->readPixels(0, 0, 256, 256, kRGBA_8888_GrPixelConfig, firstRead);

        context->setRenderTarget(tempTex->asRenderTarget());
        GrPaint paint2;
        paint2.addColorEffect(upmToPM);
        context->drawRectToRect(paint2, kDstRect, kSrcRect);
        context->setRenderTarget(readTex->asRenderTarget());

        GrPaint paint3;
        paint3.addColorEffect(pmToUPM2);
        context->drawRectToRect(paint3, kDstRect, kSrcRect);

        readTex->readPixels(0, 0, 256, 256, kRGBA_8888_GrPixelConfig, secondRead);

        failed = false;
        for (int y = 0; y < 256 && !failed; ++y) {
            for (int x = 0; x <= y; ++x) {
                if (firstRead[256 * y + x] != secondRead[256 * y + x]) {
                    failed = true;
                    break;
                }
            }
        }
    }
    if (failed) {
        *pmToUPMRule = kNone_PMConversion;
        *upmToPMRule = kNone_PMConversion;
    }
}

const GrEffect* GrConfigConversionEffect::Create(GrTexture* texture,
                                                 bool swapRedAndBlue,
                                                 PMConversion pmConversion,
                                                 const SkMatrix& matrix) {
    if (!swapRedAndBlue && kNone_PMConversion == pmConversion) {
        // If we returned a GrConfigConversionEffect that was equivalent to a GrSimpleTextureEffect
        // then we may pollute our texture cache with redundant shaders. So in the case that no
        // conversions were requested we instead return a GrSimpleTextureEffect.
        return GrSimpleTextureEffect::Create(texture, matrix);
    } else {
        if (kRGBA_8888_GrPixelConfig != texture->config() &&
            kBGRA_8888_GrPixelConfig != texture->config() &&
            kNone_PMConversion != pmConversion) {
            // The PM conversions assume colors are 0..255
            return NULL;
        }
        return SkNEW_ARGS(GrConfigConversionEffect, (texture,
                                                     swapRedAndBlue,
                                                     pmConversion,
                                                     matrix));
    }
}
