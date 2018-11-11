/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrTextContext.h"

#include "GrCaps.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrSDFMaskFilter.h"
#include "GrTextBlobCache.h"
#include "SkDistanceFieldGen.h"
#include "SkDraw.h"
#include "SkDrawProcs.h"
#include "SkGlyphRun.h"
#include "SkGr.h"
#include "SkGraphics.h"
#include "SkMakeUnique.h"
#include "SkMaskFilterBase.h"
#include "SkPaintPriv.h"
#include "SkTo.h"
#include "ops/GrMeshDrawOp.h"

// DF sizes and thresholds for usage of the small and medium sizes. For example, above
// kSmallDFFontLimit we will use the medium size. The large size is used up until the size at
// which we switch over to drawing as paths as controlled by Options.
static const int kSmallDFFontSize = 32;
static const int kSmallDFFontLimit = 32;
static const int kMediumDFFontSize = 72;
static const int kMediumDFFontLimit = 72;
static const int kLargeDFFontSize = 162;

static const int kDefaultMinDistanceFieldFontSize = 18;
#ifdef SK_BUILD_FOR_ANDROID
static const int kDefaultMaxDistanceFieldFontSize = 384;
#else
static const int kDefaultMaxDistanceFieldFontSize = 2 * kLargeDFFontSize;
#endif

GrTextContext::GrTextContext(const Options& options)
        : fDistanceAdjustTable(new GrDistanceFieldAdjustTable), fOptions(options) {
    SanitizeOptions(&fOptions);
}

std::unique_ptr<GrTextContext> GrTextContext::Make(const Options& options) {
    return std::unique_ptr<GrTextContext>(new GrTextContext(options));
}

SkColor GrTextContext::ComputeCanonicalColor(const SkPaint& paint, bool lcd) {
    SkColor canonicalColor = paint.computeLuminanceColor();
    if (lcd) {
        // This is the correct computation, but there are tons of cases where LCD can be overridden.
        // For now we just regenerate if any run in a textblob has LCD.
        // TODO figure out where all of these overrides are and see if we can incorporate that logic
        // at a higher level *OR* use sRGB
        SkASSERT(false);
        //canonicalColor = SkMaskGamma::CanonicalColor(canonicalColor);
    } else {
        // A8, though can have mixed BMP text but it shouldn't matter because BMP text won't have
        // gamma corrected masks anyways, nor color
        U8CPU lum = SkComputeLuminance(SkColorGetR(canonicalColor),
                                       SkColorGetG(canonicalColor),
                                       SkColorGetB(canonicalColor));
        // reduce to our finite number of bits
        canonicalColor = SkMaskGamma::CanonicalColor(SkColorSetRGB(lum, lum, lum));
    }
    return canonicalColor;
}

SkScalerContextFlags GrTextContext::ComputeScalerContextFlags(
        const GrColorSpaceInfo& colorSpaceInfo) {
    // If we're doing linear blending, then we can disable the gamma hacks.
    // Otherwise, leave them on. In either case, we still want the contrast boost:
    // TODO: Can we be even smarter about mask gamma based on the dest transfer function?
    if (colorSpaceInfo.isLinearlyBlended()) {
        return SkScalerContextFlags::kBoostContrast;
    } else {
        return SkScalerContextFlags::kFakeGammaAndBoostContrast;
    }
}

void GrTextContext::SanitizeOptions(Options* options) {
    if (options->fMaxDistanceFieldFontSize < 0.f) {
        options->fMaxDistanceFieldFontSize = kDefaultMaxDistanceFieldFontSize;
    }
    if (options->fMinDistanceFieldFontSize < 0.f) {
        options->fMinDistanceFieldFontSize = kDefaultMinDistanceFieldFontSize;
    }
}

bool GrTextContext::CanDrawAsDistanceFields(const SkPaint& skPaint, const SkMatrix& viewMatrix,
                                            const SkSurfaceProps& props,
                                            bool contextSupportsDistanceFieldText,
                                            const Options& options) {
    if (!viewMatrix.hasPerspective()) {
        SkScalar maxScale = viewMatrix.getMaxScale();
        SkScalar scaledTextSize = maxScale * skPaint.getTextSize();
        // Hinted text looks far better at small resolutions
        // Scaling up beyond 2x yields undesireable artifacts
        if (scaledTextSize < options.fMinDistanceFieldFontSize ||
            scaledTextSize > options.fMaxDistanceFieldFontSize) {
            return false;
        }

        bool useDFT = props.isUseDeviceIndependentFonts();
#if SK_FORCE_DISTANCE_FIELD_TEXT
        useDFT = true;
#endif

        if (!useDFT && scaledTextSize < kLargeDFFontSize) {
            return false;
        }
    }

    // mask filters modify alpha, which doesn't translate well to distance
    if (skPaint.getMaskFilter() || !contextSupportsDistanceFieldText) {
        return false;
    }

    // TODO: add some stroking support
    if (skPaint.getStyle() != SkPaint::kFill_Style) {
        return false;
    }

    return true;
}

