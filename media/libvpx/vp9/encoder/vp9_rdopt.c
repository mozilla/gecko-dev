/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>

#include "./vp9_rtcd.h"

#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_idct.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/common/vp9_scan.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_systemdependent.h"

#include "vp9/encoder/vp9_cost.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_rd.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_variance.h"
#include "vp9/encoder/vp9_aq_variance.h"

#define LAST_FRAME_MODE_MASK    ((1 << GOLDEN_FRAME) | (1 << ALTREF_FRAME) | \
                                 (1 << INTRA_FRAME))
#define GOLDEN_FRAME_MODE_MASK  ((1 << LAST_FRAME) | (1 << ALTREF_FRAME) | \
                                 (1 << INTRA_FRAME))
#define ALT_REF_MODE_MASK       ((1 << LAST_FRAME) | (1 << GOLDEN_FRAME) | \
                                 (1 << INTRA_FRAME))

#define SECOND_REF_FRAME_MASK   ((1 << ALTREF_FRAME) | 0x01)

#define MIN_EARLY_TERM_INDEX    3
#define NEW_MV_DISCOUNT_FACTOR  8

typedef struct {
  PREDICTION_MODE mode;
  MV_REFERENCE_FRAME ref_frame[2];
} MODE_DEFINITION;

typedef struct {
  MV_REFERENCE_FRAME ref_frame[2];
} REF_DEFINITION;

struct rdcost_block_args {
  MACROBLOCK *x;
  ENTROPY_CONTEXT t_above[16];
  ENTROPY_CONTEXT t_left[16];
  int rate;
  int64_t dist;
  int64_t sse;
  int this_rate;
  int64_t this_dist;
  int64_t this_sse;
  int64_t this_rd;
  int64_t best_rd;
  int skip;
  int use_fast_coef_costing;
  const scan_order *so;
};

#define LAST_NEW_MV_INDEX 6
static const MODE_DEFINITION vp9_mode_order[MAX_MODES] = {
  {NEARESTMV, {LAST_FRAME,   NONE}},
  {NEARESTMV, {ALTREF_FRAME, NONE}},
  {NEARESTMV, {GOLDEN_FRAME, NONE}},

  {DC_PRED,   {INTRA_FRAME,  NONE}},

  {NEWMV,     {LAST_FRAME,   NONE}},
  {NEWMV,     {ALTREF_FRAME, NONE}},
  {NEWMV,     {GOLDEN_FRAME, NONE}},

  {NEARMV,    {LAST_FRAME,   NONE}},
  {NEARMV,    {ALTREF_FRAME, NONE}},
  {NEARMV,    {GOLDEN_FRAME, NONE}},

  {ZEROMV,    {LAST_FRAME,   NONE}},
  {ZEROMV,    {GOLDEN_FRAME, NONE}},
  {ZEROMV,    {ALTREF_FRAME, NONE}},

  {NEARESTMV, {LAST_FRAME,   ALTREF_FRAME}},
  {NEARESTMV, {GOLDEN_FRAME, ALTREF_FRAME}},

  {TM_PRED,   {INTRA_FRAME,  NONE}},

  {NEARMV,    {LAST_FRAME,   ALTREF_FRAME}},
  {NEWMV,     {LAST_FRAME,   ALTREF_FRAME}},
  {NEARMV,    {GOLDEN_FRAME, ALTREF_FRAME}},
  {NEWMV,     {GOLDEN_FRAME, ALTREF_FRAME}},

  {ZEROMV,    {LAST_FRAME,   ALTREF_FRAME}},
  {ZEROMV,    {GOLDEN_FRAME, ALTREF_FRAME}},

  {H_PRED,    {INTRA_FRAME,  NONE}},
  {V_PRED,    {INTRA_FRAME,  NONE}},
  {D135_PRED, {INTRA_FRAME,  NONE}},
  {D207_PRED, {INTRA_FRAME,  NONE}},
  {D153_PRED, {INTRA_FRAME,  NONE}},
  {D63_PRED,  {INTRA_FRAME,  NONE}},
  {D117_PRED, {INTRA_FRAME,  NONE}},
  {D45_PRED,  {INTRA_FRAME,  NONE}},
};

static const REF_DEFINITION vp9_ref_order[MAX_REFS] = {
  {{LAST_FRAME,   NONE}},
  {{GOLDEN_FRAME, NONE}},
  {{ALTREF_FRAME, NONE}},
  {{LAST_FRAME,   ALTREF_FRAME}},
  {{GOLDEN_FRAME, ALTREF_FRAME}},
  {{INTRA_FRAME,  NONE}},
};

static void swap_block_ptr(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
                           int m, int n, int min_plane, int max_plane) {
  int i;

  for (i = min_plane; i < max_plane; ++i) {
    struct macroblock_plane *const p = &x->plane[i];
    struct macroblockd_plane *const pd = &x->e_mbd.plane[i];

    p->coeff    = ctx->coeff_pbuf[i][m];
    p->qcoeff   = ctx->qcoeff_pbuf[i][m];
    pd->dqcoeff = ctx->dqcoeff_pbuf[i][m];
    p->eobs     = ctx->eobs_pbuf[i][m];

    ctx->coeff_pbuf[i][m]   = ctx->coeff_pbuf[i][n];
    ctx->qcoeff_pbuf[i][m]  = ctx->qcoeff_pbuf[i][n];
    ctx->dqcoeff_pbuf[i][m] = ctx->dqcoeff_pbuf[i][n];
    ctx->eobs_pbuf[i][m]    = ctx->eobs_pbuf[i][n];

    ctx->coeff_pbuf[i][n]   = p->coeff;
    ctx->qcoeff_pbuf[i][n]  = p->qcoeff;
    ctx->dqcoeff_pbuf[i][n] = pd->dqcoeff;
    ctx->eobs_pbuf[i][n]    = p->eobs;
  }
}

static void model_rd_for_sb(VP9_COMP *cpi, BLOCK_SIZE bsize,
                            MACROBLOCK *x, MACROBLOCKD *xd,
                            int *out_rate_sum, int64_t *out_dist_sum,
                            int *skip_txfm_sb, int64_t *skip_sse_sb) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  int i;
  int64_t rate_sum = 0;
  int64_t dist_sum = 0;
  const int ref = xd->mi[0]->mbmi.ref_frame[0];
  unsigned int sse;
  unsigned int var = 0;
  unsigned int sum_sse = 0;
  int64_t total_sse = 0;
  int skip_flag = 1;
  const int shift = 6;
  int rate;
  int64_t dist;

  x->pred_sse[ref] = 0;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    struct macroblock_plane *const p = &x->plane[i];
    struct macroblockd_plane *const pd = &xd->plane[i];
    const BLOCK_SIZE bs = get_plane_block_size(bsize, pd);
    const TX_SIZE max_tx_size = max_txsize_lookup[bs];
    const BLOCK_SIZE unit_size = txsize_to_bsize[max_tx_size];
    const int64_t dc_thr = p->quant_thred[0] >> shift;
    const int64_t ac_thr = p->quant_thred[1] >> shift;
    // The low thresholds are used to measure if the prediction errors are
    // low enough so that we can skip the mode search.
    const int64_t low_dc_thr = MIN(50, dc_thr >> 2);
    const int64_t low_ac_thr = MIN(80, ac_thr >> 2);
    int bw = 1 << (b_width_log2_lookup[bs] - b_width_log2_lookup[unit_size]);
    int bh = 1 << (b_height_log2_lookup[bs] - b_width_log2_lookup[unit_size]);
    int idx, idy;
    int lw = b_width_log2_lookup[unit_size] + 2;
    int lh = b_height_log2_lookup[unit_size] + 2;

    sum_sse = 0;

    for (idy = 0; idy < bh; ++idy) {
      for (idx = 0; idx < bw; ++idx) {
        uint8_t *src = p->src.buf + (idy * p->src.stride << lh) + (idx << lw);
        uint8_t *dst = pd->dst.buf + (idy * pd->dst.stride << lh) + (idx << lh);
        int block_idx = (idy << 1) + idx;
        int low_err_skip = 0;

        var = cpi->fn_ptr[unit_size].vf(src, p->src.stride,
                                        dst, pd->dst.stride, &sse);
        x->bsse[(i << 2) + block_idx] = sse;
        sum_sse += sse;

        x->skip_txfm[(i << 2) + block_idx] = 0;
        if (!x->select_tx_size) {
          // Check if all ac coefficients can be quantized to zero.
          if (var < ac_thr || var == 0) {
            x->skip_txfm[(i << 2) + block_idx] = 2;

            // Check if dc coefficient can be quantized to zero.
            if (sse - var < dc_thr || sse == var) {
              x->skip_txfm[(i << 2) + block_idx] = 1;

              if (!sse || (var < low_ac_thr && sse - var < low_dc_thr))
                low_err_skip = 1;
            }
          }
        }

        if (skip_flag && !low_err_skip)
          skip_flag = 0;

        if (i == 0)
          x->pred_sse[ref] += sse;
      }
    }

    total_sse += sum_sse;

    // Fast approximate the modelling function.
    if (cpi->oxcf.speed > 4) {
      int64_t rate;
      const int64_t square_error = sum_sse;
      int quantizer = (pd->dequant[1] >> 3);
#if CONFIG_VP9_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        quantizer >>= (xd->bd - 8);
      }
#endif  // CONFIG_VP9_HIGHBITDEPTH

      if (quantizer < 120)
        rate = (square_error * (280 - quantizer)) >> 8;
      else
        rate = 0;
      dist = (square_error * quantizer) >> 8;
      rate_sum += rate;
      dist_sum += dist;
    } else {
#if CONFIG_VP9_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        vp9_model_rd_from_var_lapndz(sum_sse, num_pels_log2_lookup[bs],
                                     pd->dequant[1] >> (xd->bd - 5),
                                     &rate, &dist);
      } else {
        vp9_model_rd_from_var_lapndz(sum_sse, num_pels_log2_lookup[bs],
                                     pd->dequant[1] >> 3, &rate, &dist);
      }
#else
      vp9_model_rd_from_var_lapndz(sum_sse, num_pels_log2_lookup[bs],
                                   pd->dequant[1] >> 3, &rate, &dist);
#endif  // CONFIG_VP9_HIGHBITDEPTH
      rate_sum += rate;
      dist_sum += dist;
    }
  }

  *skip_txfm_sb = skip_flag;
  *skip_sse_sb = total_sse << 4;
  *out_rate_sum = (int)rate_sum;
  *out_dist_sum = dist_sum << 4;
}

int64_t vp9_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                          intptr_t block_size, int64_t *ssz) {
  int i;
  int64_t error = 0, sqcoeff = 0;

  for (i = 0; i < block_size; i++) {
    const int diff = coeff[i] - dqcoeff[i];
    error +=  diff * diff;
    sqcoeff += coeff[i] * coeff[i];
  }

  *ssz = sqcoeff;
  return error;
}

int64_t vp9_block_error_fp_c(const int16_t *coeff, const int16_t *dqcoeff,
                             int block_size) {
  int i;
  int64_t error = 0;

  for (i = 0; i < block_size; i++) {
    const int diff = coeff[i] - dqcoeff[i];
    error +=  diff * diff;
  }

  return error;
}

#if CONFIG_VP9_HIGHBITDEPTH
int64_t vp9_highbd_block_error_c(const tran_low_t *coeff,
                                 const tran_low_t *dqcoeff,
                                 intptr_t block_size,
                                 int64_t *ssz, int bd) {
  int i;
  int64_t error = 0, sqcoeff = 0;
  int shift = 2 * (bd - 8);
  int rounding = shift > 0 ? 1 << (shift - 1) : 0;

  for (i = 0; i < block_size; i++) {
    const int64_t diff = coeff[i] - dqcoeff[i];
    error +=  diff * diff;
    sqcoeff += (int64_t)coeff[i] * (int64_t)coeff[i];
  }
  assert(error >= 0 && sqcoeff >= 0);
  error = (error + rounding) >> shift;
  sqcoeff = (sqcoeff + rounding) >> shift;

  *ssz = sqcoeff;
  return error;
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

/* The trailing '0' is a terminator which is used inside cost_coeffs() to
 * decide whether to include cost of a trailing EOB node or not (i.e. we
 * can skip this if the last coefficient in this transform block, e.g. the
 * 16th coefficient in a 4x4 block or the 64th coefficient in a 8x8 block,
 * were non-zero). */
static const int16_t band_counts[TX_SIZES][8] = {
  { 1, 2, 3, 4,  3,   16 - 13, 0 },
  { 1, 2, 3, 4, 11,   64 - 21, 0 },
  { 1, 2, 3, 4, 11,  256 - 21, 0 },
  { 1, 2, 3, 4, 11, 1024 - 21, 0 },
};
static int cost_coeffs(MACROBLOCK *x,
                       int plane, int block,
                       ENTROPY_CONTEXT *A, ENTROPY_CONTEXT *L,
                       TX_SIZE tx_size,
                       const int16_t *scan, const int16_t *nb,
                       int use_fast_coef_costing) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const struct macroblockd_plane *pd = &xd->plane[plane];
  const PLANE_TYPE type = pd->plane_type;
  const int16_t *band_count = &band_counts[tx_size][1];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  unsigned int (*token_costs)[2][COEFF_CONTEXTS][ENTROPY_TOKENS] =
                   x->token_costs[tx_size][type][is_inter_block(mbmi)];
  uint8_t token_cache[32 * 32];
  int pt = combine_entropy_contexts(*A, *L);
  int c, cost;
#if CONFIG_VP9_HIGHBITDEPTH
  const int16_t *cat6_high_cost = vp9_get_high_cost_table(xd->bd);
#else
  const int16_t *cat6_high_cost = vp9_get_high_cost_table(8);
#endif

  // Check for consistency of tx_size with mode info
  assert(type == PLANE_TYPE_Y ? mbmi->tx_size == tx_size
                              : get_uv_tx_size(mbmi, pd) == tx_size);

  if (eob == 0) {
    // single eob token
    cost = token_costs[0][0][pt][EOB_TOKEN];
    c = 0;
  } else {
    int band_left = *band_count++;

    // dc token
    int v = qcoeff[0];
    int16_t prev_t;
    EXTRABIT e;
    vp9_get_token_extra(v, &prev_t, &e);
    cost = (*token_costs)[0][pt][prev_t] +
        vp9_get_cost(prev_t, e, cat6_high_cost);

    token_cache[0] = vp9_pt_energy_class[prev_t];
    ++token_costs;

    // ac tokens
    for (c = 1; c < eob; c++) {
      const int rc = scan[c];
      int16_t t;

      v = qcoeff[rc];
      vp9_get_token_extra(v, &t, &e);
      if (use_fast_coef_costing) {
        cost += (*token_costs)[!prev_t][!prev_t][t] +
            vp9_get_cost(t, e, cat6_high_cost);
      } else {
        pt = get_coef_context(nb, token_cache, c);
        cost += (*token_costs)[!prev_t][pt][t] +
            vp9_get_cost(t, e, cat6_high_cost);
        token_cache[rc] = vp9_pt_energy_class[t];
      }
      prev_t = t;
      if (!--band_left) {
        band_left = *band_count++;
        ++token_costs;
      }
    }

    // eob token
    if (band_left) {
      if (use_fast_coef_costing) {
        cost += (*token_costs)[0][!prev_t][EOB_TOKEN];
      } else {
        pt = get_coef_context(nb, token_cache, c);
        cost += (*token_costs)[0][pt][EOB_TOKEN];
      }
    }
  }

  // is eob first coefficient;
  *A = *L = (c > 0);

  return cost;
}

#if CONFIG_VP9_HIGHBITDEPTH
static void dist_block(int plane, int block, TX_SIZE tx_size,
                       struct rdcost_block_args* args, int bd) {
#else
static void dist_block(int plane, int block, TX_SIZE tx_size,
                       struct rdcost_block_args* args) {
#endif  // CONFIG_VP9_HIGHBITDEPTH
  const int ss_txfrm_size = tx_size << 1;
  MACROBLOCK* const x = args->x;
  MACROBLOCKD* const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  int64_t this_sse;
  int shift = tx_size == TX_32X32 ? 0 : 2;
  tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
#if CONFIG_VP9_HIGHBITDEPTH
  args->dist = vp9_highbd_block_error(coeff, dqcoeff, 16 << ss_txfrm_size,
                                      &this_sse, bd) >> shift;
#else
  args->dist = vp9_block_error(coeff, dqcoeff, 16 << ss_txfrm_size,
                               &this_sse) >> shift;
#endif  // CONFIG_VP9_HIGHBITDEPTH
  args->sse  = this_sse >> shift;

  if (x->skip_encode && !is_inter_block(&xd->mi[0]->mbmi)) {
    // TODO(jingning): tune the model to better capture the distortion.
    int64_t p = (pd->dequant[1] * pd->dequant[1] *
                    (1 << ss_txfrm_size)) >> (shift + 2);
#if CONFIG_VP9_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      p >>= ((xd->bd - 8) * 2);
    }
#endif  // CONFIG_VP9_HIGHBITDEPTH
    args->dist += (p >> 4);
    args->sse  += p;
  }
}

static void rate_block(int plane, int block, BLOCK_SIZE plane_bsize,
                       TX_SIZE tx_size, struct rdcost_block_args* args) {
  int x_idx, y_idx;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &x_idx, &y_idx);

  args->rate = cost_coeffs(args->x, plane, block, args->t_above + x_idx,
                           args->t_left + y_idx, tx_size,
                           args->so->scan, args->so->neighbors,
                           args->use_fast_coef_costing);
}

static void block_rd_txfm(int plane, int block, BLOCK_SIZE plane_bsize,
                          TX_SIZE tx_size, void *arg) {
  struct rdcost_block_args *args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  int64_t rd1, rd2, rd;

  if (args->skip)
    return;

  if (!is_inter_block(mbmi)) {
    struct encode_b_args arg = {x, NULL, &mbmi->skip};
    vp9_encode_block_intra(plane, block, plane_bsize, tx_size, &arg);
#if CONFIG_VP9_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      dist_block(plane, block, tx_size, args, xd->bd);
    } else {
      dist_block(plane, block, tx_size, args, 8);
    }
#else
    dist_block(plane, block, tx_size, args);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  } else if (max_txsize_lookup[plane_bsize] == tx_size) {
    if (x->skip_txfm[(plane << 2) + (block >> (tx_size << 1))] == 0) {
      // full forward transform and quantization
      vp9_xform_quant(x, plane, block, plane_bsize, tx_size);
#if CONFIG_VP9_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        dist_block(plane, block, tx_size, args, xd->bd);
      } else {
        dist_block(plane, block, tx_size, args, 8);
      }
#else
      dist_block(plane, block, tx_size, args);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    } else if (x->skip_txfm[(plane << 2) + (block >> (tx_size << 1))] == 2) {
      // compute DC coefficient
      tran_low_t *const coeff   = BLOCK_OFFSET(x->plane[plane].coeff, block);
      tran_low_t *const dqcoeff = BLOCK_OFFSET(xd->plane[plane].dqcoeff, block);
      vp9_xform_quant_dc(x, plane, block, plane_bsize, tx_size);
      args->sse  = x->bsse[(plane << 2) + (block >> (tx_size << 1))] << 4;
      args->dist = args->sse;
      if (x->plane[plane].eobs[block]) {
        const int64_t orig_sse = (int64_t)coeff[0] * coeff[0];
        const int64_t resd_sse = coeff[0] - dqcoeff[0];
        int64_t dc_correct = orig_sse - resd_sse * resd_sse;
#if CONFIG_VP9_HIGHBITDEPTH
        dc_correct >>= ((xd->bd - 8) * 2);
#endif
        if (tx_size != TX_32X32)
          dc_correct >>= 2;

        args->dist = MAX(0, args->sse - dc_correct);
      }
    } else {
      // skip forward transform
      x->plane[plane].eobs[block] = 0;
      args->sse  = x->bsse[(plane << 2) + (block >> (tx_size << 1))] << 4;
      args->dist = args->sse;
    }
  } else {
    // full forward transform and quantization
    vp9_xform_quant(x, plane, block, plane_bsize, tx_size);
#if CONFIG_VP9_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      dist_block(plane, block, tx_size, args, xd->bd);
    } else {
      dist_block(plane, block, tx_size, args, 8);
    }
#else
    dist_block(plane, block, tx_size, args);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  }

  rate_block(plane, block, plane_bsize, tx_size, args);
  rd1 = RDCOST(x->rdmult, x->rddiv, args->rate, args->dist);
  rd2 = RDCOST(x->rdmult, x->rddiv, 0, args->sse);

  // TODO(jingning): temporarily enabled only for luma component
  rd = MIN(rd1, rd2);
  if (plane == 0)
    x->zcoeff_blk[tx_size][block] = !x->plane[plane].eobs[block] ||
                                    (rd1 > rd2 && !xd->lossless);

  args->this_rate += args->rate;
  args->this_dist += args->dist;
  args->this_sse  += args->sse;
  args->this_rd += rd;

  if (args->this_rd > args->best_rd) {
    args->skip = 1;
    return;
  }
}

