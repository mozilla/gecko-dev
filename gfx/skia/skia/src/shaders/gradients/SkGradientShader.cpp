/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <algorithm>
#include "Sk4fLinearGradient.h"
#include "SkColorSpacePriv.h"
#include "SkColorSpaceXformer.h"
#include "SkFlattenablePriv.h"
#include "SkFloatBits.h"
#include "SkGradientShaderPriv.h"
#include "SkHalf.h"
#include "SkLinearGradient.h"
#include "SkMallocPixelRef.h"
#include "SkRadialGradient.h"
#include "SkReadBuffer.h"
#include "SkSweepGradient.h"
#include "SkTwoPointConicalGradient.h"
#include "SkWriteBuffer.h"
#include "../../jumper/SkJumper.h"
#include "../../third_party/skcms/skcms.h"

enum GradientSerializationFlags {
    // Bits 29:31 used for various boolean flags
    kHasPosition_GSF    = 0x80000000,
    kHasLocalMatrix_GSF = 0x40000000,
    kHasColorSpace_GSF  = 0x20000000,

    // Bits 12:28 unused

    // Bits 8:11 for fTileMode
    kTileModeShift_GSF  = 8,
    kTileModeMask_GSF   = 0xF,

    // Bits 0:7 for fGradFlags (note that kForce4fContext_PrivateFlag is 0x80)
    kGradFlagsShift_GSF = 0,
    kGradFlagsMask_GSF  = 0xFF,
};

void SkGradientShaderBase::Descriptor::flatten(SkWriteBuffer& buffer) const {
    uint32_t flags = 0;
    if (fPos) {
        flags |= kHasPosition_GSF;
    }
    if (fLocalMatrix) {
        flags |= kHasLocalMatrix_GSF;
    }
    sk_sp<SkData> colorSpaceData = fColorSpace ? fColorSpace->serialize() : nullptr;
    if (colorSpaceData) {
        flags |= kHasColorSpace_GSF;
    }
    SkASSERT(static_cast<uint32_t>(fTileMode) <= kTileModeMask_GSF);
    flags |= (fTileMode << kTileModeShift_GSF);
    SkASSERT(fGradFlags <= kGradFlagsMask_GSF);
    flags |= (fGradFlags << kGradFlagsShift_GSF);

    buffer.writeUInt(flags);

    buffer.writeColor4fArray(fColors, fCount);
    if (colorSpaceData) {
        buffer.writeDataAsByteArray(colorSpaceData.get());
    }
    if (fPos) {
        buffer.writeScalarArray(fPos, fCount);
    }
    if (fLocalMatrix) {
        buffer.writeMatrix(*fLocalMatrix);
    }
}

template <int N, typename T, bool MEM_MOVE>
static bool validate_array(SkReadBuffer& buffer, size_t count, SkSTArray<N, T, MEM_MOVE>* array) {
    if (!buffer.validateCanReadN<T>(count)) {
        return false;
    }

    array->resize_back(count);
    return true;
}

bool SkGradientShaderBase::DescriptorScope::unflatten(SkReadBuffer& buffer) {
    // New gradient format. Includes floating point color, color space, densely packed flags
    uint32_t flags = buffer.readUInt();

    fTileMode = (SkShader::TileMode)((flags >> kTileModeShift_GSF) & kTileModeMask_GSF);
    fGradFlags = (flags >> kGradFlagsShift_GSF) & kGradFlagsMask_GSF;

    fCount = buffer.getArrayCount();

    if (!(validate_array(buffer, fCount, &fColorStorage) &&
          buffer.readColor4fArray(fColorStorage.begin(), fCount))) {
        return false;
    }
    fColors = fColorStorage.begin();

    if (SkToBool(flags & kHasColorSpace_GSF)) {
        sk_sp<SkData> data = buffer.readByteArrayAsData();
        fColorSpace = data ? SkColorSpace::Deserialize(data->data(), data->size()) : nullptr;
    } else {
        fColorSpace = nullptr;
    }
    if (SkToBool(flags & kHasPosition_GSF)) {
        if (!(validate_array(buffer, fCount, &fPosStorage) &&
              buffer.readScalarArray(fPosStorage.begin(), fCount))) {
            return false;
        }
        fPos = fPosStorage.begin();
    } else {
        fPos = nullptr;
    }
    if (SkToBool(flags & kHasLocalMatrix_GSF)) {
        fLocalMatrix = &fLocalMatrixStorage;
        buffer.readMatrix(&fLocalMatrixStorage);
    } else {
        fLocalMatrix = nullptr;
    }
    return buffer.isValid();
}