void GrTextContext::InitDistanceFieldPaint(GrTextBlob* blob,
                                           SkPaint* skPaint,
                                           const SkMatrix& viewMatrix,
                                           const Options& options,
                                           SkScalar* textRatio,
                                           SkScalerContextFlags* flags) {
    SkScalar textSize = skPaint->getTextSize();
    SkScalar scaledTextSize = textSize;

    if (viewMatrix.hasPerspective()) {
        // for perspective, we simply force to the medium size
        // TODO: compute a size based on approximate screen area
        scaledTextSize = kMediumDFFontLimit;
    } else {
        SkScalar maxScale = viewMatrix.getMaxScale();
        // if we have non-unity scale, we need to choose our base text size
        // based on the SkPaint's text size multiplied by the max scale factor
        // TODO: do we need to do this if we're scaling down (i.e. maxScale < 1)?
        if (maxScale > 0 && !SkScalarNearlyEqual(maxScale, SK_Scalar1)) {
            scaledTextSize *= maxScale;
        }
    }

    // We have three sizes of distance field text, and within each size 'bucket' there is a floor
    // and ceiling.  A scale outside of this range would require regenerating the distance fields
    SkScalar dfMaskScaleFloor;
    SkScalar dfMaskScaleCeil;
    if (scaledTextSize <= kSmallDFFontLimit) {
        dfMaskScaleFloor = options.fMinDistanceFieldFontSize;
        dfMaskScaleCeil = kSmallDFFontLimit;
        *textRatio = textSize / kSmallDFFontSize;
        skPaint->setTextSize(SkIntToScalar(kSmallDFFontSize));
    } else if (scaledTextSize <= kMediumDFFontLimit) {
        dfMaskScaleFloor = kSmallDFFontLimit;
        dfMaskScaleCeil = kMediumDFFontLimit;
        *textRatio = textSize / kMediumDFFontSize;
        skPaint->setTextSize(SkIntToScalar(kMediumDFFontSize));
    } else {
        dfMaskScaleFloor = kMediumDFFontLimit;
        dfMaskScaleCeil = options.fMaxDistanceFieldFontSize;
        *textRatio = textSize / kLargeDFFontSize;
        skPaint->setTextSize(SkIntToScalar(kLargeDFFontSize));
    }

    // Because there can be multiple runs in the blob, we want the overall maxMinScale, and
    // minMaxScale to make regeneration decisions.  Specifically, we want the maximum minimum scale
    // we can tolerate before we'd drop to a lower mip size, and the minimum maximum scale we can
    // tolerate before we'd have to move to a large mip size.  When we actually test these values
    // we look at the delta in scale between the new viewmatrix and the old viewmatrix, and test
    // against these values to decide if we can reuse or not(ie, will a given scale change our mip
    // level)
    SkASSERT(dfMaskScaleFloor <= scaledTextSize && scaledTextSize <= dfMaskScaleCeil);
    if (blob) {
        blob->setMinAndMaxScale(dfMaskScaleFloor / scaledTextSize,
                                dfMaskScaleCeil / scaledTextSize);
    }

    skPaint->setAntiAlias(true);
    skPaint->setLCDRenderText(false);
    skPaint->setAutohinted(false);
    skPaint->setHinting(SkPaint::kNormal_Hinting);
    skPaint->setSubpixelText(true);

    skPaint->setMaskFilter(GrSDFMaskFilter::Make());

    // We apply the fake-gamma by altering the distance in the shader, so we ignore the
    // passed-in scaler context flags. (It's only used when we fall-back to bitmap text).
    *flags = SkScalerContextFlags::kNone;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if GR_TEST_UTILS

#include "GrRenderTargetContext.h"

GR_DRAW_OP_TEST_DEFINE(GrAtlasTextOp) {
    static uint32_t gContextID = SK_InvalidGenID;
    static std::unique_ptr<GrTextContext> gTextContext;
    static SkSurfaceProps gSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType);

    if (context->uniqueID() != gContextID) {
        gContextID = context->uniqueID();
        gTextContext = GrTextContext::Make(GrTextContext::Options());
    }

    // Setup dummy SkPaint / GrPaint / GrRenderTargetContext
    sk_sp<GrRenderTargetContext> rtc(context->contextPriv().makeDeferredRenderTargetContext(
        SkBackingFit::kApprox, 1024, 1024, kRGBA_8888_GrPixelConfig, nullptr));

    SkMatrix viewMatrix = GrTest::TestMatrixInvertible(random);

    SkPaint skPaint;
    skPaint.setColor(random->nextU());
    skPaint.setLCDRenderText(random->nextBool());
    skPaint.setAntiAlias(skPaint.isLCDRenderText() ? true : random->nextBool());
    skPaint.setSubpixelText(random->nextBool());

    const char* text = "The quick brown fox jumps over the lazy dog.";

    // create some random x/y offsets, including negative offsets
    static const int kMaxTrans = 1024;
    int xPos = (random->nextU() % 2) * 2 - 1;
    int yPos = (random->nextU() % 2) * 2 - 1;
    int xInt = (random->nextU() % kMaxTrans) * xPos;
    int yInt = (random->nextU() % kMaxTrans) * yPos;

    return gTextContext->createOp_TestingOnly(context, gTextContext.get(), rtc.get(),
                                              skPaint, viewMatrix, text, xInt, yInt);
}

#endif