static void txfm_rd_in_plane(MACROBLOCK *x,
                             int *rate, int64_t *distortion,
                             int *skippable, int64_t *sse,
                             int64_t ref_best_rd, int plane,
                             BLOCK_SIZE bsize, TX_SIZE tx_size,
                             int use_fast_coef_casting) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  struct rdcost_block_args args;
  vp9_zero(args);
  args.x = x;
  args.best_rd = ref_best_rd;
  args.use_fast_coef_costing = use_fast_coef_casting;

  if (plane == 0)
    xd->mi[0]->mbmi.tx_size = tx_size;

  vp9_get_entropy_contexts(bsize, tx_size, pd, args.t_above, args.t_left);

  args.so = get_scan(xd, tx_size, pd->plane_type, 0);

  vp9_foreach_transformed_block_in_plane(xd, bsize, plane,
                                         block_rd_txfm, &args);
  if (args.skip) {
    *rate       = INT_MAX;
    *distortion = INT64_MAX;
    *sse        = INT64_MAX;
    *skippable  = 0;
  } else {
    *distortion = args.this_dist;
    *rate       = args.this_rate;
    *sse        = args.this_sse;
    *skippable  = vp9_is_skippable_in_plane(x, bsize, plane);
  }
}

static void choose_largest_tx_size(VP9_COMP *cpi, MACROBLOCK *x,
                                   int *rate, int64_t *distortion,
                                   int *skip, int64_t *sse,
                                   int64_t ref_best_rd,
                                   BLOCK_SIZE bs) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  VP9_COMMON *const cm = &cpi->common;
  const TX_SIZE largest_tx_size = tx_mode_to_biggest_tx_size[cm->tx_mode];
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;

  mbmi->tx_size = MIN(max_tx_size, largest_tx_size);

  txfm_rd_in_plane(x, rate, distortion, skip,
                   sse, ref_best_rd, 0, bs,
                   mbmi->tx_size, cpi->sf.use_fast_coef_costing);
}

static void choose_tx_size_from_rd(VP9_COMP *cpi, MACROBLOCK *x,
                                   int *rate,
                                   int64_t *distortion,
                                   int *skip,
                                   int64_t *psse,
                                   int64_t tx_cache[TX_MODES],
                                   int64_t ref_best_rd,
                                   BLOCK_SIZE bs) {
  const TX_SIZE max_tx_size = max_txsize_lookup[bs];
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  vp9_prob skip_prob = vp9_get_skip_prob(cm, xd);
  int r[TX_SIZES][2], s[TX_SIZES];
  int64_t d[TX_SIZES], sse[TX_SIZES];
  int64_t rd[TX_SIZES][2] = {{INT64_MAX, INT64_MAX},
                             {INT64_MAX, INT64_MAX},
                             {INT64_MAX, INT64_MAX},
                             {INT64_MAX, INT64_MAX}};
  int n, m;
  int s0, s1;
  const TX_SIZE max_mode_tx_size = tx_mode_to_biggest_tx_size[cm->tx_mode];
  int64_t best_rd = INT64_MAX;
  TX_SIZE best_tx = max_tx_size;

  const vp9_prob *tx_probs = get_tx_probs2(max_tx_size, xd, &cm->fc->tx_probs);
  assert(skip_prob > 0);
  s0 = vp9_cost_bit(skip_prob, 0);
  s1 = vp9_cost_bit(skip_prob, 1);

  for (n = max_tx_size; n >= 0;  n--) {
    txfm_rd_in_plane(x, &r[n][0], &d[n], &s[n],
                     &sse[n], ref_best_rd, 0, bs, n,
                     cpi->sf.use_fast_coef_costing);
    r[n][1] = r[n][0];
    if (r[n][0] < INT_MAX) {
      for (m = 0; m <= n - (n == (int) max_tx_size); m++) {
        if (m == n)
          r[n][1] += vp9_cost_zero(tx_probs[m]);
        else
          r[n][1] += vp9_cost_one(tx_probs[m]);
      }
    }
    if (d[n] == INT64_MAX) {
      rd[n][0] = rd[n][1] = INT64_MAX;
    } else if (s[n]) {
      rd[n][0] = rd[n][1] = RDCOST(x->rdmult, x->rddiv, s1, d[n]);
    } else {
      rd[n][0] = RDCOST(x->rdmult, x->rddiv, r[n][0] + s0, d[n]);
      rd[n][1] = RDCOST(x->rdmult, x->rddiv, r[n][1] + s0, d[n]);
    }

    // Early termination in transform size search.
    if (cpi->sf.tx_size_search_breakout &&
        (rd[n][1] == INT64_MAX ||
        (n < (int) max_tx_size && rd[n][1] > rd[n + 1][1]) ||
        s[n] == 1))
      break;

    if (rd[n][1] < best_rd) {
      best_tx = n;
      best_rd = rd[n][1];
    }
  }
  mbmi->tx_size = cm->tx_mode == TX_MODE_SELECT ?
                      best_tx : MIN(max_tx_size, max_mode_tx_size);


  *distortion = d[mbmi->tx_size];
  *rate       = r[mbmi->tx_size][cm->tx_mode == TX_MODE_SELECT];
  *skip       = s[mbmi->tx_size];
  *psse       = sse[mbmi->tx_size];

  tx_cache[ONLY_4X4] = rd[TX_4X4][0];
  tx_cache[ALLOW_8X8] = rd[TX_8X8][0];
  tx_cache[ALLOW_16X16] = rd[MIN(max_tx_size, TX_16X16)][0];
  tx_cache[ALLOW_32X32] = rd[MIN(max_tx_size, TX_32X32)][0];

  if (max_tx_size == TX_32X32 && best_tx == TX_32X32) {
    tx_cache[TX_MODE_SELECT] = rd[TX_32X32][1];
  } else if (max_tx_size >= TX_16X16 && best_tx == TX_16X16) {
    tx_cache[TX_MODE_SELECT] = rd[TX_16X16][1];
  } else if (rd[TX_8X8][1] < rd[TX_4X4][1]) {
    tx_cache[TX_MODE_SELECT] = rd[TX_8X8][1];
  } else {
    tx_cache[TX_MODE_SELECT] = rd[TX_4X4][1];
  }
}

static void super_block_yrd(VP9_COMP *cpi, MACROBLOCK *x, int *rate,
                            int64_t *distortion, int *skip,
                            int64_t *psse, BLOCK_SIZE bs,
                            int64_t txfm_cache[TX_MODES],
                            int64_t ref_best_rd) {
  MACROBLOCKD *xd = &x->e_mbd;
  int64_t sse;
  int64_t *ret_sse = psse ? psse : &sse;

  assert(bs == xd->mi[0]->mbmi.sb_type);

  if (cpi->sf.tx_size_search_method == USE_LARGESTALL || xd->lossless) {
    memset(txfm_cache, 0, TX_MODES * sizeof(int64_t));
    choose_largest_tx_size(cpi, x, rate, distortion, skip, ret_sse, ref_best_rd,
                           bs);
  } else {
    choose_tx_size_from_rd(cpi, x, rate, distortion, skip, ret_sse,
                           txfm_cache, ref_best_rd, bs);
  }
}

static int conditional_skipintra(PREDICTION_MODE mode,
                                 PREDICTION_MODE best_intra_mode) {
  if (mode == D117_PRED &&
      best_intra_mode != V_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  if (mode == D63_PRED &&
      best_intra_mode != V_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D207_PRED &&
      best_intra_mode != H_PRED &&
      best_intra_mode != D45_PRED)
    return 1;
  if (mode == D153_PRED &&
      best_intra_mode != H_PRED &&
      best_intra_mode != D135_PRED)
    return 1;
  return 0;
}

static int64_t rd_pick_intra4x4block(VP9_COMP *cpi, MACROBLOCK *x, int ib,
                                     PREDICTION_MODE *best_mode,
                                     const int *bmode_costs,
                                     ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l,
                                     int *bestrate, int *bestratey,
                                     int64_t *bestdistortion,
                                     BLOCK_SIZE bsize, int64_t rd_thresh) {
  PREDICTION_MODE mode;
  MACROBLOCKD *const xd = &x->e_mbd;
  int64_t best_rd = rd_thresh;

  struct macroblock_plane *p = &x->plane[0];
  struct macroblockd_plane *pd = &xd->plane[0];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const uint8_t *src_init = &p->src.buf[vp9_raster_block_offset(BLOCK_8X8, ib,
                                                                src_stride)];
  uint8_t *dst_init = &pd->dst.buf[vp9_raster_block_offset(BLOCK_8X8, ib,
                                                           dst_stride)];
  ENTROPY_CONTEXT ta[2], tempa[2];
  ENTROPY_CONTEXT tl[2], templ[2];

  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int idx, idy;
  uint8_t best_dst[8 * 8];
#if CONFIG_VP9_HIGHBITDEPTH
  uint16_t best_dst16[8 * 8];
#endif

  assert(ib < 4);

  memcpy(ta, a, sizeof(ta));
  memcpy(tl, l, sizeof(tl));
  xd->mi[0]->mbmi.tx_size = TX_4X4;

#if CONFIG_VP9_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
      int64_t this_rd;
      int ratey = 0;
      int64_t distortion = 0;
      int rate = bmode_costs[mode];

      if (!(cpi->sf.intra_y_mode_mask[TX_4X4] & (1 << mode)))
        continue;

      // Only do the oblique modes if the best so far is
      // one of the neighboring directional modes
      if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
        if (conditional_skipintra(mode, *best_mode))
            continue;
      }

      memcpy(tempa, ta, sizeof(ta));
      memcpy(templ, tl, sizeof(tl));

      for (idy = 0; idy < num_4x4_blocks_high; ++idy) {
        for (idx = 0; idx < num_4x4_blocks_wide; ++idx) {
          const int block = ib + idy * 2 + idx;
          const uint8_t *const src = &src_init[idx * 4 + idy * 4 * src_stride];
          uint8_t *const dst = &dst_init[idx * 4 + idy * 4 * dst_stride];
          int16_t *const src_diff = vp9_raster_block_offset_int16(BLOCK_8X8,
                                                                  block,
                                                                  p->src_diff);
          tran_low_t *const coeff = BLOCK_OFFSET(x->plane[0].coeff, block);
          xd->mi[0]->bmi[block].as_mode = mode;
          vp9_predict_intra_block(xd, block, 1,
                                  TX_4X4, mode,
                                  x->skip_encode ? src : dst,
                                  x->skip_encode ? src_stride : dst_stride,
                                  dst, dst_stride, idx, idy, 0);
          vp9_highbd_subtract_block(4, 4, src_diff, 8, src, src_stride,
                                    dst, dst_stride, xd->bd);
          if (xd->lossless) {
            const scan_order *so = &vp9_default_scan_orders[TX_4X4];
            vp9_highbd_fwht4x4(src_diff, coeff, 8);
            vp9_regular_quantize_b_4x4(x, 0, block, so->scan, so->iscan);
            ratey += cost_coeffs(x, 0, block, tempa + idx, templ + idy, TX_4X4,
                                 so->scan, so->neighbors,
                                 cpi->sf.use_fast_coef_costing);
            if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
              goto next_highbd;
            vp9_highbd_iwht4x4_add(BLOCK_OFFSET(pd->dqcoeff, block),
                                   dst, dst_stride,
                                   p->eobs[block], xd->bd);
          } else {
            int64_t unused;
            const TX_TYPE tx_type = get_tx_type_4x4(PLANE_TYPE_Y, xd, block);
            const scan_order *so = &vp9_scan_orders[TX_4X4][tx_type];
            vp9_highbd_fht4x4(src_diff, coeff, 8, tx_type);
            vp9_regular_quantize_b_4x4(x, 0, block, so->scan, so->iscan);
            ratey += cost_coeffs(x, 0, block, tempa + idx, templ + idy, TX_4X4,
                                 so->scan, so->neighbors,
                                 cpi->sf.use_fast_coef_costing);
            distortion += vp9_highbd_block_error(
                coeff, BLOCK_OFFSET(pd->dqcoeff, block),
                16, &unused, xd->bd) >> 2;
            if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
              goto next_highbd;
            vp9_highbd_iht4x4_add(tx_type, BLOCK_OFFSET(pd->dqcoeff, block),
                                  dst, dst_stride, p->eobs[block], xd->bd);
          }
        }
      }

      rate += ratey;
      this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

      if (this_rd < best_rd) {
        *bestrate = rate;
        *bestratey = ratey;
        *bestdistortion = distortion;
        best_rd = this_rd;
        *best_mode = mode;
        memcpy(a, tempa, sizeof(tempa));
        memcpy(l, templ, sizeof(templ));
        for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy) {
          memcpy(best_dst16 + idy * 8,
                 CONVERT_TO_SHORTPTR(dst_init + idy * dst_stride),
                 num_4x4_blocks_wide * 4 * sizeof(uint16_t));
        }
      }
    next_highbd:
      {}
    }
    if (best_rd >= rd_thresh || x->skip_encode)
      return best_rd;

    for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy) {
      memcpy(CONVERT_TO_SHORTPTR(dst_init + idy * dst_stride),
             best_dst16 + idy * 8,
             num_4x4_blocks_wide * 4 * sizeof(uint16_t));
    }

    return best_rd;
  }
#endif  // CONFIG_VP9_HIGHBITDEPTH

  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
    int64_t this_rd;
    int ratey = 0;
    int64_t distortion = 0;
    int rate = bmode_costs[mode];

    if (!(cpi->sf.intra_y_mode_mask[TX_4X4] & (1 << mode)))
      continue;

    // Only do the oblique modes if the best so far is
    // one of the neighboring directional modes
    if (cpi->sf.mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
      if (conditional_skipintra(mode, *best_mode))
          continue;
    }

    memcpy(tempa, ta, sizeof(ta));
    memcpy(templ, tl, sizeof(tl));

    for (idy = 0; idy < num_4x4_blocks_high; ++idy) {
      for (idx = 0; idx < num_4x4_blocks_wide; ++idx) {
        const int block = ib + idy * 2 + idx;
        const uint8_t *const src = &src_init[idx * 4 + idy * 4 * src_stride];
        uint8_t *const dst = &dst_init[idx * 4 + idy * 4 * dst_stride];
        int16_t *const src_diff =
            vp9_raster_block_offset_int16(BLOCK_8X8, block, p->src_diff);
        tran_low_t *const coeff = BLOCK_OFFSET(x->plane[0].coeff, block);
        xd->mi[0]->bmi[block].as_mode = mode;
        vp9_predict_intra_block(xd, block, 1,
                                TX_4X4, mode,
                                x->skip_encode ? src : dst,
                                x->skip_encode ? src_stride : dst_stride,
                                dst, dst_stride, idx, idy, 0);
        vp9_subtract_block(4, 4, src_diff, 8, src, src_stride, dst, dst_stride);

        if (xd->lossless) {
          const scan_order *so = &vp9_default_scan_orders[TX_4X4];
          vp9_fwht4x4(src_diff, coeff, 8);
          vp9_regular_quantize_b_4x4(x, 0, block, so->scan, so->iscan);
          ratey += cost_coeffs(x, 0, block, tempa + idx, templ + idy, TX_4X4,
                               so->scan, so->neighbors,
                               cpi->sf.use_fast_coef_costing);
          if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
            goto next;
          vp9_iwht4x4_add(BLOCK_OFFSET(pd->dqcoeff, block), dst, dst_stride,
                          p->eobs[block]);
        } else {
          int64_t unused;
          const TX_TYPE tx_type = get_tx_type_4x4(PLANE_TYPE_Y, xd, block);
          const scan_order *so = &vp9_scan_orders[TX_4X4][tx_type];
          vp9_fht4x4(src_diff, coeff, 8, tx_type);
          vp9_regular_quantize_b_4x4(x, 0, block, so->scan, so->iscan);
          ratey += cost_coeffs(x, 0, block, tempa + idx, templ + idy, TX_4X4,
                             so->scan, so->neighbors,
                             cpi->sf.use_fast_coef_costing);
          distortion += vp9_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, block),
                                        16, &unused) >> 2;
          if (RDCOST(x->rdmult, x->rddiv, ratey, distortion) >= best_rd)
            goto next;
          vp9_iht4x4_add(tx_type, BLOCK_OFFSET(pd->dqcoeff, block),
                         dst, dst_stride, p->eobs[block]);
        }
      }
    }

    rate += ratey;
    this_rd = RDCOST(x->rdmult, x->rddiv, rate, distortion);

    if (this_rd < best_rd) {
      *bestrate = rate;
      *bestratey = ratey;
      *bestdistortion = distortion;
      best_rd = this_rd;
      *best_mode = mode;
      memcpy(a, tempa, sizeof(tempa));
      memcpy(l, templ, sizeof(templ));
      for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy)
        memcpy(best_dst + idy * 8, dst_init + idy * dst_stride,
               num_4x4_blocks_wide * 4);
    }
  next:
    {}
  }

  if (best_rd >= rd_thresh || x->skip_encode)
    return best_rd;

  for (idy = 0; idy < num_4x4_blocks_high * 4; ++idy)
    memcpy(dst_init + idy * dst_stride, best_dst + idy * 8,
           num_4x4_blocks_wide * 4);

  return best_rd;
}

static int64_t rd_pick_intra_sub_8x8_y_mode(VP9_COMP *cpi, MACROBLOCK *mb,
                                            int *rate, int *rate_y,
                                            int64_t *distortion,
                                            int64_t best_rd) {
  int i, j;
  const MACROBLOCKD *const xd = &mb->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int idx, idy;
  int cost = 0;
  int64_t total_distortion = 0;
  int tot_rate_y = 0;
  int64_t total_rd = 0;
  ENTROPY_CONTEXT t_above[4], t_left[4];
  const int *bmode_costs = cpi->mbmode_cost;

  memcpy(t_above, xd->plane[0].above_context, sizeof(t_above));
  memcpy(t_left, xd->plane[0].left_context, sizeof(t_left));

  // Pick modes for each sub-block (of size 4x4, 4x8, or 8x4) in an 8x8 block.
  for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
    for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
      PREDICTION_MODE best_mode = DC_PRED;
      int r = INT_MAX, ry = INT_MAX;
      int64_t d = INT64_MAX, this_rd = INT64_MAX;
      i = idy * 2 + idx;
      if (cpi->common.frame_type == KEY_FRAME) {
        const PREDICTION_MODE A = vp9_above_block_mode(mic, above_mi, i);
        const PREDICTION_MODE L = vp9_left_block_mode(mic, left_mi, i);

        bmode_costs  = cpi->y_mode_costs[A][L];
      }

      this_rd = rd_pick_intra4x4block(cpi, mb, i, &best_mode, bmode_costs,
                                      t_above + idx, t_left + idy, &r, &ry, &d,
                                      bsize, best_rd - total_rd);
      if (this_rd >= best_rd - total_rd)
        return INT64_MAX;

      total_rd += this_rd;
      cost += r;
      total_distortion += d;
      tot_rate_y += ry;

      mic->bmi[i].as_mode = best_mode;
      for (j = 1; j < num_4x4_blocks_high; ++j)
        mic->bmi[i + j * 2].as_mode = best_mode;
      for (j = 1; j < num_4x4_blocks_wide; ++j)
        mic->bmi[i + j].as_mode = best_mode;

      if (total_rd >= best_rd)
        return INT64_MAX;
    }
  }

  *rate = cost;
  *rate_y = tot_rate_y;
  *distortion = total_distortion;
  mic->mbmi.mode = mic->bmi[3].as_mode;

  return RDCOST(mb->rdmult, mb->rddiv, cost, total_distortion);
}

// This function is used only for intra_only frames
static int64_t rd_pick_intra_sby_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                      int *rate, int *rate_tokenonly,
                                      int64_t *distortion, int *skippable,
                                      BLOCK_SIZE bsize,
                                      int64_t tx_cache[TX_MODES],
                                      int64_t best_rd) {
  PREDICTION_MODE mode;
  PREDICTION_MODE mode_selected = DC_PRED;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mic = xd->mi[0];
  int this_rate, this_rate_tokenonly, s;
  int64_t this_distortion, this_rd;
  TX_SIZE best_tx = TX_4X4;
  int i;
  int *bmode_costs;
  const MODE_INFO *above_mi = xd->above_mi;
  const MODE_INFO *left_mi = xd->left_mi;
  const PREDICTION_MODE A = vp9_above_block_mode(mic, above_mi, 0);
  const PREDICTION_MODE L = vp9_left_block_mode(mic, left_mi, 0);
  bmode_costs = cpi->y_mode_costs[A][L];

  if (cpi->sf.tx_size_search_method == USE_FULL_RD)
    for (i = 0; i < TX_MODES; i++)
      tx_cache[i] = INT64_MAX;

  memset(x->skip_txfm, 0, sizeof(x->skip_txfm));
  /* Y Search for intra prediction mode */
  for (mode = DC_PRED; mode <= TM_PRED; mode++) {
    int64_t local_tx_cache[TX_MODES];

    if (cpi->sf.use_nonrd_pick_mode) {
      // These speed features are turned on in hybrid non-RD and RD mode
      // for key frame coding in the context of real-time setting.
      if (conditional_skipintra(mode, mode_selected))
          continue;
      if (*skippable)
        break;
    }

    mic->mbmi.mode = mode;

    super_block_yrd(cpi, x, &this_rate_tokenonly, &this_distortion,
        &s, NULL, bsize, local_tx_cache, best_rd);

    if (this_rate_tokenonly == INT_MAX)
      continue;

    this_rate = this_rate_tokenonly + bmode_costs[mode];
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);

    if (this_rd < best_rd) {
      mode_selected   = mode;
      best_rd         = this_rd;
      best_tx         = mic->mbmi.tx_size;
      *rate           = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion     = this_distortion;
      *skippable      = s;
    }

    if (cpi->sf.tx_size_search_method == USE_FULL_RD && this_rd < INT64_MAX) {
      for (i = 0; i < TX_MODES && local_tx_cache[i] < INT64_MAX; i++) {
        const int64_t adj_rd = this_rd + local_tx_cache[i] -
            local_tx_cache[cpi->common.tx_mode];
        if (adj_rd < tx_cache[i]) {
          tx_cache[i] = adj_rd;
        }
      }
    }
  }

  mic->mbmi.mode = mode_selected;
  mic->mbmi.tx_size = best_tx;

  return best_rd;
}

// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int super_block_uvrd(const VP9_COMP *cpi, MACROBLOCK *x,
                            int *rate, int64_t *distortion, int *skippable,
                            int64_t *sse, BLOCK_SIZE bsize,
                            int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const TX_SIZE uv_tx_size = get_uv_tx_size(mbmi, &xd->plane[1]);
  int plane;
  int pnrate = 0, pnskip = 1;
  int64_t pndist = 0, pnsse = 0;
  int is_cost_valid = 1;

  if (ref_best_rd < 0)
    is_cost_valid = 0;

  if (is_inter_block(mbmi) && is_cost_valid) {
    int plane;
    for (plane = 1; plane < MAX_MB_PLANE; ++plane)
      vp9_subtract_plane(x, bsize, plane);
  }

  *rate = 0;
  *distortion = 0;
  *sse = 0;
  *skippable = 1;

  for (plane = 1; plane < MAX_MB_PLANE; ++plane) {
    txfm_rd_in_plane(x, &pnrate, &pndist, &pnskip, &pnsse,
                     ref_best_rd, plane, bsize, uv_tx_size,
                     cpi->sf.use_fast_coef_costing);
    if (pnrate == INT_MAX) {
      is_cost_valid = 0;
      break;
    }
    *rate += pnrate;
    *distortion += pndist;
    *sse += pnsse;
    *skippable &= pnskip;
  }

  if (!is_cost_valid) {
    // reset cost value
    *rate = INT_MAX;
    *distortion = INT64_MAX;
    *sse = INT64_MAX;
    *skippable = 0;
  }

  return is_cost_valid;
}

static int64_t rd_pick_intra_sbuv_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                       PICK_MODE_CONTEXT *ctx,
                                       int *rate, int *rate_tokenonly,
                                       int64_t *distortion, int *skippable,
                                       BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  PREDICTION_MODE mode;
  PREDICTION_MODE mode_selected = DC_PRED;
  int64_t best_rd = INT64_MAX, this_rd;
  int this_rate_tokenonly, this_rate, s;
  int64_t this_distortion, this_sse;

  memset(x->skip_txfm, 0, sizeof(x->skip_txfm));
  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
    if (!(cpi->sf.intra_uv_mode_mask[max_tx_size] & (1 << mode)))
      continue;

    xd->mi[0]->mbmi.uv_mode = mode;

    if (!super_block_uvrd(cpi, x, &this_rate_tokenonly,
                          &this_distortion, &s, &this_sse, bsize, best_rd))
      continue;
    this_rate = this_rate_tokenonly +
                cpi->intra_uv_mode_cost[cpi->common.frame_type][mode];
    this_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_distortion);

    if (this_rd < best_rd) {
      mode_selected   = mode;
      best_rd         = this_rd;
      *rate           = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion     = this_distortion;
      *skippable      = s;
      if (!x->select_tx_size)
        swap_block_ptr(x, ctx, 2, 0, 1, MAX_MB_PLANE);
    }
  }

  xd->mi[0]->mbmi.uv_mode = mode_selected;
  return best_rd;
}

static int64_t rd_sbuv_dcpred(const VP9_COMP *cpi, MACROBLOCK *x,
                              int *rate, int *rate_tokenonly,
                              int64_t *distortion, int *skippable,
                              BLOCK_SIZE bsize) {
  const VP9_COMMON *cm = &cpi->common;
  int64_t unused;

  x->e_mbd.mi[0]->mbmi.uv_mode = DC_PRED;
  memset(x->skip_txfm, 0, sizeof(x->skip_txfm));
  super_block_uvrd(cpi, x, rate_tokenonly, distortion,
                   skippable, &unused, bsize, INT64_MAX);
  *rate = *rate_tokenonly + cpi->intra_uv_mode_cost[cm->frame_type][DC_PRED];
  return RDCOST(x->rdmult, x->rddiv, *rate, *distortion);
}

static void choose_intra_uv_mode(VP9_COMP *cpi, MACROBLOCK *const x,
                                 PICK_MODE_CONTEXT *ctx,
                                 BLOCK_SIZE bsize, TX_SIZE max_tx_size,
                                 int *rate_uv, int *rate_uv_tokenonly,
                                 int64_t *dist_uv, int *skip_uv,
                                 PREDICTION_MODE *mode_uv) {
  // Use an estimated rd for uv_intra based on DC_PRED if the
  // appropriate speed flag is set.
  if (cpi->sf.use_uv_intra_rd_estimate) {
    rd_sbuv_dcpred(cpi, x, rate_uv, rate_uv_tokenonly, dist_uv,
                   skip_uv, bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize);
  // Else do a proper rd search for each possible transform size that may
  // be considered in the main rd loop.
  } else {
    rd_pick_intra_sbuv_mode(cpi, x, ctx,
                            rate_uv, rate_uv_tokenonly, dist_uv, skip_uv,
                            bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize, max_tx_size);
  }
  *mode_uv = x->e_mbd.mi[0]->mbmi.uv_mode;
}

static int cost_mv_ref(const VP9_COMP *cpi, PREDICTION_MODE mode,
                       int mode_context) {
  assert(is_inter_mode(mode));
  return cpi->inter_mode_cost[mode_context][INTER_OFFSET(mode)];
}

static int set_and_cost_bmi_mvs(VP9_COMP *cpi, MACROBLOCKD *xd, int i,
                                PREDICTION_MODE mode, int_mv this_mv[2],
                                int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES],
                                int_mv seg_mvs[MAX_REF_FRAMES],
                                int_mv *best_ref_mv[2], const int *mvjcost,
                                int *mvcost[2]) {
  MODE_INFO *const mic = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mic->mbmi;
  int thismvcost = 0;
  int idx, idy;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[mbmi->sb_type];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[mbmi->sb_type];
  const int is_compound = has_second_ref(mbmi);

  switch (mode) {
    case NEWMV:
      this_mv[0].as_int = seg_mvs[mbmi->ref_frame[0]].as_int;
      thismvcost += vp9_mv_bit_cost(&this_mv[0].as_mv, &best_ref_mv[0]->as_mv,
                                    mvjcost, mvcost, MV_COST_WEIGHT_SUB);
      if (is_compound) {
        this_mv[1].as_int = seg_mvs[mbmi->ref_frame[1]].as_int;
        thismvcost += vp9_mv_bit_cost(&this_mv[1].as_mv, &best_ref_mv[1]->as_mv,
                                      mvjcost, mvcost, MV_COST_WEIGHT_SUB);
      }
      break;
    case NEARMV:
    case NEARESTMV:
      this_mv[0].as_int = frame_mv[mode][mbmi->ref_frame[0]].as_int;
      if (is_compound)
        this_mv[1].as_int = frame_mv[mode][mbmi->ref_frame[1]].as_int;
      break;
    case ZEROMV:
      this_mv[0].as_int = 0;
      if (is_compound)
        this_mv[1].as_int = 0;
      break;
    default:
      break;
  }

  mic->bmi[i].as_mv[0].as_int = this_mv[0].as_int;
  if (is_compound)
    mic->bmi[i].as_mv[1].as_int = this_mv[1].as_int;

  mic->bmi[i].as_mode = mode;

  for (idy = 0; idy < num_4x4_blocks_high; ++idy)
    for (idx = 0; idx < num_4x4_blocks_wide; ++idx)
      memmove(&mic->bmi[i + idy * 2 + idx], &mic->bmi[i], sizeof(mic->bmi[i]));

  return cost_mv_ref(cpi, mode, mbmi->mode_context[mbmi->ref_frame[0]]) +
            thismvcost;
}

static int64_t encode_inter_mb_segment(VP9_COMP *cpi,
                                       MACROBLOCK *x,
                                       int64_t best_yrd,
                                       int i,
                                       int *labelyrate,
                                       int64_t *distortion, int64_t *sse,
                                       ENTROPY_CONTEXT *ta,
                                       ENTROPY_CONTEXT *tl,
                                       int mi_row, int mi_col) {
  int k;
  MACROBLOCKD *xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[0];
  struct macroblock_plane *const p = &x->plane[0];
  MODE_INFO *const mi = xd->mi[0];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(mi->mbmi.sb_type, pd);
  const int width = 4 * num_4x4_blocks_wide_lookup[plane_bsize];
  const int height = 4 * num_4x4_blocks_high_lookup[plane_bsize];
  int idx, idy;

  const uint8_t *const src =
      &p->src.buf[vp9_raster_block_offset(BLOCK_8X8, i, p->src.stride)];
  uint8_t *const dst = &pd->dst.buf[vp9_raster_block_offset(BLOCK_8X8, i,
                                                            pd->dst.stride)];
  int64_t thisdistortion = 0, thissse = 0;
  int thisrate = 0, ref;
  const scan_order *so = &vp9_default_scan_orders[TX_4X4];
  const int is_compound = has_second_ref(&mi->mbmi);
  const InterpKernel *kernel = vp9_get_interp_kernel(mi->mbmi.interp_filter);

  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const uint8_t *pre = &pd->pre[ref].buf[vp9_raster_block_offset(BLOCK_8X8, i,
                                               pd->pre[ref].stride)];
#if CONFIG_VP9_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    vp9_highbd_build_inter_predictor(pre, pd->pre[ref].stride,
                                     dst, pd->dst.stride,
                                     &mi->bmi[i].as_mv[ref].as_mv,
                                     &xd->block_refs[ref]->sf, width, height,
                                     ref, kernel, MV_PRECISION_Q3,
                                     mi_col * MI_SIZE + 4 * (i % 2),
                                     mi_row * MI_SIZE + 4 * (i / 2), xd->bd);
  } else {
    vp9_build_inter_predictor(pre, pd->pre[ref].stride,
                              dst, pd->dst.stride,
                              &mi->bmi[i].as_mv[ref].as_mv,
                              &xd->block_refs[ref]->sf, width, height, ref,
                              kernel, MV_PRECISION_Q3,
                              mi_col * MI_SIZE + 4 * (i % 2),
                              mi_row * MI_SIZE + 4 * (i / 2));
  }
#else
    vp9_build_inter_predictor(pre, pd->pre[ref].stride,
                              dst, pd->dst.stride,
                              &mi->bmi[i].as_mv[ref].as_mv,
                              &xd->block_refs[ref]->sf, width, height, ref,
                              kernel, MV_PRECISION_Q3,
                              mi_col * MI_SIZE + 4 * (i % 2),
                              mi_row * MI_SIZE + 4 * (i / 2));
#endif  // CONFIG_VP9_HIGHBITDEPTH
  }

#if CONFIG_VP9_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    vp9_highbd_subtract_block(
        height, width, vp9_raster_block_offset_int16(BLOCK_8X8, i, p->src_diff),
        8, src, p->src.stride, dst, pd->dst.stride, xd->bd);
  } else {
    vp9_subtract_block(
        height, width, vp9_raster_block_offset_int16(BLOCK_8X8, i, p->src_diff),
        8, src, p->src.stride, dst, pd->dst.stride);
  }
#else
  vp9_subtract_block(height, width,
                     vp9_raster_block_offset_int16(BLOCK_8X8, i, p->src_diff),
                     8, src, p->src.stride, dst, pd->dst.stride);
#endif  // CONFIG_VP9_HIGHBITDEPTH

  k = i;
  for (idy = 0; idy < height / 4; ++idy) {
    for (idx = 0; idx < width / 4; ++idx) {
      int64_t ssz, rd, rd1, rd2;
      tran_low_t* coeff;

      k += (idy * 2 + idx);
      coeff = BLOCK_OFFSET(p->coeff, k);
      x->fwd_txm4x4(vp9_raster_block_offset_int16(BLOCK_8X8, k, p->src_diff),
                    coeff, 8);
      vp9_regular_quantize_b_4x4(x, 0, k, so->scan, so->iscan);
#if CONFIG_VP9_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        thisdistortion += vp9_highbd_block_error(coeff,
                                                 BLOCK_OFFSET(pd->dqcoeff, k),
                                                 16, &ssz, xd->bd);
      } else {
        thisdistortion += vp9_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, k),
                                          16, &ssz);
      }
#else
      thisdistortion += vp9_block_error(coeff, BLOCK_OFFSET(pd->dqcoeff, k),
                                        16, &ssz);
#endif  // CONFIG_VP9_HIGHBITDEPTH
      thissse += ssz;
      thisrate += cost_coeffs(x, 0, k, ta + (k & 1), tl + (k >> 1), TX_4X4,
                              so->scan, so->neighbors,
                              cpi->sf.use_fast_coef_costing);
      rd1 = RDCOST(x->rdmult, x->rddiv, thisrate, thisdistortion >> 2);
      rd2 = RDCOST(x->rdmult, x->rddiv, 0, thissse >> 2);
      rd = MIN(rd1, rd2);
      if (rd >= best_yrd)
        return INT64_MAX;
    }
  }

  *distortion = thisdistortion >> 2;
  *labelyrate = thisrate;
  *sse = thissse >> 2;

  return RDCOST(x->rdmult, x->rddiv, *labelyrate, *distortion);
}

typedef struct {
  int eobs;
  int brate;
  int byrate;
  int64_t bdist;
  int64_t bsse;
  int64_t brdcost;
  int_mv mvs[2];
  ENTROPY_CONTEXT ta[2];
  ENTROPY_CONTEXT tl[2];
} SEG_RDSTAT;

typedef struct {
  int_mv *ref_mv[2];
  int_mv mvp;

  int64_t segment_rd;
  int r;
  int64_t d;
  int64_t sse;
  int segment_yrate;
  PREDICTION_MODE modes[4];
  SEG_RDSTAT rdstat[4][INTER_MODES];
  int mvthresh;
} BEST_SEG_INFO;

static INLINE int mv_check_bounds(const MACROBLOCK *x, const MV *mv) {
  return (mv->row >> 3) < x->mv_row_min ||
         (mv->row >> 3) > x->mv_row_max ||
         (mv->col >> 3) < x->mv_col_min ||
         (mv->col >> 3) > x->mv_col_max;
}

static INLINE void mi_buf_shift(MACROBLOCK *x, int i) {
  MB_MODE_INFO *const mbmi = &x->e_mbd.mi[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &x->e_mbd.plane[0];

  p->src.buf = &p->src.buf[vp9_raster_block_offset(BLOCK_8X8, i,
                                                   p->src.stride)];
  assert(((intptr_t)pd->pre[0].buf & 0x7) == 0);
  pd->pre[0].buf = &pd->pre[0].buf[vp9_raster_block_offset(BLOCK_8X8, i,
                                                           pd->pre[0].stride)];
  if (has_second_ref(mbmi))
    pd->pre[1].buf = &pd->pre[1].buf[vp9_raster_block_offset(BLOCK_8X8, i,
                                                           pd->pre[1].stride)];
}

static INLINE void mi_buf_restore(MACROBLOCK *x, struct buf_2d orig_src,
                                  struct buf_2d orig_pre[2]) {
  MB_MODE_INFO *mbmi = &x->e_mbd.mi[0]->mbmi;
  x->plane[0].src = orig_src;
  x->e_mbd.plane[0].pre[0] = orig_pre[0];
  if (has_second_ref(mbmi))
    x->e_mbd.plane[0].pre[1] = orig_pre[1];
}

static INLINE int mv_has_subpel(const MV *mv) {
  return (mv->row & 0x0F) || (mv->col & 0x0F);
}

// Check if NEARESTMV/NEARMV/ZEROMV is the cheapest way encode zero motion.
// TODO(aconverse): Find out if this is still productive then clean up or remove
static int check_best_zero_mv(
    const VP9_COMP *cpi, const uint8_t mode_context[MAX_REF_FRAMES],
    int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES], int this_mode,
    const MV_REFERENCE_FRAME ref_frames[2]) {
  if ((this_mode == NEARMV || this_mode == NEARESTMV || this_mode == ZEROMV) &&
      frame_mv[this_mode][ref_frames[0]].as_int == 0 &&
      (ref_frames[1] == NONE ||
       frame_mv[this_mode][ref_frames[1]].as_int == 0)) {
    int rfc = mode_context[ref_frames[0]];
    int c1 = cost_mv_ref(cpi, NEARMV, rfc);
    int c2 = cost_mv_ref(cpi, NEARESTMV, rfc);
    int c3 = cost_mv_ref(cpi, ZEROMV, rfc);

    if (this_mode == NEARMV) {
      if (c1 > c3) return 0;
    } else if (this_mode == NEARESTMV) {
      if (c2 > c3) return 0;
    } else {
      assert(this_mode == ZEROMV);
      if (ref_frames[1] == NONE) {
        if ((c3 >= c2 && frame_mv[NEARESTMV][ref_frames[0]].as_int == 0) ||
            (c3 >= c1 && frame_mv[NEARMV][ref_frames[0]].as_int == 0))
          return 0;
      } else {
        if ((c3 >= c2 && frame_mv[NEARESTMV][ref_frames[0]].as_int == 0 &&
             frame_mv[NEARESTMV][ref_frames[1]].as_int == 0) ||
            (c3 >= c1 && frame_mv[NEARMV][ref_frames[0]].as_int == 0 &&
             frame_mv[NEARMV][ref_frames[1]].as_int == 0))
          return 0;
      }
    }
  }
  return 1;
}

static void joint_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                BLOCK_SIZE bsize,
                                int_mv *frame_mv,
                                int mi_row, int mi_col,
                                int_mv single_newmv[MAX_REF_FRAMES],
                                int *rate_mv) {
  const VP9_COMMON *const cm = &cpi->common;
  const int pw = 4 * num_4x4_blocks_wide_lookup[bsize];
  const int ph = 4 * num_4x4_blocks_high_lookup[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int refs[2] = {mbmi->ref_frame[0],
                       mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]};
  int_mv ref_mv[2];
  int ite, ref;
  const InterpKernel *kernel = vp9_get_interp_kernel(mbmi->interp_filter);
  struct scale_factors sf;

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  int last_besterr[2] = {INT_MAX, INT_MAX};
  const YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    vp9_get_scaled_ref_frame(cpi, mbmi->ref_frame[0]),
    vp9_get_scaled_ref_frame(cpi, mbmi->ref_frame[1])
  };

  // Prediction buffer from second frame.
#if CONFIG_VP9_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[64 * 64]);
  uint8_t *second_pred;
#else
  DECLARE_ALIGNED(16, uint8_t, second_pred[64 * 64]);
#endif  // CONFIG_VP9_HIGHBITDEPTH

  for (ref = 0; ref < 2; ++ref) {
    ref_mv[ref] = mbmi->ref_mvs[refs[ref]][0];

    if (scaled_ref_frame[ref]) {
      int i;
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      for (i = 0; i < MAX_MB_PLANE; i++)
        backup_yv12[ref][i] = xd->plane[i].pre[ref];
      vp9_setup_pre_planes(xd, ref, scaled_ref_frame[ref], mi_row, mi_col,
                           NULL);
    }

    frame_mv[refs[ref]].as_int = single_newmv[refs[ref]].as_int;
  }

  // Since we have scaled the reference frames to match the size of the current
  // frame we must use a unit scaling factor during mode selection.
#if CONFIG_VP9_HIGHBITDEPTH
  vp9_setup_scale_factors_for_frame(&sf, cm->width, cm->height,
                                    cm->width, cm->height,
                                    cm->use_highbitdepth);
#else
  vp9_setup_scale_factors_for_frame(&sf, cm->width, cm->height,
                                    cm->width, cm->height);
#endif  // CONFIG_VP9_HIGHBITDEPTH

  // Allow joint search multiple times iteratively for each reference frame
  // and break out of the search loop if it couldn't find a better mv.
  for (ite = 0; ite < 4; ite++) {
    struct buf_2d ref_yv12[2];
    int bestsme = INT_MAX;
    int sadpb = x->sadperbit16;
    MV tmp_mv;
    int search_range = 3;

    int tmp_col_min = x->mv_col_min;
    int tmp_col_max = x->mv_col_max;
    int tmp_row_min = x->mv_row_min;
    int tmp_row_max = x->mv_row_max;
    int id = ite % 2;  // Even iterations search in the first reference frame,
                       // odd iterations search in the second. The predictor
                       // found for the 'other' reference frame is factored in.

    // Initialized here because of compiler problem in Visual Studio.
    ref_yv12[0] = xd->plane[0].pre[0];
    ref_yv12[1] = xd->plane[0].pre[1];

    // Get the prediction block from the 'other' reference frame.
#if CONFIG_VP9_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
      vp9_highbd_build_inter_predictor(ref_yv12[!id].buf,
                                       ref_yv12[!id].stride,
                                       second_pred, pw,
                                       &frame_mv[refs[!id]].as_mv,
                                       &sf, pw, ph, 0,
                                       kernel, MV_PRECISION_Q3,
                                       mi_col * MI_SIZE, mi_row * MI_SIZE,
                                       xd->bd);
    } else {
      second_pred = (uint8_t *)second_pred_alloc_16;
      vp9_build_inter_predictor(ref_yv12[!id].buf,
                                ref_yv12[!id].stride,
                                second_pred, pw,
                                &frame_mv[refs[!id]].as_mv,
                                &sf, pw, ph, 0,
                                kernel, MV_PRECISION_Q3,
                                mi_col * MI_SIZE, mi_row * MI_SIZE);
    }
