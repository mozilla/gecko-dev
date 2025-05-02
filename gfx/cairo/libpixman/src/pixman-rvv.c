/*
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 *             2024 Filip Wasil, Samsung Electronics
 *             2024 Bernard Gingold, Samsung Electronics
 *             2025 Marek Pikuła, Samsung Electronics
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <pixman-config.h>
#endif

#include "pixman-combine-float.h"
#include "pixman-combine32.h"
#include "pixman-inlines.h"
#include "pixman-private.h"

#include <riscv_vector.h>

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Convenience macros {

#define __FE_PTR(p, vl) ((p) += (vl))

#define _RVV_FE_PRE(total_len, vn, vl, vspec)                                  \
    size_t vn = total_len, vl = __riscv_vsetvl_##vspec (vn);                   \
    vn > 0

#define _RVV_FE_POST(vn, vl, vspec) vn -= (vl), vl = __riscv_vsetvl_##vspec (vn)

#define RVV_FOREACH_1(total_len, vl, vspec, p1)                                \
    for (_RVV_FE_PRE (total_len, vn, vl, vspec);                               \
	 __FE_PTR (p1, vl), _RVV_FE_POST (vn, vl, vspec))

#define RVV_FOREACH_2(total_len, vl, vspec, p1, p2)                            \
    for (_RVV_FE_PRE (total_len, vn, vl, vspec);                               \
	 __FE_PTR (p1, vl), __FE_PTR (p2, vl), _RVV_FE_POST (vn, vl, vspec))

#define RVV_FOREACH_3(total_len, vl, vspec, p1, p2, p3)                        \
    for (_RVV_FE_PRE (total_len, vn, vl, vspec);                               \
	 __FE_PTR (p1, vl), __FE_PTR (p2, vl), __FE_PTR (p3, vl),              \
	 _RVV_FE_POST (vn, vl, vspec))

// vuintXXmYY_t for use in macros (less token concatenation).
#define VUINT(ELEN, LMUL) vuint##ELEN##LMUL##_t
#define VUINT32(LMUL)     VUINT (32, LMUL)
#define VUINT16(LMUL)     VUINT (16, LMUL)
#define VUINT8(LMUL)      VUINT (8, LMUL)

// Short for vreinterpret commonly used for ARGB batch operations.
#define RVV_U8x4_U32(LMUL, value)                                              \
    __riscv_vreinterpret_v_u8##LMUL##_u32##LMUL (value)
#define RVV_U8x4_U32_m2(value) RVV_U8x4_U32 (m2, value)
#define RVV_U8x4_U32_m4(value) RVV_U8x4_U32 (m4, value)

#define RVV_U32_U8x4(LMUL, value)                                              \
    __riscv_vreinterpret_v_u32##LMUL##_u8##LMUL (value)
#define RVV_U32_U8x4_m2(value) RVV_U32_U8x4 (m2, value)
#define RVV_U32_U8x4_m4(value) RVV_U32_U8x4 (m4, value)

// }

// Float implementation

/*
 * Screen
 *
 *      ad * as * B(d/ad, s/as)
 *    = ad * as * (d/ad + s/as - s/as * d/ad)
 *    = ad * s + as * d - s * d
 */

static force_inline vfloat32m1_t
rvv_blend_screen_float (const vfloat32m1_t sa,
			const vfloat32m1_t s,
			const vfloat32m1_t da,
			const vfloat32m1_t d,
			size_t             vl)
{
    vfloat32m1_t t0, t1, t2;
    t0 = __riscv_vfmul_vv_f32m1 (s, da, vl);
    t1 = __riscv_vfmul_vv_f32m1 (d, sa, vl);
    t2 = __riscv_vfmul_vv_f32m1 (s, d, vl);
    return __riscv_vfsub_vv_f32m1 (__riscv_vfadd_vv_f32m1 (t0, t1, vl), t2, vl);
}

/*
 * Multiply
 *
 *      ad * as * B(d / ad, s / as)
 *    = ad * as * d/ad * s/as
 *    = d * s
 *
 */

static force_inline vfloat32m1_t
rvv_blend_multiply_float (const vfloat32m1_t sa,
			  const vfloat32m1_t s,
			  const vfloat32m1_t da,
			  const vfloat32m1_t d,
			  size_t             vl)
{
    return __riscv_vfmul_vv_f32m1 (s, d, vl);
}

/*
 * Overlay
 *
 *     ad * as * B(d/ad, s/as)
 *   = ad * as * Hardlight (s, d)
 *   = if (d / ad < 0.5)
 *         as * ad * Multiply (s/as, 2 * d/ad)
 *     else
 *         as * ad * Screen (s/as, 2 * d / ad - 1)
 *   = if (d < 0.5 * ad)
 *         as * ad * s/as * 2 * d /ad
 *     else
 *         as * ad * (s/as + 2 * d / ad - 1 - s / as * (2 * d / ad - 1))
 *   = if (2 * d < ad)
 *         2 * s * d
 *     else
 *         ad * s + 2 * as * d - as * ad - ad * s * (2 * d / ad - 1)
 *   = if (2 * d < ad)
 *         2 * s * d
 *     else
 *         as * ad - 2 * (ad - d) * (as - s)
 */

static force_inline vfloat32m1_t
rvv_blend_overlay_float (const vfloat32m1_t sa,
			 const vfloat32m1_t s,
			 const vfloat32m1_t da,
			 const vfloat32m1_t d,
			 size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4, f0, f1, f2;
    vbool32_t    vb;
    t0 = __riscv_vfadd_vv_f32m1 (d, d, vl);
    t1 = __riscv_vfmul_vv_f32m1 (__riscv_vfadd_vv_f32m1 (s, s, vl), d, vl);
    vb = __riscv_vmflt_vv_f32m1_b32 (t0, da, vl);
    t2 = __riscv_vfmul_vv_f32m1 (sa, da, vl);
    f2 = __riscv_vfsub_vv_f32m1 (da, d, vl);
    t3 = __riscv_vfmul_vf_f32m1 (f2, 2.0f, vl);
    t4 = __riscv_vfsub_vv_f32m1 (sa, s, vl);
    f0 = __riscv_vfmul_vv_f32m1 (t3, t4, vl);
    f1 = __riscv_vfsub_vv_f32m1 (t2, f0, vl);
    return __riscv_vmerge_vvm_f32m1 (f1, t1, vb, vl);
}

/*
 * Darken
 *
 *     ad * as * B(d/ad, s/as)
 *   = ad * as * MIN(d/ad, s/as)
 *   = MIN (as * d, ad * s)
 */

static force_inline vfloat32m1_t
rvv_blend_darken_float (const vfloat32m1_t sa,
			const vfloat32m1_t s,
			const vfloat32m1_t da,
			const vfloat32m1_t d,
			size_t             vl)
{
    vfloat32m1_t ss, dd;
    vbool32_t    vb;
    ss = __riscv_vfmul_vv_f32m1 (da, s, vl);
    dd = __riscv_vfmul_vv_f32m1 (sa, d, vl);
    vb = __riscv_vmfgt_vv_f32m1_b32 (ss, dd, vl);
    return __riscv_vmerge_vvm_f32m1 (ss, dd, vb, vl);
}

/*
 * Lighten
 *
 *     ad * as * B(d/ad, s/as)
 *   = ad * as * MAX(d/ad, s/as)
 *   = MAX (as * d, ad * s)
 */

static force_inline vfloat32m1_t
rvv_blend_lighten_float (const vfloat32m1_t sa,
			 const vfloat32m1_t s,
			 const vfloat32m1_t da,
			 const vfloat32m1_t d,
			 size_t             vl)
{
    vfloat32m1_t ss, dd;
    vbool32_t    vb;
    ss = __riscv_vfmul_vv_f32m1 (s, da, vl);
    dd = __riscv_vfmul_vv_f32m1 (d, sa, vl);
    vb = __riscv_vmfgt_vv_f32m1_b32 (ss, dd, vl);
    return __riscv_vmerge_vvm_f32m1 (dd, ss, vb, vl);
}

/*
 * Color dodge
 *
 *     ad * as * B(d/ad, s/as)
 *   = if d/ad = 0
 *         ad * as * 0
 *     else if (d/ad >= (1 - s/as)
 *         ad * as * 1
 *     else
 *         ad * as * ((d/ad) / (1 - s/as))
 *   = if d = 0
 *         0
 *     elif as * d >= ad * (as - s)
 *         ad * as
 *     else
 *         as * (as * d / (as - s))
 *
 */

static force_inline vfloat32m1_t
rvv_blend_color_dodge_float (const vfloat32m1_t sa,
			     const vfloat32m1_t s,
			     const vfloat32m1_t da,
			     const vfloat32m1_t d,
			     size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4;
    vbool32_t    is_d_zero, vb, is_t0_non_zero;

    is_d_zero = __riscv_vmfeq_vf_f32m1_b32 (d, 0.0f, vl);

    t0 = __riscv_vfsub_vv_f32m1 (sa, s, vl);  // sa - s
    t1 = __riscv_vfmul_vv_f32m1 (sa, d, vl);  // d * sa
    t2 = __riscv_vfmul_vv_f32m1 (sa, da, vl); // sa * da
    t3 = __riscv_vfsub_vv_f32m1 (t2, __riscv_vfmul_vv_f32m1 (s, da, vl),
				 vl); // sa * da - s * da

    is_t0_non_zero = __riscv_vmfne_vf_f32m1_b32 (t0, 0.0f, vl);
    vb             = __riscv_vmflt_vv_f32m1_b32 (t3, t1, vl);
    t4 = __riscv_vfdiv_vv_f32m1 (__riscv_vfmul_vv_f32m1 (sa, t1, vl), t0,
				 vl); // sa * sa * d / (sa - s);

    return __riscv_vfmerge_vfm_f32m1 (
	__riscv_vmerge_vvm_f32m1 (
	    __riscv_vmerge_vvm_f32m1 (t2, t4, is_t0_non_zero, vl), t2, vb, vl),
	0.0f, is_d_zero, vl);
}

/*
 * Color burn
 *
 * We modify the first clause "if d = 1" to "if d >= 1" since with
 * premultiplied colors d > 1 can actually happen.
 *
 *     ad * as * B(d/ad, s/as)
 *   = if d/ad >= 1
 *         ad * as * 1
 *     elif (1 - d/ad) >= s/as
 *         ad * as * 0
 *     else
 *         ad * as * (1 - ((1 - d/ad) / (s/as)))
 *   = if d >= ad
 *         ad * as
 *     elif as * ad - as * d >= ad * s
 *         0
 *     else
 *         ad * as  - as * as * (ad - d) / s
 */

static force_inline vfloat32m1_t
rvv_blend_color_burn_float (const vfloat32m1_t sa,
			    const vfloat32m1_t s,
			    const vfloat32m1_t da,
			    const vfloat32m1_t d,
			    size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4, t5, t6, t7;
    vbool32_t    is_d_ge_da, is_s_zero, vb;

    is_d_ge_da = __riscv_vmfge_vv_f32m1_b32 (d, da, vl);
    is_s_zero  = __riscv_vmfeq_vf_f32m1_b32 (s, 0.0f, vl);

    t0 = __riscv_vfmul_vv_f32m1 (sa, __riscv_vfsub_vv_f32m1 (da, d, vl),
				 vl); // sa * (da - d)
    t1 = __riscv_vfsub_vv_f32m1 (da, __riscv_vfdiv_vv_f32m1 (t0, s, vl),
				 vl);         // da - sa * (da - d) / s)
    t2 = __riscv_vfmul_vv_f32m1 (sa, da, vl); // sa * da
    t3 = __riscv_vfmul_vv_f32m1 (sa, t1, vl); // sa * (da - sa * (da - d) / s)
    t4 = __riscv_vfmul_vv_f32m1 (s, da, vl);  // s * da
    vb = __riscv_vmfge_vf_f32m1_b32 (__riscv_vfsub_vv_f32m1 (t0, t4, vl), 0.0f,
				     vl); // if (sa * (da - d) - s * da >= 0.0f)

    t6 = __riscv_vfmerge_vfm_f32m1 (t3, 0.0f, is_s_zero, vl);
    t5 = __riscv_vfmerge_vfm_f32m1 (t6, 0.0f, vb, vl);
    t7 = __riscv_vmerge_vvm_f32m1 (t5, t2, is_d_ge_da, vl);

    return t7;
}

/*
 * Hard light
 *
 *     ad * as * B(d/ad, s/as)
 *   = if (s/as <= 0.5)
 *         ad * as * Multiply (d/ad, 2 * s/as)
 *     else
 *         ad * as * Screen (d/ad, 2 * s/as - 1)
 *   = if 2 * s <= as
 *         ad * as * d/ad * 2 * s / as
 *     else
 *         ad * as * (d/ad + (2 * s/as - 1) + d/ad * (2 * s/as - 1))
 *   = if 2 * s <= as
 *         2 * s * d
 *     else
 *         as * ad - 2 * (ad - d) * (as - s)
 */

static force_inline vfloat32m1_t
rvv_blend_hard_light_float (const vfloat32m1_t sa,
			    const vfloat32m1_t s,
			    const vfloat32m1_t da,
			    const vfloat32m1_t d,
			    size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4;
    vbool32_t    vb;
    t0 = __riscv_vfadd_vv_f32m1 (s, s, vl);
    t1 = __riscv_vfmul_vv_f32m1 (__riscv_vfadd_vv_f32m1 (s, s, vl), d, vl);
    vb = __riscv_vmfgt_vv_f32m1_b32 (t0, sa, vl);
    t2 = __riscv_vfmul_vv_f32m1 (sa, da, vl);
    t3 = __riscv_vfmul_vf_f32m1 (__riscv_vfsub_vv_f32m1 (da, d, vl), 2.0f, vl);
    t4 = __riscv_vfsub_vv_f32m1 (sa, s, vl);
    return __riscv_vmerge_vvm_f32m1 (
	t1,
	__riscv_vfsub_vv_f32m1 (t2, __riscv_vfmul_vv_f32m1 (t3, t4, vl), vl),
	vb, vl);
}

/*
 * Soft light
 *
 *     ad * as * B(d/ad, s/as)
 *   = if (s/as <= 0.5)
 *         ad * as * (d/ad - (1 - 2 * s/as) * d/ad * (1 - d/ad))
 *     else if (d/ad <= 0.25)
 *         ad * as * (d/ad + (2 * s/as - 1) * ((((16 * d/ad - 12) * d/ad + 4) * d/ad) - d/ad))
 *     else
 *         ad * as * (d/ad + (2 * s/as - 1) * sqrt (d/ad))
 *   = if (2 * s <= as)
 *         d * as - d * (ad - d) * (as - 2 * s) / ad;
 *     else if (4 * d <= ad)
 *         (2 * s - as) * d * ((16 * d / ad - 12) * d / ad + 3);
 *     else
 *         d * as + (sqrt (d * ad) - d) * (2 * s - as);
 */

