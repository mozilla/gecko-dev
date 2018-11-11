/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkRasterPipeline.h"
#include "../jumper/SkJumper.h"
#include <algorithm>

SkRasterPipeline::SkRasterPipeline(SkArenaAlloc* alloc) : fAlloc(alloc) {
    this->reset();
}
void SkRasterPipeline::reset() {
    fStages      = nullptr;
    fNumStages   = 0;
    fSlotsNeeded = 1;  // We always need one extra slot for just_return().
}

void SkRasterPipeline::append(StockStage stage, void* ctx) {
    SkASSERT(stage !=           uniform_color);  // Please use append_constant_color().
    SkASSERT(stage != unbounded_uniform_color);  // Please use append_constant_color().
    SkASSERT(stage !=                 set_rgb);  // Please use append_set_rgb().
    SkASSERT(stage !=       unbounded_set_rgb);  // Please use append_set_rgb().
    SkASSERT(stage !=             clamp_gamut);  // Please use append_gamut_clamp_if_normalized().
    this->unchecked_append(stage, ctx);
}
void SkRasterPipeline::unchecked_append(StockStage stage, void* ctx) {
    fStages = fAlloc->make<StageList>( StageList{fStages, (uint64_t) stage, ctx, false} );
    fNumStages   += 1;
    fSlotsNeeded += ctx ? 2 : 1;
}
void SkRasterPipeline::append(void* fn, void* ctx) {
    fStages = fAlloc->make<StageList>( StageList{fStages, (uint64_t) fn, ctx, true} );
    fNumStages   += 1;
    fSlotsNeeded += ctx ? 2 : 1;
}

void SkRasterPipeline::extend(const SkRasterPipeline& src) {
    if (src.empty()) {
        return;
    }
    auto stages = fAlloc->makeArrayDefault<StageList>(src.fNumStages);

    int n = src.fNumStages;
    const StageList* st = src.fStages;
    while (n --> 1) {
        stages[n]      = *st;
        stages[n].prev = &stages[n-1];
        st = st->prev;
    }
    stages[0]      = *st;
    stages[0].prev = fStages;

    fStages = &stages[src.fNumStages - 1];
    fNumStages   += src.fNumStages;
    fSlotsNeeded += src.fSlotsNeeded - 1;  // Don't double count just_returns().
}

void SkRasterPipeline::dump() const {
    SkDebugf("SkRasterPipeline, %d stages\n", fNumStages);
    std::vector<const char*> stages;
    for (auto st = fStages; st; st = st->prev) {
        const char* name = "";
        switch (st->stage) {
        #define M(x) case x: name = #x; break;
            SK_RASTER_PIPELINE_STAGES(M)
        #undef M
        }
        stages.push_back(name);
    }
    std::reverse(stages.begin(), stages.end());
    for (const char* name : stages) {
        SkDebugf("\t%s\n", name);
    }
    SkDebugf("\n");
}

//#define TRACK_COLOR_HISTOGRAM
#ifdef TRACK_COLOR_HISTOGRAM
    static int gBlack;
    static int gWhite;
    static int gColor;
    #define INC_BLACK   gBlack++
    #define INC_WHITE   gWhite++
    #define INC_COLOR   gColor++
#else
    #define INC_BLACK
    #define INC_WHITE
    #define INC_COLOR
#endif

void SkRasterPipeline::append_set_rgb(SkArenaAlloc* alloc, const float rgb[3]) {
    auto arg = alloc->makeArrayDefault<float>(3);
    arg[0] = rgb[0];
    arg[1] = rgb[1];
    arg[2] = rgb[2];

    auto stage = unbounded_set_rgb;
    if (0 <= rgb[0] && rgb[0] <= 1 &&
        0 <= rgb[1] && rgb[1] <= 1 &&
        0 <= rgb[2] && rgb[2] <= 1)
    {
        stage = set_rgb;
    }

    this->unchecked_append(stage, arg);
}

