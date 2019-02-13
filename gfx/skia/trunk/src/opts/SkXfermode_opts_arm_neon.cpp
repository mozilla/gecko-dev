#include "SkXfermode.h"
#include "SkXfermode_proccoeff.h"
#include "SkColorPriv.h"

#include <arm_neon.h>
#include "SkColor_opts_neon.h"
#include "SkXfermode_opts_arm_neon.h"

#define SkAlphaMulAlpha(a, b)   SkMulDiv255Round(a, b)


////////////////////////////////////////////////////////////////////////////////
// NEONized skia functions
////////////////////////////////////////////////////////////////////////////////

static inline uint8x8_t SkAlphaMulAlpha_neon8(uint8x8_t color, uint8x8_t alpha) {
    uint16x8_t tmp;
    uint8x8_t ret;

    tmp = vmull_u8(color, alpha);
    tmp = vaddq_u16(tmp, vdupq_n_u16(128));
    tmp = vaddq_u16(tmp, vshrq_n_u16(tmp, 8));

    ret = vshrn_n_u16(tmp, 8);

    return ret;
}

static inline uint16x8_t SkAlphaMulAlpha_neon8_16(uint8x8_t color, uint8x8_t alpha) {
    uint16x8_t ret;

    ret = vmull_u8(color, alpha);
    ret = vaddq_u16(ret, vdupq_n_u16(128));
    ret = vaddq_u16(ret, vshrq_n_u16(ret, 8));

    ret = vshrq_n_u16(ret, 8);

    return ret;
}

static inline uint8x8_t SkDiv255Round_neon8_32_8(int32x4_t p1, int32x4_t p2) {
    uint16x8_t tmp;

#ifdef SK_CPU_ARM64
    tmp = vmovn_high_u32(vmovn_u32(vreinterpretq_u32_s32(p1)),
                         vreinterpretq_u32_s32(p2));
#else
    tmp = vcombine_u16(vmovn_u32(vreinterpretq_u32_s32(p1)),
                       vmovn_u32(vreinterpretq_u32_s32(p2)));
#endif

    tmp += vdupq_n_u16(128);
    tmp += vshrq_n_u16(tmp, 8);

    return vshrn_n_u16(tmp, 8);
}

static inline uint16x8_t SkDiv255Round_neon8_16_16(uint16x8_t prod) {
    prod += vdupq_n_u16(128);
    prod += vshrq_n_u16(prod, 8);

    return vshrq_n_u16(prod, 8);
}