static force_inline vfloat32m1_t
rvv_blend_soft_light_float (const vfloat32m1_t sa,
			    const vfloat32m1_t s,
			    const vfloat32m1_t da,
			    const vfloat32m1_t d,
			    size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13;
    vbool32_t    is_sa_lt_2s, is_da_ls_4d, is_da_non_zero;
    is_da_non_zero = __riscv_vmfne_vf_f32m1_b32 (da, 0.0f, vl);
    t0             = __riscv_vfadd_vv_f32m1 (s, s, vl); // 2 * s
    is_sa_lt_2s    = __riscv_vmflt_vv_f32m1_b32 (sa, t0, vl);
    t1             = __riscv_vfmul_vv_f32m1 (sa, d, vl);  // d * sa
    t2             = __riscv_vfsub_vv_f32m1 (sa, t0, vl); // (sa - 2*s)
    t3             = __riscv_vfmul_vv_f32m1 (d, t2, vl);  // (sa - 2*s) * d
    t7 = __riscv_vfdiv_vv_f32m1 (__riscv_vfmul_vf_f32m1 (d, 16.0f, vl), da,
				 vl); // 16 * d / da
    t8 = __riscv_vfmul_vv_f32m1 (d, __riscv_vfsub_vf_f32m1 (t7, 12.0f, vl),
				 vl); // (16 * d / da - 12) * d
    t9 = __riscv_vfadd_vf_f32m1 (__riscv_vfdiv_vv_f32m1 (t8, da, vl), 3.0f,
				 vl); // (16 * d / da - 12) * d / da + 3)
    t4 = __riscv_vfmul_vv_f32m1 (
	t3, t9, vl); // (sa - 2*s) * d * ((16 * d / da - 12) * d / da + 3)
    t5 = __riscv_vfsub_vv_f32m1 (
	t1, t4,
	vl); // d * sa - (sa - 2*s) * d * ((16 * d / da - 12) * d / da + 3)
    t6          = __riscv_vfadd_vv_f32m1 (__riscv_vfadd_vv_f32m1 (d, d, vl),
					  __riscv_vfadd_vv_f32m1 (d, d, vl), vl);
    is_da_ls_4d = __riscv_vmflt_vv_f32m1_b32 (da, t6, vl);
    t10         = __riscv_vfsub_vv_f32m1 (
        __riscv_vfsqrt_v_f32m1 (__riscv_vfmul_vv_f32m1 (d, da, vl), vl), d,
        vl); // sqrtf (d * da) - d
    t11 = __riscv_vfmul_vv_f32m1 (t2, t10,
				  vl); // (sqrtf (d * da) - d) * (sa - 2 * s)
    t12 = __riscv_vfsub_vv_f32m1 (
	t1, t11, vl); // d * sa - (sqrtf (d * da) - d) * (sa - 2 * s)
    // d * sa - d * (da - d) * (sa - 2 * s) / da
    t13 = __riscv_vfsub_vv_f32m1 (
	t1,
	__riscv_vfdiv_vv_f32m1 (
	    __riscv_vfmul_vv_f32m1 (__riscv_vfmul_vv_f32m1 (d, t2, vl),
				    __riscv_vfsub_vv_f32m1 (da, d, vl), vl),
	    da, vl),
	vl);
    return __riscv_vmerge_vvm_f32m1 (
	t1, // if (!FLOAT_IS_ZERO (da))
	__riscv_vmerge_vvm_f32m1 (
	    t13, // if (4 * d > da)
	    __riscv_vmerge_vvm_f32m1 (t5, t12, is_da_ls_4d, vl), is_sa_lt_2s,
	    vl),
	is_da_non_zero, vl);
}

/*
 * Difference
 *
 *     ad * as * B(s/as, d/ad)
 *   = ad * as * abs (s/as - d/ad)
 *   = if (s/as <= d/ad)
 *         ad * as * (d/ad - s/as)
 *     else
 *         ad * as * (s/as - d/ad)
 *   = if (ad * s <= as * d)
 *        as * d - ad * s
 *     else
 *        ad * s - as * d
 */

static force_inline vfloat32m1_t
rvv_blend_difference_float (const vfloat32m1_t sa,
			    const vfloat32m1_t s,
			    const vfloat32m1_t da,
			    const vfloat32m1_t d,
			    size_t             vl)
{
    vfloat32m1_t dsa, sda;
    vbool32_t    vb;
    dsa = __riscv_vfmul_vv_f32m1 (d, sa, vl);
    sda = __riscv_vfmul_vv_f32m1 (s, da, vl);
    vb  = __riscv_vmflt_vv_f32m1_b32 (sda, dsa, vl);
    return __riscv_vmerge_vvm_f32m1 (__riscv_vfsub_vv_f32m1 (sda, dsa, vl),
				     __riscv_vfsub_vv_f32m1 (dsa, sda, vl), vb,
				     vl);
}

/*
 * Exclusion
 *
 *     ad * as * B(s/as, d/ad)
 *   = ad * as * (d/ad + s/as - 2 * d/ad * s/as)
 *   = as * d + ad * s - 2 * s * d
 */

static force_inline vfloat32m1_t
rvv_blend_exclusion_float (const vfloat32m1_t sa,
			   const vfloat32m1_t s,
			   const vfloat32m1_t da,
			   const vfloat32m1_t d,
			   size_t             vl)
{
    vfloat32m1_t t0, t1;
    t0 = __riscv_vfmul_vv_f32m1 (__riscv_vfadd_vv_f32m1 (d, d, vl), s, vl);
    t1 = __riscv_vfadd_vv_f32m1 (__riscv_vfmul_vv_f32m1 (s, da, vl),
				 __riscv_vfmul_vv_f32m1 (d, sa, vl), vl);
    return __riscv_vfsub_vv_f32m1 (t1, t0, vl);
}

typedef vfloat32m1_t (*rvv_combine_channel_float_t) (const vfloat32m1_t sa,
						     const vfloat32m1_t s,
						     const vfloat32m1_t da,
						     const vfloat32m1_t d,
						     size_t             vl);

static force_inline void
rvv_combine_inner_float (pixman_bool_t               component,
			 float                      *dest,
			 const float                *src,
			 const float                *mask,
			 int                         n_pixels,
			 rvv_combine_channel_float_t combine_a,
			 rvv_combine_channel_float_t combine_c)
{
    float *__restrict__ pd       = dest;
    const float *__restrict__ ps = src;
    const float *__restrict__ pm = mask;

    const int component_count = 4;
    int       vn              = component_count * n_pixels;
    int       vl              = 0;
    int       vl_step         = 0;

    const ptrdiff_t stride = component_count * sizeof (float);

    vfloat32m1x4_t sa_sr_sg_sb, da_dr_dg_db, ma_mr_mg_mb;
    vfloat32m1_t   da2, dr2, dg2, db2, ma2, mr2, mg2, mb2, sr2, sg2, sb2, sa2;

    if (n_pixels == 0)
    {
	return;
    }

    if (!mask)
    {
	for (; vn > 0; vn -= vl_step, pd += vl_step, ps += vl_step)
	{
	    vl          = __riscv_vsetvl_e32m1 (vn / component_count);
	    sa_sr_sg_sb = __riscv_vlseg4e32_v_f32m1x4 (ps, vl);
	    da_dr_dg_db = __riscv_vlseg4e32_v_f32m1x4 (pd, vl);

	    da2 = combine_a (__riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0), vl);

	    dr2 = combine_c (__riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 1),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 1), vl);

	    dg2 = combine_c (__riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 2),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 2), vl);

	    db2 = combine_c (__riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 3),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
			     __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 3), vl);

	    __riscv_vsseg4e32_v_f32m1x4 (
		pd, __riscv_vcreate_v_f32m1x4 (da2, dr2, dg2, db2), vl);

	    vl_step = vl * component_count;
	}
    }
    else
    {
	if (component)
	{
	    for (; vn > 0;
		 vn -= vl_step, pd += vl_step, ps += vl_step, pm += vl_step)
	    {
		vl = __riscv_vsetvl_e32m1 (vn / component_count);

		sa_sr_sg_sb = __riscv_vlseg4e32_v_f32m1x4 (ps, vl);
		da_dr_dg_db = __riscv_vlseg4e32_v_f32m1x4 (pd, vl);
		ma_mr_mg_mb = __riscv_vlseg4e32_v_f32m1x4 (pm, vl);

		sr2 = __riscv_vfmul_vv_f32m1 (
		    __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 1),
		    __riscv_vget_v_f32m1x4_f32m1 (ma_mr_mg_mb, 1), vl);

		sg2 = __riscv_vfmul_vv_f32m1 (
		    __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 2),
		    __riscv_vget_v_f32m1x4_f32m1 (ma_mr_mg_mb, 2), vl);

		sb2 = __riscv_vfmul_vv_f32m1 (
		    __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 3),
		    __riscv_vget_v_f32m1x4_f32m1 (ma_mr_mg_mb, 3), vl);

		ma2 = __riscv_vfmul_vv_f32m1 (
		    __riscv_vget_v_f32m1x4_f32m1 (ma_mr_mg_mb, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0), vl);

		mr2 = __riscv_vfmul_vv_f32m1 (
		    __riscv_vget_v_f32m1x4_f32m1 (ma_mr_mg_mb, 1),
		    __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0), vl);

		mg2 = __riscv_vfmul_vv_f32m1 (
		    __riscv_vget_v_f32m1x4_f32m1 (ma_mr_mg_mb, 2),
		    __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0), vl);

		mb2 = __riscv_vfmul_vv_f32m1 (
		    __riscv_vget_v_f32m1x4_f32m1 (ma_mr_mg_mb, 3),
		    __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0), vl);

		da2 = combine_a (
		    ma2, ma2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0), vl);

		dr2 = combine_c (
		    mr2, sr2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 1), vl);

		dg2 = combine_c (
		    mg2, sg2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 2), vl);

		db2 = combine_c (
		    mb2, sb2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 3), vl);

		__riscv_vsseg4e32_v_f32m1x4 (
		    pd, __riscv_vcreate_v_f32m1x4 (da2, dr2, dg2, db2), vl);

		vl_step = vl * component_count;
	    }
	}
	else
	{
	    for (; vn > 0;
		 vn -= vl_step, pd += vl_step, ps += vl_step, pm += vl_step)
	    {
		vl = __riscv_vsetvl_e32m1 (vn / component_count);

		sa_sr_sg_sb = __riscv_vlseg4e32_v_f32m1x4 (ps, vl);
		da_dr_dg_db = __riscv_vlseg4e32_v_f32m1x4 (pd, vl);
		ma2         = __riscv_vlse32_v_f32m1 (pm, stride, vl);

		sa2 = __riscv_vfmul_vv_f32m1 (
		    ma2, __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 0), vl);
		sr2 = __riscv_vfmul_vv_f32m1 (
		    ma2, __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 1), vl);
		sg2 = __riscv_vfmul_vv_f32m1 (
		    ma2, __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 2), vl);
		sb2 = __riscv_vfmul_vv_f32m1 (
		    ma2, __riscv_vget_v_f32m1x4_f32m1 (sa_sr_sg_sb, 3), vl);

		ma2 = sa2;

		dr2 = combine_c (
		    ma2, sr2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 1), vl);

		dg2 = combine_c (
		    ma2, sg2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 2), vl);

		db2 = combine_c (
		    ma2, sb2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 3), vl);

		da2 = combine_a (
		    ma2, sa2, __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0),
		    __riscv_vget_v_f32m1x4_f32m1 (da_dr_dg_db, 0), vl);

		__riscv_vsseg4e32_v_f32m1x4 (
		    pd, __riscv_vcreate_v_f32m1x4 (da2, dr2, dg2, db2), vl);

		vl_step = vl * component_count;
	    }
	}
    }
}

#define RVV_MAKE_COMBINER(name, component, combine_a, combine_c)               \
    static void rvv_combine_##name##_float (                                   \
	pixman_implementation_t *imp, pixman_op_t op, float *dest,             \
	const float *src, const float *mask, int n_pixels)                     \
    {                                                                          \
	rvv_combine_inner_float (component, dest, src, mask, n_pixels,         \
				 combine_a, combine_c);                        \
    }