////////////////////////////////////////////////////////////////////////////////////////////

SkGradientShaderBase::SkGradientShaderBase(const Descriptor& desc, const SkMatrix& ptsToUnit)
    : INHERITED(desc.fLocalMatrix)
    , fPtsToUnit(ptsToUnit)
    , fColorSpace(desc.fColorSpace ? desc.fColorSpace : SkColorSpace::MakeSRGB())
    , fColorsAreOpaque(true)
{
    fPtsToUnit.getType();  // Precache so reads are threadsafe.
    SkASSERT(desc.fCount > 1);

    fGradFlags = static_cast<uint8_t>(desc.fGradFlags);

    SkASSERT((unsigned)desc.fTileMode < SkShader::kTileModeCount);
    fTileMode = desc.fTileMode;

    /*  Note: we let the caller skip the first and/or last position.
        i.e. pos[0] = 0.3, pos[1] = 0.7
        In these cases, we insert dummy entries to ensure that the final data
        will be bracketed by [0, 1].
        i.e. our_pos[0] = 0, our_pos[1] = 0.3, our_pos[2] = 0.7, our_pos[3] = 1

        Thus colorCount (the caller's value, and fColorCount (our value) may
        differ by up to 2. In the above example:
            colorCount = 2
            fColorCount = 4
     */
    fColorCount = desc.fCount;
    // check if we need to add in dummy start and/or end position/colors
    bool dummyFirst = false;
    bool dummyLast = false;
    if (desc.fPos) {
        dummyFirst = desc.fPos[0] != 0;
        dummyLast = desc.fPos[desc.fCount - 1] != SK_Scalar1;
        fColorCount += dummyFirst + dummyLast;
    }

    size_t storageSize = fColorCount * (sizeof(SkColor4f) + (desc.fPos ? sizeof(SkScalar) : 0));
    fOrigColors4f      = reinterpret_cast<SkColor4f*>(fStorage.reset(storageSize));
    fOrigPos           = desc.fPos ? reinterpret_cast<SkScalar*>(fOrigColors4f + fColorCount)
                                   : nullptr;

    // Now copy over the colors, adding the dummies as needed
    SkColor4f* origColors = fOrigColors4f;
    if (dummyFirst) {
        *origColors++ = desc.fColors[0];
    }
    for (int i = 0; i < desc.fCount; ++i) {
        origColors[i] = desc.fColors[i];
        fColorsAreOpaque = fColorsAreOpaque && (desc.fColors[i].fA == 1);
    }
    if (dummyLast) {
        origColors += desc.fCount;
        *origColors = desc.fColors[desc.fCount - 1];
    }

    if (desc.fPos) {
        SkScalar prev = 0;
        SkScalar* origPosPtr = fOrigPos;
        *origPosPtr++ = prev; // force the first pos to 0

        int startIndex = dummyFirst ? 0 : 1;
        int count = desc.fCount + dummyLast;

        bool uniformStops = true;
        const SkScalar uniformStep = desc.fPos[startIndex] - prev;
        for (int i = startIndex; i < count; i++) {
            // Pin the last value to 1.0, and make sure pos is monotonic.
            auto curr = (i == desc.fCount) ? 1 : SkScalarPin(desc.fPos[i], prev, 1);
            uniformStops &= SkScalarNearlyEqual(uniformStep, curr - prev);

            *origPosPtr++ = prev = curr;
        }

        // If the stops are uniform, treat them as implicit.
        if (uniformStops) {
            fOrigPos = nullptr;
        }
    }
}