static inline uint8x8_t clamp_div255round_simd8_32(int32x4_t val1, int32x4_t val2) {
    uint8x8_t ret;
    uint32x4_t cmp1, cmp2;
    uint16x8_t cmp16;
    uint8x8_t cmp8, cmp8_1;

    // Test if <= 0
    cmp1 = vcleq_s32(val1, vdupq_n_s32(0));
    cmp2 = vcleq_s32(val2, vdupq_n_s32(0));
#ifdef SK_CPU_ARM64
    cmp16 = vmovn_high_u32(vmovn_u32(cmp1), cmp2);
#else
    cmp16 = vcombine_u16(vmovn_u32(cmp1), vmovn_u32(cmp2));
#endif
    cmp8_1 = vmovn_u16(cmp16);

    // Init to zero
    ret = vdup_n_u8(0);

    // Test if >= 255*255
    cmp1 = vcgeq_s32(val1, vdupq_n_s32(255*255));
    cmp2 = vcgeq_s32(val2, vdupq_n_s32(255*255));
#ifdef SK_CPU_ARM64
    cmp16 = vmovn_high_u32(vmovn_u32(cmp1), cmp2);
#else
    cmp16 = vcombine_u16(vmovn_u32(cmp1), vmovn_u32(cmp2));
#endif
    cmp8 = vmovn_u16(cmp16);

    // Insert 255 where true
    ret = vbsl_u8(cmp8, vdup_n_u8(255), ret);

    // Calc SkDiv255Round
    uint8x8_t div = SkDiv255Round_neon8_32_8(val1, val2);

    // Insert where false and previous test false
    cmp8 = cmp8 | cmp8_1;
    ret = vbsl_u8(cmp8, ret, div);

    // Return the final combination
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// 1 pixel modeprocs
////////////////////////////////////////////////////////////////////////////////

//  kSrcATop_Mode,  //!< [Da, Sc * Da + (1 - Sa) * Dc]
SkPMColor srcatop_modeproc_neon(SkPMColor src, SkPMColor dst) {
    unsigned sa = SkGetPackedA32(src);
    unsigned da = SkGetPackedA32(dst);
    unsigned isa = 255 - sa;

    uint8x8_t vda, visa, vsrc, vdst;

    vda = vdup_n_u8(da);
    visa = vdup_n_u8(isa);

    uint16x8_t vsrc_wide, vdst_wide;
    vsrc_wide = vmull_u8(vda, vreinterpret_u8_u32(vdup_n_u32(src)));
    vdst_wide = vmull_u8(visa, vreinterpret_u8_u32(vdup_n_u32(dst)));

    vsrc_wide += vdupq_n_u16(128);
    vsrc_wide += vshrq_n_u16(vsrc_wide, 8);

    vdst_wide += vdupq_n_u16(128);
    vdst_wide += vshrq_n_u16(vdst_wide, 8);

    vsrc = vshrn_n_u16(vsrc_wide, 8);
    vdst = vshrn_n_u16(vdst_wide, 8);

    vsrc += vdst;
    vsrc = vset_lane_u8(da, vsrc, 3);

    return vget_lane_u32(vreinterpret_u32_u8(vsrc), 0);
}

//  kDstATop_Mode,  //!< [Sa, Sa * Dc + Sc * (1 - Da)]
SkPMColor dstatop_modeproc_neon(SkPMColor src, SkPMColor dst) {
    unsigned sa = SkGetPackedA32(src);
    unsigned da = SkGetPackedA32(dst);
    unsigned ida = 255 - da;

    uint8x8_t vsa, vida, vsrc, vdst;

    vsa = vdup_n_u8(sa);
    vida = vdup_n_u8(ida);

    uint16x8_t vsrc_wide, vdst_wide;
    vsrc_wide = vmull_u8(vida, vreinterpret_u8_u32(vdup_n_u32(src)));
    vdst_wide = vmull_u8(vsa, vreinterpret_u8_u32(vdup_n_u32(dst)));

    vsrc_wide += vdupq_n_u16(128);
    vsrc_wide += vshrq_n_u16(vsrc_wide, 8);

    vdst_wide += vdupq_n_u16(128);
    vdst_wide += vshrq_n_u16(vdst_wide, 8);

    vsrc = vshrn_n_u16(vsrc_wide, 8);
    vdst = vshrn_n_u16(vdst_wide, 8);

    vsrc += vdst;
    vsrc = vset_lane_u8(sa, vsrc, 3);

    return vget_lane_u32(vreinterpret_u32_u8(vsrc), 0);
}

//  kXor_Mode   [Sa + Da - 2 * Sa * Da, Sc * (1 - Da) + (1 - Sa) * Dc]
SkPMColor xor_modeproc_neon(SkPMColor src, SkPMColor dst) {
    unsigned sa = SkGetPackedA32(src);
    unsigned da = SkGetPackedA32(dst);
    unsigned ret_alpha = sa + da - (SkAlphaMulAlpha(sa, da) << 1);
    unsigned isa = 255 - sa;
    unsigned ida = 255 - da;

    uint8x8_t vsrc, vdst, visa, vida;
    uint16x8_t vsrc_wide, vdst_wide;

    visa = vdup_n_u8(isa);
    vida = vdup_n_u8(ida);
    vsrc = vreinterpret_u8_u32(vdup_n_u32(src));
    vdst = vreinterpret_u8_u32(vdup_n_u32(dst));

    vsrc_wide = vmull_u8(vsrc, vida);
    vdst_wide = vmull_u8(vdst, visa);

    vsrc_wide += vdupq_n_u16(128);
    vsrc_wide += vshrq_n_u16(vsrc_wide, 8);

    vdst_wide += vdupq_n_u16(128);
    vdst_wide += vshrq_n_u16(vdst_wide, 8);

    vsrc = vshrn_n_u16(vsrc_wide, 8);
    vdst = vshrn_n_u16(vdst_wide, 8);

    vsrc += vdst;

    vsrc = vset_lane_u8(ret_alpha, vsrc, 3);

    return vget_lane_u32(vreinterpret_u32_u8(vsrc), 0);
}

// kPlus_Mode
SkPMColor plus_modeproc_neon(SkPMColor src, SkPMColor dst) {
    uint8x8_t vsrc, vdst;
    vsrc = vreinterpret_u8_u32(vdup_n_u32(src));
    vdst = vreinterpret_u8_u32(vdup_n_u32(dst));
    vsrc = vqadd_u8(vsrc, vdst);

    return vget_lane_u32(vreinterpret_u32_u8(vsrc), 0);
}

// kModulate_Mode
SkPMColor modulate_modeproc_neon(SkPMColor src, SkPMColor dst) {
    uint8x8_t vsrc, vdst, vres;
    uint16x8_t vres_wide;

    vsrc = vreinterpret_u8_u32(vdup_n_u32(src));
    vdst = vreinterpret_u8_u32(vdup_n_u32(dst));

    vres_wide = vmull_u8(vsrc, vdst);

    vres_wide += vdupq_n_u16(128);
    vres_wide += vshrq_n_u16(vres_wide, 8);

    vres = vshrn_n_u16(vres_wide, 8);

    return vget_lane_u32(vreinterpret_u32_u8(vres), 0);
}

////////////////////////////////////////////////////////////////////////////////
// 8 pixels modeprocs
////////////////////////////////////////////////////////////////////////////////

uint8x8x4_t dstover_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint16x8_t src_scale;

    src_scale = vsubw_u8(vdupq_n_u16(256), dst.val[NEON_A]);

    ret.val[NEON_A] = dst.val[NEON_A] + SkAlphaMul_neon8(src.val[NEON_A], src_scale);
    ret.val[NEON_R] = dst.val[NEON_R] + SkAlphaMul_neon8(src.val[NEON_R], src_scale);
    ret.val[NEON_G] = dst.val[NEON_G] + SkAlphaMul_neon8(src.val[NEON_G], src_scale);
    ret.val[NEON_B] = dst.val[NEON_B] + SkAlphaMul_neon8(src.val[NEON_B], src_scale);

    return ret;
}