#define RVV_MAKE_COMBINERS(name, combine_a, combine_c)                         \
    RVV_MAKE_COMBINER (name##_ca, TRUE, combine_a, combine_c)                  \
    RVV_MAKE_COMBINER (name##_u, FALSE, combine_a, combine_c)

static force_inline vfloat32m1_t
rvv_get_factor_float (combine_factor_t factor,
		      vfloat32m1_t     sa,
		      vfloat32m1_t     da,
		      size_t           vl)
{
    vfloat32m1_t vone  = __riscv_vfmv_v_f_f32m1 (1.0f, vl);
    vfloat32m1_t vzero = __riscv_vfmv_v_f_f32m1 (0.0f, vl);

    switch (factor)
    {
	case ZERO:
	    return vzero;

	case ONE:
	    return vone;

	case SRC_ALPHA:
	    return sa;

	case DEST_ALPHA:
	    return da;

	case INV_SA:
	    return __riscv_vfsub_vv_f32m1 (vone, sa, vl);

	case INV_DA:
	    return __riscv_vfsub_vv_f32m1 (vone, da, vl);

	case SA_OVER_DA:
	    return __riscv_vmerge_vvm_f32m1 (
		vone,
		__riscv_vfmin_vv_f32m1 (
		    vone,
		    __riscv_vfmax_vv_f32m1 (
			vzero, __riscv_vfdiv_vv_f32m1 (sa, da, vl), vl),
		    vl),
		__riscv_vmfne_vf_f32m1_b32 (da, 0.0f, vl), vl);

	case DA_OVER_SA:
	    return __riscv_vmerge_vvm_f32m1 (
		__riscv_vfmin_vv_f32m1 (
		    vone,
		    __riscv_vfmax_vv_f32m1 (
			vzero, __riscv_vfdiv_vv_f32m1 (da, sa, vl), vl),
		    vl),
		vone, __riscv_vmfeq_vf_f32m1_b32 (sa, 0.0f, vl), vl);

	case INV_SA_OVER_DA:
	    {
		vfloat32m1_t t0 = __riscv_vfdiv_vv_f32m1 (
		    __riscv_vfsub_vv_f32m1 (vone, sa, vl), da, vl);
		return __riscv_vmerge_vvm_f32m1 (
		    vone,
		    __riscv_vfmin_vv_f32m1 (
			vone, __riscv_vfmax_vv_f32m1 (vzero, t0, vl), vl),
		    __riscv_vmfne_vf_f32m1_b32 (da, 0.0f, vl), vl);
	    }

	case INV_DA_OVER_SA:
	    {
		vfloat32m1_t t0 = __riscv_vfdiv_vv_f32m1 (
		    __riscv_vfsub_vv_f32m1 (vone, da, vl), sa, vl);
		return __riscv_vmerge_vvm_f32m1 (
		    vone,
		    __riscv_vfmin_vv_f32m1 (
			vone, __riscv_vfmax_vv_f32m1 (vzero, t0, vl), vl),
		    __riscv_vmfne_vf_f32m1_b32 (sa, 0.0f, vl), vl);
	    }

	case ONE_MINUS_SA_OVER_DA:
	    {
		vfloat32m1_t t0 = __riscv_vfsub_vv_f32m1 (
		    vone, __riscv_vfdiv_vv_f32m1 (sa, da, vl), vl);
		return __riscv_vmerge_vvm_f32m1 (
		    vzero,
		    __riscv_vfmin_vv_f32m1 (
			vone, __riscv_vfmax_vv_f32m1 (vzero, t0, vl), vl),
		    __riscv_vmfne_vf_f32m1_b32 (da, 0.0f, vl), vl);
	    }

	case ONE_MINUS_DA_OVER_SA:
	    {
		vfloat32m1_t t0 = __riscv_vfsub_vv_f32m1 (
		    vone, __riscv_vfdiv_vv_f32m1 (da, sa, vl), vl);
		return __riscv_vmerge_vvm_f32m1 (
		    vzero,
		    __riscv_vfmin_vv_f32m1 (
			vone, __riscv_vfmax_vv_f32m1 (vzero, t0, vl), vl),
		    __riscv_vmfne_vf_f32m1_b32 (sa, 0.0f, vl), vl);
	    }

	case ONE_MINUS_INV_DA_OVER_SA:
	    {
		vbool32_t is_zero = __riscv_vmand_mm_b32 (
		    __riscv_vmflt_vf_f32m1_b32 (sa, FLT_MIN, vl),
		    __riscv_vmfgt_vf_f32m1_b32 (sa, -FLT_MAX, vl), vl);
		vfloat32m1_t t0 = __riscv_vfsub_vv_f32m1 (
		    vone,
		    __riscv_vfdiv_vv_f32m1 (
			__riscv_vfsub_vv_f32m1 (vone, da, vl), sa, vl),
		    vl);
		return __riscv_vmerge_vvm_f32m1 (
		    __riscv_vfmin_vv_f32m1 (
			vone, __riscv_vfmax_vv_f32m1 (vzero, t0, vl), vl),
		    vzero, is_zero, vl);
	    }

	case ONE_MINUS_INV_SA_OVER_DA:
	    {
		vfloat32m1_t t0 = __riscv_vfsub_vv_f32m1 (
		    vone,
		    __riscv_vfdiv_vv_f32m1 (
			__riscv_vfsub_vv_f32m1 (vone, sa, vl), da, vl),
		    vl);
		return __riscv_vmerge_vvm_f32m1 (
		    __riscv_vfmin_vv_f32m1 (
			vone, __riscv_vfmax_vv_f32m1 (vzero, t0, vl), vl),
		    vzero, __riscv_vmfeq_vf_f32m1_b32 (da, 0.0f, vl), vl);
	    }
    }

    return __riscv_vfmv_v_f_f32m1 (-1.0f, vl);
}

#define RVV_MAKE_PD_COMBINERS(name, a, b)                                      \
    static vfloat32m1_t force_inline rvv_pd_combine_##name##_float (           \
	vfloat32m1_t sa, vfloat32m1_t s, vfloat32m1_t da, vfloat32m1_t d,      \
	size_t vl)                                                             \
    {                                                                          \
	const vfloat32m1_t fa = rvv_get_factor_float (a, sa, da, vl);          \
	const vfloat32m1_t fb = rvv_get_factor_float (b, sa, da, vl);          \
	vfloat32m1_t       t0 = __riscv_vfadd_vv_f32m1 (                       \
            __riscv_vfmul_vv_f32m1 (s, fa, vl),                          \
            __riscv_vfmul_vv_f32m1 (d, fb, vl), vl);                     \
	return __riscv_vfmin_vv_f32m1 (__riscv_vfmv_v_f_f32m1 (1.0f, vl), t0,  \
				       vl);                                    \
    }                                                                          \
                                                                               \
    RVV_MAKE_COMBINERS (name, rvv_pd_combine_##name##_float,                   \
			rvv_pd_combine_##name##_float)

RVV_MAKE_PD_COMBINERS (clear, ZERO, ZERO)
RVV_MAKE_PD_COMBINERS (src, ONE, ZERO)
RVV_MAKE_PD_COMBINERS (dst, ZERO, ONE)
RVV_MAKE_PD_COMBINERS (over, ONE, INV_SA)
RVV_MAKE_PD_COMBINERS (over_reverse, INV_DA, ONE)
RVV_MAKE_PD_COMBINERS (in, DEST_ALPHA, ZERO)
RVV_MAKE_PD_COMBINERS (in_reverse, ZERO, SRC_ALPHA)
RVV_MAKE_PD_COMBINERS (out, INV_DA, ZERO)
RVV_MAKE_PD_COMBINERS (out_reverse, ZERO, INV_SA)
RVV_MAKE_PD_COMBINERS (atop, DEST_ALPHA, INV_SA)
RVV_MAKE_PD_COMBINERS (atop_reverse, INV_DA, SRC_ALPHA)
RVV_MAKE_PD_COMBINERS (xor, INV_DA, INV_SA)
RVV_MAKE_PD_COMBINERS (add, ONE, ONE)

RVV_MAKE_PD_COMBINERS (saturate, INV_DA_OVER_SA, ONE)

RVV_MAKE_PD_COMBINERS (disjoint_clear, ZERO, ZERO)
RVV_MAKE_PD_COMBINERS (disjoint_src, ONE, ZERO)
RVV_MAKE_PD_COMBINERS (disjoint_dst, ZERO, ONE)
RVV_MAKE_PD_COMBINERS (disjoint_over, ONE, INV_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (disjoint_over_reverse, INV_DA_OVER_SA, ONE)
RVV_MAKE_PD_COMBINERS (disjoint_in, ONE_MINUS_INV_DA_OVER_SA, ZERO)
RVV_MAKE_PD_COMBINERS (disjoint_in_reverse, ZERO, ONE_MINUS_INV_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (disjoint_out, INV_DA_OVER_SA, ZERO)
RVV_MAKE_PD_COMBINERS (disjoint_out_reverse, ZERO, INV_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (disjoint_atop, ONE_MINUS_INV_DA_OVER_SA, INV_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (disjoint_atop_reverse,
		       INV_DA_OVER_SA,
		       ONE_MINUS_INV_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (disjoint_xor, INV_DA_OVER_SA, INV_SA_OVER_DA)

RVV_MAKE_PD_COMBINERS (conjoint_clear, ZERO, ZERO)
RVV_MAKE_PD_COMBINERS (conjoint_src, ONE, ZERO)
RVV_MAKE_PD_COMBINERS (conjoint_dst, ZERO, ONE)
RVV_MAKE_PD_COMBINERS (conjoint_over, ONE, ONE_MINUS_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (conjoint_over_reverse, ONE_MINUS_DA_OVER_SA, ONE)
RVV_MAKE_PD_COMBINERS (conjoint_in, DA_OVER_SA, ZERO)
RVV_MAKE_PD_COMBINERS (conjoint_in_reverse, ZERO, SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (conjoint_out, ONE_MINUS_DA_OVER_SA, ZERO)
RVV_MAKE_PD_COMBINERS (conjoint_out_reverse, ZERO, ONE_MINUS_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (conjoint_atop, DA_OVER_SA, ONE_MINUS_SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (conjoint_atop_reverse, ONE_MINUS_DA_OVER_SA, SA_OVER_DA)
RVV_MAKE_PD_COMBINERS (conjoint_xor, ONE_MINUS_DA_OVER_SA, ONE_MINUS_SA_OVER_DA)

#define RVV_MAKE_SEPARABLE_PDF_COMBINERS(name)                                 \
    static force_inline vfloat32m1_t rvv_combine_##name##_a (                  \
	vfloat32m1_t sa, vfloat32m1_t s, vfloat32m1_t da, vfloat32m1_t d,      \
	size_t vl)                                                             \
    {                                                                          \
	return __riscv_vfsub_vv_f32m1 (__riscv_vfadd_vv_f32m1 (da, sa, vl),    \
				       __riscv_vfmul_vv_f32m1 (da, sa, vl),    \
				       vl);                                    \
    }                                                                          \
                                                                               \
    static force_inline vfloat32m1_t rvv_combine_##name##_c (                  \
	vfloat32m1_t sa, vfloat32m1_t s, vfloat32m1_t da, vfloat32m1_t d,      \
	size_t vl)                                                             \
    {                                                                          \
	vfloat32m1_t f = __riscv_vfmul_vf_f32m1 (                              \
	    __riscv_vfadd_vv_f32m1 (                                           \
		__riscv_vfmul_vv_f32m1 (__riscv_vfsub_vf_f32m1 (sa, 1.0f, vl), \
					d, vl),                                \
		__riscv_vfmul_vv_f32m1 (__riscv_vfsub_vf_f32m1 (da, 1.0f, vl), \
					s, vl),                                \
		vl),                                                           \
	    -1.0f, vl);                                                        \
                                                                               \
	return __riscv_vfadd_vv_f32m1 (                                        \
	    f, rvv_blend_##name##_float (sa, s, da, d, vl), vl);               \
    }                                                                          \
                                                                               \
    RVV_MAKE_COMBINERS (name, rvv_combine_##name##_a, rvv_combine_##name##_c)

RVV_MAKE_SEPARABLE_PDF_COMBINERS (multiply)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (screen)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (overlay)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (darken)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (lighten)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (color_dodge)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (color_burn)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (hard_light)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (soft_light)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (difference)
RVV_MAKE_SEPARABLE_PDF_COMBINERS (exclusion)

// int implementation

// pixman-combine32.h RVV implementation plus some convenience functions {

/*
 * x_c = min(x_c + y_c, 255)
 */

#define rvv_UN8_ADD_UN8_vv(x, y, vl) __riscv_vsaddu (x, y, vl)

#define rvv_UN8x4_ADD_UN8x4_vv_m4(x, y, vl)                                    \
    RVV_U8x4_U32_m4 (rvv_UN8_ADD_UN8_vv (RVV_U32_U8x4_m4 (x),                  \
					 RVV_U32_U8x4_m4 (y), (vl) * 4))

/*
* x_c = (x_c * a_c) / 255
*/

#define __rvv_UN8_MUL_UN8_vv(LMUL, LMUL16)                                     \
    static force_inline VUINT8 (LMUL) rvv_UN8_MUL_UN8_vv_##LMUL (              \
	const VUINT8 (LMUL) x, const VUINT8 (LMUL) a, size_t vl)               \
    {                                                                          \
	VUINT16 (LMUL16)                                                       \
	mul_higher = __riscv_vwmaccu (                                         \
	    __riscv_vmv_v_x_u16##LMUL16 (ONE_HALF, vl), x, a, vl);             \
                                                                               \
	VUINT16 (LMUL16)                                                       \
	mul_lower = __riscv_vsrl (mul_higher, G_SHIFT, vl);                    \
                                                                               \
	return __riscv_vnsrl (__riscv_vadd (mul_higher, mul_lower, vl),        \
			      G_SHIFT, vl);                                    \
    }
__rvv_UN8_MUL_UN8_vv (m1, m2);
__rvv_UN8_MUL_UN8_vv (m2, m4);
__rvv_UN8_MUL_UN8_vv (m4, m8);

static force_inline vuint8m4_t
rvv_UN8_MUL_UN8_vx_m4 (const vuint8m4_t x, const uint8_t a, size_t vl)
{
    vuint16m8_t mul_higher = __riscv_vwmaccu (
	__riscv_vmv_v_x_u16m8 (ONE_HALF, vl), a, x, vl);
    vuint16m8_t mul_lower = __riscv_vsrl (mul_higher, G_SHIFT, vl);

    return __riscv_vnsrl (__riscv_vadd (mul_higher, mul_lower, vl), G_SHIFT,
			  vl);
}

#define __rvv_UN8x4_MUL_UN8x4_vv(LMUL, x, a, vl)                               \
    RVV_U8x4_U32 (LMUL, rvv_UN8_MUL_UN8_vv_##LMUL (RVV_U32_U8x4 (LMUL, x),     \
						   RVV_U32_U8x4 (LMUL, a),     \
						   (vl) * 4))
#define rvv_UN8x4_MUL_UN8x4_vv_m2(x, a, vl)                                    \
    __rvv_UN8x4_MUL_UN8x4_vv (m2, x, a, vl)
#define rvv_UN8x4_MUL_UN8x4_vv_m4(x, a, vl)                                    \
    __rvv_UN8x4_MUL_UN8x4_vv (m4, x, a, vl)

/*
* a_c = a (broadcast to all components)
*/

#define __rvv_UN16_bcast_UN8x4_v(LMUL, LMUL16)                                 \
    static force_inline VUINT32 (LMUL)                                         \
	rvv_UN16_bcast_UN8x4_v_##LMUL (const VUINT16 (LMUL16) a, size_t vl)    \
    {                                                                          \
	VUINT32 (LMUL)                                                         \
	a32 = __riscv_vwcvtu_x (__riscv_vmadd (a, 1 << 8, a, vl), vl);         \
                                                                               \
	return __riscv_vmadd (a32, 1 << 16, a32, vl);                          \
    }
__rvv_UN16_bcast_UN8x4_v (m2, m1);
__rvv_UN16_bcast_UN8x4_v (m4, m2);

#define rvv_UN8_bcast_UN8x4_v_m4(a, vl)                                        \
    rvv_UN16_bcast_UN8x4_v_m4 (__riscv_vwcvtu_x (a, vl), vl)

/*
* x_c = (x_c * a) / 255
*/

#define rvv_UN8x4_MUL_UN8_vv_m4(x, a, vl)                                      \
    rvv_UN8x4_MUL_UN8x4_vv_m4 (x, rvv_UN8_bcast_UN8x4_v_m4 (a, vl), vl)

#define __rvv_UN8x4_MUL_UN16_vv(LMUL, x, a, vl)                                \
    rvv_UN8x4_MUL_UN8x4_vv_##LMUL (x, rvv_UN16_bcast_UN8x4_v_##LMUL (a, vl), vl)
#define rvv_UN8x4_MUL_UN16_vv_m2(x, a, vl)                                     \
    __rvv_UN8x4_MUL_UN16_vv (m2, x, a, vl)
#define rvv_UN8x4_MUL_UN16_vv_m4(x, a, vl)                                     \
    __rvv_UN8x4_MUL_UN16_vv (m4, x, a, vl)

#define rvv_UN8x4_MUL_UN8_vx_m4(x, a, vl)                                      \
    RVV_U8x4_U32_m4 (rvv_UN8_MUL_UN8_vx_m4 (RVV_U32_U8x4_m4 (x), a, (vl) * 4))

static force_inline vuint32m2_t
rvv_DIV_ONE_UN32m2_UN32m2_v (const vuint32m2_t x, size_t vl)
{
    vuint32m2_t mul_higher = __riscv_vadd (x, ONE_HALF, vl);
    vuint32m2_t mul_lower  = __riscv_vsrl (mul_higher, G_SHIFT, vl);

    return __riscv_vsrl (__riscv_vadd (mul_higher, mul_lower, vl), G_SHIFT, vl);
}

static force_inline vuint8m2_t
rvv_DIV_ONE_UN32m8_UN8m2_v (const vuint32m8_t x, size_t vl)
{
    vuint32m8_t mul_higher = __riscv_vadd (x, ONE_HALF, vl);
    vuint32m8_t mul_lower  = __riscv_vsrl (mul_higher, G_SHIFT, vl);

    return __riscv_vncvt_x (
	__riscv_vnsrl (__riscv_vadd (mul_higher, mul_lower, vl), G_SHIFT, vl),
	vl);
}

/*
* x_c = (x_c * a) / 255 + y_c
*/

#define rvv_UN8x4_MUL_UN16_ADD_UN8x4_vvv_m4(x, a, y, vl)                       \
    rvv_UN8x4_ADD_UN8x4_vv_m4 (rvv_UN8x4_MUL_UN16_vv_m4 (x, a, vl), y, vl)

/*
* x_c = (x_c * a + y_c * b) / 255
*/

#define rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4(x, a, y, b, vl)          \
    rvv_UN8x4_ADD_UN8x4_vv_m4 (rvv_UN8x4_MUL_UN16_vv_m4 (x, a, vl),            \
			       rvv_UN8x4_MUL_UN16_vv_m4 (y, b, vl), vl)

/*
* x_c = (x_c * a_c) / 255 + y_c
*/

#define rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_vvv_m4(x, a, y, vl)                      \
    rvv_UN8x4_ADD_UN8x4_vv_m4 (rvv_UN8x4_MUL_UN8x4_vv_m4 (x, a, vl), y, vl)

/*
* x_c = (x_c * a_c + y_c * b) / 255
*/

#define rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN16_vvvv_m4(x, a, y, b, vl)         \
    rvv_UN8x4_ADD_UN8x4_vv_m4 (rvv_UN8x4_MUL_UN8x4_vv_m4 (x, a, vl),           \
			       rvv_UN8x4_MUL_UN16_vv_m4 (y, b, vl), vl)

// } pixman-combine32.h

// Additional functions.

#define rvv_shift_alpha_u16(x, vl) __riscv_vnsrl (x, 24, vl)

#define rvv_shift_not_alpha_u16(x, vl)                                         \
    rvv_shift_alpha_u16 (__riscv_vnot (x, vl), vl)

#define rvv_load_alpha_u8m1(src, vl)                                           \
    __riscv_vlse8_v_u8m1 ((uint8_t *)src + 3, 4, vl)

#define rvv_load_not_alpha_u8m1(src, vl)                                       \
    __riscv_vnot (rvv_load_alpha_u8m1 (src, vl), vl)

#define rvv_u8m2_to_i16m4(in, vl)                                              \
    __riscv_vreinterpret_i16m4 (__riscv_vwcvtu_x (in, vl))

#define rvv_over_m4(src, dest, vl)                                             \
    rvv_UN8x4_MUL_UN16_ADD_UN8x4_vvv_m4 (                                      \
	dest, rvv_shift_not_alpha_u16 (src, vl), src, vl)

#define rvv_in_m4(x, y, vl) rvv_UN8x4_MUL_UN8_vv_m4 (x, y, vl)

#define rvv_in_load_s_m_m4(src, mask, vl)                                      \
    rvv_in_m4 (__riscv_vle32_v_u32m4 (src, vl),                                \
	       rvv_load_alpha_u8m1 (mask, vl), vl)

#define rvv_in_load_s_nm_m4(src, mask, vl)                                     \
    rvv_in_m4 (__riscv_vle32_v_u32m4 (src, vl),                                \
	       rvv_load_not_alpha_u8m1 (mask, vl), vl)

static force_inline vuint16m2_t
rvv_convert_8888_to_0565_m2 (const vuint32m4_t s, size_t vl)
{
    vuint32m4_t rb = __riscv_vand (s, 0xF800F8, vl);

    return __riscv_vor (
	__riscv_vor (__riscv_vnsrl (rb, 3, vl), __riscv_vnsrl (rb, 8, vl), vl),
	__riscv_vand (__riscv_vnsrl (s, 5, vl), 0x7E0, vl), vl);
}

static force_inline vuint32m4_t
rvv_convert_0565_to_0888_m4 (const vuint16m2_t s, size_t vl)
{
    vuint8m1_t  g1, g2;
    vuint16m2_t r, g_w, b;
    vuint32m4_t r_w, rb_w;

    r    = __riscv_vand (s, 0xF800, vl);
    b    = __riscv_vand (s, 0x001F, vl);
    r_w  = __riscv_vwmulu (r, 1 << 8, vl);
    rb_w = __riscv_vwmaccu (r_w, 1 << 3, b, vl);
    rb_w = __riscv_vand (__riscv_vor (rb_w, __riscv_vsrl (rb_w, 5, vl), vl),
			 0xFF00FF, vl);

    g1  = __riscv_vsll (__riscv_vnsrl (s, 5, vl), 2, vl);
    g2  = __riscv_vsrl (g1, 6, vl);
    g_w = __riscv_vwaddu_vv (g1, g2, vl);

    return __riscv_vwmaccu (rb_w, 1 << 8, g_w, vl);
}

#define rvv_convert_0565_to_8888_m4(s, vl)                                     \
    __riscv_vor (rvv_convert_0565_to_0888_m4 (s, vl), 0xff000000, vl)

#define __rvv_combine_mask_value_ca(LMUL, src, mask, vl)                       \
    rvv_UN8x4_MUL_UN8x4_vv_##LMUL (src, mask, vl)
#define rvv_combine_mask_value_ca_m2(src, mask, vl)                            \
    __rvv_combine_mask_value_ca (m2, src, mask, vl)
#define rvv_combine_mask_value_ca_m4(src, mask, vl)                            \
    __rvv_combine_mask_value_ca (m4, src, mask, vl)

#define __rvv_combine_mask_alpha_ca(LMUL, src, mask, vl)                       \
    rvv_UN8x4_MUL_UN16_vv_##LMUL (mask, rvv_shift_alpha_u16 (src, vl), vl)
#define rvv_combine_mask_alpha_ca_m2(src, mask, vl)                            \
    __rvv_combine_mask_alpha_ca (m2, src, mask, vl)
#define rvv_combine_mask_alpha_ca_m4(src, mask, vl)                            \
    __rvv_combine_mask_alpha_ca (m4, src, mask, vl)

#define __rvv_combine_mask(LMUL, src, mask, vl)                                \
    rvv_UN8x4_MUL_UN16_vv_##LMUL (src, rvv_shift_alpha_u16 (mask, vl), vl)
#define rvv_combine_mask_m2(src, mask, vl)                                     \
    __rvv_combine_mask (m2, src, mask, vl)
#define rvv_combine_mask_m4(src, mask, vl)                                     \
    __rvv_combine_mask (m4, src, mask, vl)

#define __rvv_combine_mask_ca(LMUL)                                            \
    static force_inline void rvv_combine_mask_ca_##LMUL (                      \
	VUINT32 (LMUL) *__restrict__ src, VUINT32 (LMUL) *__restrict__ mask,   \
	size_t vl)                                                             \
    {                                                                          \
	VUINT32 (LMUL) src_cpy = *src;                                         \
	*(src)  = rvv_combine_mask_value_ca_##LMUL (*(src), *(mask), vl);      \
	*(mask) = rvv_combine_mask_alpha_ca_##LMUL (src_cpy, *(mask), vl);     \
    }
__rvv_combine_mask_ca (m2);
__rvv_combine_mask_ca (m4);

static void
rvv_combine_clear (pixman_implementation_t *__restrict__ imp,
		   pixman_op_t op,
		   uint32_t *__restrict__ dest,
		   const uint32_t *__restrict__ src,
		   const uint32_t *__restrict__ mask,
		   int width)
{
    uint32_t *pd = dest;

    vuint32m8_t v = __riscv_vmv_v_x_u32m8 (0, __riscv_vsetvlmax_e32m8 ());
    RVV_FOREACH_1 (width, vl, e32m8, pd) { __riscv_vse32 (pd, v, vl); }
}

static void
rvv_combine_src_u (pixman_implementation_t *__restrict__ imp,
		   pixman_op_t op,
		   uint32_t *__restrict__ dest,
		   const uint32_t *__restrict__ src,
		   const uint32_t *__restrict__ mask,
		   int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (pd, rvv_in_load_s_m_m4 (ps, pm, vl), vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m8, ps, pd)
	{
	    __riscv_vse32 (pd, __riscv_vle32_v_u32m8 (ps, vl), vl);
	}
    }
}

static void
rvv_combine_over_u (pixman_implementation_t *__restrict__ imp,
		    pixman_op_t op,
		    uint32_t *__restrict__ dest,
		    const uint32_t *__restrict__ src,
		    const uint32_t *__restrict__ mask,
		    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (pd,
			   rvv_over_m4 (rvv_in_load_s_m_m4 (ps, pm, vl),
					__riscv_vle32_v_u32m4 (pd, vl), vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    __riscv_vse32 (pd,
			   rvv_over_m4 (__riscv_vle32_v_u32m4 (ps, vl),
					__riscv_vle32_v_u32m4 (pd, vl), vl),
			   vl);
	}
    }
}

static void
rvv_combine_over_reverse_u (pixman_implementation_t *__restrict__ imp,
			    pixman_op_t op,
			    uint32_t *__restrict__ dest,
			    const uint32_t *__restrict__ src,
			    const uint32_t *__restrict__ mask,
			    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (pd,
			   rvv_over_m4 (__riscv_vle32_v_u32m4 (pd, vl),
					rvv_in_load_s_m_m4 (ps, pm, vl), vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    __riscv_vse32 (pd,
			   rvv_over_m4 (__riscv_vle32_v_u32m4 (pd, vl),
					__riscv_vle32_v_u32m4 (ps, vl), vl),
			   vl);
	}
    }
}

static void
rvv_combine_in_u (pixman_implementation_t *__restrict__ imp,
		  pixman_op_t op,
		  uint32_t *__restrict__ dest,
		  const uint32_t *__restrict__ src,
		  const uint32_t *__restrict__ mask,
		  int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (pd,
			   rvv_in_m4 (rvv_in_load_s_m_m4 (ps, pm, vl),
				      rvv_load_alpha_u8m1 (pd, vl), vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    __riscv_vse32 (pd, rvv_in_load_s_m_m4 (ps, pd, vl), vl);
	}
    }
}

static void
rvv_combine_in_reverse_u (pixman_implementation_t *__restrict__ imp,
			  pixman_op_t op,
			  uint32_t *__restrict__ dest,
			  const uint32_t *__restrict__ src,
			  const uint32_t *__restrict__ mask,
			  int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (pd,
			   rvv_in_m4 (__riscv_vle32_v_u32m4 (pd, vl),
				      rvv_UN8_MUL_UN8_vv_m1 (
					  rvv_load_alpha_u8m1 (ps, vl),
					  rvv_load_alpha_u8m1 (pm, vl), vl),
				      vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    __riscv_vse32 (pd, rvv_in_load_s_m_m4 (pd, ps, vl), vl);
	}
    }
}

static void
rvv_combine_out_u (pixman_implementation_t *__restrict__ imp,
		   pixman_op_t op,
		   uint32_t *__restrict__ dest,
		   const uint32_t *__restrict__ src,
		   const uint32_t *__restrict__ mask,
		   int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (pd,
			   rvv_in_m4 (rvv_in_load_s_m_m4 (ps, pm, vl),
				      rvv_load_not_alpha_u8m1 (pd, vl), vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    __riscv_vse32 (pd, rvv_in_load_s_nm_m4 (ps, pd, vl), vl);
	}
    }
}

static void
rvv_combine_out_reverse_u (pixman_implementation_t *__restrict__ imp,
			   pixman_op_t op,
			   uint32_t *__restrict__ dest,
			   const uint32_t *__restrict__ src,
			   const uint32_t *__restrict__ mask,
			   int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (
		pd,
		rvv_in_m4 (__riscv_vle32_v_u32m4 (pd, vl),
			   __riscv_vnot (rvv_UN8_MUL_UN8_vv_m1 (
					     rvv_load_alpha_u8m1 (ps, vl),
					     rvv_load_alpha_u8m1 (pm, vl), vl),
					 vl),
			   vl),
		vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    __riscv_vse32 (pd, rvv_in_load_s_nm_m4 (pd, ps, vl), vl);
	}
    }
}

static void
rvv_combine_atop_u (pixman_implementation_t *__restrict__ imp,
		    pixman_op_t op,
		    uint32_t *__restrict__ dest,
		    const uint32_t *__restrict__ src,
		    const uint32_t *__restrict__ mask,
		    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;
    vuint32m4_t s, d;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    s = rvv_in_load_s_m_m4 (ps, pm, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);
	    __riscv_vse32 (pd,
			   rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			       s, rvv_shift_alpha_u16 (d, vl), d,
			       rvv_shift_not_alpha_u16 (s, vl), vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    s = __riscv_vle32_v_u32m4 (ps, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);
	    __riscv_vse32 (pd,
			   rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			       s, rvv_shift_alpha_u16 (d, vl), d,
			       rvv_shift_not_alpha_u16 (s, vl), vl),
			   vl);
	}
    }
}

static void
rvv_combine_atop_reverse_u (pixman_implementation_t *__restrict__ imp,
			    pixman_op_t op,
			    uint32_t *__restrict__ dest,
			    const uint32_t *__restrict__ src,
			    const uint32_t *__restrict__ mask,
			    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;
    vuint32m4_t s, d;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    s = rvv_in_load_s_m_m4 (ps, pm, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);
	    __riscv_vse32 (pd,
			   rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			       s, rvv_shift_not_alpha_u16 (d, vl), d,
			       rvv_shift_alpha_u16 (s, vl), vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    s = __riscv_vle32_v_u32m4 (ps, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);
	    __riscv_vse32 (pd,
			   rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			       s, rvv_shift_not_alpha_u16 (d, vl), d,
			       rvv_shift_alpha_u16 (s, vl), vl),
			   vl);
	}
    }
}

static void
rvv_combine_xor_u (pixman_implementation_t *__restrict__ imp,
		   pixman_op_t op,
		   uint32_t *__restrict__ dest,
		   const uint32_t *__restrict__ src,
		   const uint32_t *__restrict__ mask,
		   int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;
    vuint32m4_t s, d;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    s = rvv_in_load_s_m_m4 (ps, pm, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);
	    __riscv_vse32 (pd,
			   rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			       s, rvv_shift_not_alpha_u16 (d, vl), d,
			       rvv_shift_not_alpha_u16 (s, vl), vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    s = __riscv_vle32_v_u32m4 (ps, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);
	    __riscv_vse32 (pd,
			   rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			       s, rvv_shift_not_alpha_u16 (d, vl), d,
			       rvv_shift_not_alpha_u16 (s, vl), vl),
			   vl);
	}
    }
}

static void
rvv_combine_add_u (pixman_implementation_t *__restrict__ imp,
		   pixman_op_t op,
		   uint32_t *__restrict__ dest,
		   const uint32_t *__restrict__ src,
		   const uint32_t *__restrict__ mask,
		   int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    __riscv_vse32 (
		pd,
		rvv_UN8x4_ADD_UN8x4_vv_m4 (__riscv_vle32_v_u32m4 (pd, vl),
					   rvv_in_load_s_m_m4 (ps, pm, vl), vl),
		vl);
	}
    }
    else
    {
	RVV_FOREACH_2 (width, vl, e32m4, ps, pd)
	{
	    __riscv_vse32 (
		pd,
		rvv_UN8x4_ADD_UN8x4_vv_m4 (__riscv_vle32_v_u32m4 (pd, vl),
					   __riscv_vle32_v_u32m4 (ps, vl), vl),
		vl);
	}
    }
}