SkGradientShaderBase::~SkGradientShaderBase() {}

void SkGradientShaderBase::flatten(SkWriteBuffer& buffer) const {
    Descriptor desc;
    desc.fColors = fOrigColors4f;
    desc.fColorSpace = fColorSpace;
    desc.fPos = fOrigPos;
    desc.fCount = fColorCount;
    desc.fTileMode = fTileMode;
    desc.fGradFlags = fGradFlags;

    const SkMatrix& m = this->getLocalMatrix();
    desc.fLocalMatrix = m.isIdentity() ? nullptr : &m;
    desc.flatten(buffer);
}

static void add_stop_color(SkJumper_GradientCtx* ctx, size_t stop, SkPMColor4f Fs, SkPMColor4f Bs) {
    (ctx->fs[0])[stop] = Fs.fR;
    (ctx->fs[1])[stop] = Fs.fG;
    (ctx->fs[2])[stop] = Fs.fB;
    (ctx->fs[3])[stop] = Fs.fA;
    (ctx->bs[0])[stop] = Bs.fR;
    (ctx->bs[1])[stop] = Bs.fG;
    (ctx->bs[2])[stop] = Bs.fB;
    (ctx->bs[3])[stop] = Bs.fA;
}

static void add_const_color(SkJumper_GradientCtx* ctx, size_t stop, SkPMColor4f color) {
    add_stop_color(ctx, stop, { 0, 0, 0, 0 }, color);
}

// Calculate a factor F and a bias B so that color = F*t + B when t is in range of
// the stop. Assume that the distance between stops is 1/gapCount.
static void init_stop_evenly(
    SkJumper_GradientCtx* ctx, float gapCount, size_t stop, SkPMColor4f c_l, SkPMColor4f c_r) {
    // Clankium's GCC 4.9 targeting ARMv7 is barfing when we use Sk4f math here, so go scalar...
    SkPMColor4f Fs = {
        (c_r.fR - c_l.fR) * gapCount,
        (c_r.fG - c_l.fG) * gapCount,
        (c_r.fB - c_l.fB) * gapCount,
        (c_r.fA - c_l.fA) * gapCount,
    };
    SkPMColor4f Bs = {
        c_l.fR - Fs.fR*(stop/gapCount),
        c_l.fG - Fs.fG*(stop/gapCount),
        c_l.fB - Fs.fB*(stop/gapCount),
        c_l.fA - Fs.fA*(stop/gapCount),
    };
    add_stop_color(ctx, stop, Fs, Bs);
}

// For each stop we calculate a bias B and a scale factor F, such that
// for any t between stops n and n+1, the color we want is B[n] + F[n]*t.
static void init_stop_pos(
    SkJumper_GradientCtx* ctx, size_t stop, float t_l, float t_r, SkPMColor4f c_l, SkPMColor4f c_r) {
    // See note about Clankium's old compiler in init_stop_evenly().
    SkPMColor4f Fs = {
        (c_r.fR - c_l.fR) / (t_r - t_l),
        (c_r.fG - c_l.fG) / (t_r - t_l),
        (c_r.fB - c_l.fB) / (t_r - t_l),
        (c_r.fA - c_l.fA) / (t_r - t_l),
    };
    SkPMColor4f Bs = {
        c_l.fR - Fs.fR*t_l,
        c_l.fG - Fs.fG*t_l,
        c_l.fB - Fs.fB*t_l,
        c_l.fA - Fs.fA*t_l,
    };
    ctx->ts[stop] = t_l;
    add_stop_color(ctx, stop, Fs, Bs);
}