#else
    vp9_build_inter_predictor(ref_yv12[!id].buf,
                              ref_yv12[!id].stride,
                              second_pred, pw,
                              &frame_mv[refs[!id]].as_mv,
                              &sf, pw, ph, 0,
                              kernel, MV_PRECISION_Q3,
                              mi_col * MI_SIZE, mi_row * MI_SIZE);
#endif  // CONFIG_VP9_HIGHBITDEPTH

    // Do compound motion search on the current reference frame.
    if (id)
      xd->plane[0].pre[0] = ref_yv12[id];
    vp9_set_mv_search_range(x, &ref_mv[id].as_mv);

    // Use the mv result from the single mode as mv predictor.
    tmp_mv = frame_mv[refs[id]].as_mv;

    tmp_mv.col >>= 3;
    tmp_mv.row >>= 3;

    // Small-range full-pixel motion search.
    bestsme = vp9_refining_search_8p_c(x, &tmp_mv, sadpb,
                                       search_range,
                                       &cpi->fn_ptr[bsize],
                                       &ref_mv[id].as_mv, second_pred);
    if (bestsme < INT_MAX)
      bestsme = vp9_get_mvpred_av_var(x, &tmp_mv, &ref_mv[id].as_mv,
                                      second_pred, &cpi->fn_ptr[bsize], 1);

    x->mv_col_min = tmp_col_min;
    x->mv_col_max = tmp_col_max;
    x->mv_row_min = tmp_row_min;
    x->mv_row_max = tmp_row_max;

    if (bestsme < INT_MAX) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      bestsme = cpi->find_fractional_mv_step(
          x, &tmp_mv,
          &ref_mv[id].as_mv,
          cpi->common.allow_high_precision_mv,
          x->errorperbit,
          &cpi->fn_ptr[bsize],
          0, cpi->sf.mv.subpel_iters_per_step,
          NULL,
          x->nmvjointcost, x->mvcost,
          &dis, &sse, second_pred,
          pw, ph);
    }

    // Restore the pointer to the first (possibly scaled) prediction buffer.
    if (id)
      xd->plane[0].pre[0] = ref_yv12[0];

    if (bestsme < last_besterr[id]) {
      frame_mv[refs[id]].as_mv = tmp_mv;
      last_besterr[id] = bestsme;
    } else {
      break;
    }
  }

  *rate_mv = 0;

  for (ref = 0; ref < 2; ++ref) {
    if (scaled_ref_frame[ref]) {
      // Restore the prediction frame pointers to their unscaled versions.
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++)
        xd->plane[i].pre[ref] = backup_yv12[ref][i];
    }

    *rate_mv += vp9_mv_bit_cost(&frame_mv[refs[ref]].as_mv,
                                &mbmi->ref_mvs[refs[ref]][0].as_mv,
                                x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
  }
}

static int64_t rd_pick_best_sub8x8_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                        const TileInfo * const tile,
                                        int_mv *best_ref_mv,
                                        int_mv *second_best_ref_mv,
                                        int64_t best_rd, int *returntotrate,
                                        int *returnyrate,
                                        int64_t *returndistortion,
                                        int *skippable, int64_t *psse,
                                        int mvthresh,
                                        int_mv seg_mvs[4][MAX_REF_FRAMES],
                                        BEST_SEG_INFO *bsi_buf, int filter_idx,
                                        int mi_row, int mi_col) {
  int i;
  BEST_SEG_INFO *bsi = bsi_buf + filter_idx;
  MACROBLOCKD *xd = &x->e_mbd;
  MODE_INFO *mi = xd->mi[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  int mode_idx;
  int k, br = 0, idx, idy;
  int64_t bd = 0, block_sse = 0;
  PREDICTION_MODE this_mode;
  VP9_COMMON *cm = &cpi->common;
  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int label_count = 4;
  int64_t this_segment_rd = 0;
  int label_mv_thresh;
  int segmentyrate = 0;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  ENTROPY_CONTEXT t_above[2], t_left[2];
  int subpelmv = 1, have_ref = 0;
  const int has_second_rf = has_second_ref(mbmi);
  const int inter_mode_mask = cpi->sf.inter_mode_mask[bsize];

  vp9_zero(*bsi);

  bsi->segment_rd = best_rd;
  bsi->ref_mv[0] = best_ref_mv;
  bsi->ref_mv[1] = second_best_ref_mv;
  bsi->mvp.as_int = best_ref_mv->as_int;
  bsi->mvthresh = mvthresh;

  for (i = 0; i < 4; i++)
    bsi->modes[i] = ZEROMV;

  memcpy(t_above, pd->above_context, sizeof(t_above));
  memcpy(t_left, pd->left_context, sizeof(t_left));

  // 64 makes this threshold really big effectively
  // making it so that we very rarely check mvs on
  // segments.   setting this to 1 would make mv thresh
  // roughly equal to what it is for macroblocks
  label_mv_thresh = 1 * bsi->mvthresh / label_count;

  // Segmentation method overheads
  for (idy = 0; idy < 2; idy += num_4x4_blocks_high) {
    for (idx = 0; idx < 2; idx += num_4x4_blocks_wide) {
      // TODO(jingning,rbultje): rewrite the rate-distortion optimization
      // loop for 4x4/4x8/8x4 block coding. to be replaced with new rd loop
      int_mv mode_mv[MB_MODE_COUNT][2];
      int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
      PREDICTION_MODE mode_selected = ZEROMV;
      int64_t best_rd = INT64_MAX;
      const int i = idy * 2 + idx;
      int ref;

      for (ref = 0; ref < 1 + has_second_rf; ++ref) {
        const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
        frame_mv[ZEROMV][frame].as_int = 0;
        vp9_append_sub8x8_mvs_for_idx(cm, xd, tile, i, ref, mi_row, mi_col,
                                      &frame_mv[NEARESTMV][frame],
                                      &frame_mv[NEARMV][frame]);
      }

      // search for the best motion vector on this segment
      for (this_mode = NEARESTMV; this_mode <= NEWMV; ++this_mode) {
        const struct buf_2d orig_src = x->plane[0].src;
        struct buf_2d orig_pre[2];

        mode_idx = INTER_OFFSET(this_mode);
        bsi->rdstat[i][mode_idx].brdcost = INT64_MAX;
        if (!(inter_mode_mask & (1 << this_mode)))
          continue;

        if (!check_best_zero_mv(cpi, mbmi->mode_context, frame_mv,
                                this_mode, mbmi->ref_frame))
          continue;

        memcpy(orig_pre, pd->pre, sizeof(orig_pre));
        memcpy(bsi->rdstat[i][mode_idx].ta, t_above,
               sizeof(bsi->rdstat[i][mode_idx].ta));
        memcpy(bsi->rdstat[i][mode_idx].tl, t_left,
               sizeof(bsi->rdstat[i][mode_idx].tl));

        // motion search for newmv (single predictor case only)
        if (!has_second_rf && this_mode == NEWMV &&
            seg_mvs[i][mbmi->ref_frame[0]].as_int == INVALID_MV) {
          MV *const new_mv = &mode_mv[NEWMV][0].as_mv;
          int step_param = 0;
          int thissme, bestsme = INT_MAX;
          int sadpb = x->sadperbit4;
          MV mvp_full;
          int max_mv;
          int cost_list[5];

          /* Is the best so far sufficiently good that we cant justify doing
           * and new motion search. */
          if (best_rd < label_mv_thresh)
            break;

          if (cpi->oxcf.mode != BEST) {
            // use previous block's result as next block's MV predictor.
            if (i > 0) {
              bsi->mvp.as_int = mi->bmi[i - 1].as_mv[0].as_int;
              if (i == 2)
                bsi->mvp.as_int = mi->bmi[i - 2].as_mv[0].as_int;
            }
          }
          if (i == 0)
            max_mv = x->max_mv_context[mbmi->ref_frame[0]];
          else
            max_mv = MAX(abs(bsi->mvp.as_mv.row), abs(bsi->mvp.as_mv.col)) >> 3;

          if (cpi->sf.mv.auto_mv_step_size && cm->show_frame) {
            // Take wtd average of the step_params based on the last frame's
            // max mv magnitude and the best ref mvs of the current block for
            // the given reference.
            step_param = (vp9_init_search_range(max_mv) +
                              cpi->mv_step_param) / 2;
          } else {
            step_param = cpi->mv_step_param;
          }

          mvp_full.row = bsi->mvp.as_mv.row >> 3;
          mvp_full.col = bsi->mvp.as_mv.col >> 3;

          if (cpi->sf.adaptive_motion_search) {
            mvp_full.row = x->pred_mv[mbmi->ref_frame[0]].row >> 3;
            mvp_full.col = x->pred_mv[mbmi->ref_frame[0]].col >> 3;
            step_param = MAX(step_param, 8);
          }

          // adjust src pointer for this block
          mi_buf_shift(x, i);

          vp9_set_mv_search_range(x, &bsi->ref_mv[0]->as_mv);

          bestsme = vp9_full_pixel_search(
              cpi, x, bsize, &mvp_full, step_param, sadpb,
              cpi->sf.mv.subpel_search_method != SUBPEL_TREE ? cost_list : NULL,
              &bsi->ref_mv[0]->as_mv, new_mv,
              INT_MAX, 1);

          // Should we do a full search (best quality only)
          if (cpi->oxcf.mode == BEST) {
            int_mv *const best_mv = &mi->bmi[i].as_mv[0];
            /* Check if mvp_full is within the range. */
            clamp_mv(&mvp_full, x->mv_col_min, x->mv_col_max,
                     x->mv_row_min, x->mv_row_max);
            thissme = cpi->full_search_sad(x, &mvp_full,
                                           sadpb, 16, &cpi->fn_ptr[bsize],
                                           &bsi->ref_mv[0]->as_mv,
                                           &best_mv->as_mv);
            cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;
            if (thissme < bestsme) {
              bestsme = thissme;
              *new_mv = best_mv->as_mv;
            } else {
              // The full search result is actually worse so re-instate the
              // previous best vector
              best_mv->as_mv = *new_mv;
            }
          }

          if (bestsme < INT_MAX) {
            int distortion;
            cpi->find_fractional_mv_step(
                x,
                new_mv,
                &bsi->ref_mv[0]->as_mv,
                cm->allow_high_precision_mv,
                x->errorperbit, &cpi->fn_ptr[bsize],
                cpi->sf.mv.subpel_force_stop,
                cpi->sf.mv.subpel_iters_per_step,
                cond_cost_list(cpi, cost_list),
                x->nmvjointcost, x->mvcost,
                &distortion,
                &x->pred_sse[mbmi->ref_frame[0]],
                NULL, 0, 0);

            // save motion search result for use in compound prediction
            seg_mvs[i][mbmi->ref_frame[0]].as_mv = *new_mv;
          }

          if (cpi->sf.adaptive_motion_search)
            x->pred_mv[mbmi->ref_frame[0]] = *new_mv;

          // restore src pointers
          mi_buf_restore(x, orig_src, orig_pre);
        }

        if (has_second_rf) {
          if (seg_mvs[i][mbmi->ref_frame[1]].as_int == INVALID_MV ||
              seg_mvs[i][mbmi->ref_frame[0]].as_int == INVALID_MV)
            continue;
        }

        if (has_second_rf && this_mode == NEWMV &&
            mbmi->interp_filter == EIGHTTAP) {
          // adjust src pointers
          mi_buf_shift(x, i);
          if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
            int rate_mv;
            joint_motion_search(cpi, x, bsize, frame_mv[this_mode],
                                mi_row, mi_col, seg_mvs[i],
                                &rate_mv);
            seg_mvs[i][mbmi->ref_frame[0]].as_int =
                frame_mv[this_mode][mbmi->ref_frame[0]].as_int;
            seg_mvs[i][mbmi->ref_frame[1]].as_int =
                frame_mv[this_mode][mbmi->ref_frame[1]].as_int;
          }
          // restore src pointers
          mi_buf_restore(x, orig_src, orig_pre);
        }

        bsi->rdstat[i][mode_idx].brate =
            set_and_cost_bmi_mvs(cpi, xd, i, this_mode, mode_mv[this_mode],
                                 frame_mv, seg_mvs[i], bsi->ref_mv,
                                 x->nmvjointcost, x->mvcost);

        for (ref = 0; ref < 1 + has_second_rf; ++ref) {
          bsi->rdstat[i][mode_idx].mvs[ref].as_int =
              mode_mv[this_mode][ref].as_int;
          if (num_4x4_blocks_wide > 1)
            bsi->rdstat[i + 1][mode_idx].mvs[ref].as_int =
                mode_mv[this_mode][ref].as_int;
          if (num_4x4_blocks_high > 1)
            bsi->rdstat[i + 2][mode_idx].mvs[ref].as_int =
                mode_mv[this_mode][ref].as_int;
        }

        // Trap vectors that reach beyond the UMV borders
        if (mv_check_bounds(x, &mode_mv[this_mode][0].as_mv) ||
            (has_second_rf &&
             mv_check_bounds(x, &mode_mv[this_mode][1].as_mv)))
          continue;

        if (filter_idx > 0) {
          BEST_SEG_INFO *ref_bsi = bsi_buf;
          subpelmv = 0;
          have_ref = 1;

          for (ref = 0; ref < 1 + has_second_rf; ++ref) {
            subpelmv |= mv_has_subpel(&mode_mv[this_mode][ref].as_mv);
            have_ref &= mode_mv[this_mode][ref].as_int ==
                ref_bsi->rdstat[i][mode_idx].mvs[ref].as_int;
          }

          if (filter_idx > 1 && !subpelmv && !have_ref) {
            ref_bsi = bsi_buf + 1;
            have_ref = 1;
            for (ref = 0; ref < 1 + has_second_rf; ++ref)
              have_ref &= mode_mv[this_mode][ref].as_int ==
                  ref_bsi->rdstat[i][mode_idx].mvs[ref].as_int;
          }

          if (!subpelmv && have_ref &&
              ref_bsi->rdstat[i][mode_idx].brdcost < INT64_MAX) {
            memcpy(&bsi->rdstat[i][mode_idx], &ref_bsi->rdstat[i][mode_idx],
                   sizeof(SEG_RDSTAT));
            if (num_4x4_blocks_wide > 1)
              bsi->rdstat[i + 1][mode_idx].eobs =
                  ref_bsi->rdstat[i + 1][mode_idx].eobs;
            if (num_4x4_blocks_high > 1)
              bsi->rdstat[i + 2][mode_idx].eobs =
                  ref_bsi->rdstat[i + 2][mode_idx].eobs;

            if (bsi->rdstat[i][mode_idx].brdcost < best_rd) {
              mode_selected = this_mode;
              best_rd = bsi->rdstat[i][mode_idx].brdcost;
            }
            continue;
          }
        }

        bsi->rdstat[i][mode_idx].brdcost =
            encode_inter_mb_segment(cpi, x,
                                    bsi->segment_rd - this_segment_rd, i,
                                    &bsi->rdstat[i][mode_idx].byrate,
                                    &bsi->rdstat[i][mode_idx].bdist,
                                    &bsi->rdstat[i][mode_idx].bsse,
                                    bsi->rdstat[i][mode_idx].ta,
                                    bsi->rdstat[i][mode_idx].tl,
                                    mi_row, mi_col);
        if (bsi->rdstat[i][mode_idx].brdcost < INT64_MAX) {
          bsi->rdstat[i][mode_idx].brdcost += RDCOST(x->rdmult, x->rddiv,
                                            bsi->rdstat[i][mode_idx].brate, 0);
          bsi->rdstat[i][mode_idx].brate += bsi->rdstat[i][mode_idx].byrate;
          bsi->rdstat[i][mode_idx].eobs = p->eobs[i];
          if (num_4x4_blocks_wide > 1)
            bsi->rdstat[i + 1][mode_idx].eobs = p->eobs[i + 1];
          if (num_4x4_blocks_high > 1)
            bsi->rdstat[i + 2][mode_idx].eobs = p->eobs[i + 2];
        }

        if (bsi->rdstat[i][mode_idx].brdcost < best_rd) {
          mode_selected = this_mode;
          best_rd = bsi->rdstat[i][mode_idx].brdcost;
        }
      } /*for each 4x4 mode*/

      if (best_rd == INT64_MAX) {
        int iy, midx;
        for (iy = i + 1; iy < 4; ++iy)
          for (midx = 0; midx < INTER_MODES; ++midx)
            bsi->rdstat[iy][midx].brdcost = INT64_MAX;
        bsi->segment_rd = INT64_MAX;
        return INT64_MAX;
      }

      mode_idx = INTER_OFFSET(mode_selected);
      memcpy(t_above, bsi->rdstat[i][mode_idx].ta, sizeof(t_above));
      memcpy(t_left, bsi->rdstat[i][mode_idx].tl, sizeof(t_left));

      set_and_cost_bmi_mvs(cpi, xd, i, mode_selected, mode_mv[mode_selected],
                           frame_mv, seg_mvs[i], bsi->ref_mv, x->nmvjointcost,
                           x->mvcost);

      br += bsi->rdstat[i][mode_idx].brate;
      bd += bsi->rdstat[i][mode_idx].bdist;
      block_sse += bsi->rdstat[i][mode_idx].bsse;
      segmentyrate += bsi->rdstat[i][mode_idx].byrate;
      this_segment_rd += bsi->rdstat[i][mode_idx].brdcost;

      if (this_segment_rd > bsi->segment_rd) {
        int iy, midx;
        for (iy = i + 1; iy < 4; ++iy)
          for (midx = 0; midx < INTER_MODES; ++midx)
            bsi->rdstat[iy][midx].brdcost = INT64_MAX;
        bsi->segment_rd = INT64_MAX;
        return INT64_MAX;
      }
    }
  } /* for each label */

  bsi->r = br;
  bsi->d = bd;
  bsi->segment_yrate = segmentyrate;
  bsi->segment_rd = this_segment_rd;
  bsi->sse = block_sse;

  // update the coding decisions
  for (k = 0; k < 4; ++k)
    bsi->modes[k] = mi->bmi[k].as_mode;

  if (bsi->segment_rd > best_rd)
    return INT64_MAX;
  /* set it to the best */
  for (i = 0; i < 4; i++) {
    mode_idx = INTER_OFFSET(bsi->modes[i]);
    mi->bmi[i].as_mv[0].as_int = bsi->rdstat[i][mode_idx].mvs[0].as_int;
    if (has_second_ref(mbmi))
      mi->bmi[i].as_mv[1].as_int = bsi->rdstat[i][mode_idx].mvs[1].as_int;
    x->plane[0].eobs[i] = bsi->rdstat[i][mode_idx].eobs;
    mi->bmi[i].as_mode = bsi->modes[i];
  }

  /*
   * used to set mbmi->mv.as_int
   */
  *returntotrate = bsi->r;
  *returndistortion = bsi->d;
  *returnyrate = bsi->segment_yrate;
  *skippable = vp9_is_skippable_in_plane(x, BLOCK_8X8, 0);
  *psse = bsi->sse;
  mbmi->mode = bsi->modes[3];

  return bsi->segment_rd;
}