/*
 * Multiply
 *
 *      ad * as * B(d / ad, s / as)
 *    = ad * as * d/ad * s/as
 *    = d * s
 *
 */
static void
rvv_combine_multiply_u (pixman_implementation_t *imp,
			pixman_op_t              op,
			uint32_t *__restrict__ dest,
			const uint32_t *__restrict__ src,
			const uint32_t *__restrict__ mask,
			int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    vuint32m4_t s, d;
    if (mask)
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    s = rvv_in_load_s_m_m4 (ps, pm, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);

	    __riscv_vse32 (pd,
			   rvv_UN8x4_ADD_UN8x4_vv_m4 (
			       rvv_UN8x4_MUL_UN8x4_vv_m4 (d, s, vl),
			       rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
				   s, rvv_shift_not_alpha_u16 (d, vl), d,
				   rvv_shift_not_alpha_u16 (s, vl), vl),
			       vl),
			   vl);
	}
    }
    else
    {
	RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
	{
	    s = __riscv_vle32_v_u32m4 (ps, vl);
	    d = __riscv_vle32_v_u32m4 (pd, vl);

	    __riscv_vse32 (pd,
			   rvv_UN8x4_ADD_UN8x4_vv_m4 (
			       rvv_UN8x4_MUL_UN8x4_vv_m4 (d, s, vl),
			       rvv_UN8x4_MUL_UN16_ADD_UN8x4_MUL_UN16_vvvv_m4 (
				   s, rvv_shift_not_alpha_u16 (d, vl), d,
				   rvv_shift_not_alpha_u16 (s, vl), vl),
			       vl),
			   vl);
	}
    }
}