uint8x8x4_t srcin_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint16x8_t scale;

    scale = SkAlpha255To256_neon8(dst.val[NEON_A]);

    ret.val[NEON_A] = SkAlphaMul_neon8(src.val[NEON_A], scale);
    ret.val[NEON_R] = SkAlphaMul_neon8(src.val[NEON_R], scale);
    ret.val[NEON_G] = SkAlphaMul_neon8(src.val[NEON_G], scale);
    ret.val[NEON_B] = SkAlphaMul_neon8(src.val[NEON_B], scale);

    return ret;
}

uint8x8x4_t dstin_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint16x8_t scale;

    scale = SkAlpha255To256_neon8(src.val[NEON_A]);

    ret = SkAlphaMulQ_neon8(dst, scale);

    return ret;
}

uint8x8x4_t srcout_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint16x8_t scale = vsubw_u8(vdupq_n_u16(256), dst.val[NEON_A]);

    ret = SkAlphaMulQ_neon8(src, scale);

    return ret;
}

uint8x8x4_t dstout_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint16x8_t scale = vsubw_u8(vdupq_n_u16(256), src.val[NEON_A]);

    ret = SkAlphaMulQ_neon8(dst, scale);

    return ret;
}

uint8x8x4_t srcatop_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint8x8_t isa;

    isa = vsub_u8(vdup_n_u8(255), src.val[NEON_A]);

    ret.val[NEON_A] = dst.val[NEON_A];
    ret.val[NEON_R] = SkAlphaMulAlpha_neon8(src.val[NEON_R], dst.val[NEON_A])
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_R], isa);
    ret.val[NEON_G] = SkAlphaMulAlpha_neon8(src.val[NEON_G], dst.val[NEON_A])
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_G], isa);
    ret.val[NEON_B] = SkAlphaMulAlpha_neon8(src.val[NEON_B], dst.val[NEON_A])
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_B], isa);

    return ret;
}

uint8x8x4_t dstatop_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint8x8_t ida;

    ida = vsub_u8(vdup_n_u8(255), dst.val[NEON_A]);

    ret.val[NEON_A] = src.val[NEON_A];
    ret.val[NEON_R] = SkAlphaMulAlpha_neon8(src.val[NEON_R], ida)
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_R], src.val[NEON_A]);
    ret.val[NEON_G] = SkAlphaMulAlpha_neon8(src.val[NEON_G], ida)
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_G], src.val[NEON_A]);
    ret.val[NEON_B] = SkAlphaMulAlpha_neon8(src.val[NEON_B], ida)
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_B], src.val[NEON_A]);

    return ret;
}

uint8x8x4_t xor_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;
    uint8x8_t isa, ida;
    uint16x8_t tmp_wide, tmp_wide2;

    isa = vsub_u8(vdup_n_u8(255), src.val[NEON_A]);
    ida = vsub_u8(vdup_n_u8(255), dst.val[NEON_A]);

    // First calc alpha
    tmp_wide = vmovl_u8(src.val[NEON_A]);
    tmp_wide = vaddw_u8(tmp_wide, dst.val[NEON_A]);
    tmp_wide2 = vshll_n_u8(SkAlphaMulAlpha_neon8(src.val[NEON_A], dst.val[NEON_A]), 1);
    tmp_wide = vsubq_u16(tmp_wide, tmp_wide2);
    ret.val[NEON_A] = vmovn_u16(tmp_wide);

    // Then colors
    ret.val[NEON_R] = SkAlphaMulAlpha_neon8(src.val[NEON_R], ida)
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_R], isa);
    ret.val[NEON_G] = SkAlphaMulAlpha_neon8(src.val[NEON_G], ida)
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_G], isa);
    ret.val[NEON_B] = SkAlphaMulAlpha_neon8(src.val[NEON_B], ida)
                      + SkAlphaMulAlpha_neon8(dst.val[NEON_B], isa);

    return ret;
}

uint8x8x4_t plus_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = vqadd_u8(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = vqadd_u8(src.val[NEON_R], dst.val[NEON_R]);
    ret.val[NEON_G] = vqadd_u8(src.val[NEON_G], dst.val[NEON_G]);
    ret.val[NEON_B] = vqadd_u8(src.val[NEON_B], dst.val[NEON_B]);

    return ret;
}