static void estimate_ref_frame_costs(const VP9_COMMON *cm,
                                     const MACROBLOCKD *xd,
                                     int segment_id,
                                     unsigned int *ref_costs_single,
                                     unsigned int *ref_costs_comp,
                                     vp9_prob *comp_mode_p) {
  int seg_ref_active = vp9_segfeature_active(&cm->seg, segment_id,
                                             SEG_LVL_REF_FRAME);
  if (seg_ref_active) {
    memset(ref_costs_single, 0, MAX_REF_FRAMES * sizeof(*ref_costs_single));
    memset(ref_costs_comp,   0, MAX_REF_FRAMES * sizeof(*ref_costs_comp));
    *comp_mode_p = 128;
  } else {
    vp9_prob intra_inter_p = vp9_get_intra_inter_prob(cm, xd);
    vp9_prob comp_inter_p = 128;

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      comp_inter_p = vp9_get_reference_mode_prob(cm, xd);
      *comp_mode_p = comp_inter_p;
    } else {
      *comp_mode_p = 128;
    }

    ref_costs_single[INTRA_FRAME] = vp9_cost_bit(intra_inter_p, 0);

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      vp9_prob ref_single_p1 = vp9_get_pred_prob_single_ref_p1(cm, xd);
      vp9_prob ref_single_p2 = vp9_get_pred_prob_single_ref_p2(cm, xd);
      unsigned int base_cost = vp9_cost_bit(intra_inter_p, 1);

      if (cm->reference_mode == REFERENCE_MODE_SELECT)
        base_cost += vp9_cost_bit(comp_inter_p, 0);

      ref_costs_single[LAST_FRAME] = ref_costs_single[GOLDEN_FRAME] =
          ref_costs_single[ALTREF_FRAME] = base_cost;
      ref_costs_single[LAST_FRAME]   += vp9_cost_bit(ref_single_p1, 0);
      ref_costs_single[GOLDEN_FRAME] += vp9_cost_bit(ref_single_p1, 1);
      ref_costs_single[ALTREF_FRAME] += vp9_cost_bit(ref_single_p1, 1);
      ref_costs_single[GOLDEN_FRAME] += vp9_cost_bit(ref_single_p2, 0);
      ref_costs_single[ALTREF_FRAME] += vp9_cost_bit(ref_single_p2, 1);
    } else {
      ref_costs_single[LAST_FRAME]   = 512;
      ref_costs_single[GOLDEN_FRAME] = 512;
      ref_costs_single[ALTREF_FRAME] = 512;
    }
    if (cm->reference_mode != SINGLE_REFERENCE) {
      vp9_prob ref_comp_p = vp9_get_pred_prob_comp_ref_p(cm, xd);
      unsigned int base_cost = vp9_cost_bit(intra_inter_p, 1);

      if (cm->reference_mode == REFERENCE_MODE_SELECT)
        base_cost += vp9_cost_bit(comp_inter_p, 1);

      ref_costs_comp[LAST_FRAME]   = base_cost + vp9_cost_bit(ref_comp_p, 0);
      ref_costs_comp[GOLDEN_FRAME] = base_cost + vp9_cost_bit(ref_comp_p, 1);
    } else {
      ref_costs_comp[LAST_FRAME]   = 512;
      ref_costs_comp[GOLDEN_FRAME] = 512;
    }
  }
}

static void store_coding_context(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx,
                         int mode_index,
                         int64_t comp_pred_diff[REFERENCE_MODES],
                         const int64_t tx_size_diff[TX_MODES],
                         int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS],
                         int skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;

  // Take a snapshot of the coding context so it can be
  // restored if we decide to encode this way
  ctx->skip = x->skip;
  ctx->skippable = skippable;
  ctx->best_mode_index = mode_index;
  ctx->mic = *xd->mi[0];
  ctx->single_pred_diff = (int)comp_pred_diff[SINGLE_REFERENCE];
  ctx->comp_pred_diff   = (int)comp_pred_diff[COMPOUND_REFERENCE];
  ctx->hybrid_pred_diff = (int)comp_pred_diff[REFERENCE_MODE_SELECT];

  memcpy(ctx->tx_rd_diff, tx_size_diff, sizeof(ctx->tx_rd_diff));
  memcpy(ctx->best_filter_diff, best_filter_diff,
         sizeof(*best_filter_diff) * SWITCHABLE_FILTER_CONTEXTS);
}

static void setup_buffer_inter(VP9_COMP *cpi, MACROBLOCK *x,
                               const TileInfo *const tile,
                               MV_REFERENCE_FRAME ref_frame,
                               BLOCK_SIZE block_size,
                               int mi_row, int mi_col,
                               int_mv frame_nearest_mv[MAX_REF_FRAMES],
                               int_mv frame_near_mv[MAX_REF_FRAMES],
                               struct buf_2d yv12_mb[4][MAX_MB_PLANE]) {
  const VP9_COMMON *cm = &cpi->common;
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_buffer(cpi, ref_frame);
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO *const mi = xd->mi[0];
  int_mv *const candidates = mi->mbmi.ref_mvs[ref_frame];
  const struct scale_factors *const sf = &cm->frame_refs[ref_frame - 1].sf;

  assert(yv12 != NULL);

  // TODO(jkoleszar): Is the UV buffer ever used here? If so, need to make this
  // use the UV scaling factors.
  vp9_setup_pred_block(xd, yv12_mb[ref_frame], yv12, mi_row, mi_col, sf, sf);

  // Gets an initial list of candidate vectors from neighbours and orders them
  vp9_find_mv_refs(cm, xd, tile, mi, ref_frame, candidates, mi_row, mi_col,
                   NULL, NULL);

  // Candidate refinement carried out at encoder and decoder
  vp9_find_best_ref_mvs(xd, cm->allow_high_precision_mv, candidates,
                        &frame_nearest_mv[ref_frame],
                        &frame_near_mv[ref_frame]);

  // Further refinement that is encode side only to test the top few candidates
  // in full and choose the best as the centre point for subsequent searches.
  // The current implementation doesn't support scaling.
  if (!vp9_is_scaled(sf) && block_size >= BLOCK_8X8)
    vp9_mv_pred(cpi, x, yv12_mb[ref_frame][0].buf, yv12->y_stride,
                ref_frame, block_size);
}

static void single_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize,
                                 int mi_row, int mi_col,
                                 int_mv *tmp_mv, int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  const VP9_COMMON *cm = &cpi->common;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = {{0, 0}};
  int bestsme = INT_MAX;
  int step_param;
  int sadpb = x->sadperbit16;
  MV mvp_full;
  int ref = mbmi->ref_frame[0];
  MV ref_mv = mbmi->ref_mvs[ref][0].as_mv;

  int tmp_col_min = x->mv_col_min;
  int tmp_col_max = x->mv_col_max;
  int tmp_row_min = x->mv_row_min;
  int tmp_row_max = x->mv_row_max;
  int cost_list[5];

  const YV12_BUFFER_CONFIG *scaled_ref_frame = vp9_get_scaled_ref_frame(cpi,
                                                                        ref);

  MV pred_mv[3];
  pred_mv[0] = mbmi->ref_mvs[ref][0].as_mv;
  pred_mv[1] = mbmi->ref_mvs[ref][1].as_mv;
  pred_mv[2] = x->pred_mv[ref];

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[0];

    vp9_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  vp9_set_mv_search_range(x, &ref_mv);

  // Work out the size of the first step in the mv step search.
  // 0 here is maximum length first step. 1 is MAX >> 1 etc.
  if (cpi->sf.mv.auto_mv_step_size && cm->show_frame) {
    // Take wtd average of the step_params based on the last frame's
    // max mv magnitude and that based on the best ref mvs of the current
    // block for the given reference.
    step_param = (vp9_init_search_range(x->max_mv_context[ref]) +
                    cpi->mv_step_param) / 2;
  } else {
    step_param = cpi->mv_step_param;
  }

  if (cpi->sf.adaptive_motion_search && bsize < BLOCK_64X64) {
    int boffset = 2 * (b_width_log2_lookup[BLOCK_64X64] -
          MIN(b_height_log2_lookup[bsize], b_width_log2_lookup[bsize]));
    step_param = MAX(step_param, boffset);
  }

  if (cpi->sf.adaptive_motion_search) {
    int bwl = b_width_log2_lookup[bsize];
    int bhl = b_height_log2_lookup[bsize];
    int tlevel = x->pred_mv_sad[ref] >> (bwl + bhl + 4);

    if (tlevel < 5)
      step_param += 2;

    // prev_mv_sad is not setup for dynamically scaled frames.
    if (cpi->oxcf.resize_mode != RESIZE_DYNAMIC) {
      int i;
      for (i = LAST_FRAME; i <= ALTREF_FRAME && cm->show_frame; ++i) {
        if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
          x->pred_mv[ref].row = 0;
          x->pred_mv[ref].col = 0;
          tmp_mv->as_int = INVALID_MV;

          if (scaled_ref_frame) {
            int i;
            for (i = 0; i < MAX_MB_PLANE; ++i)
              xd->plane[i].pre[0] = backup_yv12[i];
          }
          return;
        }
      }
    }
  }

  mvp_full = pred_mv[x->mv_best_ref_index[ref]];

  mvp_full.col >>= 3;
  mvp_full.row >>= 3;

  bestsme = vp9_full_pixel_search(cpi, x, bsize, &mvp_full, step_param, sadpb,
                                  cond_cost_list(cpi, cost_list),
                                  &ref_mv, &tmp_mv->as_mv, INT_MAX, 1);

  x->mv_col_min = tmp_col_min;
  x->mv_col_max = tmp_col_max;
  x->mv_row_min = tmp_row_min;
  x->mv_row_max = tmp_row_max;

  if (bestsme < INT_MAX) {
    int dis;  /* TODO: use dis in distortion calculation later. */
    cpi->find_fractional_mv_step(x, &tmp_mv->as_mv, &ref_mv,
                                 cm->allow_high_precision_mv,
                                 x->errorperbit,
                                 &cpi->fn_ptr[bsize],
                                 cpi->sf.mv.subpel_force_stop,
                                 cpi->sf.mv.subpel_iters_per_step,
                                 cond_cost_list(cpi, cost_list),
                                 x->nmvjointcost, x->mvcost,
                                 &dis, &x->pred_sse[ref], NULL, 0, 0);
  }
  *rate_mv = vp9_mv_bit_cost(&tmp_mv->as_mv, &ref_mv,
                             x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);

  if (cpi->sf.adaptive_motion_search)
    x->pred_mv[ref] = tmp_mv->as_mv;

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[0] = backup_yv12[i];
  }
}



static INLINE void restore_dst_buf(MACROBLOCKD *xd,
                                   uint8_t *orig_dst[MAX_MB_PLANE],
                                   int orig_dst_stride[MAX_MB_PLANE]) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].dst.buf = orig_dst[i];
    xd->plane[i].dst.stride = orig_dst_stride[i];
  }
}

// In some situations we want to discount tha pparent cost of a new motion
// vector. Where there is a subtle motion field and especially where there is
// low spatial complexity then it can be hard to cover the cost of a new motion
// vector in a single block, even if that motion vector reduces distortion.
// However, once established that vector may be usable through the nearest and
// near mv modes to reduce distortion in subsequent blocks and also improve
// visual quality.
static int discount_newmv_test(const VP9_COMP *cpi,
                               int this_mode,
                               int_mv this_mv,
                               int_mv (*mode_mv)[MAX_REF_FRAMES],
                               int ref_frame) {
  return (!cpi->rc.is_src_frame_alt_ref &&
          (this_mode == NEWMV) &&
          (this_mv.as_int != 0) &&
          ((mode_mv[NEARESTMV][ref_frame].as_int == 0) ||
           (mode_mv[NEARESTMV][ref_frame].as_int == INVALID_MV)) &&
          ((mode_mv[NEARMV][ref_frame].as_int == 0) ||
           (mode_mv[NEARMV][ref_frame].as_int == INVALID_MV)));
}

static int64_t handle_inter_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                 BLOCK_SIZE bsize,
                                 int64_t txfm_cache[],
                                 int *rate2, int64_t *distortion,
                                 int *skippable,
                                 int *rate_y, int *rate_uv,
                                 int *disable_skip,
                                 int_mv (*mode_mv)[MAX_REF_FRAMES],
                                 int mi_row, int mi_col,
                                 int_mv single_newmv[MAX_REF_FRAMES],
                                 INTERP_FILTER (*single_filter)[MAX_REF_FRAMES],
                                 int (*single_skippable)[MAX_REF_FRAMES],
                                 int64_t *psse,
                                 const int64_t ref_best_rd,
                                 int64_t *mask_filter,
                                 int64_t filter_cache[]) {
  VP9_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const int is_comp_pred = has_second_ref(mbmi);
  const int this_mode = mbmi->mode;
  int_mv *frame_mv = mode_mv[this_mode];
  int i;
  int refs[2] = { mbmi->ref_frame[0],
    (mbmi->ref_frame[1] < 0 ? 0 : mbmi->ref_frame[1]) };
  int_mv cur_mv[2];
#if CONFIG_VP9_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, tmp_buf16[MAX_MB_PLANE * 64 * 64]);
  uint8_t *tmp_buf;
#else
  DECLARE_ALIGNED(16, uint8_t, tmp_buf[MAX_MB_PLANE * 64 * 64]);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  int pred_exists = 0;
  int intpel_mv;
  int64_t rd, tmp_rd, best_rd = INT64_MAX;
  int best_needs_copy = 0;
  uint8_t *orig_dst[MAX_MB_PLANE];
  int orig_dst_stride[MAX_MB_PLANE];
  int rs = 0;
  INTERP_FILTER best_filter = SWITCHABLE;
  uint8_t skip_txfm[MAX_MB_PLANE << 2] = {0};
  int64_t bsse[MAX_MB_PLANE << 2] = {0};

  int bsl = mi_width_log2_lookup[bsize];
  int pred_filter_search = cpi->sf.cb_pred_filter_search ?
      (((mi_row + mi_col) >> bsl) +
       get_chessboard_index(cm->current_video_frame)) & 0x1 : 0;

  int skip_txfm_sb = 0;
  int64_t skip_sse_sb = INT64_MAX;
  int64_t distortion_y = 0, distortion_uv = 0;

#if CONFIG_VP9_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    tmp_buf = CONVERT_TO_BYTEPTR(tmp_buf16);
  } else {
    tmp_buf = (uint8_t *)tmp_buf16;
  }
#endif  // CONFIG_VP9_HIGHBITDEPTH

  if (pred_filter_search) {
    INTERP_FILTER af = SWITCHABLE, lf = SWITCHABLE;
    if (xd->up_available)
      af = xd->mi[-xd->mi_stride]->mbmi.interp_filter;
    if (xd->left_available)
      lf = xd->mi[-1]->mbmi.interp_filter;

    if ((this_mode != NEWMV) || (af == lf))
      best_filter = af;
  }

  if (is_comp_pred) {
    if (frame_mv[refs[0]].as_int == INVALID_MV ||
        frame_mv[refs[1]].as_int == INVALID_MV)
      return INT64_MAX;

    if (cpi->sf.adaptive_mode_search) {
      if (single_filter[this_mode][refs[0]] ==
          single_filter[this_mode][refs[1]])
        best_filter = single_filter[this_mode][refs[0]];
    }
  }

  if (this_mode == NEWMV) {
    int rate_mv;
    if (is_comp_pred) {
      // Initialize mv using single prediction mode result.
      frame_mv[refs[0]].as_int = single_newmv[refs[0]].as_int;
      frame_mv[refs[1]].as_int = single_newmv[refs[1]].as_int;

      if (cpi->sf.comp_inter_joint_search_thresh <= bsize) {
        joint_motion_search(cpi, x, bsize, frame_mv,
                            mi_row, mi_col, single_newmv, &rate_mv);
      } else {
        rate_mv  = vp9_mv_bit_cost(&frame_mv[refs[0]].as_mv,
                                   &mbmi->ref_mvs[refs[0]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
        rate_mv += vp9_mv_bit_cost(&frame_mv[refs[1]].as_mv,
                                   &mbmi->ref_mvs[refs[1]][0].as_mv,
                                   x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
      }
      *rate2 += rate_mv;
    } else {
      int_mv tmp_mv;
      single_motion_search(cpi, x, bsize, mi_row, mi_col,
                           &tmp_mv, &rate_mv);
      if (tmp_mv.as_int == INVALID_MV)
        return INT64_MAX;

      frame_mv[refs[0]].as_int =
          xd->mi[0]->bmi[0].as_mv[0].as_int = tmp_mv.as_int;
      single_newmv[refs[0]].as_int = tmp_mv.as_int;

      // Estimate the rate implications of a new mv but discount this
      // under certain circumstances where we want to help initiate a weak
      // motion field, where the distortion gain for a single block may not
      // be enough to overcome the cost of a new mv.
      if (discount_newmv_test(cpi, this_mode, tmp_mv, mode_mv, refs[0])) {
        *rate2 += MAX((rate_mv / NEW_MV_DISCOUNT_FACTOR), 1);
      } else {
        *rate2 += rate_mv;
      }
    }
  }

  for (i = 0; i < is_comp_pred + 1; ++i) {
    cur_mv[i] = frame_mv[refs[i]];
    // Clip "next_nearest" so that it does not extend to far out of image
    if (this_mode != NEWMV)
      clamp_mv2(&cur_mv[i].as_mv, xd);

    if (mv_check_bounds(x, &cur_mv[i].as_mv))
      return INT64_MAX;
    mbmi->mv[i].as_int = cur_mv[i].as_int;
  }

  // do first prediction into the destination buffer. Do the next
  // prediction into a temporary buffer. Then keep track of which one
  // of these currently holds the best predictor, and use the other
  // one for future predictions. In the end, copy from tmp_buf to
  // dst if necessary.
  for (i = 0; i < MAX_MB_PLANE; i++) {
    orig_dst[i] = xd->plane[i].dst.buf;
    orig_dst_stride[i] = xd->plane[i].dst.stride;
  }

  // We don't include the cost of the second reference here, because there
  // are only three options: Last/Golden, ARF/Last or Golden/ARF, or in other
  // words if you present them in that order, the second one is always known
  // if the first is known.
  //
  // Under some circumstances we discount the cost of new mv mode to encourage
  // initiation of a motion field.
  if (discount_newmv_test(cpi, this_mode, frame_mv[refs[0]],
                          mode_mv, refs[0])) {
    *rate2 += MIN(cost_mv_ref(cpi, this_mode, mbmi->mode_context[refs[0]]),
                  cost_mv_ref(cpi, NEARESTMV, mbmi->mode_context[refs[0]]));
  } else {
    *rate2 += cost_mv_ref(cpi, this_mode, mbmi->mode_context[refs[0]]);
  }

  if (RDCOST(x->rdmult, x->rddiv, *rate2, 0) > ref_best_rd &&
      mbmi->mode != NEARESTMV)
    return INT64_MAX;

  pred_exists = 0;
  // Are all MVs integer pel for Y and UV
  intpel_mv = !mv_has_subpel(&mbmi->mv[0].as_mv);
  if (is_comp_pred)
    intpel_mv &= !mv_has_subpel(&mbmi->mv[1].as_mv);

  // Search for best switchable filter by checking the variance of
  // pred error irrespective of whether the filter will be used
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
    filter_cache[i] = INT64_MAX;

  if (cm->interp_filter != BILINEAR) {
    if (x->source_variance < cpi->sf.disable_filter_search_var_thresh) {
      best_filter = EIGHTTAP;
    } else if (best_filter == SWITCHABLE) {
      int newbest;
      int tmp_rate_sum = 0;
      int64_t tmp_dist_sum = 0;

      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        int j;
        int64_t rs_rd;
        int tmp_skip_sb = 0;
        int64_t tmp_skip_sse = INT64_MAX;

        mbmi->interp_filter = i;
        rs = vp9_get_switchable_rate(cpi, xd);
        rs_rd = RDCOST(x->rdmult, x->rddiv, rs, 0);

        if (i > 0 && intpel_mv) {
          rd = RDCOST(x->rdmult, x->rddiv, tmp_rate_sum, tmp_dist_sum);
          filter_cache[i] = rd;
          filter_cache[SWITCHABLE_FILTERS] =
              MIN(filter_cache[SWITCHABLE_FILTERS], rd + rs_rd);
          if (cm->interp_filter == SWITCHABLE)
            rd += rs_rd;
          *mask_filter = MAX(*mask_filter, rd);
        } else {
          int rate_sum = 0;
          int64_t dist_sum = 0;
          if (i > 0 && cpi->sf.adaptive_interp_filter_search &&
              (cpi->sf.interp_filter_search_mask & (1 << i))) {
            rate_sum = INT_MAX;
            dist_sum = INT64_MAX;
            continue;
          }

          if ((cm->interp_filter == SWITCHABLE &&
               (!i || best_needs_copy)) ||
              (cm->interp_filter != SWITCHABLE &&
               (cm->interp_filter == mbmi->interp_filter ||
                (i == 0 && intpel_mv)))) {
            restore_dst_buf(xd, orig_dst, orig_dst_stride);
          } else {
            for (j = 0; j < MAX_MB_PLANE; j++) {
              xd->plane[j].dst.buf = tmp_buf + j * 64 * 64;
              xd->plane[j].dst.stride = 64;
            }
          }
          vp9_build_inter_predictors_sb(xd, mi_row, mi_col, bsize);
          model_rd_for_sb(cpi, bsize, x, xd, &rate_sum, &dist_sum,
                          &tmp_skip_sb, &tmp_skip_sse);

          rd = RDCOST(x->rdmult, x->rddiv, rate_sum, dist_sum);
          filter_cache[i] = rd;
          filter_cache[SWITCHABLE_FILTERS] =
              MIN(filter_cache[SWITCHABLE_FILTERS], rd + rs_rd);
          if (cm->interp_filter == SWITCHABLE)
            rd += rs_rd;
          *mask_filter = MAX(*mask_filter, rd);

          if (i == 0 && intpel_mv) {
            tmp_rate_sum = rate_sum;
            tmp_dist_sum = dist_sum;
          }
        }

        if (i == 0 && cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
          if (rd / 2 > ref_best_rd) {
            restore_dst_buf(xd, orig_dst, orig_dst_stride);
            return INT64_MAX;
          }
        }
        newbest = i == 0 || rd < best_rd;

        if (newbest) {
          best_rd = rd;
          best_filter = mbmi->interp_filter;
          if (cm->interp_filter == SWITCHABLE && i && !intpel_mv)
            best_needs_copy = !best_needs_copy;
        }

        if ((cm->interp_filter == SWITCHABLE && newbest) ||
            (cm->interp_filter != SWITCHABLE &&
             cm->interp_filter == mbmi->interp_filter)) {
          pred_exists = 1;
          tmp_rd = best_rd;

          skip_txfm_sb = tmp_skip_sb;
          skip_sse_sb = tmp_skip_sse;
          memcpy(skip_txfm, x->skip_txfm, sizeof(skip_txfm));
          memcpy(bsse, x->bsse, sizeof(bsse));
        }
      }
      restore_dst_buf(xd, orig_dst, orig_dst_stride);
    }
  }
  // Set the appropriate filter
  mbmi->interp_filter = cm->interp_filter != SWITCHABLE ?
      cm->interp_filter : best_filter;
  rs = cm->interp_filter == SWITCHABLE ? vp9_get_switchable_rate(cpi, xd) : 0;

  if (pred_exists) {
    if (best_needs_copy) {
      // again temporarily set the buffers to local memory to prevent a memcpy
      for (i = 0; i < MAX_MB_PLANE; i++) {
        xd->plane[i].dst.buf = tmp_buf + i * 64 * 64;
        xd->plane[i].dst.stride = 64;
      }
    }
    rd = tmp_rd + RDCOST(x->rdmult, x->rddiv, rs, 0);
  } else {
    int tmp_rate;
    int64_t tmp_dist;
    // Handles the special case when a filter that is not in the
    // switchable list (ex. bilinear) is indicated at the frame level, or
    // skip condition holds.
    vp9_build_inter_predictors_sb(xd, mi_row, mi_col, bsize);
    model_rd_for_sb(cpi, bsize, x, xd, &tmp_rate, &tmp_dist,
                    &skip_txfm_sb, &skip_sse_sb);
    rd = RDCOST(x->rdmult, x->rddiv, rs + tmp_rate, tmp_dist);
    memcpy(skip_txfm, x->skip_txfm, sizeof(skip_txfm));
    memcpy(bsse, x->bsse, sizeof(bsse));
  }

  if (!is_comp_pred)
    single_filter[this_mode][refs[0]] = mbmi->interp_filter;

  if (cpi->sf.adaptive_mode_search)
    if (is_comp_pred)
      if (single_skippable[this_mode][refs[0]] &&
          single_skippable[this_mode][refs[1]])
        memset(skip_txfm, 1, sizeof(skip_txfm));

  if (cpi->sf.use_rd_breakout && ref_best_rd < INT64_MAX) {
    // if current pred_error modeled rd is substantially more than the best
    // so far, do not bother doing full rd
    if (rd / 2 > ref_best_rd) {
      restore_dst_buf(xd, orig_dst, orig_dst_stride);
      return INT64_MAX;
    }
  }

  if (cm->interp_filter == SWITCHABLE)
    *rate2 += rs;

  memcpy(x->skip_txfm, skip_txfm, sizeof(skip_txfm));
  memcpy(x->bsse, bsse, sizeof(bsse));

  if (!skip_txfm_sb) {
    int skippable_y, skippable_uv;
    int64_t sseuv = INT64_MAX;
    int64_t rdcosty = INT64_MAX;

    // Y cost and distortion
    vp9_subtract_plane(x, bsize, 0);
    super_block_yrd(cpi, x, rate_y, &distortion_y, &skippable_y, psse,
                    bsize, txfm_cache, ref_best_rd);

    if (*rate_y == INT_MAX) {
      *rate2 = INT_MAX;
      *distortion = INT64_MAX;
      restore_dst_buf(xd, orig_dst, orig_dst_stride);
      return INT64_MAX;
    }

    *rate2 += *rate_y;
    *distortion += distortion_y;

    rdcosty = RDCOST(x->rdmult, x->rddiv, *rate2, *distortion);
    rdcosty = MIN(rdcosty, RDCOST(x->rdmult, x->rddiv, 0, *psse));

    if (!super_block_uvrd(cpi, x, rate_uv, &distortion_uv, &skippable_uv,
                          &sseuv, bsize, ref_best_rd - rdcosty)) {
      *rate2 = INT_MAX;
      *distortion = INT64_MAX;
      restore_dst_buf(xd, orig_dst, orig_dst_stride);
      return INT64_MAX;
    }

    *psse += sseuv;
    *rate2 += *rate_uv;
    *distortion += distortion_uv;
    *skippable = skippable_y && skippable_uv;
  } else {
    x->skip = 1;
    *disable_skip = 1;

    // The cost of skip bit needs to be added.
    *rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 1);

    *distortion = skip_sse_sb;
  }

  if (!is_comp_pred)
    single_skippable[this_mode][refs[0]] = *skippable;

  restore_dst_buf(xd, orig_dst, orig_dst_stride);
  return 0;  // The rate-distortion cost will be re-calculated by caller.
}