static void
rvv_combine_multiply_ca (pixman_implementation_t *__restrict__ imp,
			 pixman_op_t op,
			 uint32_t *__restrict__ dest,
			 const uint32_t *__restrict__ src,
			 const uint32_t *__restrict__ mask,
			 int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    vuint32m4_t s, m, d;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	s = __riscv_vle32_v_u32m4 (ps, vl);
	m = __riscv_vle32_v_u32m4 (pm, vl);
	rvv_combine_mask_ca_m4 (&s, &m, vl);

	d = __riscv_vle32_v_u32m4 (pd, vl);

	__riscv_vse32 (pd,
		       rvv_UN8x4_ADD_UN8x4_vv_m4 (
			   rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			       d, __riscv_vnot (m, vl), s,
			       rvv_shift_not_alpha_u16 (d, vl), vl),
			   rvv_UN8x4_MUL_UN8x4_vv_m4 (d, s, vl), vl),
		       vl);
    }
}

#define PDF_SEPARABLE_BLEND_MODE(name)                                         \
    static void rvv_combine_##name##_u (                                       \
	pixman_implementation_t *imp, pixman_op_t op, uint32_t *dest,          \
	const uint32_t *src, const uint32_t *mask, int width)                  \
    {                                                                          \
	uint32_t *__restrict__ pd       = dest;                                \
	const uint32_t *__restrict__ ps = src;                                 \
	const uint32_t *__restrict__ pm = mask;                                \
                                                                               \
	vuint32m2_t s, d, ra, rx;                                              \
	vuint16m1_t da, sa;                                                    \
	size_t      vl4;                                                       \
	vuint8m2_t  s4, d4, sa4, isa4, da4, ida4;                              \
	vuint32m8_t rx4;                                                       \
                                                                               \
	RVV_FOREACH_3 (width, vl, e32m2, ps, pm, pd)                           \
	{                                                                      \
	    vl4 = vl * 4;                                                      \
                                                                               \
	    s = __riscv_vle32_v_u32m2 (ps, vl);                                \
	    if (mask)                                                          \
		s = rvv_combine_mask_m2 (s, __riscv_vle32_v_u32m2 (pm, vl),    \
					 vl);                                  \
	    sa = rvv_shift_alpha_u16 (s, vl);                                  \
                                                                               \
	    d  = __riscv_vle32_v_u32m2 (pd, vl);                               \
	    da = rvv_shift_alpha_u16 (d, vl);                                  \
                                                                               \
	    ra = __riscv_vsub (__riscv_vwaddu_vv (__riscv_vmul (da, 0xFF, vl), \
						  __riscv_vmul (sa, 0xFF, vl), \
						  vl),                         \
			       __riscv_vwmulu (sa, da, vl), vl);               \
                                                                               \
	    s4   = RVV_U32_U8x4_m2 (s);                                        \
	    sa4  = RVV_U32_U8x4_m2 (rvv_UN16_bcast_UN8x4_v_m2 (sa, vl));       \
	    isa4 = __riscv_vnot (sa4, vl4);                                    \
	    d4   = RVV_U32_U8x4_m2 (d);                                        \
	    da4  = RVV_U32_U8x4_m2 (rvv_UN16_bcast_UN8x4_v_m2 (da, vl));       \
	    ida4 = __riscv_vnot (da4, vl4);                                    \
                                                                               \
	    rx4 = __riscv_vadd (                                               \
		__riscv_vwaddu_vv (__riscv_vwmulu (isa4, d4, vl4),             \
				   __riscv_vwmulu (ida4, s4, vl4), vl4),       \
		rvv_blend_##name##_int (d4, da4, s4, sa4, vl4), vl4);          \
                                                                               \
	    ra  = __riscv_vminu (ra, 255 * 255, vl);                           \
	    rx4 = __riscv_vminu (rx4, 255 * 255, vl4);                         \
                                                                               \
	    ra = rvv_DIV_ONE_UN32m2_UN32m2_v (ra, vl);                         \
	    rx = RVV_U8x4_U32_m2 (rvv_DIV_ONE_UN32m8_UN8m2_v (rx4, vl4));      \
                                                                               \
	    __riscv_vse32 (pd,                                                 \
			   __riscv_vor (__riscv_vsll (ra, 24, vl),             \
					__riscv_vand (rx, 0x00FFFFFF, vl),     \
					vl),                                   \
			   vl);                                                \
	}                                                                      \
    }                                                                          \
                                                                               \
    static void rvv_combine_##name##_ca (                                      \
	pixman_implementation_t *imp, pixman_op_t op, uint32_t *dest,          \
	const uint32_t *src, const uint32_t *mask, int width)                  \
    {                                                                          \
	uint32_t *__restrict__ pd       = dest;                                \
	const uint32_t *__restrict__ ps = src;                                 \
	const uint32_t *__restrict__ pm = mask;                                \
                                                                               \
	vuint32m2_t s, m, d, ra, rx;                                           \
	vuint16m1_t da, sa;                                                    \
	size_t      vl4;                                                       \
	vuint8m2_t  s4, m4, d4, ixa4, da4, ida4;                               \
	vuint32m8_t rx4;                                                       \
                                                                               \
	RVV_FOREACH_3 (width, vl, e32m2, ps, pm, pd)                           \
	{                                                                      \
	    m = __riscv_vle32_v_u32m2 (pm, vl);                                \
	    s = __riscv_vle32_v_u32m2 (ps, vl);                                \
	    rvv_combine_mask_ca_m2 (&s, &m, vl);                               \
	    sa = rvv_shift_alpha_u16 (s, vl);                                  \
                                                                               \
	    d  = __riscv_vle32_v_u32m2 (pd, vl);                               \
	    da = rvv_shift_alpha_u16 (d, vl);                                  \
                                                                               \
	    ra = __riscv_vsub (__riscv_vwaddu_vv (__riscv_vmul (da, 0xFF, vl), \
						  __riscv_vmul (sa, 0xFF, vl), \
						  vl),                         \
			       __riscv_vwmulu (sa, da, vl), vl);               \
                                                                               \
	    ixa4 = RVV_U32_U8x4_m2 (__riscv_vnot (m, vl));                     \
	    d4   = RVV_U32_U8x4_m2 (d);                                        \
	    ida4 = RVV_U32_U8x4_m2 (                                           \
		__riscv_vnot (rvv_UN16_bcast_UN8x4_v_m2 (da, vl), vl));        \
	    s4  = RVV_U32_U8x4_m2 (s);                                         \
	    da4 = RVV_U32_U8x4_m2 (rvv_UN16_bcast_UN8x4_v_m2 (da, vl));        \
	    m4  = RVV_U32_U8x4_m2 (m);                                         \
                                                                               \
	    vl4 = vl * 4;                                                      \
	    rx4 = __riscv_vadd (                                               \
		__riscv_vwaddu_vv (__riscv_vwmulu (ixa4, d4, vl4),             \
				   __riscv_vwmulu (ida4, s4, vl4), vl4),       \
		rvv_blend_##name##_int (d4, da4, s4, m4, vl4), vl4);           \
                                                                               \
	    ra  = __riscv_vminu (ra, 255 * 255, vl);                           \
	    rx4 = __riscv_vminu (rx4, 255 * 255, vl4);                         \
                                                                               \
	    ra = rvv_DIV_ONE_UN32m2_UN32m2_v (ra, vl);                         \
	    rx = RVV_U8x4_U32_m2 (rvv_DIV_ONE_UN32m8_UN8m2_v (rx4, vl4));      \
                                                                               \
	    __riscv_vse32 (pd,                                                 \
			   __riscv_vor (__riscv_vsll (ra, 24, vl),             \
					__riscv_vand (rx, 0x00FFFFFF, vl),     \
					vl),                                   \
			   vl);                                                \
	}                                                                      \
    }

static force_inline vuint32m8_t
rvv_blend_screen_int (const vuint8m2_t d,
		      const vuint8m2_t ad,
		      const vuint8m2_t s,
		      const vuint8m2_t as,
		      size_t           vl)
{
    return __riscv_vsub (__riscv_vwaddu_vv (__riscv_vwmulu (s, ad, vl),
					    __riscv_vwmulu (d, as, vl), vl),
			 __riscv_vwcvtu_x (__riscv_vwmulu (s, d, vl), vl), vl);
}

PDF_SEPARABLE_BLEND_MODE (screen)

static force_inline vuint32m8_t
_rvv_blend_overlay_hard_light (const vuint8m2_t d,
			       const vuint8m2_t ad,
			       const vuint8m2_t s,
			       const vuint8m2_t as,
			       const vbool4_t   selector,
			       size_t           vl)
{
    vuint32m8_t out_true = __riscv_vwmulu (__riscv_vwmulu (s, d, vl), 2, vl);

    vint16m4_t d_i  = rvv_u8m2_to_i16m4 (d, vl);
    vint16m4_t ad_i = rvv_u8m2_to_i16m4 (ad, vl);
    vint16m4_t s_i  = rvv_u8m2_to_i16m4 (s, vl);
    vint16m4_t as_i = rvv_u8m2_to_i16m4 (as, vl);

    vuint32m8_t out_false = __riscv_vreinterpret_v_i32m8_u32m8 (__riscv_vsub (
	__riscv_vwmul (as_i, ad_i, vl),
	__riscv_vsll (__riscv_vwmul (__riscv_vsub (ad_i, d_i, vl),
				     __riscv_vsub (as_i, s_i, vl), vl),
		      1, vl),
	vl));

    return __riscv_vmerge (out_false, out_true, selector, vl);
}

static force_inline vuint32m8_t
rvv_blend_overlay_int (const vuint8m2_t d,
		       const vuint8m2_t ad,
		       const vuint8m2_t s,
		       const vuint8m2_t as,
		       size_t           vl)
{
    return _rvv_blend_overlay_hard_light (
	d, ad, s, as,
	__riscv_vmsltu (__riscv_vwmulu (d, 2, vl), __riscv_vwcvtu_x (ad, vl),
			vl),
	vl);
}

PDF_SEPARABLE_BLEND_MODE (overlay)

static force_inline vuint32m8_t
rvv_blend_darken_int (const vuint8m2_t d,
		      const vuint8m2_t ad,
		      const vuint8m2_t s,
		      const vuint8m2_t as,
		      size_t           vl)
{
    return __riscv_vwcvtu_x (__riscv_vminu (__riscv_vwmulu (ad, s, vl),
					    __riscv_vwmulu (as, d, vl), vl),
			     vl);
}

PDF_SEPARABLE_BLEND_MODE (darken)

static force_inline vuint32m8_t
rvv_blend_lighten_int (const vuint8m2_t d,
		       const vuint8m2_t ad,
		       const vuint8m2_t s,
		       const vuint8m2_t as,
		       size_t           vl)
{
    return __riscv_vwcvtu_x (__riscv_vmaxu (__riscv_vwmulu (as, d, vl),
					    __riscv_vwmulu (ad, s, vl), vl),
			     vl);
}

PDF_SEPARABLE_BLEND_MODE (lighten)

static force_inline vuint32m8_t
rvv_blend_hard_light_int (const vuint8m2_t d,
			  const vuint8m2_t ad,
			  const vuint8m2_t s,
			  const vuint8m2_t as,
			  size_t           vl)
{
    return _rvv_blend_overlay_hard_light (
	d, ad, s, as,
	__riscv_vmsltu (__riscv_vwmulu (s, 2, vl), __riscv_vwcvtu_x (as, vl),
			vl),
	vl);
}

PDF_SEPARABLE_BLEND_MODE (hard_light)

static force_inline vuint32m8_t
rvv_blend_difference_int (const vuint8m2_t d,
			  const vuint8m2_t ad,
			  const vuint8m2_t s,
			  const vuint8m2_t as,
			  size_t           vl)
{
    vuint16m4_t das = __riscv_vwmulu (d, as, vl);
    vuint16m4_t sad = __riscv_vwmulu (s, ad, vl);

    return __riscv_vmerge (__riscv_vwsubu_vv (sad, das, vl),
			   __riscv_vwsubu_vv (das, sad, vl),
			   __riscv_vmsltu (sad, das, vl), vl);
}

PDF_SEPARABLE_BLEND_MODE (difference)

static force_inline vuint32m8_t
rvv_blend_exclusion_int (const vuint8m2_t d,
			 const vuint8m2_t ad,
			 const vuint8m2_t s,
			 const vuint8m2_t as,
			 size_t           vl)
{
    return __riscv_vsub (__riscv_vwaddu_vv (__riscv_vwmulu (s, ad, vl),
					    __riscv_vwmulu (d, as, vl), vl),
			 __riscv_vwmulu (__riscv_vwmulu (d, s, vl), 2, vl), vl);
}

PDF_SEPARABLE_BLEND_MODE (exclusion)

#undef PDF_SEPARABLE_BLEND_MODE

static void
rvv_combine_over_ca (pixman_implementation_t *__restrict__ imp,
		     pixman_op_t op,
		     uint32_t *__restrict__ dest,
		     const uint32_t *__restrict__ src,
		     const uint32_t *__restrict__ mask,
		     int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    vuint32m4_t s, m;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	s = __riscv_vle32_v_u32m4 (ps, vl);
	m = __riscv_vle32_v_u32m4 (pm, vl);
	rvv_combine_mask_ca_m4 (&s, &m, vl);

	__riscv_vse32 (
	    pd,
	    rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_vvv_m4 (
		__riscv_vle32_v_u32m4 (pd, vl), __riscv_vnot (m, vl), s, vl),
	    vl);
    }
}

