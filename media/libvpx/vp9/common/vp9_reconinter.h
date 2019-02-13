/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_RECONINTER_H_
#define VP9_COMMON_VP9_RECONINTER_H_

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

void inter_predictor(const uint8_t *src, int src_stride,
                            uint8_t *dst, int dst_stride,
                            const int subpel_x,
                            const int subpel_y,
                            const struct scale_factors *sf,
                            int w, int h, int ref,
                            const InterpKernel *kernel,
                            int xs, int ys);

#if CONFIG_VP9_HIGHBITDEPTH
void high_inter_predictor(const uint8_t *src, int src_stride,
                                 uint8_t *dst, int dst_stride,
                                 const int subpel_x,
                                 const int subpel_y,
                                 const struct scale_factors *sf,
                                 int w, int h, int ref,
                                 const InterpKernel *kernel,
                                 int xs, int ys, int bd);
#endif  // CONFIG_VP9_HIGHBITDEPTH

MV average_split_mvs(const struct macroblockd_plane *pd, const MODE_INFO *mi,
                     int ref, int block);

MV clamp_mv_to_umv_border_sb(const MACROBLOCKD *xd, const MV *src_mv,
                             int bw, int bh, int ss_x, int ss_y);

void build_inter_predictors(MACROBLOCKD *xd, int plane, int block,
                                   int bw, int bh,
                                   int x, int y, int w, int h,
                                   int mi_x, int mi_y);

void vp9_build_inter_predictors_sby(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize);

void vp9_build_inter_predictors_sbp(MACROBLOCKD *xd, int mi_row, int mi_col,
                                    BLOCK_SIZE bsize, int plane);

void vp9_build_inter_predictors_sbuv(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     BLOCK_SIZE bsize);

void vp9_build_inter_predictors_sb(MACROBLOCKD *xd, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize);

void vp9_build_inter_predictor(const uint8_t *src, int src_stride,
                               uint8_t *dst, int dst_stride,
                               const MV *mv_q3,
                               const struct scale_factors *sf,
                               int w, int h, int do_avg,
                               const InterpKernel *kernel,
                               enum mv_precision precision,
                               int x, int y);

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_highbd_build_inter_predictor(const uint8_t *src, int src_stride,
                                      uint8_t *dst, int dst_stride,
                                      const MV *mv_q3,
                                      const struct scale_factors *sf,
                                      int w, int h, int do_avg,
                                      const InterpKernel *kernel,
                                      enum mv_precision precision,
                                      int x, int y, int bd);
#endif

static INLINE int scaled_buffer_offset(int x_offset, int y_offset, int stride,
                                       const struct scale_factors *sf) {
  const int x = sf ? sf->scale_value_x(x_offset, sf) : x_offset;
  const int y = sf ? sf->scale_value_y(y_offset, sf) : y_offset;
  return y * stride + x;
}

static INLINE void setup_pred_plane(struct buf_2d *dst,
                                    uint8_t *src, int stride,
                                    int mi_row, int mi_col,
                                    const struct scale_factors *scale,
                                    int subsampling_x, int subsampling_y) {
  const int x = (MI_SIZE * mi_col) >> subsampling_x;
  const int y = (MI_SIZE * mi_row) >> subsampling_y;
  dst->buf = src + scaled_buffer_offset(x, y, stride, scale);
  dst->stride = stride;
}

void vp9_setup_dst_planes(struct macroblockd_plane planes[MAX_MB_PLANE],
                          const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col);

void vp9_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_RECONINTER_H_