void vp9_rd_pick_intra_mode_sb(VP9_COMP *cpi, MACROBLOCK *x,
                               RD_COST *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblockd_plane *const pd = xd->plane;
  int rate_y = 0, rate_uv = 0, rate_y_tokenonly = 0, rate_uv_tokenonly = 0;
  int y_skip = 0, uv_skip = 0;
  int64_t dist_y = 0, dist_uv = 0, tx_cache[TX_MODES] = { 0 };
  TX_SIZE max_uv_tx_size;
  x->skip_encode = 0;
  ctx->skip = 0;
  xd->mi[0]->mbmi.ref_frame[0] = INTRA_FRAME;
  xd->mi[0]->mbmi.ref_frame[1] = NONE;

  if (bsize >= BLOCK_8X8) {
    if (rd_pick_intra_sby_mode(cpi, x, &rate_y, &rate_y_tokenonly,
                               &dist_y, &y_skip, bsize, tx_cache,
                               best_rd) >= best_rd) {
      rd_cost->rate = INT_MAX;
      return;
    }
  } else {
    y_skip = 0;
    if (rd_pick_intra_sub_8x8_y_mode(cpi, x, &rate_y, &rate_y_tokenonly,
                                     &dist_y, best_rd) >= best_rd) {
      rd_cost->rate = INT_MAX;
      return;
    }
  }
  max_uv_tx_size = get_uv_tx_size_impl(xd->mi[0]->mbmi.tx_size, bsize,
                                       pd[1].subsampling_x,
                                       pd[1].subsampling_y);
  rd_pick_intra_sbuv_mode(cpi, x, ctx, &rate_uv, &rate_uv_tokenonly,
                          &dist_uv, &uv_skip, MAX(BLOCK_8X8, bsize),
                          max_uv_tx_size);

  if (y_skip && uv_skip) {
    rd_cost->rate = rate_y + rate_uv - rate_y_tokenonly - rate_uv_tokenonly +
                    vp9_cost_bit(vp9_get_skip_prob(cm, xd), 1);
    rd_cost->dist = dist_y + dist_uv;
    vp9_zero(ctx->tx_rd_diff);
  } else {
    int i;
    rd_cost->rate = rate_y + rate_uv +
                      vp9_cost_bit(vp9_get_skip_prob(cm, xd), 0);
    rd_cost->dist = dist_y + dist_uv;
    if (cpi->sf.tx_size_search_method == USE_FULL_RD)
      for (i = 0; i < TX_MODES; i++) {
        if (tx_cache[i] < INT64_MAX && tx_cache[cm->tx_mode] < INT64_MAX)
          ctx->tx_rd_diff[i] = tx_cache[i] - tx_cache[cm->tx_mode];
        else
          ctx->tx_rd_diff[i] = 0;
      }
  }

  ctx->mic = *xd->mi[0];
  rd_cost->rdcost = RDCOST(x->rdmult, x->rddiv, rd_cost->rate, rd_cost->dist);
}

// This function is designed to apply a bias or adjustment to an rd value based
// on the relative variance of the source and reconstruction.
#define LOW_VAR_THRESH 16
#define VLOW_ADJ_MAX 25
#define VHIGH_ADJ_MAX 8
static void rd_variance_adjustment(VP9_COMP *cpi,
                                   MACROBLOCK *x,
                                   BLOCK_SIZE bsize,
                                   int64_t *this_rd,
                                   MV_REFERENCE_FRAME ref_frame,
                                   unsigned int source_variance) {
  MACROBLOCKD *const xd = &x->e_mbd;
  unsigned int recon_variance;
  unsigned int absvar_diff = 0;
  int64_t var_error = 0;
  int64_t var_factor = 0;

  if (*this_rd == INT64_MAX)
    return;

#if CONFIG_VP9_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    recon_variance =
      vp9_high_get_sby_perpixel_variance(cpi, &xd->plane[0].dst, bsize, xd->bd);
  } else {
    recon_variance =
      vp9_get_sby_perpixel_variance(cpi, &xd->plane[0].dst, bsize);
  }
#else
  recon_variance =
    vp9_get_sby_perpixel_variance(cpi, &xd->plane[0].dst, bsize);
#endif  // CONFIG_VP9_HIGHBITDEPTH

  if ((source_variance + recon_variance) > LOW_VAR_THRESH) {
    absvar_diff = (source_variance > recon_variance)
      ? (source_variance - recon_variance)
      : (recon_variance - source_variance);

    var_error = (200 * source_variance * recon_variance) /
      ((source_variance * source_variance) +
       (recon_variance * recon_variance));
    var_error = 100 - var_error;
  }

  // Source variance above a threshold and ref frame is intra.
  // This case is targeted mainly at discouraging intra modes that give rise
  // to a predictor with a low spatial complexity compared to the source.
  if ((source_variance > LOW_VAR_THRESH) && (ref_frame == INTRA_FRAME) &&
      (source_variance > recon_variance)) {
    var_factor = MIN(absvar_diff, MIN(VLOW_ADJ_MAX, var_error));
  // A second possible case of interest is where the source variance
  // is very low and we wish to discourage false texture or motion trails.
  } else if ((source_variance < (LOW_VAR_THRESH >> 1)) &&
             (recon_variance > source_variance)) {
    var_factor = MIN(absvar_diff, MIN(VHIGH_ADJ_MAX, var_error));
  }
  *this_rd += (*this_rd * var_factor) / 100;
}