void SkRasterPipeline::append_constant_color(SkArenaAlloc* alloc, const float rgba[4]) {
    // r,g,b might be outside [0,1], but alpha should probably always be in [0,1].
    SkASSERT(0 <= rgba[3] && rgba[3] <= 1);

    if (rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 1) {
        this->append(black_color);
        INC_BLACK;
    } else if (rgba[0] == 1 && rgba[1] == 1 && rgba[2] == 1 && rgba[3] == 1) {
        this->append(white_color);
        INC_WHITE;
    } else {
        auto ctx = alloc->make<SkJumper_UniformColorCtx>();
        Sk4f color = Sk4f::Load(rgba);
        color.store(&ctx->r);

        // uniform_color requires colors in range and can go lowp,
        // while unbounded_uniform_color supports out-of-range colors too but not lowp.
        if (0 <= rgba[0] && rgba[0] <= rgba[3] &&
            0 <= rgba[1] && rgba[1] <= rgba[3] &&
            0 <= rgba[2] && rgba[2] <= rgba[3]) {
            // To make loads more direct, we store 8-bit values in 16-bit slots.
            color = color * 255.0f + 0.5f;
            ctx->rgba[0] = (uint16_t)color[0];
            ctx->rgba[1] = (uint16_t)color[1];
            ctx->rgba[2] = (uint16_t)color[2];
            ctx->rgba[3] = (uint16_t)color[3];
            this->unchecked_append(uniform_color, ctx);
        } else {
            this->unchecked_append(unbounded_uniform_color, ctx);
        }

        INC_COLOR;
    }

#ifdef TRACK_COLOR_HISTOGRAM
    SkDebugf("B=%d W=%d C=%d\n", gBlack, gWhite, gColor);
#endif
}

#undef INC_BLACK
#undef INC_WHITE
#undef INC_COLOR

//static int gCounts[5] = { 0, 0, 0, 0, 0 };

