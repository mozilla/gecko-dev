/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>

#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_speed_features.h"

// Intra only frames, golden frames (except alt ref overlays) and
// alt ref frames tend to be coded at a higher than ambient quality
static int frame_is_boosted(const VP9_COMP *cpi) {
  return frame_is_kf_gf_arf(cpi) || vp9_is_upper_layer_key_frame(cpi);
}

static void set_good_speed_feature_framesize_dependent(VP9_COMMON *cm,
                                                       SPEED_FEATURES *sf,
                                                       int speed) {
  if (speed >= 1) {
    if (MIN(cm->width, cm->height) >= 720) {
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
      sf->partition_search_breakout_dist_thr = (1 << 23);
    } else {
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;
      sf->partition_search_breakout_dist_thr = (1 << 21);
    }
  }

  if (speed >= 2) {
    if (MIN(cm->width, cm->height) >= 720) {
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
      sf->adaptive_pred_interp_filter = 0;
      sf->partition_search_breakout_dist_thr = (1 << 24);
      sf->partition_search_breakout_rate_thr = 120;
    } else {
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;
      sf->partition_search_breakout_dist_thr = (1 << 22);
      sf->partition_search_breakout_rate_thr = 100;
    }
  }

  if (speed >= 3) {
    if (MIN(cm->width, cm->height) >= 720) {
      sf->disable_split_mask = DISABLE_ALL_SPLIT;
      sf->schedule_mode_search = cm->base_qindex < 220 ? 1 : 0;
      sf->partition_search_breakout_dist_thr = (1 << 25);
      sf->partition_search_breakout_rate_thr = 200;
    } else {
      sf->max_intra_bsize = BLOCK_32X32;
      sf->disable_split_mask = DISABLE_ALL_INTER_SPLIT;
      sf->schedule_mode_search = cm->base_qindex < 175 ? 1 : 0;
      sf->partition_search_breakout_dist_thr = (1 << 23);
      sf->partition_search_breakout_rate_thr = 120;
    }
  }

  if (speed >= 4) {
    if (MIN(cm->width, cm->height) >= 720) {
      sf->partition_search_breakout_dist_thr = (1 << 26);
    } else {
      sf->partition_search_breakout_dist_thr = (1 << 24);
    }
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
  }
}

// Sets a partition size down to which the auto partition code will always
// search (can go lower), based on the image dimensions. The logic here
// is that the extent to which ringing artefacts are offensive, depends
// partly on the screen area that over which they propogate. Propogation is
// limited by transform block size but the screen area take up by a given block
// size will be larger for a small image format stretched to full screen.
static BLOCK_SIZE set_partition_min_limit(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  unsigned int screen_area = (cm->width * cm->height);

  // Select block size based on image format size.
  if (screen_area < 1280 * 720) {
    // Formats smaller in area than 720P
    return BLOCK_4X4;
  } else if (screen_area < 1920 * 1080) {
    // Format >= 720P and < 1080P
    return BLOCK_8X8;
  } else {
    // Formats 1080P and up
    return BLOCK_16X16;
  }
}