uint8x8x4_t modulate_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = SkAlphaMulAlpha_neon8(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = SkAlphaMulAlpha_neon8(src.val[NEON_R], dst.val[NEON_R]);
    ret.val[NEON_G] = SkAlphaMulAlpha_neon8(src.val[NEON_G], dst.val[NEON_G]);
    ret.val[NEON_B] = SkAlphaMulAlpha_neon8(src.val[NEON_B], dst.val[NEON_B]);

    return ret;
}

static inline uint8x8_t srcover_color(uint8x8_t a, uint8x8_t b) {
    uint16x8_t tmp;

    tmp = vaddl_u8(a, b);
    tmp -= SkAlphaMulAlpha_neon8_16(a, b);

    return vmovn_u16(tmp);
}

uint8x8x4_t screen_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = srcover_color(src.val[NEON_R], dst.val[NEON_R]);
    ret.val[NEON_G] = srcover_color(src.val[NEON_G], dst.val[NEON_G]);
    ret.val[NEON_B] = srcover_color(src.val[NEON_B], dst.val[NEON_B]);

    return ret;
}

template <bool overlay>
static inline uint8x8_t overlay_hardlight_color(uint8x8_t sc, uint8x8_t dc,
                                               uint8x8_t sa, uint8x8_t da) {
    /*
     * In the end we're gonna use (rc + tmp) with a different rc
     * coming from an alternative.
     * The whole value (rc + tmp) can always be expressed as
     * VAL = COM - SUB in the if case
     * VAL = COM + SUB - sa*da in the else case
     *
     * with COM = 255 * (sc + dc)
     * and  SUB = sc*da + dc*sa - 2*dc*sc
     */

    // Prepare common subexpressions
    uint16x8_t const255 = vdupq_n_u16(255);
    uint16x8_t sc_plus_dc = vaddl_u8(sc, dc);
    uint16x8_t scda = vmull_u8(sc, da);
    uint16x8_t dcsa = vmull_u8(dc, sa);
    uint16x8_t sada = vmull_u8(sa, da);

    // Prepare non common subexpressions
    uint16x8_t dc2, sc2;
    uint32x4_t scdc2_1, scdc2_2;
    if (overlay) {
        dc2 = vshll_n_u8(dc, 1);
        scdc2_1 = vmull_u16(vget_low_u16(dc2), vget_low_u16(vmovl_u8(sc)));
#ifdef SK_CPU_ARM64
        scdc2_2 = vmull_high_u16(dc2, vmovl_u8(sc));
#else
        scdc2_2 = vmull_u16(vget_high_u16(dc2), vget_high_u16(vmovl_u8(sc)));
#endif
    } else {
        sc2 = vshll_n_u8(sc, 1);
        scdc2_1 = vmull_u16(vget_low_u16(sc2), vget_low_u16(vmovl_u8(dc)));
#ifdef SK_CPU_ARM64
        scdc2_2 = vmull_high_u16(sc2, vmovl_u8(dc));
#else
        scdc2_2 = vmull_u16(vget_high_u16(sc2), vget_high_u16(vmovl_u8(dc)));
#endif
    }

    // Calc COM
    int32x4_t com1, com2;
    com1 = vreinterpretq_s32_u32(
                vmull_u16(vget_low_u16(const255), vget_low_u16(sc_plus_dc)));
    com2 = vreinterpretq_s32_u32(
#ifdef SK_CPU_ARM64
                vmull_high_u16(const255, sc_plus_dc));
#else
                vmull_u16(vget_high_u16(const255), vget_high_u16(sc_plus_dc)));
#endif

    // Calc SUB
    int32x4_t sub1, sub2;
    sub1 = vreinterpretq_s32_u32(vaddl_u16(vget_low_u16(scda), vget_low_u16(dcsa)));
#ifdef SK_CPU_ARM64
    sub2 = vreinterpretq_s32_u32(vaddl_high_u16(scda, dcsa));
#else
    sub2 = vreinterpretq_s32_u32(vaddl_u16(vget_high_u16(scda), vget_high_u16(dcsa)));
#endif
    sub1 = vsubq_s32(sub1, vreinterpretq_s32_u32(scdc2_1));
    sub2 = vsubq_s32(sub2, vreinterpretq_s32_u32(scdc2_2));

    // Compare 2*dc <= da
    uint16x8_t cmp;

    if (overlay) {
        cmp = vcleq_u16(dc2, vmovl_u8(da));
    } else {
        cmp = vcleq_u16(sc2, vmovl_u8(sa));
    }

    // Prepare variables
    int32x4_t val1_1, val1_2;
    int32x4_t val2_1, val2_2;
    uint32x4_t cmp1, cmp2;

    // Doing a signed lengthening allows to save a few instructions
    // thanks to sign extension.
    cmp1 = vreinterpretq_u32_s32(vmovl_s16(vreinterpret_s16_u16(vget_low_u16(cmp))));
#ifdef SK_CPU_ARM64
    cmp2 = vreinterpretq_u32_s32(vmovl_high_s16(vreinterpretq_s16_u16(cmp)));
