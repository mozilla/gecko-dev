/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "./vpx_config.h"
#include "./vpx_scale_rtcd.h"
#include "./vp9_rtcd.h"

#include "vpx_ports/mem.h"
#include "vpx_scale/vpx_scale.h"
#include "vpx_scale/yv12config.h"

#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/common/vp9_postproc.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/common/vp9_textblit.h"

#if CONFIG_VP9_POSTPROC
static const short kernel5[] = {
  1, 1, 4, 1, 1
};

const short vp9_rv[] = {
  8, 5, 2, 2, 8, 12, 4, 9, 8, 3,
  0, 3, 9, 0, 0, 0, 8, 3, 14, 4,
  10, 1, 11, 14, 1, 14, 9, 6, 12, 11,
  8, 6, 10, 0, 0, 8, 9, 0, 3, 14,
  8, 11, 13, 4, 2, 9, 0, 3, 9, 6,
  1, 2, 3, 14, 13, 1, 8, 2, 9, 7,
  3, 3, 1, 13, 13, 6, 6, 5, 2, 7,
  11, 9, 11, 8, 7, 3, 2, 0, 13, 13,
  14, 4, 12, 5, 12, 10, 8, 10, 13, 10,
  4, 14, 4, 10, 0, 8, 11, 1, 13, 7,
  7, 14, 6, 14, 13, 2, 13, 5, 4, 4,
  0, 10, 0, 5, 13, 2, 12, 7, 11, 13,
  8, 0, 4, 10, 7, 2, 7, 2, 2, 5,
  3, 4, 7, 3, 3, 14, 14, 5, 9, 13,
  3, 14, 3, 6, 3, 0, 11, 8, 13, 1,
  13, 1, 12, 0, 10, 9, 7, 6, 2, 8,
  5, 2, 13, 7, 1, 13, 14, 7, 6, 7,
  9, 6, 10, 11, 7, 8, 7, 5, 14, 8,
  4, 4, 0, 8, 7, 10, 0, 8, 14, 11,
  3, 12, 5, 7, 14, 3, 14, 5, 2, 6,
  11, 12, 12, 8, 0, 11, 13, 1, 2, 0,
  5, 10, 14, 7, 8, 0, 4, 11, 0, 8,
  0, 3, 10, 5, 8, 0, 11, 6, 7, 8,
  10, 7, 13, 9, 2, 5, 1, 5, 10, 2,
  4, 3, 5, 6, 10, 8, 9, 4, 11, 14,
  0, 10, 0, 5, 13, 2, 12, 7, 11, 13,
  8, 0, 4, 10, 7, 2, 7, 2, 2, 5,
  3, 4, 7, 3, 3, 14, 14, 5, 9, 13,
  3, 14, 3, 6, 3, 0, 11, 8, 13, 1,
  13, 1, 12, 0, 10, 9, 7, 6, 2, 8,
  5, 2, 13, 7, 1, 13, 14, 7, 6, 7,
  9, 6, 10, 11, 7, 8, 7, 5, 14, 8,
  4, 4, 0, 8, 7, 10, 0, 8, 14, 11,
  3, 12, 5, 7, 14, 3, 14, 5, 2, 6,
  11, 12, 12, 8, 0, 11, 13, 1, 2, 0,
  5, 10, 14, 7, 8, 0, 4, 11, 0, 8,
  0, 3, 10, 5, 8, 0, 11, 6, 7, 8,
  10, 7, 13, 9, 2, 5, 1, 5, 10, 2,
  4, 3, 5, 6, 10, 8, 9, 4, 11, 14,
  3, 8, 3, 7, 8, 5, 11, 4, 12, 3,
  11, 9, 14, 8, 14, 13, 4, 3, 1, 2,
  14, 6, 5, 4, 4, 11, 4, 6, 2, 1,
  5, 8, 8, 12, 13, 5, 14, 10, 12, 13,
  0, 9, 5, 5, 11, 10, 13, 9, 10, 13,
};