static void
rvv_combine_over_reverse_ca (pixman_implementation_t *__restrict__ imp,
			     pixman_op_t op,
			     uint32_t *__restrict__ dest,
			     const uint32_t *__restrict__ src,
			     const uint32_t *__restrict__ mask,
			     int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    vuint32m4_t d;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	d = __riscv_vle32_v_u32m4 (pd, vl);
	__riscv_vse32 (
	    pd,
	    rvv_UN8x4_MUL_UN16_ADD_UN8x4_vvv_m4 (
		rvv_UN8x4_MUL_UN8x4_vv_m4 (__riscv_vle32_v_u32m4 (ps, vl),
					   __riscv_vle32_v_u32m4 (pm, vl), vl),
		rvv_shift_not_alpha_u16 (d, vl), d, vl),
	    vl);
    }
}

static void
rvv_combine_atop_ca (pixman_implementation_t *__restrict__ imp,
		     pixman_op_t op,
		     uint32_t *__restrict__ dest,
		     const uint32_t *__restrict__ src,
		     const uint32_t *__restrict__ mask,
		     int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    vuint32m4_t d, s, m;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	s = __riscv_vle32_v_u32m4 (ps, vl);
	m = __riscv_vle32_v_u32m4 (pm, vl);
	rvv_combine_mask_ca_m4 (&s, &m, vl);

	d = __riscv_vle32_v_u32m4 (pd, vl);
	__riscv_vse32 (
	    pd,
	    rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN16_vvvv_m4 (
		d, __riscv_vnot (m, vl), s, rvv_shift_alpha_u16 (d, vl), vl),
	    vl);
    }
}

static void
rvv_combine_xor_ca (pixman_implementation_t *__restrict__ imp,
		    pixman_op_t op,
		    uint32_t *__restrict__ dest,
		    const uint32_t *__restrict__ src,
		    const uint32_t *__restrict__ mask,
		    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    vuint32m4_t d, s, m;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	s = __riscv_vle32_v_u32m4 (ps, vl);
	m = __riscv_vle32_v_u32m4 (pm, vl);
	rvv_combine_mask_ca_m4 (&s, &m, vl);

	d = __riscv_vle32_v_u32m4 (pd, vl);
	__riscv_vse32 (pd,
		       rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			   d, __riscv_vnot (m, vl), s,
			   rvv_shift_not_alpha_u16 (d, vl), vl),
		       vl);
    }
}

static void
rvv_combine_atop_reverse_ca (pixman_implementation_t *__restrict__ imp,
			     pixman_op_t op,
			     uint32_t *__restrict__ dest,
			     const uint32_t *__restrict__ src,
			     const uint32_t *__restrict__ mask,
			     int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    vuint32m4_t d, s, m;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	s = __riscv_vle32_v_u32m4 (ps, vl);
	m = __riscv_vle32_v_u32m4 (pm, vl);
	rvv_combine_mask_ca_m4 (&s, &m, vl);

	d = __riscv_vle32_v_u32m4 (pd, vl);
	__riscv_vse32 (pd,
		       rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_MUL_UN16_vvvv_m4 (
			   d, m, s, rvv_shift_not_alpha_u16 (d, vl), vl),
		       vl);
    }
}

static void
rvv_combine_src_ca (pixman_implementation_t *__restrict__ imp,
		    pixman_op_t op,
		    uint32_t *__restrict__ dest,
		    const uint32_t *__restrict__ src,
		    const uint32_t *__restrict__ mask,
		    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	__riscv_vse32 (
	    pd,
	    rvv_combine_mask_value_ca_m4 (__riscv_vle32_v_u32m4 (ps, vl),
					  __riscv_vle32_v_u32m4 (pm, vl), vl),
	    vl);
    }
}

static void
rvv_combine_in_ca (pixman_implementation_t *__restrict__ imp,
		   pixman_op_t op,
		   uint32_t *__restrict__ dest,
		   const uint32_t *__restrict__ src,
		   const uint32_t *__restrict__ mask,
		   int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	__riscv_vse32 (pd,
		       rvv_in_m4 (rvv_combine_mask_value_ca_m4 (
				      __riscv_vle32_v_u32m4 (ps, vl),
				      __riscv_vle32_v_u32m4 (pm, vl), vl),
				  rvv_load_alpha_u8m1 (pd, vl), vl),
		       vl);
    }
}

static void
rvv_combine_in_reverse_ca (pixman_implementation_t *imp,
			   pixman_op_t              op,
			   uint32_t                *dest,
			   const uint32_t          *src,
			   const uint32_t          *mask,
			   int                      width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	__riscv_vse32 (
	    pd,
	    rvv_UN8x4_MUL_UN8x4_vv_m4 (__riscv_vle32_v_u32m4 (pd, vl),
				       rvv_combine_mask_alpha_ca_m4 (
					   __riscv_vle32_v_u32m4 (ps, vl),
					   __riscv_vle32_v_u32m4 (pm, vl), vl),
				       vl),
	    vl);
    }
}

static void
rvv_combine_out_ca (pixman_implementation_t *__restrict__ imp,
		    pixman_op_t op,
		    uint32_t *__restrict__ dest,
		    const uint32_t *__restrict__ src,
		    const uint32_t *__restrict__ mask,
		    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	__riscv_vse32 (pd,
		       rvv_in_m4 (rvv_combine_mask_value_ca_m4 (
				      __riscv_vle32_v_u32m4 (ps, vl),
				      __riscv_vle32_v_u32m4 (pm, vl), vl),
				  rvv_load_not_alpha_u8m1 (pd, vl), vl),
		       vl);
    }
}

static void
rvv_combine_out_reverse_ca (pixman_implementation_t *imp,
			    pixman_op_t              op,
			    uint32_t                *dest,
			    const uint32_t          *src,
			    const uint32_t          *mask,
			    int                      width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	__riscv_vse32 (
	    pd,
	    rvv_UN8x4_MUL_UN8x4_vv_m4 (
		__riscv_vle32_v_u32m4 (pd, vl),
		__riscv_vnot_v_u32m4 (rvv_combine_mask_alpha_ca_m4 (
					  __riscv_vle32_v_u32m4 (ps, vl),
					  __riscv_vle32_v_u32m4 (pm, vl), vl),
				      vl),
		vl),
	    vl);
    }
}

static void
rvv_combine_add_ca (pixman_implementation_t *__restrict__ imp,
		    pixman_op_t op,
		    uint32_t *__restrict__ dest,
		    const uint32_t *__restrict__ src,
		    const uint32_t *__restrict__ mask,
		    int width)
{
    uint32_t *__restrict__ pd       = dest;
    const uint32_t *__restrict__ ps = src;
    const uint32_t *__restrict__ pm = mask;

    RVV_FOREACH_3 (width, vl, e32m4, ps, pm, pd)
    {
	__riscv_vse32 (
	    pd,
	    rvv_UN8x4_ADD_UN8x4_vv_m4 (__riscv_vle32_v_u32m4 (pd, vl),
				       rvv_combine_mask_value_ca_m4 (
					   __riscv_vle32_v_u32m4 (ps, vl),
					   __riscv_vle32_v_u32m4 (pm, vl), vl),
				       vl),
	    vl);
    }
}

static void
rvv_composite_src_x888_8888 (pixman_implementation_t *__restrict__ imp,
			     pixman_composite_info_t *__restrict__ info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *__restrict__ dst_line, *__restrict__ dst;
    uint32_t *__restrict__ src_line, *__restrict__ src;
    int32_t dst_stride, src_stride;
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride,
			   src_line, 1);
    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e32m8, src, dst)
	{
	    __riscv_vse32 (
		dst,
		__riscv_vor (__riscv_vle32_v_u32m8 (src, vl), 0xff000000, vl),
		vl);
	}
    }
}

static void
rvv_composite_src_8888_8888 (pixman_implementation_t *__restrict__ imp,
			     pixman_composite_info_t *__restrict__ info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *__restrict__ dst_line, *__restrict__ dst;
    uint32_t *__restrict__ src_line, *__restrict__ src;
    int32_t dst_stride, src_stride;
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride,
			   src_line, 1);
    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e32m8, src, dst)
	{
	    __riscv_vse32 (dst, __riscv_vle32_v_u32m8 (src, vl), vl);
	}
    }
}

static void
rvv_composite_over_x888_8_8888 (pixman_implementation_t *__restrict__ imp,
				pixman_composite_info_t *__restrict__ info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *__restrict__ src, *__restrict__ src_line;
    uint32_t *__restrict__ dst, *__restrict__ dst_line;
    uint8_t *__restrict__ mask, *__restrict__ mask_line;
    int32_t src_stride, mask_stride, dst_stride;
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride,
			   mask_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride,
			   src_line, 1);

    while (height--)
    {
	src = src_line;
	src_line += src_stride;
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	RVV_FOREACH_3 (width, vl, e32m4, src, mask, dst)
	{
	    __riscv_vse32 (
		dst,
		rvv_over_m4 (
		    rvv_in_m4 (__riscv_vor (__riscv_vle32_v_u32m4 (src, vl),
					    0xff000000, vl),
			       __riscv_vle8_v_u8m1 (mask, vl), vl),
		    __riscv_vle32_v_u32m4 (dst, vl), vl),
		vl);
	}
    }
}

static void
rvv_composite_over_8888_8888 (pixman_implementation_t *imp,
			      pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *dst_line, *dst;
    uint32_t *src_line, *src;
    int       dst_stride, src_stride;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride,
			   src_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e32m4, src, dst)
	{
	    __riscv_vse32 (dst,
			   rvv_over_m4 (__riscv_vle32_v_u32m4 (src, vl),
					__riscv_vle32_v_u32m4 (dst, vl), vl),
			   vl);
	}
    }
}

static void
rvv_composite_over_n_8_0565 (pixman_implementation_t *imp,
			     pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint16_t *__restrict__ dst_line, *__restrict__ dst;
    uint8_t *__restrict__ mask_line, *__restrict__ mask;
    int         dst_stride, mask_stride;
    uint32_t    src;
    vuint32m4_t vsrc;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    if (src == 0)
	return;
    vsrc = __riscv_vmv_v_x_u32m4 (src, __riscv_vsetvlmax_e32m4 ());

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride,
			   mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	RVV_FOREACH_2 (width, vl, e16m2, mask, dst)
	{
	    __riscv_vse16 (
		dst,
		rvv_convert_8888_to_0565_m2 (
		    rvv_over_m4 (
			rvv_in_m4 (vsrc, __riscv_vle8_v_u8m1 (mask, vl), vl),
			rvv_convert_0565_to_0888_m4 (
			    __riscv_vle16_v_u16m2 (dst, vl), vl),
			vl),
		    vl),
		vl);
	}
    }
}

static void
rvv_composite_over_n_8_8888 (pixman_implementation_t *imp,
			     pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t   *dst_line, *dst;
    uint8_t    *mask_line, *mask;
    int         dst_stride, mask_stride;
    uint32_t    src;
    vuint32m4_t vsrc;

    src  = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    vsrc = __riscv_vmv_v_x_u32m4 (src, __riscv_vsetvlmax_e32m4 ());

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride,
			   mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	RVV_FOREACH_2 (width, vl, e32m4, mask, dst)
	{
	    __riscv_vse32 (
		dst,
		rvv_over_m4 (
		    rvv_in_m4 (vsrc, __riscv_vle8_v_u8m1 (mask, vl), vl),
		    __riscv_vle32_v_u32m4 (dst, vl), vl),
		vl);
	}
    }
}

static void
rvv_composite_add_n_8888_8888_ca (pixman_implementation_t *imp,
				  pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t   *dst_line, *dst;
    uint32_t   *mask_line, *mask;
    int         dst_stride, mask_stride;
    uint32_t    src;
    vuint32m4_t vsrc;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    if (src == 0)
	return;
    vsrc = __riscv_vmv_v_x_u32m4 (src, __riscv_vsetvlmax_e32m4 ());

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride,
			   mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	RVV_FOREACH_2 (width, vl, e32m4, mask, dst)
	{
	    __riscv_vse32 (dst,
			   rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_vvv_m4 (
			       __riscv_vle32_v_u32m4 (mask, vl), vsrc,
			       __riscv_vle32_v_u32m4 (dst, vl), vl),
			   vl);
	}
    }
}

static void
rvv_composite_over_n_8888_8888_ca (pixman_implementation_t *__restrict__ imp,
				   pixman_composite_info_t *__restrict__ info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *__restrict__ dst_line, *__restrict__ dst;
    uint32_t *__restrict__ mask_line, *__restrict__ mask;
    int         dst_stride, mask_stride;
    uint32_t    src, srca;
    vuint32m4_t vsrc;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    if (src == 0)
	return;
    srca = src >> 24;
    vsrc = __riscv_vmv_v_x_u32m4 (src, __riscv_vsetvlmax_e32m4 ());

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride,
			   mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	RVV_FOREACH_2 (width, vl, e32m4, mask, dst)
	{
	    vuint32m4_t m = __riscv_vle32_v_u32m4 (mask, vl);
	    __riscv_vse32 (
		dst,
		rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_vvv_m4 (
		    __riscv_vle32_v_u32m4 (dst, vl),
		    __riscv_vnot (rvv_UN8x4_MUL_UN8_vx_m4 (m, srca, vl), vl),
		    rvv_UN8x4_MUL_UN8x4_vv_m4 (m, vsrc, vl), vl),
		vl);
	}
    }
}

static void
rvv_composite_over_n_8888_0565_ca (pixman_implementation_t *__restrict__ imp,
				   pixman_composite_info_t *__restrict__ info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint16_t *__restrict__ dst_line, *__restrict__ dst;
    uint32_t *__restrict__ mask_line, *__restrict__ mask;
    int         dst_stride, mask_stride;
    uint32_t    src, srca;
    vuint32m4_t vsrc;

    src  = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    srca = src >> 24;
    if (src == 0)
	return;
    vsrc = __riscv_vmv_v_x_u32m4 (src, __riscv_vsetvlmax_e32m4 ());

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint32_t, mask_stride,
			   mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	RVV_FOREACH_2 (width, vl, e32m4, mask, dst)
	{
	    vuint32m4_t ma = __riscv_vle32_v_u32m4 (mask, vl);

	    __riscv_vse16 (
		dst,
		rvv_convert_8888_to_0565_m2 (
		    rvv_UN8x4_MUL_UN8x4_ADD_UN8x4_vvv_m4 (
			rvv_convert_0565_to_0888_m4 (
			    __riscv_vle16_v_u16m2 (dst, vl), vl),
			__riscv_vnot (rvv_UN8x4_MUL_UN8_vx_m4 (ma, srca, vl),
				      vl),
			rvv_UN8x4_MUL_UN8x4_vv_m4 (ma, vsrc, vl), vl),
		    vl),
		vl);
	}
    }
}

static void
rvv_composite_over_8888_0565 (pixman_implementation_t *__restrict__ imp,
			      pixman_composite_info_t *__restrict__ info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint16_t *__restrict__ dst_line, *__restrict__ dst;
    uint32_t *__restrict__ src_line, *__restrict__ src;
    int dst_stride, src_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride,
			   src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16_t, dst_stride,
			   dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e16m2, src, dst)
	{
	    __riscv_vse16 (
		dst,
		rvv_convert_8888_to_0565_m2 (
		    rvv_over_m4 (__riscv_vle32_v_u32m4 (src, vl),
				 rvv_convert_0565_to_0888_m4 (
				     __riscv_vle16_v_u16m2 (dst, vl), vl),
				 vl),
		    vl),
		vl);
	}
    }
}