#else
    cmp2 = vreinterpretq_u32_s32(vmovl_s16(vreinterpret_s16_u16(vget_high_u16(cmp))));
#endif

    // Calc COM - SUB
    val1_1 = com1 - sub1;
    val1_2 = com2 - sub2;

    // Calc COM + SUB - sa*da
    val2_1 = com1 + sub1;
    val2_2 = com2 + sub2;

    val2_1 = vsubq_s32(val2_1, vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(sada))));
#ifdef SK_CPU_ARM64
    val2_2 = vsubq_s32(val2_2, vreinterpretq_s32_u32(vmovl_high_u16(sada)));
#else
    val2_2 = vsubq_s32(val2_2, vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(sada))));
#endif

    // Insert where needed
    val1_1 = vbslq_s32(cmp1, val1_1, val2_1);
    val1_2 = vbslq_s32(cmp2, val1_2, val2_2);

    // Call the clamp_div255round function
    return clamp_div255round_simd8_32(val1_1, val1_2);
}

static inline uint8x8_t overlay_color(uint8x8_t sc, uint8x8_t dc,
                                      uint8x8_t sa, uint8x8_t da) {
    return overlay_hardlight_color<true>(sc, dc, sa, da);
}

uint8x8x4_t overlay_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = overlay_color(src.val[NEON_R], dst.val[NEON_R],
                                    src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_G] = overlay_color(src.val[NEON_G], dst.val[NEON_G],
                                    src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_B] = overlay_color(src.val[NEON_B], dst.val[NEON_B],
                                    src.val[NEON_A], dst.val[NEON_A]);

    return ret;
}

template <bool lighten>
static inline uint8x8_t lighten_darken_color(uint8x8_t sc, uint8x8_t dc,
                                             uint8x8_t sa, uint8x8_t da) {
    uint16x8_t sd, ds, cmp, tmp, tmp2;

    // Prepare
    sd = vmull_u8(sc, da);
    ds = vmull_u8(dc, sa);

    // Do test
    if (lighten) {
        cmp = vcgtq_u16(sd, ds);
    } else {
        cmp = vcltq_u16(sd, ds);
    }

    // Assign if
    tmp = vaddl_u8(sc, dc);
    tmp2 = tmp;
    tmp -= SkDiv255Round_neon8_16_16(ds);

    // Calc else
    tmp2 -= SkDiv255Round_neon8_16_16(sd);

    // Insert where needed
    tmp = vbslq_u16(cmp, tmp, tmp2);

    return vmovn_u16(tmp);
}

static inline uint8x8_t darken_color(uint8x8_t sc, uint8x8_t dc,
                                     uint8x8_t sa, uint8x8_t da) {
    return lighten_darken_color<false>(sc, dc, sa, da);
}

uint8x8x4_t darken_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = darken_color(src.val[NEON_R], dst.val[NEON_R],
                                   src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_G] = darken_color(src.val[NEON_G], dst.val[NEON_G],
                                   src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_B] = darken_color(src.val[NEON_B], dst.val[NEON_B],
                                   src.val[NEON_A], dst.val[NEON_A]);

    return ret;
}

static inline uint8x8_t lighten_color(uint8x8_t sc, uint8x8_t dc,
                                      uint8x8_t sa, uint8x8_t da) {
    return lighten_darken_color<true>(sc, dc, sa, da);
}

uint8x8x4_t lighten_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = lighten_color(src.val[NEON_R], dst.val[NEON_R],
                                    src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_G] = lighten_color(src.val[NEON_G], dst.val[NEON_G],
                                    src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_B] = lighten_color(src.val[NEON_B], dst.val[NEON_B],
                                    src.val[NEON_A], dst.val[NEON_A]);

    return ret;
}

static inline uint8x8_t hardlight_color(uint8x8_t sc, uint8x8_t dc,
                                        uint8x8_t sa, uint8x8_t da) {
    return overlay_hardlight_color<false>(sc, dc, sa, da);
}

uint8x8x4_t hardlight_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = hardlight_color(src.val[NEON_R], dst.val[NEON_R],
                                      src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_G] = hardlight_color(src.val[NEON_G], dst.val[NEON_G],
                                      src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_B] = hardlight_color(src.val[NEON_B], dst.val[NEON_B],
                                      src.val[NEON_A], dst.val[NEON_A]);

    return ret;
}

static inline uint8x8_t difference_color(uint8x8_t sc, uint8x8_t dc,
                                         uint8x8_t sa, uint8x8_t da) {
    uint16x8_t sd, ds, tmp;
    int16x8_t val;

    sd = vmull_u8(sc, da);
    ds = vmull_u8(dc, sa);

    tmp = vminq_u16(sd, ds);
    tmp = SkDiv255Round_neon8_16_16(tmp);
    tmp = vshlq_n_u16(tmp, 1);

    val = vreinterpretq_s16_u16(vaddl_u8(sc, dc));

    val -= vreinterpretq_s16_u16(tmp);

    val = vmaxq_s16(val, vdupq_n_s16(0));
    val = vminq_s16(val, vdupq_n_s16(255));

    return vmovn_u16(vreinterpretq_u16_s16(val));
}