bool SkGradientShaderBase::onAppendStages(const StageRec& rec) const {
    SkRasterPipeline* p = rec.fPipeline;
    SkArenaAlloc* alloc = rec.fAlloc;
    SkJumper_DecalTileCtx* decal_ctx = nullptr;

    SkMatrix matrix;
    if (!this->computeTotalInverse(rec.fCTM, rec.fLocalM, &matrix)) {
        return false;
    }
    matrix.postConcat(fPtsToUnit);

    SkRasterPipeline_<256> postPipeline;

    p->append(SkRasterPipeline::seed_shader);
    p->append_matrix(alloc, matrix);
    this->appendGradientStages(alloc, p, &postPipeline);

    switch(fTileMode) {
        case kMirror_TileMode: p->append(SkRasterPipeline::mirror_x_1); break;
        case kRepeat_TileMode: p->append(SkRasterPipeline::repeat_x_1); break;
        case kDecal_TileMode:
            decal_ctx = alloc->make<SkJumper_DecalTileCtx>();
            decal_ctx->limit_x = SkBits2Float(SkFloat2Bits(1.0f) + 1);
            // reuse mask + limit_x stage, or create a custom decal_1 that just stores the mask
            p->append(SkRasterPipeline::decal_x, decal_ctx);
            // fall-through to clamp
        case kClamp_TileMode:
            if (!fOrigPos) {
                // We clamp only when the stops are evenly spaced.
                // If not, there may be hard stops, and clamping ruins hard stops at 0 and/or 1.
                // In that case, we must make sure we're using the general "gradient" stage,
                // which is the only stage that will correctly handle unclamped t.
                p->append(SkRasterPipeline::clamp_x_1);
            }
            break;
    }

    const bool premulGrad = fGradFlags & SkGradientShader::kInterpolateColorsInPremul_Flag;

    // Transform all of the colors to destination color space
    SkColor4fXformer xformedColors(fOrigColors4f, fColorCount, fColorSpace.get(), rec.fDstCS);

    auto prepareColor = [premulGrad, &xformedColors](int i) {
        SkColor4f c = xformedColors.fColors[i];
        return premulGrad ? c.premul()
                          : SkPMColor4f{ c.fR, c.fG, c.fB, c.fA };
    };

    // The two-stop case with stops at 0 and 1.
    if (fColorCount == 2 && fOrigPos == nullptr) {
        const SkPMColor4f c_l = prepareColor(0),
                          c_r = prepareColor(1);

        // See F and B below.
        auto ctx = alloc->make<SkJumper_EvenlySpaced2StopGradientCtx>();
        (Sk4f::Load(c_r.vec()) - Sk4f::Load(c_l.vec())).store(ctx->f);
        (                        Sk4f::Load(c_l.vec())).store(ctx->b);
        ctx->interpolatedInPremul = premulGrad;

        p->append(SkRasterPipeline::evenly_spaced_2_stop_gradient, ctx);
    } else {
        auto* ctx = alloc->make<SkJumper_GradientCtx>();
        ctx->interpolatedInPremul = premulGrad;

        // Note: In order to handle clamps in search, the search assumes a stop conceptully placed
        // at -inf. Therefore, the max number of stops is fColorCount+1.
        for (int i = 0; i < 4; i++) {
            // Allocate at least at for the AVX2 gather from a YMM register.
            ctx->fs[i] = alloc->makeArray<float>(std::max(fColorCount+1, 8));
            ctx->bs[i] = alloc->makeArray<float>(std::max(fColorCount+1, 8));
        }

        if (fOrigPos == nullptr) {
            // Handle evenly distributed stops.

            size_t stopCount = fColorCount;
            float gapCount = stopCount - 1;

            SkPMColor4f c_l = prepareColor(0);
            for (size_t i = 0; i < stopCount - 1; i++) {
                SkPMColor4f c_r = prepareColor(i + 1);
                init_stop_evenly(ctx, gapCount, i, c_l, c_r);
                c_l = c_r;
            }
            add_const_color(ctx, stopCount - 1, c_l);

            ctx->stopCount = stopCount;
            p->append(SkRasterPipeline::evenly_spaced_gradient, ctx);
        } else {
            // Handle arbitrary stops.

            ctx->ts = alloc->makeArray<float>(fColorCount+1);

            // Remove the dummy stops inserted by SkGradientShaderBase::SkGradientShaderBase
            // because they are naturally handled by the search method.
            int firstStop;
            int lastStop;
            if (fColorCount > 2) {
                firstStop = fOrigColors4f[0] != fOrigColors4f[1] ? 0 : 1;
                lastStop = fOrigColors4f[fColorCount - 2] != fOrigColors4f[fColorCount - 1]
                           ? fColorCount - 1 : fColorCount - 2;
            } else {
                firstStop = 0;
                lastStop = 1;
            }

            size_t stopCount = 0;
            float  t_l = fOrigPos[firstStop];
            SkPMColor4f c_l = prepareColor(firstStop);
            add_const_color(ctx, stopCount++, c_l);
            // N.B. lastStop is the index of the last stop, not one after.
            for (int i = firstStop; i < lastStop; i++) {
                float  t_r = fOrigPos[i + 1];
                SkPMColor4f c_r = prepareColor(i + 1);
                SkASSERT(t_l <= t_r);
                if (t_l < t_r) {
                    init_stop_pos(ctx, stopCount, t_l, t_r, c_l, c_r);
                    stopCount += 1;
                }
                t_l = t_r;
                c_l = c_r;
            }

            ctx->ts[stopCount] = t_l;
            add_const_color(ctx, stopCount++, c_l);

            ctx->stopCount = stopCount;
            p->append(SkRasterPipeline::gradient, ctx);
        }
    }

    if (decal_ctx) {
        p->append(SkRasterPipeline::check_decal_mask, decal_ctx);
    }

    if (!premulGrad && !this->colorsAreOpaque()) {
        p->append(SkRasterPipeline::premul);
    }

    p->extend(postPipeline);

    return true;
}