void vp9_rd_pick_inter_mode_sb(VP9_COMP *cpi,
                               TileDataEnc *tile_data,
                               MACROBLOCK *x,
                               int mi_row, int mi_col,
                               RD_COST *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd_so_far) {
  VP9_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  RD_OPT *const rd_opt = &cpi->rd;
  SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct segmentation *const seg = &cm->seg;
  PREDICTION_MODE this_mode;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i, k;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[4][MAX_MB_PLANE];
  int_mv single_newmv[MAX_REF_FRAMES] = { { 0 } };
  INTERP_FILTER single_inter_filter[MB_MODE_COUNT][MAX_REF_FRAMES];
  int single_skippable[MB_MODE_COUNT][MAX_REF_FRAMES];
  static const int flag_list[4] = { 0, VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                    VP9_ALT_FLAG };
  int64_t best_rd = best_rd_so_far;
  int64_t best_tx_rd[TX_MODES];
  int64_t best_tx_diff[TX_MODES];
  int64_t best_pred_diff[REFERENCE_MODES];
  int64_t best_pred_rd[REFERENCE_MODES];
  int64_t best_filter_rd[SWITCHABLE_FILTER_CONTEXTS];
  int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  MB_MODE_INFO best_mbmode;
  int best_mode_skippable = 0;
  int midx, best_mode_index = -1;
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  vp9_prob comp_mode_p;
  int64_t best_intra_rd = INT64_MAX;
  unsigned int best_pred_sse = UINT_MAX;
  PREDICTION_MODE best_intra_mode = DC_PRED;
  int rate_uv_intra[TX_SIZES], rate_uv_tokenonly[TX_SIZES];
  int64_t dist_uv[TX_SIZES];
  int skip_uv[TX_SIZES];
  PREDICTION_MODE mode_uv[TX_SIZES];
  const int intra_cost_penalty = vp9_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->bit_depth);
  int best_skip2 = 0;
  uint8_t ref_frame_skip_mask[2] = { 0 };
  uint16_t mode_skip_mask[MAX_REF_FRAMES] = { 0 };
  int mode_skip_start = sf->mode_skip_start + 1;
  const int *const rd_threshes = rd_opt->threshes[segment_id][bsize];
  const int *const rd_thresh_freq_fact = tile_data->thresh_freq_fact[bsize];
  int64_t mode_threshold[MAX_MODES];
  int *mode_map = tile_data->mode_map[bsize];
  const int mode_search_skip_flags = sf->mode_search_skip_flags;
  int64_t mask_filter = 0;
  int64_t filter_cache[SWITCHABLE_FILTER_CONTEXTS];

  vp9_zero(best_mbmode);

  x->skip_encode = sf->skip_encode_frame && x->q_index < QIDX_SKIP_THRESH;

  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
    filter_cache[i] = INT64_MAX;

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < REFERENCE_MODES; ++i)
    best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < TX_MODES; i++)
    best_tx_rd[i] = INT64_MAX;
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
    best_filter_rd[i] = INT64_MAX;
  for (i = 0; i < TX_SIZES; i++)
    rate_uv_intra[i] = INT_MAX;
  for (i = 0; i < MAX_REF_FRAMES; ++i)
    x->pred_sse[i] = INT_MAX;
  for (i = 0; i < MB_MODE_COUNT; ++i) {
    for (k = 0; k < MAX_REF_FRAMES; ++k) {
      single_inter_filter[i][k] = SWITCHABLE;
      single_skippable[i][k] = 0;
    }
  }

  rd_cost->rate = INT_MAX;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      assert(get_ref_frame_buffer(cpi, ref_frame) != NULL);
      setup_buffer_inter(cpi, x, tile_info, ref_frame, bsize, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV], yv12_mb);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (!(cpi->ref_frame_flags & flag_list[ref_frame])) {
      // Skip checking missing references in both single and compound reference
      // modes. Note that a mode will be skipped iff both reference frames
      // are masked out.
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
    } else if (sf->reference_masking) {
      for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
        // Skip fixed mv modes for poor references
        if ((x->pred_mv_sad[ref_frame] >> 2) > x->pred_mv_sad[i]) {
          mode_skip_mask[ref_frame] |= INTER_NEAREST_NEAR_ZERO;
          break;
        }
      }
    }
    // If the segment reference frame feature is enabled....
    // then do nothing if the current ref frame is not allowed..
    if (vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        vp9_get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) != (int)ref_frame) {
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
    }
  }

  // Disable this drop out case if the ref frame
  // segment level feature is enabled for this segment. This is to
  // prevent the possibility that we end up unable to pick any mode.
  if (!vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
    // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
    // unless ARNR filtering is enabled in which case we want
    // an unfiltered alternative. We allow near/nearest as well
    // because they may result in zero-zero MVs but be cheaper.
    if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0)) {
      ref_frame_skip_mask[0] = (1 << LAST_FRAME) | (1 << GOLDEN_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
      mode_skip_mask[ALTREF_FRAME] = ~INTER_NEAREST_NEAR_ZERO;
      if (frame_mv[NEARMV][ALTREF_FRAME].as_int != 0)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARMV);
      if (frame_mv[NEARESTMV][ALTREF_FRAME].as_int != 0)
        mode_skip_mask[ALTREF_FRAME] |= (1 << NEARESTMV);
    }
  }

  if (cpi->rc.is_src_frame_alt_ref) {
    if (sf->alt_ref_search_fp) {
      mode_skip_mask[ALTREF_FRAME] = 0;
      ref_frame_skip_mask[0] = ~(1 << ALTREF_FRAME);
      ref_frame_skip_mask[1] = SECOND_REF_FRAME_MASK;
    }
  }

  if (sf->alt_ref_search_fp)
    if (!cm->show_frame && x->pred_mv_sad[GOLDEN_FRAME] < INT_MAX)
      if (x->pred_mv_sad[ALTREF_FRAME] > (x->pred_mv_sad[GOLDEN_FRAME] << 1))
        mode_skip_mask[ALTREF_FRAME] |= INTER_ALL;

  if (sf->adaptive_mode_search) {
    if (cm->show_frame && !cpi->rc.is_src_frame_alt_ref &&
        cpi->rc.frames_since_golden >= 3)
      if (x->pred_mv_sad[GOLDEN_FRAME] > (x->pred_mv_sad[LAST_FRAME] << 1))
        mode_skip_mask[GOLDEN_FRAME] |= INTER_ALL;
  }

  if (bsize > sf->max_intra_bsize) {
    ref_frame_skip_mask[0] |= (1 << INTRA_FRAME);
    ref_frame_skip_mask[1] |= (1 << INTRA_FRAME);
  }

  mode_skip_mask[INTRA_FRAME] |=
      ~(sf->intra_y_mode_mask[max_txsize_lookup[bsize]]);

  for (i = 0; i <= LAST_NEW_MV_INDEX; ++i)
    mode_threshold[i] = 0;
  for (i = LAST_NEW_MV_INDEX + 1; i < MAX_MODES; ++i)
    mode_threshold[i] = ((int64_t)rd_threshes[i] * rd_thresh_freq_fact[i]) >> 5;

  midx =  sf->schedule_mode_search ? mode_skip_start : 0;
  while (midx > 4) {
    uint8_t end_pos = 0;
    for (i = 5; i < midx; ++i) {
      if (mode_threshold[mode_map[i - 1]] > mode_threshold[mode_map[i]]) {
        uint8_t tmp = mode_map[i];
        mode_map[i] = mode_map[i - 1];
        mode_map[i - 1] = tmp;
        end_pos = i;
      }
    }
    midx = end_pos;
  }

  for (midx = 0; midx < MAX_MODES; ++midx) {
    int mode_index = mode_map[midx];
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
    int skippable = 0;
    int64_t tx_cache[TX_MODES];
    int this_skip2 = 0;
    int64_t total_sse = INT64_MAX;
    int early_term = 0;

    this_mode = vp9_mode_order[mode_index].mode;
    ref_frame = vp9_mode_order[mode_index].ref_frame[0];
    second_ref_frame = vp9_mode_order[mode_index].ref_frame[1];

    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (midx == mode_skip_start && best_mode_index >= 0) {
      switch (best_mbmode.ref_frame[0]) {
        case INTRA_FRAME:
          break;
        case LAST_FRAME:
          ref_frame_skip_mask[0] |= LAST_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case GOLDEN_FRAME:
          ref_frame_skip_mask[0] |= GOLDEN_FRAME_MODE_MASK;
          ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
          break;
        case ALTREF_FRAME:
          ref_frame_skip_mask[0] |= ALT_REF_MODE_MASK;
          break;
        case NONE:
        case MAX_REF_FRAMES:
          assert(0 && "Invalid Reference frame");
          break;
      }
    }

    if ((ref_frame_skip_mask[0] & (1 << ref_frame)) &&
        (ref_frame_skip_mask[1] & (1 << MAX(0, second_ref_frame))))
      continue;

    if (mode_skip_mask[ref_frame] & (1 << this_mode))
      continue;

    // Test best rd so far against threshold for trying this mode.
    if (best_mode_skippable && sf->schedule_mode_search)
      mode_threshold[mode_index] <<= 1;

    if (best_rd < mode_threshold[mode_index])
      continue;

    if (sf->motion_field_mode_search) {
      const int mi_width  = MIN(num_8x8_blocks_wide_lookup[bsize],
                                tile_info->mi_col_end - mi_col);
      const int mi_height = MIN(num_8x8_blocks_high_lookup[bsize],
                                tile_info->mi_row_end - mi_row);
      const int bsl = mi_width_log2_lookup[bsize];
      int cb_partition_search_ctrl = (((mi_row + mi_col) >> bsl)
          + get_chessboard_index(cm->current_video_frame)) & 0x1;
      MB_MODE_INFO *ref_mbmi;
      int const_motion = 1;
      int skip_ref_frame = !cb_partition_search_ctrl;
      MV_REFERENCE_FRAME rf = NONE;
      int_mv ref_mv;
      ref_mv.as_int = INVALID_MV;

      if ((mi_row - 1) >= tile_info->mi_row_start) {
        ref_mv = xd->mi[-xd->mi_stride]->mbmi.mv[0];
        rf = xd->mi[-xd->mi_stride]->mbmi.ref_frame[0];
        for (i = 0; i < mi_width; ++i) {
          ref_mbmi = &xd->mi[-xd->mi_stride + i]->mbmi;
          const_motion &= (ref_mv.as_int == ref_mbmi->mv[0].as_int) &&
                          (ref_frame == ref_mbmi->ref_frame[0]);
          skip_ref_frame &= (rf == ref_mbmi->ref_frame[0]);
        }
      }

      if ((mi_col - 1) >= tile_info->mi_col_start) {
        if (ref_mv.as_int == INVALID_MV)
          ref_mv = xd->mi[-1]->mbmi.mv[0];
        if (rf == NONE)
          rf = xd->mi[-1]->mbmi.ref_frame[0];
        for (i = 0; i < mi_height; ++i) {
          ref_mbmi = &xd->mi[i * xd->mi_stride - 1]->mbmi;
          const_motion &= (ref_mv.as_int == ref_mbmi->mv[0].as_int) &&
                          (ref_frame == ref_mbmi->ref_frame[0]);
          skip_ref_frame &= (rf == ref_mbmi->ref_frame[0]);
        }
      }

      if (skip_ref_frame && this_mode != NEARESTMV && this_mode != NEWMV)
        if (rf > INTRA_FRAME)
          if (ref_frame != rf)
            continue;

      if (const_motion)
        if (this_mode == NEARMV || this_mode == ZEROMV)
          continue;
    }

    comp_pred = second_ref_frame > INTRA_FRAME;
    if (comp_pred) {
      if (!cpi->allow_comp_inter_inter)
        continue;

      // Skip compound inter modes if ARF is not available.
      if (!(cpi->ref_frame_flags & flag_list[second_ref_frame]))
        continue;

      // Do not allow compound prediction if the segment level reference frame
      // feature is in use as in this case there can only be one reference.
      if (vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
        continue;

      if ((mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA) &&
          best_mode_index >= 0 && best_mbmode.ref_frame[0] == INTRA_FRAME)
        continue;

      mode_excluded = cm->reference_mode == SINGLE_REFERENCE;
    } else {
      if (ref_frame != INTRA_FRAME)
        mode_excluded = cm->reference_mode == COMPOUND_REFERENCE;
    }

    if (ref_frame == INTRA_FRAME) {
      if (sf->adaptive_mode_search)
        if ((x->source_variance << num_pels_log2_lookup[bsize]) > best_pred_sse)
          continue;

      if (this_mode != DC_PRED) {
        // Disable intra modes other than DC_PRED for blocks with low variance
        // Threshold for intra skipping based on source variance
        // TODO(debargha): Specialize the threshold for super block sizes
        const unsigned int skip_intra_var_thresh = 64;
        if ((mode_search_skip_flags & FLAG_SKIP_INTRA_LOWVAR) &&
            x->source_variance < skip_intra_var_thresh)
          continue;
        // Only search the oblique modes if the best so far is
        // one of the neighboring directional modes
        if ((mode_search_skip_flags & FLAG_SKIP_INTRA_BESTINTER) &&
            (this_mode >= D45_PRED && this_mode <= TM_PRED)) {
          if (best_mode_index >= 0 &&
              best_mbmode.ref_frame[0] > INTRA_FRAME)
            continue;
        }
        if (mode_search_skip_flags & FLAG_SKIP_INTRA_DIRMISMATCH) {
          if (conditional_skipintra(this_mode, best_intra_mode))
              continue;
        }
      }
    } else {
      const MV_REFERENCE_FRAME ref_frames[2] = {ref_frame, second_ref_frame};
      if (!check_best_zero_mv(cpi, mbmi->mode_context, frame_mv,
                              this_mode, ref_frames))
        continue;
    }

    mbmi->mode = this_mode;
    mbmi->uv_mode = DC_PRED;
    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;
    // Evaluate all sub-pel filters irrespective of whether we can use
    // them for this frame.
    mbmi->interp_filter = cm->interp_filter == SWITCHABLE ? EIGHTTAP
                                                          : cm->interp_filter;
    mbmi->mv[0].as_int = mbmi->mv[1].as_int = 0;

    x->skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred)
        xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    for (i = 0; i < TX_MODES; ++i)
      tx_cache[i] = INT64_MAX;

    if (ref_frame == INTRA_FRAME) {
      TX_SIZE uv_tx;
      struct macroblockd_plane *const pd = &xd->plane[1];
      memset(x->skip_txfm, 0, sizeof(x->skip_txfm));
      super_block_yrd(cpi, x, &rate_y, &distortion_y, &skippable,
                      NULL, bsize, tx_cache, best_rd);
      if (rate_y == INT_MAX)
        continue;

      uv_tx = get_uv_tx_size_impl(mbmi->tx_size, bsize, pd->subsampling_x,
                                  pd->subsampling_y);
      if (rate_uv_intra[uv_tx] == INT_MAX) {
        choose_intra_uv_mode(cpi, x, ctx, bsize, uv_tx,
                             &rate_uv_intra[uv_tx], &rate_uv_tokenonly[uv_tx],
                             &dist_uv[uv_tx], &skip_uv[uv_tx], &mode_uv[uv_tx]);
      }

      rate_uv = rate_uv_tokenonly[uv_tx];
      distortion_uv = dist_uv[uv_tx];
      skippable = skippable && skip_uv[uv_tx];
      mbmi->uv_mode = mode_uv[uv_tx];

      rate2 = rate_y + cpi->mbmode_cost[mbmi->mode] + rate_uv_intra[uv_tx];
      if (this_mode != DC_PRED && this_mode != TM_PRED)
        rate2 += intra_cost_penalty;
      distortion2 = distortion_y + distortion_uv;
    } else {
      this_rd = handle_inter_mode(cpi, x, bsize,
                                  tx_cache,
                                  &rate2, &distortion2, &skippable,
                                  &rate_y, &rate_uv,
                                  &disable_skip, frame_mv,
                                  mi_row, mi_col,
                                  single_newmv, single_inter_filter,
                                  single_skippable, &total_sse, best_rd,
                                  &mask_filter, filter_cache);
      if (this_rd == INT64_MAX)
        continue;

      compmode_cost = vp9_cost_bit(comp_mode_p, comp_pred);

      if (cm->reference_mode == REFERENCE_MODE_SELECT)
        rate2 += compmode_cost;
    }

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (comp_pred) {
      rate2 += ref_costs_comp[ref_frame];
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

    if (!disable_skip) {
      if (skippable) {
        // Back out the coefficient coding costs
        rate2 -= (rate_y + rate_uv);

        // Cost the skip mb case
        rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 1);
      } else if (ref_frame != INTRA_FRAME && !xd->lossless) {
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv, distortion2) <
            RDCOST(x->rdmult, x->rddiv, 0, total_sse)) {
          // Add in the cost of the no skip flag.
          rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 0);
        } else {
          // FIXME(rbultje) make this work for splitmv also
          rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 1);
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          this_skip2 = 1;
        }
      } else {
        // Add in the cost of the no skip flag.
        rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 0);
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
    }

    // Apply an adjustment to the rd value based on the similarity of the
    // source variance and reconstructed variance.
    rd_variance_adjustment(cpi, x, bsize, &this_rd,
                           ref_frame, x->source_variance);

    if (ref_frame == INTRA_FRAME) {
    // Keep record of best intra rd
      if (this_rd < best_intra_rd) {
        best_intra_rd = this_rd;
        best_intra_mode = mbmi->mode;
      }
    }

    if (!disable_skip && ref_frame == INTRA_FRAME) {
      for (i = 0; i < REFERENCE_MODES; ++i)
        best_pred_rd[i] = MIN(best_pred_rd[i], this_rd);
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
        best_filter_rd[i] = MIN(best_filter_rd[i], this_rd);
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < best_rd || x->skip) {
      int max_plane = MAX_MB_PLANE;
      if (!mode_excluded) {
        // Note index of best mode so far
        best_mode_index = mode_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
          max_plane = 1;
        } else {
          best_pred_sse = x->pred_sse[ref_frame];
        }

        rd_cost->rate = rate2;
        rd_cost->dist = distortion2;
        rd_cost->rdcost = this_rd;
        best_rd = this_rd;
        best_mbmode = *mbmi;
        best_skip2 = this_skip2;
        best_mode_skippable = skippable;

        if (!x->select_tx_size)
          swap_block_ptr(x, ctx, 1, 0, 0, max_plane);
        memcpy(ctx->zcoeff_blk, x->zcoeff_blk[mbmi->tx_size],
               sizeof(uint8_t) * ctx->num_4x4_blk);

        // TODO(debargha): enhance this test with a better distortion prediction
        // based on qp, activity mask and history
        if ((mode_search_skip_flags & FLAG_EARLY_TERMINATE) &&
            (mode_index > MIN_EARLY_TERM_INDEX)) {
          int qstep = xd->plane[0].dequant[1];
          // TODO(debargha): Enhance this by specializing for each mode_index
          int scale = 4;
#if CONFIG_VP9_HIGHBITDEPTH
          if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
            qstep >>= (xd->bd - 8);
          }
#endif  // CONFIG_VP9_HIGHBITDEPTH
          if (x->source_variance < UINT_MAX) {
            const int var_adjust = (x->source_variance < 16);
            scale -= var_adjust;
          }
          if (ref_frame > INTRA_FRAME &&
              distortion2 * scale < qstep * qstep) {
            early_term = 1;
          }
        }
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip && ref_frame != INTRA_FRAME) {
      int64_t single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cm->reference_mode == REFERENCE_MODE_SELECT) {
        single_rate = rate2 - compmode_cost;
        hybrid_rate = rate2;
      } else {
        single_rate = rate2;
        hybrid_rate = rate2 + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, x->rddiv, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, x->rddiv, hybrid_rate, distortion2);

      if (!comp_pred) {
        if (single_rd < best_pred_rd[SINGLE_REFERENCE])
          best_pred_rd[SINGLE_REFERENCE] = single_rd;
      } else {
        if (single_rd < best_pred_rd[COMPOUND_REFERENCE])
          best_pred_rd[COMPOUND_REFERENCE] = single_rd;
      }
      if (hybrid_rd < best_pred_rd[REFERENCE_MODE_SELECT])
        best_pred_rd[REFERENCE_MODE_SELECT] = hybrid_rd;

      /* keep record of best filter type */
      if (!mode_excluded && cm->interp_filter != BILINEAR) {
        int64_t ref = filter_cache[cm->interp_filter == SWITCHABLE ?
                              SWITCHABLE_FILTERS : cm->interp_filter];

        for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
          int64_t adj_rd;
          if (ref == INT64_MAX)
            adj_rd = 0;
          else if (filter_cache[i] == INT64_MAX)
            // when early termination is triggered, the encoder does not have
            // access to the rate-distortion cost. it only knows that the cost
            // should be above the maximum valid value. hence it takes the known
            // maximum plus an arbitrary constant as the rate-distortion cost.
            adj_rd = mask_filter - ref + 10;
          else
            adj_rd = filter_cache[i] - ref;

          adj_rd += this_rd;
          best_filter_rd[i] = MIN(best_filter_rd[i], adj_rd);
        }
      }
    }

    /* keep record of best txfm size */
    if (bsize < BLOCK_32X32) {
      if (bsize < BLOCK_16X16)
        tx_cache[ALLOW_16X16] = tx_cache[ALLOW_8X8];

      tx_cache[ALLOW_32X32] = tx_cache[ALLOW_16X16];
    }
    if (!mode_excluded && this_rd != INT64_MAX) {
      for (i = 0; i < TX_MODES && tx_cache[i] < INT64_MAX; i++) {
        int64_t adj_rd = INT64_MAX;
        adj_rd = this_rd + tx_cache[i] - tx_cache[cm->tx_mode];

        if (adj_rd < best_tx_rd[i])
          best_tx_rd[i] = adj_rd;
      }
    }

    if (early_term)
      break;

    if (x->skip && !comp_pred)
      break;
  }

  // The inter modes' rate costs are not calculated precisely in some cases.
  // Therefore, sometimes, NEWMV is chosen instead of NEARESTMV, NEARMV, and
  // ZEROMV. Here, checks are added for those cases, and the mode decisions
  // are corrected.
  if (best_mbmode.mode == NEWMV) {
    const MV_REFERENCE_FRAME refs[2] = {best_mbmode.ref_frame[0],
        best_mbmode.ref_frame[1]};
    int comp_pred_mode = refs[1] > INTRA_FRAME;

    if (frame_mv[NEARESTMV][refs[0]].as_int == best_mbmode.mv[0].as_int &&
        ((comp_pred_mode && frame_mv[NEARESTMV][refs[1]].as_int ==
            best_mbmode.mv[1].as_int) || !comp_pred_mode))
      best_mbmode.mode = NEARESTMV;
    else if (frame_mv[NEARMV][refs[0]].as_int == best_mbmode.mv[0].as_int &&
        ((comp_pred_mode && frame_mv[NEARMV][refs[1]].as_int ==
            best_mbmode.mv[1].as_int) || !comp_pred_mode))
      best_mbmode.mode = NEARMV;
    else if (best_mbmode.mv[0].as_int == 0 &&
        ((comp_pred_mode && best_mbmode.mv[1].as_int == 0) || !comp_pred_mode))
      best_mbmode.mode = ZEROMV;
  }

  if (best_mode_index < 0 || best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  // If we used an estimate for the uv intra rd in the loop above...
  if (sf->use_uv_intra_rd_estimate) {
    // Do Intra UV best rd mode selection if best mode choice above was intra.
    if (best_mbmode.ref_frame[0] == INTRA_FRAME) {
      TX_SIZE uv_tx_size;
      *mbmi = best_mbmode;
      uv_tx_size = get_uv_tx_size(mbmi, &xd->plane[1]);
      rd_pick_intra_sbuv_mode(cpi, x, ctx, &rate_uv_intra[uv_tx_size],
                              &rate_uv_tokenonly[uv_tx_size],
                              &dist_uv[uv_tx_size],
                              &skip_uv[uv_tx_size],
                              bsize < BLOCK_8X8 ? BLOCK_8X8 : bsize,
                              uv_tx_size);
    }
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == best_mbmode.interp_filter) ||
         !is_inter_block(&best_mbmode));

  if (!cpi->rc.is_src_frame_alt_ref)
    vp9_update_rd_thresh_fact(tile_data->thresh_freq_fact,
                              sf->adaptive_rd_thresh, bsize, best_mode_index);

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;

  for (i = 0; i < REFERENCE_MODES; ++i) {
    if (best_pred_rd[i] == INT64_MAX)
      best_pred_diff[i] = INT_MIN;
    else
      best_pred_diff[i] = best_rd - best_pred_rd[i];
  }

  if (!x->skip) {
    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
      if (best_filter_rd[i] == INT64_MAX)
        best_filter_diff[i] = 0;
      else
        best_filter_diff[i] = best_rd - best_filter_rd[i];
    }
    if (cm->interp_filter == SWITCHABLE)
      assert(best_filter_diff[SWITCHABLE_FILTERS] == 0);
    for (i = 0; i < TX_MODES; i++) {
      if (best_tx_rd[i] == INT64_MAX)
        best_tx_diff[i] = 0;
      else
        best_tx_diff[i] = best_rd - best_tx_rd[i];
    }
  } else {
    vp9_zero(best_filter_diff);
    vp9_zero(best_tx_diff);
  }

  // TODO(yunqingwang): Moving this line in front of the above best_filter_diff
  // updating code causes PSNR loss. Need to figure out the confliction.
  x->skip |= best_mode_skippable;

  if (!x->skip && !x->select_tx_size) {
    int has_high_freq_coeff = 0;
    int plane;
    int max_plane = is_inter_block(&xd->mi[0]->mbmi)
                        ? MAX_MB_PLANE : 1;
    for (plane = 0; plane < max_plane; ++plane) {
      x->plane[plane].eobs = ctx->eobs_pbuf[plane][1];
      has_high_freq_coeff |= vp9_has_high_freq_in_plane(x, bsize, plane);
    }

    for (plane = max_plane; plane < MAX_MB_PLANE; ++plane) {
      x->plane[plane].eobs = ctx->eobs_pbuf[plane][2];
      has_high_freq_coeff |= vp9_has_high_freq_in_plane(x, bsize, plane);
    }

    best_mode_skippable |= !has_high_freq_coeff;
  }

  assert(best_mode_index >= 0);

  store_coding_context(x, ctx, best_mode_index, best_pred_diff,
                       best_tx_diff, best_filter_diff, best_mode_skippable);
}

void vp9_rd_pick_inter_mode_sb_seg_skip(VP9_COMP *cpi,
                                        TileDataEnc *tile_data,
                                        MACROBLOCK *x,
                                        RD_COST *rd_cost,
                                        BLOCK_SIZE bsize,
                                        PICK_MODE_CONTEXT *ctx,
                                        int64_t best_rd_so_far) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  unsigned char segment_id = mbmi->segment_id;
  const int comp_pred = 0;
  int i;
  int64_t best_tx_diff[TX_MODES];
  int64_t best_pred_diff[REFERENCE_MODES];
  int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  vp9_prob comp_mode_p;
  INTERP_FILTER best_filter = SWITCHABLE;
  int64_t this_rd = INT64_MAX;
  int rate2 = 0;
  const int64_t distortion2 = 0;

  x->skip_encode = cpi->sf.skip_encode_frame && x->q_index < QIDX_SKIP_THRESH;

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < MAX_REF_FRAMES; ++i)
    x->pred_sse[i] = INT_MAX;
  for (i = LAST_FRAME; i < MAX_REF_FRAMES; ++i)
    x->pred_mv_sad[i] = INT_MAX;

  rd_cost->rate = INT_MAX;

  assert(vp9_segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP));

  mbmi->mode = ZEROMV;
  mbmi->uv_mode = DC_PRED;
  mbmi->ref_frame[0] = LAST_FRAME;
  mbmi->ref_frame[1] = NONE;
  mbmi->mv[0].as_int = 0;
  x->skip = 1;

  if (cm->interp_filter != BILINEAR) {
    best_filter = EIGHTTAP;
    if (cm->interp_filter == SWITCHABLE &&
        x->source_variance >= cpi->sf.disable_filter_search_var_thresh) {
      int rs;
      int best_rs = INT_MAX;
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        mbmi->interp_filter = i;
        rs = vp9_get_switchable_rate(cpi, xd);
        if (rs < best_rs) {
          best_rs = rs;
          best_filter = mbmi->interp_filter;
        }
      }
    }
  }
  // Set the appropriate filter
  if (cm->interp_filter == SWITCHABLE) {
    mbmi->interp_filter = best_filter;
    rate2 += vp9_get_switchable_rate(cpi, xd);
  } else {
    mbmi->interp_filter = cm->interp_filter;
  }

  if (cm->reference_mode == REFERENCE_MODE_SELECT)
    rate2 += vp9_cost_bit(comp_mode_p, comp_pred);

  // Estimate the reference frame signaling cost and add it
  // to the rolling cost variable.
  rate2 += ref_costs_single[LAST_FRAME];
  this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);

  rd_cost->rate = rate2;
  rd_cost->dist = distortion2;
  rd_cost->rdcost = this_rd;

  if (this_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == mbmi->interp_filter));

  vp9_update_rd_thresh_fact(tile_data->thresh_freq_fact,
                            cpi->sf.adaptive_rd_thresh, bsize, THR_ZEROMV);

  vp9_zero(best_pred_diff);
  vp9_zero(best_filter_diff);
  vp9_zero(best_tx_diff);

  if (!x->select_tx_size)
    swap_block_ptr(x, ctx, 1, 0, 0, MAX_MB_PLANE);
  store_coding_context(x, ctx, THR_ZEROMV,
                       best_pred_diff, best_tx_diff, best_filter_diff, 0);
}

