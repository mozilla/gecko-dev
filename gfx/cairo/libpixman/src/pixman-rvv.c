/*
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 *             2024 Filip Wasil, Samsung Electronics
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

/*
 * Screen
 *
 *      ad * as * B(d/ad, s/as)
 *    = ad * as * (d/ad + s/as - s/as * d/ad)
 *    = ad * s + as * d - s * d
 */

static force_inline vfloat32m1_t
rvv_blend_screen (const vfloat32m1_t sa,
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
rvv_blend_multiply (const vfloat32m1_t sa,
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
rvv_blend_overlay (const vfloat32m1_t sa,
		   const vfloat32m1_t s,
		   const vfloat32m1_t da,
		   const vfloat32m1_t d,
		   size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4, f0, f1, f2;
    vbool32_t             vb;
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
rvv_blend_darken (const vfloat32m1_t sa,
		  const vfloat32m1_t s,
		  const vfloat32m1_t da,
		  const vfloat32m1_t d,
		  size_t             vl)
{
    vfloat32m1_t ss, dd;
    vbool32_t             vb;
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
rvv_blend_lighten (const vfloat32m1_t sa,
		   const vfloat32m1_t s,
		   const vfloat32m1_t da,
		   const vfloat32m1_t d,
		   size_t             vl)
{
    vfloat32m1_t ss, dd;
    vbool32_t             vb;
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
rvv_blend_color_dodge (const vfloat32m1_t sa,
		       const vfloat32m1_t s,
		       const vfloat32m1_t da,
		       const vfloat32m1_t d,
		       size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4;
    vbool32_t             is_d_zero, vb, is_t0_non_zero;

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
rvv_blend_color_burn (const vfloat32m1_t sa,
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
rvv_blend_hard_light (const vfloat32m1_t sa,
		      const vfloat32m1_t s,
		      const vfloat32m1_t da,
		      const vfloat32m1_t d,
		      size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4;
    vbool32_t             vb;
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
rvv_blend_soft_light (const vfloat32m1_t sa,
		      const vfloat32m1_t s,
		      const vfloat32m1_t da,
		      const vfloat32m1_t d,
		      size_t             vl)
{
    vfloat32m1_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,
	t13;
    vbool32_t is_sa_lt_2s, is_da_ls_4d, is_da_non_zero;
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
rvv_blend_difference (const vfloat32m1_t sa,
		      const vfloat32m1_t s,
		      const vfloat32m1_t da,
		      const vfloat32m1_t d,
		      size_t             vl)
{
    vfloat32m1_t dsa, sda;
    vbool32_t             vb;
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
rvv_blend_exclusion (const vfloat32m1_t sa,
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
rvv_combine_inner (pixman_bool_t               component,
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
	rvv_combine_inner (component, dest, src, mask, n_pixels, combine_a,    \
			   combine_c);                                         \
    }

#define RVV_MAKE_COMBINERS(name, combine_a, combine_c)                         \
    RVV_MAKE_COMBINER (name##_ca, TRUE, combine_a, combine_c)                  \
    RVV_MAKE_COMBINER (name##_u, FALSE, combine_a, combine_c)

static force_inline vfloat32m1_t
rvv_get_factor (combine_factor_t factor,
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
    static vfloat32m1_t force_inline rvv_pd_combine_##name (                   \
	vfloat32m1_t sa, vfloat32m1_t s, vfloat32m1_t da, vfloat32m1_t d,      \
	size_t vl)                                                             \
    {                                                                          \
	const vfloat32m1_t fa = rvv_get_factor (a, sa, da, vl);                \
	const vfloat32m1_t fb = rvv_get_factor (b, sa, da, vl);                \
	vfloat32m1_t       t0 = __riscv_vfadd_vv_f32m1 (                       \
		  __riscv_vfmul_vv_f32m1 (s, fa, vl),                          \
		  __riscv_vfmul_vv_f32m1 (d, fb, vl), vl);                     \
	return __riscv_vfmin_vv_f32m1 (__riscv_vfmv_v_f_f32m1 (1.0f, vl), t0,  \
				       vl);                                    \
    }                                                                          \
                                                                               \
    RVV_MAKE_COMBINERS (name, rvv_pd_combine_##name, rvv_pd_combine_##name)

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
	return __riscv_vfadd_vv_f32m1 (f, rvv_blend_##name (sa, s, da, d, vl), \
				       vl);                                    \
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

static const pixman_fast_path_t rvv_fast_paths[] = {
    {PIXMAN_OP_NONE},
};

// clang-format off
pixman_implementation_t *
_pixman_implementation_create_rvv (pixman_implementation_t *fallback)
{
    pixman_implementation_t *imp = _pixman_implementation_create (fallback, rvv_fast_paths);

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
    imp->combine_float_ca[PIXMAN_OP_CONJOINT_IN_REVERSE] =rvv_combine_conjoint_in_reverse_ca_float;
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

    return imp;
}

// clang-format on