uint8x8x4_t difference_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = difference_color(src.val[NEON_R], dst.val[NEON_R],
                                       src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_G] = difference_color(src.val[NEON_G], dst.val[NEON_G],
                                       src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_B] = difference_color(src.val[NEON_B], dst.val[NEON_B],
                                       src.val[NEON_A], dst.val[NEON_A]);

    return ret;
}

static inline uint8x8_t exclusion_color(uint8x8_t sc, uint8x8_t dc,
                                        uint8x8_t sa, uint8x8_t da) {
    /* The equation can be simplified to 255(sc + dc) - 2 * sc * dc */

    uint16x8_t sc_plus_dc, scdc, const255;
    int32x4_t term1_1, term1_2, term2_1, term2_2;

    /* Calc (sc + dc) and (sc * dc) */
    sc_plus_dc = vaddl_u8(sc, dc);
    scdc = vmull_u8(sc, dc);

    /* Prepare constants */
    const255 = vdupq_n_u16(255);

    /* Calc the first term */
    term1_1 = vreinterpretq_s32_u32(
                vmull_u16(vget_low_u16(const255), vget_low_u16(sc_plus_dc)));
    term1_2 = vreinterpretq_s32_u32(
#ifdef SK_CPU_ARM64
                vmull_high_u16(const255, sc_plus_dc));
#else
                vmull_u16(vget_high_u16(const255), vget_high_u16(sc_plus_dc)));
#endif

    /* Calc the second term */
    term2_1 = vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(scdc), 1));
#ifdef SK_CPU_ARM64
    term2_2 = vreinterpretq_s32_u32(vshll_high_n_u16(scdc, 1));
#else
    term2_2 = vreinterpretq_s32_u32(vshll_n_u16(vget_high_u16(scdc), 1));
#endif

    return clamp_div255round_simd8_32(term1_1 - term2_1, term1_2 - term2_2);
}

uint8x8x4_t exclusion_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = exclusion_color(src.val[NEON_R], dst.val[NEON_R],
                                      src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_G] = exclusion_color(src.val[NEON_G], dst.val[NEON_G],
                                      src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_B] = exclusion_color(src.val[NEON_B], dst.val[NEON_B],
                                      src.val[NEON_A], dst.val[NEON_A]);

    return ret;
}

static inline uint8x8_t blendfunc_multiply_color(uint8x8_t sc, uint8x8_t dc,
                                                 uint8x8_t sa, uint8x8_t da) {
    uint32x4_t val1, val2;
    uint16x8_t scdc, t1, t2;

    t1 = vmull_u8(sc, vdup_n_u8(255) - da);
    t2 = vmull_u8(dc, vdup_n_u8(255) - sa);
    scdc = vmull_u8(sc, dc);

    val1 = vaddl_u16(vget_low_u16(t1), vget_low_u16(t2));
#ifdef SK_CPU_ARM64
    val2 = vaddl_high_u16(t1, t2);
#else
    val2 = vaddl_u16(vget_high_u16(t1), vget_high_u16(t2));
#endif

    val1 = vaddw_u16(val1, vget_low_u16(scdc));
#ifdef SK_CPU_ARM64
    val2 = vaddw_high_u16(val2, scdc);
#else
    val2 = vaddw_u16(val2, vget_high_u16(scdc));
#endif

    return clamp_div255round_simd8_32(
                vreinterpretq_s32_u32(val1), vreinterpretq_s32_u32(val2));
}