static void
rvv_composite_add_8_8 (pixman_implementation_t *imp,
		       pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint8_t *dst_line, *dst;
    uint8_t *src_line, *src;
    int      dst_stride, src_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t, src_stride,
			   src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t, dst_stride,
			   dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e8m8, src, dst)
	{
	    __riscv_vse8 (dst,
			  rvv_UN8_ADD_UN8_vv (__riscv_vle8_v_u8m8 (src, vl),
					      __riscv_vle8_v_u8m8 (dst, vl),
					      vl),
			  vl);
	}
    }
}

static void
rvv_composite_add_0565_0565 (pixman_implementation_t *imp,
			     pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint16_t *dst_line, *dst;
    uint16_t *src_line, *src;
    int       dst_stride, src_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint16_t, src_stride,
			   src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint16_t, dst_stride,
			   dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e16m2, src, dst)
	{
	    __riscv_vse16 (dst,
			   rvv_convert_8888_to_0565_m2 (
			       rvv_UN8x4_ADD_UN8x4_vv_m4 (
				   rvv_convert_0565_to_8888_m4 (
				       __riscv_vle16_v_u16m2 (src, vl), vl),
				   rvv_convert_0565_to_8888_m4 (
				       __riscv_vle16_v_u16m2 (dst, vl), vl),
				   vl),
			       vl),
			   vl);
	}
    }
}

static void
rvv_composite_add_8888_8888 (pixman_implementation_t *__restrict__ imp,
			     pixman_composite_info_t *__restrict__ info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t *__restrict__ dst_line, *__restrict__ dst;
    uint32_t *__restrict__ src_line, *__restrict__ src;
    int dst_stride, src_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride,
			   src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint32_t, dst_stride,
			   dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e32m4, src, dst)
	{
	    __riscv_vse32 (
		dst,
		rvv_UN8x4_ADD_UN8x4_vv_m4 (__riscv_vle32_v_u32m4 (src, vl),
					   __riscv_vle32_v_u32m4 (dst, vl), vl),
		vl);
	}
    }
}

static void
rvv_composite_add_n_8_8 (pixman_implementation_t *imp,
			 pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint8_t *dst_line, *dst;
    uint8_t *mask_line, *mask;
    int      dst_stride, mask_stride;
    uint32_t src;
    uint8_t  sa;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride,
			   mask_line, 1);
    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    sa  = (src >> 24);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	RVV_FOREACH_2 (width, vl, e8m4, mask, dst)
	{
	    __riscv_vse8 (
		dst,
		rvv_UN8_ADD_UN8_vv (rvv_UN8_MUL_UN8_vx_m4 (
					__riscv_vle8_v_u8m4 (mask, vl), sa, vl),
				    __riscv_vle8_v_u8m4 (dst, vl), vl),
		vl);
	}
    }
}

static void
rvv_composite_src_memcpy (pixman_implementation_t *imp,
			  pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    int      bpp     = PIXMAN_FORMAT_BPP (dest_image->bits.format) / 8;
    uint32_t n_bytes = width * bpp;
    int      dst_stride, src_stride;
    uint8_t *dst;
    uint8_t *src;

    src_stride = src_image->bits.rowstride * 4;
    dst_stride = dest_image->bits.rowstride * 4;

    src = (uint8_t *)src_image->bits.bits + src_y * src_stride + src_x * bpp;
    dst = (uint8_t *)dest_image->bits.bits + dest_y * dst_stride + dest_x * bpp;

    while (height--)
    {
	memcpy (dst, src, n_bytes);

	dst += dst_stride;
	src += src_stride;
    }
}

static void
rvv_composite_in_n_8_8 (pixman_implementation_t *imp,
			pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t src, srca;
    uint8_t *dst_line, *dst;
    uint8_t *mask_line, *mask;
    int      dst_stride, mask_stride;

    src  = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    srca = src >> 24;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t, dst_stride,
			   dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride,
			   mask_line, 1);

    if (srca == 0xff)
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;

	    RVV_FOREACH_2 (width, vl, e8m4, mask, dst)
	    {
		__riscv_vse8 (
		    dst,
		    rvv_UN8_MUL_UN8_vv_m4 (__riscv_vle8_v_u8m4 (mask, vl),
					   __riscv_vle8_v_u8m4 (dst, vl), vl),
		    vl);
	    }
	}
    }
    else
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;

	    RVV_FOREACH_2 (width, vl, e8m4, mask, dst)
	    {
		__riscv_vse8 (dst,
			      rvv_UN8_MUL_UN8_vv_m4 (
				  rvv_UN8_MUL_UN8_vx_m4 (
				      __riscv_vle8_v_u8m4 (mask, vl), srca, vl),
				  __riscv_vle8_v_u8m4 (dst, vl), vl),
			      vl);
	    }
	}
    }
}

static void
rvv_composite_in_8_8 (pixman_implementation_t *imp,
		      pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint8_t *dst_line, *dst;
    uint8_t *src_line, *src;
    int      dst_stride, src_stride;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t, src_stride,
			   src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, uint8_t, dst_stride,
			   dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;

	RVV_FOREACH_2 (width, vl, e8m4, src, dst)
	{
	    __riscv_vse8 (dst,
			  rvv_UN8_MUL_UN8_vv_m4 (__riscv_vle8_v_u8m4 (src, vl),
						 __riscv_vle8_v_u8m4 (dst, vl),
						 vl),
			  vl);
	}
    }
}

#define A1_FILL_MASK(n, offs) (((1U << (n)) - 1) << (offs))

/*
 * There is some potential for hand vectorization, but for now let's leave it
 * autovectorized.
 */
static force_inline void
pixman_fill1_line (uint32_t *dst, int offs, int width, int v)
{
    if (offs)
    {
	int leading_pixels = 32 - offs;
	if (leading_pixels >= width)
	{
	    if (v)
		*dst |= A1_FILL_MASK (width, offs);
	    else
		*dst &= ~A1_FILL_MASK (width, offs);
	    return;
	}
	else
	{
	    if (v)
		*dst++ |= A1_FILL_MASK (leading_pixels, offs);
	    else
		*dst++ &= ~A1_FILL_MASK (leading_pixels, offs);
	    width -= leading_pixels;
	}
    }
    while (width >= 32)
    {
	if (v)
	    *dst++ = 0xFFFFFFFF;
	else
	    *dst++ = 0;
	width -= 32;
    }
    if (width > 0)
    {
	if (v)
	    *dst |= A1_FILL_MASK (width, 0);
	else
	    *dst &= ~A1_FILL_MASK (width, 0);
    }
}

static void
rvv_fill1 (uint32_t *bits,
	   int       stride,
	   int       x,
	   int       y,
	   int       width,
	   int       height,
	   uint32_t  filler)
{
    uint32_t *dst  = bits + y * stride + (x >> 5);
    int       offs = x & 31;

    while (height--)
    {
	pixman_fill1_line (dst, offs, width, (filler & 1));
	dst += stride;
    }
}

