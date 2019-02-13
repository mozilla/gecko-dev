/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

static const uint8_t bifilter4_coeff[8][2] = {
    {128,   0},
    {112,  16},
    { 96,  32},
    { 80,  48},
    { 64,  64},
    { 48,  80},
    { 32,  96},
    { 16, 112}
};

void vp8_bilinear_predict4x4_neon(
        unsigned char *src_ptr,
        int src_pixels_per_line,
        int xoffset,
        int yoffset,
        unsigned char *dst_ptr,
        int dst_pitch) {
    uint8x8_t d0u8, d1u8, d2u8, d3u8, d4u8, d5u8, d6u8;
    uint8x8_t d26u8, d27u8, d28u8, d29u8, d30u8;
    uint8x16_t q1u8, q2u8;
    uint16x8_t q1u16, q2u16;
    uint16x8_t q7u16, q8u16, q9u16;
    uint64x2_t q4u64, q5u64;
    uint64x1_t d12u64;
    uint32x2x2_t d0u32x2, d1u32x2, d2u32x2, d3u32x2;

    if (xoffset == 0) {  // skip_1stpass_filter
        uint32x2_t d28u32 = vdup_n_u32(0);
        uint32x2_t d29u32 = vdup_n_u32(0);
        uint32x2_t d30u32 = vdup_n_u32(0);

        d28u32 = vld1_lane_u32((const uint32_t *)src_ptr, d28u32, 0);
        src_ptr += src_pixels_per_line;
        d28u32 = vld1_lane_u32((const uint32_t *)src_ptr, d28u32, 1);
        src_ptr += src_pixels_per_line;
        d29u32 = vld1_lane_u32((const uint32_t *)src_ptr, d29u32, 0);
        src_ptr += src_pixels_per_line;
        d29u32 = vld1_lane_u32((const uint32_t *)src_ptr, d29u32, 1);
        src_ptr += src_pixels_per_line;
        d30u32 = vld1_lane_u32((const uint32_t *)src_ptr, d30u32, 0);
        d28u8 = vreinterpret_u8_u32(d28u32);
        d29u8 = vreinterpret_u8_u32(d29u32);
        d30u8 = vreinterpret_u8_u32(d30u32);
    } else {
        d2u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d3u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d4u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d5u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d6u8 = vld1_u8(src_ptr);

        q1u8 = vcombine_u8(d2u8, d3u8);
        q2u8 = vcombine_u8(d4u8, d5u8);

        d0u8 = vdup_n_u8(bifilter4_coeff[xoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[xoffset][1]);

        q4u64  = vshrq_n_u64(vreinterpretq_u64_u8(q1u8), 8);
        q5u64  = vshrq_n_u64(vreinterpretq_u64_u8(q2u8), 8);
        d12u64 = vshr_n_u64(vreinterpret_u64_u8(d6u8), 8);

        d0u32x2 = vzip_u32(vreinterpret_u32_u8(vget_low_u8(q1u8)),
                           vreinterpret_u32_u8(vget_high_u8(q1u8)));
        d1u32x2 = vzip_u32(vreinterpret_u32_u8(vget_low_u8(q2u8)),
                           vreinterpret_u32_u8(vget_high_u8(q2u8)));
        d2u32x2 = vzip_u32(vreinterpret_u32_u64(vget_low_u64(q4u64)),
                           vreinterpret_u32_u64(vget_high_u64(q4u64)));
        d3u32x2 = vzip_u32(vreinterpret_u32_u64(vget_low_u64(q5u64)),
                           vreinterpret_u32_u64(vget_high_u64(q5u64)));

        q7u16 = vmull_u8(vreinterpret_u8_u32(d0u32x2.val[0]), d0u8);
        q8u16 = vmull_u8(vreinterpret_u8_u32(d1u32x2.val[0]), d0u8);
        q9u16 = vmull_u8(d6u8, d0u8);

        q7u16 = vmlal_u8(q7u16, vreinterpret_u8_u32(d2u32x2.val[0]), d1u8);
        q8u16 = vmlal_u8(q8u16, vreinterpret_u8_u32(d3u32x2.val[0]), d1u8);
        q9u16 = vmlal_u8(q9u16, vreinterpret_u8_u64(d12u64), d1u8);

        d28u8 = vqrshrn_n_u16(q7u16, 7);
        d29u8 = vqrshrn_n_u16(q8u16, 7);
        d30u8 = vqrshrn_n_u16(q9u16, 7);
    }

    // secondpass_filter
    if (yoffset == 0) {  // skip_2ndpass_filter
        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d28u8), 0);
        dst_ptr += dst_pitch;
        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d28u8), 1);
        dst_ptr += dst_pitch;
        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d29u8), 0);
        dst_ptr += dst_pitch;
        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d29u8), 1);
    } else {
        d0u8 = vdup_n_u8(bifilter4_coeff[yoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[yoffset][1]);

        q1u16 = vmull_u8(d28u8, d0u8);
        q2u16 = vmull_u8(d29u8, d0u8);

        d26u8 = vext_u8(d28u8, d29u8, 4);
        d27u8 = vext_u8(d29u8, d30u8, 4);

        q1u16 = vmlal_u8(q1u16, d26u8, d1u8);
        q2u16 = vmlal_u8(q2u16, d27u8, d1u8);

        d2u8 = vqrshrn_n_u16(q1u16, 7);
        d3u8 = vqrshrn_n_u16(q2u16, 7);

        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d2u8), 0);
        dst_ptr += dst_pitch;
        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d2u8), 1);
        dst_ptr += dst_pitch;
        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d3u8), 0);
        dst_ptr += dst_pitch;
        vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_u8(d3u8), 1);
    }
    return;
}

