/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP8_DECODER_ERROR_CONCEALMENT_H_
#define VP8_DECODER_ERROR_CONCEALMENT_H_

#include "onyxd_int.h"
#include "ec_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate memory for the overlap lists */
int vp8_alloc_overlap_lists(VP8D_COMP *pbi);

/* Deallocate the overlap lists */
void vp8_de_alloc_overlap_lists(VP8D_COMP *pbi);

/* Estimate all missing motion vectors. */
void vp8_estimate_missing_mvs(VP8D_COMP *pbi);

/* Functions for spatial MV interpolation */

/* Interpolates all motion vectors for a macroblock mb at position
 * (mb_row, mb_col). */
void vp8_interpolate_motion(MACROBLOCKD *mb,
                            int mb_row, int mb_col,
                            int mb_rows, int mb_cols,
                            int mi_stride);

/* Conceal a macroblock with corrupt residual.
 * Copies the prediction signal to the reconstructed image.
 */
void vp8_conceal_corrupt_mb(MACROBLOCKD *xd);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP8_DECODER_ERROR_CONCEALMENT_H_