static void set_good_speed_feature(VP9_COMP *cpi, VP9_COMMON *cm,
                                   SPEED_FEATURES *sf, int speed) {
  const int boosted = frame_is_boosted(cpi);

  sf->adaptive_rd_thresh = 1;
  sf->allow_skip_recode = 1;

  if (speed >= 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check  = 1;

    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->mv.auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->mv.subpel_iters_per_step = 1;
    sf->mode_skip_start = 10;
    sf->adaptive_pred_interp_filter = 1;

    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;

    sf->tx_size_search_breakout = 1;
    sf->partition_search_breakout_rate_thr = 80;
  }

  if (speed >= 2) {
    sf->tx_size_search_method = frame_is_boosted(cpi) ? USE_FULL_RD
                                                      : USE_LARGESTALL;

    // Reference masking is not supported in dynamic scaling mode.
    sf->reference_masking = cpi->oxcf.resize_mode != RESIZE_DYNAMIC ? 1 : 0;

    sf->mode_search_skip_flags = (cm->frame_type == KEY_FRAME) ? 0 :
                                 FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_INTRA_LOWVAR;
    sf->disable_filter_search_var_thresh = 100;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;
    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->rd_auto_partition_min_limit = set_partition_min_limit(cpi);
    sf->allow_partition_search_skip = 1;
  }

  if (speed >= 3) {
    sf->tx_size_search_method = frame_is_intra_only(cm) ? USE_FULL_RD
                                                        : USE_LARGESTALL;
    sf->mv.subpel_search_method = SUBPEL_TREE_PRUNED;
    sf->adaptive_pred_interp_filter = 0;
    sf->adaptive_mode_search = 1;
    sf->cb_partition_search = !boosted;
    sf->cb_pred_filter_search = 1;
    sf->alt_ref_search_fp = 1;
    sf->recode_loop = ALLOW_RECODE_KFMAXBW;
    sf->adaptive_rd_thresh = 3;
    sf->mode_skip_start = 6;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC;
    sf->adaptive_interp_filter_search = 1;
  }

  if (speed >= 4) {
    sf->use_square_partition_only = 1;
    sf->tx_size_search_method = USE_LARGESTALL;
    sf->mv.search_method = BIGDIA;
    sf->mv.subpel_search_method = SUBPEL_TREE_PRUNED_MORE;
    sf->adaptive_rd_thresh = 4;
    if (cm->frame_type != KEY_FRAME)
      sf->mode_search_skip_flags |= FLAG_EARLY_TERMINATE;
    sf->disable_filter_search_var_thresh = 200;
    sf->use_lp32x32fdct = 1;
    sf->use_fast_coef_updates = ONE_LOOP_REDUCED;
    sf->use_fast_coef_costing = 1;
    sf->motion_field_mode_search = !boosted;
    sf->partition_search_breakout_rate_thr = 300;
  }

  if (speed >= 5) {
    int i;
    sf->optimize_coefficients = 0;
    sf->mv.search_method = HEX;
    sf->disable_filter_search_var_thresh = 500;
    for (i = 0; i < TX_SIZES; ++i) {
      sf->intra_y_mode_mask[i] = INTRA_DC;
      sf->intra_uv_mode_mask[i] = INTRA_DC;
    }
    sf->partition_search_breakout_rate_thr = 500;
    sf->mv.reduce_first_step_size = 1;
  }
}

static void set_rt_speed_feature_framesize_dependent(VP9_COMP *cpi,
    SPEED_FEATURES *sf, int speed) {
  VP9_COMMON *const cm = &cpi->common;

  if (speed >= 1) {
    if (MIN(cm->width, cm->height) >= 720) {
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    } else {
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;
    }
  }

  if (speed >= 2) {
    if (MIN(cm->width, cm->height) >= 720) {
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    } else {
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;
    }
  }

  if (speed >= 5) {
    if (MIN(cm->width, cm->height) >= 720) {
      sf->partition_search_breakout_dist_thr = (1 << 25);
    } else {
      sf->partition_search_breakout_dist_thr = (1 << 23);
    }
  }

  if (speed >= 7) {
    sf->encode_breakout_thresh = (MIN(cm->width, cm->height) >= 720) ?
        800 : 300;
  }
}