bool SkGradientShaderBase::isOpaque() const {
    return fColorsAreOpaque && (this->getTileMode() != SkShader::kDecal_TileMode);
}

static unsigned rounded_divide(unsigned numer, unsigned denom) {
    return (numer + (denom >> 1)) / denom;
}

bool SkGradientShaderBase::onAsLuminanceColor(SkColor* lum) const {
    // we just compute an average color.
    // possibly we could weight this based on the proportional width for each color
    //   assuming they are not evenly distributed in the fPos array.
    int r = 0;
    int g = 0;
    int b = 0;
    const int n = fColorCount;
    // TODO: use linear colors?
    for (int i = 0; i < n; ++i) {
        SkColor c = this->getLegacyColor(i);
        r += SkColorGetR(c);
        g += SkColorGetG(c);
        b += SkColorGetB(c);
    }
    *lum = SkColorSetRGB(rounded_divide(r, n), rounded_divide(g, n), rounded_divide(b, n));
    return true;
}

SkGradientShaderBase::AutoXformColors::AutoXformColors(const SkGradientShaderBase& grad,
                                                       SkColorSpaceXformer* xformer)
    : fColors(grad.fColorCount) {
    // TODO: stay in 4f to preserve precision?

    SkAutoSTMalloc<8, SkColor> origColors(grad.fColorCount);
    for (int i = 0; i < grad.fColorCount; ++i) {
        origColors[i] = grad.getLegacyColor(i);
    }

    xformer->apply(fColors.get(), origColors.get(), grad.fColorCount);
}

SkColor4fXformer::SkColor4fXformer(const SkColor4f* colors, int colorCount,
                                   SkColorSpace* src, SkColorSpace* dst) {
    // Transform all of the colors to destination color space
    fColors = colors;

    // Treat null destinations as sRGB.
    if (!dst) {
        dst = sk_srgb_singleton();
    }

    // Treat null sources as sRGB.
    if (!src) {
        src = sk_srgb_singleton();
    }

    if (!SkColorSpace::Equals(src, dst)) {
        skcms_ICCProfile srcProfile, dstProfile;
        src->toProfile(&srcProfile);
        dst->toProfile(&dstProfile);
        fStorage.reset(colorCount);
        const skcms_PixelFormat rgba_f32 = skcms_PixelFormat_RGBA_ffff;
        const skcms_AlphaFormat unpremul = skcms_AlphaFormat_Unpremul;
        SkAssertResult(skcms_Transform(colors,           rgba_f32, unpremul, &srcProfile,
                                       fStorage.begin(), rgba_f32, unpremul, &dstProfile,
                                       colorCount));
        fColors = fStorage.begin();
    }
}

