/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "src/cpu.h"
#include "src/mc.h"

decl_mc_fn(dav1d_put_8tap_regular_avx2);
decl_mc_fn(dav1d_put_8tap_regular_smooth_avx2);
decl_mc_fn(dav1d_put_8tap_regular_sharp_avx2);
decl_mc_fn(dav1d_put_8tap_smooth_avx2);
decl_mc_fn(dav1d_put_8tap_smooth_regular_avx2);
decl_mc_fn(dav1d_put_8tap_smooth_sharp_avx2);
decl_mc_fn(dav1d_put_8tap_sharp_avx2);
decl_mc_fn(dav1d_put_8tap_sharp_regular_avx2);
decl_mc_fn(dav1d_put_8tap_sharp_smooth_avx2);
decl_mc_fn(dav1d_put_bilin_avx2);

decl_mct_fn(dav1d_prep_8tap_regular_avx2);
decl_mct_fn(dav1d_prep_8tap_regular_smooth_avx2);
decl_mct_fn(dav1d_prep_8tap_regular_sharp_avx2);
decl_mct_fn(dav1d_prep_8tap_smooth_avx2);
decl_mct_fn(dav1d_prep_8tap_smooth_regular_avx2);
decl_mct_fn(dav1d_prep_8tap_smooth_sharp_avx2);
decl_mct_fn(dav1d_prep_8tap_sharp_avx2);
decl_mct_fn(dav1d_prep_8tap_sharp_regular_avx2);
decl_mct_fn(dav1d_prep_8tap_sharp_smooth_avx2);
decl_mct_fn(dav1d_prep_bilin_avx2);

decl_avg_fn(dav1d_avg_avx2);
decl_avg_fn(dav1d_avg_ssse3);
decl_w_avg_fn(dav1d_w_avg_avx2);
decl_w_avg_fn(dav1d_w_avg_ssse3);
decl_mask_fn(dav1d_mask_avx2);
decl_mask_fn(dav1d_mask_ssse3);
decl_w_mask_fn(dav1d_w_mask_420_avx2);
decl_blend_fn(dav1d_blend_avx2);
decl_blend_dir_fn(dav1d_blend_v_avx2);
decl_blend_dir_fn(dav1d_blend_h_avx2);

decl_warp8x8_fn(dav1d_warp_affine_8x8_avx2);
decl_warp8x8t_fn(dav1d_warp_affine_8x8t_avx2);

decl_emu_edge_fn(dav1d_emu_edge_avx2);

void bitfn(dav1d_mc_dsp_init_x86)(Dav1dMCDSPContext *const c) {
#define init_mc_fn(type, name, suffix) \
    c->mc[type] = dav1d_put_##name##_##suffix
#define init_mct_fn(type, name, suffix) \
    c->mct[type] = dav1d_prep_##name##_##suffix
    const unsigned flags = dav1d_get_cpu_flags();


    if(!(flags & DAV1D_X86_CPU_FLAG_SSSE3))
        return;

#if BITDEPTH == 8 && ARCH_X86_64
    c->avg = dav1d_avg_ssse3;
    c->w_avg = dav1d_w_avg_ssse3;
    c->mask = dav1d_mask_ssse3;
#endif

    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2))
        return;

#if BITDEPTH == 8 && ARCH_X86_64
    init_mc_fn (FILTER_2D_8TAP_REGULAR,        8tap_regular,        avx2);
    init_mc_fn (FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, avx2);
    init_mc_fn (FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  avx2);
    init_mc_fn (FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, avx2);
    init_mc_fn (FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         avx2);
    init_mc_fn (FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   avx2);
    init_mc_fn (FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  avx2);
    init_mc_fn (FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   avx2);
    init_mc_fn (FILTER_2D_8TAP_SHARP,          8tap_sharp,          avx2);
    init_mc_fn (FILTER_2D_BILINEAR,            bilin,               avx2);

    init_mct_fn(FILTER_2D_8TAP_REGULAR,        8tap_regular,        avx2);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth, avx2);
    init_mct_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp,  avx2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular, avx2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH,         8tap_smooth,         avx2);
    init_mct_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp,   avx2);
    init_mct_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular,  avx2);
    init_mct_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth,   avx2);
    init_mct_fn(FILTER_2D_8TAP_SHARP,          8tap_sharp,          avx2);
    init_mct_fn(FILTER_2D_BILINEAR,            bilin,               avx2);

    c->avg = dav1d_avg_avx2;
    c->w_avg = dav1d_w_avg_avx2;
    c->mask = dav1d_mask_avx2;
    c->w_mask[2] = dav1d_w_mask_420_avx2;
    c->blend = dav1d_blend_avx2;
    c->blend_v = dav1d_blend_v_avx2;
    c->blend_h = dav1d_blend_h_avx2;

    c->warp8x8  = dav1d_warp_affine_8x8_avx2;
    c->warp8x8t = dav1d_warp_affine_8x8t_avx2;

    c->emu_edge = dav1d_emu_edge_avx2;
#endif
}