static void set_rt_speed_feature(VP9_COMP *cpi, SPEED_FEATURES *sf,
                                 int speed, vp9e_tune_content content) {
  VP9_COMMON *const cm = &cpi->common;
  const int is_keyframe = cm->frame_type == KEY_FRAME;
  const int frames_since_key = is_keyframe ? 0 : cpi->rc.frames_since_key;
  sf->static_segmentation = 0;
  sf->adaptive_rd_thresh = 1;
  sf->use_fast_coef_costing = 1;

  if (speed >= 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check = 1;
    sf->tx_size_search_method = frame_is_intra_only(cm) ? USE_FULL_RD
                                                        : USE_LARGESTALL;

    sf->use_rd_breakout = 1;

    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 1;
    sf->mv.auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
  }

  if (speed >= 2) {
    sf->mode_search_skip_flags = (cm->frame_type == KEY_FRAME) ? 0 :
                                 FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_INTRA_LOWVAR;
    sf->adaptive_pred_interp_filter = 2;

    // Disable reference masking if using spatial scaling since
    // pred_mv_sad will not be set (since vp9_mv_pred will not
    // be called).
    // TODO(marpan/agrange): Fix this condition.
    sf->reference_masking = (cpi->oxcf.resize_mode != RESIZE_DYNAMIC &&
                             cpi->svc.number_spatial_layers == 1) ? 1 : 0;

    sf->disable_filter_search_var_thresh = 50;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;
    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->lf_motion_threshold = LOW_MOTION_THRESHOLD;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;
    sf->use_lp32x32fdct = 1;
    sf->mode_skip_start = 11;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
  }

  if (speed >= 3) {
    sf->use_square_partition_only = 1;
    sf->disable_filter_search_var_thresh = 100;
    sf->use_uv_intra_rd_estimate = 1;
    sf->skip_encode_sb = 1;
    sf->mv.subpel_iters_per_step = 1;
    sf->adaptive_rd_thresh = 4;
    sf->mode_skip_start = 6;
    sf->allow_skip_recode = 0;
    sf->optimize_coefficients = 0;
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
    sf->lpf_pick = LPF_PICK_FROM_Q;
  }

  if (speed >= 4) {
    int i;
    sf->last_partitioning_redo_frequency = 4;
    sf->adaptive_rd_thresh = 5;
    sf->use_fast_coef_costing = 0;
    sf->auto_min_max_partition_size = STRICT_NEIGHBORING_MIN_MAX;
    sf->adjust_partitioning_from_last_frame =
        cm->last_frame_type != cm->frame_type || (0 ==
        (frames_since_key + 1) % sf->last_partitioning_redo_frequency);
    sf->mv.subpel_force_stop = 1;
    for (i = 0; i < TX_SIZES; i++) {
      sf->intra_y_mode_mask[i] = INTRA_DC_H_V;
      sf->intra_uv_mode_mask[i] = INTRA_DC;
    }
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC;
    sf->frame_parameter_update = 0;
    sf->mv.search_method = FAST_HEX;

    sf->inter_mode_mask[BLOCK_32X32] = INTER_NEAREST_NEAR_NEW;
    sf->inter_mode_mask[BLOCK_32X64] = INTER_NEAREST;
    sf->inter_mode_mask[BLOCK_64X32] = INTER_NEAREST;
    sf->inter_mode_mask[BLOCK_64X64] = INTER_NEAREST;
    sf->max_intra_bsize = BLOCK_32X32;
    sf->allow_skip_recode = 1;
  }

  if (speed >= 5) {
    sf->use_quant_fp = !is_keyframe;
    sf->auto_min_max_partition_size = is_keyframe ? RELAXED_NEIGHBORING_MIN_MAX
                                                  : STRICT_NEIGHBORING_MIN_MAX;
    sf->default_max_partition_size = BLOCK_32X32;
    sf->default_min_partition_size = BLOCK_8X8;
    sf->force_frame_boost = is_keyframe ||
        (frames_since_key % (sf->last_partitioning_redo_frequency << 1) == 1);
    sf->max_delta_qindex = is_keyframe ? 20 : 15;
    sf->partition_search_type = REFERENCE_PARTITION;
    sf->use_nonrd_pick_mode = 1;
    sf->allow_skip_recode = 0;
    sf->inter_mode_mask[BLOCK_32X32] = INTER_NEAREST_NEW_ZERO;
    sf->inter_mode_mask[BLOCK_32X64] = INTER_NEAREST_NEW_ZERO;
    sf->inter_mode_mask[BLOCK_64X32] = INTER_NEAREST_NEW_ZERO;
    sf->inter_mode_mask[BLOCK_64X64] = INTER_NEAREST_NEW_ZERO;
    sf->adaptive_rd_thresh = 2;
    // This feature is only enabled when partition search is disabled.
    sf->reuse_inter_pred_sby = 1;
    sf->partition_search_breakout_rate_thr = 200;
    sf->coeff_prob_appx_step = 4;
    sf->use_fast_coef_updates = is_keyframe ? TWO_LOOP : ONE_LOOP_REDUCED;
    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH;
    sf->tx_size_search_method = is_keyframe ? USE_LARGESTALL : USE_TX_8X8;

    if (!is_keyframe) {
      int i;
      if (content == VP9E_CONTENT_SCREEN) {
        for (i = 0; i < BLOCK_SIZES; ++i)
          sf->intra_y_mode_bsize_mask[i] = INTRA_DC_TM_H_V;
      } else {
        for (i = 0; i < BLOCK_SIZES; ++i)
          if (i >= BLOCK_16X16)
            sf->intra_y_mode_bsize_mask[i] = INTRA_DC;
          else
            // Use H and V intra mode for block sizes <= 16X16.
            sf->intra_y_mode_bsize_mask[i] = INTRA_DC_H_V;
      }
    }
  }

  if (speed >= 6) {
    // Adaptively switch between SOURCE_VAR_BASED_PARTITION and FIXED_PARTITION.
    sf->partition_search_type = VAR_BASED_PARTITION;
    // Turn on this to use non-RD key frame coding mode.
    sf->use_nonrd_pick_mode = 1;
    sf->mv.search_method = NSTEP;
    sf->mv.reduce_first_step_size = 1;
    sf->skip_encode_sb = 0;
  }

  if (speed >= 7) {
    sf->adaptive_rd_thresh = 3;
    sf->mv.search_method = FAST_DIAMOND;
    sf->mv.fullpel_search_step_param = 10;
  }
  if (speed >= 8) {
    sf->adaptive_rd_thresh = 4;
    sf->mv.subpel_force_stop = 2;
    sf->lpf_pick = LPF_PICK_MINIMAL_LPF;
  }
}