uint8x8x4_t multiply_modeproc_neon8(uint8x8x4_t src, uint8x8x4_t dst) {
    uint8x8x4_t ret;

    ret.val[NEON_A] = srcover_color(src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_R] = blendfunc_multiply_color(src.val[NEON_R], dst.val[NEON_R],
                                               src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_G] = blendfunc_multiply_color(src.val[NEON_G], dst.val[NEON_G],
                                               src.val[NEON_A], dst.val[NEON_A]);
    ret.val[NEON_B] = blendfunc_multiply_color(src.val[NEON_B], dst.val[NEON_B],
                                               src.val[NEON_A], dst.val[NEON_A]);

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

typedef uint8x8x4_t (*SkXfermodeProcSIMD)(uint8x8x4_t src, uint8x8x4_t dst);

extern SkXfermodeProcSIMD gNEONXfermodeProcs[];

SkNEONProcCoeffXfermode::SkNEONProcCoeffXfermode(SkReadBuffer& buffer)
        : INHERITED(buffer) {
    fProcSIMD = reinterpret_cast<void*>(gNEONXfermodeProcs[this->getMode()]);
}

void SkNEONProcCoeffXfermode::xfer32(SkPMColor* SK_RESTRICT dst,
                                     const SkPMColor* SK_RESTRICT src, int count,
                                     const SkAlpha* SK_RESTRICT aa) const {
    SkASSERT(dst && src && count >= 0);

    SkXfermodeProc proc = this->getProc();
    SkXfermodeProcSIMD procSIMD = reinterpret_cast<SkXfermodeProcSIMD>(fProcSIMD);
    SkASSERT(procSIMD != NULL);

    if (NULL == aa) {
        // Unrolled NEON code
        // We'd like to just do this (modulo a few casts):
        // vst4_u8(dst, procSIMD(vld4_u8(src), vld4_u8(dst)));
        // src += 8;
        // dst += 8;
        // but that tends to generate miserable code. Here are a bunch of faster
        // workarounds for different architectures and compilers.
        while (count >= 8) {

#ifdef SK_CPU_ARM32
            uint8x8x4_t vsrc, vdst, vres;
#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 6))
            asm volatile (
                "vld4.u8    %h[vsrc], [%[src]]!  \t\n"
                "vld4.u8    %h[vdst], [%[dst]]   \t\n"
                : [vsrc] "=w" (vsrc), [vdst] "=w" (vdst), [src] "+&r" (src)
                : [dst] "r" (dst)
                :
            );
#else
            register uint8x8_t d0 asm("d0");
            register uint8x8_t d1 asm("d1");
            register uint8x8_t d2 asm("d2");
            register uint8x8_t d3 asm("d3");
            register uint8x8_t d4 asm("d4");
            register uint8x8_t d5 asm("d5");
            register uint8x8_t d6 asm("d6");
            register uint8x8_t d7 asm("d7");

            asm volatile (
                "vld4.u8    {d0-d3},[%[src]]!;"
                "vld4.u8    {d4-d7},[%[dst]];"
                : "=w" (d0), "=w" (d1), "=w" (d2), "=w" (d3),
                  "=w" (d4), "=w" (d5), "=w" (d6), "=w" (d7),
                  [src] "+&r" (src)
                : [dst] "r" (dst)
                :
            );
            vsrc.val[0] = d0; vdst.val[0] = d4;
            vsrc.val[1] = d1; vdst.val[1] = d5;
            vsrc.val[2] = d2; vdst.val[2] = d6;
            vsrc.val[3] = d3; vdst.val[3] = d7;
#endif

            vres = procSIMD(vsrc, vdst);

            vst4_u8((uint8_t*)dst, vres);

            dst += 8;

#else // #ifdef SK_CPU_ARM32

            asm volatile (
                "ld4    {v0.8b - v3.8b}, [%[src]], #32 \t\n"
                "ld4    {v4.8b - v7.8b}, [%[dst]]      \t\n"
                "blr    %[proc]                        \t\n"
                "st4    {v0.8b - v3.8b}, [%[dst]], #32 \t\n"
                : [src] "+&r" (src), [dst] "+&r" (dst)
                : [proc] "r" (procSIMD)
                : "cc", "memory",
                  /* We don't know what proc is going to clobber so we must
                   * add everything that is not callee-saved.
                   */
                  "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9",
                  "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18",
                  "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16", "v17",
                  "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26",
                  "v27", "v28", "v29", "v30", "v31"
            );

#endif // #ifdef SK_CPU_ARM32

            count -= 8;
        }
        // Leftovers
        for (int i = 0; i < count; i++) {
            dst[i] = proc(src[i], dst[i]);
        }
    } else {
        for (int i = count - 1; i >= 0; --i) {
            unsigned a = aa[i];
            if (0 != a) {
                SkPMColor dstC = dst[i];
                SkPMColor C = proc(src[i], dstC);
                if (a != 0xFF) {
                    C = SkFourByteInterp_neon(C, dstC, a);
                }
                dst[i] = C;
            }
        }
    }
}

void SkNEONProcCoeffXfermode::xfer16(uint16_t* SK_RESTRICT dst,
                                     const SkPMColor* SK_RESTRICT src, int count,
                                     const SkAlpha* SK_RESTRICT aa) const {
    SkASSERT(dst && src && count >= 0);

    SkXfermodeProc proc = this->getProc();
    SkXfermodeProcSIMD procSIMD = reinterpret_cast<SkXfermodeProcSIMD>(fProcSIMD);
    SkASSERT(procSIMD != NULL);

    if (NULL == aa) {
        while(count >= 8) {
            uint16x8_t vdst, vres16;
            uint8x8x4_t vdst32, vsrc, vres;

            vdst = vld1q_u16(dst);

#ifdef SK_CPU_ARM64
            vsrc = vld4_u8((uint8_t*)src);
#else
#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 6))
            asm volatile (
                "vld4.u8    %h[vsrc], [%[src]]!  \t\n"
                : [vsrc] "=w" (vsrc), [src] "+&r" (src)
                : :
            );
#else
            register uint8x8_t d0 asm("d0");
            register uint8x8_t d1 asm("d1");
            register uint8x8_t d2 asm("d2");
            register uint8x8_t d3 asm("d3");

            asm volatile (
                "vld4.u8    {d0-d3},[%[src]]!;"
                : "=w" (d0), "=w" (d1), "=w" (d2), "=w" (d3),
                  [src] "+&r" (src)
                : :
            );
            vsrc.val[0] = d0;
            vsrc.val[1] = d1;
            vsrc.val[2] = d2;
            vsrc.val[3] = d3;
#endif
#endif // #ifdef SK_CPU_ARM64

            vdst32 = SkPixel16ToPixel32_neon8(vdst);
            vres = procSIMD(vsrc, vdst32);
            vres16 = SkPixel32ToPixel16_neon8(vres);

            vst1q_u16(dst, vres16);

            count -= 8;
            dst += 8;
#ifdef SK_CPU_ARM64
            src += 8;
#endif
        }
        for (int i = 0; i < count; i++) {
            SkPMColor dstC = SkPixel16ToPixel32(dst[i]);
            dst[i] = SkPixel32ToPixel16_ToU16(proc(src[i], dstC));
        }
    } else {
        for (int i = count - 1; i >= 0; --i) {
            unsigned a = aa[i];
            if (0 != a) {
                SkPMColor dstC = SkPixel16ToPixel32(dst[i]);
                SkPMColor C = proc(src[i], dstC);
                if (0xFF != a) {
                    C = SkFourByteInterp_neon(C, dstC, a);
                }
                dst[i] = SkPixel32ToPixel16_ToU16(C);
            }
        }
    }
}

#ifndef SK_IGNORE_TO_STRING
void SkNEONProcCoeffXfermode::toString(SkString* str) const {
    this->INHERITED::toString(str);
}
#endif

////////////////////////////////////////////////////////////////////////////////

SkXfermodeProcSIMD gNEONXfermodeProcs[] = {
    NULL, // kClear_Mode
    NULL, // kSrc_Mode
    NULL, // kDst_Mode
    NULL, // kSrcOver_Mode
    dstover_modeproc_neon8,
    srcin_modeproc_neon8,
    dstin_modeproc_neon8,
    srcout_modeproc_neon8,
    dstout_modeproc_neon8,
    srcatop_modeproc_neon8,
    dstatop_modeproc_neon8,
    xor_modeproc_neon8,
    plus_modeproc_neon8,
    modulate_modeproc_neon8,
    screen_modeproc_neon8,

    overlay_modeproc_neon8,
    darken_modeproc_neon8,
    lighten_modeproc_neon8,
    NULL, // kColorDodge_Mode
    NULL, // kColorBurn_Mode
    hardlight_modeproc_neon8,
    NULL, // kSoftLight_Mode
    difference_modeproc_neon8,
    exclusion_modeproc_neon8,
    multiply_modeproc_neon8,

    NULL, // kHue_Mode
    NULL, // kSaturation_Mode
    NULL, // kColor_Mode
    NULL, // kLuminosity_Mode
};

SK_COMPILE_ASSERT(
    SK_ARRAY_COUNT(gNEONXfermodeProcs) == SkXfermode::kLastMode + 1,
    mode_count_arm
);

SkXfermodeProc gNEONXfermodeProcs1[] = {
    NULL, // kClear_Mode
    NULL, // kSrc_Mode
    NULL, // kDst_Mode
    NULL, // kSrcOver_Mode
    NULL, // kDstOver_Mode
    NULL, // kSrcIn_Mode
    NULL, // kDstIn_Mode
    NULL, // kSrcOut_Mode
    NULL, // kDstOut_Mode
    srcatop_modeproc_neon,
    dstatop_modeproc_neon,
    xor_modeproc_neon,
    plus_modeproc_neon,
    modulate_modeproc_neon,
    NULL, // kScreen_Mode

    NULL, // kOverlay_Mode
    NULL, // kDarken_Mode
    NULL, // kLighten_Mode
    NULL, // kColorDodge_Mode
    NULL, // kColorBurn_Mode
    NULL, // kHardLight_Mode
    NULL, // kSoftLight_Mode
    NULL, // kDifference_Mode
    NULL, // kExclusion_Mode
    NULL, // kMultiply_Mode

    NULL, // kHue_Mode
    NULL, // kSaturation_Mode
    NULL, // kColor_Mode
    NULL, // kLuminosity_Mode
};

SK_COMPILE_ASSERT(
    SK_ARRAY_COUNT(gNEONXfermodeProcs1) == SkXfermode::kLastMode + 1,
    mode1_count_arm
);

SkProcCoeffXfermode* SkPlatformXfermodeFactory_impl_neon(const ProcCoeff& rec,
                                                         SkXfermode::Mode mode) {

    void* procSIMD = reinterpret_cast<void*>(gNEONXfermodeProcs[mode]);

    if (procSIMD != NULL) {
        return SkNEW_ARGS(SkNEONProcCoeffXfermode, (rec, mode, procSIMD));
    }
    return NULL;
}

SkXfermodeProc SkPlatformXfermodeProcFactory_impl_neon(SkXfermode::Mode mode) {
    return gNEONXfermodeProcs1[mode];
}