static const uint8_t q_diff_thresh = 20;
static const uint8_t last_q_thresh = 170;

void vp9_post_proc_down_and_across_c(const uint8_t *src_ptr,
                                     uint8_t *dst_ptr,
                                     int src_pixels_per_line,
                                     int dst_pixels_per_line,
                                     int rows,
                                     int cols,
                                     int flimit) {
  uint8_t const *p_src;
  uint8_t *p_dst;
  int row, col, i, v, kernel;
  int pitch = src_pixels_per_line;
  uint8_t d[8];
  (void)dst_pixels_per_line;

  for (row = 0; row < rows; row++) {
    /* post_proc_down for one row */
    p_src = src_ptr;
    p_dst = dst_ptr;

    for (col = 0; col < cols; col++) {
      kernel = 4;
      v = p_src[col];

      for (i = -2; i <= 2; i++) {
        if (abs(v - p_src[col + i * pitch]) > flimit)
          goto down_skip_convolve;

        kernel += kernel5[2 + i] * p_src[col + i * pitch];
      }

      v = (kernel >> 3);
    down_skip_convolve:
      p_dst[col] = v;
    }

    /* now post_proc_across */
    p_src = dst_ptr;
    p_dst = dst_ptr;

    for (i = 0; i < 8; i++)
      d[i] = p_src[i];

    for (col = 0; col < cols; col++) {
      kernel = 4;
      v = p_src[col];

      d[col & 7] = v;

      for (i = -2; i <= 2; i++) {
        if (abs(v - p_src[col + i]) > flimit)
          goto across_skip_convolve;

        kernel += kernel5[2 + i] * p_src[col + i];
      }

      d[col & 7] = (kernel >> 3);
    across_skip_convolve:

      if (col >= 2)
        p_dst[col - 2] = d[(col - 2) & 7];
    }

    /* handle the last two pixels */
    p_dst[col - 2] = d[(col - 2) & 7];
    p_dst[col - 1] = d[(col - 1) & 7];


    /* next row */
    src_ptr += pitch;
    dst_ptr += pitch;
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_highbd_post_proc_down_and_across_c(const uint16_t *src_ptr,
                                            uint16_t *dst_ptr,
                                            int src_pixels_per_line,
                                            int dst_pixels_per_line,
                                            int rows,
                                            int cols,
                                            int flimit) {
  uint16_t const *p_src;
  uint16_t *p_dst;
  int row, col, i, v, kernel;
  int pitch = src_pixels_per_line;
  uint16_t d[8];

  for (row = 0; row < rows; row++) {
    // post_proc_down for one row.
    p_src = src_ptr;
    p_dst = dst_ptr;

    for (col = 0; col < cols; col++) {
      kernel = 4;
      v = p_src[col];

      for (i = -2; i <= 2; i++) {
        if (abs(v - p_src[col + i * pitch]) > flimit)
          goto down_skip_convolve;

        kernel += kernel5[2 + i] * p_src[col + i * pitch];
      }

      v = (kernel >> 3);

    down_skip_convolve:
      p_dst[col] = v;
    }

    /* now post_proc_across */
    p_src = dst_ptr;
    p_dst = dst_ptr;

    for (i = 0; i < 8; i++)
      d[i] = p_src[i];

    for (col = 0; col < cols; col++) {
      kernel = 4;
      v = p_src[col];

      d[col & 7] = v;

      for (i = -2; i <= 2; i++) {
        if (abs(v - p_src[col + i]) > flimit)
          goto across_skip_convolve;

        kernel += kernel5[2 + i] * p_src[col + i];
      }

      d[col & 7] = (kernel >> 3);

    across_skip_convolve:
      if (col >= 2)
        p_dst[col - 2] = d[(col - 2) & 7];
    }

    /* handle the last two pixels */
    p_dst[col - 2] = d[(col - 2) & 7];
    p_dst[col - 1] = d[(col - 1) & 7];


    /* next row */
    src_ptr += pitch;
    dst_ptr += dst_pixels_per_line;
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

static int q2mbl(int x) {
  if (x < 20) x = 20;

  x = 50 + (x - 50) * 10 / 8;
  return x * x / 3;
}

void vp9_mbpost_proc_across_ip_c(uint8_t *src, int pitch,
                                 int rows, int cols, int flimit) {
  int r, c, i;
  uint8_t *s = src;
  uint8_t d[16];

  for (r = 0; r < rows; r++) {
    int sumsq = 0;
    int sum = 0;

    for (i = -8; i <= 6; i++) {
      sumsq += s[i] * s[i];
      sum += s[i];
      d[i + 8] = 0;
    }

    for (c = 0; c < cols + 8; c++) {
      int x = s[c + 7] - s[c - 8];
      int y = s[c + 7] + s[c - 8];

      sum += x;
      sumsq += x * y;

      d[c & 15] = s[c];

      if (sumsq * 15 - sum * sum < flimit) {
        d[c & 15] = (8 + sum + s[c]) >> 4;
      }

      s[c - 8] = d[(c - 8) & 15];
    }
    s += pitch;
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_highbd_mbpost_proc_across_ip_c(uint16_t *src, int pitch,
                                        int rows, int cols, int flimit) {
  int r, c, i;

  uint16_t *s = src;
  uint16_t d[16];


  for (r = 0; r < rows; r++) {
    int sumsq = 0;
    int sum   = 0;

    for (i = -8; i <= 6; i++) {
      sumsq += s[i] * s[i];
      sum   += s[i];
      d[i + 8] = 0;
    }

    for (c = 0; c < cols + 8; c++) {
      int x = s[c + 7] - s[c - 8];
      int y = s[c + 7] + s[c - 8];

      sum  += x;
      sumsq += x * y;

      d[c & 15] = s[c];

      if (sumsq * 15 - sum * sum < flimit) {
        d[c & 15] = (8 + sum + s[c]) >> 4;
      }

      s[c - 8] = d[(c - 8) & 15];
    }

    s += pitch;
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

void vp9_mbpost_proc_down_c(uint8_t *dst, int pitch,
                            int rows, int cols, int flimit) {
  int r, c, i;
  const short *rv3 = &vp9_rv[63 & rand()]; // NOLINT

  for (c = 0; c < cols; c++) {
    uint8_t *s = &dst[c];
    int sumsq = 0;
    int sum   = 0;
    uint8_t d[16];
    const short *rv2 = rv3 + ((c * 17) & 127);

    for (i = -8; i <= 6; i++) {
      sumsq += s[i * pitch] * s[i * pitch];
      sum   += s[i * pitch];
    }

    for (r = 0; r < rows + 8; r++) {
      sumsq += s[7 * pitch] * s[ 7 * pitch] - s[-8 * pitch] * s[-8 * pitch];
      sum  += s[7 * pitch] - s[-8 * pitch];
      d[r & 15] = s[0];

      if (sumsq * 15 - sum * sum < flimit) {
        d[r & 15] = (rv2[r & 127] + sum + s[0]) >> 4;
      }

      s[-8 * pitch] = d[(r - 8) & 15];
      s += pitch;
    }
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_highbd_mbpost_proc_down_c(uint16_t *dst, int pitch,
                                   int rows, int cols, int flimit) {
  int r, c, i;
  const int16_t *rv3 = &vp9_rv[63 & rand()];  // NOLINT

  for (c = 0; c < cols; c++) {
    uint16_t *s = &dst[c];
    int sumsq = 0;
    int sum = 0;
    uint16_t d[16];
    const int16_t *rv2 = rv3 + ((c * 17) & 127);

    for (i = -8; i <= 6; i++) {
      sumsq += s[i * pitch] * s[i * pitch];
      sum += s[i * pitch];
    }

    for (r = 0; r < rows + 8; r++) {
      sumsq += s[7 * pitch] * s[ 7 * pitch] - s[-8 * pitch] * s[-8 * pitch];
      sum += s[7 * pitch] - s[-8 * pitch];
      d[r & 15] = s[0];

      if (sumsq * 15 - sum * sum < flimit) {
        d[r & 15] = (rv2[r & 127] + sum + s[0]) >> 4;
      }

      s[-8 * pitch] = d[(r - 8) & 15];
      s += pitch;
    }
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

static void deblock_and_de_macro_block(YV12_BUFFER_CONFIG   *source,
                                       YV12_BUFFER_CONFIG   *post,
                                       int                   q,
                                       int                   low_var_thresh,
                                       int                   flag) {
  double level = 6.0e-05 * q * q * q - .0067 * q * q + .306 * q + .0065;
  int ppl = (int)(level + .5);
  (void) low_var_thresh;
  (void) flag;

#if CONFIG_VP9_HIGHBITDEPTH
  if (source->flags & YV12_FLAG_HIGHBITDEPTH) {
    vp9_highbd_post_proc_down_and_across(CONVERT_TO_SHORTPTR(source->y_buffer),
                                         CONVERT_TO_SHORTPTR(post->y_buffer),
                                         source->y_stride, post->y_stride,
                                         source->y_height, source->y_width,
                                         ppl);

    vp9_highbd_mbpost_proc_across_ip(CONVERT_TO_SHORTPTR(post->y_buffer),
                                     post->y_stride, post->y_height,
                                     post->y_width, q2mbl(q));

    vp9_highbd_mbpost_proc_down(CONVERT_TO_SHORTPTR(post->y_buffer),
                                post->y_stride, post->y_height,
                                post->y_width, q2mbl(q));

    vp9_highbd_post_proc_down_and_across(CONVERT_TO_SHORTPTR(source->u_buffer),
                                         CONVERT_TO_SHORTPTR(post->u_buffer),
                                         source->uv_stride, post->uv_stride,
                                         source->uv_height, source->uv_width,
                                         ppl);
    vp9_highbd_post_proc_down_and_across(CONVERT_TO_SHORTPTR(source->v_buffer),
                                         CONVERT_TO_SHORTPTR(post->v_buffer),
                                         source->uv_stride, post->uv_stride,
                                         source->uv_height, source->uv_width,
                                         ppl);
  } else {
    vp9_post_proc_down_and_across(source->y_buffer, post->y_buffer,
                                  source->y_stride, post->y_stride,
                                  source->y_height, source->y_width, ppl);

    vp9_mbpost_proc_across_ip(post->y_buffer, post->y_stride, post->y_height,
                              post->y_width, q2mbl(q));

    vp9_mbpost_proc_down(post->y_buffer, post->y_stride, post->y_height,
                         post->y_width, q2mbl(q));

    vp9_post_proc_down_and_across(source->u_buffer, post->u_buffer,
                                  source->uv_stride, post->uv_stride,
                                  source->uv_height, source->uv_width, ppl);
    vp9_post_proc_down_and_across(source->v_buffer, post->v_buffer,
                                  source->uv_stride, post->uv_stride,
                                  source->uv_height, source->uv_width, ppl);
  }
#else
  vp9_post_proc_down_and_across(source->y_buffer, post->y_buffer,
                                source->y_stride, post->y_stride,
                                source->y_height, source->y_width, ppl);

  vp9_mbpost_proc_across_ip(post->y_buffer, post->y_stride, post->y_height,
                            post->y_width, q2mbl(q));

  vp9_mbpost_proc_down(post->y_buffer, post->y_stride, post->y_height,
                       post->y_width, q2mbl(q));

  vp9_post_proc_down_and_across(source->u_buffer, post->u_buffer,
                                source->uv_stride, post->uv_stride,
                                source->uv_height, source->uv_width, ppl);
  vp9_post_proc_down_and_across(source->v_buffer, post->v_buffer,
                                source->uv_stride, post->uv_stride,
                                source->uv_height, source->uv_width, ppl);
#endif  // CONFIG_VP9_HIGHBITDEPTH
}

void vp9_deblock(const YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst,
                 int q) {
  const int ppl = (int)(6.0e-05 * q * q * q - 0.0067 * q * q + 0.306 * q
                        + 0.0065 + 0.5);
  int i;

  const uint8_t *const srcs[3] = {src->y_buffer, src->u_buffer, src->v_buffer};
  const int src_strides[3] = {src->y_stride, src->uv_stride, src->uv_stride};
  const int src_widths[3] = {src->y_width, src->uv_width, src->uv_width};
  const int src_heights[3] = {src->y_height, src->uv_height, src->uv_height};

  uint8_t *const dsts[3] = {dst->y_buffer, dst->u_buffer, dst->v_buffer};
  const int dst_strides[3] = {dst->y_stride, dst->uv_stride, dst->uv_stride};

  for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_VP9_HIGHBITDEPTH
    assert((src->flags & YV12_FLAG_HIGHBITDEPTH) ==
           (dst->flags & YV12_FLAG_HIGHBITDEPTH));
    if (src->flags & YV12_FLAG_HIGHBITDEPTH) {
      vp9_highbd_post_proc_down_and_across(CONVERT_TO_SHORTPTR(srcs[i]),
                                           CONVERT_TO_SHORTPTR(dsts[i]),
                                           src_strides[i], dst_strides[i],
                                           src_heights[i], src_widths[i], ppl);
    } else {
      vp9_post_proc_down_and_across(srcs[i], dsts[i],
                                    src_strides[i], dst_strides[i],
                                    src_heights[i], src_widths[i], ppl);
    }
#else
    vp9_post_proc_down_and_across(srcs[i], dsts[i],
                                  src_strides[i], dst_strides[i],
                                  src_heights[i], src_widths[i], ppl);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  }
}

void vp9_denoise(const YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst,
                 int q) {
  const int ppl = (int)(6.0e-05 * q * q * q - 0.0067 * q * q + 0.306 * q
                        + 0.0065 + 0.5);
  int i;

  const uint8_t *const srcs[3] = {src->y_buffer, src->u_buffer, src->v_buffer};
  const int src_strides[3] = {src->y_stride, src->uv_stride, src->uv_stride};
  const int src_widths[3] = {src->y_width, src->uv_width, src->uv_width};
  const int src_heights[3] = {src->y_height, src->uv_height, src->uv_height};

  uint8_t *const dsts[3] = {dst->y_buffer, dst->u_buffer, dst->v_buffer};
  const int dst_strides[3] = {dst->y_stride, dst->uv_stride, dst->uv_stride};

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    const int src_stride = src_strides[i];
    const int src_width = src_widths[i] - 4;
    const int src_height = src_heights[i] - 4;
    const int dst_stride = dst_strides[i];

#if CONFIG_VP9_HIGHBITDEPTH
    assert((src->flags & YV12_FLAG_HIGHBITDEPTH) ==
           (dst->flags & YV12_FLAG_HIGHBITDEPTH));
    if (src->flags & YV12_FLAG_HIGHBITDEPTH) {
      const uint16_t *const src_plane = CONVERT_TO_SHORTPTR(
          srcs[i] + 2 * src_stride + 2);
      uint16_t *const dst_plane = CONVERT_TO_SHORTPTR(
          dsts[i] + 2 * dst_stride + 2);
      vp9_highbd_post_proc_down_and_across(src_plane, dst_plane, src_stride,
                                           dst_stride, src_height, src_width,
                                           ppl);
    } else {
      const uint8_t *const src_plane = srcs[i] + 2 * src_stride + 2;
      uint8_t *const dst_plane = dsts[i] + 2 * dst_stride + 2;

      vp9_post_proc_down_and_across(src_plane, dst_plane, src_stride,
                                    dst_stride, src_height, src_width, ppl);
    }
#else
    const uint8_t *const src_plane = srcs[i] + 2 * src_stride + 2;
    uint8_t *const dst_plane = dsts[i] + 2 * dst_stride + 2;
    vp9_post_proc_down_and_across(src_plane, dst_plane, src_stride, dst_stride,
                                  src_height, src_width, ppl);
#endif
  }
}

static double gaussian(double sigma, double mu, double x) {
  return 1 / (sigma * sqrt(2.0 * 3.14159265)) *
         (exp(-(x - mu) * (x - mu) / (2 * sigma * sigma)));
}

static void fillrd(struct postproc_state *state, int q, int a) {
  char char_dist[300];

  double sigma;
  int ai = a, qi = q, i;

  vp9_clear_system_state();

  sigma = ai + .5 + .6 * (63 - qi) / 63.0;

  /* set up a lookup table of 256 entries that matches
   * a gaussian distribution with sigma determined by q.
   */
  {
    int next, j;

    next = 0;

    for (i = -32; i < 32; i++) {
      int a_i = (int)(0.5 + 256 * gaussian(sigma, 0, i));

      if (a_i) {
        for (j = 0; j < a_i; j++) {
          char_dist[next + j] = (char) i;
        }

        next = next + j;
      }
    }

    for (; next < 256; next++)
      char_dist[next] = 0;
  }

  for (i = 0; i < 3072; i++) {
    state->noise[i] = char_dist[rand() & 0xff];  // NOLINT
  }

  for (i = 0; i < 16; i++) {
    state->blackclamp[i] = -char_dist[0];
    state->whiteclamp[i] = -char_dist[0];
    state->bothclamp[i] = -2 * char_dist[0];
  }

  state->last_q = q;
  state->last_noise = a;
}

void vp9_plane_add_noise_c(uint8_t *start, char *noise,
                           char blackclamp[16],
                           char whiteclamp[16],
                           char bothclamp[16],
                           unsigned int width, unsigned int height, int pitch) {
  unsigned int i, j;

  // TODO(jbb): why does simd code use both but c doesn't,  normalize and
  // fix..
  (void) bothclamp;
  for (i = 0; i < height; i++) {
    uint8_t *pos = start + i * pitch;
    char  *ref = (char *)(noise + (rand() & 0xff));  // NOLINT

    for (j = 0; j < width; j++) {
      if (pos[j] < blackclamp[0])
        pos[j] = blackclamp[0];

      if (pos[j] > 255 + whiteclamp[0])
        pos[j] = 255 + whiteclamp[0];

      pos[j] += ref[j];
    }
  }
}

static void swap_mi_and_prev_mi(VP9_COMMON *cm) {
  // Current mip will be the prev_mip for the next frame.
  MODE_INFO *temp = cm->postproc_state.prev_mip;
  cm->postproc_state.prev_mip = cm->mip;
  cm->mip = temp;

  // Update the upper left visible macroblock ptrs.
  cm->mi = cm->mip + cm->mi_stride + 1;
  cm->postproc_state.prev_mi = cm->postproc_state.prev_mip + cm->mi_stride + 1;
}

int vp9_post_proc_frame(struct VP9Common *cm,
                        YV12_BUFFER_CONFIG *dest, vp9_ppflags_t *ppflags) {
  const int q = MIN(105, cm->lf.filter_level * 2);
  const int flags = ppflags->post_proc_flag;
  YV12_BUFFER_CONFIG *const ppbuf = &cm->post_proc_buffer;
  struct postproc_state *const ppstate = &cm->postproc_state;

  if (!cm->frame_to_show)
    return -1;

  if (!flags) {
    *dest = *cm->frame_to_show;
    return 0;
  }

  vp9_clear_system_state();

  // Alloc memory for prev_mip in the first frame.
  if (cm->current_video_frame == 1) {
    cm->postproc_state.last_base_qindex = cm->base_qindex;
    cm->postproc_state.last_frame_valid = 1;
    ppstate->prev_mip = vpx_calloc(cm->mi_alloc_size, sizeof(*cm->mip));
    if (!ppstate->prev_mip) {
      return 1;
    }
    ppstate->prev_mi = ppstate->prev_mip + cm->mi_stride + 1;
    memset(ppstate->prev_mip, 0,
           cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->mip));
  }

  // Allocate post_proc_buffer_int if needed.
  if ((flags & VP9D_MFQE) && !cm->post_proc_buffer_int.buffer_alloc) {
    if ((flags & VP9D_DEMACROBLOCK) || (flags & VP9D_DEBLOCK)) {
      const int width = ALIGN_POWER_OF_TWO(cm->width, 4);
      const int height = ALIGN_POWER_OF_TWO(cm->height, 4);

      if (vp9_alloc_frame_buffer(&cm->post_proc_buffer_int, width, height,
                                 cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                                 cm->use_highbitdepth,
#endif  // CONFIG_VP9_HIGHBITDEPTH
                                 VP9_ENC_BORDER_IN_PIXELS,
                                 cm->byte_alignment) < 0) {
        vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                           "Failed to allocate MFQE framebuffer");
      }

      // Ensure that postproc is set to all 0s so that post proc
      // doesn't pull random data in from edge.
      memset(cm->post_proc_buffer_int.buffer_alloc, 128,
             cm->post_proc_buffer.frame_size);
    }
  }

  if (vp9_realloc_frame_buffer(&cm->post_proc_buffer, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_DEC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL) < 0)
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate post-processing buffer");

  if ((flags & VP9D_MFQE) && cm->current_video_frame >= 2 &&
      cm->postproc_state.last_frame_valid && cm->bit_depth == 8 &&
      cm->postproc_state.last_base_qindex <= last_q_thresh &&
      cm->base_qindex - cm->postproc_state.last_base_qindex >= q_diff_thresh) {
    vp9_mfqe(cm);
    // TODO(jackychen): Consider whether enable deblocking by default
    // if mfqe is enabled. Need to take both the quality and the speed
    // into consideration.
    if ((flags & VP9D_DEMACROBLOCK) || (flags & VP9D_DEBLOCK)) {
      vp8_yv12_copy_frame(ppbuf, &cm->post_proc_buffer_int);
    }
    if ((flags & VP9D_DEMACROBLOCK) && cm->post_proc_buffer_int.buffer_alloc) {
      deblock_and_de_macro_block(&cm->post_proc_buffer_int, ppbuf,
                                 q + (ppflags->deblocking_level - 5) * 10,
                                 1, 0);
    } else if (flags & VP9D_DEBLOCK) {
      vp9_deblock(&cm->post_proc_buffer_int, ppbuf, q);
    } else {
      vp8_yv12_copy_frame(&cm->post_proc_buffer_int, ppbuf);
    }
  } else if (flags & VP9D_DEMACROBLOCK) {
    deblock_and_de_macro_block(cm->frame_to_show, ppbuf,
                               q + (ppflags->deblocking_level - 5) * 10, 1, 0);
  } else if (flags & VP9D_DEBLOCK) {
    vp9_deblock(cm->frame_to_show, ppbuf, q);
  } else {
    vp8_yv12_copy_frame(cm->frame_to_show, ppbuf);
  }

  cm->postproc_state.last_base_qindex = cm->base_qindex;
  cm->postproc_state.last_frame_valid = 1;

  if (flags & VP9D_ADDNOISE) {
    const int noise_level = ppflags->noise_level;
    if (ppstate->last_q != q ||
        ppstate->last_noise != noise_level) {
      fillrd(ppstate, 63 - q, noise_level);
    }

    vp9_plane_add_noise(ppbuf->y_buffer, ppstate->noise, ppstate->blackclamp,
                        ppstate->whiteclamp, ppstate->bothclamp,
                        ppbuf->y_width, ppbuf->y_height, ppbuf->y_stride);
  }

  *dest = *ppbuf;

  /* handle problem with extending borders */
  dest->y_width = cm->width;
  dest->y_height = cm->height;
  dest->uv_width = dest->y_width >> cm->subsampling_x;
  dest->uv_height = dest->y_height >> cm->subsampling_y;

  swap_mi_and_prev_mi(cm);
  return 0;
}
#endif  // CONFIG_VP9_POSTPROC