void vp9_set_speed_features_framesize_dependent(VP9_COMP *cpi) {
  SPEED_FEATURES *const sf = &cpi->sf;
  VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  RD_OPT *const rd = &cpi->rd;
  int i;

  if (oxcf->mode == REALTIME) {
    set_rt_speed_feature_framesize_dependent(cpi, sf, oxcf->speed);
  } else if (oxcf->mode == GOOD) {
    set_good_speed_feature_framesize_dependent(cm, sf, oxcf->speed);
  }

  if (sf->disable_split_mask == DISABLE_ALL_SPLIT) {
    sf->adaptive_pred_interp_filter = 0;
  }

  if (cpi->encode_breakout && oxcf->mode == REALTIME &&
      sf->encode_breakout_thresh > cpi->encode_breakout) {
    cpi->encode_breakout = sf->encode_breakout_thresh;
  }

  // Check for masked out split cases.
  for (i = 0; i < MAX_REFS; ++i) {
    if (sf->disable_split_mask & (1 << i)) {
      rd->thresh_mult_sub8x8[i] = INT_MAX;
    }
  }
}

void vp9_set_speed_features_framesize_independent(VP9_COMP *cpi) {
  SPEED_FEATURES *const sf = &cpi->sf;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  int i;

  // best quality defaults
  sf->frame_parameter_update = 1;
  sf->mv.search_method = NSTEP;
  sf->recode_loop = ALLOW_RECODE;
  sf->mv.subpel_search_method = SUBPEL_TREE;
  sf->mv.subpel_iters_per_step = 2;
  sf->mv.subpel_force_stop = 0;
  sf->optimize_coefficients = !is_lossless_requested(&cpi->oxcf);
  sf->mv.reduce_first_step_size = 0;
  sf->coeff_prob_appx_step = 1;
  sf->mv.auto_mv_step_size = 0;
  sf->mv.fullpel_search_step_param = 6;
  sf->comp_inter_joint_search_thresh = BLOCK_4X4;
  sf->adaptive_rd_thresh = 0;
  sf->tx_size_search_method = USE_FULL_RD;
  sf->use_lp32x32fdct = 0;
  sf->adaptive_motion_search = 0;
  sf->adaptive_pred_interp_filter = 0;
  sf->adaptive_mode_search = 0;
  sf->cb_pred_filter_search = 0;
  sf->cb_partition_search = 0;
  sf->motion_field_mode_search = 0;
  sf->alt_ref_search_fp = 0;
  sf->use_quant_fp = 0;
  sf->reference_masking = 0;
  sf->partition_search_type = SEARCH_PARTITION;
  sf->less_rectangular_check = 0;
  sf->use_square_partition_only = 0;
  sf->auto_min_max_partition_size = NOT_IN_USE;
  sf->rd_auto_partition_min_limit = BLOCK_4X4;
  sf->default_max_partition_size = BLOCK_64X64;
  sf->default_min_partition_size = BLOCK_4X4;
  sf->adjust_partitioning_from_last_frame = 0;
  sf->last_partitioning_redo_frequency = 4;
  sf->disable_split_mask = 0;
  sf->mode_search_skip_flags = 0;
  sf->force_frame_boost = 0;
  sf->max_delta_qindex = 0;
  sf->disable_filter_search_var_thresh = 0;
  sf->adaptive_interp_filter_search = 0;
  sf->allow_partition_search_skip = 0;

  for (i = 0; i < TX_SIZES; i++) {
    sf->intra_y_mode_mask[i] = INTRA_ALL;
    sf->intra_uv_mode_mask[i] = INTRA_ALL;
  }
  sf->use_rd_breakout = 0;
  sf->skip_encode_sb = 0;
  sf->use_uv_intra_rd_estimate = 0;
  sf->allow_skip_recode = 0;
  sf->lpf_pick = LPF_PICK_FROM_FULL_IMAGE;
  sf->use_fast_coef_updates = TWO_LOOP;
  sf->use_fast_coef_costing = 0;
  sf->mode_skip_start = MAX_MODES;  // Mode index at which mode skip mask set
  sf->schedule_mode_search = 0;
  sf->use_nonrd_pick_mode = 0;
  for (i = 0; i < BLOCK_SIZES; ++i)
    sf->inter_mode_mask[i] = INTER_ALL;
  sf->max_intra_bsize = BLOCK_64X64;
  sf->reuse_inter_pred_sby = 0;
  // This setting only takes effect when partition_search_type is set
  // to FIXED_PARTITION.
  sf->always_this_block_size = BLOCK_16X16;
  sf->search_type_check_frequency = 50;
  sf->encode_breakout_thresh = 0;
  // Recode loop tolerance %.
  sf->recode_tolerance = 25;
  sf->default_interp_filter = SWITCHABLE;
  sf->tx_size_search_breakout = 0;
  sf->partition_search_breakout_dist_thr = 0;
  sf->partition_search_breakout_rate_thr = 0;

  if (oxcf->mode == REALTIME)
    set_rt_speed_feature(cpi, sf, oxcf->speed, oxcf->content);
  else if (oxcf->mode == GOOD)
    set_good_speed_feature(cpi, cm, sf, oxcf->speed);

  cpi->full_search_sad = vp9_full_search_sad;
  cpi->diamond_search_sad = oxcf->mode == BEST ? vp9_full_range_search
                                               : vp9_diamond_search_sad;

  // Slow quant, dct and trellis not worthwhile for first pass
  // so make sure they are always turned off.
  if (oxcf->pass == 1)
    sf->optimize_coefficients = 0;

  // No recode for 1 pass.
  if (oxcf->pass == 0) {
    sf->recode_loop = DISALLOW_RECODE;
    sf->optimize_coefficients = 0;
  }

  if (sf->mv.subpel_search_method == SUBPEL_TREE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree;
  } else if (sf->mv.subpel_search_method == SUBPEL_TREE_PRUNED) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree_pruned;
  } else if (sf->mv.subpel_search_method == SUBPEL_TREE_PRUNED_MORE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree_pruned_more;
  } else if (sf->mv.subpel_search_method == SUBPEL_TREE_PRUNED_EVENMORE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree_pruned_evenmore;
  }

  x->optimize = sf->optimize_coefficients == 1 && oxcf->pass != 1;

  x->min_partition_size = sf->default_min_partition_size;
  x->max_partition_size = sf->default_max_partition_size;

  if (!cpi->oxcf.frame_periodic_boost) {
    sf->max_delta_qindex = 0;
  }
}