void vp8_bilinear_predict8x4_neon(
        unsigned char *src_ptr,
        int src_pixels_per_line,
        int xoffset,
        int yoffset,
        unsigned char *dst_ptr,
        int dst_pitch) {
    uint8x8_t d0u8, d1u8, d2u8, d3u8, d4u8, d5u8;
    uint8x8_t d7u8, d9u8, d11u8, d22u8, d23u8, d24u8, d25u8, d26u8;
    uint8x16_t q1u8, q2u8, q3u8, q4u8, q5u8;
    uint16x8_t q1u16, q2u16, q3u16, q4u16;
    uint16x8_t q6u16, q7u16, q8u16, q9u16, q10u16;

    if (xoffset == 0) {  // skip_1stpass_filter
        d22u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d23u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d24u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d25u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d26u8 = vld1_u8(src_ptr);
    } else {
        q1u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q2u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q3u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q4u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q5u8 = vld1q_u8(src_ptr);

        d0u8 = vdup_n_u8(bifilter4_coeff[xoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[xoffset][1]);

        q6u16 = vmull_u8(vget_low_u8(q1u8), d0u8);
        q7u16 = vmull_u8(vget_low_u8(q2u8), d0u8);
        q8u16 = vmull_u8(vget_low_u8(q3u8), d0u8);
        q9u16 = vmull_u8(vget_low_u8(q4u8), d0u8);
        q10u16 = vmull_u8(vget_low_u8(q5u8), d0u8);

        d3u8 = vext_u8(vget_low_u8(q1u8), vget_high_u8(q1u8), 1);
        d5u8 = vext_u8(vget_low_u8(q2u8), vget_high_u8(q2u8), 1);
        d7u8 = vext_u8(vget_low_u8(q3u8), vget_high_u8(q3u8), 1);
        d9u8 = vext_u8(vget_low_u8(q4u8), vget_high_u8(q4u8), 1);
        d11u8 = vext_u8(vget_low_u8(q5u8), vget_high_u8(q5u8), 1);

        q6u16 = vmlal_u8(q6u16, d3u8, d1u8);
        q7u16 = vmlal_u8(q7u16, d5u8, d1u8);
        q8u16 = vmlal_u8(q8u16, d7u8, d1u8);
        q9u16 = vmlal_u8(q9u16, d9u8, d1u8);
        q10u16 = vmlal_u8(q10u16, d11u8, d1u8);

        d22u8 = vqrshrn_n_u16(q6u16, 7);
        d23u8 = vqrshrn_n_u16(q7u16, 7);
        d24u8 = vqrshrn_n_u16(q8u16, 7);
        d25u8 = vqrshrn_n_u16(q9u16, 7);
        d26u8 = vqrshrn_n_u16(q10u16, 7);
    }

    // secondpass_filter
    if (yoffset == 0) {  // skip_2ndpass_filter
        vst1_u8((uint8_t *)dst_ptr, d22u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d23u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d24u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d25u8);
    } else {
        d0u8 = vdup_n_u8(bifilter4_coeff[yoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[yoffset][1]);

        q1u16 = vmull_u8(d22u8, d0u8);
        q2u16 = vmull_u8(d23u8, d0u8);
        q3u16 = vmull_u8(d24u8, d0u8);
        q4u16 = vmull_u8(d25u8, d0u8);

        q1u16 = vmlal_u8(q1u16, d23u8, d1u8);
        q2u16 = vmlal_u8(q2u16, d24u8, d1u8);
        q3u16 = vmlal_u8(q3u16, d25u8, d1u8);
        q4u16 = vmlal_u8(q4u16, d26u8, d1u8);

        d2u8 = vqrshrn_n_u16(q1u16, 7);
        d3u8 = vqrshrn_n_u16(q2u16, 7);
        d4u8 = vqrshrn_n_u16(q3u16, 7);
        d5u8 = vqrshrn_n_u16(q4u16, 7);

        vst1_u8((uint8_t *)dst_ptr, d2u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d3u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d4u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d5u8);
    }
    return;
}

void vp8_bilinear_predict8x8_neon(
        unsigned char *src_ptr,
        int src_pixels_per_line,
        int xoffset,
        int yoffset,
        unsigned char *dst_ptr,
        int dst_pitch) {
    uint8x8_t d0u8, d1u8, d2u8, d3u8, d4u8, d5u8, d6u8, d7u8, d8u8, d9u8, d11u8;
    uint8x8_t d22u8, d23u8, d24u8, d25u8, d26u8, d27u8, d28u8, d29u8, d30u8;
    uint8x16_t q1u8, q2u8, q3u8, q4u8, q5u8;
    uint16x8_t q1u16, q2u16, q3u16, q4u16, q5u16;
    uint16x8_t q6u16, q7u16, q8u16, q9u16, q10u16;

    if (xoffset == 0) {  // skip_1stpass_filter
        d22u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d23u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d24u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d25u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d26u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d27u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d28u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d29u8 = vld1_u8(src_ptr); src_ptr += src_pixels_per_line;
        d30u8 = vld1_u8(src_ptr);
    } else {
        q1u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q2u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q3u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q4u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;

        d0u8 = vdup_n_u8(bifilter4_coeff[xoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[xoffset][1]);

        q6u16 = vmull_u8(vget_low_u8(q1u8), d0u8);
        q7u16 = vmull_u8(vget_low_u8(q2u8), d0u8);
        q8u16 = vmull_u8(vget_low_u8(q3u8), d0u8);
        q9u16 = vmull_u8(vget_low_u8(q4u8), d0u8);

        d3u8 = vext_u8(vget_low_u8(q1u8), vget_high_u8(q1u8), 1);
        d5u8 = vext_u8(vget_low_u8(q2u8), vget_high_u8(q2u8), 1);
        d7u8 = vext_u8(vget_low_u8(q3u8), vget_high_u8(q3u8), 1);
        d9u8 = vext_u8(vget_low_u8(q4u8), vget_high_u8(q4u8), 1);

        q6u16 = vmlal_u8(q6u16, d3u8, d1u8);
        q7u16 = vmlal_u8(q7u16, d5u8, d1u8);
        q8u16 = vmlal_u8(q8u16, d7u8, d1u8);
        q9u16 = vmlal_u8(q9u16, d9u8, d1u8);

        d22u8 = vqrshrn_n_u16(q6u16, 7);
        d23u8 = vqrshrn_n_u16(q7u16, 7);
        d24u8 = vqrshrn_n_u16(q8u16, 7);
        d25u8 = vqrshrn_n_u16(q9u16, 7);

        // first_pass filtering on the rest 5-line data
        q1u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q2u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q3u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q4u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
        q5u8 = vld1q_u8(src_ptr);

        q6u16 = vmull_u8(vget_low_u8(q1u8), d0u8);
        q7u16 = vmull_u8(vget_low_u8(q2u8), d0u8);
        q8u16 = vmull_u8(vget_low_u8(q3u8), d0u8);
        q9u16 = vmull_u8(vget_low_u8(q4u8), d0u8);
        q10u16 = vmull_u8(vget_low_u8(q5u8), d0u8);

        d3u8 = vext_u8(vget_low_u8(q1u8), vget_high_u8(q1u8), 1);
        d5u8 = vext_u8(vget_low_u8(q2u8), vget_high_u8(q2u8), 1);
        d7u8 = vext_u8(vget_low_u8(q3u8), vget_high_u8(q3u8), 1);
        d9u8 = vext_u8(vget_low_u8(q4u8), vget_high_u8(q4u8), 1);
        d11u8 = vext_u8(vget_low_u8(q5u8), vget_high_u8(q5u8), 1);

        q6u16 = vmlal_u8(q6u16, d3u8, d1u8);
        q7u16 = vmlal_u8(q7u16, d5u8, d1u8);
        q8u16 = vmlal_u8(q8u16, d7u8, d1u8);
        q9u16 = vmlal_u8(q9u16, d9u8, d1u8);
        q10u16 = vmlal_u8(q10u16, d11u8, d1u8);

        d26u8 = vqrshrn_n_u16(q6u16, 7);
        d27u8 = vqrshrn_n_u16(q7u16, 7);
        d28u8 = vqrshrn_n_u16(q8u16, 7);
        d29u8 = vqrshrn_n_u16(q9u16, 7);
        d30u8 = vqrshrn_n_u16(q10u16, 7);
    }

    // secondpass_filter
    if (yoffset == 0) {  // skip_2ndpass_filter
        vst1_u8((uint8_t *)dst_ptr, d22u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d23u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d24u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d25u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d26u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d27u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d28u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d29u8);
    } else {
        d0u8 = vdup_n_u8(bifilter4_coeff[yoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[yoffset][1]);

        q1u16 = vmull_u8(d22u8, d0u8);
        q2u16 = vmull_u8(d23u8, d0u8);
        q3u16 = vmull_u8(d24u8, d0u8);
        q4u16 = vmull_u8(d25u8, d0u8);
        q5u16 = vmull_u8(d26u8, d0u8);
        q6u16 = vmull_u8(d27u8, d0u8);
        q7u16 = vmull_u8(d28u8, d0u8);
        q8u16 = vmull_u8(d29u8, d0u8);

        q1u16 = vmlal_u8(q1u16, d23u8, d1u8);
        q2u16 = vmlal_u8(q2u16, d24u8, d1u8);
        q3u16 = vmlal_u8(q3u16, d25u8, d1u8);
        q4u16 = vmlal_u8(q4u16, d26u8, d1u8);
        q5u16 = vmlal_u8(q5u16, d27u8, d1u8);
        q6u16 = vmlal_u8(q6u16, d28u8, d1u8);
        q7u16 = vmlal_u8(q7u16, d29u8, d1u8);
        q8u16 = vmlal_u8(q8u16, d30u8, d1u8);

        d2u8 = vqrshrn_n_u16(q1u16, 7);
        d3u8 = vqrshrn_n_u16(q2u16, 7);
        d4u8 = vqrshrn_n_u16(q3u16, 7);
        d5u8 = vqrshrn_n_u16(q4u16, 7);
        d6u8 = vqrshrn_n_u16(q5u16, 7);
        d7u8 = vqrshrn_n_u16(q6u16, 7);
        d8u8 = vqrshrn_n_u16(q7u16, 7);
        d9u8 = vqrshrn_n_u16(q8u16, 7);

        vst1_u8((uint8_t *)dst_ptr, d2u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d3u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d4u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d5u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d6u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d7u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d8u8); dst_ptr += dst_pitch;
        vst1_u8((uint8_t *)dst_ptr, d9u8);
    }
    return;
}

void vp8_bilinear_predict16x16_neon(
        unsigned char *src_ptr,
        int src_pixels_per_line,
        int xoffset,
        int yoffset,
        unsigned char *dst_ptr,
        int dst_pitch) {
    int i;
    unsigned char tmp[272];
    unsigned char *tmpp;
    uint8x8_t d0u8, d1u8, d2u8, d3u8, d4u8, d5u8, d6u8, d7u8, d8u8, d9u8;
    uint8x8_t d10u8, d11u8, d12u8, d13u8, d14u8, d15u8, d16u8, d17u8, d18u8;
    uint8x8_t d19u8, d20u8, d21u8;
    uint8x16_t q1u8, q2u8, q3u8, q4u8, q5u8, q6u8, q7u8, q8u8, q9u8, q10u8;
    uint8x16_t q11u8, q12u8, q13u8, q14u8, q15u8;
    uint16x8_t q1u16, q2u16, q3u16, q4u16, q5u16, q6u16, q7u16, q8u16;
    uint16x8_t q9u16, q10u16, q11u16, q12u16, q13u16, q14u16;

    if (xoffset == 0) {  // secondpass_bfilter16x16_only
        d0u8 = vdup_n_u8(bifilter4_coeff[yoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[yoffset][1]);

        q11u8 = vld1q_u8(src_ptr);
        src_ptr += src_pixels_per_line;
        for (i = 4; i > 0; i--) {
            q12u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
            q13u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
            q14u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;
            q15u8 = vld1q_u8(src_ptr); src_ptr += src_pixels_per_line;

            q1u16 = vmull_u8(vget_low_u8(q11u8), d0u8);
            q2u16 = vmull_u8(vget_high_u8(q11u8), d0u8);
            q3u16 = vmull_u8(vget_low_u8(q12u8), d0u8);
            q4u16 = vmull_u8(vget_high_u8(q12u8), d0u8);
            q5u16 = vmull_u8(vget_low_u8(q13u8), d0u8);
            q6u16 = vmull_u8(vget_high_u8(q13u8), d0u8);
            q7u16 = vmull_u8(vget_low_u8(q14u8), d0u8);
            q8u16 = vmull_u8(vget_high_u8(q14u8), d0u8);

            q1u16 = vmlal_u8(q1u16, vget_low_u8(q12u8), d1u8);
            q2u16 = vmlal_u8(q2u16, vget_high_u8(q12u8), d1u8);
            q3u16 = vmlal_u8(q3u16, vget_low_u8(q13u8), d1u8);
            q4u16 = vmlal_u8(q4u16, vget_high_u8(q13u8), d1u8);
            q5u16 = vmlal_u8(q5u16, vget_low_u8(q14u8), d1u8);
            q6u16 = vmlal_u8(q6u16, vget_high_u8(q14u8), d1u8);
            q7u16 = vmlal_u8(q7u16, vget_low_u8(q15u8), d1u8);
            q8u16 = vmlal_u8(q8u16, vget_high_u8(q15u8), d1u8);

            d2u8 = vqrshrn_n_u16(q1u16, 7);
            d3u8 = vqrshrn_n_u16(q2u16, 7);
            d4u8 = vqrshrn_n_u16(q3u16, 7);
            d5u8 = vqrshrn_n_u16(q4u16, 7);
            d6u8 = vqrshrn_n_u16(q5u16, 7);
            d7u8 = vqrshrn_n_u16(q6u16, 7);
            d8u8 = vqrshrn_n_u16(q7u16, 7);
            d9u8 = vqrshrn_n_u16(q8u16, 7);

            q1u8 = vcombine_u8(d2u8, d3u8);
            q2u8 = vcombine_u8(d4u8, d5u8);
            q3u8 = vcombine_u8(d6u8, d7u8);
            q4u8 = vcombine_u8(d8u8, d9u8);

            q11u8 = q15u8;

            vst1q_u8((uint8_t *)dst_ptr, q1u8); dst_ptr += dst_pitch;
            vst1q_u8((uint8_t *)dst_ptr, q2u8); dst_ptr += dst_pitch;
            vst1q_u8((uint8_t *)dst_ptr, q3u8); dst_ptr += dst_pitch;
            vst1q_u8((uint8_t *)dst_ptr, q4u8); dst_ptr += dst_pitch;
        }
        return;
    }

    if (yoffset == 0) {  // firstpass_bfilter16x16_only
        d0u8 = vdup_n_u8(bifilter4_coeff[xoffset][0]);
        d1u8 = vdup_n_u8(bifilter4_coeff[xoffset][1]);

        for (i = 4; i > 0 ; i--) {
            d2u8 = vld1_u8(src_ptr);
            d3u8 = vld1_u8(src_ptr + 8);
            d4u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
            d5u8 = vld1_u8(src_ptr);
            d6u8 = vld1_u8(src_ptr + 8);
            d7u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
            d8u8 = vld1_u8(src_ptr);
            d9u8 = vld1_u8(src_ptr + 8);
            d10u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
            d11u8 = vld1_u8(src_ptr);
            d12u8 = vld1_u8(src_ptr + 8);
            d13u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;

            q7u16  = vmull_u8(d2u8, d0u8);
            q8u16  = vmull_u8(d3u8, d0u8);
            q9u16  = vmull_u8(d5u8, d0u8);
            q10u16 = vmull_u8(d6u8, d0u8);
            q11u16 = vmull_u8(d8u8, d0u8);
            q12u16 = vmull_u8(d9u8, d0u8);
            q13u16 = vmull_u8(d11u8, d0u8);
            q14u16 = vmull_u8(d12u8, d0u8);

            d2u8  = vext_u8(d2u8, d3u8, 1);
            d5u8  = vext_u8(d5u8, d6u8, 1);
            d8u8  = vext_u8(d8u8, d9u8, 1);
            d11u8 = vext_u8(d11u8, d12u8, 1);

            q7u16  = vmlal_u8(q7u16, d2u8, d1u8);
            q9u16  = vmlal_u8(q9u16, d5u8, d1u8);
            q11u16 = vmlal_u8(q11u16, d8u8, d1u8);
            q13u16 = vmlal_u8(q13u16, d11u8, d1u8);

            d3u8  = vext_u8(d3u8, d4u8, 1);
            d6u8  = vext_u8(d6u8, d7u8, 1);
            d9u8  = vext_u8(d9u8, d10u8, 1);
            d12u8 = vext_u8(d12u8, d13u8, 1);

            q8u16  = vmlal_u8(q8u16,  d3u8, d1u8);
            q10u16 = vmlal_u8(q10u16, d6u8, d1u8);
            q12u16 = vmlal_u8(q12u16, d9u8, d1u8);
            q14u16 = vmlal_u8(q14u16, d12u8, d1u8);

            d14u8 = vqrshrn_n_u16(q7u16, 7);
            d15u8 = vqrshrn_n_u16(q8u16, 7);
            d16u8 = vqrshrn_n_u16(q9u16, 7);
            d17u8 = vqrshrn_n_u16(q10u16, 7);
            d18u8 = vqrshrn_n_u16(q11u16, 7);
            d19u8 = vqrshrn_n_u16(q12u16, 7);
            d20u8 = vqrshrn_n_u16(q13u16, 7);
            d21u8 = vqrshrn_n_u16(q14u16, 7);

            q7u8 = vcombine_u8(d14u8, d15u8);
            q8u8 = vcombine_u8(d16u8, d17u8);
            q9u8 = vcombine_u8(d18u8, d19u8);
            q10u8 =vcombine_u8(d20u8, d21u8);

            vst1q_u8((uint8_t *)dst_ptr, q7u8); dst_ptr += dst_pitch;
            vst1q_u8((uint8_t *)dst_ptr, q8u8); dst_ptr += dst_pitch;
            vst1q_u8((uint8_t *)dst_ptr, q9u8); dst_ptr += dst_pitch;
            vst1q_u8((uint8_t *)dst_ptr, q10u8); dst_ptr += dst_pitch;
        }
        return;
    }

    d0u8 = vdup_n_u8(bifilter4_coeff[xoffset][0]);
    d1u8 = vdup_n_u8(bifilter4_coeff[xoffset][1]);

    d2u8 = vld1_u8(src_ptr);
    d3u8 = vld1_u8(src_ptr + 8);
    d4u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
    d5u8 = vld1_u8(src_ptr);
    d6u8 = vld1_u8(src_ptr + 8);
    d7u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
    d8u8 = vld1_u8(src_ptr);
    d9u8 = vld1_u8(src_ptr + 8);
    d10u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
    d11u8 = vld1_u8(src_ptr);
    d12u8 = vld1_u8(src_ptr + 8);
    d13u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;

    // First Pass: output_height lines x output_width columns (17x16)
    tmpp = tmp;
    for (i = 3; i > 0; i--) {
        q7u16  = vmull_u8(d2u8, d0u8);
        q8u16  = vmull_u8(d3u8, d0u8);
        q9u16  = vmull_u8(d5u8, d0u8);
        q10u16 = vmull_u8(d6u8, d0u8);
        q11u16 = vmull_u8(d8u8, d0u8);
        q12u16 = vmull_u8(d9u8, d0u8);
        q13u16 = vmull_u8(d11u8, d0u8);
        q14u16 = vmull_u8(d12u8, d0u8);

        d2u8  = vext_u8(d2u8, d3u8, 1);
        d5u8  = vext_u8(d5u8, d6u8, 1);
        d8u8  = vext_u8(d8u8, d9u8, 1);
        d11u8 = vext_u8(d11u8, d12u8, 1);

        q7u16  = vmlal_u8(q7u16, d2u8, d1u8);
        q9u16  = vmlal_u8(q9u16, d5u8, d1u8);
        q11u16 = vmlal_u8(q11u16, d8u8, d1u8);
        q13u16 = vmlal_u8(q13u16, d11u8, d1u8);

        d3u8  = vext_u8(d3u8, d4u8, 1);
        d6u8  = vext_u8(d6u8, d7u8, 1);
        d9u8  = vext_u8(d9u8, d10u8, 1);
        d12u8 = vext_u8(d12u8, d13u8, 1);

        q8u16  = vmlal_u8(q8u16,  d3u8, d1u8);
        q10u16 = vmlal_u8(q10u16, d6u8, d1u8);
        q12u16 = vmlal_u8(q12u16, d9u8, d1u8);
        q14u16 = vmlal_u8(q14u16, d12u8, d1u8);

        d14u8 = vqrshrn_n_u16(q7u16, 7);
        d15u8 = vqrshrn_n_u16(q8u16, 7);
        d16u8 = vqrshrn_n_u16(q9u16, 7);
        d17u8 = vqrshrn_n_u16(q10u16, 7);
        d18u8 = vqrshrn_n_u16(q11u16, 7);
        d19u8 = vqrshrn_n_u16(q12u16, 7);
        d20u8 = vqrshrn_n_u16(q13u16, 7);
        d21u8 = vqrshrn_n_u16(q14u16, 7);

        d2u8 = vld1_u8(src_ptr);
        d3u8 = vld1_u8(src_ptr + 8);
        d4u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
        d5u8 = vld1_u8(src_ptr);
        d6u8 = vld1_u8(src_ptr + 8);
        d7u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
        d8u8 = vld1_u8(src_ptr);
        d9u8 = vld1_u8(src_ptr + 8);
        d10u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;
        d11u8 = vld1_u8(src_ptr);
        d12u8 = vld1_u8(src_ptr + 8);
        d13u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;

        q7u8 = vcombine_u8(d14u8, d15u8);
        q8u8 = vcombine_u8(d16u8, d17u8);
        q9u8 = vcombine_u8(d18u8, d19u8);
        q10u8 = vcombine_u8(d20u8, d21u8);

        vst1q_u8((uint8_t *)tmpp, q7u8); tmpp += 16;
        vst1q_u8((uint8_t *)tmpp, q8u8); tmpp += 16;
        vst1q_u8((uint8_t *)tmpp, q9u8); tmpp += 16;
        vst1q_u8((uint8_t *)tmpp, q10u8); tmpp += 16;
    }

    // First-pass filtering for rest 5 lines
    d14u8 = vld1_u8(src_ptr);
    d15u8 = vld1_u8(src_ptr + 8);
    d16u8 = vld1_u8(src_ptr + 16); src_ptr += src_pixels_per_line;

    q9u16  = vmull_u8(d2u8, d0u8);
    q10u16 = vmull_u8(d3u8, d0u8);
    q11u16 = vmull_u8(d5u8, d0u8);
    q12u16 = vmull_u8(d6u8, d0u8);
    q13u16 = vmull_u8(d8u8, d0u8);
    q14u16 = vmull_u8(d9u8, d0u8);

    d2u8  = vext_u8(d2u8, d3u8, 1);
    d5u8  = vext_u8(d5u8, d6u8, 1);
    d8u8  = vext_u8(d8u8, d9u8, 1);

    q9u16  = vmlal_u8(q9u16, d2u8, d1u8);
    q11u16 = vmlal_u8(q11u16, d5u8, d1u8);
    q13u16 = vmlal_u8(q13u16, d8u8, d1u8);

    d3u8  = vext_u8(d3u8, d4u8, 1);
    d6u8  = vext_u8(d6u8, d7u8, 1);
    d9u8  = vext_u8(d9u8, d10u8, 1);

    q10u16 = vmlal_u8(q10u16, d3u8, d1u8);
    q12u16 = vmlal_u8(q12u16, d6u8, d1u8);
    q14u16 = vmlal_u8(q14u16, d9u8, d1u8);

    q1u16 = vmull_u8(d11u8, d0u8);
    q2u16 = vmull_u8(d12u8, d0u8);
    q3u16 = vmull_u8(d14u8, d0u8);
    q4u16 = vmull_u8(d15u8, d0u8);

    d11u8 = vext_u8(d11u8, d12u8, 1);
    d14u8 = vext_u8(d14u8, d15u8, 1);

    q1u16 = vmlal_u8(q1u16, d11u8, d1u8);
    q3u16 = vmlal_u8(q3u16, d14u8, d1u8);

    d12u8 = vext_u8(d12u8, d13u8, 1);
    d15u8 = vext_u8(d15u8, d16u8, 1);

    q2u16 = vmlal_u8(q2u16, d12u8, d1u8);
    q4u16 = vmlal_u8(q4u16, d15u8, d1u8);

    d10u8 = vqrshrn_n_u16(q9u16, 7);
    d11u8 = vqrshrn_n_u16(q10u16, 7);
    d12u8 = vqrshrn_n_u16(q11u16, 7);
    d13u8 = vqrshrn_n_u16(q12u16, 7);
    d14u8 = vqrshrn_n_u16(q13u16, 7);
    d15u8 = vqrshrn_n_u16(q14u16, 7);
    d16u8 = vqrshrn_n_u16(q1u16, 7);
    d17u8 = vqrshrn_n_u16(q2u16, 7);
    d18u8 = vqrshrn_n_u16(q3u16, 7);
    d19u8 = vqrshrn_n_u16(q4u16, 7);

    q5u8 = vcombine_u8(d10u8, d11u8);
    q6u8 = vcombine_u8(d12u8, d13u8);
    q7u8 = vcombine_u8(d14u8, d15u8);
    q8u8 = vcombine_u8(d16u8, d17u8);
    q9u8 = vcombine_u8(d18u8, d19u8);

    vst1q_u8((uint8_t *)tmpp, q5u8); tmpp += 16;
    vst1q_u8((uint8_t *)tmpp, q6u8); tmpp += 16;
    vst1q_u8((uint8_t *)tmpp, q7u8); tmpp += 16;
    vst1q_u8((uint8_t *)tmpp, q8u8); tmpp += 16;
    vst1q_u8((uint8_t *)tmpp, q9u8);

    // secondpass_filter
    d0u8 = vdup_n_u8(bifilter4_coeff[yoffset][0]);
    d1u8 = vdup_n_u8(bifilter4_coeff[yoffset][1]);

    tmpp = tmp;
    q11u8 = vld1q_u8(tmpp);
    tmpp += 16;
    for (i = 4; i > 0; i--) {
        q12u8 = vld1q_u8(tmpp); tmpp += 16;
        q13u8 = vld1q_u8(tmpp); tmpp += 16;
        q14u8 = vld1q_u8(tmpp); tmpp += 16;
        q15u8 = vld1q_u8(tmpp); tmpp += 16;

        q1u16 = vmull_u8(vget_low_u8(q11u8), d0u8);
        q2u16 = vmull_u8(vget_high_u8(q11u8), d0u8);
        q3u16 = vmull_u8(vget_low_u8(q12u8), d0u8);
        q4u16 = vmull_u8(vget_high_u8(q12u8), d0u8);
        q5u16 = vmull_u8(vget_low_u8(q13u8), d0u8);
        q6u16 = vmull_u8(vget_high_u8(q13u8), d0u8);
        q7u16 = vmull_u8(vget_low_u8(q14u8), d0u8);
        q8u16 = vmull_u8(vget_high_u8(q14u8), d0u8);

        q1u16 = vmlal_u8(q1u16, vget_low_u8(q12u8), d1u8);
        q2u16 = vmlal_u8(q2u16, vget_high_u8(q12u8), d1u8);
        q3u16 = vmlal_u8(q3u16, vget_low_u8(q13u8), d1u8);
        q4u16 = vmlal_u8(q4u16, vget_high_u8(q13u8), d1u8);
        q5u16 = vmlal_u8(q5u16, vget_low_u8(q14u8), d1u8);
        q6u16 = vmlal_u8(q6u16, vget_high_u8(q14u8), d1u8);
        q7u16 = vmlal_u8(q7u16, vget_low_u8(q15u8), d1u8);
        q8u16 = vmlal_u8(q8u16, vget_high_u8(q15u8), d1u8);

        d2u8 = vqrshrn_n_u16(q1u16, 7);
        d3u8 = vqrshrn_n_u16(q2u16, 7);
        d4u8 = vqrshrn_n_u16(q3u16, 7);
        d5u8 = vqrshrn_n_u16(q4u16, 7);
        d6u8 = vqrshrn_n_u16(q5u16, 7);
        d7u8 = vqrshrn_n_u16(q6u16, 7);
        d8u8 = vqrshrn_n_u16(q7u16, 7);
        d9u8 = vqrshrn_n_u16(q8u16, 7);

        q1u8 = vcombine_u8(d2u8, d3u8);
        q2u8 = vcombine_u8(d4u8, d5u8);
        q3u8 = vcombine_u8(d6u8, d7u8);
        q4u8 = vcombine_u8(d8u8, d9u8);

        q11u8 = q15u8;

        vst1q_u8((uint8_t *)dst_ptr, q1u8); dst_ptr += dst_pitch;
        vst1q_u8((uint8_t *)dst_ptr, q2u8); dst_ptr += dst_pitch;
        vst1q_u8((uint8_t *)dst_ptr, q3u8); dst_ptr += dst_pitch;
        vst1q_u8((uint8_t *)dst_ptr, q4u8); dst_ptr += dst_pitch;
    }
    return;
}