#define RVV_FILL(dtypew)                                                            \
    static void rvv_fill_u##dtypew (uint32_t *__restrict__ bits, int stride,        \
				    int x, int y, int width, int height,            \
				    uint32_t filler)                                \
    {                                                                               \
	uint##dtypew##_t *__restrict__ bitsw = (uint##dtypew##_t *)bits;            \
	int32_t             vstride          = stride * (32 / dtypew);              \
	vuint##dtypew##m8_t vfiller          = __riscv_vmv_v_x_u##dtypew##m8 (      \
            (uint##dtypew##_t)filler, __riscv_vsetvlmax_e##dtypew##m8 ()); \
                                                                                    \
	bitsw += y * vstride + x;                                                   \
	while (height--)                                                            \
	{                                                                           \
	    uint##dtypew##_t *__restrict__ d = bitsw;                               \
                                                                                    \
	    RVV_FOREACH_1 (width, vl, e##dtypew##m8, d)                             \
	    {                                                                       \
		__riscv_vse##dtypew (d, vfiller, vl);                               \
	    }                                                                       \
                                                                                    \
	    bitsw += vstride;                                                       \
	}                                                                           \
    }

RVV_FILL (8);
RVV_FILL (16);
RVV_FILL (32);

static pixman_bool_t
rvv_fill (pixman_implementation_t *__restrict__ imp,
	  uint32_t *__restrict__ bits,
	  int      stride,
	  int      bpp,
	  int      x,
	  int      y,
	  int      width,
	  int      height,
	  uint32_t filler)
{
    switch (bpp)
    {
	case 1:
	    rvv_fill1 (bits, stride, x, y, width, height, filler);
	    break;
	case 8:
	    rvv_fill_u8 (bits, stride, x, y, width, height, filler);
	    break;
	case 16:
	    rvv_fill_u16 (bits, stride, x, y, width, height, filler);
	    break;
	case 32:
	    rvv_fill_u32 (bits, stride, x, y, width, height, filler);
	    break;
	default:
	    return FALSE;
    }

    return TRUE;
}

static void
rvv_composite_solid_fill (pixman_implementation_t *imp,
			  pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    uint32_t src;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    if (dest_image->bits.format == PIXMAN_a1)
    {
	src = src >> 31;
    }
    else if (dest_image->bits.format == PIXMAN_a8)
    {
	src = src >> 24;
    }
    else if (dest_image->bits.format == PIXMAN_r5g6b5 ||
	     dest_image->bits.format == PIXMAN_b5g6r5)
    {
	src = convert_8888_to_0565 (src);
    }

    rvv_fill (imp, dest_image->bits.bits, dest_image->bits.rowstride,
	      PIXMAN_FORMAT_BPP (dest_image->bits.format), dest_x, dest_y,
	      width, height, src);
}

#define RVV_BLT(dtypew)                                                        \
    static void rvv_blt_u##dtypew (                                            \
	uint32_t *__restrict__ src_bits, uint32_t *__restrict__ dst_bits,      \
	int src_stride, int dst_stride, int src_x, int src_y, int dest_x,      \
	int dest_y, int width, int height)                                     \
    {                                                                          \
	uint##dtypew##_t *src_w = (uint##dtypew##_t *)src_bits;                \
	uint##dtypew##_t *dst_w = (uint##dtypew##_t *)dst_bits;                \
                                                                               \
	src_stride = src_stride * (32 / dtypew);                               \
	dst_stride = dst_stride * (32 / dtypew);                               \
                                                                               \
	src_w += src_stride * src_y + src_x;                                   \
	dst_w += dst_stride * dest_y + dest_x;                                 \
                                                                               \
	while (height--)                                                       \
	{                                                                      \
	    uint##dtypew##_t *__restrict__ pd = dst_w;                         \
	    uint##dtypew##_t *__restrict__ ps = src_w;                         \
                                                                               \
	    RVV_FOREACH_2 (width, vl, e##dtypew##m8, ps, pd)                   \
	    {                                                                  \
		__riscv_vse##dtypew (                                          \
		    pd, __riscv_vle##dtypew##_v_u##dtypew##m8 (ps, vl), vl);   \
	    }                                                                  \
                                                                               \
	    dst_w += dst_stride;                                               \
	    src_w += src_stride;                                               \
	}                                                                      \
    }
RVV_BLT (8);
RVV_BLT (16);
RVV_BLT (32);

static pixman_bool_t
rvv_blt (pixman_implementation_t *__restrict__ imp,
	 uint32_t *__restrict__ src_bits,
	 uint32_t *__restrict__ dst_bits,
	 int src_stride,
	 int dst_stride,
	 int src_bpp,
	 int dst_bpp,
	 int src_x,
	 int src_y,
	 int dest_x,
	 int dest_y,
	 int width,
	 int height)
{
    if (src_bpp != dst_bpp)
	return FALSE;

    switch (src_bpp)
    {
	case 8:
	    rvv_blt_u8 (src_bits, dst_bits, src_stride, dst_stride, src_x,
			src_y, dest_x, dest_y, width, height);
	    break;
	case 16:
	    rvv_blt_u16 (src_bits, dst_bits, src_stride, dst_stride, src_x,
			 src_y, dest_x, dest_y, width, height);
	    break;
	case 32:
	    rvv_blt_u32 (src_bits, dst_bits, src_stride, dst_stride, src_x,
			 src_y, dest_x, dest_y, width, height);
	    break;
	default:
	    return FALSE;
    }

    return TRUE;
}

// clang-format off
static const pixman_fast_path_t rvv_fast_paths[] = {
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, r5g6b5, rvv_composite_over_n_8_0565),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, b5g6r5, rvv_composite_over_n_8_0565),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a8, r8g8b8, rvv_composite_over_n_8_0888),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a8, b8g8r8, rvv_composite_over_n_8_0888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, a8r8g8b8, rvv_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, x8r8g8b8, rvv_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, a8b8g8r8, rvv_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, x8b8g8r8, rvv_composite_over_n_8_8888),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a1, a8r8g8b8, rvv_composite_over_n_1_8888),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a1, x8r8g8b8, rvv_composite_over_n_1_8888),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a1, a8b8g8r8, rvv_composite_over_n_1_8888),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a1, x8b8g8r8, rvv_composite_over_n_1_8888),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a1, r5g6b5,   rvv_composite_over_n_1_0565),
    // PIXMAN_STD_FAST_PATH (OVER, solid, a1, b5g6r5,   rvv_composite_over_n_1_0565),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8r8g8b8, a8r8g8b8, rvv_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8r8g8b8, x8r8g8b8, rvv_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8r8g8b8, r5g6b5, rvv_composite_over_n_8888_0565_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8b8g8r8, a8b8g8r8, rvv_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8b8g8r8, x8b8g8r8, rvv_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8b8g8r8, b5g6r5, rvv_composite_over_n_8888_0565_ca),
    PIXMAN_STD_FAST_PATH (OVER, x8r8g8b8, a8, x8r8g8b8, rvv_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8r8g8b8, a8, a8r8g8b8, rvv_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8b8g8r8, a8, x8b8g8r8, rvv_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8b8g8r8, a8, a8b8g8r8, rvv_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, a8r8g8b8, rvv_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, x8r8g8b8, rvv_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, r5g6b5, rvv_composite_over_8888_0565),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, a8b8g8r8, rvv_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, x8b8g8r8, rvv_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, b5g6r5, rvv_composite_over_8888_0565),
    PIXMAN_STD_FAST_PATH (ADD, r5g6b5, null, r5g6b5, rvv_composite_add_0565_0565),
    PIXMAN_STD_FAST_PATH (ADD, b5g6r5, null, b5g6r5, rvv_composite_add_0565_0565),
    PIXMAN_STD_FAST_PATH (ADD, a8r8g8b8, null, a8r8g8b8, rvv_composite_add_8888_8888),
    PIXMAN_STD_FAST_PATH (ADD, a8b8g8r8, null, a8b8g8r8, rvv_composite_add_8888_8888),
    PIXMAN_STD_FAST_PATH (ADD, a8, null, a8, rvv_composite_add_8_8),
    // PIXMAN_STD_FAST_PATH (ADD, a1, null, a1, fast_composite_add_1_1),
    PIXMAN_STD_FAST_PATH_CA (ADD, solid, a8r8g8b8, a8r8g8b8, rvv_composite_add_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH (ADD, solid, a8, a8, rvv_composite_add_n_8_8),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a8r8g8b8, rvv_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, x8r8g8b8, rvv_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a8b8g8r8, rvv_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, x8b8g8r8, rvv_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a1, rvv_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a8, rvv_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, r5g6b5, rvv_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, x8r8g8b8, null, a8r8g8b8, rvv_composite_src_x888_8888),
    PIXMAN_STD_FAST_PATH (SRC, x8b8g8r8, null, a8b8g8r8, rvv_composite_src_x888_8888),
    PIXMAN_STD_FAST_PATH (SRC, a8r8g8b8, null, x8r8g8b8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, a8r8g8b8, null, a8r8g8b8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, x8r8g8b8, null, x8r8g8b8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, a8b8g8r8, null, x8b8g8r8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, a8b8g8r8, null, a8b8g8r8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, x8b8g8r8, null, x8b8g8r8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8a8, null, b8g8r8x8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8a8, null, b8g8r8a8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8x8, null, b8g8r8x8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (SRC, r5g6b5, null, r5g6b5, rvv_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, b5g6r5, null, b5g6r5, rvv_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, r8g8b8, null, r8g8b8, rvv_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8, null, b8g8r8, rvv_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, x1r5g5b5, null, x1r5g5b5, rvv_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, a1r5g5b5, null, x1r5g5b5, rvv_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, a8, null, a8, rvv_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (IN, a8, null, a8, rvv_composite_in_8_8),
    PIXMAN_STD_FAST_PATH (IN, solid, a8, a8, rvv_composite_in_n_8_8),
    PIXMAN_STD_FAST_PATH (OVER, x8r8g8b8, null, x8r8g8b8, rvv_composite_src_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8b8g8r8, null, x8b8g8r8, rvv_composite_src_8888_8888),

    {PIXMAN_OP_NONE},
};

pixman_implementation_t *
_pixman_implementation_create_rvv (pixman_implementation_t *fallback)
{
    pixman_implementation_t *imp = _pixman_implementation_create (
	fallback, rvv_fast_paths);

    // clang-format off
    imp->combine_float[PIXMAN_OP_CLEAR] = rvv_combine_clear_u_float;
    imp->combine_float[PIXMAN_OP_SRC] = rvv_combine_src_u_float;
    imp->combine_float[PIXMAN_OP_DST] = rvv_combine_dst_u_float;
    imp->combine_float[PIXMAN_OP_OVER] = rvv_combine_over_u_float;
    imp->combine_float[PIXMAN_OP_OVER_REVERSE] = rvv_combine_over_reverse_u_float;
    imp->combine_float[PIXMAN_OP_IN] = rvv_combine_in_u_float;
    imp->combine_float[PIXMAN_OP_IN_REVERSE] = rvv_combine_in_reverse_u_float;
    imp->combine_float[PIXMAN_OP_OUT] = rvv_combine_out_u_float;
    imp->combine_float[PIXMAN_OP_OUT_REVERSE] = rvv_combine_out_reverse_u_float;
    imp->combine_float[PIXMAN_OP_ATOP] = rvv_combine_atop_u_float;
    imp->combine_float[PIXMAN_OP_ATOP_REVERSE] = rvv_combine_atop_reverse_u_float;
    imp->combine_float[PIXMAN_OP_XOR] = rvv_combine_xor_u_float;
    imp->combine_float[PIXMAN_OP_ADD] = rvv_combine_add_u_float;
    imp->combine_float[PIXMAN_OP_SATURATE] = rvv_combine_saturate_u_float;

    /* Disjoint, unified */
    imp->combine_float[PIXMAN_OP_DISJOINT_CLEAR] = rvv_combine_disjoint_clear_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_SRC] = rvv_combine_disjoint_src_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_DST] = rvv_combine_disjoint_dst_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_OVER] = rvv_combine_disjoint_over_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_OVER_REVERSE] = rvv_combine_disjoint_over_reverse_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_IN] = rvv_combine_disjoint_in_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_IN_REVERSE] = rvv_combine_disjoint_in_reverse_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_OUT] = rvv_combine_disjoint_out_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_OUT_REVERSE] = rvv_combine_disjoint_out_reverse_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_ATOP] = rvv_combine_disjoint_atop_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_ATOP_REVERSE] = rvv_combine_disjoint_atop_reverse_u_float;
    imp->combine_float[PIXMAN_OP_DISJOINT_XOR] = rvv_combine_disjoint_xor_u_float;

    /* Conjoint, unified */
    imp->combine_float[PIXMAN_OP_CONJOINT_CLEAR] = rvv_combine_conjoint_clear_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_SRC] = rvv_combine_conjoint_src_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_DST] = rvv_combine_conjoint_dst_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_OVER] = rvv_combine_conjoint_over_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_OVER_REVERSE] = rvv_combine_conjoint_over_reverse_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_IN] = rvv_combine_conjoint_in_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_IN_REVERSE] = rvv_combine_conjoint_in_reverse_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_OUT] = rvv_combine_conjoint_out_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_OUT_REVERSE] = rvv_combine_conjoint_out_reverse_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_ATOP] = rvv_combine_conjoint_atop_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_ATOP_REVERSE] = rvv_combine_conjoint_atop_reverse_u_float;
    imp->combine_float[PIXMAN_OP_CONJOINT_XOR] = rvv_combine_conjoint_xor_u_float;

    /* PDF operators, unified */
    imp->combine_float[PIXMAN_OP_MULTIPLY] = rvv_combine_multiply_u_float;
    imp->combine_float[PIXMAN_OP_SCREEN] = rvv_combine_screen_u_float;
    imp->combine_float[PIXMAN_OP_OVERLAY] = rvv_combine_overlay_u_float;
    imp->combine_float[PIXMAN_OP_DARKEN] = rvv_combine_darken_u_float;
    imp->combine_float[PIXMAN_OP_LIGHTEN] = rvv_combine_lighten_u_float;
    imp->combine_float[PIXMAN_OP_HARD_LIGHT] = rvv_combine_hard_light_u_float;
    imp->combine_float[PIXMAN_OP_SOFT_LIGHT] = rvv_combine_soft_light_u_float;
    imp->combine_float[PIXMAN_OP_DIFFERENCE] = rvv_combine_difference_u_float;
    imp->combine_float[PIXMAN_OP_EXCLUSION] = rvv_combine_exclusion_u_float;
    imp->combine_float[PIXMAN_OP_COLOR_DODGE] = rvv_combine_color_dodge_u_float;
    imp->combine_float[PIXMAN_OP_COLOR_BURN] = rvv_combine_color_burn_u_float;

    /* Component alpha combiners */
    imp->combine_float_ca[PIXMAN_OP_CLEAR] = rvv_combine_clear_ca_float;
    imp->combine_float_ca[PIXMAN_OP_SRC] = rvv_combine_src_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DST] = rvv_combine_dst_ca_float;
    imp->combine_float_ca[PIXMAN_OP_OVER] = rvv_combine_over_ca_float;
    imp->combine_float_ca[PIXMAN_OP_OVER_REVERSE] = rvv_combine_over_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_IN] = rvv_combine_in_ca_float;
    imp->combine_float_ca[PIXMAN_OP_IN_REVERSE] = rvv_combine_in_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_OUT] = rvv_combine_out_ca_float;
    imp->combine_float_ca[PIXMAN_OP_OUT_REVERSE] = rvv_combine_out_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_ATOP] = rvv_combine_atop_ca_float;
    imp->combine_float_ca[PIXMAN_OP_ATOP_REVERSE] = rvv_combine_atop_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_XOR] = rvv_combine_xor_ca_float;
    imp->combine_float_ca[PIXMAN_OP_ADD] = rvv_combine_add_ca_float;
    imp->combine_float_ca[PIXMAN_OP_SATURATE] = rvv_combine_saturate_ca_float;

    /* Disjoint CA */
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_CLEAR] = rvv_combine_disjoint_clear_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_SRC] = rvv_combine_disjoint_src_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_DST] = rvv_combine_disjoint_dst_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_OVER] = rvv_combine_disjoint_over_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_OVER_REVERSE] = rvv_combine_disjoint_over_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_IN] = rvv_combine_disjoint_in_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_IN_REVERSE] = rvv_combine_disjoint_in_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_OUT] = rvv_combine_disjoint_out_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_OUT_REVERSE] = rvv_combine_disjoint_out_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_ATOP] = rvv_combine_disjoint_atop_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_ATOP_REVERSE] = rvv_combine_disjoint_atop_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DISJOINT_XOR] = rvv_combine_disjoint_xor_ca_float;

    /* Conjoint CA */
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_CLEAR] = rvv_combine_conjoint_clear_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_SRC] = rvv_combine_conjoint_src_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_DST] = rvv_combine_conjoint_dst_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_OVER] = rvv_combine_conjoint_over_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_OVER_REVERSE] = rvv_combine_conjoint_over_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_IN] = rvv_combine_conjoint_in_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_IN_REVERSE] = rvv_combine_conjoint_in_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_OUT] = rvv_combine_conjoint_out_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_OUT_REVERSE] = rvv_combine_conjoint_out_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_ATOP] = rvv_combine_conjoint_atop_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_ATOP_REVERSE] = rvv_combine_conjoint_atop_reverse_ca_float;
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_XOR] = rvv_combine_conjoint_xor_ca_float;

    /* PDF operators CA */
    imp->combine_float_ca[PIXMAN_OP_MULTIPLY] = rvv_combine_multiply_ca_float;
    imp->combine_float_ca[PIXMAN_OP_SCREEN] = rvv_combine_screen_ca_float;
    imp->combine_float_ca[PIXMAN_OP_OVERLAY] = rvv_combine_overlay_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DARKEN] = rvv_combine_darken_ca_float;
    imp->combine_float_ca[PIXMAN_OP_LIGHTEN] = rvv_combine_lighten_ca_float;
    imp->combine_float_ca[PIXMAN_OP_COLOR_DODGE] = rvv_combine_color_dodge_ca_float;
    imp->combine_float_ca[PIXMAN_OP_COLOR_BURN] = rvv_combine_color_burn_ca_float;
    imp->combine_float_ca[PIXMAN_OP_HARD_LIGHT] = rvv_combine_hard_light_ca_float;
    imp->combine_float_ca[PIXMAN_OP_SOFT_LIGHT] = rvv_combine_soft_light_ca_float;
    imp->combine_float_ca[PIXMAN_OP_DIFFERENCE] = rvv_combine_difference_ca_float;
    imp->combine_float_ca[PIXMAN_OP_EXCLUSION] = rvv_combine_exclusion_ca_float;

    /* It is not clear that these make sense, so make them noops for now */
    imp->combine_float_ca[PIXMAN_OP_HSL_HUE] = rvv_combine_dst_u_float;
    imp->combine_float_ca[PIXMAN_OP_HSL_SATURATION] = rvv_combine_dst_u_float;
    imp->combine_float_ca[PIXMAN_OP_HSL_COLOR] = rvv_combine_dst_u_float;
    imp->combine_float_ca[PIXMAN_OP_HSL_LUMINOSITY] = rvv_combine_dst_u_float;

    /* Set up function pointers */
    imp->combine_32[PIXMAN_OP_CLEAR] = rvv_combine_clear;
    imp->combine_32[PIXMAN_OP_SRC] = rvv_combine_src_u;
    imp->combine_32[PIXMAN_OP_OVER] = rvv_combine_over_u;
    imp->combine_32[PIXMAN_OP_OVER_REVERSE] = rvv_combine_over_reverse_u;
    imp->combine_32[PIXMAN_OP_IN] = rvv_combine_in_u;
    imp->combine_32[PIXMAN_OP_IN_REVERSE] = rvv_combine_in_reverse_u;
    imp->combine_32[PIXMAN_OP_OUT] = rvv_combine_out_u;
    imp->combine_32[PIXMAN_OP_OUT_REVERSE] = rvv_combine_out_reverse_u;
    imp->combine_32[PIXMAN_OP_ATOP] = rvv_combine_atop_u;
    imp->combine_32[PIXMAN_OP_ATOP_REVERSE] = rvv_combine_atop_reverse_u;
    imp->combine_32[PIXMAN_OP_XOR] = rvv_combine_xor_u;
    imp->combine_32[PIXMAN_OP_ADD] = rvv_combine_add_u;

    imp->combine_32[PIXMAN_OP_MULTIPLY] = rvv_combine_multiply_u;
    imp->combine_32[PIXMAN_OP_SCREEN] = rvv_combine_screen_u;
    imp->combine_32[PIXMAN_OP_OVERLAY] = rvv_combine_overlay_u;
    imp->combine_32[PIXMAN_OP_DARKEN] = rvv_combine_darken_u;
    imp->combine_32[PIXMAN_OP_LIGHTEN] = rvv_combine_lighten_u;
    imp->combine_32[PIXMAN_OP_HARD_LIGHT] = rvv_combine_hard_light_u;
    imp->combine_32[PIXMAN_OP_DIFFERENCE] = rvv_combine_difference_u;
    imp->combine_32[PIXMAN_OP_EXCLUSION] = rvv_combine_exclusion_u;

    imp->combine_32_ca[PIXMAN_OP_CLEAR] = rvv_combine_clear;
    imp->combine_32_ca[PIXMAN_OP_SRC] = rvv_combine_src_ca;
    imp->combine_32_ca[PIXMAN_OP_OVER] = rvv_combine_over_ca;
    imp->combine_32_ca[PIXMAN_OP_OVER_REVERSE] = rvv_combine_over_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_IN] = rvv_combine_in_ca;
    imp->combine_32_ca[PIXMAN_OP_IN_REVERSE] = rvv_combine_in_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_OUT] = rvv_combine_out_ca;
    imp->combine_32_ca[PIXMAN_OP_OUT_REVERSE] = rvv_combine_out_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_ATOP] = rvv_combine_atop_ca;
    imp->combine_32_ca[PIXMAN_OP_ATOP_REVERSE] = rvv_combine_atop_reverse_ca;
    imp->combine_32_ca[PIXMAN_OP_XOR] = rvv_combine_xor_ca;
    imp->combine_32_ca[PIXMAN_OP_ADD] = rvv_combine_add_ca;

    imp->combine_32_ca[PIXMAN_OP_MULTIPLY] = rvv_combine_multiply_ca;
    imp->combine_32_ca[PIXMAN_OP_SCREEN] = rvv_combine_screen_ca;
    imp->combine_32_ca[PIXMAN_OP_OVERLAY] = rvv_combine_overlay_ca;
    imp->combine_32_ca[PIXMAN_OP_DARKEN] = rvv_combine_darken_ca;
    imp->combine_32_ca[PIXMAN_OP_LIGHTEN] = rvv_combine_lighten_ca;
    imp->combine_32_ca[PIXMAN_OP_HARD_LIGHT] = rvv_combine_hard_light_ca;
    imp->combine_32_ca[PIXMAN_OP_DIFFERENCE] = rvv_combine_difference_ca;
    imp->combine_32_ca[PIXMAN_OP_EXCLUSION] = rvv_combine_exclusion_ca;

    imp->fill = rvv_fill;
    imp->blt = rvv_blt;

    return imp;
}
// clang-format on