void SkGradientShaderBase::commonAsAGradient(GradientInfo* info) const {
    if (info) {
        if (info->fColorCount >= fColorCount) {
            if (info->fColors) {
                for (int i = 0; i < fColorCount; ++i) {
                    info->fColors[i] = this->getLegacyColor(i);
                }
            }
            if (info->fColorOffsets) {
                for (int i = 0; i < fColorCount; ++i) {
                    info->fColorOffsets[i] = this->getPos(i);
                }
            }
        }
        info->fColorCount = fColorCount;
        info->fTileMode = fTileMode;
        info->fGradientFlags = fGradFlags;
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Return true if these parameters are valid/legal/safe to construct a gradient
//
static bool valid_grad(const SkColor4f colors[], const SkScalar pos[], int count,
                       unsigned tileMode) {
    return nullptr != colors && count >= 1 && tileMode < (unsigned)SkShader::kTileModeCount;
}

static void desc_init(SkGradientShaderBase::Descriptor* desc,
                      const SkColor4f colors[], sk_sp<SkColorSpace> colorSpace,
                      const SkScalar pos[], int colorCount,
                      SkShader::TileMode mode, uint32_t flags, const SkMatrix* localMatrix) {
    SkASSERT(colorCount > 1);

    desc->fColors       = colors;
    desc->fColorSpace   = std::move(colorSpace);
    desc->fPos          = pos;
    desc->fCount        = colorCount;
    desc->fTileMode     = mode;
    desc->fGradFlags    = flags;
    desc->fLocalMatrix  = localMatrix;
}

// assumes colors is SkColor4f* and pos is SkScalar*
#define EXPAND_1_COLOR(count)                \
     SkColor4f tmp[2];                       \
     do {                                    \
         if (1 == count) {                   \
             tmp[0] = tmp[1] = colors[0];    \
             colors = tmp;                   \
             pos = nullptr;                  \
             count = 2;                      \
         }                                   \
     } while (0)

struct ColorStopOptimizer {
    ColorStopOptimizer(const SkColor4f* colors, const SkScalar* pos,
                       int count, SkShader::TileMode mode)
        : fColors(colors)
        , fPos(pos)
        , fCount(count) {

            if (!pos || count != 3) {
                return;
            }

            if (SkScalarNearlyEqual(pos[0], 0.0f) &&
                SkScalarNearlyEqual(pos[1], 0.0f) &&
                SkScalarNearlyEqual(pos[2], 1.0f)) {

                if (SkShader::kRepeat_TileMode == mode ||
                    SkShader::kMirror_TileMode == mode ||
                    colors[0] == colors[1]) {

                    // Ignore the leftmost color/pos.
                    fColors += 1;
                    fPos    += 1;
                    fCount   = 2;
                }
            } else if (SkScalarNearlyEqual(pos[0], 0.0f) &&
                       SkScalarNearlyEqual(pos[1], 1.0f) &&
                       SkScalarNearlyEqual(pos[2], 1.0f)) {

                if (SkShader::kRepeat_TileMode == mode ||
                    SkShader::kMirror_TileMode == mode ||
                    colors[1] == colors[2]) {

                    // Ignore the rightmost color/pos.
                    fCount  = 2;
                }
            }
    }

    const SkColor4f* fColors;
    const SkScalar*  fPos;
    int              fCount;
};

struct ColorConverter {
    ColorConverter(const SkColor* colors, int count) {
        const float ONE_OVER_255 = 1.f / 255;
        for (int i = 0; i < count; ++i) {
            fColors4f.push_back({
                SkColorGetR(colors[i]) * ONE_OVER_255,
                SkColorGetG(colors[i]) * ONE_OVER_255,
                SkColorGetB(colors[i]) * ONE_OVER_255,
                SkColorGetA(colors[i]) * ONE_OVER_255 });
        }
    }

    SkSTArray<2, SkColor4f, true> fColors4f;
};

sk_sp<SkShader> SkGradientShader::MakeLinear(const SkPoint pts[2],
                                             const SkColor colors[],
                                             const SkScalar pos[], int colorCount,
                                             SkShader::TileMode mode,
                                             uint32_t flags,
                                             const SkMatrix* localMatrix) {
    ColorConverter converter(colors, colorCount);
    return MakeLinear(pts, converter.fColors4f.begin(), nullptr, pos, colorCount, mode, flags,
                      localMatrix);
}

sk_sp<SkShader> SkGradientShader::MakeLinear(const SkPoint pts[2],
                                             const SkColor4f colors[],
                                             sk_sp<SkColorSpace> colorSpace,
                                             const SkScalar pos[], int colorCount,
                                             SkShader::TileMode mode,
                                             uint32_t flags,
                                             const SkMatrix* localMatrix) {
    if (!pts || !SkScalarIsFinite((pts[1] - pts[0]).length())) {
        return nullptr;
    }
    if (!valid_grad(colors, pos, colorCount, mode)) {
        return nullptr;
    }
    if (1 == colorCount) {
        return SkShader::MakeColorShader(colors[0], std::move(colorSpace));
    }
    if (localMatrix && !localMatrix->invert(nullptr)) {
        return nullptr;
    }

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    SkGradientShaderBase::Descriptor desc;
    desc_init(&desc, opt.fColors, std::move(colorSpace), opt.fPos, opt.fCount, mode, flags,
              localMatrix);
    return sk_make_sp<SkLinearGradient>(pts, desc);
}

sk_sp<SkShader> SkGradientShader::MakeRadial(const SkPoint& center, SkScalar radius,
                                             const SkColor colors[],
                                             const SkScalar pos[], int colorCount,
                                             SkShader::TileMode mode,
                                             uint32_t flags,
                                             const SkMatrix* localMatrix) {
    ColorConverter converter(colors, colorCount);
    return MakeRadial(center, radius, converter.fColors4f.begin(), nullptr, pos, colorCount, mode,
                      flags, localMatrix);
}

sk_sp<SkShader> SkGradientShader::MakeRadial(const SkPoint& center, SkScalar radius,
                                             const SkColor4f colors[],
                                             sk_sp<SkColorSpace> colorSpace,
                                             const SkScalar pos[], int colorCount,
                                             SkShader::TileMode mode,
                                             uint32_t flags,
                                             const SkMatrix* localMatrix) {
    if (radius <= 0) {
        return nullptr;
    }
    if (!valid_grad(colors, pos, colorCount, mode)) {
        return nullptr;
    }
    if (1 == colorCount) {
        return SkShader::MakeColorShader(colors[0], std::move(colorSpace));
    }
    if (localMatrix && !localMatrix->invert(nullptr)) {
        return nullptr;
    }

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    SkGradientShaderBase::Descriptor desc;
    desc_init(&desc, opt.fColors, std::move(colorSpace), opt.fPos, opt.fCount, mode, flags,
              localMatrix);
    return sk_make_sp<SkRadialGradient>(center, radius, desc);
}

sk_sp<SkShader> SkGradientShader::MakeTwoPointConical(const SkPoint& start,
                                                      SkScalar startRadius,
                                                      const SkPoint& end,
                                                      SkScalar endRadius,
                                                      const SkColor colors[],
                                                      const SkScalar pos[],
                                                      int colorCount,
                                                      SkShader::TileMode mode,
                                                      uint32_t flags,
                                                      const SkMatrix* localMatrix) {
    ColorConverter converter(colors, colorCount);
    return MakeTwoPointConical(start, startRadius, end, endRadius, converter.fColors4f.begin(),
                               nullptr, pos, colorCount, mode, flags, localMatrix);
}

sk_sp<SkShader> SkGradientShader::MakeTwoPointConical(const SkPoint& start,
                                                      SkScalar startRadius,
                                                      const SkPoint& end,
                                                      SkScalar endRadius,
                                                      const SkColor4f colors[],
                                                      sk_sp<SkColorSpace> colorSpace,
                                                      const SkScalar pos[],
                                                      int colorCount,
                                                      SkShader::TileMode mode,
                                                      uint32_t flags,
                                                      const SkMatrix* localMatrix) {
    if (startRadius < 0 || endRadius < 0) {
        return nullptr;
    }
    if (SkScalarNearlyZero((start - end).length()) && SkScalarNearlyZero(startRadius)) {
        // We can treat this gradient as radial, which is faster.
        return MakeRadial(start, endRadius, colors, std::move(colorSpace), pos, colorCount,
                          mode, flags, localMatrix);
    }
    if (!valid_grad(colors, pos, colorCount, mode)) {
        return nullptr;
    }
    if (startRadius == endRadius) {
        if (start == end || startRadius == 0) {
            return SkShader::MakeEmptyShader();
        }
    }
    if (localMatrix && !localMatrix->invert(nullptr)) {
        return nullptr;
    }
    EXPAND_1_COLOR(colorCount);

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    SkGradientShaderBase::Descriptor desc;
    desc_init(&desc, opt.fColors, std::move(colorSpace), opt.fPos, opt.fCount, mode, flags,
              localMatrix);
    return SkTwoPointConicalGradient::Create(start, startRadius, end, endRadius, desc);
}

sk_sp<SkShader> SkGradientShader::MakeSweep(SkScalar cx, SkScalar cy,
                                            const SkColor colors[],
                                            const SkScalar pos[],
                                            int colorCount,
                                            SkShader::TileMode mode,
                                            SkScalar startAngle,
                                            SkScalar endAngle,
                                            uint32_t flags,
                                            const SkMatrix* localMatrix) {
    ColorConverter converter(colors, colorCount);
    return MakeSweep(cx, cy, converter.fColors4f.begin(), nullptr, pos, colorCount,
                     mode, startAngle, endAngle, flags, localMatrix);
}

sk_sp<SkShader> SkGradientShader::MakeSweep(SkScalar cx, SkScalar cy,
                                            const SkColor4f colors[],
                                            sk_sp<SkColorSpace> colorSpace,
                                            const SkScalar pos[],
                                            int colorCount,
                                            SkShader::TileMode mode,
                                            SkScalar startAngle,
                                            SkScalar endAngle,
                                            uint32_t flags,
                                            const SkMatrix* localMatrix) {
    if (!valid_grad(colors, pos, colorCount, mode)) {
        return nullptr;
    }
    if (1 == colorCount) {
        return SkShader::MakeColorShader(colors[0], std::move(colorSpace));
    }
    if (!SkScalarIsFinite(startAngle) || !SkScalarIsFinite(endAngle) || startAngle >= endAngle) {
        return nullptr;
    }
    if (localMatrix && !localMatrix->invert(nullptr)) {
        return nullptr;
    }

    if (startAngle <= 0 && endAngle >= 360) {
        // If the t-range includes [0,1], then we can always use clamping (presumably faster).
        mode = SkShader::kClamp_TileMode;
    }

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    SkGradientShaderBase::Descriptor desc;
    desc_init(&desc, opt.fColors, std::move(colorSpace), opt.fPos, opt.fCount, mode, flags,
              localMatrix);

    const SkScalar t0 = startAngle / 360,
                   t1 =   endAngle / 360;

    return sk_make_sp<SkSweepGradient>(SkPoint::Make(cx, cy), t0, t1, desc);
}

SK_DEFINE_FLATTENABLE_REGISTRAR_GROUP_START(SkGradientShader)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkLinearGradient)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkRadialGradient)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkSweepGradient)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkTwoPointConicalGradient)
SK_DEFINE_FLATTENABLE_REGISTRAR_GROUP_END
