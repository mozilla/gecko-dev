/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_PRED_COMMON_H_
#define VP9_COMMON_VP9_PRED_COMMON_H_

#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

int vp9_get_segment_id(const VP9_COMMON *cm, const uint8_t *segment_ids,
                       BLOCK_SIZE bsize, int mi_row, int mi_col);

static INLINE int vp9_get_pred_context_seg_id(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_sip = (above_mi != NULL) ?
                        above_mi->mbmi.seg_id_predicted : 0;
  const int left_sip = (left_mi != NULL) ? left_mi->mbmi.seg_id_predicted : 0;

  return above_sip + left_sip;
}

static INLINE vp9_prob vp9_get_pred_prob_seg_id(const struct segmentation *seg,
                                                const MACROBLOCKD *xd) {
  return seg->pred_probs[vp9_get_pred_context_seg_id(xd)];
}

static INLINE int vp9_get_skip_context(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_skip = (above_mi != NULL) ? above_mi->mbmi.skip : 0;
  const int left_skip = (left_mi != NULL) ? left_mi->mbmi.skip : 0;
  return above_skip + left_skip;
}

static INLINE vp9_prob vp9_get_skip_prob(const VP9_COMMON *cm,
                                         const MACROBLOCKD *xd) {
  return cm->fc->skip_probs[vp9_get_skip_context(xd)];
}

int vp9_get_pred_context_switchable_interp(const MACROBLOCKD *xd);

int vp9_get_intra_inter_context(const MACROBLOCKD *xd);

static INLINE vp9_prob vp9_get_intra_inter_prob(const VP9_COMMON *cm,
                                                const MACROBLOCKD *xd) {
  return cm->fc->intra_inter_prob[vp9_get_intra_inter_context(xd)];
}

int vp9_get_reference_mode_context(const VP9_COMMON *cm, const MACROBLOCKD *xd);

static INLINE vp9_prob vp9_get_reference_mode_prob(const VP9_COMMON *cm,
                                                   const MACROBLOCKD *xd) {
  return cm->fc->comp_inter_prob[vp9_get_reference_mode_context(cm, xd)];
}

int vp9_get_pred_context_comp_ref_p(const VP9_COMMON *cm,
                                    const MACROBLOCKD *xd);

static INLINE vp9_prob vp9_get_pred_prob_comp_ref_p(const VP9_COMMON *cm,
                                                    const MACROBLOCKD *xd) {
  const int pred_context = vp9_get_pred_context_comp_ref_p(cm, xd);
  return cm->fc->comp_ref_prob[pred_context];
}

int vp9_get_pred_context_single_ref_p1(const MACROBLOCKD *xd);

static INLINE vp9_prob vp9_get_pred_prob_single_ref_p1(const VP9_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[vp9_get_pred_context_single_ref_p1(xd)][0];
}

int vp9_get_pred_context_single_ref_p2(const MACROBLOCKD *xd);

static INLINE vp9_prob vp9_get_pred_prob_single_ref_p2(const VP9_COMMON *cm,
                                                       const MACROBLOCKD *xd) {
  return cm->fc->single_ref_prob[vp9_get_pred_context_single_ref_p2(xd)][1];
}

int vp9_get_tx_size_context(const MACROBLOCKD *xd);

static INLINE const vp9_prob *get_tx_probs(TX_SIZE max_tx_size, int ctx,
                                           const struct tx_probs *tx_probs) {
  switch (max_tx_size) {
    case TX_8X8:
      return tx_probs->p8x8[ctx];
    case TX_16X16:
      return tx_probs->p16x16[ctx];
    case TX_32X32:
      return tx_probs->p32x32[ctx];
    default:
      assert(0 && "Invalid max_tx_size.");
      return NULL;
  }
}

static INLINE const vp9_prob *get_tx_probs2(TX_SIZE max_tx_size,
                                            const MACROBLOCKD *xd,
                                            const struct tx_probs *tx_probs) {
  return get_tx_probs(max_tx_size, vp9_get_tx_size_context(xd), tx_probs);
}

static INLINE unsigned int *get_tx_counts(TX_SIZE max_tx_size, int ctx,
                                          struct tx_counts *tx_counts) {
  switch (max_tx_size) {
    case TX_8X8:
      return tx_counts->p8x8[ctx];
    case TX_16X16:
      return tx_counts->p16x16[ctx];
    case TX_32X32:
      return tx_counts->p32x32[ctx];
    default:
      assert(0 && "Invalid max_tx_size.");
      return NULL;
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_PRED_COMMON_H_