void SkRasterPipeline::append_matrix(SkArenaAlloc* alloc, const SkMatrix& matrix) {
    SkMatrix::TypeMask mt = matrix.getType();
#if 0
    if (mt > 4) mt = 4;
    gCounts[mt] += 1;
    SkDebugf("matrices: %d %d %d %d %d\n",
             gCounts[0], gCounts[1], gCounts[2], gCounts[3], gCounts[4]);
#endif

    // Based on a histogram of skps, we determined the following special cases were common, more
    // or fewer can be used if client behaviors change.

    if (mt == SkMatrix::kIdentity_Mask) {
        return;
    }
    if (mt == SkMatrix::kTranslate_Mask) {
        float* trans = alloc->makeArrayDefault<float>(2);
        trans[0] = matrix.getTranslateX();
        trans[1] = matrix.getTranslateY();
        this->append(SkRasterPipeline::matrix_translate, trans);
    } else if ((mt | (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) ==
                     (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) {
        float* scaleTrans = alloc->makeArrayDefault<float>(4);
        scaleTrans[0] = matrix.getScaleX();
        scaleTrans[1] = matrix.getScaleY();
        scaleTrans[2] = matrix.getTranslateX();
        scaleTrans[3] = matrix.getTranslateY();
        this->append(SkRasterPipeline::matrix_scale_translate, scaleTrans);
    } else {
        float* storage = alloc->makeArrayDefault<float>(9);
        if (matrix.asAffine(storage)) {
            // note: asAffine and the 2x3 stage really only need 6 entries
            this->append(SkRasterPipeline::matrix_2x3, storage);
        } else {
            matrix.get9(storage);
            this->append(SkRasterPipeline::matrix_perspective, storage);
        }
    }
}

void SkRasterPipeline::append_load(SkColorType ct, const SkJumper_MemoryCtx* ctx) {
    switch (ct) {
        case kUnknown_SkColorType: SkASSERT(false); break;

        case kGray_8_SkColorType:       this->append(load_g8,      ctx); break;
        case kAlpha_8_SkColorType:      this->append(load_a8,      ctx); break;
        case kRGB_565_SkColorType:      this->append(load_565,     ctx); break;
        case kARGB_4444_SkColorType:    this->append(load_4444,    ctx); break;
        case kBGRA_8888_SkColorType:    this->append(load_bgra,    ctx); break;
        case kRGBA_8888_SkColorType:    this->append(load_8888,    ctx); break;
        case kRGBA_1010102_SkColorType: this->append(load_1010102, ctx); break;
        case kRGBA_F16_SkColorType:     this->append(load_f16,     ctx); break;
        case kRGBA_F32_SkColorType:     this->append(load_f32,     ctx); break;

        case kRGB_888x_SkColorType:    this->append(load_8888, ctx);
                                       this->append(force_opaque);
                                       break;

        case kRGB_101010x_SkColorType: this->append(load_1010102, ctx);
                                       this->append(force_opaque);
                                       break;
    }
}

void SkRasterPipeline::append_load_dst(SkColorType ct, const SkJumper_MemoryCtx* ctx) {
    switch (ct) {
        case kUnknown_SkColorType: SkASSERT(false); break;

        case kGray_8_SkColorType:       this->append(load_g8_dst,      ctx); break;
        case kAlpha_8_SkColorType:      this->append(load_a8_dst,      ctx); break;
        case kRGB_565_SkColorType:      this->append(load_565_dst,     ctx); break;
        case kARGB_4444_SkColorType:    this->append(load_4444_dst,    ctx); break;
        case kBGRA_8888_SkColorType:    this->append(load_bgra_dst,    ctx); break;
        case kRGBA_8888_SkColorType:    this->append(load_8888_dst,    ctx); break;
        case kRGBA_1010102_SkColorType: this->append(load_1010102_dst, ctx); break;
        case kRGBA_F16_SkColorType:     this->append(load_f16_dst,     ctx); break;
        case kRGBA_F32_SkColorType:     this->append(load_f32_dst,     ctx); break;

        case kRGB_888x_SkColorType:     this->append(load_8888_dst, ctx);
                                        this->append(force_opaque_dst);
                                        break;

        case kRGB_101010x_SkColorType:  this->append(load_1010102_dst, ctx);
                                        this->append(force_opaque_dst);
                                        break;
    }
}

void SkRasterPipeline::append_store(SkColorType ct, const SkJumper_MemoryCtx* ctx) {
    switch (ct) {
        case kUnknown_SkColorType: SkASSERT(false); break;

        case kAlpha_8_SkColorType:      this->append(store_a8,      ctx); break;
        case kRGB_565_SkColorType:      this->append(store_565,     ctx); break;
        case kARGB_4444_SkColorType:    this->append(store_4444,    ctx); break;
        case kBGRA_8888_SkColorType:    this->append(store_bgra,    ctx); break;
        case kRGBA_8888_SkColorType:    this->append(store_8888,    ctx); break;
        case kRGBA_1010102_SkColorType: this->append(store_1010102, ctx); break;
        case kRGBA_F16_SkColorType:     this->append(store_f16,     ctx); break;
        case kRGBA_F32_SkColorType:     this->append(store_f32,     ctx); break;

        case kRGB_888x_SkColorType:    this->append(force_opaque);
                                       this->append(store_8888, ctx);
                                       break;

        case kRGB_101010x_SkColorType: this->append(force_opaque);
                                       this->append(store_1010102, ctx);
                                       break;

        case kGray_8_SkColorType:      this->append(luminance_to_alpha);
                                       this->append(store_a8, ctx);
                                       break;
    }
}

void SkRasterPipeline::append_gamut_clamp_if_normalized(const SkImageInfo& dstInfo) {
    if (dstInfo.colorType() != kRGBA_F16_SkColorType &&
        dstInfo.colorType() != kRGBA_F32_SkColorType &&
        dstInfo.alphaType() == kPremul_SkAlphaType)
    {
        this->unchecked_append(SkRasterPipeline::clamp_gamut, nullptr);
    }
}