void vp9_rd_pick_inter_mode_sub8x8(VP9_COMP *cpi,
                                   TileDataEnc *tile_data,
                                   MACROBLOCK *x,
                                   int mi_row, int mi_col,
                                   RD_COST *rd_cost,
                                   BLOCK_SIZE bsize,
                                   PICK_MODE_CONTEXT *ctx,
                                   int64_t best_rd_so_far) {
  VP9_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  RD_OPT *const rd_opt = &cpi->rd;
  SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct segmentation *const seg = &cm->seg;
  MV_REFERENCE_FRAME ref_frame, second_ref_frame;
  unsigned char segment_id = mbmi->segment_id;
  int comp_pred, i;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[4][MAX_MB_PLANE];
  static const int flag_list[4] = { 0, VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                    VP9_ALT_FLAG };
  int64_t best_rd = best_rd_so_far;
  int64_t best_yrd = best_rd_so_far;  // FIXME(rbultje) more precise
  static const int64_t best_tx_diff[TX_MODES] = { 0 };
  int64_t best_pred_diff[REFERENCE_MODES];
  int64_t best_pred_rd[REFERENCE_MODES];
  int64_t best_filter_rd[SWITCHABLE_FILTER_CONTEXTS];
  int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS];
  MB_MODE_INFO best_mbmode;
  int ref_index, best_ref_index = 0;
  unsigned int ref_costs_single[MAX_REF_FRAMES], ref_costs_comp[MAX_REF_FRAMES];
  vp9_prob comp_mode_p;
  INTERP_FILTER tmp_best_filter = SWITCHABLE;
  int rate_uv_intra, rate_uv_tokenonly;
  int64_t dist_uv;
  int skip_uv;
  PREDICTION_MODE mode_uv = DC_PRED;
  const int intra_cost_penalty = vp9_get_intra_cost_penalty(
      cm->base_qindex, cm->y_dc_delta_q, cm->bit_depth);
  int_mv seg_mvs[4][MAX_REF_FRAMES];
  b_mode_info best_bmodes[4];
  int best_skip2 = 0;
  int ref_frame_skip_mask[2] = { 0 };
  int64_t mask_filter = 0;
  int64_t filter_cache[SWITCHABLE_FILTER_CONTEXTS];

  x->skip_encode = sf->skip_encode_frame && x->q_index < QIDX_SKIP_THRESH;
  memset(x->zcoeff_blk[TX_4X4], 0, 4);
  vp9_zero(best_mbmode);

  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
    filter_cache[i] = INT64_MAX;

  for (i = 0; i < 4; i++) {
    int j;
    for (j = 0; j < MAX_REF_FRAMES; j++)
      seg_mvs[i][j].as_int = INVALID_MV;
  }

  estimate_ref_frame_costs(cm, xd, segment_id, ref_costs_single, ref_costs_comp,
                           &comp_mode_p);

  for (i = 0; i < REFERENCE_MODES; ++i)
    best_pred_rd[i] = INT64_MAX;
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
    best_filter_rd[i] = INT64_MAX;
  rate_uv_intra = INT_MAX;

  rd_cost->rate = INT_MAX;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      setup_buffer_inter(cpi, x, tile_info,
                         ref_frame, bsize, mi_row, mi_col,
                         frame_mv[NEARESTMV], frame_mv[NEARMV],
                         yv12_mb);
    } else {
      ref_frame_skip_mask[0] |= (1 << ref_frame);
      ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

  for (ref_index = 0; ref_index < MAX_REFS; ++ref_index) {
    int mode_excluded = 0;
    int64_t this_rd = INT64_MAX;
    int disable_skip = 0;
    int compmode_cost = 0;
    int rate2 = 0, rate_y = 0, rate_uv = 0;
    int64_t distortion2 = 0, distortion_y = 0, distortion_uv = 0;
    int skippable = 0;
    int i;
    int this_skip2 = 0;
    int64_t total_sse = INT_MAX;
    int early_term = 0;

    ref_frame = vp9_ref_order[ref_index].ref_frame[0];
    second_ref_frame = vp9_ref_order[ref_index].ref_frame[1];

    // Look at the reference frame of the best mode so far and set the
    // skip mask to look at a subset of the remaining modes.
    if (ref_index > 2 && sf->mode_skip_start < MAX_MODES) {
      if (ref_index == 3) {
        switch (best_mbmode.ref_frame[0]) {
          case INTRA_FRAME:
            break;
          case LAST_FRAME:
            ref_frame_skip_mask[0] |= (1 << GOLDEN_FRAME) | (1 << ALTREF_FRAME);
            ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
            break;
          case GOLDEN_FRAME:
            ref_frame_skip_mask[0] |= (1 << LAST_FRAME) | (1 << ALTREF_FRAME);
            ref_frame_skip_mask[1] |= SECOND_REF_FRAME_MASK;
            break;
          case ALTREF_FRAME:
            ref_frame_skip_mask[0] |= (1 << GOLDEN_FRAME) | (1 << LAST_FRAME);
            break;
          case NONE:
          case MAX_REF_FRAMES:
            assert(0 && "Invalid Reference frame");
            break;
        }
      }
    }

    if ((ref_frame_skip_mask[0] & (1 << ref_frame)) &&
        (ref_frame_skip_mask[1] & (1 << MAX(0, second_ref_frame))))
      continue;

    // Test best rd so far against threshold for trying this mode.
    if (rd_less_than_thresh(best_rd,
                            rd_opt->threshes[segment_id][bsize][ref_index],
                            tile_data->thresh_freq_fact[bsize][ref_index]))
      continue;

    comp_pred = second_ref_frame > INTRA_FRAME;
    if (comp_pred) {
      if (!cpi->allow_comp_inter_inter)
        continue;
      if (!(cpi->ref_frame_flags & flag_list[second_ref_frame]))
        continue;
      // Do not allow compound prediction if the segment level reference frame
      // feature is in use as in this case there can only be one reference.
      if (vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
        continue;

      if ((sf->mode_search_skip_flags & FLAG_SKIP_COMP_BESTINTRA) &&
          best_mbmode.ref_frame[0] == INTRA_FRAME)
        continue;
    }

    // TODO(jingning, jkoleszar): scaling reference frame not supported for
    // sub8x8 blocks.
    if (ref_frame > INTRA_FRAME &&
        vp9_is_scaled(&cm->frame_refs[ref_frame - 1].sf))
      continue;

    if (second_ref_frame > INTRA_FRAME &&
        vp9_is_scaled(&cm->frame_refs[second_ref_frame - 1].sf))
      continue;

    if (comp_pred)
      mode_excluded = cm->reference_mode == SINGLE_REFERENCE;
    else if (ref_frame != INTRA_FRAME)
      mode_excluded = cm->reference_mode == COMPOUND_REFERENCE;

    // If the segment reference frame feature is enabled....
    // then do nothing if the current ref frame is not allowed..
    if (vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME) &&
        vp9_get_segdata(seg, segment_id, SEG_LVL_REF_FRAME) != (int)ref_frame) {
      continue;
    // Disable this drop out case if the ref frame
    // segment level feature is enabled for this segment. This is to
    // prevent the possibility that we end up unable to pick any mode.
    } else if (!vp9_segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME)) {
      // Only consider ZEROMV/ALTREF_FRAME for alt ref frame,
      // unless ARNR filtering is enabled in which case we want
      // an unfiltered alternative. We allow near/nearest as well
      // because they may result in zero-zero MVs but be cheaper.
      if (cpi->rc.is_src_frame_alt_ref && (cpi->oxcf.arnr_max_frames == 0))
        continue;
    }

    mbmi->tx_size = TX_4X4;
    mbmi->uv_mode = DC_PRED;
    mbmi->ref_frame[0] = ref_frame;
    mbmi->ref_frame[1] = second_ref_frame;
    // Evaluate all sub-pel filters irrespective of whether we can use
    // them for this frame.
    mbmi->interp_filter = cm->interp_filter == SWITCHABLE ? EIGHTTAP
                                                          : cm->interp_filter;
    x->skip = 0;
    set_ref_ptrs(cm, xd, ref_frame, second_ref_frame);

    // Select prediction reference frames.
    for (i = 0; i < MAX_MB_PLANE; i++) {
      xd->plane[i].pre[0] = yv12_mb[ref_frame][i];
      if (comp_pred)
        xd->plane[i].pre[1] = yv12_mb[second_ref_frame][i];
    }

    if (ref_frame == INTRA_FRAME) {
      int rate;
      if (rd_pick_intra_sub_8x8_y_mode(cpi, x, &rate, &rate_y,
                                       &distortion_y, best_rd) >= best_rd)
        continue;
      rate2 += rate;
      rate2 += intra_cost_penalty;
      distortion2 += distortion_y;

      if (rate_uv_intra == INT_MAX) {
        choose_intra_uv_mode(cpi, x, ctx, bsize, TX_4X4,
                             &rate_uv_intra,
                             &rate_uv_tokenonly,
                             &dist_uv, &skip_uv,
                             &mode_uv);
      }
      rate2 += rate_uv_intra;
      rate_uv = rate_uv_tokenonly;
      distortion2 += dist_uv;
      distortion_uv = dist_uv;
      mbmi->uv_mode = mode_uv;
    } else {
      int rate;
      int64_t distortion;
      int64_t this_rd_thresh;
      int64_t tmp_rd, tmp_best_rd = INT64_MAX, tmp_best_rdu = INT64_MAX;
      int tmp_best_rate = INT_MAX, tmp_best_ratey = INT_MAX;
      int64_t tmp_best_distortion = INT_MAX, tmp_best_sse, uv_sse;
      int tmp_best_skippable = 0;
      int switchable_filter_index;
      int_mv *second_ref = comp_pred ?
                             &mbmi->ref_mvs[second_ref_frame][0] : NULL;
      b_mode_info tmp_best_bmodes[16];
      MB_MODE_INFO tmp_best_mbmode;
      BEST_SEG_INFO bsi[SWITCHABLE_FILTERS];
      int pred_exists = 0;
      int uv_skippable;

      this_rd_thresh = (ref_frame == LAST_FRAME) ?
          rd_opt->threshes[segment_id][bsize][THR_LAST] :
          rd_opt->threshes[segment_id][bsize][THR_ALTR];
      this_rd_thresh = (ref_frame == GOLDEN_FRAME) ?
      rd_opt->threshes[segment_id][bsize][THR_GOLD] : this_rd_thresh;
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
        filter_cache[i] = INT64_MAX;

      if (cm->interp_filter != BILINEAR) {
        tmp_best_filter = EIGHTTAP;
        if (x->source_variance < sf->disable_filter_search_var_thresh) {
          tmp_best_filter = EIGHTTAP;
        } else if (sf->adaptive_pred_interp_filter == 1 &&
                   ctx->pred_interp_filter < SWITCHABLE) {
          tmp_best_filter = ctx->pred_interp_filter;
        } else if (sf->adaptive_pred_interp_filter == 2) {
          tmp_best_filter = ctx->pred_interp_filter < SWITCHABLE ?
                              ctx->pred_interp_filter : 0;
        } else {
          for (switchable_filter_index = 0;
               switchable_filter_index < SWITCHABLE_FILTERS;
               ++switchable_filter_index) {
            int newbest, rs;
            int64_t rs_rd;
            mbmi->interp_filter = switchable_filter_index;
            tmp_rd = rd_pick_best_sub8x8_mode(cpi, x, tile_info,
                                              &mbmi->ref_mvs[ref_frame][0],
                                              second_ref, best_yrd, &rate,
                                              &rate_y, &distortion,
                                              &skippable, &total_sse,
                                              (int) this_rd_thresh, seg_mvs,
                                              bsi, switchable_filter_index,
                                              mi_row, mi_col);

            if (tmp_rd == INT64_MAX)
              continue;
            rs = vp9_get_switchable_rate(cpi, xd);
            rs_rd = RDCOST(x->rdmult, x->rddiv, rs, 0);
            filter_cache[switchable_filter_index] = tmp_rd;
            filter_cache[SWITCHABLE_FILTERS] =
                MIN(filter_cache[SWITCHABLE_FILTERS],
                    tmp_rd + rs_rd);
            if (cm->interp_filter == SWITCHABLE)
              tmp_rd += rs_rd;

            mask_filter = MAX(mask_filter, tmp_rd);

            newbest = (tmp_rd < tmp_best_rd);
            if (newbest) {
              tmp_best_filter = mbmi->interp_filter;
              tmp_best_rd = tmp_rd;
            }
            if ((newbest && cm->interp_filter == SWITCHABLE) ||
                (mbmi->interp_filter == cm->interp_filter &&
                 cm->interp_filter != SWITCHABLE)) {
              tmp_best_rdu = tmp_rd;
              tmp_best_rate = rate;
              tmp_best_ratey = rate_y;
              tmp_best_distortion = distortion;
              tmp_best_sse = total_sse;
              tmp_best_skippable = skippable;
              tmp_best_mbmode = *mbmi;
              for (i = 0; i < 4; i++) {
                tmp_best_bmodes[i] = xd->mi[0]->bmi[i];
                x->zcoeff_blk[TX_4X4][i] = !x->plane[0].eobs[i];
              }
              pred_exists = 1;
              if (switchable_filter_index == 0 &&
                  sf->use_rd_breakout &&
                  best_rd < INT64_MAX) {
                if (tmp_best_rdu / 2 > best_rd) {
                  // skip searching the other filters if the first is
                  // already substantially larger than the best so far
                  tmp_best_filter = mbmi->interp_filter;
                  tmp_best_rdu = INT64_MAX;
                  break;
                }
              }
            }
          }  // switchable_filter_index loop
        }
      }

      if (tmp_best_rdu == INT64_MAX && pred_exists)
        continue;

      mbmi->interp_filter = (cm->interp_filter == SWITCHABLE ?
                             tmp_best_filter : cm->interp_filter);
      if (!pred_exists) {
        // Handles the special case when a filter that is not in the
        // switchable list (bilinear, 6-tap) is indicated at the frame level
        tmp_rd = rd_pick_best_sub8x8_mode(cpi, x, tile_info,
                                          &mbmi->ref_mvs[ref_frame][0],
                                          second_ref, best_yrd, &rate, &rate_y,
                                          &distortion, &skippable, &total_sse,
                                          (int) this_rd_thresh, seg_mvs, bsi, 0,
                                          mi_row, mi_col);
        if (tmp_rd == INT64_MAX)
          continue;
      } else {
        total_sse = tmp_best_sse;
        rate = tmp_best_rate;
        rate_y = tmp_best_ratey;
        distortion = tmp_best_distortion;
        skippable = tmp_best_skippable;
        *mbmi = tmp_best_mbmode;
        for (i = 0; i < 4; i++)
          xd->mi[0]->bmi[i] = tmp_best_bmodes[i];
      }

      rate2 += rate;
      distortion2 += distortion;

      if (cm->interp_filter == SWITCHABLE)
        rate2 += vp9_get_switchable_rate(cpi, xd);

      if (!mode_excluded)
        mode_excluded = comp_pred ? cm->reference_mode == SINGLE_REFERENCE
                                  : cm->reference_mode == COMPOUND_REFERENCE;

      compmode_cost = vp9_cost_bit(comp_mode_p, comp_pred);

      tmp_best_rdu = best_rd -
          MIN(RDCOST(x->rdmult, x->rddiv, rate2, distortion2),
              RDCOST(x->rdmult, x->rddiv, 0, total_sse));

      if (tmp_best_rdu > 0) {
        // If even the 'Y' rd value of split is higher than best so far
        // then dont bother looking at UV
        vp9_build_inter_predictors_sbuv(&x->e_mbd, mi_row, mi_col,
                                        BLOCK_8X8);
        memset(x->skip_txfm, 0, sizeof(x->skip_txfm));
        if (!super_block_uvrd(cpi, x, &rate_uv, &distortion_uv, &uv_skippable,
                              &uv_sse, BLOCK_8X8, tmp_best_rdu))
          continue;

        rate2 += rate_uv;
        distortion2 += distortion_uv;
        skippable = skippable && uv_skippable;
        total_sse += uv_sse;
      }
    }

    if (cm->reference_mode == REFERENCE_MODE_SELECT)
      rate2 += compmode_cost;

    // Estimate the reference frame signaling cost and add it
    // to the rolling cost variable.
    if (second_ref_frame > INTRA_FRAME) {
      rate2 += ref_costs_comp[ref_frame];
    } else {
      rate2 += ref_costs_single[ref_frame];
    }

    if (!disable_skip) {
      // Skip is never coded at the segment level for sub8x8 blocks and instead
      // always coded in the bitstream at the mode info level.

      if (ref_frame != INTRA_FRAME && !xd->lossless) {
        if (RDCOST(x->rdmult, x->rddiv, rate_y + rate_uv, distortion2) <
            RDCOST(x->rdmult, x->rddiv, 0, total_sse)) {
          // Add in the cost of the no skip flag.
          rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 0);
        } else {
          // FIXME(rbultje) make this work for splitmv also
          rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 1);
          distortion2 = total_sse;
          assert(total_sse >= 0);
          rate2 -= (rate_y + rate_uv);
          rate_y = 0;
          rate_uv = 0;
          this_skip2 = 1;
        }
      } else {
        // Add in the cost of the no skip flag.
        rate2 += vp9_cost_bit(vp9_get_skip_prob(cm, xd), 0);
      }

      // Calculate the final RD estimate for this mode.
      this_rd = RDCOST(x->rdmult, x->rddiv, rate2, distortion2);
    }

    if (!disable_skip && ref_frame == INTRA_FRAME) {
      for (i = 0; i < REFERENCE_MODES; ++i)
        best_pred_rd[i] = MIN(best_pred_rd[i], this_rd);
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
        best_filter_rd[i] = MIN(best_filter_rd[i], this_rd);
    }

    // Did this mode help.. i.e. is it the new best mode
    if (this_rd < best_rd || x->skip) {
      if (!mode_excluded) {
        int max_plane = MAX_MB_PLANE;
        // Note index of best mode so far
        best_ref_index = ref_index;

        if (ref_frame == INTRA_FRAME) {
          /* required for left and above block mv */
          mbmi->mv[0].as_int = 0;
          max_plane = 1;
        }

        rd_cost->rate = rate2;
        rd_cost->dist = distortion2;
        rd_cost->rdcost = this_rd;
        best_rd = this_rd;
        best_yrd = best_rd -
                   RDCOST(x->rdmult, x->rddiv, rate_uv, distortion_uv);
        best_mbmode = *mbmi;
        best_skip2 = this_skip2;
        if (!x->select_tx_size)
          swap_block_ptr(x, ctx, 1, 0, 0, max_plane);
        memcpy(ctx->zcoeff_blk, x->zcoeff_blk[TX_4X4],
               sizeof(uint8_t) * ctx->num_4x4_blk);

        for (i = 0; i < 4; i++)
          best_bmodes[i] = xd->mi[0]->bmi[i];

        // TODO(debargha): enhance this test with a better distortion prediction
        // based on qp, activity mask and history
        if ((sf->mode_search_skip_flags & FLAG_EARLY_TERMINATE) &&
            (ref_index > MIN_EARLY_TERM_INDEX)) {
          int qstep = xd->plane[0].dequant[1];
          // TODO(debargha): Enhance this by specializing for each mode_index
          int scale = 4;
#if CONFIG_VP9_HIGHBITDEPTH
          if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
            qstep >>= (xd->bd - 8);
          }
#endif  // CONFIG_VP9_HIGHBITDEPTH
          if (x->source_variance < UINT_MAX) {
            const int var_adjust = (x->source_variance < 16);
            scale -= var_adjust;
          }
          if (ref_frame > INTRA_FRAME &&
              distortion2 * scale < qstep * qstep) {
            early_term = 1;
          }
        }
      }
    }

    /* keep record of best compound/single-only prediction */
    if (!disable_skip && ref_frame != INTRA_FRAME) {
      int64_t single_rd, hybrid_rd, single_rate, hybrid_rate;

      if (cm->reference_mode == REFERENCE_MODE_SELECT) {
        single_rate = rate2 - compmode_cost;
        hybrid_rate = rate2;
      } else {
        single_rate = rate2;
        hybrid_rate = rate2 + compmode_cost;
      }

      single_rd = RDCOST(x->rdmult, x->rddiv, single_rate, distortion2);
      hybrid_rd = RDCOST(x->rdmult, x->rddiv, hybrid_rate, distortion2);

      if (!comp_pred && single_rd < best_pred_rd[SINGLE_REFERENCE])
        best_pred_rd[SINGLE_REFERENCE] = single_rd;
      else if (comp_pred && single_rd < best_pred_rd[COMPOUND_REFERENCE])
        best_pred_rd[COMPOUND_REFERENCE] = single_rd;

      if (hybrid_rd < best_pred_rd[REFERENCE_MODE_SELECT])
        best_pred_rd[REFERENCE_MODE_SELECT] = hybrid_rd;
    }

    /* keep record of best filter type */
    if (!mode_excluded && !disable_skip && ref_frame != INTRA_FRAME &&
        cm->interp_filter != BILINEAR) {
      int64_t ref = filter_cache[cm->interp_filter == SWITCHABLE ?
                              SWITCHABLE_FILTERS : cm->interp_filter];
      int64_t adj_rd;
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
        if (ref == INT64_MAX)
          adj_rd = 0;
        else if (filter_cache[i] == INT64_MAX)
          // when early termination is triggered, the encoder does not have
          // access to the rate-distortion cost. it only knows that the cost
          // should be above the maximum valid value. hence it takes the known
          // maximum plus an arbitrary constant as the rate-distortion cost.
          adj_rd = mask_filter - ref + 10;
        else
          adj_rd = filter_cache[i] - ref;

        adj_rd += this_rd;
        best_filter_rd[i] = MIN(best_filter_rd[i], adj_rd);
      }
    }

    if (early_term)
      break;

    if (x->skip && !comp_pred)
      break;
  }

  if (best_rd >= best_rd_so_far) {
    rd_cost->rate = INT_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  // If we used an estimate for the uv intra rd in the loop above...
  if (sf->use_uv_intra_rd_estimate) {
    // Do Intra UV best rd mode selection if best mode choice above was intra.
    if (best_mbmode.ref_frame[0] == INTRA_FRAME) {
      *mbmi = best_mbmode;
      rd_pick_intra_sbuv_mode(cpi, x, ctx, &rate_uv_intra,
                              &rate_uv_tokenonly,
                              &dist_uv,
                              &skip_uv,
                              BLOCK_8X8, TX_4X4);
    }
  }

  if (best_rd == INT64_MAX) {
    rd_cost->rate = INT_MAX;
    rd_cost->dist = INT64_MAX;
    rd_cost->rdcost = INT64_MAX;
    return;
  }

  assert((cm->interp_filter == SWITCHABLE) ||
         (cm->interp_filter == best_mbmode.interp_filter) ||
         !is_inter_block(&best_mbmode));

  vp9_update_rd_thresh_fact(tile_data->thresh_freq_fact,
                            sf->adaptive_rd_thresh, bsize, best_ref_index);

  // macroblock modes
  *mbmi = best_mbmode;
  x->skip |= best_skip2;
  if (!is_inter_block(&best_mbmode)) {
    for (i = 0; i < 4; i++)
      xd->mi[0]->bmi[i].as_mode = best_bmodes[i].as_mode;
  } else {
    for (i = 0; i < 4; ++i)
      memcpy(&xd->mi[0]->bmi[i], &best_bmodes[i], sizeof(b_mode_info));

    mbmi->mv[0].as_int = xd->mi[0]->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = xd->mi[0]->bmi[3].as_mv[1].as_int;
  }

  for (i = 0; i < REFERENCE_MODES; ++i) {
    if (best_pred_rd[i] == INT64_MAX)
      best_pred_diff[i] = INT_MIN;
    else
      best_pred_diff[i] = best_rd - best_pred_rd[i];
  }

  if (!x->skip) {
    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
      if (best_filter_rd[i] == INT64_MAX)
        best_filter_diff[i] = 0;
      else
        best_filter_diff[i] = best_rd - best_filter_rd[i];
    }
    if (cm->interp_filter == SWITCHABLE)
      assert(best_filter_diff[SWITCHABLE_FILTERS] == 0);
  } else {
    vp9_zero(best_filter_diff);
  }

  store_coding_context(x, ctx, best_ref_index,
                       best_pred_diff, best_tx_diff, best_filter_diff, 0);
}
